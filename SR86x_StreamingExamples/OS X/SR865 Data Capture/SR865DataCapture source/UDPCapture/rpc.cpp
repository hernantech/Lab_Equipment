/* rpc module  -- implements the ONC remote procedure call protocol.
 *
 * This module implements the ONC remote procedure call protocol as
 * descibed in RFC 1831 to facilitate the creation of RPC servers.
 * The module provides the machinery necessary for unpacking
 * remote procedure calls forwarding the calls to registered
 * programs and packaging up the responses to return to the
 * caller.
 *
 * Create an RpcServer by deriving from RpcServer and overriding
 * rpcCall() and creating any reponse packing functions
 * you need. When responding to rpcCall(), you must create
 * a response in order to complete the call. Otherwise, the
 * module assumes that the execution is still pending and subsequent
 * calls will be for the same rpc. If an error occurs, call
 * createErrorResponse(error) to complete the call. Error codes
 * should be one of the following:
 *
 * ERROR_PROC_UNAVAIL -> Procedure unavailable
 * ERROR_GARBAGE_ARGS -> Parameters passed to the procedure could not be unpacked correctly.
 * ERROR_SYSTEM_ERR   -> There was a system error such as a memory allocation error.
 *
 * Most rpc clients expect a response for every call, even if it contains no data.
 * When procedures are successfully executed, it is the responsibility of the rpcCall()
 * to create an appropriate response to send back. If the program does not
 * need to return data, it shoud call createVoidResponse(). Otherwise is should call
 * createResponse() with the packing function to call to pack the data
 * for the response.
 */

#include <string.h>
#include "rpc.hpp"
#include <arpa/inet.h>


enum auth_flavor { AUTH_NONE, AUTH_SYS, AUTH_SHORT };
enum msg_type { CALL, REPLY };
enum reply_stat { MSG_ACCEPTED, MSG_DENIED };
enum accept_stat {
  SUCCESS,       /* RPC executed successfully */
  PROG_UNAVAIL,  /* remote hasn't exported program */
  PROG_MISMATCH, /* remote can't support version # */
  PROC_UNAVAIL,  /* program can't support procedure */
  GARBAGE_ARGS,  /* procedure can't decode params */
  SYSTEM_ERR     /* errors like memory allocation failure */
};
enum reject_stat {
  RPC_MISMATCH,  /* RPC version number != 2 */
  AUTH_ERROR     /* remote can't autenticate caller */
};
enum auth_stat {
  AUTH_OK,           /* success */
  /*
   * failed at remote end
   */
  AUTH_BADCRED,      /* bad credential (seal broken) */
  AUTH_REJECTEDCRED, /* client must begin new session */
  AUTH_BADVERF,      /* bad verifier (seal broken) */
  AUTH_REJECTEDVERF, /* verifier expired or replayed */
  AUTH_TOOWEAK,      /* rejected for security reasons */
  /*
   * failed locally
   */
  AUTH_INVALIDRESP,  /* bogus response verifier */
  AUTH_FAILED        /* reason unknown */
};

/****************************************************************************
 * void packAuth(struct opaque_auth *auth)
 *   Packs an authentication field
 *
 * Arguments:
 *   auth: pointer to the opaque_auth structure to pack
 *
 * Specification:
 * 1. Pack the auth structure
 * 2. Set ERROR_EOF if the internal buffer overflows.
 */
void RpcPacker::packAuth(opaque_auth *auth)
{
  packEnum(auth->flavor);
  packOpaque(auth->body,auth->len);
}

/****************************************************************************
 * void unpackAuth(struct opaque_auth *auth)
 *   Unpacks an authentication structure
 *
 * Arguments:
 *   auth: pointer to opaque_auth structure that will receive the
 *         unpacked data.
 *
 * Specification:
 * 1. Sets error to ERROR_EOF if the internal buffer overflows.
 */
void RpcUnpacker::unpackAuth(opaque_auth *auth)
{
  auth->flavor = (unsigned)unpackEnum();
  auth->len = (unsigned)unpackUint();
  auth->body = const_cast<char *>(unpackFopaque(auth->len));
}


/****************************************************************************
 * RpcServer(unsigned short sz,unsigned long program,unsigned version)
 *   Construct an RpcServer for the given program and version. The
 *   client should call hasAllocatedBuffer() to verify that the buffer
 *   was successfully created.
 *
 * Arguments:
 *   sz: Sets the size of the internal rcvBuffer and the maximum sized
 *       record that can be received. Records larger than this will
 *       be dropped with no reply.
 *   program: The rpc program id for this server.
 *   version: The rpc version for this program.
 *
 * Specification:
 * 1. Construct an rps server for program and version with a sz byte buffer.
 */
RpcServer::RpcServer(void *buffer,unsigned short sz,uint32 program,unsigned version)
      : prog(program), vers(version), rcvBuffer((char *)buffer), maxSize(sz)
{
  //ASSERT0(buffer != NULL );
  //ASSERT0( maxSize > 0 );
  reset();
}

/****************************************************************************
 * ~RpcServer()
 *   Destructor for RpcServer
 *
 * Specification:
 * 1. Deletes internal buffer.
 */
RpcServer::~RpcServer()
{
}

/****************************************************************************
 * void reset(void)
 *   Reset the server to initial IDLE state with an empty receive buffer.
 */
void RpcServer::reset(void)
{
  pConn = (void *)0;
  state = IDLE;
  rcvBytes = 0;
  sentLen = 0;
  sentRecordLen = 0;
  bNewResponse = false;
}

/****************************************************************************
 * unsigned short receiveNewBytes(char *data,unsigned len,unsigned offset)
 *   Space permitting, copy up to len-offset bytes to the receive buffer
 *   starting at the byte offset.
 *
 * Arguments:
 *   data: pointer to bytes to be copied
 *   len: number of bytes pointed to by data
 *   offset: start the copy at this offset.
 *
 * Return: the number of bytes copied.
 *
 * Specification:
 * 1. Only copy bytes if rcvBytes < maxSize
 * 2. Only copy bytes if offset < len
 * 3. Do not allow rcvBuffer to contain more than maxBytes
 */
unsigned short RpcServer::receiveNewBytes(char *data,uint32 len,uint32 offset)
{
  unsigned short count = 0;

  if ( offset < len && rcvBytes < maxSize ) {
    count = maxSize - rcvBytes;
    if ( len - offset < count )
      count = len - offset;
    memcpy(&rcvBuffer[rcvBytes],&data[offset],count);
    rcvBytes += count;
  }
  return count;
}

/****************************************************************************
 * unsigned newData(char *data, unsigned len, unsigned urg_len)
 *   Space permitting, copy up to len bytes to the receive buffer. Process
 *   any complete records received. Return the number of bytes accepted.
 *
 * Arguments:
 *   data: pointer to bytes to be copied
 *   len: number of bytes pointed to by data
 *   urg_len: number of bytes pointed to by data that are marked urgent
 *     by the tcp. (This is ignored)
 *
 * Return: the number of bytes accepted.
 *
 * Specification:
 * 1. Copy new data to receive buffer
 * 2. Execute the first record if complete
 * 3. Delete any bytes in the receive buffer that are no longer needed
 * 4. Copy new data to receive buffer
 * 5. Goto step 2 if execution is complete and we have more data
 * 6. If execution isn't complete process any incomplete rpc
 * 7. Copy any new data to the receive buffer
 * 8. Return total number of bytes copied.
 */
unsigned RpcServer::newData(char *data, uint32 len, uint32 urg_len)
{
  unsigned short count,num;
  char *pBuffer;
  (void)urg_len; // Ignore urg_len

  /* Implementation Note:
   *
   * I try to save an unnecessary copy by checking to see if the receive
   * buffer is empty and we're waiting for a new rpc. If so, then
   * process the record in place and only copy unused bytes to rcvBuffer.
   * In the general case, just copy the bytes to rcvBuffer before
   * commensing processing.
   */
  if ( rcvBytes == 0 && len < maxSize && IDLE == state ) {
    // Try to avoid an unneeded copy to rcvBuffer
    rcvBytes = num = len;
    pBuffer = data;
  }
  else {
    // First copy data to buffer
    num = receiveNewBytes(data,len,0);
    pBuffer = rcvBuffer;
  }

  while ( IDLE == state && rcvBytes > 0) {
    // Merge fragments and get record length
    if ( mergeAllFragments(pBuffer) ) {
      // We've got a complete rpc: process it.
      count = processRpc(pBuffer, rcvRecordLen);
      if ( count || pBuffer == data) {      // if we processed an RPC, or we tried to leave data in place
        rcvBytes -= count;                  // subtract number of processed bytes from rcvBytes
        rcvRecordLen -= count;              // subtract number of processed bytes from rcvRecordLen
        memmove(rcvBuffer,&pBuffer[count],rcvBytes);   // move any bytes beyond count to beginning of buffer
        pBuffer = rcvBuffer;                           // and set pBuffer to rcvBuffer
      }
      // Copy any more data that will fit
      num += receiveNewBytes(data,len,num);
    }
    else {
      // The merge failed: either the record is too big
      // or we don't have all the data yet.
      if ( rcvRecordLen > maxSize ) {
        rcvBytes = 0;
        rcvRecordLen = 0;
        return len;
      }
      // If we skipped the copy to rcvBuffer above
      // we have to do it now.
      if ( pBuffer == data )
        memcpy(rcvBuffer,data,rcvBytes);
      return num;
    }
  }
  processPendingRpc();

  // Copy any more data that will fit
  num += receiveNewBytes(data,len,num);

  return num;
}

/****************************************************************************
 * bool mergeAllFragments(char *data)
 *   Merge all RPC fragments into one record
 *
 * Arguments:
 *   data: pointer to the beginning of the record
 *
 * Return:
 *   Return true if the merge was successful and a complete record has been
 *   received, otherwise false. In any case update len to the merged length
 *   pointed to by data, and set rcvRecordLen to the size of the record for
 *   all fragments merged so far.
 *
 * PRECONDITIONS:
 *   rcvBytes == number of bytes pointed to by data
 *   data == pointer to beginning of an RPC fragment. Several fragments may
 *           be appended back to back. The last fragment in the record
 *           will have the last fragment flag set in its record marker.
 *
 * POSTCONDITIONS:
 *   rcvBytes == number of valid bytes pointed to by data. This may be changed
 *           from its initial value due to the merging of fragments
 *   rcvRecordLen == the size of the fragments merged so far. If this is
 *           larger than maxSize, the record should be discarded.
 *
 * Specification:
 * 1. If not enough data to read the recordLength, set rcvRecordLen to 0 and return false
 * 2. Otherwise, read the record marker and set rcvRecordLen to the fragment length
 * 3. Data availability permitting, merge all fragments into a single fragment, updating
 *    the head record marker as we go.
 * 4. If the recordLength is larger than maxSize, stop the merge, set rcvRecordLen to
 *    maxSize +1 and return false.
 * 5. If we don't have a complete record return false
 */
bool RpcServer::mergeAllFragments(char *data)
{
  // Extract fragment size and last fragment flag
  uint32 recordLength;
  uint32 lastFragment;
  uint32 nextLength;
  uint32 offset;
  bool bResult = true;



  // Do we have enough data to read the record marker?
  if ( rcvBytes >= 4 ) {
    // Read the record marker
    recordLength = ntohl( *(uint32 *)data );

    // Split out lastFragment flag
    lastFragment = recordLength & 0x80000000;
    // Split out record length
    recordLength &= 0x7fffffff;

    // First check if we have all the fragments
    offset = recordLength + 4;
    while (!lastFragment && bResult) {
      // Do we have enough data to read the next record marker?
      if ( rcvBytes >= offset + 4 && recordLength + 4 <= maxSize) {
        // Read in next record marker
        nextLength = ntohl( *(uint32 *)&data[offset] );
        lastFragment = nextLength & 0x80000000;
        nextLength &= 0x7fffffff;

        // Update recordLength and merge with original fragment
        recordLength += nextLength;
        *(uint32 *)data = htonl( recordLength | lastFragment );
        memmove(&data[offset],&data[offset+4],rcvBytes-offset-4);
        rcvBytes -= 4;
        offset += nextLength;
      }
      else {
        bResult = false;
      }
    }

    // Do we have all the data
    if ( rcvBytes < offset )
      bResult = false;

    // Update rcvRecordLen
    if ( recordLength + 4 > maxSize )  {
      // Make sure recordLength is truncated to a size greater than maxSize
      bResult = false;
      rcvRecordLen = (unsigned short)(maxSize + 1);
    }
    else {
      rcvRecordLen = (unsigned short)(recordLength + 4);
    }
  }
  else {
    rcvRecordLen = 0;
    bResult = false;
  }

  return bResult;
}


/****************************************************************************
 * unsigned processRpc(char *data,unsigned len)
 *   Execute the rpc identified by proc. When in the IDLE
 *   state, this should only be called when data points to a
 *   complete rpc (ie mergeAllFragments() returned true)
 *
 * Arguments:
 *   data: pointer to the rpc data associated with proc
 *   len: number of bytes pointed to by data
 *
 * Return: the number bytes processed
 *
 * Specification:
 * 1. If state is IDLE, then process the rpc header to get the proc
 *    that should be executed.
 * 2. If execution is still pending call rpcCall(proc)
 * 3. If execution isn't complete set state to EXECUTION_PENDING
 * 4. Return total number of bytes processed.
 */
uint32 RpcServer::processRpc(char *data,uint32 len)
{
  char *pBuffer = data;
  uint32 count = 0;

  if ( state == IDLE ) {
    count = processRpcHeader(pBuffer,len);
    pBuffer += count;
  }
  if ( state == FIRST_EXECUTION || state == EXECUTION_PENDING ) {
    count += rpcCall(pBuffer,len-count,proc);
    // If execution not complete, bump state to EXECUTION_PENDING
    if ( RESPONSE_PENDING != state )
      state = EXECUTION_PENDING;
  }

  return count;
}

/****************************************************************************
 * unsigned processRpcHeader(char *data,unsigned len)
 *   Unpack the rpc header pointed to by len, checking for errors. Return
 *   the number of bytes processed.
 *
 * Arguments:
 *   data: pointer to the rpc header (including the rcp record marker)
 *   len: number of bytes pointed to by data
 *
 * Return: the number bytes processed
 *
 * Specification:
 * 1. Unpack RM, XID, MSG_TYPE, RPC_VER, PROG, VER, PROC, AUTH_CRED, AUTH_VERF
 * 2. If an error occurs create the appropriate error response and return len
 * 3. Set xid = XID
 * 4. Set proc = PROC
 * 5. If proc == 0 createErrorResponse( SUCCESS) and return len
 * 6. If successful set state = FIRST_EXECUTION and return byte processed.
 */
uint32 RpcServer::processRpcHeader(char *data,uint32 len)
{
  // Unpack the RPC header
  RpcUnpacker unpckr(data,len);
  uint32 value;
  opaque_auth auth;

  // Record Marker
  value = unpckr.unpackUint();
  // Xid
  xid = unpckr.unpackInt();
  // Msg type
  value = unpckr.unpackEnum();
  if ( unpckr.getError() || value != CALL ) {
    // Not worthy of a response
    // Leave state in IDLE
    return len;
  }
  // Rpc version
  value = unpckr.unpackUint();
  if ( RPCVERSION != value ) {
    createErrorResponse( ERROR_RPC_MISMATCH );
    return len;
  }
  // Prog
  value = unpckr.unpackUint();
  if ( value != prog ) {
    createErrorResponse( ERROR_PROG_UNAVAIL );
    return len;
  }
  // Version
  value = unpckr.unpackUint();
  if ( value != vers ) {
    createErrorResponse( ERROR_PROG_MISMATCH );
    return len;
  }
  // Procedure
  proc = unpckr.unpackUint();
  // Unpack authentication
  unpckr.unpackAuth( &auth );
  unpckr.unpackAuth( &auth );
  // Ignore authentication unless there's an unpacking error
  if ( unpckr.getError() ) {
    createErrorResponse( ERROR_GARBAGE_ARGS );
    return len;
  }
  // proc 0 is always a null procedure
  if ( proc == 0 ) {
    createErrorResponse( SUCCESS );
    return len;
  }
  state = FIRST_EXECUTION;
  return len - unpckr.getUnpackedSize();
}

/****************************************************************************
 * void processPendingRpc(void)
 *   This function should be called regularly by the client to complete
 *   processing of any pending rpc
 *
 * Specification:
 * 1. rcvBuffer should contain the rpc data
 * 2. rcvBytes should contain the number of bytes in rcvBuffer
 * 3. If we've got a complete or pending rpc execute it and update
 *    rcvBytes, rcvRecordLen, and rcvBuffer accordingly.
 */
void RpcServer::processPendingRpc(void)
{
  if ( (IDLE == state && rcvBytes > 0 && mergeAllFragments(rcvBuffer)) || EXECUTION_PENDING == state ) {
    uint32 count = processRpc(rcvBuffer,rcvRecordLen);
    if ( count ) {
      rcvBytes -= count;
      rcvRecordLen -= count;
      memmove(rcvBuffer,&rcvBuffer[count],rcvBytes);
    }
  }
}

/****************************************************************************
 * unsigned sendData(char *data, unsigned len )
 *   Called by the tcp for rpc response transmissions
 *
 * Arguments:
 *   data: pointer buffer to receive the rpc reply.
 *   len: number of bytes pointed to by data
 *
 * Return: the number bytes written to data
 *
 * Specification:
 * 1. State will be set to RESPONSE_PENDING if a response is ready
 * 2. sentLen = number of bytes already sent. Use this as an offset
 *      when packing the rest of the reply.
 * 3. packup the reply, including the record marker.
 * 4. Set sentRecordLen to total number of bytes in the reply
 *      and mark the record appropriately
 * 5. Set bNewResponse to false
 * 6. Return the actual number of bytes written.
 */
uint32 RpcServer::sendData(char *data, uint32 len )
{
  uint32 actual_size = 0;

  if ( RESPONSE_PENDING == state ) {
    bNewResponse = false;
    RpcPacker pckr(data,len,sentLen);

    // Pack placeholder for record marker
    pckr.packUint(0);
    // Pack the response
    if ( generateResult == &RpcServer::packErrorResult )
      packErrorResult(&pckr, responseResult );
    else {
      packResponseHeader(&pckr);
      if ( generateResult != NULL ) {
        (this->*generateResult)(&pckr, responseResult);
      }
    }
    // Update sentRecordLen
    sentRecordLen = (unsigned short)pckr.getPackedSize();
    // Get the record size
    uint32 record_size = sentRecordLen-4;
    // Get the actual size
    actual_size = pckr.getActualSize();
    // Repack the record marker
    pckr.reset();
    pckr.packUint( record_size | 0x80000000 );
  }

  return actual_size;
}

/****************************************************************************
 * void acked(unsigned len)
 *   Callback function. Called when connection has received an ACK that
 *   data previously sent has been received. This data should not be
 *   sent again.
 *
 *   The client may need to know when a response is complete so that it
 *   can discard data associated with it. To do this, override acked() but
 *   make sure it calls RpcServer::acked() before doing its own processing.
 *   Use getState to determine when the call is complete.
 *
 *   void ClientServer::acked(unsigned len)
 *   {
 *     RpcServer::acked(len);
 *     if ( getState() != RESPONSE_PENDING && getProc() == PROC_WITH_DATA_TO_DISCARD) {
 *       // Response is complete: discard unneeded data
 *     }
 *   }
 *
 * Arguments:
 *   size: the amount of data that was successfully sent
 *
 * 1. sentLen is the total number of bytes we've successfully sent
 * 2. sentRecordLen is the size of the record we're sending.
 * 3. Update sentLen by adding len to sentLen.
 * 4. If sentLen == sentRecordLen, then we are done.
 *    Update state accordingly.
 */
void RpcServer::acked(uint32 len)
{
  sentLen += len;
  if ( sentLen >= sentRecordLen ) {
    sentLen = 0;
    sentRecordLen = 0;
    state = IDLE;
  }
}

/****************************************************************************
 * unsigned rpcCall(char *data,unsigned len,unsigned long proc)
 *   Default rpcCall(). Just creates a void response. Override this
 *   member function to do something useful.
 *
 * Arguments:
 *   data: Points to packed arguments for the given procedure
 *   len: Number of bytes pointed to by data
 *   proc: procedure to execute.
 *
 * Return: number of bytes processed.
 *
 * Specification:
 * 1. createVoidResponse()
 * 2. return len.
 */
uint32 RpcServer::rpcCall(char *,uint32 len,uint32)
{

  createVoidResponse();
  return len;
}

/****************************************************************************
 * void createResponse( void (RpcServer::*funcResult)(RpcPacker *, unsigned long), unsigned long tag )
 *   Generic function for creating a reponse. When the tcp is ready for transmission,
 *   funcResult(&RpcPacker,tag) will be called to pack up the response.
 *
 * Arguments:
 *   funcResult: member function to call to pack the response
 *   tag: User supplied value that will be passed to funcResult when called
 *
 * Specification:
 * 1. save tag and funcResult
 * 2. set state to RESPONSE_PENDING
 * 3. set bNewResponse to true
 */
void RpcServer::createResponse( void (RpcServer::*funcResult)(RpcPacker *, uint32), uint32 tag )
{
  responseResult = tag;
  generateResult = funcResult;
  state = RESPONSE_PENDING;
  bNewResponse = true;
}

/****************************************************************************
 * void createVoidResponse(void)
 *   Convenience function for creating a void RPC response.
 */
void RpcServer::createVoidResponse(void)
{
  generateResult = NULL;
  state = RESPONSE_PENDING;
  bNewResponse = true;
}

/****************************************************************************
 * void createLongResponse(unsigned long result)
 *   Convenience function for creating an RPC long result.
 */
void RpcServer::createLongResponse(uint32 result)
{
  responseResult = result;
  generateResult = &RpcServer::packLongResult;
  state = RESPONSE_PENDING;
  bNewResponse = true;
}

/****************************************************************************
 * void createErrorResponse(unsigned long error)
 *   Creates an RPC error result. Error should be one of the RPC errors defined
 *   in the header file. ( ERROR_PROG_UNAVAIL, etc. )
 */
void RpcServer::createErrorResponse(uint32 error)
{
  responseResult = error;
  generateResult = &RpcServer::packErrorResult;
  state = RESPONSE_PENDING;
  bNewResponse = true;
}

/****************************************************************************
 * void packErrorResult(RpcPacker *pckr, unsigned long error)
 *   Packing function for error responses. Used by createErrorResponse()
 */
void RpcServer::packErrorResult(RpcPacker *pckr, uint32 error)
{
  pckr->packUint(xid);
  pckr->packEnum(REPLY);

  if ( ERROR_RPC_MISMATCH == error ) {
    pckr->packEnum( MSG_DENIED );
    pckr->packEnum( RPC_MISMATCH );
    pckr->packUint( RPCVERSION );
    pckr->packUint( RPCVERSION );
  }
  else {
    opaque_auth auth_verf;

    auth_verf.flavor = AUTH_NONE;
    auth_verf.len = 0;
    pckr->packEnum( MSG_ACCEPTED );
    pckr->packAuth( &auth_verf );
    pckr->packInt( error );
    if ( ERROR_PROG_MISMATCH == error ) {
      pckr->packUint( vers );
      pckr->packUint( vers );
    }
  }
}

/****************************************************************************
 * void packResponseHeader(RpcPacker *pckr)
 *   Called by sendData() to create the rpc response header.
 *   genericResponse() is called right after this to package 
 *   up the procedure specific data.
 */
void RpcServer::packResponseHeader(RpcPacker *pckr)
{
  opaque_auth auth_verf;

  auth_verf.flavor = AUTH_NONE;
  auth_verf.len = 0;

  pckr->packUint(xid);
  pckr->packEnum(REPLY);
  pckr->packEnum( MSG_ACCEPTED );
  pckr->packAuth( &auth_verf );
  pckr->packInt( SUCCESS );
}

/****************************************************************************
 * void packLongResult(RpcPacker *pckr, unsigned long tag)
 *   Packing function for long results. Used by createLongResponse().
 */
void RpcServer::packLongResult(RpcPacker *pckr, uint32 tag)
{
  pckr->packUint(tag);
}

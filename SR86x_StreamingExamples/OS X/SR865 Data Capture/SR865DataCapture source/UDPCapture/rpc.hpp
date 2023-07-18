/* rpc.h -- header for rpc.c */
#ifndef _RPC_H_
#define _RPC_H_

#include "xdr.hpp"

typedef struct {
  uint32 flavor; // The authentication flavor
  uint32 len;    // Length of opaque data pointed to by body
  char *body;      // Points to opaque data
}
opaque_auth;

/* Extend Packer and Unpacker to handle opaque_auth structures */
class RpcPacker : public XdrPacker {
public:
  RpcPacker(char *buffer, uint32 size, uint32 offset) : XdrPacker(buffer,size,offset) {}
  void packAuth(opaque_auth *auth);
};

class RpcUnpacker : public XdrUnpacker {
public:
  RpcUnpacker(const char *buffer, uint32 size) : XdrUnpacker(buffer,size) {}
  void unpackAuth(opaque_auth *auth);
};

enum eState { IDLE=0, FIRST_EXECUTION, EXECUTION_PENDING, RESPONSE_PENDING };

// RpcServer
class RpcServer {
public:
  void *pConn;                    // Pointer to the connection
private:
  // RPC call info
  const uint32 prog;       // Program identifier for this RPC server
  const uint32 vers;            // Version identifier for this RPC server
  uint32 proc;             // Procedure identifier for this RPC call
  enum eState state;              // State of this call
  // RPC response info
  uint32 xid;              // ID of current RPC call
  int32 responseResult;            // long result for RPC call
  void (RpcServer::*generateResult)(RpcPacker *,uint32); // Pointer to function to call to generate response
  // Receive buffer management
  char *rcvBuffer;                // Receive buffer
  unsigned short rcvBytes;        // Indicates total data bytes stored in rcvBuffer
  unsigned short rcvRecordLen;    // Indicates receive record length
  // Send buffer management
  unsigned short sentLen;         // Bytes sent in a previous transmission
  unsigned short sentRecordLen;   // Total bytes for the sent record.
  const unsigned short maxSize;   // Indicates the maximum record size we can receive
  bool bNewResponse;

  // Helper functions
  void packErrorResult(RpcPacker *pckr,uint32 tag);
  void packLongResult(RpcPacker *pckr,uint32 tag);
  void packResponseHeader(RpcPacker *pckr);
  unsigned short receiveNewBytes(char *data,uint32 len,uint32 offset);
  bool mergeAllFragments(char *data);
  uint32 processRpc(char *data,uint32 len);
  uint32 processRpcHeader(char *data,uint32 len);

public:
  // Constructor/Destructor
  RpcServer(void *buffer, unsigned short sz, uint32 prog, uint32 vers);
  virtual ~RpcServer();
  void reset(void);

  // Process any pending rpc
  void processPendingRpc(void);
  bool hasNewResponse(void) { return bNewResponse; }
  // Response creation functions: these set state to RESPONSE_PENDING
  void createErrorResponse(uint32 error);
  void createLongResponse(uint32 result);
  void createVoidResponse(void);
  void createResponse( void (RpcServer::*funcResponse)(RpcPacker *,uint32), uint32 tag );
  // Get the current rpc state
  enum eState getState(void) { return state; }
  // Get the current proc
  uint32 getProc(void) { return proc; }

  // Overrides
  virtual uint32 rpcCall(char *data, uint32 len, uint32 proc);

  // Call backs
  virtual void aborted(void) { reset(); }
  virtual void timedout(void) {reset(); }
  virtual void closed(void) { reset(); }
  virtual void connected(void *pCnx) { reset(); pConn=pCnx; }
  virtual uint32 newData(char *data, uint32 len, uint32 urg_len);
  virtual uint32 sendData(char *data, uint32 len);
  virtual void acked(uint32 len);
  virtual unsigned short getWindow(void) { return maxSize - rcvBytes; }
};

// Templated class for selecting the buffer size of the RpcServer
template<unsigned short sz>
class BufferedRpcServer : public RpcServer {
private:
  char buffer[sz];
public:
  BufferedRpcServer(uint32 prog, uint32 vers) : RpcServer(buffer,sz,prog,vers) {}
};

// Response Errors
#define ERROR_PROG_UNAVAIL  1
#define ERROR_PROG_MISMATCH 2
#define ERROR_PROC_UNAVAIL  3
#define ERROR_GARBAGE_ARGS  4
#define ERROR_SYSTEM_ERR    5
#define ERROR_RPC_MISMATCH  6

#define RPCVERSION 2

// For information purposes only, this is the rpc call/reply structure
#if 0
struct call_body_header {
  uint32 rpcvers;
  uint32 prog;
  uint32 vers;
  uint32 proc;
  struct opaque_auth cred;
  struct opaque_auth verf;
};
struct reply_body_header {
  uint32 xid;
  enum reply_stat stat;
  union {
    struct {
      struct opaque_auth verf;
      enum accept_stat stat;
      union {
        uint32 results;
        struct {
          uint32 low;
          uint32 high;
        } mismatch_info;
      } reply_data;
    } areply;
    struct {
      enum reject_stat stat;
      union {
        struct {
          uint32 low;
          uint32 high;
        } mismatch_info;
        enum auth_stat stat;
      } rejected_reply;
    } rreply;
  } reply;
};
struct rpc_msg {
  uint32 xid;
  enum msg_type mtype;
  union {
    struct call_body_header cbody;
    struct reply_body_header rbody;
  } body;
};
#endif

#endif // _RPC_H_

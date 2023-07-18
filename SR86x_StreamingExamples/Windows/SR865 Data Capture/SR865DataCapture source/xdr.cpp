/* xdr module  -- implements packers and unpackers of XDR data.
 */

#include <string.h>
#include "xdr.h"
#include <winsock.h>


/****************************************************************************
 * XdrPacker(char *buffer, unsigned size, unsigned byteOffset)
 *   Constructor for the packer.
 *
 * Arguments:
 *   buffer: pointer to storage of size bytes.
 *   size: number of bytes pointed to by buffer.
 *   byteOffset: number of bytes to pack before we start storing
 *     packed bytes in the buffer.
 *
 * Specification:
 * 1. Initialize the packer to use buffer of given size and offset.
 * 2. reset packer
 */
XdrPacker::XdrPacker(char *buffer, uint32 size, uint32 byteOffset) : p_start(buffer), p_end(buffer+size), p_offset(buffer-byteOffset)
{
  //ASSERT0( buffer != NULL );
  
  reset();
}


/****************************************************************************
 * void packRawBytes(char (*pRead)(void *), void *param, const char *buffer, unsigned size)
 *   Pack the bytes pointed to by buffer or returned by pRead into the packer copying only
 *   those bytes that are within the packer buffer window.
 *
 * Arguments:
 *   pRead: pointer to function to call to get characters. If NULL, then buffer is used
 *   param: the parameter to pass to pRead()
 *   buffer: pointer to the bytes to pack
 *   size: number of bytes pointed to by buffer or returned by pRead(param)
 *
 * Specification:
 * 1. Copy only those bytes such that p_start <= p < p_end
 * 2. If p + size > p_end at the end of the copy then set ERROR_EOF
 * 3. Set p = p + size
 */
void XdrPacker::packRawBytes(char (*pRead)(void *), void *param, const char *buffer, uint32 size)
{
  if ( p + size <= p_start ) {
    // Do nothing
  }
  else if ( p < p_end ) {
    char *p_off;
    size_t count;

    // Find the starting point for the copy
    if ( p < p_start )
      p_off = p_start;
    else
      p_off = p;

    // Calculate the number of bytes to copy
    if ( p + size > p_end ) {
      count = (size_t)(p_end - p_off);
      setError( ERROR_EOF );
    }
    else {
      count = (size_t)(p + size - p_off);
    }
    
    // Make the copy at the appropriate offset
    if ( pRead == NULL )
      memcpy(p_off,buffer+(p_off-p),count);
    else {
      for (char *p_cpy=p; p_cpy<p_off; p_cpy++ )
        pRead(param);
      for ( size_t i=0; i<count; i++ )
        *p_off++ = pRead(param);
    }
  }
  else if ( p + size > p_end ) {
    setError( ERROR_EOF );
  }
  // Update the packing pointer
  p += size;
}

/****************************************************************************
 * void packInt(long val);
 *   This function packs an integer or long into the packer
 *
 * Arguments:
 *   val: the value to be packed.
 *
 * Specification:
 * 1. Pack 4 byte val in network byte order into the buffer.
 */
void XdrPacker::packInt(int32 val)
{
  uint32 temp = htonl(val);

  if ( p >= p_start && p + 4 <= p_end ) {
    *(int32 *)p = temp;
    p += 4;
  }
  else
    packRawBytes(NULL,NULL,(char *)&temp,4);
}


/****************************************************************************
 * void packFopaque(const char *buffer,unsigned long len);
 *   Pack opaque data of fixed length len into the packer
 *
 * Arguments:
 *   buffer: pointer to the data to be packed.
 *   len: number of bytes to pack
 * 
 * Specification:
 * 1. Pack the len bytes pointed to by buffer. Add appropriate zero padding to
 *    bring total to a 4-byte boundary.
 */
void XdrPacker::packFopaque(const char *buffer,uint32 len)
{
  uint32 pad;
  uint32 zero = 0;

  // Calculate padding
  pad = len & 3;
  if (pad)
    pad = 4-pad;

  packRawBytes(NULL,NULL,buffer,len);
  if (pad)
    packRawBytes(NULL,NULL,(const char *)&zero,pad);
}

/****************************************************************************
 * void packFopaque(char (*pRead)(void),unsigned long len)
 *   Pack opaque data of fixed length len into the packer
 *
 * Arguments:
 *   pRead: pointer to function to call to get data to be packed.
 *   param: the parameter to pass to pRead when it is called.
 *   len: number of bytes to pack
 * 
 * Specification:
 * 1. Pack the len bytes pointed to by buffer. Add appropriate zero padding to
 *    bring total to a 4-byte boundary.
 */
void XdrPacker::packFopaque(char (*pRead)(void *),void *param,uint32 len)
{
  uint32 pad;
  uint32 zero = 0;

  // Calculate padding
  pad = len & 3;
  if (pad)
    pad = 4-pad;

  packRawBytes(pRead,param,NULL,len);
  if (pad)
    packRawBytes(NULL,NULL,(const char *)&zero,pad);
}

/****************************************************************************
 * void packOpaque(const char *buffer, unsigned long len);
 *   Pack opaque data of length len into the packer
 *
 * Arguments:
 *   buffer: pointer to data to be packed
 *   len: number of bytes to pack
 * 
 * Specification:
 * 1. Pack buffer
 * 2. Set ERROR_EOF if the internal buffer overflows.
 */
void XdrPacker::packOpaque(const char *buffer, uint32 len)
{
  packUint(len);
  packFopaque(buffer,len);
}

/****************************************************************************
 * void packOpaque(char (*pRead)(void *),void *param, unsigned long len)
 *   Pack opaque data of length len into the packer
 *
 * Arguments:
 *   pRead: pointer to the data to be packed.
 *   param:
 *   len: number of bytes to pack
 * 
 * Specification:
 * 1. Pack buffer
 * 2. Set ERROR_EOF if the internal buffer overflows.
 */
void XdrPacker::packOpaque(char (*pRead)(void *),void *param, uint32 len)
{
  packUint(len);
  packFopaque(pRead,param,len);
}

/****************************************************************************
 * XdrUnpacker(const char *buffer, unsigned size)
 *   Constructor for the unpacker.
 *
 * Arguments:
 *   buffer: pointer to storage that contains bytes to be unpacked.
 *   size: number of bytes pointed to by buffer.
 *
 * Specification:
 * 1. Initialize the unpacker to use buffer of given size.
 * 2. reset unpacker
 */
XdrUnpacker::XdrUnpacker(const char *buffer, uint32 size) : p_start(buffer), p_end(buffer+size)
{
  //ASSERT0( buffer != NULL );
  
  reset();
}


/****************************************************************************
 * long unpackInt(void)
 *   Unpacks an integer or long and returns it as a long
 *
 * Specification:
 *
 * 1. Returns the unpacked integer or long value.
 * 2. Updates the pointer
 * 2. Sets ERROR_EOF and returns 0 if buffer overflows
 */
int32 XdrUnpacker::unpackInt(void)
{
  int32 value;
  if ( p + 4 <= p_end ) {
    value = ntohl( *(int32 *)p );
    p += 4;
  }
  else {
    value = 0;
    p = p_end;
    setError( ERROR_EOF );
  }

  return value;
}

/****************************************************************************
 * long unpackBool(void)
 *   Unpacks an enum bool
 *
 * Specification:
 * 1. Returns the unpacked boolean.
 * 2. Updates the pointer
 * 3. Set ERROR_BAD_ENUM if not equal to TRUE or FALSE
 */
int32 XdrUnpacker::unpackBool(void)
{
  int32 val = unpackEnum();
  if ( val > TRUE ) {
    setError( ERROR_BAD_ENUM );
  }
  return val;
}

/****************************************************************************
 * const char *unpackFopaque(unsigned long len)
 *   Unpacks opaque data of fixed length len and returns a pointer to the data
 *
 * Specification:
 * 1. len -- the fixed length of the opaque data. buffer must be of at least this
 *      size in bytes.
 *
 * 4. Sets unpckr->error to ERROR_EOF if the internal buffer overflows.
 */
const char *XdrUnpacker::unpackFopaque(uint32 len)
{
  uint32 total_len;
  const char *pOpaque = p;

  // Calculate padding
  total_len = len & ~3;
  if (len & 3)
    total_len += 4;

  if ( p + total_len <= p_end ) {
    p += total_len;
  }
  else {
    p = p_end;
    setError( ERROR_EOF );
  }
  return pOpaque;
}

/****************************************************************************
 * const char *unpackOpaque(unsigned long *len)
 *   Unpacks variable length opaque data. Sets len to the length of the
 *   unpacked data and returns a pointer to it.
 *
 * Arguments:
 *   len: pointer that will receive the length of the unpacked data
 * 
 * Return: a pointer to the unpacked data
 *
 * Specification:
 * 1. Sets error to ERROR_EOF if the internal buffer overflows.
 */
const char *XdrUnpacker::unpackOpaque(uint32 *len)
{
  const char *pOpaque;
  
  *len = (uint32)unpackUint();
  pOpaque = unpackFopaque(*len);
  if ( getError() == ERROR_EOF )
    *len = 0;
  return pOpaque;
}

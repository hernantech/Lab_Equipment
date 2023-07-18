/* xdr.h -- header for xdr.cpp */
#ifndef _XDR_H_
#define _XDR_H_

// XDR Packing/Unpacking errors
#define ERROR_EOF 1
#define ERROR_BAD_ENUM 2

// Bool enum values
#define TRUE 1
#define FALSE 0

#define uint32 unsigned int
#define int32 int
#define uint16 unsigned short
#define int16 short

/****************************************************************************
 * class XdrPacker
 *   XDR data packer. This class provides methods for packing data into
 *   XDR presentation format for use with RPC function calls and replies.
 *   In order to facilitate the use of the packer with small buffers,
 *   the packer allows you to define the packer size and an offset.
 *   The size indicates the size of the buffer associated with the packer.
 *   Only size bytes will be written to the buffer. The offset lets you
 *   define the byte offset beyond which packing commenses. Only after offset
 *   bytes have been packed, will data actually be copied to the buffer. All
 *   the packing functions also protect against overwriting beyond
 *   the end of the packing buffer. The error flag is set to 
 *   ERROR_EOF if this would have occurred.
 *
 *   getPackedSize() returns the size of all packed added to the packer, 
 *      regardless of whether it was actually written to the buffer.
 *   getActualSize() returns the number of bytes actually written to the buffer.
 */
class XdrPacker {
  char * const p_start;   // Points to start of the buffer
  char * const p_end;     // Points to end of the buffer
  char * const p_offset;  // Points to offset bytes before start of buffer
  char *p;                // Points to next byte to be packed
  int error;              // Stores any errors

  // Implementation helper functions
  void packRawBytes(char (*pRead)(void *), void *param, const char *buffer, uint32 size);


protected:
  void setError(int err) { if ( !error || err == ERROR_EOF ) error = err; }

public:
  XdrPacker(char *buffer, uint32 size, uint32 offset);
  void reset(void) { p = p_offset; error = 0; }
  int getError(void) { return error; }
  uint32 getPackedSize(void) { return (uint32)(p-p_offset); }
  uint32 getActualSize(void) { return (p < p_start) ? 0 : (p > p_end ) ? (uint32)(p_end-p_start) : (uint32)(p-p_start); }
  char *getPackedData(void) { return p_start; }
  void packInt(int32 val);
  void packUint(uint32 val) { packInt( (int32)val ); }
  void packEnum(int32 val) { packInt(val); }
  void packBool(int val) { if (val) packInt(TRUE); else packInt(FALSE); }
  void packFopaque(const char *buffer, uint32 len);
  void packFopaque(char (*pRead)(void *), void *param, uint32 len);
  void packOpaque(const char *buffer, uint32 len);
  void packOpaque(char (*pRead)(void *), void *param, uint32 len);
};

/****************************************************************************
 * class XdrUnpacker
 *   XDR data unpacker. This class provides methods for unpacking data in
 *   XDR presentation format for use with RPC function calls and replies.
 *   If the unpacker reads beyond the end of the buffer, the error flag
 *   is set to ERROR_EOF.
 */
class XdrUnpacker {
  const char * const p_start; // Points to start of buffer
  const char * const p_end;   // Points to end of buffer
  const char *p;              // Points to next byte to be unpacked
  int error;                  // Stores any errors

protected:
  void setError(int err) { if ( !error || err == ERROR_EOF ) error = err; }

public:
  XdrUnpacker(const char *buffer, uint32 size);
  void reset(void) { p = p_start; error =0; }
  int getError(void) { return error; }
  uint32 getUnpackedSize(void) { return (uint32)(p_end - p); }
  uint32 getSize(void) { return (uint32)(p_end - p_start); }
  int32 unpackInt(void);
  uint32 unpackUint(void) { return (uint32)unpackInt(); }
  int32 unpackEnum(void) { return unpackInt(); }
  int32 unpackBool(void);
  const char *unpackFopaque(uint32 len);
  const char *unpackOpaque(uint32 *len);
};

#endif // _XDR_H_

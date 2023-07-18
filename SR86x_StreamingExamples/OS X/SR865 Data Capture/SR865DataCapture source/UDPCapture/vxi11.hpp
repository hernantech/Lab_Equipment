//---------------------------------------------------------------------------

#ifndef vxi11H
#define vxi11H

//---------------------------------------------------------------------------
#include <CoreFoundation/CoreFoundation.h>
#include "rpc.hpp"

#define BUFF_SIZE   1024
#define DATA_SIZE   786
#define RECORD_SIZE 4

typedef int32 Device_Link;
typedef int32 Device_Error;
typedef uint32 Device_Flags;
typedef struct
{
    uint32 max_len;
    uint32 len;
    char *str;

    void init(uint32 l);
    uint32 set(const char *s);
    uint32 set(const char *s, uint32 ulen);
}
var_string;
enum Device_AddrFamily
{ /* used by interrupts */
    DEVICE_TCP,
    DEVICE_UDP
};

typedef struct
{
    int32 clientId; /* implementation specific value.*/
    bool lockDevice; /* attempt to lock the device */
    uint32 lock_timeout; /* time to wait on a lock */
    var_string device; /* name of device */
}
Create_LinkParms;
typedef struct
{
    Device_Error error;
    Device_Link lid;
    unsigned short abortPort; /* for the abort RPC */
    uint32 maxRecvSize; /* specifies max data size in bytes device will accept on a write */
}
Create_LinkResp;

typedef struct
{
    Device_Link lid; /* link id from create_link */
    uint32 io_timeout;       /* time to wait for I/O */
    uint32 lock_timeout;     /* time to wait for lock */
    Device_Flags flags;
    var_string data; /* the data length and the data itself */
}
Device_WriteParms;
typedef struct
{
    Device_Error error;
    uint32 size; /* Number of bytes written */
}
Device_WriteResp;

typedef struct
{
    Device_Link lid; /* link id from create_link */
    uint32 requestSize; /* Bytes requested */
    uint32 io_timeout; /* time to wait for I/O */
    uint32 lock_timeout;/* time to wait for lock */
    Device_Flags flags;
    char termChar; /* valid if flags & termchrset */
}
Device_ReadParms;
typedef struct
{
    Device_Error error;
    int32 reason; /* Reason(s) read completed */
    var_string data; /* data_len and data_val */
}
Device_ReadResp;

typedef struct
{
    Device_Error error; /* error code */
    unsigned char stb; /* the returned status byte */
}
Device_ReadStbResp;

typedef struct
{
    Device_Link lid; /* Device_Link id from connect call */
    Device_Flags flags; /* flags with options */
    uint32 lock_timeout; /* time to wait for lock */
    uint32 io_timeout; /* time to wait for I/O */
}
Device_GenericParms;

typedef struct
{
    uint32 hostAddr; /* Host servicing Interrupt */
    unsigned short hostPort; /* valid port # on client */
    uint32 progNum; /* DEVICE_INTR */
    uint32 progVers; /* DEVICE_INTR_VERSION */
    Device_AddrFamily progFamily; /* DEVICE_UDP | DEVICE_TCP */
}
Device_RemoteFunc;

typedef struct
{
    Device_Link lid;
    bool enable; /* Enable or disable interrupts */
    var_string handle; /* <40> Host specific data */
}
Device_EnableSrqParms;

typedef struct
{
    Device_Link lid; /* link id from create_link */
    Device_Flags flags; /* Contains the waitlock flag */
    uint32 lock_timeout; /* time to wait to acquire lock */
}
Device_LockParms;

typedef struct
{
    Device_Link lid; /* link id from create_link */
    Device_Flags flags; /* flags specifying various options */
    uint32 io_timeout; /* time to wait for I/O to complete */
    uint32 lock_timeout; /* time to wait on a lock */
    int32 cmd; /* which command to execute */
    bool network_order; /* client's byte order */
    int32 datasize; /* size of individual data elements */
    var_string data_in; /* docmd data parameters */
}
Device_DocmdParms;
typedef struct
{
    Device_Error error; /* returned status */
    var_string data_out; /* returned data parameter */
}
Device_DocmdResp;




class vxi11_client
{
public:
    vxi11_client();
    virtual ~vxi11_client();
    
    bool connectToDevice(CFStringRef address, unsigned short myport=0, bool excl=false);
    bool connectionOK();
    void setCallback(void (*func)(bool, bool));
    void cancelConnect();
    bool canWrite();
    
    bool get_port();

    bool create_link();
    
    bool device_write(const char *str);
    bool device_read(char **str);
    bool device_readstb(unsigned char *stb);
    bool device_trigger();
    bool device_clear();
    bool device_remote();
    bool device_local();
    bool device_lock();
    bool device_unlock();
    bool create_intr_chan();
    bool destroy_intr_chan(void);
    bool device_enable_srq();
    Device_DocmdResp *device_docmd();
    bool destroy_link();

    bool device_abort();

    void device_intr_srq();
    
    const char *getLastError();

    
protected:
    CFStringRef device_addr;
    void (*callbackFunc)(bool, bool);
    
    uint32 xid;
    uint32 prog;
    uint32 vers;
    bool silence;
    unsigned short port;
    opaque_auth rpc_auth, rpc_verf;

    CFWriteStreamRef writeStrm;
    CFReadStreamRef readStrm;
    bool createPortMapperStream();
    bool createVXI11Stream();
    void closeStream();
    
    char *tx_buff;
    char *rx_buff;
    RpcPacker *packer, *record_packer;
    RpcUnpacker *unpacker;

    void setPortMapperProg();
    void setVXI11CoreProg();
    void setVXI11AbortProg();
    void setVXI11IntrProg();
    
    void packRPC(uint32 proc);
    bool unpackRPC();
    
    bool writeToStream();
    bool readFromStream();

    uint32 instr_addr;
    uint32 core_port;

    struct
    {
        int32 clientId;
        bool lockDevice;
        uint32 lock_timeout;
        var_string data;
        Device_Link lid;
        uint32 io_timeout;
        Device_Flags flags;
        uint32 requestSize;
        char termChar;
        uint32 hostAddr;
        unsigned short hostPort;
        uint32 progNum;
        uint32 progVers;
        Device_AddrFamily progFamily;
        bool enable;
        int32 cmd;
        bool network_order;
        int32 datasize;
    }
    Combo_Params;

    struct
    {
        Device_Error error;
        int32 reason;
        var_string data;
        unsigned char stb;
        uint32 size;
        Device_Link lid;
        unsigned short abortPort;
        uint32 maxRecvSize;
    }
    Combo_Resp;

    int32 reading;
    CFStreamClientContext myContext;
    static void writeStreamCallback(CFWriteStreamRef stream, CFStreamEventType eventType, void *clientCallBackInfo);
    void continueExec(bool ok);
    bool get_port2(bool ok);
    bool create_link2(bool ok);
    void connectToDevice2(bool ok);
    static char errstring[36];
};

#endif

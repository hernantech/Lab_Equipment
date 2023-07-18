//---------------------------------------------------------------------------



#include "vxi11.hpp"
#include <iostream>

//---------------------------------------------------------------------------
//
// vxi11 class
// A VXI11 connection starts with an inital connection to a SunRPC server on the device
// to obtain the (dynamic) VXI11 core port.
// Then VXI11 talks to the instrument over that core port.
//
// The user only needs to
// 1) call connectToDevice() to connect to the remote instrument
// 2) then user can then use device_write() & device_read() to send and receive data with the instrument
// 3) when the user is done, call destroy_link() to close the vxi11 connection
//
// NOTE: not tested for transferring binary data;
// use with caution on binary data!
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------


// string helper object
void var_string::init(uint32 l)
{
    max_len = l;
    len = 0;
    str = new char[l];
}
uint32 var_string::set(const char *s)
{
    len = (uint32)strlen(s);
    if (len < max_len)
        strcpy(str, s);
    return len;
}
uint32 var_string::set(const char *s, uint32 ulen)
{
    len = ulen;
    if (len < max_len)
    {
        strncpy(str, s, len);
        str[len] = '\0';
    }
    return len;
}



vxi11_client::vxi11_client()
{
    device_addr = NULL;
    
    xid = 0;
    prog = 0;
    vers = 0;
    port = 0;
    rpc_auth.flavor = 0;
    rpc_auth.len = 0;
    rpc_auth.body = NULL;
    rpc_verf.flavor = 0;
    rpc_verf.len = 0;
    rpc_verf.body = NULL;

    writeStrm = NULL;
    readStrm = NULL;
    callbackFunc = NULL;
    
    // there is an additional "record marking" on top of rpc;
    // it consists of 4 extra bytes before the rpc data
    tx_buff = new char[BUFF_SIZE];
    rx_buff = new char[BUFF_SIZE];
    record_packer = new RpcPacker(tx_buff, BUFF_SIZE, 0);
    packer = new RpcPacker(tx_buff+RECORD_SIZE, BUFF_SIZE-RECORD_SIZE, 0);
    unpacker = new RpcUnpacker(rx_buff, BUFF_SIZE);

    instr_addr = 0;
    core_port = 0;

    Combo_Params.clientId = 123456;
    Combo_Params.lockDevice = false;
    Combo_Params.lock_timeout = 8000;  // in ms
    Combo_Params.data.init(DATA_SIZE);
    Combo_Params.data.set("");
    Combo_Params.lid = -1;
    Combo_Params.io_timeout = 8000;  // in ms
    Combo_Params.flags = 0;
    Combo_Params.requestSize = 0;
    Combo_Params.termChar = '\0';
    Combo_Params.hostAddr = 0;
    Combo_Params.hostPort = 0;
    Combo_Params.progNum = 0;
    Combo_Params.progVers = 0;
    Combo_Params.progFamily = DEVICE_TCP;
    Combo_Params.enable = false;
    Combo_Params.cmd = 0;
    Combo_Params.network_order = 0;
    Combo_Params.datasize = 0;

    Combo_Resp.error = 0;
    Combo_Resp.reason = 0;
    Combo_Resp.data.init(DATA_SIZE);
    Combo_Resp.data.set("");
    Combo_Resp.stb = 0;
    Combo_Resp.size = 0;
    Combo_Resp.lid = -1;
    Combo_Resp.abortPort = 0;
    Combo_Resp.maxRecvSize = DATA_SIZE;
    
    reading = 0;
    myContext.version = 0;
    myContext.info = this;
    myContext.retain = NULL;
    myContext.release = NULL;
    myContext.copyDescription = NULL;
}
/*virtual*/ vxi11_client::~vxi11_client()
{
    destroy_link();
    closeStream();  // redundant
    
    delete packer;
    delete record_packer;
    delete unpacker;
    
    delete []tx_buff;
    delete []rx_buff;
}



// sunrpc prog id
void vxi11_client::setPortMapperProg()
{
    prog = 100000;
    vers = 2;
}
// vxi11 core, abort and interrupt prog ids
void vxi11_client::setVXI11CoreProg()
{
    prog = 395183;
    vers = 1;
}
void vxi11_client::setVXI11AbortProg()
{
    prog = 395184;
    vers = 1;
}
void vxi11_client::setVXI11IntrProg()
{
    prog = 395185;
    vers = 1;
}


// sunrpc conenction
bool vxi11_client::createPortMapperStream()
{
    closeStream();

    // port mapper
    CFStringRef myaddr = device_addr;
    CFStreamCreatePairWithSocketToHost(nil, myaddr, 111, &readStrm, &writeStrm);
    
    if (CFWriteStreamSetClient(writeStrm, kCFStreamEventCanAcceptBytes | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered, writeStreamCallback, &myContext))
        CFWriteStreamScheduleWithRunLoop(writeStrm, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    reading = 1;

    bool wrok = CFWriteStreamOpen(writeStrm);
    bool rdok = CFReadStreamOpen(readStrm);
    if (!wrok)
        std::cout << "could not open port mapper write stream" << std::endl;
    if (!rdok)
        std::cout << "could not open port mapper read stream" << std::endl;
    return wrok & rdok;
}
// vxi11 connection
bool vxi11_client::createVXI11Stream()
{
    closeStream();

    // VXI11
    CFStringRef myaddr = device_addr;
    CFStreamCreatePairWithSocketToHost(nil, myaddr, port, &readStrm, &writeStrm);

    if (CFWriteStreamSetClient(writeStrm, kCFStreamEventCanAcceptBytes | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered, writeStreamCallback, &myContext))
        CFWriteStreamScheduleWithRunLoop(writeStrm, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    reading = 2;
    
    bool wrok = CFWriteStreamOpen(writeStrm);
    bool rdok = CFReadStreamOpen(readStrm);
    if (!wrok)
        std::cout << "could not open vxi11 write stream" << std::endl;
    if (!rdok)
        std::cout << "could not open vxi11 read stream" << std::endl;
    return wrok & rdok;
}
void vxi11_client::closeStream()
{
    bool docallback = !silence && (writeStrm || readStrm);
    if (writeStrm)
    {
        CFWriteStreamClose(writeStrm);
        writeStrm = NULL;
    }
    if (readStrm)
    {
        CFReadStreamClose(readStrm);
        readStrm = NULL;
    }
    
    Combo_Params.lid = -1;
    Combo_Resp.lid = -1;
    
    if (docallback && callbackFunc)
        (*callbackFunc)(false, false);
}
void vxi11_client::setCallback(void (*func)(bool, bool))
{
    callbackFunc = func;
}



// RPC header packing
void vxi11_client::packRPC(uint32 proc)
{
    packer->reset();
    packer->packUint(++xid); // id no.
    packer->packInt(0);      // call
    packer->packUint(RPCVERSION); // rpc version no
    packer->packUint(prog);
    packer->packUint(vers);
    packer->packUint(proc);
    packer->packAuth(&rpc_auth);     // auth
    packer->packAuth(&rpc_verf);     // verf
}
bool vxi11_client::unpackRPC()
{
    uint32 val;
    val = unpacker->unpackUint();
    if (val != xid)
    {
        std::cout << "rpc reply xid doesn't match" << std::endl;
        return false;       // xid doesn't match
    }
    val = unpacker->unpackUint();
    if (val != 1)
    {
        std::cout << "rpc reply is not a reply" << std::endl;
        return false;       // not a reply!
    }
    val = unpacker->unpackUint();
    if (val == 0)
    {
        unpacker->unpackAuth(&rpc_verf);
        val = unpacker->unpackUint();
        if (val != 0)
        {
            std::cout << "rpc reply malformed" << std::endl;
            return false;       // not a reply!
        }
        return true;
    }
    else
    {
        std::cout << "rpc reply rejected" << std::endl;
        return false;       // message rejected!
    }
}


// write to tcp socket
bool vxi11_client::writeToStream()
{
    // write correct record marker:
    record_packer->reset();
    uint32 len = 0x7fffffff & packer->getActualSize();
    uint32 rec = 0x80000000 | len;
    record_packer->packUint(rec);
    
    // tx data
    if (writeStrm)
    {
        len = len + RECORD_SIZE;
        CFIndex nb = CFWriteStreamWrite(writeStrm, (const UInt8 *)tx_buff, len);
        //std::cout << "  vxi11 wrote " << nb << " bytes" << std::endl;
        CFErrorRef err = CFWriteStreamCopyError(writeStrm);
        if (err)
            closeStream();
 
        if (nb != len)
            std::cout << "vxi11 wrote different no. of bytes: " << nb << "; wanted " << len << std::endl;
        return (err == NULL);
    }
    else
    {
        std::cout << "  null writeStream" << std::endl;
        return false;
    }
}
// read from tcp socket
bool vxi11_client::readFromStream()
{
    unpacker->reset();
    if (readStrm)
    {
        CFIndex nb = CFReadStreamRead(readStrm, (UInt8 *)rx_buff, BUFF_SIZE);
        //std::cout << "  vxi11 read " << nb << " bytes" << std::endl;
        CFErrorRef err = CFReadStreamCopyError(readStrm);
        if (err)
            return false;   // tcp read error
    
        // ok, check record marking
        uint32 rec = unpacker->unpackUint();
        if (!(rec & 0x80000000))
        {
            std::cout << "not final record; aggregating not implemented yet!" << std::endl;
            return false;   // not final record!
        }
        rec = (rec & 0x7fffffff) + RECORD_SIZE;
        if (nb != rec)
        {
            std::cout << "length of reply not equal to bytes read! " << rec << "/" << nb << std::endl;
            return false;
        }
        if (rec > BUFF_SIZE)
        {
            std::cout << "length of reply too long! " << rec << std::endl;
            return false;   // length exceeds buffer size
        }
        return (err == NULL);
    }
    else
    {
        std::cout << "  null readStream" << std::endl;
        return false;
    }
}


// connect to vxi11 device
bool vxi11_client::connectToDevice(CFStringRef address, unsigned short myport, bool excl)
{
    // same address!
    if (address)
    {
        Combo_Params.lockDevice = excl;
        port = myport;

        if (device_addr)
        {
            silence = true;
            destroy_link();
            silence = false;
        }
        device_addr = CFStringCreateCopy(NULL, address);
        
        if (port)
        {
            std::cout << "VXI11 port " << port << " specified by user" << std::endl;
            return createVXI11Stream();
        }
        else
            return get_port();
        
        /*
        if (get_port())
        {
            bool ok = createVXI11Stream();
            if (!ok)
                return ok;
            
            return create_link();
        }
        */
    }
    return false;
}
bool vxi11_client::connectionOK()
{
    return (Combo_Params.lid != -1);
}
bool vxi11_client::canWrite()
{
    return (writeStrm && CFWriteStreamCanAcceptBytes(writeStrm));
}


// get vxi11 port from sunrpc server
bool vxi11_client::get_port()
{
    bool ok = createPortMapperStream();
    if (!ok)
        return ok;
    
    // get vxi11 port
    setPortMapperProg();
    packRPC(3);     // getport is proc 3

    packer->packUint(395183);        // VXI11 core prog
    packer->packUint(1);             // VXI11 core vers
    packer->packUint(6);             // VXI11 core TCP/IP
    packer->packUint(0);             // port no.
    
    return true;
}

// vxi11 create_link() command
bool vxi11_client::create_link()
{
    if (Combo_Params.lid != -1)
    {
        // we already have an active link!
        return true;
    }

    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(10);     // create_link is proc 10

    // pack create_link params
    packer->packInt(Combo_Params.clientId);
    packer->packBool(Combo_Params.lockDevice);
    packer->packUint(Combo_Params.lock_timeout);
    Combo_Params.data.set("inst0");
    packer->packOpaque(Combo_Params.data.str, Combo_Params.data.len);
    
    return true;
}

// vxi11 device_write() command
bool vxi11_client::device_write(const char *str)
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "device_write no valid link!" << std::endl;
        return false;
    }

    Combo_Params.data.set(str);
    if (Combo_Params.data.len > Combo_Resp.maxRecvSize)
    {
        // too long!
        std::cout << "device_write too long" << std::endl;
        return false;
    }

    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(11);     // device_write is proc 11

    // pack device_write params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.io_timeout);
    packer->packUint(Combo_Params.lock_timeout);
    uint32 prev = Combo_Params.flags;
    Combo_Params.flags |= 0x08;     // end of write
    packer->packUint(Combo_Params.flags);
    Combo_Params.flags = prev;
    packer->packOpaque(Combo_Params.data.str, Combo_Params.data.len);

    writeToStream();
    readFromStream();

    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        Combo_Resp.size = unpacker->unpackUint();
        if (Combo_Resp.error || (Combo_Resp.size != Combo_Params.data.len))
        {
            std::cout << "device_write error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
// vxi11 device_read() command
bool vxi11_client::device_read(char **str)
{
    strcpy(*str, "");
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "device_read no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(12);     // device_read is proc 12

    // pack device_write params
    packer->packInt(Combo_Params.lid);
    packer->packUint(DATA_SIZE);
    packer->packUint(Combo_Params.io_timeout);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.flags);
    packer->packInt(Combo_Params.termChar);

    writeToStream();
    readFromStream();

    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        Combo_Resp.reason = unpacker->unpackInt();
        uint32 len;
        const char *data = unpacker->unpackOpaque(&len);
        Combo_Resp.data.set(data, len);
        if (Combo_Resp.error)
        {
            std::cout << "device_read error " << getLastError() << std::endl;
            return false;
        }
        else
        {
            strncpy(*str, Combo_Resp.data.str, Combo_Resp.data.len);
            (*str)[Combo_Resp.data.len] = '\0';
            return true;
        }
    }
    else
        return false;
}
// vxi11 device_readstb() command
bool vxi11_client::device_readstb(unsigned char *stb)
{
    *stb = 0;
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(13);     // device_readstb is proc 13
    
    // pack generic params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.io_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        Combo_Resp.stb = unpacker->unpackUint();
        if (Combo_Resp.error)
        {
            std::cout << "device_readstb error " << getLastError() << std::endl;
            return false;
        }
        else
        {
            *stb = Combo_Resp.stb;
            return true;
        }
    }
    else
        return false;
}
// vxi11 device_trigger() command
bool vxi11_client::device_trigger()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(14);     // device_trigger is proc 14
    
    // pack generic params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.io_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_trigger error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}


// vxi11 device_clear() command
bool vxi11_client::device_clear()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(15);     // device_clear is proc 15
    
    // pack generic params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.io_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_clear error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
// vxi11 device_remote() command
bool vxi11_client::device_remote()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(16);     // device_remote is proc 16
    
    // pack generic params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.io_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_remote error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
// vxi11 device_local() command
bool vxi11_client::device_local()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(17);     // device_local is proc 17
    
    // pack generic params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    packer->packUint(Combo_Params.io_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_local error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
// vxi11 device_lock() command
bool vxi11_client::device_lock()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(18);     // device_lock is proc 18
    
    // pack device_lock params
    packer->packInt(Combo_Params.lid);
    packer->packUint(Combo_Params.flags);
    packer->packUint(Combo_Params.lock_timeout);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_lock error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
// vxi11 device_unlock() command
bool vxi11_client::device_unlock()
{
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(19);     // device_unlock is proc 19
    
    // pack device_unlock params
    packer->packInt(Combo_Params.lid);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "device_unlock error " << getLastError() << std::endl;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}
bool vxi11_client::create_intr_chan()
{
    return false;
}
bool vxi11_client::destroy_intr_chan(void)
{
    return false;
}
bool vxi11_client::device_enable_srq()
{
    return false;
}
Device_DocmdResp *vxi11_client::device_docmd()
{
    return NULL;
}
// vxi11 destroy_link() command
bool vxi11_client::destroy_link()
{
    CFRelease(device_addr);
    device_addr = NULL;
    if (Combo_Params.lid == -1)
    {
        // we must have an active link!
        std::cout << "destroy_link no valid link!" << std::endl;
        return false;
    }
    
    // pack rpc stuff
    setVXI11CoreProg();
    packRPC(23);     // destroy_link is proc 23
    
    // pack destroy_link params
    packer->packInt(Combo_Params.lid);
    
    writeToStream();
    readFromStream();
    
    if (unpackRPC())
    {
        // unpack response
        Combo_Resp.error = unpacker->unpackInt();
        if (Combo_Resp.error)
        {
            std::cout << "destroy_link error " << getLastError() << std::endl;
            return false;
        }
        else
        {
            closeStream();
            std::cout << "destroyed link" << std::endl;
            return true;
        }
    }
    else
        return false;
}


/*static*/ void vxi11_client::writeStreamCallback(CFWriteStreamRef stream, CFStreamEventType eventType, void *clientCallBackInfo)
{
    vxi11_client *lockin = (vxi11_client *)clientCallBackInfo;
    lockin->continueExec(eventType == kCFStreamEventCanAcceptBytes);
}

void vxi11_client::cancelConnect()
{
    connectToDevice2(false);
}
void vxi11_client::continueExec(bool ok)
{
    switch (reading)
    {
        case 1:
            ok = get_port2(ok);
            if (ok)
                ok = createVXI11Stream();
            else
                connectToDevice2(ok);
            break;
        case 2:
            if (ok)
                ok = create_link();
            if (ok)
                ok = create_link2(ok);
            connectToDevice2(ok);
            break;
        
        default:
            if (!ok)
            {
                std::cout << "Stream error or ended!" << std::endl;
                closeStream();
            }
    }
}
bool vxi11_client::get_port2(bool ok)
{
    if (ok)
    {
        writeToStream();
        readFromStream();
        
        // send & receive data
        if (unpackRPC())
        {
            port = unpacker->unpackInt();
            std::cout << "   vxi11 core port = " << port << std::endl;
        }
        else
            ok = false;
    }

    silence = true;
    closeStream();
    silence = false;
    return ok;
}
bool vxi11_client::create_link2(bool ok)
{
    if (ok)
    {
        writeToStream();
        readFromStream();
        
        if (unpackRPC())
        {
            // unpack response
            Combo_Resp.error = unpacker->unpackInt();
            Combo_Resp.lid = unpacker->unpackInt();
            Combo_Resp.abortPort = (short)unpacker->unpackInt();
            Combo_Resp.maxRecvSize = unpacker->unpackInt();
            if (Combo_Resp.error)
            {
                Combo_Params.lid = -1;
                Combo_Resp.lid = -1;
                Combo_Resp.maxRecvSize = DATA_SIZE;
                std::cout << "create_link error " << getLastError() << std::endl;
                return false;
            }
            else
            {
                Combo_Params.lid = Combo_Resp.lid;
                if (Combo_Resp.maxRecvSize > DATA_SIZE)     // limit to smaller of data_size or what vxi11 server says
                    Combo_Resp.maxRecvSize = DATA_SIZE;
                std::cout << "   link id " << Combo_Resp.lid << std::endl;
                return true;
            }
        }
        else
            return false;
    }
    else
        return ok;
}
void vxi11_client::connectToDevice2(bool ok)
{
    reading = 0;
    if (callbackFunc)
        (*callbackFunc)(ok, true);
}

/*static*/ char vxi11_client::errstring[36];
const char *vxi11_client::getLastError()
{
    strcpy(errstring, "");
    switch (Combo_Resp.error)
    {
        case 0:
            break;
        case 1:
            strcpy(errstring, "Syntax error");
            break;
        case 3:
            strcpy(errstring, "Device not accessible");
            break;
        case 4:
            strcpy(errstring, "Invalid link identifier");
            break;
        case 5:
            strcpy(errstring, "Parameter error");
            break;
        case 6:
            strcpy(errstring, "Channel not established");
            break;
        case 8:
            strcpy(errstring, "Operation not supported");
            break;
        case 9:
            strcpy(errstring, "Out of resources");
            break;
        case 11:
            strcpy(errstring, "Device locked by another link");
            break;
        case 12:
            strcpy(errstring, "No lock held by this link");
            break;
        case 15:
            strcpy(errstring, "I/O timeout");
            break;
        case 17:
            strcpy(errstring, "I/O error");
            break;
        case 21:
            strcpy(errstring, "Invalid address");
            break;
        case 23:
            strcpy(errstring, "Abort");
            break;
        case 29:
            strcpy(errstring, "Channel already established");
            break;
        default:
            strcpy(errstring, "Unknown");
            break;
    }
    return errstring;
}

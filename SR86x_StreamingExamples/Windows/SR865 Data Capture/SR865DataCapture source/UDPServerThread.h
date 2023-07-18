//---------------------------------------------------------------------------

#ifndef UDPServerThreadH
#define UDPServerThreadH

#include <Classes.hpp>
#include <SyncObjs.hpp>
#include <Sockets.hpp>
#include <fstream>
#include "PacketHeader.h"
//---------------------------------------------------------------------------

class UDPServerThread : public TThread
{
protected:
    int port;
    float liax, liay, liar, liath;
    unsigned int buffer[300];       // 1200 bytes for receiving UDP packet
    int byte_count;
    int counter;
    bool missed;
    bool over;
    ofstream fstream;
    PacketHeader hdr;
    bool csvFmt;
    int lastHeader;
    TMutex *serverMutex;
    TMutex *fileMutex;

    SOCKET sd;
    sockaddr_in server;
    sockaddr_in client;

public:
    UDPServerThread();
    virtual __fastcall ~UDPServerThread();

    void setPort(int inport);
    void setFileFmt(bool csv);
    void setFile(UnicodeString fname, bool trunc);
    bool fileIsOpen();
    void closeFile();
    void saveData(int nwords);
    virtual void __fastcall Execute(void);

    void stopServer();
    void startServer();
    bool serverOk();
    void gotData(int nwords);
    void getData(int *pwhat, int *prate, float *pliax, float *pliay, float *pliar, float *pliath, int *pbyte_count, bool *pmissed, bool *pover);
};

#endif

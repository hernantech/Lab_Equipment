//---------------------------------------------------------------------------


#pragma hdrstop

#include "UDPServerThread.h"
#include <time.h>
//---------------------------------------------------------------------------

#pragma package(smart_init)


// this thread receives streaming data from a UDP port
// it displays the first sample of data,
// and then saves the data to disk.
// For binary file format, the UDP packet is saved in native endian format, and includes the header.
// For ASCII file format, the data is saved in CSV format, with a date-time at the beginning, and a description of the data & data rate when they change.


// UDP packet format
//
// 32-bit int header
// Always Big-endian!
// bits 7-0 (8 bits) are packet counter
// ie sequential packets have counter incr by 1.
// bits 11-8 (4 bits) are what is contained in packet
//   0 = x-only (float), 1 = x&y (float) 2 = r&th (float), 3 = xyrth (float)
//   4 = x-only (int),   5 = x&y (int)   3 = r&th (int),   7 = xyrth (int)
// bits 15-12 (4bits) are packet length
//   0 = 1024 bytes follow header, 1 = 512bytes follow header, 2 = 256 bytes follow header, 3 = 128 bytes follow header
// bits 23-16 (8 bits) are sample rate
//   0 = 1.25MHz, 1 = 625kHz, 2 = 312.5kHz, ...
// bits 31-24 are status flags
//   bit 24 is an overload indicator, bit 25 is an error indicator
//   if set, an overload or error was detected in the _previous_ packet.
//   Overload indicates an input overload, sync overload, or output overload (if recording integer data)
//   Error indicates pll unlock, or sync error (if sync can't follow freq)
//   bit 28 is little-endian flag; if true, data is little-endian (data only!)
//   bit 29 is udp checksum flag; if true, udp checksum was sent
//
// Data array
// Data following the header is stored in big-endian format
// Float data is 32bit float,
// and must be endian-swapped in 32bit words.
// Integer data is 16bit signed integer,
// and must be endian-swapped in 16bit words.



UDPServerThread::UDPServerThread() : TThread(true)
{
    port = 1865;
    liax = 0.0;
    liay = 0.0;
    liar = 0.0;
    liath = 0.0;
    byte_count = 0;
    counter = -1;
    over = false;
    missed = false;
    csvFmt = false;
    hdr.setHeader(-1);

    sd = INVALID_SOCKET;
    serverMutex = new TMutex(true);     // mutex to handle exclusive access to udp server
    fileMutex = new TMutex(true);       // mutex to handle exclusive access to file object

    // start up UDP server (server RECEIVES data)
    WSAData wsdat;
    if (WSAStartup(0x0101, &wsdat) != 0)
    {
        fprintf(stderr, "Could not open Winsock connection.\n");
        exit(0);
    }
}
/*virtual*/ __fastcall UDPServerThread::~UDPServerThread()
{
    stopServer();
    WSACleanup();
    delete serverMutex;

    closeFile();
}

void UDPServerThread::setPort(int inport)
{
    stopServer();
    port = inport;
    startServer();
}
void UDPServerThread::setFileFmt(bool csv)
{
    if (csvFmt != csv)
    {
        fstream.close();
        csvFmt = csv;
    }
}
void UDPServerThread::setFile(UnicodeString fname, bool trunc)
{
    closeFile();
    fileMutex->Acquire();
    fstream.clear();
    fstream.open(fname.c_str(), ios::out | (csvFmt?0:ios::binary) | (trunc?ios::trunc:ios::app));
    if (!(fstream.is_open() && fstream.good() && !fstream.fail()))
        fstream.close();

    counter = -1;
    if (csvFmt)
    {
        time_t t = time(0);
        struct tm *now = localtime(&t);
        char dtbuff[80];
        strftime(dtbuff, 80, "%Y-%m-%d %H:%M", now);
        lastHeader = 0;
        fstream << dtbuff << std::endl;
        // specify float fmt
        // precision means (1 + prec) sig fig;
        // 24 bits of mantissa in float means 7.2 decimal digits,
        // so we need 8 sig fig to completely specify float
        // however, 6 sig fig (1ppm) is usually more than enough!
        // we could go as low as 5 sig fig (1 in 100,000), which is still better than 16bits
        fstream << std::scientific;
        fstream.precision(5);
    }
    fileMutex->Release();
}
bool UDPServerThread::fileIsOpen()
{
    return fstream.is_open();
}
void UDPServerThread::closeFile()
{
    fileMutex->Acquire();
    fstream.close();
    fileMutex->Release();
}

// thread's main execution loop
/*virtual*/ void __fastcall UDPServerThread::Execute(void)
{
    int client_length = (int)sizeof(struct sockaddr_in);
    int bytes_received;
    do
    {
        // receive up to 1200 bytes from UDP socket
        bytes_received = recvfrom(sd, (char *)buffer, 1200, 0, (struct sockaddr *)&client, &client_length);
        if (bytes_received < 0)
        {
            //fprintf(stderr, "Could not receive datagram.\n");
            Sleep(10);
        }
        else
        {
            // got packet data!
            byte_count += bytes_received;
            gotData(bytes_received >> 2);   // process data
        }
    }
    while (!Terminated);

}

void UDPServerThread::startServer()
{
    // create socket
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sd == INVALID_SOCKET)
    {
        fprintf(stderr, "Could not create socket.\n");
        WSACleanup();
        exit(0);
    }

    // bind to local address
    memset((void *)&server, '\0', sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    // any ip address this computer has (e.g ethernet ip + wifi ip)
    server.sin_addr.S_un.S_un_b.s_b1 = (unsigned char)0;
    server.sin_addr.S_un.S_un_b.s_b2 = (unsigned char)0;
    server.sin_addr.S_un.S_un_b.s_b3 = (unsigned char)0;
    server.sin_addr.S_un.S_un_b.s_b4 = (unsigned char)0;
    /* Bind address to socket */
    if (bind(sd, (struct sockaddr *)&server,
                         sizeof(struct sockaddr_in)) == -1)
    {
        fprintf(stderr, "Could not bind to port %d.\n",port);
        closesocket(sd);
        sd = INVALID_SOCKET;
        //WSACleanup();
        //exit(0);
        throw Exception("Could not bind to UDP port.");
    }
}
void UDPServerThread::stopServer()
{
    serverMutex->Acquire();
    closesocket(sd);
    sd = INVALID_SOCKET;
    serverMutex->Release();
}
bool UDPServerThread::serverOk()
{
    return (sd != INVALID_SOCKET);
}

// process UDP packet
// here, we record first data point(s)
// and save the packet to disk
void UDPServerThread::gotData(int nwords)   // number of 32bit words
{
    serverMutex->Acquire();

    bool ok = true;

    // do network transformation
    // ie big-endian to little-endian
    // header is always big-endian
    buffer[0] = ntohl(buffer[0]);        // ntohl() does network (big-endian) to host (little-endian) conversion of a 32bit word

    // interpret header
    hdr.setHeader(buffer[0]);
    if (!hdr.littleEnd)
    {
        if (hdr.what >= 4)
        {
            // int: convert 16bits at a time
            unsigned short *sarr = (unsigned short *)(buffer + 1);
            int nshorts = (hdr.byteLength() >> 1);
            for (int i=0;i<nshorts;++i)
                sarr[i] = ntohs(sarr[i]);           // ntohs() does network (big-endian) to host (little-endian) conversion of a 16bit word
        }
        else
        {
            // float: convert 32bit word at a time
            unsigned int *warr = (unsigned int *)(buffer + 1);
            int nints = (hdr.byteLength() >> 2);
            for (int i=0;i<nints;++i)
                warr[i] = ntohl(warr[i]);        // ntohl() does network (big-endian) to host (little-endian) conversion of a 32bit word
        }
    }

    // check for dropped packet
    // is (last packet counter + 1)%256 == this packet counter?
    int counter2 = hdr.counter;
    if (counter >= 0)
    {
        counter = (counter + 1)%256;
        if (counter != counter2)
        {
            missed = true;
            if (csvFmt)
            {
                counter = counter2 - counter;
                if (counter < 0)
                    counter += 256;
                fstream << "Dropped " << counter << " packets!" << std::endl;
            }
        }
    }
    counter = counter2;

    // union for interpreting 32bit data word as different types
    union
    {
        unsigned int ival;      // 32bit word as unsigned int
        float fval;             // 32bit word as float
        short sval[2];          // 32bit word as 2 short ints
                                // sval[0] is the first int, and sval[1] is the second int
    }
    dat;
    // Grab data at beginning of packet
    switch (hdr.what)
    {
        default:
            // x-only (float)
            dat.ival = buffer[1];
            liax = dat.fval;                // interpret as float
            liay = liar = liath = 0.0;
            break;
        case 1:
            // x&y (float)
            dat.ival = buffer[1];
            liax = dat.fval;                // interpret as float
            dat.ival = buffer[2];
            liay = dat.fval;                // interpret as float
            liar = liath = 0.0;
            break;
        case 2:
            // r&th (float)
            dat.ival = buffer[1];
            liar = dat.fval;                // interpret as float
            dat.ival = buffer[2];
            liath = dat.fval;               // interpret as float
            liax = liay = 0.0;
            break;
        case 3:
            // xyr&th (float)
            dat.ival = buffer[1];
            liax = dat.fval;                // interpret as float
            dat.ival = buffer[2];
            liay = dat.fval;                // interpret as float
            dat.ival = buffer[3];
            liar = dat.fval;                // interpret as float
            dat.ival = buffer[4];
            liath = dat.fval;               // interpret as float
            break;

        case 4:
            // x-only (int)
            dat.ival = buffer[1];
            liax = dat.sval[0];             // interpret as short int
            liay = liar = liath = 0.0;
            break;
        case 5:
            // x&y (int)
            dat.ival = buffer[1];
            liax = dat.sval[0];             // interpret as short int
            liay = dat.sval[1];             // interpret as short int
            liar = liath = 0.0;
            break;
        case 6:
            // r&th (int)
            dat.ival = buffer[1];
            liar = dat.sval[0];             // interpret as short int
            liath = dat.sval[1];            // interpret as short int
            liax = liay = 0.0;
            break;
        case 7:
            // xyr&th (int)
            dat.ival = buffer[1];
            liax = dat.sval[0];             // interpret as short int
            liay = dat.sval[1];             // interpret as short int
            dat.ival = buffer[2];
            liar = dat.sval[0];             // interpret as short int
            liath = dat.sval[1];            // interpret as short int
            break;
    }

    if (!over)
        over = hdr.over || !ok;

    // save file
    // data saved in native endian format
    saveData(nwords);
    serverMutex->Release();
}
void UDPServerThread::saveData(int nwords)
{
    fileMutex->Acquire();
    if (fstream.is_open())
    {
        if (csvFmt)
        {
            // comma separated values (ASCII) format
            // did data content or rate change?
            if ((lastHeader ^ hdr.getHeader()) & 0x00ff0f00)
            {
                lastHeader = hdr.getHeader();
                switch (hdr.what)
                {
                    default: fstream << "X (float)"; break;
                    case 1: fstream << "X,Y (float)"; break;
                    case 2: fstream << "R,theta (float)"; break;
                    case 3: fstream << "X,Y,R,theta (float)"; break;
                    case 4: fstream << "X (int)"; break;
                    case 5: fstream << "X,Y (int)"; break;
                    case 6: fstream << "R,theta (int)"; break;
                    case 7: fstream << "X,Y,R,theta (int)"; break;
                }
                fstream << " @ " << hdr.sampleRate() << " Hz" << std::endl;
            }

            union
            {
                unsigned int raw;
                float fval;
                short sval[2];
            }
            dat;
            for (int i=1;i<nwords;)
            {
                dat.raw = buffer[i];
                switch (hdr.what)
                {
                    default:
                        // x only (float)
                        fstream << dat.fval << std::endl;
                        ++i;
                        break;
                    case 1:
                    case 2:
                        // x&y or r&th (float)
                        fstream << dat.fval << ",";
                        dat.raw = buffer[i+1];
                        fstream << dat.fval << std::endl;
                        i += 2;
                        break;
                    case 3:
                        // xyr&th (float)
                        fstream << dat.fval << ",";
                        dat.raw = buffer[i+1];
                        fstream << dat.fval << ",";
                        dat.raw = buffer[i+2];
                        fstream << dat.fval << ",";
                        dat.raw = buffer[i+3];
                        fstream << dat.fval << std::endl;
                        i += 4;
                        break;

                    case 4:
                        // x-only (int)
                        fstream << dat.sval[0] << std::endl << dat.sval[1] << std::endl;
                        ++i;
                        break;
                    case 5:
                    case 6:
                        // x&y or r&th (int)
                        fstream << dat.sval[0] << "," << dat.sval[1] << std::endl;
                        ++i;
                        break;
                    case 7:
                        // xyr&th (int)
                        fstream << dat.sval[0] << "," << dat.sval[1] << ",";
                        dat.raw = buffer[i+1];
                        fstream << dat.sval[0] << "," << dat.sval[1] << std::endl;
                        i += 2;
                        break;
                }
            }
        }
        else
            fstream.write((char *)buffer, nwords << 2);     // binary data; save entire udp packet to disk
    }
    fileMutex->Release();
}
void UDPServerThread::getData(int *pwhat, int *prate, float *pliax, float *pliay, float *pliar, float *pliath, int *pbyte_count, bool *pmissed, bool *pover)
{
    // return latest data to user interface
    *pwhat = hdr.what;
    *prate = hdr.rate;
    *pliax = liax;
    *pliay = liay;
    *pliar = liar;
    *pliath = liath;
    *pbyte_count = byte_count;
    *pmissed = missed;
    *pover = over;

    byte_count = 0;
    missed = false;
    over = false;
}


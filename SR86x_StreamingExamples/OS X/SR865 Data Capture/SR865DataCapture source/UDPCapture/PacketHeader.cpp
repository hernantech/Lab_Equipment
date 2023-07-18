//---------------------------------------------------------------------------


#pragma hdrstop

#include "PacketHeader.h"
#include <math.h>

//---------------------------------------------------------------------------

#pragma package(smart_init)

// Helper class to interpret udp packet header


PacketHeader::PacketHeader(unsigned int head)
{
    setHeader(head);
}

void PacketHeader::setHeader(unsigned int hd)
{
    hdr = hd;
    counter = (hd & 0x000000ff);           // counter; bottom 8 bits
    what    = (hd & 0x00000f00) >> 8;      // content; next 4 bits
    length  = (hd & 0x0000f000) >> 12;     // length;  next 4 bits
    rate    = (hd & 0x00ff0000) >> 16;     // sample rate; next 8 bits
    over    = (hd & 0x03000000);           // overload/error; bits 24 & 25
    littleEnd = (hd & 0x10000000);         // little-end data
    parityEn  = (hd & 0x20000000);         // error-correcting checksum
}
unsigned int PacketHeader::getHeader() const
{
    return hdr;
}

bool PacketHeader::isGood() const
{
    return (what<=7 && length<=3 && rate>=0 && rate<=31);
}
int PacketHeader::byteLength() const
{
    return (1024 >> length);
}
double PacketHeader::sampleRate() const
{
    return (1.25e6 / pow(2.0, rate));
}

bool PacketHeader::dataErr() const
{
    return over;
}


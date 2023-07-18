//---------------------------------------------------------------------------

#ifndef PacketHeaderH
#define PacketHeaderH


class PacketHeader
{
protected:
    unsigned int hdr;

public:
    int counter;
    int what, length, rate;
    bool over;
    bool littleEnd;
    // bool parityEn;

public:
    PacketHeader(unsigned int head=0);

    unsigned int getHeader() const;
    void setHeader(unsigned int hd);
    bool isGood() const;
    int byteLength() const;
    double sampleRate() const;
};

//---------------------------------------------------------------------------
#endif

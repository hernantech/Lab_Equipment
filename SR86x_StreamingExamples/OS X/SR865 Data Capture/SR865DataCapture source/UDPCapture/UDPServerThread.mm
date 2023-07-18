//
//  UDPServerThread.m
//  UDPCapture
//
//  Created by Ian Chan on 2/1/14.
//  Copyright (c) 2015 Stanford Research Systems. All rights reserved.
//

// UDP packet header format
// Big-endian!!!
// 32-bit int header
// bits 7-0 (8 bits) are packet counter
// ie sequential packets have counter incr by 1.
// bits 11-8 (4 bits) are what is contained in packet
// 0 = x-only (float), 1 = x&y (float) 2 = r&th (float), 3 = xyrth (float)
// 4 = x-only (int),   5 = x&y (int)   3 = r&th (int),   7 = xyrth (int)
// bits 15-12 (4bits) are packet length
// 0 = 1024 bytes follow header, 1 = 512bytes follow header, 2 = 256 bytes follotimw header, 3 = 128 bytes follow header
// bits 23-16 (8 bits) are sample rate
// 0 = 1.25MHz, 1 = 625kHz, 2 = 312.5kHz, ...
// bits 31-24 are status flags
// bit 24 is an overload indicator, bit 25 is an error indicator
// if set, an overload or error was detected in the previous packet.
// Overload indicates an input overload, sync overload, or output overload (if recording integer data)
// Error indicates pll unlock, or sync error (if sync can't follow freq)
// bit 28 is the little-endian flag, bit 29 is the udp checksum flag.
// If bit 28 is set, the data only (not the header) are little-endian.
// If bit 29 is set, the UDP checksum is being sent;
// if an error is detected, the packet is automatically dropped before it gets to us.
//
// Data array
// Data following the header is stored in big-endian format
// Float data is 32bit float,
// and must be endian-swapped in 32bit words.
// Integer data is 16bit signed integer,
// and must be endian-swapped in 16bit words.

// Saved data is in little-endian format (native Intel Mac format).
// The entire UDP packet is saved, including the header.
//
// Note: int on a Mac is 32bits, long on a Mac is 64bits.


#import "UDPServerThread.h"
#include <netdb.h>
#include <fstream>

#pragma mark * Utilities

// Returns a dotted decimal string for the specified address (a (struct sockaddr)
// within the address NSData).
static NSString * DisplayAddressForAddress(NSData * address)
{
    int         err;
    NSString *  result;
    char        hostStr[NI_MAXHOST];
    char        servStr[NI_MAXSERV];
    
    result = nil;
    
    if (address != nil) {
        
        // If it's a IPv4 address embedded in an IPv6 address, just bring it as an IPv4
        // address.  Remember, this is about display, not functionality, and users don't
        // want to see mapped addresses.
        
        if ([address length] >= sizeof(struct sockaddr_in6)) {
            const struct sockaddr_in6 * addr6Ptr;
            
            addr6Ptr = (const struct sockaddr_in6 *)[address bytes];
            if (addr6Ptr->sin6_family == AF_INET6) {
                if ( IN6_IS_ADDR_V4MAPPED(&addr6Ptr->sin6_addr) || IN6_IS_ADDR_V4COMPAT(&addr6Ptr->sin6_addr) ) {
                    struct sockaddr_in  addr4;
                    
                    memset(&addr4, 0, sizeof(addr4));
                    addr4.sin_len         = sizeof(addr4);
                    addr4.sin_family      = AF_INET;
                    addr4.sin_port        = addr6Ptr->sin6_port;
                    addr4.sin_addr.s_addr = addr6Ptr->sin6_addr.__u6_addr.__u6_addr32[3];
                    address = [NSData dataWithBytes:&addr4 length:sizeof(addr4)];
                    assert(address != nil);
                }
            }
        }
        err = getnameinfo((const struct sockaddr *)[address bytes], (socklen_t) [address length], hostStr, sizeof(hostStr), servStr, sizeof(servStr), NI_NUMERICHOST | NI_NUMERICSERV);
        if (err == 0) {
            result = [NSString stringWithFormat:@"%s:%s", hostStr, servStr];
            assert(result != nil);
        }
    }
    
    return result;
}

// Given an NSError, returns a short error string that we can print, handling
// some special cases along the way.
static NSString * DisplayErrorFromError(NSError *error)
{
    NSString *      result;
    NSNumber *      failureNum;
    int             failure;
    const char *    failureStr;
    
    assert(error != nil);
    
    result = nil;
    
    // Handle DNS errors as a special case.
    
    if ( [[error domain] isEqual:(NSString *)kCFErrorDomainCFNetwork] && ([error code] == kCFHostErrorUnknown) ) {
        failureNum = [[error userInfo] objectForKey:(id)kCFGetAddrInfoFailureKey];
        if ( [failureNum isKindOfClass:[NSNumber class]] ) {
            failure = [failureNum intValue];
            if (failure != 0) {
                failureStr = gai_strerror(failure);
                if (failureStr != NULL) {
                    result = [NSString stringWithUTF8String:failureStr];
                    assert(result != nil);
                }
            }
        }
    }
    
    // Otherwise try various properties of the error object.
    
    if (result == nil) {
        result = [error localizedFailureReason];
    }
    if (result == nil) {
        result = [error localizedDescription];
    }
    if (result == nil) {
        result = [error description];
    }
    assert(result != nil);
    return result;
}


#pragma mark * ServerThread

@interface UDPServerThread ()
@property (nonatomic, strong, readwrite) UDPServe *echo;
@property (atomic, strong) NSLock *streamLock;
@property (atomic, strong) NSLock *udpLock;
@property (atomic, readwrite) unsigned int *buffer;
@property (nonatomic, readwrite) PacketHeader *hdr;
@property (nonatomic, readwrite) int port;
@property (atomic, readwrite) int index;
@property (atomic, readwrite) bool overload;
@property (atomic, readwrite) bool missed;
@property (atomic, readwrite) int count;
@property (atomic, readwrite) float liaX;
@property (atomic, readwrite) float liaY;
@property (atomic, readwrite) float liaR;
@property (atomic, readwrite) float liaTh;
@property (atomic, readwrite) bool csvFmt;
@property (atomic, readwrite) unsigned int lastHeader;
@end

@implementation UDPServerThread

std::ofstream stream;
- (void)dealloc
{
    [self stopServer];
    stream.close();
}

// This creates a UDPEcho
// object and runs it in server mode (listens for incoming data).
- (void)main
{
    _streamLock = [[NSLock alloc] init];
    _udpLock = [[NSLock alloc] init];
    
    _hdr = new PacketHeader;
    _hdr->setHeader(-1);
    
    assert(self.echo == nil);
    self.echo = [[UDPServe alloc] init];
    assert(self.echo != nil);
    self.echo.delegate = self;
    //[self startServer];
    
    // while (self.echo != nil)
    {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
    }
    
    // The loop above is supposed to run forever.  If it doesn't, something must
    // have failed and we want main to return EXIT_FAILURE.
}

- (void)stopServer
{
    if (self.echo)
        [self.echo stop];

}
- (void)startServer
{
    if (self.echo)
        [self.echo startServerOnPort:_port];
}
- (bool)serverOk
{
    if (self.echo)
        return self.echo.port;
    else
        return 0;
}

- (void)setPort:(int)inport
{
    [_udpLock lock];
    [self stopServer];
    _port = inport;
    [self startServer];
    [_udpLock unlock];
}

- (void)setFileFmt:(bool)csv
{
    if (_csvFmt != csv)
    {
        [self closeFile];
        _csvFmt = csv;
    }
}
- (void)setFile:(NSString *)fname truncate:(bool)trunc
{
    [self closeFile];
    [_streamLock lock];
    _index = -1;
    stream.clear();
    stream.open([fname UTF8String], (_csvFmt?0:std::ios::binary) | (trunc?std::ios::trunc:std::ios::app));
    if (_csvFmt)
    {
        time_t t = time(0);
        struct tm *now = localtime(&t);
        char dtbuff[80];
        strftime(dtbuff, 80, "%Y-%m-%d %H:%M", now);
        stream << dtbuff << std::endl;
        _lastHeader = 0;
        // specify float fmt
        // precision means (1 + prec) sig fig;
        // 24 bits of mantissa in float means 7.2 decimal digits,
        // so we need 8 sig fig to completely specify float
        // however, 6 sig fig (1ppm) is usually more than enough!
        // we could go as low as 5 sig fig (1 in 100,000), which is still better than 16bits
        stream << std::scientific;
        stream.precision(5);
    }
    [_streamLock unlock];
}
- (bool)fileIsOpen
{
    return stream.is_open();
}
- (void)closeFile
{
    [_streamLock lock];
    stream.close();
    [_streamLock unlock];
}

// process received data
// we just record data at beginning of packet
// and save packet to disk
- (void) gotData:(unsigned int)nwords
{
    [_udpLock lock];
    
    // do network transformation
    // ie big-endian to little-endian
    // header is always big-endian
    _buffer[0] = ntohl(_buffer[0]);     // ntohl() does network (big-endian) to host (little-endian) of 32bit word

    // interpret header
    _hdr->setHeader(_buffer[0]);

    // little-endian flag applies to data only!
    // it does not apply to header
    unsigned int dwords = (_hdr->byteLength() >> 2) + 1; // data words + header
    if (!_hdr->littleEnd)
    {
        if (_hdr->what < 4)
        {
            // float: convert 32bit word at a time
            for (int i=1;i<dwords;++i)
                _buffer[i] = ntohl(_buffer[i]);        // ntohl() does network (big-endian) to host (little-endian) conversion of a 32bit word
        }
        else
        {
            // int: convert 16bits at a time
            unsigned short *sarr = (unsigned short *)_buffer;
            int nshorts = dwords << 1;
            for (int i=2;i<nshorts;++i)
                sarr[i] = ntohs(sarr[i]);           // ntohs() does network (big-endian) to host (little-endian) conversion of a 16bit word
        }
    }
    
    // check for dropped packet
    // is (last packet counter + 1)%256 == this packet counter?
    int idx = _hdr->counter;
    if (_index >= 0)
    {
        _index = (_index + 1)%256;
        if (_index != idx)
        {
            _missed = true;
            _index = idx - _index;
            if (_index < 0)
                _index += 256;
            stream << "Dropped " << _index << " packets!" << std::endl;
            NSLog([NSString stringWithFormat:@"dropped %d", _index]);
        }
    }
    _index = idx;
    
    // overload
    if (!_overload)
        _overload = _hdr->over;
    
    
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
    switch (_hdr->what)
    {
        case 0:
            // x-only (float)
            dat.ival = _buffer[1];
            _liaX = dat.fval;         // interpret as float
            _liaY = _liaR = _liaTh = 0.0;
            break;
        case 1:
            // xy (float)
            dat.ival = _buffer[1];
            _liaX = dat.fval;         // interpret as float
            dat.ival = _buffer[2];
            _liaY = dat.fval;         // interpret as float
            _liaR = _liaTh = 0.0;
            break;
        case 2:
            // rth (float)
            dat.ival = _buffer[1];
            _liaR = dat.fval;         // interpret as float
            dat.ival = _buffer[2];
            _liaTh = dat.fval;        // interpret as float
            _liaX = _liaY = 0.0;
            break;
        case 3:
            // xyrth (float)
            dat.ival = _buffer[1];
            _liaX = dat.fval;         // interpret as float
            dat.ival = _buffer[2];
            _liaY = dat.fval;         // interpret as float
            dat.ival = _buffer[3];
            _liaR = dat.fval;         // interpret as float
            dat.ival = _buffer[4];
            _liaTh = dat.fval;        // interpret as float
            break;

        case 4:
            // x-only (int)
            dat.ival = _buffer[1];
            _liaX = dat.sval[0];      // interpret as int
            _liaY = _liaR = _liaTh = 0.0;
            break;
        case 5:
            // xy (int)
            dat.ival = _buffer[1];
            _liaX = dat.sval[0];      // interpret as int
            _liaY = dat.sval[1];      // interpret as int
            _liaR = _liaTh = 0.0;
            break;
        case 6:
            // rth (int)
            dat.ival = _buffer[1];
            _liaR = dat.sval[0];      // interpret as int
            _liaTh = dat.sval[1];     // interpret as int
            _liaX = _liaY = 0.0;
            break;
        case 7:
            // xyrth (int)
            dat.ival = _buffer[1];
            _liaX = dat.sval[0];      // interpret as int
            _liaY = dat.sval[1];      // interpret as int
            dat.ival = _buffer[2];
            _liaR = dat.sval[0];      // interpret as int
            _liaTh = dat.sval[1];     // interpret as int
            break;
        default:
            _liaX = _liaY = _liaR = _liaTh = 0.0;
            break;
    }

    
    // save file
    // data saved in native endian format
    [self saveData:nwords];
    [_udpLock unlock];
}
- (void)saveData:(unsigned int)nwords
{
    [_streamLock lock];
    if (stream.is_open())
    {
        if (_csvFmt)
        {
            // comma separated values (ASCII) format
            // did data content or rate change?
            if ((_lastHeader ^ _hdr->getHeader()) & 0x00ff0f00)
            {
                _lastHeader = _hdr->getHeader();
                switch (_hdr->what)
                {
                    default: stream << "X (float)"; break;
                    case 1: stream << "X,Y (float)"; break;
                    case 2: stream << "R,theta (float)"; break;
                    case 3: stream << "X,Y,R,theta (float)"; break;
                    case 4: stream << "X (int)"; break;
                    case 5: stream << "X,Y (int)"; break;
                    case 6: stream << "R,theta (int)"; break;
                    case 7: stream << "X,Y,R,theta (int)"; break;
                }
                stream << " @ " << _hdr->sampleRate() << " Hz" << std::endl;
            }
            
            unsigned int dwords = nwords;
            union
            {
                unsigned int raw;
                float fval;
                short sval[2];
            }
            dat;
            for (unsigned int i=1;i<dwords;)
            {
                dat.raw = _buffer[i];
                switch (_hdr->what)
                {
                    default:
                        // x-only (float)
                        stream << dat.fval << std::endl;
                        ++i;
                        break;
                    case 1:
                    case 2:
                        // x&y or r&th (float)
                        stream << dat.fval << ",";
                        dat.raw = _buffer[i+1];
                        stream << dat.fval << std::endl;
                        i += 2;
                        break;
                    case 3:
                        // xyr&th (float)
                        stream << dat.fval << ",";
                        dat.raw = _buffer[i+1];
                        stream << dat.fval << ",";
                        dat.raw = _buffer[i+2];
                        stream << dat.fval << ",";
                        dat.raw = _buffer[i+3];
                        stream << dat.fval << std::endl;
                        i += 4;
                        break;
                        
                    case 4:
                        // x-only (int)
                        stream << dat.sval[0] << std::endl << dat.sval[1] << std::endl;
                        ++i;
                        break;
                    case 5:
                    case 6:
                        // x&y or r&th (int)
                        stream << dat.sval[0] << "," << dat.sval[1] << std::endl;
                        ++i;
                        break;
                    case 7:
                        // xyr&th (int)
                        stream << dat.sval[0] << "," << dat.sval[1] << ",";
                        dat.raw = _buffer[i+1];
                        stream << dat.sval[0] << "," << dat.sval[1] << std::endl;
                        i += 2;
                        break;
                }
            }
        }
        else
            stream.write((char *)_buffer, nwords << 2);     // binary data, incl header
    }
    [_streamLock unlock];
}
- (void)getData:(int *)pwhat rate:(int *)prate liax:(float *)pliax liay:(float *)pliay liar:(float *)pliar liath:(float *)pliath count:(int *)pbyte_count missed:(bool *)pmissed overload:(bool *)pover
{
    *pwhat = _hdr->what;
    *prate = _hdr->rate;
    *pliax = _liaX;
    *pliay = _liaY;
    *pliar = _liaR;
    *pliath = _liaTh;
    *pbyte_count = _count;
    *pmissed = _missed;
    *pover = _overload;
    
    _count = 0;
    _missed = false;
    _overload = false;
}


// UDPServerDelegate protocol
// process UDP packet
- (void)echo:(UDPServe *)echo didReceiveData:(uint8_t *)data ofLength:(NSUInteger)len fromAddress:(NSData *)addr
{
    assert(echo == self.echo);
#pragma unused(echo)
    assert(data != nil);
    assert(addr != nil);
    //NSLog(@"received %@ from %@", DisplayStringFromData(data), DisplayAddressForAddress(addr));

    _buffer = (unsigned int *)data;
    unsigned int len32 = (unsigned int)len;
    _count += len32;
    [self gotData:(len32 >> 2)];
}

- (void)echo:(UDPServe *)echo didReceiveError:(NSError *)error
// This UDPEcho delegate method is called after a failure to receive data.
{
    assert(echo == self.echo);
#pragma unused(echo)
    assert(error != nil);
    NSLog(@"received error: %@", DisplayErrorFromError(error));
}

- (void)echo:(UDPServe *)echo didStartWithAddress:(NSData *)address
// This UDPEcho delegate method is called after the object has successfully started up.
{
    assert(echo == self.echo);
#pragma unused(echo)
    assert(address != nil);
    
    NSLog(@"receiving on %@", DisplayAddressForAddress(address));
}

- (void)echo:(UDPServe *)echo didStopWithError:(NSError *)error
// This UDPEcho delegate method is called  after the object stops spontaneously.
{
    assert(echo == self.echo);
#pragma unused(echo)
    assert(error != nil);
    NSLog(@"failed with error: %@", DisplayErrorFromError(error));
    //self.echo = nil;
}

@end

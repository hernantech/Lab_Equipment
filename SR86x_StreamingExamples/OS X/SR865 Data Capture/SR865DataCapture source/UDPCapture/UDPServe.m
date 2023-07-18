
#if ! defined(UDPSERVE_IPV4_ONLY)
    #define UDPSERVE_IPV4_ONLY 0
#endif

#import "UDPServe.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

@interface UDPServe ()

// redeclare as readwrite for private use

@property (nonatomic, copy,   readwrite) NSString *             hostName;
@property (nonatomic, copy,   readwrite) NSData *               hostAddress;
@property (nonatomic, assign, readwrite) NSUInteger             port;

// forward declarations

- (void)stopHostResolution;
- (void)stopWithError:(NSError *)error;
- (void)stopWithStreamError:(CFStreamError)streamError;

@end

@implementation UDPServe
{
    CFHostRef               _cfHost;
    CFSocketRef             _cfSocket;
}

@synthesize delegate    = _delegate;
@synthesize port        = _port;

- (id)init
{
    self = [super init];
    if (self != nil) {
        // do nothing
    }
    return self;
}

- (void)dealloc
{
    [self stop];
}

- (BOOL)isServer
{
    return true;
}

uint8_t                 buffer[2048];
- (void)readData
    // Called by the CFSocket read callback to actually read and process data 
    // from the socket.
{
    int                     err;
    int                     sock;
    struct sockaddr_storage addr;
    socklen_t               addrLen;
    ssize_t                 bytesRead;
    
    sock = CFSocketGetNative(self->_cfSocket);
    assert(sock >= 0);
    
    addrLen = sizeof(addr);
    bytesRead = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr, &addrLen);
    if (bytesRead < 0) {
        err = errno;
    } else if (bytesRead == 0) {
        err = EPIPE;
    } else {
        NSData *    addrObj;

        err = 0;

        addrObj = [NSData dataWithBytes:&addr  length:addrLen  ];
        assert(addrObj != nil);

        // We got UDP packet!
        // Tell the delegate about the data.
        if ( (self.delegate != nil) && [self.delegate respondsToSelector:@selector(echo:didReceiveData:ofLength:fromAddress:)] )
        {
            [self.delegate echo:self didReceiveData:buffer ofLength:bytesRead fromAddress:addrObj];
        }
    }
    
    // If we got an error, tell the delegate.
    
    if (err != 0) {
        if ( (self.delegate != nil) && [self.delegate respondsToSelector:@selector(echo:didReceiveError:)] )
        {
            [self.delegate echo:self didReceiveError:[NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil]];
        }
    }
}

static void SocketReadCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
    // This C routine is called by CFSocket when there's data waiting on our 
    // UDP socket.  It just redirects the call to Objective-C code.
{
    UDPServe *       obj;
    
    obj = (__bridge UDPServe *) info;
    assert([obj isKindOfClass:[UDPServe class]]);
    
    #pragma unused(s)
    assert(s == obj->_cfSocket);
    #pragma unused(type)
    assert(type == kCFSocketReadCallBack);
    #pragma unused(address)
    assert(address == nil);
    #pragma unused(data)
    assert(data == nil);
    
    [obj readData];
}

#if UDPSERVE_IPV4_ONLY
    
    - (BOOL)setupSocketConnectedToAddress:(NSData *)address port:(NSUInteger)port error:(NSError **)errorPtr
        // Sets up the CFSocket in either client or server mode.  In client mode, 
        // address contains the address that the socket should be connected to. 
        // The address contains zero port number, so the port parameter is used instead. 
        // In server mode, address is nil and the socket is bound to the wildcard 
        // address on the specified port.
    {
        int                     err;
        int                     junk;
        int                     sock;
        const CFSocketContext   context = { 0, (__bridge void *)(self), NULL, NULL, NULL };
        CFRunLoopSourceRef      rls;
        
        assert( (address == nil) == self.isServer );
        assert( (address == nil) || ([address length] <= sizeof(struct sockaddr_storage)) );
        assert(port < 65536);
        
        assert(self->_cfSocket == NULL);
        
        // Create the UDP socket itself.
        
        err = 0;
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            err = errno;
        }
        
        // Bind or connect the socket, depending on whether we're in server or client mode.
        
        if (err == 0) {
            struct sockaddr_in      addr;

            memset(&addr, 0, sizeof(addr));
            if (address == nil) {
                // Server mode.  Set up the address based on the socket family of the socket 
                // that we created, with the wildcard address and the caller-supplied port number.
                addr.sin_len         = sizeof(addr);
                addr.sin_family      = AF_INET;
                addr.sin_port        = htons(port);
                addr.sin_addr.s_addr = INADDR_ANY;
                err = bind(sock, (const struct sockaddr *) &addr, sizeof(addr));
            } else {
                // Client mode.  Set up the address on the caller-supplied address and port 
                // number.
                if ([address length] > sizeof(addr)) {
                    assert(NO);         // very weird
                    [address getBytes:&addr length:sizeof(addr)];
                } else {
                    [address getBytes:&addr length:[address length]];
                }
                assert(addr.sin_family == AF_INET);
                addr.sin_port = htons(port);
                err = connect(sock, (const struct sockaddr *) &addr, sizeof(addr));
            }
            if (err < 0) {
                err = errno;
            }
        }
        
        // From now on we want the socket in non-blocking mode to prevent any unexpected 
        // blocking of the main thread.  None of the above should block for any meaningful 
        // amount of time.
        
        if (err == 0) {
            int flags;
            
            flags = fcntl(sock, F_GETFL);
            err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            if (err < 0) {
                err = errno;
            }
        }
        
        // Wrap the socket in a CFSocket that's scheduled on the runloop.
        
        if (err == 0) {
            self->_cfSocket = CFSocketCreateWithNative(NULL, sock, kCFSocketReadCallBack, SocketReadCallback, &context);

            // The socket will now take care of cleaning up our file descriptor.

            assert( CFSocketGetSocketFlags(self->_cfSocket) & kCFSocketCloseOnInvalidate );
            sock = -1;

            rls = CFSocketCreateRunLoopSource(NULL, self->_cfSocket, 0);
            assert(rls != NULL);
            
            CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
            
            CFRelease(rls);
        }
        
        // Handle any errors.
        
        if (sock != -1) {
            junk = close(sock);
            assert(junk == 0);
        }
        assert( (err == 0) == (self->_cfSocket != NULL) );
        if ( (self->_cfSocket == NULL) && (errorPtr != NULL) ) {
            *errorPtr = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
        }
        
        return (err == 0);
    }

#else   // ! UDPSERVE_IPV4_ONLY

    - (BOOL)setupSocketConnectedToAddress:(NSData *)address port:(NSUInteger)port error:(NSError **)errorPtr
        // Sets up the CFSocket in either client or server mode.  In client mode, 
        // address contains the address that the socket should be connected to. 
        // The address contains zero port number, so the port parameter is used instead. 
        // In server mode, address is nil and the socket is bound to the wildcard 
        // address on the specified port.
    {
        sa_family_t             socketFamily;
        int                     err;
        int                     junk;
        int                     sock;
        const CFSocketContext   context = { 0, (__bridge void *) (self), NULL, NULL, NULL };
        CFRunLoopSourceRef      rls;
        
        assert( (address == nil) == self.isServer );
        assert( (address == nil) || ([address length] <= sizeof(struct sockaddr_storage)) );
        assert(port < 65536);
        
        assert(self->_cfSocket == NULL);
        
        // Create the UDP socket itself.  First try IPv6 and, if that's not available, revert to IPv6. 
        //
        // IMPORTANT: Even though we're using IPv6 by default, we can still work with IPv4 due to the 
        // miracle of IPv4-mapped addresses.
        
        err = 0;
        sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock >= 0) {
            socketFamily = AF_INET6;
        } else {
            sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                socketFamily = AF_INET;
            } else {
                err = errno;
                socketFamily = 0;       // quietens a warning from the compiler
                assert(err != 0);       // Obvious, but it quietens a warning from the static analyser.
            }
        }
        
        // Bind or connect the socket, depending on whether we're in server or client mode.
        
        if (err == 0) {
            struct sockaddr_storage addr;
            struct sockaddr_in *    addr4;
            struct sockaddr_in6 *   addr6;

            addr4 = (struct sockaddr_in * ) &addr;
            addr6 = (struct sockaddr_in6 *) &addr;

            memset(&addr, 0, sizeof(addr));
            if (address == nil) {
                // Server mode.  Set up the address based on the socket family of the socket 
                // that we created, with the wildcard address and the caller-supplied port number.
                addr.ss_family = socketFamily;
                if (socketFamily == AF_INET) {
                    addr4->sin_len         = sizeof(*addr4);
                    addr4->sin_port        = htons(port);
                    addr4->sin_addr.s_addr = INADDR_ANY;
                } else {
                    assert(socketFamily == AF_INET6);
                    addr6->sin6_len         = sizeof(*addr6);
                    addr6->sin6_port        = htons(port);
                    addr6->sin6_addr        = in6addr_any;
                }
            } else {
                // Client mode.  Set up the address on the caller-supplied address and port 
                // number.  Also, if the address is IPv4 and we created an IPv6 socket, 
                // convert the address to an IPv4-mapped address.
                if ([address length] > sizeof(addr)) {
                    assert(NO);         // very weird
                    [address getBytes:&addr length:sizeof(addr)];
                } else {
                    [address getBytes:&addr length:[address length]];
                }
                if (addr.ss_family == AF_INET) {
                    if (socketFamily == AF_INET6) {
                        struct	in_addr ipv4Addr;
                        
                        // Convert IPv4 address to IPv4-mapped-into-IPv6 address.
                        
                        ipv4Addr = addr4->sin_addr;
                        
                        addr6->sin6_len         = sizeof(*addr6);
                        addr6->sin6_family      = AF_INET6;
                        addr6->sin6_port        = htons(port);
                        addr6->sin6_addr.__u6_addr.__u6_addr32[0] = 0;
                        addr6->sin6_addr.__u6_addr.__u6_addr32[1] = 0;
                        addr6->sin6_addr.__u6_addr.__u6_addr16[4] = 0;
                        addr6->sin6_addr.__u6_addr.__u6_addr16[5] = 0xffff;
                        addr6->sin6_addr.__u6_addr.__u6_addr32[3] = ipv4Addr.s_addr;
                    } else {
                        addr4->sin_port = htons(port);
                    }
                } else {
                    assert(addr.ss_family == AF_INET6);
                    addr6->sin6_port        = htons(port);
                }
                if ( (addr.ss_family == AF_INET) && (socketFamily == AF_INET6) ) {
                    addr6->sin6_len         = sizeof(*addr6);
                    addr6->sin6_port        = htons(port);
                    addr6->sin6_addr        = in6addr_any;
                }
            }
            if (address == nil) {
                err = bind(sock, (const struct sockaddr *) &addr, addr.ss_len);
            } else {
                err = connect(sock, (const struct sockaddr *) &addr, addr.ss_len);
            }
            if (err < 0) {
                err = errno;
            }
        }
        
        // From now on we want the socket in non-blocking mode to prevent any unexpected 
        // blocking of the main thread.  None of the above should block for any meaningful 
        // amount of time.
        
        if (err == 0) {
            int flags;
            
            flags = fcntl(sock, F_GETFL);
            err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            if (err < 0) {
                err = errno;
            }
        }
        
        // Wrap the socket in a CFSocket that's scheduled on the runloop.
        
        if (err == 0) {
            self->_cfSocket = CFSocketCreateWithNative(NULL, sock, kCFSocketReadCallBack, SocketReadCallback, &context);

            // The socket will now take care of cleaning up our file descriptor.

            assert( CFSocketGetSocketFlags(self->_cfSocket) & kCFSocketCloseOnInvalidate );
            sock = -1;

            rls = CFSocketCreateRunLoopSource(NULL, self->_cfSocket, 0);
            assert(rls != NULL);
            
            CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
            
            CFRelease(rls);
        }
        
        // Handle any errors.
        
        if (sock != -1) {
            junk = close(sock);
            assert(junk == 0);
        }
        assert( (err == 0) == (self->_cfSocket != NULL) );
        if ( (self->_cfSocket == NULL) && (errorPtr != NULL) ) {
            *errorPtr = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
        }
        
        return (err == 0);
    }

#endif  // ! UDPSERVE_IPV4_ONLY

- (void)startServerOnPort:(NSUInteger)port
    // See comment in header.
{
    assert( (port > 0) && (port < 65536) );

    assert(self.port == 0);     // don't try and start a started object
    if (self.port == 0) {
        BOOL        success;
        NSError *   error;

        // Create a fully configured socket.
        
        success = [self setupSocketConnectedToAddress:nil port:port error:&error];

        // If we can create the socket, we're good to go.  Otherwise, we report an error 
        // to the delegate.

        if (success) {
            self.port = port;

            if ( (self.delegate != nil) && [self.delegate respondsToSelector:@selector(echo:didStartWithAddress:)] ) {
                CFDataRef   localAddress;
                
                localAddress = CFSocketCopyAddress(self->_cfSocket);
                assert(localAddress != NULL);
                
                [self.delegate echo:self didStartWithAddress:(__bridge NSData *) localAddress];

                CFRelease(localAddress);
            }
        } else {
            [self stopWithError:error];
        }
    }
}

- (void)hostResolutionDone
    // Called by our CFHost resolution callback (HostResolveCallback) when host 
    // resolution is complete.  We find the best IP address and create a socket 
    // connected to that.
{
    NSError *           error;
    Boolean             resolved;
    NSArray *           resolvedAddresses;
    
    assert(self.port != 0);
    assert(self->_cfHost != NULL);
    assert(self->_cfSocket == NULL);
    assert(self.hostAddress == nil);
    
    error = nil;
    
    // Walk through the resolved addresses looking for one that we can work with.
    
    resolvedAddresses = (__bridge NSArray *) CFHostGetAddressing(self->_cfHost, &resolved);
    if ( resolved && (resolvedAddresses != nil) ) {
        for (NSData * address in resolvedAddresses) {
            BOOL                    success;
            const struct sockaddr * addrPtr;
            NSUInteger              addrLen;
            
            addrPtr = (const struct sockaddr *) [address bytes];
            addrLen = [address length];
            assert(addrLen >= sizeof(struct sockaddr));

            // Try to create a connected CFSocket for this address.  If that fails, 
            // we move along to the next address.  If it succeeds, we're done.
            
            success = NO;
            if ( 
                (addrPtr->sa_family == AF_INET) 
#if ! UDPSERVE_IPV4_ONLY
             || (addrPtr->sa_family == AF_INET6) 
#endif
               ) {
                success = [self setupSocketConnectedToAddress:address port:self.port error:&error];
                if (success) {
                    CFDataRef   hostAddress;
                    
                    hostAddress = CFSocketCopyPeerAddress(self->_cfSocket);
                    assert(hostAddress != NULL);
                    
                    self.hostAddress = (__bridge NSData *) hostAddress;
                    
                    CFRelease(hostAddress);
                }
            }
            if (success) {
                break;
            }
        }
    }
    
    // If we didn't get an address and didn't get an error, synthesise a host not found error.
    
    if ( (self.hostAddress == nil) && (error == nil) ) {
        error = [NSError errorWithDomain:(NSString *)kCFErrorDomainCFNetwork code:kCFHostErrorHostNotFound userInfo:nil];
    }

    if (error == nil) {
        // We're done resolving, so shut that down.

        [self stopHostResolution];

        // Tell the delegate that we're up.
        
        if ( (self.delegate != nil) && [self.delegate respondsToSelector:@selector(echo:didStartWithAddress:)] ) {
            [self.delegate echo:self didStartWithAddress:self.hostAddress];
        }
    } else {
        [self stopWithError:error];
    }
}

static void HostResolveCallback(CFHostRef theHost, CFHostInfoType typeInfo, const CFStreamError *error, void *info)
    // This C routine is called by CFHost when the host resolution is complete. 
    // It just redirects the call to the appropriate Objective-C method.
{
    UDPServe *    obj;
    
    obj = (__bridge UDPServe *) info;
    assert([obj isKindOfClass:[UDPServe class]]);
    
    #pragma unused(theHost)
    assert(theHost == obj->_cfHost);
    #pragma unused(typeInfo)
    assert(typeInfo == kCFHostAddresses);
    
    if ( (error != NULL) && (error->domain != 0) ) {
        [obj stopWithStreamError:*error];
    } else {
        [obj hostResolutionDone];
    }
}

- (void)startConnectedToHostName:(NSString *)hostName port:(NSUInteger)port
    // See comment in header.
{
    assert(hostName != nil);
    assert( (port > 0) && (port < 65536) );
    
    assert(self.port == 0);     // don't try and start a started object
    if (self.port == 0) {
        Boolean             success;
        CFHostClientContext context = {0, (__bridge void *)(self), NULL, NULL, NULL};
        CFStreamError       streamError;

        assert(self->_cfHost == NULL);

        self->_cfHost = CFHostCreateWithName(NULL, (__bridge CFStringRef) hostName);
        assert(self->_cfHost != NULL);
        
        CFHostSetClient(self->_cfHost, HostResolveCallback, &context);
        
        CFHostScheduleWithRunLoop(self->_cfHost, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        
        success = CFHostStartInfoResolution(self->_cfHost, kCFHostAddresses, &streamError);
        if (success) {
            self.hostName = hostName;
            self.port = port;
            // ... continue in HostResolveCallback
        } else {
            [self stopWithStreamError:streamError];
        }
    }
}

- (void)stopHostResolution
    // Called to stop the CFHost part of the object, if it's still running.
{
    if (self->_cfHost != NULL) {
        CFHostSetClient(self->_cfHost, NULL, NULL);
        CFHostCancelInfoResolution(self->_cfHost, kCFHostAddresses);
        CFHostUnscheduleFromRunLoop(self->_cfHost, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        CFRelease(self->_cfHost);
        self->_cfHost = NULL;
    }
}

- (void)stop
    // See comment in header.
{
    self.hostName = nil;
    self.hostAddress = nil;
    self.port = 0;
    [self stopHostResolution];
    if (self->_cfSocket != NULL) {
        CFSocketInvalidate(self->_cfSocket);
        CFRelease(self->_cfSocket);
        self->_cfSocket = NULL;
    }
}

- (void)noop
{
}

- (void)stopWithError:(NSError *)error
    // Stops the object, reporting the supplied error to the delegate.
{
    assert(error != nil);
    [self stop];
    if ( (self.delegate != nil) && [self.delegate respondsToSelector:@selector(echo:didStopWithError:)] ) {
        // The following line ensures that we don't get deallocated until the next time around the 
        // run loop.  This is important if our delegate holds the last reference to us and 
        // this callback causes it to release that reference.  At that point our object (self) gets 
        // deallocated, which causes problems if any of the routines that called us reference self. 
        // We prevent this problem by performing a no-op method on ourself, which keeps self alive 
        // until the perform occurs.
        [self performSelector:@selector(noop) withObject:nil afterDelay:0.0];
        [self.delegate echo:self didStopWithError:error];
    }
}

- (void)stopWithStreamError:(CFStreamError)streamError
    // Stops the object, reporting the supplied error to the delegate.
{
    NSDictionary *  userInfo;
    NSError *       error;

    if (streamError.domain == kCFStreamErrorDomainNetDB) {
        userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInteger:streamError.error], kCFGetAddrInfoFailureKey,
            nil
        ];
    } else {
        userInfo = nil;
    }
    error = [NSError errorWithDomain:(NSString *)kCFErrorDomainCFNetwork code:kCFHostErrorUnknown userInfo:userInfo];
    assert(error != nil);
    
    [self stopWithError:error];
}

@end

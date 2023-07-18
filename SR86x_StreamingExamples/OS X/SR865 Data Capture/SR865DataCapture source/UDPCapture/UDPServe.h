#import <Foundation/Foundation.h>

#if TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR
    #import <CFNetwork/CFNetwork.h>
#else
    #import <CoreServices/CoreServices.h>
#endif

@protocol UDPServerDelegate;

@interface UDPServe : NSObject

- (id)init;

- (void)startServerOnPort:(NSUInteger)port;
    // Starts an echo server on the specified port.  Will call the 
    // -echo:didStartWithAddress: delegate method on success and the 
    // -echo:didStopWithError: on failure.  After that, the various 
    // 'data' delegate methods may be called.

- (void)stop;
    // Will stop the object, preventing any future network operations or delegate 
    // method calls until the next start call.

@property (nonatomic, weak,   readwrite) id<UDPServerDelegate>    delegate;
@property (nonatomic, assign, readonly, getter=isServer) BOOL   server;
@property (nonatomic, assign, readonly ) NSUInteger             port;           // valid in client and server mode

@end

@protocol UDPServerDelegate <NSObject>

@optional

// In all cases an address is an NSData containing some form of (struct sockaddr), 
// specifically a (struct sockaddr_in) or (struct sockaddr_in6).

- (void)echo:(UDPServe *)echo didReceiveData:(uint8_t *)data ofLength:(NSUInteger)len fromAddress:(NSData *)addr;
// Called after successfully receiving data.
//
// assert(echo != nil);
// assert(data != nil);
// assert(addr != nil);

- (void)echo:(UDPServe *)echo didReceiveError:(NSError *)error;
    // Called after a failure to receive data.
    //
    // assert(echo != nil);
    // assert(error != nil);
    
- (void)echo:(UDPServe *)echo didStartWithAddress:(NSData *)address;
    // Called after the object has successfully started up.
    // On the server, this is the local address
    // to which the server is bound.
    //
    // assert(echo != nil);
    // assert(address != nil);
    
- (void)echo:(UDPServe *)echo didStopWithError:(NSError *)error;
    // Called after the object stops spontaneously (that is, after some sort of failure, 
    // but now after a call to -stop).
    //
    // assert(echo != nil);
    // assert(error != nil);

@end

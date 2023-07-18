//
//  UDPServerThread.h
//  UDPCapture
//
//  Created by Ian Chan on 2/1/14.
//  Copyright (c) 2015 Stanford Research Systems. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "UDPServe.h"
#import "PacketHeader.h"

@interface UDPServerThread : NSThread <UDPServerDelegate>

- (void)main;

- (void)setPort:(int)inport;
- (void)setFileFmt:(bool)csv;
- (void)setFile:(NSString *)fname truncate:(bool)trunc;
- (bool)fileIsOpen;
- (void)closeFile;
- (void)saveData:(unsigned int)nwords;

- (void)stopServer;
- (void)startServer;
- (bool)serverOk;
- (void)gotData:(unsigned int)nwords;
- (void)getData:(int *)pwhat rate:(int *)prate liax:(float *)pliax liay:(float *)pliay liar:(float *)pliar liath:(float *)pliath count:(int *)pbyte_count missed:(bool *)pmissed overload:(bool *)pover;

@end

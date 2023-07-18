//
//  SRSAppDelegate.h
//  UDPCapture
//
//  Created by Ian Chan on 2/1/14.
//  Copyright (c) 2014 Stanford Research Systems. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "UDPServerThread.h"

@interface SRSAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;

@property (weak) IBOutlet NSTextField *InstrumentIPField;
@property (weak) IBOutlet NSTextField *InstrumentIDField;
@property (weak) IBOutlet NSPopUpButton *StreamWhatCombo;
@property (weak) IBOutlet NSPopUpButton *StreamRateCombo;
@property (weak) IBOutlet NSTextField *StreamMaxRateField;
@property (weak) IBOutlet NSTextField *PortField;
@property (weak) IBOutlet NSButton *StartStopButton;
@property (weak) IBOutlet NSProgressIndicator *ConnectingWheel;
@property (weak) IBOutlet NSButton *ConnectButton;

@property (weak) IBOutlet NSButton *SendChecksum;

@property (weak) IBOutlet NSLevelIndicator *LevelIndic;
@property (weak) IBOutlet NSTextField *WhatField;
@property (weak) IBOutlet NSTextField *RateField;
@property (weak) IBOutlet NSTextField *FileField;
@property (weak) IBOutlet NSTextField *XField;
@property (weak) IBOutlet NSTextField *YField;
@property (weak) IBOutlet NSTextField *RField;
@property (weak) IBOutlet NSTextField *ThField;
@property (weak) IBOutlet NSColorWell *ErrWell;
@property (weak) IBOutlet NSButton *SaveButton;
@property (weak) IBOutlet NSColorWell *SaveWell;

// periodic update timer
- (void)timerFired:(NSTimer *)sender;
- (void)updateConnection:(bool)status trying:(bool)settry;
@end

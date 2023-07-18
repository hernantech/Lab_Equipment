//
//  SRSAppDelegate.m
//  UDPCapture
//
//  Created by Ian Chan on 2/1/14.
//  Copyright (c) 2015 Stanford Research Systems. All rights reserved.
//

#import "SRSAppDelegate.h"
#import "vxi11.hpp"
#define DATA_RATE   4.0     // update UI at 4Hz

@interface SRSAppDelegate ()
@property (nonatomic, strong, readwrite) UDPServerThread *server;
@property (nonatomic, strong, readwrite) NSTimer *timer;
@property (nonatomic, strong, readwrite) NSTimer *timer2;
@property (strong) NSURL *filepath;
@property (nonatomic, strong) NSAlert *alert;
@property (atomic, readwrite) unsigned int port;
@property (atomic, readwrite) unsigned int vxiaddr;
@property (atomic, readwrite) unsigned int vxiport;
@property (atomic, readwrite) bool vxiport_specd;
@property (atomic, readwrite) bool streaming;
@property (atomic, readwrite) bool connected;
@property (atomic, readwrite) double maxRateHz;
@property (atomic, readwrite) int packetSize;
@property (atomic, readwrite) bool isCSV;
@property (atomic, readwrite) bool isLE;
@property (atomic, readwrite) bool sendLE;
@property (atomic, readwrite) bool sendCS;
@end


@implementation SRSAppDelegate

SRSAppDelegate *singleton=nil;
vxi11_client *vxiclient;


void connectionFailed(bool ok, bool settry)
{
    if (singleton)
        [singleton updateConnection:ok trying:settry];
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
    _alert = [[NSAlert alloc] init];
    [_alert addButtonWithTitle:@"OK"];
    [_alert setAlertStyle:NSWarningAlertStyle];

    _timer = [NSTimer scheduledTimerWithTimeInterval:(1.0/DATA_RATE) target:self selector:@selector(timerFired:) userInfo:nil repeats:true];
    _timer2 = [NSTimer scheduledTimerWithTimeInterval:(1.0) target:self selector:@selector(timer2Fired:) userInfo:nil repeats:true];

    // read last IP addr accessed
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([self verifyIP:[defaults stringForKey:@"lastIP"] set:true])
    {
        _vxiaddr = (192 << 24) | (168 << 16) | (0 << 8) | 2;
        _vxiport = 0;
        _vxiport_specd = false;
        [self updateVXIAddr];
    }
    _streaming = false;
    unsigned short test = 0x00ff;
    test = htons(test);
    _isLE = (test == 0xff00);     // are we little endian?
    _sendLE = _isLE;
    _sendCS = true;
    
    _SaveWell.color = [NSColor lightGrayColor];

    _port = 1865;
    _PortField.stringValue = [NSString stringWithFormat:@"%d",_port];
    
    _server = [[UDPServerThread alloc] init];
    [_server setPort:_port];
    _server.threadPriority = 0.5;   // normal priority
    [_server start];

    vxiclient = new vxi11_client();
    vxiclient->setCallback(connectionFailed);
    
    singleton = self;
    
    // init combo boxes
    NSString *whats = @"X only (float);X, Y (float);R, θ (float);X, Y, R, θ (float);X only (int);X, Y (int);R, θ (int);X, Y, R, θ (int)";
    [_StreamWhatCombo removeAllItems];
    [_StreamWhatCombo addItemsWithTitles:[whats componentsSeparatedByString:@";"]];
    NSString *rates = @"native;/2;/4;/8;/16;/32;/64;/128;/256;/512;/1024";
    [_StreamRateCombo removeAllItems];
    [_StreamRateCombo addItemsWithTitles:[rates componentsSeparatedByString:@";"]];
    
    [self killInfo];
    // [self updateInfo];
}

// we update UI only at a few Hz
- (void)updateInfo
{
    int what, rate, count;
    float liax, liay, liar, liath;
    bool missed, over;
    // get snapshot of data from server thread
    // and then format & update UI
    [_server getData:&what rate:&rate liax:&liax liay:&liay liar:&liar liath:&liath count:&count missed:&missed overload:&over];

    _WhatField.enabled = true;
    switch (what)
    {
        case 0: _WhatField.stringValue = @"X only (float)"; break;
        case 1: _WhatField.stringValue = @"X, Y (float)"; break;
        case 2: _WhatField.stringValue = @"R, θ (float)"; break;
        case 3: _WhatField.stringValue = @"X, Y, R, θ (float)"; break;
        case 4: _WhatField.stringValue = @"X only (int)"; break;
        case 5: _WhatField.stringValue = @"X, Y (int)"; break;
        case 6: _WhatField.stringValue = @"R, θ (int)"; break;
        case 7: _WhatField.stringValue = @"X, Y, R, θ (int)"; break;
        default:
            _WhatField.stringValue = @"N/A";
            _WhatField.enabled = false;
            break;
    }
    if (over)
        _WhatField.backgroundColor = [NSColor redColor];
    else
        _WhatField.backgroundColor = [NSColor colorWithRed:1.0 green:1.0 blue:0.625 alpha:1.0];
    
    if (rate>=0 || rate<=31)
    {
        float fs = 1.25e6 / pow(2.0, rate);
        _RateField.stringValue = [self formatRate:fs];
        _RateField.enabled = true;
    }
    else
    {
        _RateField.stringValue = @"N/A";
        _RateField.enabled = false;
    }
    
    if (what>=4 && what<8)
    {
        _XField.intValue = liax;
        _YField.intValue = liay;
        _RField.intValue = liar;
        _ThField.intValue = liath;
    }
    else
    {
        _XField.stringValue = [self formatFloat:liax];
        _YField.stringValue = [self formatFloat:liay];
        _RField.stringValue = [self formatFloat:liar];
        _ThField.stringValue = [self formatFloat:liath];
    }
    _XField.enabled = (what>=0 && what<8 && what!=2 && what!=6);
    _YField.enabled = (what==1 || what==3 || what==5 || what==7);
    _RField.enabled = (what==2 || what==3 || what==6 || what==7);
    _ThField.enabled = _RField.enabled;
    
    // 20MB/s = 100%, 0 = 64b/s or less
    count *= DATA_RATE;
    _LevelIndic.toolTip = [NSString stringWithFormat:@"%@B/s", [self formatFloat:count]];
    if (count > 0)
        count = log((float)count/64.0)/12.65*100.0;     // 100% = 20MB/s, 0% = 64B/s
    _LevelIndic.intValue = count;
    //NSLog(@"val = %d",_LevelIndic.intValue);
    
    if (missed)
        _ErrWell.color = [NSColor redColor];
    else
        _ErrWell.color = [NSColor lightGrayColor];
}
- (IBAction)SavePressed:(NSButton *)sender
{
    if (_filepath == nil)
    {
        [self NewFile:nil];
    }
    else
    {
        if ([_server fileIsOpen])
        {
            [_server closeFile];
            _SaveButton.title = @"Save";
            _SaveWell.color = [NSColor lightGrayColor];
        }
        else
        {
            [_server setFile:[_filepath path] truncate:false];
            _SaveButton.title = @"Pause";
            _SaveWell.color = [NSColor blueColor];
        }
        
    }
}
- (void)killInfo
{
    _WhatField.enabled = false;
    _WhatField.backgroundColor = [NSColor controlColor];
    _RateField.enabled = false;

    _XField.enabled = false;
    _YField.enabled = false;
    _RField.enabled = false;
    _ThField.enabled = false;
    
    _LevelIndic.intValue = 0;
    _ErrWell.color = [NSColor lightGrayColor];
}
- (IBAction)NewFile:(NSButton *)sender
{
    NSSavePanel *view = [NSSavePanel savePanel];
    NSString *types = @"dat,csv";
    view.allowedFileTypes = [types componentsSeparatedByString:@","];
    view.allowsOtherFileTypes = false;
    view.extensionHidden = false;
    view.message = @"Save stream data as binary \".dat\" file or text \".csv\" file.\nNote that text files are larger and may not keep up at high data rates.";
    int val = (int)[view runModal];
    //NSLog(@"val = %ld %@ %@", val, [view directoryURL], [view nameFieldStringValue]);

    if (val)
    {
        _filepath = [[view directoryURL] URLByAppendingPathComponent:[view nameFieldStringValue]];
        _FileField.stringValue = [_filepath lastPathComponent];
        _FileField.enabled = true;
        
        _isCSV = [[[_filepath pathExtension] lowercaseString] isEqualTo:@"csv"];
        [_server setFileFmt:_isCSV];
        [_server setFile:[_filepath path] truncate:true];
        _SaveButton.title = @"Pause";
        _SaveWell.color = [NSColor blueColor];
    }
}
- (IBAction)PortChanged:(NSTextField *)sender
{
    int val = sender.intValue;
    if (val<1024 || val>=65536)
    {
        [self ShowMessage:@"UDP Port must be between 1024 and 65535"];
    }
    else
    {
        [_server setPort:val];
        if ([_server serverOk])
            _port = val;
        else
        {
            [self ShowMessage:[NSString stringWithFormat:@"Could not listen on UDP port %d.\nIs another program using that port?",val]];
            [_server setPort:_port];
        }
        NSString *tmp = [NSString stringWithFormat:@"STREAMPORT %d", _port];
        vxiclient->device_write([tmp UTF8String]);
    }
    _PortField.stringValue = [NSString stringWithFormat:@"%d",_port];
}
- (void)updateVXIAddr
{
    unsigned int bval;
    NSString *addr = @"";
    for (int i=0;i<4;i++)
    {
        bval = (_vxiaddr >> ((3-i)*8)) & 0xff;
        addr = [NSString stringWithFormat:@"%@%d", addr, bval];
        if (i < 3)
            addr = [addr stringByAppendingString:@"."];
    }
    if (_vxiport_specd)
        addr = [NSString stringWithFormat:@"%@:%d", addr, _vxiport];
    _InstrumentIPField.stringValue = addr;
}

- (int)verifyIP:(NSString *)ip set:(bool)set
{
    NSString *addr, *port;
    int idx;
    unsigned int bval=0;
    unsigned int val=0;
    addr = ip;

    // figure out vxi11 port
    idx = (int)[addr rangeOfString:@":"].location;
    if (idx > 0)
    {
        port = [addr substringFromIndex:(idx+1)];
        addr = [addr substringToIndex:idx];
        val = port.intValue;
        if (val < 1 || val >= 65536)
            return -1;
    }
    else
        port = @"0";

    // figure out vxi11 addr
    val = 0;
    NSArray *arr = [addr componentsSeparatedByString:@"."];
    if (arr.count == 4)
    {
        for (int i=0;i<4;i++)
        {
            bval = [arr[i] intValue];
            if (bval >= 256)
                return -2;
            else
                val += (bval << ((3-i)*8));
        }
    }
    else
        return -3;

    if (set)
    {
        _vxiaddr = val;
        _vxiport = [port intValue];
        _vxiport_specd = _vxiport;
        [self updateVXIAddr];
    }
    return 0;
}

- (IBAction)IPChanged:(NSTextField *)sender
{
    NSString *addr = _InstrumentIPField.stringValue;
    int retval = [self verifyIP:addr set:true];
    switch (retval)
    {
        case -1:
            [self ShowMessage:@"VXI11 Port must be between 1 and 65535"];
            break;
        case -2:
            [self ShowMessage:@"VXI11 Address components must be between 0 and 255"];
            break;
        case -3:
            [self ShowMessage:@"VXI11 Address must contain 4 octets (W.X.Y.Z)"];
            break;
    }

    if (retval)
        [self updateVXIAddr];
}
- (IBAction)WhatComboChanged:(NSPopUpButton *)sender
{
    NSString *cmd = [NSString stringWithFormat:@"STREAMCH %d",(int)(_StreamWhatCombo.indexOfSelectedItem % 4)];
    vxiclient->device_write([cmd UTF8String]);
    cmd = [NSString stringWithFormat:@"STREAMFMT %d",(int)(_StreamWhatCombo.indexOfSelectedItem >= 4)];
    vxiclient->device_write([cmd UTF8String]);
    // sync again?
    char sbuf[128];
    char *id = (char *)sbuf;
    int val;
    if (vxiclient->device_write("STREAMCH?"))
    {
        if (vxiclient->device_read(&id))
        {
            val = [NSString stringWithUTF8String:id].intValue;
            if (vxiclient->device_write("STREAMFMT?"))
            {
                if (vxiclient->device_read(&id))
                    val = val + (4 * [NSString stringWithUTF8String:id].intValue);
                if (_StreamWhatCombo.indexOfSelectedItem != val)
                    [_StreamWhatCombo selectItemAtIndex:val];
            }
        }
    }
}
- (IBAction)RateComboChanged:(NSPopUpButton *)sender
{
    NSString *cmd = [NSString stringWithFormat:@"STREAMRATE %d",(int)_StreamRateCombo.indexOfSelectedItem];
    vxiclient->device_write([cmd UTF8String]);
    // sync again?
    char sbuf[128];
    char *id = (char *)sbuf;
    int val;
    if (vxiclient->device_write("STREAMRATE?"))
    {
        if (vxiclient->device_read(&id))
        {
            val = [NSString stringWithUTF8String:id].intValue;
            if (_StreamRateCombo.indexOfSelectedItem != val)
                [_StreamRateCombo selectItemAtIndex:val];
        }
    }
}

- (IBAction)ConnectPressed:(NSButton *)sender
{
    if (vxiclient == NULL)
        return;
    
    if (_connected & _streaming)
        vxiclient->device_write("STREAM 0");
    if (_connected)
        vxiclient->destroy_link();
    _connected = false;
    _streaming = false;
    
    _StartStopButton.title = _streaming?@"Stop":@"Start";
    _StartStopButton.enabled = _connected;
    _StreamWhatCombo.enabled = _connected;
    _StreamRateCombo.enabled = _connected;
    _PortField.enabled = _connected;
    _SendChecksum.enabled = _connected;
    _StreamMaxRateField.enabled = _connected;
    
    if ([_ConnectButton.title isEqual:@"Connect"])
    {
        _ConnectButton.title = @"Cancel";
        _InstrumentIDField.stringValue = @"Connecting...";
        _InstrumentIPField.editable = false;
        if (vxiclient->connectToDevice((__bridge CFStringRef)_InstrumentIPField.stringValue, _vxiport))
            [_ConnectingWheel startAnimation:self];
        else
        {
            _ConnectButton.title = @"Connect";
            _InstrumentIDField.stringValue = @"Connection Failed";
            _InstrumentIPField.editable = true;
        }
    }
    else
    {
        vxiclient->cancelConnect();
        _ConnectButton.title = @"Connect";
        _InstrumentIDField.stringValue = @"Connection Cancelled";
        _InstrumentIPField.editable = true;
        [_ConnectingWheel stopAnimation:self];
    }
}
- (void)updateConnection:(bool)status trying:(bool)settry
{
    if (status)
    {
        vxiclient->device_clear();
        vxiclient->device_write("*IDN?");
        char sbuf[128];
        char *id = (char *)sbuf;
        vxiclient->device_read(&id);
        _InstrumentIDField.stringValue = [NSString stringWithUTF8String:id];
        _connected = true;
        _streaming = false;

        // write last IP addr accessed
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [defaults setObject:_InstrumentIPField.stringValue forKey:@"lastIP"];
        [defaults synchronize];

        [self syncState:true];
    }
    else
    {
        if (settry)
            _InstrumentIDField.stringValue = @"Connection Failed";
        else
            _InstrumentIDField.stringValue = @"Connection Terminated";
        _connected = false;
        _streaming = false;
    }

    _StartStopButton.title = _streaming?@"Stop":@"Start";
    _StartStopButton.enabled = _connected;
    _StreamWhatCombo.enabled = _connected;
    _StreamRateCombo.enabled = _connected;
    _PortField.enabled = _connected;
    _SendChecksum.enabled = _connected;
    _StreamMaxRateField.enabled = _connected;

    _ConnectButton.title = @"Connect";
    _InstrumentIPField.editable = true;
    [_ConnectingWheel stopAnimation:self];
}

- (IBAction)StartPressed:(NSButton *)sender
{
    _streaming = !_streaming;
    if (_streaming)
    {
        // is server up?
        if (![_server serverOk])
        {
            [_server setPort:_port];
            if (![_server serverOk])
            {
                [self ShowMessage:[NSString stringWithFormat:@"Could not listen on UDP port %d.\nIs another program using that port?",_port]];
                _streaming = false;
            }
        }
    }
    
    if (_streaming)
    {
        // check packet rate?
        // at low data rates, use smaller packets
        // at higher rates, use larger packets?
        double byteRate = 4.0 * _maxRateHz * pow(0.5, _StreamRateCombo.indexOfSelectedItem);
        switch (_StreamWhatCombo.indexOfSelectedItem)
        {
            case 1:
            case 2:
            case 7:
                byteRate *= 2.0;
                break;
            case 4:
                byteRate *= 0.5;
                break;
            case 3:
                byteRate *= 4.0;
                break;
        }
        
        // calc packet rate
        byteRate /= (1024 >> _packetSize);
        if ((byteRate < 1.0) && (_packetSize < 3))
        {
            vxiclient->device_write("STREAMPCKT 3");    // 128byte size
            _packetSize = 3;
        }
        else if ((byteRate > 20000.0) && (_packetSize > 0))
        {
            vxiclient->device_write("STREAMPCKT 0");    // 1024byte size
            _packetSize = 0;
        }
        
        // ask to stream in native endian format
        if (_sendLE != _isLE)
        {
            _sendLE = _isLE;
            [self SendOptionChanged:_SendChecksum];
        }
    }
    
    NSString *cmd = [NSString stringWithFormat:@"STREAM %d",(int)_streaming];
    vxiclient->device_write([cmd UTF8String]);
    if (_streaming)
    {
        _StartStopButton.title = @"Stop";
        _StreamWhatCombo.enabled = false;
        _StreamRateCombo.enabled = false;
        _PortField.enabled = false;
        _SendChecksum.enabled = false;
    }
    else
    {
        _StartStopButton.title = @"Start";
        _StreamWhatCombo.enabled = true;
        _StreamRateCombo.enabled = true;
        _PortField.enabled = true;
        _SendChecksum.enabled = true;
    }
}

- (void)syncState:(bool)syncall
{
    // sync stream settings
    char sbuf[128];
    char *id = (char *)sbuf;
    int val;
    
    if (syncall)
    {
        if (vxiclient->device_write("STREAM?"))
        {
            if (vxiclient->device_read(&id))
            {
                _streaming = [[NSString stringWithUTF8String:id] boolValue];
                if (_streaming)
                {
                    _StartStopButton.title = @"Stop";
                    _StreamWhatCombo.enabled = false;
                    _StreamRateCombo.enabled = false;
                    _PortField.enabled = false;
                    _SendChecksum.enabled = false;
                }
                else
                {
                    _StartStopButton.title = @"Start";
                    _StreamWhatCombo.enabled = true;
                    _StreamRateCombo.enabled = true;
                    _PortField.enabled = true;
                    _SendChecksum.enabled = true;
                }
            }
        }
        if (vxiclient->device_write("STREAMCH?"))
        {
            if (vxiclient->device_read(&id))
            {
                val = [[NSString stringWithUTF8String:id] intValue];
                if (vxiclient->device_write("STREAMFMT?"))
                {
                    if (vxiclient->device_read(&id))
                        [_StreamWhatCombo selectItemAtIndex:(val + (4 * [NSString stringWithUTF8String:id].intValue))];
                }
            }
        }
    }
    
    if (vxiclient->device_write("STREAMRATEMAX?"))
    {
        if (vxiclient->device_read(&id))
        {
            _maxRateHz = [[NSString stringWithUTF8String:id] doubleValue];
            _StreamMaxRateField.stringValue = [self formatRate:_maxRateHz];
        }
    }
    if (syncall)
    {
        if (vxiclient->device_write("STREAMRATE?"))
        {
            if (vxiclient->device_read(&id))
                [_StreamRateCombo selectItemAtIndex:[NSString stringWithUTF8String:id].intValue];
        }
        if (vxiclient->device_write("STREAMPCKT?"))
        {
            if (vxiclient->device_read(&id))
                _packetSize = [[NSString stringWithUTF8String:id] intValue];
        }
        if (vxiclient->device_write("STREAMPORT?"))
        {
            if (vxiclient->device_read(&id))
            {
                _port = [[NSString stringWithUTF8String:id] intValue];
                _PortField.stringValue = [NSString stringWithFormat:@"%d",_port];
                [_server setPort:_port];
                if (![_server serverOk])
                    [self ShowMessage:[NSString stringWithFormat:@"Could not listen on UDP port %d.\nIs another program using that port?",_port]];
            }
        }
        if (vxiclient->device_write("STREAMOPTION?"))
        {
            if (vxiclient->device_read(&id))
            {
                int val = [[NSString stringWithUTF8String:id] intValue];
                _sendLE = val & 0x01;
                _sendCS = val & 0x02;
                _SendChecksum.state = _sendCS?NSOnState:NSOffState;
            }
        }
    }
    
    // blink save indic
    if (_SaveWell.color != [NSColor lightGrayColor])
    {
        if (_SaveWell.color == [NSColor blueColor])
            _SaveWell.color = [NSColor darkGrayColor];
        else
            _SaveWell.color = [NSColor blueColor];
    }
    else
        _SaveWell.color = [NSColor lightGrayColor];
}
- (IBAction)SendOptionChanged:(NSButton *)sender
{
    bool on = (sender.state == NSOnState);
    _sendCS = on;
    
    int val = _sendLE | (_sendCS << 1);
    NSString *cmd = [NSString stringWithFormat:@"STREAMOPTION %d",(int)val];
    vxiclient->device_write([cmd UTF8String]);
}


char carr[9] = "fnum kMG";
- (NSString *)formatFloat:(float)val
{
    if (val == 0.0)
        return @"+0.000  ";
    bool neg = val < 0.0;
    if (neg)
        val = -val;
    int exp = floor(log10(val));
    int rem = exp%3;
    exp /= 3;
    if (rem < 0)
    {
        --exp;
        rem += 3;
    }
    val *= pow(1000.0, -exp);
    exp += 4;
    if (neg)
        val = -val;
    NSString *fmt = [NSString stringWithFormat:@"%%+%d.%df %c", rem+1, 3-rem, carr[exp]];
    return [NSString stringWithFormat:fmt, val];
}
- (NSString *)formatRate:(float)fs
{
    NSString *unit = @"Hz";
    if (fs >= 1.0e6)
    {
        unit = @"MHz";
        fs /= 1.0e6;
    }
    else if (fs >= 1.0e3)
    {
        unit = @"kHz";
        fs /= 1.0e3;
    }
    int a=1,b=2;
    if (fs >= 99.95)
    {
        a = 3;
        b = 0;
    }
    else if (fs >= 9.995)
    {
        a = 2;
        b = 1;
    }
    NSString *fmt = [NSString stringWithFormat:@"%%%d.%df %%@", a, b];
    return [NSString stringWithFormat:fmt, fs, unit];
}

- (void)timerFired:(NSTimer *)sender
{
    [self updateInfo];
}
- (void)timer2Fired:(NSTimer *)sender
{
    if (_connected)
        [self syncState:false];
}


- (void)ShowMessage:(NSString *)message
{
    [_alert setMessageText:message];
    [_alert beginSheetModalForWindow:_window modalDelegate:self didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:) contextInfo:nil];
}
- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    
}


- (void)dealloc
{
    if (_connected && _streaming)
        vxiclient->device_write("STREAM 0");
    [self->_timer invalidate];
    [self->_timer2 invalidate];
    [self->_server cancel];
    singleton = nil;

    delete vxiclient;
    vxiclient = NULL;
}


@end

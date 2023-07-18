//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include <math.h>
#include <shlobj.h>
#include <fstream>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
#define DATA_RATE 4.0            // update UI at 4Hz

__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)
{
    // read last used instrument IP address
    {
        // XP compatible
        TCHAR szPath[MAX_PATH];
        SHGetFolderPath(NULL,CSIDL_LOCAL_APPDATA,NULL,0,szPath);
        prefFile = AnsiString(szPath);
    }

    prefFile = prefFile + AnsiString("\\com.thinksrs.sr865.cfg");
    std::ifstream istrm(prefFile.c_str());
    char dat[256];
    istrm.get(dat,256);
    istrm.close();
    AnsiString ip(dat);
    if (validateIP(ip, true))
    {
        // use default IP address
        vxiaddr = (192 << 24) | (168 << 16) | (0 << 8) | 2;
        vxiport = 0;
        vxiport_specd = false;
        updateVXIAddr();
    }
    streaming = false;

    // use native endian
    // test checksum
    unsigned short test = 0x0f;
    test = htons(test);
    isLE = (test == 0xf0);
    sendLE = isLE;
    sendCS = true;
    ChecksumCheckBox->Checked = sendCS;

    FileEdit->Text = "No File";
    FileEdit->Enabled = false;
    port = 1865;
    PortEdit->Text = AnsiString(port);

    // Timer1 updates UI
    Timer1->Interval = 1000/DATA_RATE;
    // serverThread is the thread that captures data and writes it to disk
    serverThread = new UDPServerThread();
    serverThread->Priority = tpNormal;
    serverThread->FreeOnTerminate = false;
    //serverThread->setPort(port);
    serverThread->Resume();

    // create vxiclient after serverthread creation
    // serverthread initializes winsock
    // we need to delete vxi11 client before killing serverthread...
    vxiclient = new vxi11_client();
}
/*virtual*/ __fastcall TForm1::~TForm1()
{
    if (connected && streaming)
    {   // stop streaming on close
        AnsiString cmd = "STREAM 0";
        vxiclient->device_write(cmd.c_str());
    }

    if (vxiclient)
        delete vxiclient;

    if (serverThread)
        delete serverThread;
}

// we update UI only at a few Hz
// This gets called from a timer loop
void TForm1::updateInfo()
{
    int what, rate, byte_count;
    float liax, liay, liar, liath;
    bool missed, over;
    // get snapshot of data from server thread
    // and then format & update UI
    serverThread->getData(&what, &rate, &liax, &liay, &liar, &liath, &byte_count, &missed, &over);
    UnicodeString theta = WideChar(0x3b8);  // theta

    // content of streamed data
    WhatEdit->Enabled = true;
    switch (what)
    {
        case 0: WhatEdit->Text = UnicodeString("X only (float)"); break;
        case 1: WhatEdit->Text = UnicodeString("X & Y (float)"); break;
        case 2: WhatEdit->Text = UnicodeString("R & ") + theta + UnicodeString(" (float)"); break;
        case 3: WhatEdit->Text = UnicodeString("X, Y, R & ") + theta + UnicodeString(" (float)"); break;
        case 4: WhatEdit->Text = UnicodeString("X only (int)"); break;
        case 5: WhatEdit->Text = UnicodeString("X & Y (int)"); break;
        case 6: WhatEdit->Text = UnicodeString("R & ") + theta + UnicodeString(" (int)"); break;
        case 7: WhatEdit->Text = UnicodeString("X, Y, R & ") + theta + UnicodeString(" (int)"); break;
        default:
            WhatEdit->Text = UnicodeString("N/A");
            WhatEdit->Enabled = false;
            break;
    }
    XEdit->Enabled = (what>=0 && what<8 && what!=2 && what!=6);
    YEdit->Enabled = (what==1 || what==3 || what==5 || what==7);
    REdit->Enabled = (what==2 || what==3 || what==6 || what==7);
    ThEdit->Enabled = REdit->Enabled;

    // streamed data rate
    if (rate>=0 && rate<=31)
    {
        double Fs = 1.25e6 / std::pow(2.0, rate);
        RateEdit->Text = formatRate(Fs);
        RateEdit->Enabled = true;
    }
    else
    {
        RateEdit->Text = "N/A";
        RateEdit->Enabled = false;
    }

    // data itself (only data from beginning of packet)
    if (what>=4 && what<8)
    {
        XEdit->Text = AnsiString::FormatFloat("##,##0", liax);
        YEdit->Text = AnsiString::FormatFloat("##,##0", liay);
        REdit->Text = AnsiString::FormatFloat("##,##0", liar);
        ThEdit->Text = AnsiString::FormatFloat("##,##0", liath);
    }
    else
    {
        XEdit->Text = formatFloat(liax);
        YEdit->Text = formatFloat(liay);
        REdit->Text = formatFloat(liar);
        ThEdit->Text = formatFloat(liath);
    }

    // data rate display
    // 20MB/s = 100%, 0 = 64b/s or less
    byte_count *= DATA_RATE;
    ProgressBar1->Hint = formatFloat(byte_count) + UnicodeString("B/s");
    if (byte_count > 0.0)
        byte_count = log((float)byte_count/64.0)/12.65*100.0;
    ProgressBar1->Position = byte_count;
    if (byte_count >= 92)
        ProgressBar1->State = pbsError;
    else if (byte_count >= 75)
        ProgressBar1->State = pbsPaused;
    else
        ProgressBar1->State = pbsNormal;

    // dropped packet?
    if (missed)
        MissedShape->Brush->Color = clRed;
    else
        MissedShape->Brush->Color = clSilver;

    // overload, unlock, error?
    if (over)
        WhatEdit->Color = clRed;
    else
        WhatEdit->Color = clInfoBk;
}
// format floating-point data
char carr[9] = "fnum kMG";
AnsiString TForm1::formatFloat(float val)
{
    if (val == 0.0)
        return UnicodeString("+0.000  ");
    bool neg = (val < 0.0);
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
    if (exp < 0)
        val *= pow(1000.0, exp);
    else if (exp >= 9)
        val *= pow(1000.0, exp-9);
    AnsiString fmt = AnsiString::Format(AnsiString("%%%d.%df "), ARRAYOFCONST((rem+1, 3-rem)));
    fmt = (neg?AnsiString("\u2013"):AnsiString("+")) + fmt + AnsiString(carr[exp]);
    AnsiString str = AnsiString::Format(fmt, ARRAYOFCONST((val)));
    return str;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::SaveButtonClick(TObject *Sender)
{
    if (!FileEdit->Enabled)
    {
        if (SaveDialog1->Execute())
        {
            // save data to new file
            FileEdit->Text = SaveDialog1->FileName;
            FileEdit->Enabled = true;
            isCSV = SaveDialog1->FilterIndex - 1;
            serverThread->setFileFmt(isCSV);
            serverThread->setFile(FileEdit->Text, true);
            SaveButton->Caption = "Pause";
            DiskShape->Brush->Color = clBlue;
        }
    }
    else
    {
        if (serverThread->fileIsOpen())
        {
            // close save file
            serverThread->closeFile();
            SaveButton->Caption = "Save";
            DiskShape->Brush->Color = clSilver;
        }
        else
        {
            // append data to save file
            serverThread->setFile(FileEdit->Text, false);
            SaveButton->Caption = "Pause";
            DiskShape->Brush->Color = clBlue;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::Button1Click(TObject *Sender)
{
    if (SaveDialog1->Execute())
    {
        // save data to new file
        FileEdit->Text = SaveDialog1->FileName;
        FileEdit->Enabled = true;
        isCSV = SaveDialog1->FilterIndex - 1;
        serverThread->setFileFmt(isCSV);
        serverThread->setFile(FileEdit->Text, true);
        SaveButton->Caption = "Pause";
        DiskShape->Brush->Color = clBlue;
    }
    else
        return;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Timer1Timer(TObject *Sender)
{
    updateInfo();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::PortEditExit(TObject *Sender)
{
    // user changed UDP port
    // where data is streamed to
    int val = PortEdit->Text.ToInt();
    if (val < 1024 || val >= 65536)
    {
        ShowMessage("UDP Port must be between 1024 and 65535");
        PortEdit->Text = AnsiString(port);
    }
    else
    {
        int prev = port;
        port = val;
        try
        {
            serverThread->setPort(port);
        }
        catch(Exception &e)
        {
            ShowMessage("Unable to listen on UDP port " + AnsiString(port) + "!\nIs another program listening on the same port?");
            port = prev;
            PortEdit->Text = AnsiString(port);
        }
        AnsiString cmd = AnsiString("STREAMPORT ") + AnsiString(port);
        vxiclient->device_write(cmd.c_str());
    }
}
//---------------------------------------------------------------------------

void TForm1::updateVXIAddr()
{
    // set display IP address based on numerical IP address
    unsigned int bval;
    AnsiString addr = AnsiString("");
    for (int i=0;i<4;i++)
    {
        bval = (vxiaddr >> ((3-i)*8)) & 0xff;
        addr += AnsiString(bval);
        if (i < 3)
            addr += ".";
    }
    if (vxiport_specd)
        addr += AnsiString(":") + AnsiString(vxiport);
    AddressEdit->Text = addr;
}

int TForm1::validateIP(AnsiString &ip, bool set)
{
    AnsiString addr=ip, port, tmp;
    int idx;
    unsigned int bval=0, val=0;

    // figure out vxi11 port
    idx = addr.Pos(":");
    if (idx)
    {
        port = addr.SubString(idx+1,addr.Length()).Trim();
        addr = addr.SubString(1,idx-1).Trim();
        try {val = port.ToInt();}
        catch (...) {val=65536;}
        if (val >= 65536)
            return -1;
    }
    else
        port = AnsiString(0);

    // figure out vxi11 addr
    val = 0;
    for (int i=0;i<4;i++)
    {
        idx = addr.Pos(".");
        if (i == 3 && idx == 0)
            idx = addr.Length() + 1;
        if (idx)
        {
            tmp = addr.SubString(1,idx-1);
            addr = addr.SubString(idx+1,addr.Length());
            try {bval = tmp.ToInt();}
            catch (...) {bval = 256;}
            if (bval >= 256)
                return -2;
            else
                val += bval << ((3-i)*8);
        }
        else
            return -3;
    }

    if (set)
    {
        vxiaddr = val;
        vxiport = 0;
        vxiport = port.ToInt();
        vxiport_specd = (vxiport);

        updateVXIAddr();
    }
    return 0;
}
void __fastcall TForm1::AddressEditExit(TObject *Sender)
{
    AnsiString ip(AddressEdit->Text);
    int err = validateIP(ip, true);

    switch(err)
    {
        case -1:
            ShowMessage("VXI11 Port must be between 1 and 65535");
            break;
        case -2:
            ShowMessage("VXI11 Address components must be between 0 and 255");
            break;
        case -3:
            ShowMessage("VXI11 Address must contain 4 octects (W.X.Y.Z)");
            break;
    }

    if (err)
        updateVXIAddr();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ConnectBtnClick(TObject *Sender)
{
    IDEdit->Text = AnsiString("Connecting...");
    Application->ProcessMessages();
    Screen->Cursor = crHourGlass;

    // turn off streaming
    if (connected && streaming)
    {   // stop streaming on close
        AnsiString cmd = "STREAM 0";
        vxiclient->device_write(cmd.c_str());
    }
    if (connected)
    {
        // disconnect vxi11 session
        vxiclient->destroy_link();
    }
    connected = false;
    streaming = false;

    AnsiString str = AddressEdit->Text;
    if (vxiclient->connectToDevice(str.c_str(), vxiport))
    {
        // get *IDN
        vxiclient->device_clear();
        vxiclient->device_write("*IDN?");
        char id[128];
        vxiclient->device_read(id);
        IDEdit->Text = AnsiString(id);
        connected = true;
        Button3->Enabled = connected;
        MaxRate->Enabled = connected;

        // save ip addr to pref
        char simp[64];
        wcstombs(simp,AddressEdit->Text.c_str(),AddressEdit->Text.Length());
        simp[AddressEdit->Text.Length()] = '\0';
        std::ofstream ostrm(prefFile.c_str(),ios::out | ios::trunc);
        ostrm << simp << std::endl;
        ostrm.close();

        syncState();
    }
    else
    {
        IDEdit->Text = AnsiString("Connection Failed");
    }

    Button3->Enabled = connected;
    Button3->Caption = streaming?"Stop":"Start";
    MaxRate->Enabled = connected;
    WhatComboBox->Enabled = connected & !streaming;
    RateComboBox->Enabled = connected & !streaming;
    PortEdit->Enabled = connected & !streaming;
    ChecksumCheckBox->Enabled = connected & !streaming;

    Screen->Cursor = crDefault;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button3Click(TObject *Sender)
{
    // start/stop data streaming
    streaming = !streaming;
    if (streaming)
    {
        if (!serverThread->serverOk())
        {
            try
            { serverThread->setPort(port); }
            catch (Exception &e)
            {
                ShowMessage("Unable to listen on UDP port " + AnsiString(port) + "!\nIs another program listening on the same port?");
                streaming = false;
            }
        }
    }

    if (streaming)
    {
        // check packet rate?
        // at low data rates, use smaller packets
        // at higher rates, use larger packets
        double byteRate = 4.0 * maxRateHz * pow(0.5, RateComboBox->ItemIndex);
        switch (WhatComboBox->ItemIndex)
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
        byteRate /= (1024 >> packetSize);
        if ((byteRate < 1.0) && (packetSize < 3))
        {
            AnsiString cmd = "STREAMPCKT 3";    // 128byte size
            vxiclient->device_write(cmd.c_str());
            packetSize = 3;
        }
        else if ((byteRate > 20000.0) && (packetSize > 0))
        {
            AnsiString cmd = "STREAMPCKT 0";    // 1024byte size
            vxiclient->device_write(cmd.c_str());
            packetSize = 0;
        }

        // send native endian
        if (isLE && !sendLE)
        {
            sendLE = true;
            ChecksumCheckBoxClick(NULL);
        }
    }

    AnsiString cmd = "STREAM " + AnsiString((int)streaming);
    vxiclient->device_write(cmd.c_str());
    Button3->Caption = streaming?"Stop":"Start";
    WhatComboBox->Enabled = connected & !streaming;
    RateComboBox->Enabled = connected & !streaming;
    PortEdit->Enabled = connected & !streaming;
    ChecksumCheckBox->Enabled = connected & !streaming;
}
//---------------------------------------------------------------------------
void TForm1::syncState(bool syncall)
{
    // sync local stream settings
    // with remote instrument
    char id[128];
    int val;

    if (syncall)
    {
        // data streaming state
        if (vxiclient->device_write("STREAM?"))
        {
            if (vxiclient->device_read(id))
            {
                streaming = AnsiString(id).Trim().ToInt();
                if (streaming)
                {
                    Button3->Caption = "Stop";
                    WhatComboBox->Enabled = false;
                    RateComboBox->Enabled = false;
                    PortEdit->Enabled = false;
                    ChecksumCheckBox->Enabled = false;
                }
                else
                {
                    Button3->Caption = "Start";
                    WhatComboBox->Enabled = true;
                    RateComboBox->Enabled = true;
                    PortEdit->Enabled = true;
                    ChecksumCheckBox->Enabled = true;
                }
            }
        }
        // what is being streamed
        if (vxiclient->device_write("STREAMCH?"))
        {
            if (vxiclient->device_read(id))
            {
                val = AnsiString(id).Trim().ToInt();
                if (vxiclient->device_write("STREAMFMT?"))
                {
                    if (vxiclient->device_read(id))
                        WhatComboBox->ItemIndex = val + (4 * AnsiString(id).Trim().ToInt());
                }
            }
        }
    }

    // query instrument native data rate
    if (vxiclient->device_write("STREAMRATEMAX?"))
    {
        if (vxiclient->device_read(id))
        {
            maxRateHz = AnsiString(id).Trim().ToDouble();
            MaxRate->Text = formatRate(maxRateHz);
        }
    }
    if (syncall)
    {
        // data streaming rate
        if (vxiclient->device_write("STREAMRATE?"))
        {
            if (vxiclient->device_read(id))
                RateComboBox->ItemIndex = AnsiString(id).Trim().ToInt();
        }
        // streaming packet size
        if (vxiclient->device_write("STREAMPCKT?"))
        {
            if (vxiclient->device_read(id))
                packetSize = AnsiString(id).Trim().ToInt();
        }
        // streaming UDP port
        if (vxiclient->device_write("STREAMPORT?"))
        {
            if (vxiclient->device_read(id))
            {
                port = AnsiString(id).Trim().ToInt();
                try
                {
                    PortEdit->Text = AnsiString(port);
                    serverThread->setPort(port);
                }
                catch(Exception &e)
                {
                    ShowMessage("Unable to listen on UDP port " + AnsiString(port) + "!\nIs another program listening on the same port?");
                }
            }
        }
        // streaming options (endianess & checksum)
        if (vxiclient->device_write("STREAMOPTION?"))
        {
            if (vxiclient->device_read(id))
            {
                int val = AnsiString(id).Trim().ToInt();
                sendLE = val & 0x01;
                sendCS = val & 0x02;
                ChecksumCheckBox->Checked = sendCS;
            }
        }
    }

    // blink save indic
    if (DiskShape->Brush->Color != clSilver)
    {
        if (DiskShape->Brush->Color == clBlue)
            DiskShape->Brush->Color = clGray;
        else
            DiskShape->Brush->Color = clBlue;
    }
    else
        DiskShape->Brush->Style = bsSolid;
}

void __fastcall TForm1::RateComboBoxChange(TObject *Sender)
{
    // user requested subsampling of data
    AnsiString cmd = "STREAMRATE " + AnsiString(RateComboBox->ItemIndex);
    vxiclient->device_write(cmd.c_str());
    // sync again
    char id[128];
    int val;
    if (vxiclient->device_write("STREAMRATE?"))
    {
        if (vxiclient->device_read(id))
        {
            val = AnsiString(id).Trim().ToInt();
            if (RateComboBox->ItemIndex != val)
            {
                Beep();
                RateComboBox->ItemIndex = val;
            }
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::WhatComboBoxChange(TObject *Sender)
{
    // user requested different data
    AnsiString cmd = "STREAMCH " + AnsiString(WhatComboBox->ItemIndex%4);
    vxiclient->device_write(cmd.c_str());
    cmd = "STREAMFMT " + AnsiString((int)(WhatComboBox->ItemIndex>=4));
    vxiclient->device_write(cmd.c_str());
    // sync again
    char id[128];
    int val;
    if (vxiclient->device_write("STREAMCH?"))
    {
        if (vxiclient->device_read(id))
        {
            val = AnsiString(id).Trim().ToInt();
            if (vxiclient->device_write("STREAMFMT?"))
            {
                if (vxiclient->device_read(id))
                    val = val + (4 * AnsiString(id).Trim().ToInt());
                if (WhatComboBox->ItemIndex != val)
                {
                    Beep();
                    WhatComboBox->ItemIndex = val;
                }
            }
        }
    }
}


// format data sample rate
AnsiString TForm1::formatRate(double Fs)
{
    AnsiString unit = " Hz";
    int exp=0;
    if (Fs >= 1.0e6)
    {
        exp = 2;
        unit = " MHz";
    }
    else if (Fs > 1.0e3)
    {
        exp = 1;
        unit = " kHz";
    }
    Fs = Fs / pow(1000.0, exp);
    int a=1,b=2;
    if (Fs > 99.95)
    {
        a = 3;
        b = 0;
    }
    else if (Fs > 9.995)
    {
        a = 2;
        b = 1;
    }
    AnsiString fmt = AnsiString::Format("%%%d.%df", ARRAYOFCONST((a, b)));
    return AnsiString::Format(fmt, ARRAYOFCONST((Fs))) + unit;
}
//---------------------------------------------------------------------------

// 2nd timer to update current instrument native data rate
void __fastcall TForm1::Timer2Timer(TObject *Sender)
{
    if (connected)
        syncState(false);
}
//---------------------------------------------------------------------------


void __fastcall TForm1::SaveDialog1CanClose(TObject *Sender, bool &CanClose)
{
    // make user pick binary or csv save file format
    AnsiString ext = ExtractFileExt(SaveDialog1->FileName).LowerCase();
    bool CSV = SaveDialog1->FilterIndex - 1;
    if (ext == ".csv" && !CSV)
    {
        SaveDialog1->FilterIndex = 2;
        CSV = true;
    }
    else if (ext == ".dat" && CSV)
    {
        SaveDialog1->FilterIndex = 1;
        CSV = false;
    }
    else if (ext.IsEmpty() || ((ext != ".dat") && (ext != ".csv")))
    {
        ShowMessage("You must choose either \".dat\" or \".csv\" file extension.");
        CanClose = false;
    }
}
//---------------------------------------------------------------------------



void __fastcall TForm1::ChecksumCheckBoxClick(TObject *Sender)
{
    sendCS = ChecksumCheckBox->Checked;
    int val = sendLE | (sendCS << 1);
    AnsiString cmd = AnsiString("STREAMOPTION ") + AnsiString(val);
    vxiclient->device_write(cmd.c_str());
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

#ifndef Unit1H
#define Unit1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include <fstream.h>
#include "UDPServerThread.h"
#include "vxi11.h"
#include <ExtCtrls.hpp>
#include <Sockets.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
    TSaveDialog *SaveDialog1;
    TTimer *Timer1;
    TPanel *Panel1;
    TTcpClient *TcpClient1;
    TMainMenu *MainMenu1;
    TTimer *Timer2;
    TLabel *Label10;
    TEdit *AddressEdit;
    TButton *ConnectBtn;
    TEdit *IDEdit;
    TPanel *Panel2;
    TLabel *Label11;
    TComboBox *WhatComboBox;
    TLabel *Label1;
    TEdit *PortEdit;
    TLabel *Label13;
    TEdit *MaxRate;
    TComboBox *RateComboBox;
    TButton *Button3;
    TLabel *Label12;
    TPanel *Panel3;
    TLabel *Label3;
    TButton *SaveButton;
    TEdit *FileEdit;
    TButton *Button1;
    TShape *DiskShape;
    TCheckBox *ChecksumCheckBox;
    TLabel *Label2;
    TEdit *WhatEdit;
    TLabel *Label4;
    TEdit *RateEdit;
    TLabel *Label9;
    TProgressBar *ProgressBar1;
    TShape *MissedShape;
    TEdit *ThEdit;
    TLabel *Label8;
    TEdit *REdit;
    TLabel *Label7;
    TEdit *YEdit;
    TLabel *Label6;
    TEdit *XEdit;
    TLabel *Label5;
    void __fastcall SaveButtonClick(TObject *Sender);
    void __fastcall Button1Click(TObject *Sender);
    void __fastcall Timer1Timer(TObject *Sender);
    void __fastcall PortEditExit(TObject *Sender);
    void __fastcall AddressEditExit(TObject *Sender);
    void __fastcall ConnectBtnClick(TObject *Sender);
    void __fastcall Button3Click(TObject *Sender);
    void __fastcall RateComboBoxChange(TObject *Sender);
    void __fastcall WhatComboBoxChange(TObject *Sender);
    void __fastcall Timer2Timer(TObject *Sender);
    void __fastcall SaveDialog1CanClose(TObject *Sender, bool &CanClose);
    void __fastcall ChecksumCheckBoxClick(TObject *Sender);

private:	// User declarations
    unsigned int vxiaddr, vxiport;
    bool vxiport_specd;
    bool streaming;
    vxi11_client *vxiclient;
    int port;
    UDPServerThread *serverThread;
    bool connected;
    double maxRateHz;
    int packetSize;
    bool isCSV;
    bool isLE;
    bool sendLE;
    bool sendCS;
    AnsiString prefFile;

public:		// User declarations
    __fastcall TForm1(TComponent* Owner);
    virtual __fastcall ~TForm1();

    void updateInfo();
    AnsiString formatFloat(float val);
    void updateVXIAddr();
    void syncState(bool syncall=true);
    AnsiString formatRate(double Fs);
    int validateIP(AnsiString &ip, bool set);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif

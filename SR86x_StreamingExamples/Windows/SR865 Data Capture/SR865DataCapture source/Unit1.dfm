object Form1: TForm1
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'SR865 Data Capture'
  ClientHeight = 252
  ClientWidth = 436
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  Menu = MainMenu1
  OldCreateOrder = False
  ShowHint = True
  PixelsPerInch = 96
  TextHeight = 13
  object Panel1: TPanel
    Left = 8
    Top = 7
    Width = 420
    Height = 63
    TabOrder = 0
    object Label10: TLabel
      Left = 11
      Top = 11
      Width = 53
      Height = 13
      Caption = 'Instrument'
    end
    object AddressEdit: TEdit
      Left = 70
      Top = 8
      Width = 278
      Height = 21
      Hint = 'Instrument IP Address'
      HideSelection = False
      TabOrder = 0
      Text = '192.168.0.8'
      OnExit = AddressEditExit
    end
    object ConnectBtn: TButton
      Left = 354
      Top = 6
      Width = 57
      Height = 25
      Hint = 'Connect to instrument'
      Caption = 'Connect'
      TabOrder = 1
      OnClick = ConnectBtnClick
    end
    object IDEdit: TEdit
      Left = 70
      Top = 35
      Width = 278
      Height = 21
      Hint = 'Instrument *IDN?'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 2
      Text = 'Instrument Identity'
    end
  end
  object Panel2: TPanel
    Left = 8
    Top = 76
    Width = 420
    Height = 61
    TabOrder = 1
    object Label11: TLabel
      Left = 11
      Top = 11
      Width = 26
      Height = 13
      Caption = 'What'
    end
    object Label1: TLabel
      Left = 11
      Top = 38
      Width = 43
      Height = 13
      Caption = 'UDP Port'
    end
    object Label13: TLabel
      Left = 195
      Top = 38
      Width = 46
      Height = 13
      Caption = 'Max Rate'
    end
    object Label12: TLabel
      Left = 195
      Top = 11
      Width = 23
      Height = 13
      Caption = 'Rate'
    end
    object WhatComboBox: TComboBox
      Left = 43
      Top = 8
      Width = 110
      Height = 21
      Hint = 'Select data to stream'
      Enabled = False
      ItemIndex = 1
      TabOrder = 0
      Text = 'X & Y (float)'
      OnChange = WhatComboBoxChange
      Items.Strings = (
        'X only (float)'
        'X & Y (float)'
        'R & '#952' (float)'
        'X, Y, R & '#952' (float)'
        'X only (int)'
        'X & Y (int)'
        'R & '#952' (int)'
        'X, Y, R & '#952' (int)')
    end
    object PortEdit: TEdit
      Left = 72
      Top = 35
      Width = 81
      Height = 21
      Hint = 'Streaming UDP port'
      Enabled = False
      TabOrder = 1
      Text = '12345'
      OnExit = PortEditExit
    end
    object MaxRate: TEdit
      Left = 247
      Top = 35
      Width = 55
      Height = 21
      Hint = 'Current instrument sample rate'
      Color = clInfoBk
      Enabled = False
      ReadOnly = True
      TabOrder = 2
      Text = '625 kHz'
    end
    object RateComboBox: TComboBox
      Left = 224
      Top = 8
      Width = 81
      Height = 21
      Hint = 'Select sub-sampling rate'
      Enabled = False
      ItemIndex = 0
      TabOrder = 3
      Text = 'native'
      OnChange = RateComboBoxChange
      Items.Strings = (
        'native'
        'decim 2x'
        'decim 4x'
        'decim 8x'
        'decim 16x'
        'decim 32x'
        'decim 64x'
        'decim 128x'
        'decim 256x'
        'decim 512x'
        'decim 1024x')
    end
    object Button3: TButton
      Left = 354
      Top = 6
      Width = 57
      Height = 25
      Hint = 'Start/stop streaming data'
      Caption = 'Start'
      Enabled = False
      TabOrder = 4
      OnClick = Button3Click
    end
    object ChecksumCheckBox: TCheckBox
      Left = 343
      Top = 37
      Width = 97
      Height = 17
      Hint = 'Enable data error-correction'
      Caption = 'Checksum'
      Checked = True
      Enabled = False
      State = cbChecked
      TabOrder = 5
      OnClick = ChecksumCheckBoxClick
    end
  end
  object Panel3: TPanel
    Left = 8
    Top = 143
    Width = 420
    Height = 104
    TabOrder = 2
    object Label3: TLabel
      Left = 21
      Top = 75
      Width = 16
      Height = 13
      Caption = 'File'
    end
    object DiskShape: TShape
      Left = 335
      Top = 72
      Width = 13
      Height = 20
      Hint = 'Save Indicator'
      Brush.Color = clSilver
      ParentShowHint = False
      ShowHint = True
    end
    object Label2: TLabel
      Left = 11
      Top = 12
      Width = 26
      Height = 13
      Caption = 'What'
    end
    object Label4: TLabel
      Left = 154
      Top = 12
      Width = 60
      Height = 13
      Caption = 'Sample Rate'
    end
    object Label9: TLabel
      Left = 281
      Top = 12
      Width = 49
      Height = 13
      Caption = 'Data Rate'
    end
    object MissedShape: TShape
      Left = 391
      Top = 36
      Width = 20
      Height = 20
      Hint = 'Dropped packet indicator'
      Brush.Color = clSilver
      ParentShowHint = False
      ShowHint = True
    end
    object Label8: TLabel
      Left = 309
      Top = 39
      Width = 6
      Height = 13
      Caption = #952
    end
    object Label7: TLabel
      Left = 213
      Top = 39
      Width = 7
      Height = 13
      Caption = 'R'
    end
    object Label6: TLabel
      Left = 116
      Top = 39
      Width = 6
      Height = 13
      Caption = 'Y'
    end
    object Label5: TLabel
      Left = 11
      Top = 39
      Width = 6
      Height = 13
      Caption = 'X'
    end
    object SaveButton: TButton
      Left = 354
      Top = 70
      Width = 57
      Height = 25
      Hint = 'Start/pause saving data'
      Caption = 'Save'
      TabOrder = 0
      OnClick = SaveButtonClick
    end
    object FileEdit: TEdit
      Left = 43
      Top = 72
      Width = 262
      Height = 21
      Hint = 'File to save data to'
      Enabled = False
      ReadOnly = True
      TabOrder = 1
      Text = '1234'
    end
    object Button1: TButton
      Left = 311
      Top = 70
      Width = 18
      Height = 25
      Hint = 'Select new file to save data to'
      Caption = '...'
      TabOrder = 2
      OnClick = Button1Click
    end
    object WhatEdit: TEdit
      Left = 43
      Top = 9
      Width = 96
      Height = 21
      Hint = 'Received data content'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 3
      Text = 'X, Y, R & '#952' (float)'
    end
    object RateEdit: TEdit
      Left = 220
      Top = 9
      Width = 55
      Height = 21
      Hint = 'Received data sample rate'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 4
      Text = '625 kHz'
    end
    object ProgressBar1: TProgressBar
      Left = 336
      Top = 10
      Width = 75
      Height = 17
      ParentShowHint = False
      ShowHint = True
      TabOrder = 5
    end
    object ThEdit: TEdit
      Left = 321
      Top = 36
      Width = 64
      Height = 21
      Hint = 'Received '#952' data'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 6
      Text = 'Edit1'
    end
    object REdit: TEdit
      Left = 226
      Top = 36
      Width = 66
      Height = 21
      Hint = 'Received R data'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 7
      Text = 'Edit1'
    end
    object YEdit: TEdit
      Left = 128
      Top = 36
      Width = 65
      Height = 21
      Hint = 'Received Y data'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 8
      Text = 'Edit1'
    end
    object XEdit: TEdit
      Left = 23
      Top = 36
      Width = 72
      Height = 21
      Hint = 'Received X data'
      Color = clInfoBk
      ReadOnly = True
      TabOrder = 9
      Text = 'Edit1'
    end
  end
  object SaveDialog1: TSaveDialog
    DefaultExt = 'dat'
    Filter = 'Binary Data|*.dat|Comma Separated Values|*.csv'
    FilterIndex = 0
    Options = [ofOverwritePrompt, ofHideReadOnly, ofPathMustExist, ofEnableSizing]
    OnCanClose = SaveDialog1CanClose
    Left = 18
    Top = 40
  end
  object Timer1: TTimer
    OnTimer = Timer1Timer
    Left = 354
    Top = 32
  end
  object TcpClient1: TTcpClient
    Left = 392
    Top = 40
  end
  object MainMenu1: TMainMenu
    Left = 48
    Top = 40
  end
  object Timer2: TTimer
    OnTimer = Timer2Timer
    Left = 304
    Top = 32
  end
end

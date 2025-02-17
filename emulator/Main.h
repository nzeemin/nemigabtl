﻿/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

#pragma once

#include "res\\resource.h"

//////////////////////////////////////////////////////////////////////


#define MAX_LOADSTRING 100

extern TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
extern TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name


extern HINSTANCE g_hInst; // current instance


//////////////////////////////////////////////////////////////////////
// Main Window

extern HWND g_hwnd;  // Main window handle

void MainWindow_RegisterClass();
BOOL CreateMainWindow();
void MainWindow_RestoreSettings();
void MainWindow_UpdateMenu();
void MainWindow_UpdateWindowTitle();
void MainWindow_UpdateAllViews();
BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
void MainWindow_ShowHideDebug();
void MainWindow_ShowHideToolbar();
void MainWindow_ShowHideKeyboard();
void MainWindow_ShowHideMemoryMap();
void MainWindow_AdjustWindowSize();

void MainWindow_SetToolbarImage(int commandId, int imageIndex);
void MainWindow_EnableToolbarItem(int commandId, BOOL enable);
void MainWindow_SetStatusbarText(int part, LPCTSTR message);
void MainWindow_SetStatusbarBitmap(int part, UINT resourceId);
void MainWindow_SetStatusbarIcon(int part, HICON hIcon);

enum ToolbarButtons
{
    ToolbarButtonRun = 0,
    ToolbarButtonReset = 1,
    // Separator
    ToolbarButtonColor = 3,
};

enum ToolbarButtonImages
{
    ToolbarImageRun = 0,
    ToolbarImagePause = 1,
    ToolbarImageReset = 2,
    ToolbarImageFloppyDisk = 3,
    ToolbarImageFloppySlot = 4,
    ToolbarImageCartridge = 5,
    ToolbarImageCartSlot = 6,
    ToolbarImageSoundOn = 7,
    ToolbarImageSoundOff = 8,
    ToolbarImageFloppyDiskWP = 9,
    ToolbarImageColorScreen = 10,
    ToolbarImageBWScreen = 11,
    ToolbarImageScreenshot = 12,
    ToolbarImageDebugger = 14,
    ToolbarImageStepInto = 15,
    ToolbarImageStepOver = 16,
    ToolbarImageWordByte = 18,
    ToolbarImageGotoAddress = 19,
    ToolbarImageMemoryCpu = 21,
    ToolbarImageMemoryRamLo = 22,
    ToolbarImageMemoryRamHi = 23,
};

enum StatusbarParts
{
    StatusbarPartMessage = 0,
    StatusbarPartFloppyEngine = 1,
    StatusbarPartFPS = 2,
    StatusbarPartUptime = 3,
};


enum ColorIndices
{
    ColorDebugText          = 0,
    ColorDebugBackCurrent   = 1,
    ColorDebugValueChanged  = 2,
    ColorDebugPrevious      = 3,
    ColorDebugMemoryRom     = 4,
    ColorDebugMemoryIO      = 5,
    ColorDebugMemoryNA      = 6,
    ColorDebugValue         = 7,
    ColorDebugValueRom      = 8,
    ColorDebugSubtitles     = 9,
    ColorDebugJump          = 10,
    ColorDebugJumpYes       = 11,
    ColorDebugJumpNo        = 12,
    ColorDebugJumpHint      = 13,
    ColorDebugHint          = 14,
    ColorDebugBreakpoint    = 15,
    ColorDebugHighlight     = 16,
    ColorDebugBreakptZone   = 17,

    ColorIndicesCount       = 18,
};


//////////////////////////////////////////////////////////////////////
// Settings

void Settings_Init();
void Settings_Done();
BOOL Settings_GetWindowRect(RECT * pRect);
void Settings_SetWindowRect(const RECT * pRect);
void Settings_SetWindowMaximized(BOOL flag);
BOOL Settings_GetWindowMaximized();
void Settings_SetConfiguration(int configuration);
int  Settings_GetConfiguration();
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetFloppyFilePath(int slot, LPTSTR buffer);
void Settings_SetFloppyMXFilePath(int slot, LPCTSTR sFilePath);
void Settings_GetFloppyMXFilePath(int slot, LPTSTR buffer);
void Settings_SetScreenViewMode(int mode);
int  Settings_GetScreenViewMode();
void Settings_SetDebug(BOOL flag);
BOOL Settings_GetDebug();
void Settings_GetDebugFontName(LPTSTR buffer);
void Settings_SetDebugFontName(LPCTSTR sFontName);
WORD Settings_GetDebugBreakpoint(int bpno);
void Settings_SetDebugBreakpoint(int bpno, WORD address);
void Settings_SetDebugMemoryMode(WORD mode);
WORD Settings_GetDebugMemoryMode();
void Settings_SetDebugMemoryAddress(WORD address);
WORD Settings_GetDebugMemoryAddress();
void Settings_SetDebugMemoryBase(WORD address);
WORD Settings_GetDebugMemoryBase();
BOOL Settings_GetDebugMemoryByte();
void Settings_SetDebugMemoryByte(BOOL flag);
void Settings_SetAutostart(BOOL flag);
BOOL Settings_GetAutostart();
void Settings_SetRealSpeed(WORD speed);
WORD Settings_GetRealSpeed();
void Settings_SetSound(BOOL flag);
BOOL Settings_GetSound();
void Settings_SetSoundVolume(WORD value);
WORD Settings_GetSoundVolume();
void Settings_SetToolbar(BOOL flag);
BOOL Settings_GetToolbar();
void Settings_SetKeyboard(BOOL flag);
BOOL Settings_GetKeyboard();
void Settings_SetMemoryMap(BOOL flag);
BOOL Settings_GetMemoryMap();
WORD Settings_GetSpriteAddress();
void Settings_SetSpriteAddress(WORD value);
WORD Settings_GetSpriteWidth();
void Settings_SetSpriteWidth(WORD value);
void Settings_SetSerial(BOOL flag);
BOOL Settings_GetSerial();
void Settings_GetSerialPort(LPTSTR buffer);
void Settings_SetSerialPort(LPCTSTR sValue);
void Settings_GetSerialConfig(DCB * pDcb);
void Settings_SetSerialConfig(const DCB * pDcb);
void Settings_SetParallel(BOOL flag);
BOOL Settings_GetParallel();
void Settings_SetNetwork(BOOL flag);
BOOL Settings_GetNetwork();
int  Settings_GetNetStation();
void Settings_SetNetStation(int value);
void Settings_GetNetComPort(LPTSTR buffer);
void Settings_SetNetComPort(LPCTSTR sValue);
void Settings_GetNetComConfig(DCB * pDcb);
void Settings_SetNetComConfig(const DCB * pDcb);

LPCTSTR Settings_GetColorFriendlyName(ColorIndices colorIndex);
COLORREF Settings_GetColor(ColorIndices colorIndex);
COLORREF Settings_GetDefaultColor(ColorIndices colorIndex);
void Settings_SetColor(ColorIndices colorIndex, COLORREF color);


//////////////////////////////////////////////////////////////////////
// Options

extern BOOL Option_AutoBoot;


//////////////////////////////////////////////////////////////////////

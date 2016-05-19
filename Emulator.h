/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.h

#pragma once

#include "emubase\Board.h"
#include "Views.h"


//////////////////////////////////////////////////////////////////////

enum EmulatorConfiguration
{
    EMU_CONF_NEMIGA303 = 303,
    EMU_CONF_NEMIGA405 = 405,
    EMU_CONF_NEMIGA406 = 406,
};


//////////////////////////////////////////////////////////////////////


extern CMotherboard* g_pBoard;
extern int g_nEmulatorConfiguration;  // Current configuration
extern BOOL g_okEmulatorRunning;

extern BYTE* g_pEmulatorRam;  // RAM values - for change tracking
extern BYTE* g_pEmulatorChangedRam;  // RAM change flags
extern WORD g_wEmulatorCpuPC;      // Current PC value
extern WORD g_wEmulatorPrevCpuPC;  // Previous PC value
extern WORD g_wEmulatorPpuPC;      // Current PC value
extern WORD g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


BOOL Emulator_Init();
BOOL Emulator_InitConfiguration(int configuration);
void Emulator_Done();
void Emulator_SetCPUBreakpoint(WORD address);
void Emulator_SetPPUBreakpoint(WORD address);
BOOL Emulator_IsBreakpoint();
void Emulator_SetSound(BOOL soundOnOff);
void Emulator_SetParallel(BOOL parallelOnOff);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
int  Emulator_SystemFrame();

void Emulator_GetScreenSize(int scrmode, int* pwid, int* phei);
const DWORD * Emulator_GetPalette();
void Emulator_PrepareScreenRGB32(void* pBits, int screenMode);

// Update cached values after Run or Step
void Emulator_OnUpdate();
WORD Emulator_GetChangeRamStatus(WORD address);

BOOL Emulator_SaveImage(LPCTSTR sFilePath);
BOOL Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////

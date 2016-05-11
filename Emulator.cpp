/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include <stdio.h>
#include <Share.h>
#include "Main.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase\Board.h"
#include "emubase\Processor.h"
//#include "SoundGen.h"


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;
WORD g_nEmulatorConfiguration;  // Current configuration
BOOL g_okEmulatorRunning = FALSE;

WORD m_wEmulatorCPUBreakpoint = 0177777;

BOOL m_okEmulatorSound = FALSE;
BOOL m_okEmulatorCovox = FALSE;

BOOL m_okEmulatorParallel = FALSE;
FILE* m_fpEmulatorParallelOut = NULL;

long m_nFrameCount = 0;
DWORD m_dwTickCount = 0;
DWORD m_dwEmulatorUptime = 0;  // Machine uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

BYTE* g_pEmulatorRam;  // RAM values - for change tracking
BYTE* g_pEmulatorChangedRam;  // RAM change flags
WORD g_wEmulatorCpuPC = 0177777;      // Current PC value
WORD g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value


void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R);

//////////////////////////////////////////////////////////////////////
//Прототип функции преобразования экрана
// Input:
//   pVideoBuffer   Исходные данные, биты экрана БК
//   okSmallScreen  Признак "малого" экрана
//   pPalette       Палитра
//   scroll         Текущее значение скроллинга
//   pImageBits     Результат, 32-битный цвет, размер для каждой функции свой
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(const BYTE* pVideoBuffer, const DWORD* pPalette, void* pImageBits);

void CALLBACK Emulator_PrepareScreenBW512x256(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW512x312(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);

struct ScreenModeStruct
{
    int width;
    int height;
    PREPARE_SCREEN_CALLBACK callback;
}
static ScreenModeReference[] =
{
    {  512, 256, Emulator_PrepareScreenBW512x256 },
    {  512, 312, Emulator_PrepareScreenBW512x312 },
    //{  768, 468, Emulator_PrepareScreenBW768x468 },
    //{ 1024, 624, Emulator_PrepareScreenBW1024x624 },
};

const DWORD ScreenView_Palette[4] =
{
    0x000000, 0xa0a0a0, 0xd0d0d0, 0xFFFFFF
};


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_ROM_PROC    = _T("nemiga-303.rom");


//////////////////////////////////////////////////////////////////////

BOOL Emulator_LoadRomFile(LPCTSTR strFileName, BYTE* buffer, DWORD fileOffset, DWORD bytesToRead)
{
    FILE* fpRomFile = ::_tfsopen(strFileName, _T("rb"), _SH_DENYWR);
    if (fpRomFile == NULL)
        return FALSE;

    ::memset(buffer, 0, bytesToRead);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    DWORD dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return FALSE;
    }

    ::fclose(fpRomFile);

    return TRUE;
}

BOOL Emulator_Init()
{
    ASSERT(g_pBoard == NULL);

    CProcessor::Init();

    g_pBoard = new CMotherboard();

    // Allocate memory for old RAM values
    g_pEmulatorRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorRam, 0, 65536);
    g_pEmulatorChangedRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorChangedRam, 0, 65536);

    g_pBoard->Reset();

    //if (m_okEmulatorSound)
    //{
    //    SoundGen_Initialize(Settings_GetSoundVolume());
    //    g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    //}

    return TRUE;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != NULL);

    CProcessor::Done();

    //g_pBoard->SetSoundGenCallback(NULL);
    //SoundGen_Finalize();

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);
    ::free(g_pEmulatorChangedRam);
}

BOOL Emulator_InitConfiguration(WORD configuration)
{
    g_pBoard->SetConfiguration(configuration);

    BYTE buffer[4096];

    // Load ROM file
    if (!Emulator_LoadRomFile(FILENAME_ROM_PROC, buffer, 0, 4096))
    {
        AlertWarning(_T("Failed to load the ROM file."));
        return FALSE;
    }
    g_pBoard->LoadROM(buffer);

    g_nEmulatorConfiguration = configuration;

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    return TRUE;
}

void Emulator_Start()
{
    g_okEmulatorRunning = TRUE;

    // Set title bar text
    SetWindowText(g_hwnd, _T("NEMIGA Back to Life [run]"));
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();
}
void Emulator_Stop()
{
    g_okEmulatorRunning = FALSE;
    m_wEmulatorCPUBreakpoint = 0177777;

    if (m_fpEmulatorParallelOut != NULL)
        ::fflush(m_fpEmulatorParallelOut);

    // Reset title bar message
    SetWindowText(g_hwnd, _T("NEMIGA Back to Life [stop]"));
    MainWindow_UpdateMenu();
    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, _T(""));

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != NULL);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    MainWindow_UpdateAllViews();
}

void Emulator_SetCPUBreakpoint(WORD address)
{
    m_wEmulatorCPUBreakpoint = address;
}

BOOL Emulator_IsBreakpoint()
{
    WORD wCPUAddr = g_pBoard->GetCPU()->GetPC();
    if (wCPUAddr == m_wEmulatorCPUBreakpoint)
        return TRUE;
    return FALSE;
}

void Emulator_SetSound(BOOL soundOnOff)
{
    //if (m_okEmulatorSound != soundOnOff)
    //{
    //    if (soundOnOff)
    //    {
    //        SoundGen_Initialize(Settings_GetSoundVolume());
    //        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    //    }
    //    else
    //    {
    //        g_pBoard->SetSoundGenCallback(NULL);
    //        SoundGen_Finalize();
    //    }
    //}

    m_okEmulatorSound = soundOnOff;
}

BOOL CALLBACK Emulator_ParallelOut_Callback(BYTE byte)
{
    if (m_fpEmulatorParallelOut != NULL)
    {
        ::fwrite(&byte, 1, 1, m_fpEmulatorParallelOut);
    }

    ////DEBUG
    //TCHAR buffer[32];
    //_snwprintf_s(buffer, 32, _T("Printer: <%02x>\r\n"), byte);
    //ConsoleView_Print(buffer);

    return TRUE;
}

void Emulator_SetParallel(BOOL parallelOnOff)
{
    if (m_okEmulatorParallel == parallelOnOff)
        return;

    if (!parallelOnOff)
    {
        g_pBoard->SetParallelOutCallback(NULL);
        if (m_fpEmulatorParallelOut != NULL)
            ::fclose(m_fpEmulatorParallelOut);
    }
    else
    {
        g_pBoard->SetParallelOutCallback(Emulator_ParallelOut_Callback);
        m_fpEmulatorParallelOut = ::_tfopen(_T("printer.log"), _T("wb"));
    }

    m_okEmulatorParallel = parallelOnOff;
}

int Emulator_SystemFrame()
{
    g_pBoard->SetCPUBreakpoint(m_wEmulatorCPUBreakpoint);

    ScreenView_ScanKeyboard();
    ScreenView_ProcessKeyboard();

    if (!g_pBoard->SystemFrame())
        return 0;

    // Calculate frames per second
    m_nFrameCount++;
    DWORD dwCurrentTicks = GetTickCount();
    long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1200)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        TCHAR buffer[16];
        swprintf_s(buffer, 16, _T("FPS: %05.2f"), dFramesPerSecond);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        BOOL floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : NULL);

        m_nFrameCount = 0;
        m_dwTickCount = dwCurrentTicks;
    }

    // Calculate emulator uptime (25 frames per second)
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= 25)
    {
        m_dwEmulatorUptime++;
        m_nUptimeFrameCount = 0;

        int seconds = (int) (m_dwEmulatorUptime % 60);
        int minutes = (int) (m_dwEmulatorUptime / 60 % 60);
        int hours   = (int) (m_dwEmulatorUptime / 3600 % 60);

        TCHAR buffer[20];
        swprintf_s(buffer, 20, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    return 1;
}

//void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R)
//{
//    if (m_okEmulatorCovox)
//    {
//        // Get lower byte from printer port output register
//        unsigned short data = g_pBoard->GetPrinterOutPort() & 0xff;
//        // Merge with channel data
//        L += (data << 7);
//        R += (data << 7);
//    }
//
//    SoundGen_FeedDAC(L, R);
//}

// Update cached values after Run or Step
void Emulator_OnUpdate()
{
    // Update stored PC value
    g_wEmulatorPrevCpuPC = g_wEmulatorCpuPC;
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Update memory change flags
    {
        BYTE* pOld = g_pEmulatorRam;
        BYTE* pChanged = g_pEmulatorChangedRam;
        WORD addr = 0;
        do
        {
            BYTE newvalue = g_pBoard->GetRAMByte(addr);
            BYTE oldvalue = *pOld;
            *pChanged = (newvalue != oldvalue) ? 255 : 0;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag
//   addrtype - address mode - see ADDRTYPE_XXX constants
WORD Emulator_GetChangeRamStatus(WORD address)
{
    return *((WORD*)(g_pEmulatorChangedRam + address));
}

void Emulator_GetScreenSize(int scrmode, int* pwid, int* phei)
{
    if (scrmode < 0 || scrmode >= sizeof(ScreenModeReference) / sizeof(ScreenModeStruct))
        return;
    ScreenModeStruct* pinfo = ScreenModeReference + scrmode;
    *pwid = pinfo->width;
    *phei = pinfo->height;
}

void Emulator_PrepareScreenRGB32(void* pImageBits, int screenMode)
{
    if (pImageBits == NULL) return;

    const BYTE* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != NULL);

    // Render to bitmap
    PREPARE_SCREEN_CALLBACK callback = ScreenModeReference[screenMode].callback;
    callback(pVideoBuffer, ScreenView_Palette, pImageBits);
}

const DWORD * Emulator_GetPalette()
{
    return ScreenView_Palette;
}

void CALLBACK Emulator_PrepareScreenBW512x256(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y++)
    {
        const WORD* pVideo = (WORD*)(pVideoBuffer + y * 512 / 4);
        DWORD* pBits = (DWORD*)pImageBits + (256 - 1 - y) * 512;
        for (int x = 0; x < 512 / 8; x++)
        {
            WORD src = *pVideo;

            for (int bit = 0; bit < 8; bit++)
            {
                int colorindex = (src & 0x80) >> 7 | (src & 0x8000) >> 14;
                DWORD color = palette[colorindex];
                *pBits = color;
                pBits++;
                src = src << 1;
            }

            pVideo++;
        }
    }
}

void CALLBACK Emulator_PrepareScreenBW512x312(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits)
{
    DWORD * pImageStart = ((DWORD *)pImageBits) + 512 * 28;
    Emulator_PrepareScreenBW512x256(pVideoBuffer, palette, pImageStart);
	//TODO
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        BK_IMAGE_HEADER1
//   4 bytes        BK_IMAGE_HEADER2
//   4 bytes        BK_IMAGE_VERSION
//   4 bytes        BK_IMAGE_SIZE
//   4 bytes        BK uptime
//   12 bytes       Not used

//void Emulator_SaveImage(LPCTSTR sFilePath)
//{
//    // Create file
//    HANDLE hFile = CreateFile(sFilePath,
//            GENERIC_WRITE, FILE_SHARE_READ, NULL,
//            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
//    if (hFile == INVALID_HANDLE_VALUE)
//    {
//        AlertWarning(_T("Failed to save image file."));
//        return;
//    }
//
//    // Allocate memory
//    BYTE* pImage = (BYTE*) ::malloc(BKIMAGE_SIZE);  memset(pImage, 0, BKIMAGE_SIZE);
//    ::memset(pImage, 0, BKIMAGE_SIZE);
//    // Prepare header
//    DWORD* pHeader = (DWORD*) pImage;
//    *pHeader++ = BKIMAGE_HEADER1;
//    *pHeader++ = BKIMAGE_HEADER2;
//    *pHeader++ = BKIMAGE_VERSION;
//    *pHeader++ = BKIMAGE_SIZE;
//    // Store emulator state to the image
//    //g_pBoard->SaveToImage(pImage);
//    *(DWORD*)(pImage + 16) = m_dwEmulatorUptime;
//
//    // Save image to the file
//    DWORD dwBytesWritten = 0;
//    WriteFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesWritten, NULL);
//    //TODO: Check if dwBytesWritten != BKIMAGE_SIZE
//
//    // Free memory, close file
//    ::free(pImage);
//    CloseHandle(hFile);
//}

//void Emulator_LoadImage(LPCTSTR sFilePath)
//{
//    // Open file
//    HANDLE hFile = CreateFile(sFilePath,
//            GENERIC_READ, FILE_SHARE_READ, NULL,
//            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//    if (hFile == INVALID_HANDLE_VALUE)
//    {
//        AlertWarning(_T("Failed to load image file."));
//        return;
//    }
//
//    // Read header
//    DWORD bufHeader[BKIMAGE_HEADER_SIZE / sizeof(DWORD)];
//    DWORD dwBytesRead = 0;
//    ReadFile(hFile, bufHeader, BKIMAGE_HEADER_SIZE, &dwBytesRead, NULL);
//    //TODO: Check if dwBytesRead != BKIMAGE_HEADER_SIZE
//
//    //TODO: Check version and size
//
//    // Allocate memory
//    BYTE* pImage = (BYTE*) ::malloc(BKIMAGE_SIZE);  ::memset(pImage, 0, BKIMAGE_SIZE);
//
//    // Read image
//    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
//    dwBytesRead = 0;
//    ReadFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesRead, NULL);
//    //TODO: Check if dwBytesRead != BKIMAGE_SIZE
//
//    // Restore emulator state from the image
//    //g_pBoard->LoadFromImage(pImage);
//
//    m_dwEmulatorUptime = *(DWORD*)(pImage + 16);
//
//    // Free memory, close file
//    ::free(pImage);
//    CloseHandle(hFile);
//
//    g_okEmulatorRunning = FALSE;
//
//    MainWindow_UpdateAllViews();
//}


//////////////////////////////////////////////////////////////////////

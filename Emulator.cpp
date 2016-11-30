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
#include "SoundGen.h"


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;
int g_nEmulatorConfiguration;  // Current configuration
BOOL g_okEmulatorRunning = FALSE;

WORD m_wEmulatorCPUBreakpoint = 0177777;

BOOL m_okEmulatorSound = FALSE;
BOOL m_okEmulatorCovox = FALSE;

BOOL m_okEmulatorParallel = FALSE;
BOOL m_okEmulatorSerial = FALSE;
HANDLE m_hEmulatorComPort = INVALID_HANDLE_VALUE;

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
//   pPalette       Палитра
//   pImageBits     Результат, 32-битный цвет, размер для каждой функции свой
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(const BYTE* pVideoBuffer, const DWORD* pPalette, void* pImageBits);

void CALLBACK Emulator_PrepareScreenBW512x256(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW512x312(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW768x468(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW1024x624(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits);

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
    {  768, 468, Emulator_PrepareScreenBW768x468 },
    { 1024, 624, Emulator_PrepareScreenBW1024x624 },
};

const DWORD ScreenView_Palette[4] =
{
    0x000000, 0xB0B0B0, 0x404040, 0xFFFFFF
};


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_ROM_303 = _T("nemiga-303.rom");
const LPCTSTR FILENAME_ROM_405 = _T("nemiga-405.rom");
const LPCTSTR FILENAME_ROM_406 = _T("nemiga-406.rom");


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

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    }

    return TRUE;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != NULL);

    CProcessor::Done();

    g_pBoard->SetSoundGenCallback(NULL);
    SoundGen_Finalize();

    g_pBoard->SetSerialCallbacks(NULL, NULL);
    if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorComPort);
        m_hEmulatorComPort = INVALID_HANDLE_VALUE;
    }

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);
    ::free(g_pEmulatorChangedRam);
}

BOOL Emulator_InitConfiguration(int configuration)
{
    g_pBoard->SetConfiguration(configuration);

    LPCTSTR szRomFileName = NULL;
    switch (configuration)
    {
    case EMU_CONF_NEMIGA303:
        szRomFileName = FILENAME_ROM_303;
        break;
    case EMU_CONF_NEMIGA405:
        szRomFileName = FILENAME_ROM_405;
        break;
    case EMU_CONF_NEMIGA406:
        szRomFileName = FILENAME_ROM_406;
        break;
    default:
        szRomFileName = FILENAME_ROM_303;
        break;
    }

    BYTE buffer[4096];

    // Load ROM file
    if (!Emulator_LoadRomFile(szRomFileName, buffer, 0, 4096))
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
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(NULL);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

BOOL CALLBACK Emulator_SerialIn_Callback(BYTE* pByte)
{
    DWORD dwBytesRead;
    BOOL result = ::ReadFile(m_hEmulatorComPort, pByte, 1, &dwBytesRead, NULL);

    return result && (dwBytesRead == 1);
}

BOOL CALLBACK Emulator_SerialOut_Callback(BYTE byte)
{
    DWORD dwBytesWritten;
    ::WriteFile(m_hEmulatorComPort, &byte, 1, &dwBytesWritten, NULL);

    return (dwBytesWritten == 1);
}

BOOL Emulator_SetSerial(BOOL serialOnOff, LPCTSTR serialPort)
{
    if (m_okEmulatorSerial != serialOnOff)
    {
        if (serialOnOff)
        {
            // Prepare port name
            TCHAR port[15];
            wsprintf(port, _T("\\\\.\\%s"), serialPort);

            // Open port
            m_hEmulatorComPort = ::CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_hEmulatorComPort == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open COM port (0x%08lx)."), dwError);
                return false;
            }

            // Set port settings
            DCB dcb;
            Settings_GetSerialConfig(&dcb);
            if (!::SetCommState(m_hEmulatorComPort, &dcb))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the COM port (0x%08lx)."), dwError);
                return false;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorComPort, &timeouts))
            {
                DWORD dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the COM port timeouts (0x%08lx)."), dwError);
                return false;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorComPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetSerialCallbacks(Emulator_SerialIn_Callback, Emulator_SerialOut_Callback);
        }
        else
        {
            g_pBoard->SetSerialCallbacks(NULL, NULL);  // Reset callbacks

            // Close port
            if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorSerial = serialOnOff;

    return TRUE;
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
        double dSpeed = dFramesPerSecond / 25.0 * 100;
        TCHAR buffer[16];
        swprintf_s(buffer, 16, _T("%03.f%%"), dSpeed);
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

    // Auto-boot option processing: select "boot from disk" and press Enter
    if (Option_AutoBoot)
    {
        if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 16)
        {
            ScreenView_KeyEvent(68, TRUE);  // Press "D"
            Option_AutoBoot = false;  // All done
        }
    }

    return 1;
}

void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R)
{
    SoundGen_FeedDAC(L, R);
}

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

#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

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
}

void CALLBACK Emulator_PrepareScreenBW768x468(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y += 2)
    {
        const WORD* psrc1 = (WORD*)(pVideoBuffer + y * 512 / 4);
        const WORD* psrc2 = (WORD*)(pVideoBuffer + (y + 1) * 512 / 4);
        DWORD* pdest1 = ((DWORD*)pImageBits) + (426 - 1 - y / 2 * 3) * 768;
        DWORD* pdest2 = pdest1 - 768;
        DWORD* pdest3 = pdest2 - 768;
        for (int x = 0; x < 512 / 8; x++)
        {
            WORD src1 = *psrc1;
            WORD src2 = *psrc2;
            for (int bit = 0; bit < 4; bit++)
            {
                int colorindex1a = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                int colorindex1b = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                DWORD c1a = palette[colorindex1a];
                DWORD c1b = palette[colorindex1b];
                int colorindex2a = (src2 & 0x80) >> 7 | (src2 & 0x8000) >> 14;  src2 = src2 << 1;
                int colorindex2b = (src2 & 0x80) >> 7 | (src2 & 0x8000) >> 14;  src2 = src2 << 1;
                DWORD c2a = palette[colorindex2a];
                DWORD c2b = palette[colorindex2b];

                DWORD c1ab = AVERAGERGB(c1a, c1b);
                DWORD c2ab = AVERAGERGB(c2a, c2b);
                DWORD c12a = AVERAGERGB(c1a, c2a);
                DWORD c12b = AVERAGERGB(c1b, c2b);
                DWORD c12ab = AVERAGERGB(c1ab, c2ab);

                (*pdest1++) = c1a;   (*pdest1++) = c1ab;  (*pdest1++) = c1b;
                (*pdest2++) = c12a;  (*pdest2++) = c12ab; (*pdest2++) = c12b;
                (*pdest3++) = c2a;   (*pdest3++) = c2ab;  (*pdest3++) = c2b;
            }
            psrc1++;
            psrc2++;
        }
    }
}

void CALLBACK Emulator_PrepareScreenBW1024x624(const BYTE* pVideoBuffer, const DWORD* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y++)
    {
        const WORD* pVideo = (WORD*)(pVideoBuffer + y * 512 / 4);
        DWORD* pBits1 = (DWORD*)pImageBits + (568 - 1 - y * 2) * 1024;
        DWORD* pBits2 = pBits1 - 1024;
        for (int x = 0; x < 512 / 8; x++)
        {
            WORD src = *pVideo;

            for (int bit = 0; bit < 8; bit++)
            {
                int colorindex = (src & 0x80) >> 7 | (src & 0x8000) >> 14;
                DWORD color = palette[colorindex];

                (*pBits1++) = color;  (*pBits1++) = color;
                (*pBits2++) = color;  (*pBits2++) = color;

                src = src << 1;
            }

            pVideo++;
        }
    }
}



//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        NEMIGAIMAGE_HEADER1
//   4 bytes        NEMIGAIMAGE_HEADER2
//   4 bytes        NEMIGAIMAGE_VERSION
//   4 bytes        NEMIGAIMAGE_SIZE
//   4 bytes        NEMIGA uptime
//   12 bytes       Not used

BOOL Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("w+b"), _SH_DENYWR);
    if (fpFile == NULL)
        return false;

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(NEMIGAIMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return false;
    }
    memset(pImage, 0, NEMIGAIMAGE_SIZE);
    // Prepare header
    DWORD* pHeader = (DWORD*) pImage;
    *pHeader++ = NEMIGAIMAGE_HEADER1;
    *pHeader++ = NEMIGAIMAGE_HEADER2;
    *pHeader++ = NEMIGAIMAGE_VERSION;
    *pHeader++ = NEMIGAIMAGE_SIZE;
    // Store emulator state to the image
    g_pBoard->SaveToImage(pImage);
    *(DWORD*)(pImage + 16) = m_dwEmulatorUptime;

    // Save image to the file
    size_t dwBytesWritten = ::fwrite(pImage, 1, NEMIGAIMAGE_SIZE, fpFile);
    ::free(pImage);
    ::fclose(fpFile);
    if (dwBytesWritten != NEMIGAIMAGE_SIZE)
        return false;

    return TRUE;
}

BOOL Emulator_LoadImage(LPCTSTR sFilePath)
{
    Emulator_Stop();

    // Open file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR);
    if (fpFile == NULL)
        return false;

    // Read header
    DWORD bufHeader[NEMIGAIMAGE_HEADER_SIZE / sizeof(DWORD)];
    DWORD dwBytesRead = ::fread(bufHeader, 1, NEMIGAIMAGE_HEADER_SIZE, fpFile);
    if (dwBytesRead != NEMIGAIMAGE_HEADER_SIZE)
    {
        ::fclose(fpFile);
        return false;
    }

    //TODO: Check version and size

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(NEMIGAIMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return false;
    }

    // Read image
    ::fseek(fpFile, 0, SEEK_SET);
    dwBytesRead = ::fread(pImage, 1, NEMIGAIMAGE_SIZE, fpFile);
    if (dwBytesRead != NEMIGAIMAGE_SIZE)
    {
        ::free(pImage);
        ::fclose(fpFile);
        return false;
    }

    // Restore emulator state from the image
    g_pBoard->LoadFromImage(pImage);

    m_dwEmulatorUptime = *(DWORD*)(pImage + 16);

    // Free memory, close file
    ::free(pImage);
    ::fclose(fpFile);

    return TRUE;
}


//////////////////////////////////////////////////////////////////////

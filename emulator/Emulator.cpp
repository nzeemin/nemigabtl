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
#include "emubase\Emubase.h"
#include "SoundGen.h"

//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = nullptr;
int g_nEmulatorConfiguration;  // Current configuration
bool g_okEmulatorRunning = false;

int m_wEmulatorCPUBpsCount = 0;
uint16_t m_EmulatorCPUBps[MAX_BREAKPOINTCOUNT + 1];
uint16_t m_wEmulatorTempCPUBreakpoint = 0177777;
int m_wEmulatorWatchesCount = 0;
uint16_t m_EmulatorWatches[MAX_BREAKPOINTCOUNT + 1];

bool m_okEmulatorSound = false;
uint16_t m_wEmulatorSoundSpeed = 100;
bool m_okEmulatorCovox = false;

bool m_okEmulatorParallel = false;
bool m_okEmulatorSerial = false;
HANDLE m_hEmulatorComPort = INVALID_HANDLE_VALUE;

FILE* m_fpEmulatorParallelOut = nullptr;

long m_nFrameCount = 0;
uint32_t m_dwTickCount = 0;
uint32_t m_dwEmulatorUptime = 0;  // Machine uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

uint8_t* g_pEmulatorRam = nullptr;  // RAM values - for change tracking
uint8_t* g_pEmulatorChangedRam = nullptr;  // RAM change flags
uint16_t g_wEmulatorCpuPC = 0177777;      // Current PC value
uint16_t g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value


void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R);

//////////////////////////////////////////////////////////////////////
//Прототип функции преобразования экрана
// Input:
//   pVideoBuffer   Исходные данные, биты экрана БК
//   pPalette       Палитра
//   pImageBits     Результат, 32-битный цвет, размер для каждой функции свой
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(const uint8_t* pVideoBuffer, const uint32_t* pPalette, void* pImageBits);

void CALLBACK Emulator_PrepareScreenBW512x256(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW512x312(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW768x468(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW896x624(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW1024x624(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits);

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
    {  896, 624, Emulator_PrepareScreenBW896x624 },
    { 1024, 624, Emulator_PrepareScreenBW1024x624 },
};

const uint32_t ScreenView_Palette[4] =
{
    0x000000, 0xB0B0B0, 0x404040, 0xFFFFFF
};


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_ROM_303 = _T("nemiga-303.rom");
const LPCTSTR FILENAME_ROM_405 = _T("nemiga-405.rom");
const LPCTSTR FILENAME_ROM_406 = _T("nemiga-406.rom");


//////////////////////////////////////////////////////////////////////

bool Emulator_LoadRomFile(LPCTSTR strFileName, uint8_t* buffer, uint32_t fileOffset, uint32_t bytesToRead)
{
    FILE* fpRomFile = ::_tfsopen(strFileName, _T("rb"), _SH_DENYWR);
    if (fpRomFile == nullptr)
        return false;

    ::memset(buffer, 0, bytesToRead);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    uint32_t dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return false;
    }

    ::fclose(fpRomFile);

    return true;
}

bool Emulator_Init()
{
    ASSERT(g_pBoard == nullptr);

    CProcessor::Init();

    m_wEmulatorCPUBpsCount = 0;
    for (int i = 0; i <= MAX_BREAKPOINTCOUNT; i++)
    {
        m_EmulatorCPUBps[i] = 0177777;
    }
    m_wEmulatorWatchesCount = 0;
    for (int i = 0; i <= MAX_WATCHPOINTCOUNT; i++)
    {
        m_EmulatorWatches[i] = 0177777;
    }

    g_pBoard = new CMotherboard();

    // Allocate memory for old RAM values
    g_pEmulatorRam = static_cast<uint8_t*>(::calloc(128 * 1024, 1));
    g_pEmulatorChangedRam = static_cast<uint8_t*>(::calloc(128 * 1024, 1));

    g_pBoard->Reset();

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    }

    return true;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != nullptr);

    CProcessor::Done();

    g_pBoard->SetSoundGenCallback(nullptr);
    SoundGen_Finalize();

    g_pBoard->SetSerialCallbacks(nullptr, nullptr);
    if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorComPort);
        m_hEmulatorComPort = INVALID_HANDLE_VALUE;
    }

    delete g_pBoard;
    g_pBoard = nullptr;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);  g_pEmulatorRam = nullptr;
    ::free(g_pEmulatorChangedRam);  g_pEmulatorChangedRam = nullptr;
}

bool Emulator_InitConfiguration(uint16_t configuration)
{
    g_pBoard->SetConfiguration(configuration);

    LPCTSTR szRomFileName = nullptr;
    uint16_t nRomResourceId;
    switch (configuration)
    {
    case EMU_CONF_NEMIGA303:
        szRomFileName = FILENAME_ROM_303;
        nRomResourceId = IDR_NEMIGA_ROM_303;
        break;
    case EMU_CONF_NEMIGA405:
        szRomFileName = FILENAME_ROM_405;
        nRomResourceId = IDR_NEMIGA_ROM_405;
        break;
    case EMU_CONF_NEMIGA406:
        szRomFileName = FILENAME_ROM_406;
        nRomResourceId = IDR_NEMIGA_ROM_406;
        break;
    default:
        szRomFileName = FILENAME_ROM_303;
        nRomResourceId = IDR_NEMIGA_ROM_303;
        break;
    }

    uint8_t buffer[4096];

    // Load ROM file
    if (!Emulator_LoadRomFile(szRomFileName, buffer, 0, 4096))
    {
        // ROM file not found or failed to load, load the ROM from resource instead
        HRSRC hRes = NULL;
        uint32_t dwDataSize = 0;
        HGLOBAL hResLoaded = NULL;
        void * pResData = nullptr;
        if ((hRes = ::FindResource(NULL, MAKEINTRESOURCE(nRomResourceId), _T("BIN"))) == NULL ||
            (dwDataSize = ::SizeofResource(NULL, hRes)) < 4096 ||
            (hResLoaded = ::LoadResource(NULL, hRes)) == NULL ||
            (pResData = ::LockResource(hResLoaded)) == NULL)
        {
            AlertWarning(_T("Failed to load the ROM."));
            return false;
        }
        ::memcpy(buffer, pResData, 4096);
    }
    g_pBoard->LoadROM(buffer);

    g_nEmulatorConfiguration = configuration;

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    return true;
}

void Emulator_Start()
{
    g_okEmulatorRunning = true;

    // Set title bar text
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();

    // For proper breakpoint processing
    if (m_wEmulatorCPUBpsCount != 0)
    {
        g_pBoard->GetCPU()->ClearInternalTick();
    }
}
void Emulator_Stop()
{
    g_okEmulatorRunning = false;

    Emulator_SetTempCPUBreakpoint(0177777);

    if (m_fpEmulatorParallelOut != nullptr)
        ::fflush(m_fpEmulatorParallelOut);

    // Reset title bar message
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, nullptr);

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != nullptr);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    MainWindow_UpdateAllViews();
}

bool Emulator_AddCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == MAX_BREAKPOINTCOUNT - 1 || address == 0177777)
        return false;
    for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)  // Check if the BP exists
    {
        if (m_EmulatorCPUBps[i] == address)
            return false;  // Already in the list
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)  // Put in the first empty cell
    {
        if (m_EmulatorCPUBps[i] == 0177777)
        {
            m_EmulatorCPUBps[i] = address;
            break;
        }
    }
    m_wEmulatorCPUBpsCount++;
    return true;
}
bool Emulator_RemoveCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == 0 || address == 0177777)
        return false;
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
    {
        if (m_EmulatorCPUBps[i] == address)
        {
            m_EmulatorCPUBps[i] = 0177777;
            m_wEmulatorCPUBpsCount--;
            if (m_wEmulatorCPUBpsCount > i)  // fill the hole
            {
                m_EmulatorCPUBps[i] = m_EmulatorCPUBps[m_wEmulatorCPUBpsCount];
                m_EmulatorCPUBps[m_wEmulatorCPUBpsCount] = 0177777;
            }
            return true;
        }
    }
    return false;
}
void Emulator_SetTempCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorTempCPUBreakpoint != 0177777)
        Emulator_RemoveCPUBreakpoint(m_wEmulatorTempCPUBreakpoint);
    if (address == 0177777)
    {
        m_wEmulatorTempCPUBreakpoint = 0177777;
        return;
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
    {
        if (m_EmulatorCPUBps[i] == address)
            return;  // We have regular breakpoint with the same address
    }
    m_wEmulatorTempCPUBreakpoint = address;
    m_EmulatorCPUBps[m_wEmulatorCPUBpsCount] = address;
    m_wEmulatorCPUBpsCount++;
}
const uint16_t* Emulator_GetCPUBreakpointList() { return m_EmulatorCPUBps; }
bool Emulator_IsBreakpoint()
{
    uint16_t address = g_pBoard->GetCPU()->GetPC();
    if (m_wEmulatorCPUBpsCount > 0)
    {
        for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)
        {
            if (address == m_EmulatorCPUBps[i])
                return true;
        }
    }
    return false;
}
bool Emulator_IsBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == 0)
        return false;
    for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)
    {
        if (address == m_EmulatorCPUBps[i])
            return true;
    }
    return false;
}
void Emulator_RemoveAllBreakpoints()
{
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
        m_EmulatorCPUBps[i] = 0177777;
    m_wEmulatorCPUBpsCount = 0;
}

bool Emulator_AddWatchpoint(uint16_t address)
{
    if (m_wEmulatorWatchesCount == MAX_WATCHPOINTCOUNT - 1 || address == 0177777)
        return false;
    for (int i = 0; i < m_wEmulatorWatchesCount; i++)  // Check if the BP exists
    {
        if (m_EmulatorWatches[i] == address)
            return false;  // Already in the list
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)  // Put in the first empty cell
    {
        if (m_EmulatorWatches[i] == 0177777)
        {
            m_EmulatorWatches[i] = address;
            break;
        }
    }
    m_wEmulatorWatchesCount++;
    return true;
}
const uint16_t* Emulator_GetWatchpointList() { return m_EmulatorWatches; }
bool Emulator_RemoveWatchpoint(uint16_t address)
{
    if (m_wEmulatorWatchesCount == 0 || address == 0177777)
        return false;
    for (int i = 0; i < MAX_WATCHPOINTCOUNT; i++)
    {
        if (m_EmulatorWatches[i] == address)
        {
            m_EmulatorWatches[i] = 0177777;
            m_wEmulatorWatchesCount--;
            if (m_wEmulatorWatchesCount > i)  // fill the hole
            {
                m_EmulatorWatches[i] = m_EmulatorWatches[m_wEmulatorWatchesCount];
                m_EmulatorWatches[m_wEmulatorWatchesCount] = 0177777;
            }
            return true;
        }
    }
    return false;
}
void Emulator_RemoveAllWatchpoints()
{
    for (int i = 0; i < MAX_WATCHPOINTCOUNT; i++)
        m_EmulatorWatches[i] = 0177777;
    m_wEmulatorWatchesCount = 0;
}

void Emulator_SetSpeed(uint16_t realspeed)
{
    uint16_t speedpercent = 100;
    switch (realspeed)
    {
    case 0: speedpercent = 200; break;
    case 1: speedpercent = 100; break;
    case 2: speedpercent = 200; break;
    case 0x7fff: speedpercent = 50; break;
    case 0x7ffe: speedpercent = 25; break;
    default: speedpercent = 100; break;
    }
    m_wEmulatorSoundSpeed = speedpercent;

    if (m_okEmulatorSound)
        SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
}

void Emulator_SetSound(bool soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
            g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(nullptr);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

bool CALLBACK Emulator_SerialIn_Callback(uint8_t* pByte)
{
    DWORD dwBytesRead;
    BOOL result = ::ReadFile(m_hEmulatorComPort, pByte, 1, &dwBytesRead, NULL);

    return result && (dwBytesRead == 1);
}

bool CALLBACK Emulator_SerialOut_Callback(uint8_t byte)
{
    DWORD dwBytesWritten;
    ::WriteFile(m_hEmulatorComPort, &byte, 1, &dwBytesWritten, NULL);

    return (dwBytesWritten == 1);
}

bool Emulator_SetSerial(bool serialOnOff, LPCTSTR serialPort)
{
    if (m_okEmulatorSerial != serialOnOff)
    {
        if (serialOnOff)
        {
            // Prepare port name
            TCHAR port[15];
            _sntprintf(port, sizeof(port) / sizeof(TCHAR) - 1, _T("\\\\.\\%s"), serialPort);

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
            g_pBoard->SetSerialCallbacks(nullptr, nullptr);  // Reset callbacks

            // Close port
            if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorSerial = serialOnOff;

    return true;
}

bool CALLBACK Emulator_ParallelOut_Callback(uint8_t byte)
{
    if (m_fpEmulatorParallelOut != nullptr)
    {
        ::fwrite(&byte, 1, 1, m_fpEmulatorParallelOut);
    }

    ////DEBUG
    //TCHAR buffer[32];
    //_sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("Printer: <%02x>\r\n"), byte);
    //ConsoleView_Print(buffer);

    return true;
}

void Emulator_SetParallel(bool parallelOnOff)
{
    if (m_okEmulatorParallel == parallelOnOff)
        return;

    if (!parallelOnOff)
    {
        g_pBoard->SetParallelOutCallback(nullptr);
        if (m_fpEmulatorParallelOut != nullptr)
            ::fclose(m_fpEmulatorParallelOut);
    }
    else
    {
        g_pBoard->SetParallelOutCallback(Emulator_ParallelOut_Callback);
        m_fpEmulatorParallelOut = ::_tfopen(_T("printer.log"), _T("wb"));
    }

    m_okEmulatorParallel = parallelOnOff;
}

bool Emulator_SystemFrame()
{
    g_pBoard->SetCPUBreakpoints(m_wEmulatorCPUBpsCount > 0 ? m_EmulatorCPUBps : nullptr);

    ScreenView_ScanKeyboard();
    ScreenView_ProcessKeyboard();

    if (!g_pBoard->SystemFrame())
        return false;

    // Calculate frames per second
    m_nFrameCount++;
    uint32_t dwCurrentTicks = GetTickCount();
    long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1200)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        double dSpeed = dFramesPerSecond / 25.0 * 100;
        TCHAR buffer[16];
        _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%03.f%%"), dSpeed);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        bool floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : nullptr);

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
        _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    // Auto-boot option processing: select "boot from disk" and press Enter
    if (Option_AutoBoot)
    {
        if (m_dwEmulatorUptime == 2 && m_nUptimeFrameCount == 16)
        {
            ScreenView_KeyEvent(68, true);  // Press "D"
            Option_AutoBoot = false;  // All done
        }
    }

    return true;
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
        uint8_t* pOld = g_pEmulatorRam;
        uint8_t* pChanged = g_pEmulatorChangedRam;
        uint16_t addr = 0;
        do
        {
            uint8_t newvalue = g_pBoard->GetRAMByte(addr);
            uint8_t oldvalue = *pOld;
            *pChanged = (newvalue != oldvalue) ? 255 : 0;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag for RAM word
//   addrtype - address mode - see ADDRTYPE_XXX constants
uint16_t Emulator_GetChangeRamStatus(int addrtype, uint16_t address)
{
    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        return *((uint16_t*)(g_pEmulatorChangedRam + address));
    case ADDRTYPE_HIRAM:
        return *((uint16_t*)(g_pEmulatorChangedRam + 65536 + address));
    default:
        return 0;
    }
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
    if (pImageBits == nullptr) return;

    const uint8_t* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != nullptr);

    // Render to bitmap
    PREPARE_SCREEN_CALLBACK callback = ScreenModeReference[screenMode].callback;
    callback(pVideoBuffer, ScreenView_Palette, pImageBits);
}

const uint32_t * Emulator_GetPalette()
{
    return ScreenView_Palette;
}

#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

void CALLBACK Emulator_PrepareScreenBW512x256(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y++)
    {
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + y * 512 / 4);
        uint32_t* pBits = static_cast<uint32_t*>(pImageBits) + (256 - 1 - y) * 512;
        for (int x = 0; x < 512 / 8; x++)
        {
            uint16_t src = *pVideo;

            for (int bit = 0; bit < 8; bit++)
            {
                int colorindex = (src & 0x80) >> 7 | (src & 0x8000) >> 14;
                uint32_t color = palette[colorindex];
                *pBits = color;
                pBits++;
                src = src << 1;
            }

            pVideo++;
        }
    }
}

void CALLBACK Emulator_PrepareScreenBW512x312(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits)
{
    uint32_t * pImageStart = static_cast<uint32_t*>(pImageBits) + 512 * 28;
    Emulator_PrepareScreenBW512x256(pVideoBuffer, palette, pImageStart);
}

void CALLBACK Emulator_PrepareScreenBW768x468(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y += 2)
    {
        const uint16_t* psrc1 = (uint16_t*)(pVideoBuffer + y * 512 / 4);
        const uint16_t* psrc2 = (uint16_t*)(pVideoBuffer + (y + 1) * 512 / 4);
        uint32_t* pdest1 = static_cast<uint32_t*>(pImageBits) + (426 - 1 - y / 2 * 3) * 768;
        uint32_t* pdest2 = pdest1 - 768;
        uint32_t* pdest3 = pdest2 - 768;
        for (int x = 0; x < 512 / 8; x++)
        {
            uint16_t src1 = *psrc1;
            uint16_t src2 = *psrc2;
            for (int bit = 0; bit < 4; bit++)
            {
                int colorindex1a = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                int colorindex1b = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                uint32_t c1a = palette[colorindex1a];
                uint32_t c1b = palette[colorindex1b];
                int colorindex2a = (src2 & 0x80) >> 7 | (src2 & 0x8000) >> 14;  src2 = src2 << 1;
                int colorindex2b = (src2 & 0x80) >> 7 | (src2 & 0x8000) >> 14;  src2 = src2 << 1;
                uint32_t c2a = palette[colorindex2a];
                uint32_t c2b = palette[colorindex2b];

                uint32_t c1ab = AVERAGERGB(c1a, c1b);
                uint32_t c2ab = AVERAGERGB(c2a, c2b);
                uint32_t c12a = AVERAGERGB(c1a, c2a);
                uint32_t c12b = AVERAGERGB(c1b, c2b);
                uint32_t c12ab = AVERAGERGB(c1ab, c2ab);

                (*pdest1++) = c1a;   (*pdest1++) = c1ab;  (*pdest1++) = c1b;
                (*pdest2++) = c12a;  (*pdest2++) = c12ab; (*pdest2++) = c12b;
                (*pdest3++) = c2a;   (*pdest3++) = c2ab;  (*pdest3++) = c2b;
            }
            psrc1++;
            psrc2++;
        }
    }
}

void CALLBACK Emulator_PrepareScreenBW896x624(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y++)
    {
        const uint16_t* psrc1 = (uint16_t*)(pVideoBuffer + y * 512 / 4);
        uint32_t* pdest1 = static_cast<uint32_t*>(pImageBits) + (568 - 1 - y * 2) * 896;
        uint32_t* pdest2 = pdest1 - 896;
        for (int x = 0; x < 512 / 8; x++)
        {
            uint16_t src1 = *psrc1;
            for (int bit = 0; bit < 2; bit++)
            {
                int colorindex1 = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                int colorindex2 = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                int colorindex3 = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                int colorindex4 = (src1 & 0x80) >> 7 | (src1 & 0x8000) >> 14;  src1 = src1 << 1;
                uint32_t c1 = palette[colorindex1];
                uint32_t c2 = palette[colorindex2];
                uint32_t c3 = palette[colorindex3];
                uint32_t c4 = palette[colorindex4];
                *(pdest1++) = *(pdest2++) = c1;
                *(pdest1++) = *(pdest2++) = AVERAGERGB(c1, c2);
                *(pdest1++) = *(pdest2++) = c2;
                *(pdest1++) = *(pdest2++) = AVERAGERGB(c2, c3);
                *(pdest1++) = *(pdest2++) = c3;
                *(pdest1++) = *(pdest2++) = AVERAGERGB(c3, c4);
                *(pdest1++) = *(pdest2++) = c4;
            }
            psrc1++;
        }
    }
}

void CALLBACK Emulator_PrepareScreenBW1024x624(const uint8_t* pVideoBuffer, const uint32_t* palette, void* pImageBits)
{
    for (int y = 0; y < 256; y++)
    {
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + y * 512 / 4);
        uint32_t* pBits1 = static_cast<uint32_t*>(pImageBits) + (568 - 1 - y * 2) * 1024;
        uint32_t* pBits2 = pBits1 - 1024;
        for (int x = 0; x < 512 / 8; x++)
        {
            uint16_t src = *pVideo;

            for (int bit = 0; bit < 8; bit++)
            {
                int colorindex = (src & 0x80) >> 7 | (src & 0x8000) >> 14;
                uint32_t color = palette[colorindex];

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

bool Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("w+b"), _SH_DENYWR);
    if (fpFile == nullptr)
        return false;

    // Allocate memory
    uint8_t* pImage = static_cast<uint8_t*>(::calloc(NEMIGAIMAGE_SIZE, 1));
    if (pImage == nullptr)
    {
        ::fclose(fpFile);
        return false;
    }
    memset(pImage, 0, NEMIGAIMAGE_SIZE);
    // Prepare header
    uint32_t* pHeader = (uint32_t*) pImage;
    *pHeader++ = NEMIGAIMAGE_HEADER1;
    *pHeader++ = NEMIGAIMAGE_HEADER2;
    *pHeader++ = NEMIGAIMAGE_VERSION;
    *pHeader++ = NEMIGAIMAGE_SIZE;
    // Store emulator state to the image
    g_pBoard->SaveToImage(pImage);
    *(uint32_t*)(pImage + 16) = m_dwEmulatorUptime;

    // Save image to the file
    size_t dwBytesWritten = ::fwrite(pImage, 1, NEMIGAIMAGE_SIZE, fpFile);
    ::free(pImage);
    ::fclose(fpFile);
    if (dwBytesWritten != NEMIGAIMAGE_SIZE)
        return false;

    return true;
}

bool Emulator_LoadImage(LPCTSTR sFilePath)
{
    Emulator_Stop();
    Emulator_Reset();

    // Open file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR);
    if (fpFile == nullptr)
        return false;

    // Read header
    uint32_t bufHeader[NEMIGAIMAGE_HEADER_SIZE / sizeof(uint32_t)];
    uint32_t dwBytesRead = ::fread(bufHeader, 1, NEMIGAIMAGE_HEADER_SIZE, fpFile);
    if (dwBytesRead != NEMIGAIMAGE_HEADER_SIZE)
    {
        ::fclose(fpFile);
        return false;
    }

    //TODO: Check version and size

    // Allocate memory
    uint8_t* pImage = static_cast<uint8_t*>(::calloc(NEMIGAIMAGE_SIZE, 1));
    if (pImage == nullptr)
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

    m_dwEmulatorUptime = *(uint32_t*)(pImage + 16);
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Free memory, close file
    ::free(pImage);
    ::fclose(fpFile);

    g_okEmulatorRunning = false;

    MainWindow_UpdateAllViews();

    return true;
}


//////////////////////////////////////////////////////////////////////

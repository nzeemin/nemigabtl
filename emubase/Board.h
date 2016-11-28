/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.h
//

#pragma once

#include "Defines.h"

class CProcessor;


//////////////////////////////////////////////////////////////////////


// TranslateAddress result code
#define ADDRTYPE_RAM     0  // RAM
#define ADDRTYPE_HIRAM   1  // RAM
#define ADDRTYPE_ROM    16  // ROM
#define ADDRTYPE_IO     32  // I/O port
#define ADDRTYPE_HALT   64  // Memory 177600-177777 gives HALT
#define ADDRTYPE_DENY  128  // Access denied
#define ADDRTYPE_MASK  255  // RAM type mask
#define ADDRTYPE_RAMMASK 7  // RAM chunk number mask

// Emulator image constants
#define NEMIGAIMAGE_HEADER_SIZE 32
#define NEMIGAIMAGE_SIZE 147456
#define NEMIGAIMAGE_HEADER1 0x494D454E  // "NEMI"
#define NEMIGAIMAGE_HEADER2 0x21214147  // "GA!!"
#define NEMIGAIMAGE_VERSION 0x00010000  // 1.0

//////////////////////////////////////////////////////////////////////

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);

// Parallel port output callback
// Input:
//   byte       An output byte
// Output:
//   result     TRUE means OK, FALSE means we have an error
typedef BOOL (CALLBACK* PARALLELOUTCALLBACK)(BYTE byte);

class CFloppyController;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // NEMIGA computer
{
private:  // Devices
    CProcessor*     m_pCPU;  // CPU device
    CFloppyController*  m_pFloppyCtl;  // FDD control
    BOOL        m_okTimer50OnOff;
private:  // Memory
    WORD        m_Configuration;  // See BK_COPT_Xxx flag constants
    BYTE*       m_pRAM;  // RAM, 8 * 16 = 128 KB
    BYTE*       m_pROM;  // ROM, 4 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    WORD        GetRAMWord(WORD offset);
    WORD        GetHIRAMWord(WORD offset);
    BYTE        GetRAMByte(WORD offset);
    BYTE        GetHIRAMByte(WORD offset);
    void        SetRAMWord(WORD offset, WORD word);
    void        SetHIRAMWord(WORD offset, WORD word);
    void        SetRAMByte(WORD offset, BYTE byte);
    void        SetHIRAMByte(WORD offset, BYTE byte);
    WORD        GetROMWord(WORD offset);
    BYTE        GetROMByte(WORD offset);
public:  // Debug
    void        DebugTicks();  // One Debug CPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(WORD bp) { m_CPUbp = bp; } // Set CPU breakpoint
    bool        GetTrace() const { return m_okTraceCPU; }
    void        SetTrace(bool okTraceCPU) { m_okTraceCPU = okTraceCPU; }
public:  // System control
    void        SetConfiguration(WORD conf);
    void        Reset();  // Reset computer
    void        LoadROM(const BYTE* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(int startbank, const BYTE* pBuffer, int length);  // Load data into the RAM
    void        SetTimer50OnOff(BOOL okOnOff) { m_okTimer50OnOff = okOnOff; }
    BOOL        IsTimer50OnOff() const { return m_okTimer50OnOff; }
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void		TimerTick();		// Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
    void        ResetDevices();     // INIT signal
    void        ResetHALT();//DEBUG
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    BOOL        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(BYTE scancode, BOOL okPressed);  // Key pressed or released
    //WORD        GetPrinterOutPort() const { return m_Port177714out; }
public:  // Floppy
    BOOL        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    BOOL        IsFloppyImageAttached(int slot) const;
    BOOL        IsFloppyReadOnly(int slot) const;
    BOOL        IsFloppyEngineOn() const;
public:  // Callbacks
    void		SetSoundGenCallback(SOUNDGENCALLBACK callback);
    void        SetParallelOutCallback(PARALLELOUTCALLBACK outcallback);
public:  // Memory
    // Read command for execution
    WORD GetWordExec(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, TRUE); }
    // Read word from memory
    WORD GetWord(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, FALSE); }
    // Read word
    WORD GetWord(WORD address, BOOL okHaltMode, BOOL okExec);
    // Write word
    void SetWord(WORD address, BOOL okHaltMode, WORD word);
    // Read byte
    BYTE GetByte(WORD address, BOOL okHaltMode);
    // Write byte
    void SetByte(WORD address, BOOL okHaltMode, BYTE byte);
    // Read word from memory for debugger
    WORD GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, int* pValid);
    // Read word from port for debugger
    WORD GetPortView(WORD address);
    // Get video buffer address
    const BYTE* GetVideoBuffer();
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset);
private:  // Access to I/O ports
    WORD        GetPortWord(WORD address);
    void        SetPortWord(WORD address, WORD word);
    BYTE        GetPortByte(WORD address);
    void        SetPortByte(WORD address, BYTE byte);
public:  // Saving/loading emulator status
    void        SaveToImage(BYTE* pImage);
    void        LoadFromImage(const BYTE* pImage);
private:  // Ports: implementation
    WORD        m_Port170006;       // Регистр данных клавиатуры (байт 170006) и регистр фиксации HALT-запросов (байт 170007)
    WORD        m_Port170006wr;     // Регистр 170006 на запись
    WORD        m_Port170020;       // Регистр состояния таймера
    WORD        m_Port170022;       // Регистр частоты
    WORD        m_Port170024;       // Регистр длительности
    WORD        m_Port170030;       // Регистр октавы и громкости
    WORD        m_Port177572;       // Регистр адреса косвенной адресации
    WORD        m_Port177574;       // Регистр ??
    WORD        m_Port177514;       // Регистр состояния ИРПР
    WORD        m_Port177516;       // Регистр данных ИРПР
private:
    WORD        m_CPUbp;  // CPU breakpoint address
    bool        m_okTraceCPU;
    WORD        m_Timer1div;        // Timer 1 subcounter, based on octave value
    WORD        m_Timer1;           // Timer 1 counter, initial value copied from m_Port170022
    WORD        m_Timer2;           // Timer 2 counter
    bool        m_okSoundOnOff;
private:
    SOUNDGENCALLBACK m_SoundGenCallback;
    PARALLELOUTCALLBACK m_ParallelOutCallback;

    void        DoSound();
};


//////////////////////////////////////////////////////////////////////

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

// Serial port callback for receiving
// Output:
//   pbyte      Byte received
//   result     true means we have a new byte, false means not ready yet
typedef bool (CALLBACK* SERIALINCALLBACK)(uint8_t* pbyte);

// Serial port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     true means we translated the byte successfully, false means we have an error
typedef bool (CALLBACK* SERIALOUTCALLBACK)(uint8_t byte);

// Parallel port output callback
// Input:
//   byte       An output byte
// Output:
//   result     TRUE means OK, FALSE means we have an error
typedef bool (CALLBACK* PARALLELOUTCALLBACK)(uint8_t byte);

class CFloppyController;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // NEMIGA computer
{
private:  // Devices
    CProcessor*     m_pCPU;  // CPU device
    CFloppyController*  m_pFloppyCtl;  // FDD control
    bool        m_okTimer50OnOff;
private:  // Memory
    uint16_t    m_Configuration;  // See BK_COPT_Xxx flag constants
    uint8_t*    m_pRAM;  // RAM, 8 * 16 = 128 KB
    uint8_t*    m_pROM;  // ROM, 4 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    uint16_t    GetRAMWord(uint16_t offset);
    uint16_t    GetHIRAMWord(uint16_t offset);
    uint8_t     GetRAMByte(uint16_t offset);
    uint8_t     GetHIRAMByte(uint16_t offset);
    void        SetRAMWord(uint16_t offset, uint16_t word);
    void        SetHIRAMWord(uint16_t offset, uint16_t word);
    void        SetRAMByte(uint16_t offset, uint8_t byte);
    void        SetHIRAMByte(uint16_t offset, uint8_t byte);
    uint16_t    GetROMWord(uint16_t offset);
    uint8_t     GetROMByte(uint16_t offset);
public:  // Debug
    void        DebugTicks();  // One Debug CPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(uint16_t bp) { m_CPUbp = bp; } // Set CPU breakpoint
    bool        GetTrace() const { return m_okTraceCPU; }
    void        SetTrace(bool okTraceCPU) { m_okTraceCPU = okTraceCPU; }
public:  // System control
    void        SetConfiguration(uint16_t conf);
    void        Reset();  // Reset computer
    void        LoadROM(const uint8_t* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(int startbank, const uint8_t* pBuffer, int length);  // Load data into the RAM
    void        SetTimer50OnOff(bool okOnOff) { m_okTimer50OnOff = okOnOff; }
    bool        IsTimer50OnOff() const { return m_okTimer50OnOff; }
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void		TimerTick();		// Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
    void        ResetDevices();     // INIT signal
    void        ResetHALT();//DEBUG
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    bool        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(uint8_t scancode, bool okPressed);  // Key pressed or released
    //uint16_t        GetPrinterOutPort() const { return m_Port177714out; }
public:  // Floppy
    bool        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    bool        IsFloppyImageAttached(int slot) const;
    bool        IsFloppyReadOnly(int slot) const;
    bool        IsFloppyEngineOn() const;
public:  // Callbacks
    void		SetSoundGenCallback(SOUNDGENCALLBACK callback);
    void        SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback);
    void        SetParallelOutCallback(PARALLELOUTCALLBACK outcallback);
public:  // Memory
    // Read command for execution
    uint16_t GetWordExec(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, TRUE); }
    // Read word from memory
    uint16_t GetWord(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, FALSE); }
    // Read word
    uint16_t GetWord(uint16_t address, bool okHaltMode, bool okExec);
    // Write word
    void SetWord(uint16_t address, bool okHaltMode, uint16_t word);
    // Read byte
    uint8_t GetByte(uint16_t address, bool okHaltMode);
    // Write byte
    void SetByte(uint16_t address, bool okHaltMode, uint8_t byte);
    // Read word from memory for debugger
    uint16_t GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pValid);
    // Read word from port for debugger
    uint16_t GetPortView(uint16_t address);
    // Get video buffer address
    const uint8_t* GetVideoBuffer();
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset);
private:  // Access to I/O ports
    uint16_t    GetPortWord(uint16_t address);
    void        SetPortWord(uint16_t address, uint16_t word);
    uint8_t     GetPortByte(uint16_t address);
    void        SetPortByte(uint16_t address, uint8_t byte);
public:  // Saving/loading emulator status
    void        SaveToImage(uint8_t* pImage);
    void        LoadFromImage(const uint8_t* pImage);
private:  // Ports: implementation
    uint16_t    m_Port170006;       // Регистр данных клавиатуры (байт 170006) и регистр фиксации HALT-запросов (байт 170007)
    uint16_t    m_Port170006wr;     // Регистр 170006 на запись
    uint16_t    m_Port170020;       // Регистр состояния таймера
    uint16_t    m_Port170022;       // Регистр частоты
    uint16_t    m_Port170024;       // Регистр длительности
    uint16_t    m_Port170030;       // Регистр октавы и громкости
    uint16_t    m_Port176500;       // Регистр состояния приёмника последовательного порта (отсутствует на реальной Немиге)
    uint16_t    m_Port176502;       // Регистр данных приёмника последовательного порта (отсутствует на реальной Немиге)
    uint16_t    m_Port176504;       // Регистр состояния передатчика последовательного порта (отсутствует на реальной Немиге)
    uint16_t    m_Port176506;       // Регистр данных передатчика последовательного порта (отсутствует на реальной Немиге)
    uint16_t    m_Port177572;       // Регистр адреса косвенной адресации
    uint16_t    m_Port177574;       // Регистр ??
    uint16_t    m_Port177514;       // Регистр состояния ИРПР
    uint16_t    m_Port177516;       // Регистр данных ИРПР
private:
    uint16_t    m_CPUbp;  // CPU breakpoint address
    bool        m_okTraceCPU;
    uint16_t    m_Timer1div;        // Timer 1 subcounter, based on octave value
    uint16_t    m_Timer1;           // Timer 1 counter, initial value copied from m_Port170022
    uint16_t    m_Timer2;           // Timer 2 counter
    bool        m_okSoundOnOff;
private:
    SOUNDGENCALLBACK m_SoundGenCallback;
    SERIALINCALLBACK    m_SerialInCallback;
    SERIALOUTCALLBACK   m_SerialOutCallback;
    PARALLELOUTCALLBACK m_ParallelOutCallback;

    void        DoSound();
};


//////////////////////////////////////////////////////////////////////

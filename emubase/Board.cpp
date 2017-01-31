/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.cpp
//

#include "stdafx.h"
#include "Emubase.h"

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, DWORD dwTrace);


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);
    m_pFloppyCtl = NULL;

    m_dwTrace = TRACE_NONE;
    m_SoundGenCallback = NULL;
    m_SerialInCallback = NULL;
    m_SerialOutCallback = NULL;
    m_ParallelOutCallback = NULL;
    m_okTimer50OnOff = false;
    m_okSoundOnOff = false;
    m_Timer1 = m_Timer1div = m_Timer2 = 0;

    // Allocate memory for RAM and ROM
    m_pRAM = (uint8_t*) ::malloc(128 * 1024);  ::memset(m_pRAM, 0, 128 * 1024);
    m_pROM = (uint8_t*) ::malloc(4 * 1024);  ::memset(m_pROM, 0, 4 * 1024);

    SetConfiguration(0);  // Default configuration

    Reset();
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;
    if (m_pFloppyCtl != NULL)
        delete m_pFloppyCtl;

    // Free memory
    ::free(m_pRAM);
    ::free(m_pROM);
}

void CMotherboard::SetConfiguration(uint16_t conf)
{
    m_Configuration = conf;

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 128 * 1024);
    ::memset(m_pROM, 0, 4 * 1024);

    //// Pre-fill RAM with "uninitialized" values
    //uint16_t * pMemory = (uint16_t *) m_pRAM;
    //uint16_t val = 0;
    //uint8_t flag = 0;
    //for (uint32_t i = 0; i < 128 * 1024; i += 2, flag--)
    //{
    //    *pMemory = val;  pMemory++;
    //    if (flag == 192)
    //        flag = 0;
    //    else
    //        val = ~val;
    //}

    if (m_pFloppyCtl == NULL /*&& (conf & BK_COPT_FDD) != 0*/)
    {
        m_pFloppyCtl = new CFloppyController();
        m_pFloppyCtl->SetTrace(m_dwTrace & TRACE_FLOPPY);
    }
    //if (m_pFloppyCtl != NULL /*&& (conf & BK_COPT_FDD) == 0*/)
    //{
    //    delete m_pFloppyCtl;  m_pFloppyCtl = NULL;
    //}
}

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->SetTrace(dwTrace & TRACE_FLOPPY);
}

void CMotherboard::Reset ()
{
    m_pCPU->Stop();

    // Reset ports
    m_Port170006 = m_Port170006wr = 0;
    m_Port177572 = 0;
    m_Port177574 = 0;
    m_Port177514 = 0377;
    m_Port170020 = m_Port170022 = m_Port170024 = m_Port170030 = 0;
    m_okSoundOnOff = false;
    m_Timer1 = m_Timer1div = m_Timer2 = 0;

    ResetDevices();

    m_pCPU->Start();
}

// Load 4 KB ROM image from the buffer
void CMotherboard::LoadROM(const uint8_t* pBuffer)
{
    ::memcpy(m_pROM, pBuffer, 4096);
}

void CMotherboard::LoadRAM(int startbank, const uint8_t* pBuffer, int length)
{
    ASSERT(pBuffer != NULL);
    ASSERT(startbank >= 0 && startbank < 15);
    int address = 8192 * startbank;
    ASSERT(address + length <= 128 * 1024);
    ::memcpy(m_pRAM + address, pBuffer, length);
}


// Floppy ////////////////////////////////////////////////////////////

bool CMotherboard::IsFloppyImageAttached(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->IsAttached(slot);
}

bool CMotherboard::IsFloppyReadOnly(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->IsReadOnly(slot);
}

bool CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return;
    m_pFloppyCtl->DetachImage(slot);
}

bool CMotherboard::IsFloppyEngineOn() const
{
    return m_pFloppyCtl->IsEngineOn();
}


// Работа с памятью //////////////////////////////////////////////////

uint16_t CMotherboard::GetRAMWord(uint16_t offset)
{
    return *((uint16_t*)(m_pRAM + offset));
}
uint16_t CMotherboard::GetHIRAMWord(uint16_t offset)
{
    return *((uint16_t*)(m_pRAM + 0x10000 + offset));
}
uint8_t CMotherboard::GetRAMByte(uint16_t offset)
{
    return m_pRAM[offset];
}
uint8_t CMotherboard::GetHIRAMByte(uint16_t offset)
{
    uint32_t dwOffset = (uint32_t)0x10000 + (uint32_t)offset;
    return m_pRAM[dwOffset];
}
void CMotherboard::SetRAMWord(uint16_t offset, uint16_t word)
{
    *((uint16_t*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetHIRAMWord(uint16_t offset, uint16_t word)
{
    uint32_t dwOffset = (uint32_t)0x10000 + (uint32_t)offset;
    *((uint16_t*)(m_pRAM + dwOffset)) = word;
}
void CMotherboard::SetRAMByte(uint16_t offset, uint8_t byte)
{
    m_pRAM[offset] = byte;
}
void CMotherboard::SetHIRAMByte(uint16_t offset, uint8_t byte)
{
    uint32_t dwOffset = (uint32_t)0x10000 + (uint32_t)offset;
    m_pRAM[dwOffset] = byte;
}

uint16_t CMotherboard::GetROMWord(uint16_t offset)
{
    ASSERT(offset < 1024 * 64);
    return *((uint16_t*)(m_pROM + offset));
}
uint8_t CMotherboard::GetROMByte(uint16_t offset)
{
    ASSERT(offset < 1024 * 64);
    return m_pROM[offset];
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetDevices()
{
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Reset();

    //m_pCPU->DeassertHALT();//DEBUG
    m_Port170006 |= 0100000;
    m_Port170006wr = 0;
    m_Port177574 = 0;
    m_pCPU->FireHALT();

    // Reset ports
    //TODO
}

void CMotherboard::ResetHALT()
{
    m_Port170006 = 0;
    m_Port170006wr = 0;
}

void CMotherboard::Tick50()  // 50 Hz timer
{
    if (m_okTimer50OnOff)
    {
        m_pCPU->TickIRQ2();
    }

    if (m_Timer2 == 0)
    {
        // Если разряды 4 и 5 равны 1 1, то запуск второго счётчика происходит по тактовому входу С2
        //if ((m_Port170020 & 060) == 060)
        if (m_Port170024 != 0 && (m_Port170020 & 01000) == 01000)
        {
            m_Timer2 = m_Port170024;
//#if !defined(PRODUCT)
//            DebugLog(_T("Tick50 Timer2 START\r\n"));
//#endif
        }
    }
    else //if (m_Timer2 > 0)
    {
        m_Timer2--;
        if (m_Timer2 == 0)
        {
            m_pCPU->FireHALT();
            m_Port170020 &= ~01000;  // Снимаем "Фикс прерывания 2"
            m_Port170006 |= 010000;  // Сигнал Н3
            m_okSoundOnOff = false;  // Судя по схеме, звук выключается по сигналу ЗПР2
            m_Port170024 = 0;
#if !defined(PRODUCT)
            if (m_dwTrace & TRACE_TIMER) DebugLog(_T("Tick50 Timer2 END\r\n"));
#endif
        }
    }
}

void CMotherboard::TimerTick() // Timer Tick, 8 MHz
{
    if (m_Timer1 == 0 || m_Timer1div == 0)
    {
        // Если разряды 2 и 3 равны 1 1, то запуск первого счётчика происходит по тактовому входу С1
        if ((m_Port170020 & 014) == 014)
        {
            uint16_t octave = m_Port170030 & 7;  // Октава 1..7
            m_Timer1div = (1 << octave);
        }
        return;
    }

    m_Timer1div--;
    if (m_Timer1div == 0)
    {
        uint16_t octave = m_Port170030 & 7;  // Октава 1..7
        m_Timer1div = (1 << octave);

        m_Timer1--;
        if (m_Timer1 == 0)
        {
            m_Timer1 = m_Port170022;
        }
    }
}

void CMotherboard::DebugTicks()
{
    m_pCPU->SetInternalTick(0);
    m_pCPU->Execute();
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Periodic();
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

/*
Каждый фрейм равен 1/25 секунды = 40 мс
Фрейм делим на 20000 тиков, 1 тик = 2 мкс
В каждый фрейм происходит:
* 320000 тиков таймер 1 -- 16 раз за тик -- 8 МГц
* 320000 тиков ЦП       -- 16 раз за тик
*      2 тика IRQ2 и таймер 2 -- 50 Гц, в 0-й и 10000-й тик фрейма
*    625 тиков FDD -- каждый 32-й тик (300 RPM = 5 оборотов в секунду)
*/
bool CMotherboard::SystemFrame()
{
    const int frameProcTicks = 16;
    const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);
    const int floppyTicks = 32;
    const int serialOutTicks = 20000 / (9600 / 25);
    int serialTxCount = 0;

    for (int frameticks = 0; frameticks < 20000; frameticks++)
    {
        for (int procticks = 0; procticks < frameProcTicks; procticks++)  // CPU ticks
        {
#if !defined(PRODUCT)
            if ((m_dwTrace & TRACE_CPU) && m_pCPU->GetInternalTick() == 0)
                TraceInstruction(m_pCPU, this, m_pCPU->GetPC(), m_dwTrace);
#endif
            m_pCPU->Execute();
            if (m_pCPU->GetPC() == m_CPUbp)
                return false;  // Breakpoint

            // Timer 1 ticks
            TimerTick();
        }

        if (frameticks == 0 || frameticks == 10000)
        {
            Tick50();  // 1/50 timer event
        }

        if (frameticks % floppyTicks == 0)  // FDD tick, every 64 uS
        {
            if (m_pFloppyCtl != NULL)
                m_pFloppyCtl->Periodic();
        }

        if (frameticks % audioticks == 0)  // AUDIO tick
            DoSound();

        if (m_SerialInCallback != NULL && frameticks % 52 == 0)
        {
            uint8_t b;
            if (m_SerialInCallback(&b))
            {
                if (m_Port176500 & 0200)  // Ready?
                    m_Port176500 |= 010000;  // Set Overflow flag
                else
                {
                    m_Port176502 = (uint16_t)b;
                    m_Port176500 |= 0200;  // Set Ready flag
                    if (m_Port176500 & 0100)  // Interrupt?
                        m_pCPU->InterruptVIRQ(7, 0300);
                }
            }
        }
        if (m_SerialOutCallback != NULL && frameticks % 52 == 0)
        {
            if (serialTxCount > 0)
            {
                serialTxCount--;
                if (serialTxCount == 0)  // Translation countdown finished - the byte translated
                {
                    (*m_SerialOutCallback)((uint8_t)(m_Port176506 & 0xff));
                    m_Port176504 |= 0200;  // Set Ready flag
                    if (m_Port176504 & 0100)  // Interrupt?
                        m_pCPU->InterruptVIRQ(8, 0304);
                }
            }
            else if ((m_Port176504 & 0200) == 0)  // Ready is 0?
            {
                serialTxCount = 8;  // Start translation countdown
            }
        }

        if (m_ParallelOutCallback != NULL)
        {
            //if ((m_Port177514 & 0240) == 040)
            //{
            //    m_Port177514 |= 0200;  // Set TR flag
            //    // Now printer waits for a next byte
            //    if (m_Port177514 & 0100)
            //        m_pCPU->InterruptVIRQ(5, 0200);
            //}
            //else if ((m_Port177514 & 0240) == 0)
            //{
            //    // Byte is ready, print it
            //    (*m_ParallelOutCallback)((uint8_t)(m_Port177516 & 0xff));
            //    m_Port177514 |= 040;  // Set Printer Acknowledge
            //}
        }
    }

    return true;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(uint8_t scancode, bool okPressed)
{
    if (okPressed)  // Key released
    {
        m_Port170006 |= 02000;
        m_Port170006 |= scancode;
        //if (m_Port177560 | 0100)
        m_pCPU->FireHALT();
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_KEYBOARD)
        {
            if (scancode >= ' ' && scancode <= 127)
                DebugLogFormat(_T("Keyboard '%c'\r\n"), scancode);
            else
                DebugLogFormat(_T("Keyboard 0x%02x\r\n"), scancode);
        }
#endif
        return;
    }
}


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
uint16_t CMotherboard::GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pAddrType)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    *pAddrType = addrtype;

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(offset & 0177776);
    case ADDRTYPE_HIRAM:
        return GetHIRAMWord(offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        return 0;  // I/O port, not memory
    case ADDRTYPE_TERM:
        return 0;
    case ADDRTYPE_DENY:
        return 0;  // This memory is inaccessible for reading
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint16_t CMotherboard::GetWord(uint16_t address, bool okHaltMode, bool okExec)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(offset & 0177776);
    case ADDRTYPE_HIRAM:
        return GetHIRAMWord(offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortWord(address);
    case ADDRTYPE_TERM:
        if (address == 0177560)
            return GetRAMWord(address);
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
            return 0200; //GetRAMWord(offset & 0177776);
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
#if !defined(PRODUCT)
        //if (m_dwTrace & 04000) DebugLogFormat(_T("GetWord %06o\r\n"), address);
#endif
        return GetRAMWord(offset & 0177776);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint8_t CMotherboard::GetByte(uint16_t address, bool okHaltMode)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMByte(offset);
    case ADDRTYPE_HIRAM:
        return GetHIRAMByte(offset);
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortByte(address);
    case ADDRTYPE_TERM:
        if (address == 0177560 || address == 0177561)
            return GetRAMByte(address);
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
            return 0200; //GetRAMByte(offset);
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
#if !defined(PRODUCT)
        //if (m_dwTrace & 04000) DebugLogFormat(_T("GetByte 06o\r\n"), address);
#endif
        return GetRAMByte(offset);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(uint16_t address, bool okHaltMode, uint16_t word)
{
    uint16_t offset;

    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_HIRAM:
        SetHIRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    case ADDRTYPE_TERM:
        if (address == 0177560)
        {
            SetRAMWord(address, word);
            return;
        }
        if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
        {
            SetRAMWord(offset & 0177776, word);
            return;
        }
        else if (address == 0177566)
        {
            m_Port170006 |= 040000;
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("177566 TERMOUT %04x\r\n"), word);
//#endif
        }
        m_pCPU->FireHALT();
//#if !defined(PRODUCT)
//        DebugLogFormat(_T("SetWord 06o\r\n"), address);
//#endif
        SetRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(uint16_t address, bool okHaltMode, uint8_t byte)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMByte(offset, byte);
        return;
    case ADDRTYPE_HIRAM:
        SetHIRAMByte(offset, byte);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_TERM:
        if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
        {
            SetRAMByte(offset, byte);
            return;
        }
        else if (address == 0177566)
        {
            m_Port170006 |= 040000;
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("177566 TERMOUT %02x\r\n"), byte);
//#endif
        }
        m_pCPU->FireHALT();
        SetRAMByte(offset, byte);
//#if !defined(PRODUCT)
//        DebugLogFormat(_T("SetByte 06o\r\n"), address);
//#endif
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

// Calculates video buffer start address, for screen drawing procedure
const uint8_t* CMotherboard::GetVideoBuffer()
{
    return (m_pRAM + 0140000 + 0140000);
}

int CMotherboard::TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset)
{
    if (address < 0160000)  // 000000-157777 -- RAM
    {
        // ROM 4.05 expect that screen projected to memory, controlled by port 177574 bit 0
        if ((m_Port177574 & 1) != 0 && address <= 077777)
        {
            *pOffset = address + 0100000;
            return ADDRTYPE_HIRAM;
        }

        *pOffset = address;
        return ADDRTYPE_RAM;
    }

    if (address < 0170000)  // 160000-167777 -- ROM
    {
        *pOffset = address - 0160000;
        return ADDRTYPE_ROM;
    }

    if (address < 0177600)  // 170000-177577 -- Ports
    {
        if (address >= 0177560 && address <= 0177561)
        {
            *pOffset = address;
            return ADDRTYPE_RAM;
        }
        if (address >= 0177562 && address <= 0177567)
        {
            *pOffset = address;
            return okHaltMode ? ADDRTYPE_RAM : ADDRTYPE_TERM;
        }

        if ((address >= 0170000 && address <= 0170015) ||
            (address >= 0170020 && address <= 0170033) ||
            (address >= 0176500 && address <= 0176507) && m_SerialInCallback != NULL && m_SerialOutCallback != NULL ||
            (address >= 0177100 && address <= 0177107) ||
            (address >= 0177514 && address <= 0177517) ||
            (address >= 0177570 && address <= 0177575))  // Ports
        {
            *pOffset = address;
            return ADDRTYPE_IO;
        }

        *pOffset = address;
        return ADDRTYPE_DENY;
    }

    // 177600-177777

    //if (okHaltMode && address >= 0177600 && address <= 0177777)  // HALT-mode RAM
    {
        *pOffset = address;
        return ADDRTYPE_RAM;
    }
}

uint8_t CMotherboard::GetPortByte(uint16_t address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (uint8_t) GetPortWord(address);
}

uint16_t CMotherboard::GetPortWord(uint16_t address)
{
    switch (address)
    {
    case 0170000:
    case 0170002:
    case 0170004:
        return 0;  //STUB

    case 0170006:
        return m_Port170006;

    case 0170010:  // RgSt -- Network
    case 0170012:  // RgL -- Network
        return 0xffff;  //STUB

    case 0170014:
        return 0;  //STUB

    case 0170020:  // RgStSnd -- Sound Status 10-bit register
        return m_Port170020 & 01777;
    case 0170022:  // RgF -- Sound Frequency 8 MHz
        return m_Port170022;
    case 0170024:  // RgLength -- Sound Length 50 Hz
        return m_Port170024;
    case 0170026:  // RgOn -- Sound On
#if !defined(PRODUCT)
        //DebugLogFormat(_T("%06o Sound ON\r\n"), m_pCPU->GetPC());
#endif
        m_okSoundOnOff = true;
        return 0;  //STUB
    case 0170030:  // RgOct
        return m_Port170030;
    case 0170032:  // RgOff -- Sound Off
#if !defined(PRODUCT)
        //DebugLogFormat(_T("%06o Sound OFF\r\n"), m_pCPU->GetPC());
#endif
        m_okSoundOnOff = !m_okSoundOnOff;
        return 0;  //STUB

        // Последовательный порт (отсутствует на реальной Немиге)
    case 0176500:
    case 0176501:
        return m_Port176500;
    case 0176502:
    case 0176503:
        return m_Port176502;
    case 0176504:
    case 0176505:
        return m_Port176504;
    case 0176506:
    case 0176507:
        return m_Port176506;

    case 0177100:  // RgStat -- Floppy status
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetState();
    case 0177102:  // RgData -- Floppy data
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetData();
    case 0177104:  // RgCntrl -- Floppy command
        return 0;  //STUB
    case 0177106:  // RgTime -- Floppy timer
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetTimer();

    case 0177514:
        return m_Port177514;
    case 0177516:
        return 0;

        //case 0177560:
        //    return m_Port177560;
        //case 0177562:
        //    m_Port177560 &= ~0200;
        //    return m_Port177562;
        //case 0177564:
        //case 0177566:
    case 0177574:
        return m_Port177574;
        //case 0177576:
        //    return 0xffff;  //STUB

    case 0177572:  // Регистр адреса косвенной адресации
        return m_Port177572;
    case 0177570:  // Регистр данных косвенного доступа
        return *(uint16_t*)(m_pRAM + m_Port177572 + m_Port177572);

    default:
        m_pCPU->MemoryError();
        return 0;
    }

    return 0;
}

// Read word from port for debugger
uint16_t CMotherboard::GetPortView(uint16_t address)
{
    switch (address)
    {
    case 0170000:
    case 0170002:
    case 0170004:
        return 0;  //STUB

    case 0170006:
        return m_Port170006;
    case 0170010:  // Network
    case 0170012:
        return 0;  //STUB

    case 0170014:
        return 0;  //STUB

    case 0170020:  // Timer
        return m_Port170020 & 01777;
    case 0170022:
        return m_Port170022;
    case 0170024:
        return m_Port170024;
    case 0170026:
        return 0;  //STUB
    case 0170030:
        return m_Port170030;
    case 0170032:
        return 0;  //STUB

    case 0177100:  // Floppy status
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetStateView();
    case 0177102:  // Floppy data
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetDataView();
    case 0177104:  // Floppy command
        return 0;  //STUB
    case 0177106:  // Floppy timer
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetTimer();

    case 0177514:
        return m_Port177514;

    case 0177560:
    case 0177562:
        return 0;  //STUB
    case 0177564:
        return GetRAMWord(0177564);
    case 0177566:
    case 0177570:
        return 0;  //STUB
    case 0177574:
        return m_Port177574;
    case 0177576:
        return 0;  //STUB

    case 0177572:  // Регистр адреса косвенной адресации
        return m_Port177572;

    default:
        return 0;
    }
}

void CMotherboard::SetPortByte(uint16_t address, uint8_t byte)
{
    uint16_t word;
    if (address & 1)
    {
        word = GetPortWord(address & 0xfffe);
        word &= 0xff;
        word |= byte << 8;
        SetPortWord(address & 0xfffe, word);
    }
    else
    {
        word = GetPortWord(address);
        word &= 0xff00;
        SetPortWord(address, word | byte);
    }
}

void CMotherboard::SetPortWord(uint16_t address, uint16_t word)
{
    switch (address)
    {
    case 0170000:
    case 0170002:
    case 0170004:
        break;  //STUB

    case 0170006:
        m_Port170006 = 0;
        m_Port170006wr = word;
        //if ((word & 01400) == 0) m_pCPU->SetHaltMode(false);
        m_pCPU->SetHaltMode((word & 3) != 0);
        break;  //STUB

    case 0170010:  // Network
    case 0170012:
        break;  //STUB

    case 0170014:
        break;  //STUB

    case 0170020:  // Timer status
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_TIMER) DebugLogFormat(_T("Timer Status SET %06o\r\n"), word);
#endif
        m_Port170020 = word & 01777;
        break;
    case 0170022:
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_TIMER) DebugLogFormat(_T("Timer Freq  SET %06o\r\n"), word);
#endif
        m_Port170022 = word;
        break;
    case 0170024:
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_TIMER) DebugLogFormat(_T("Timer Len   SET %06o\r\n"), word);
#endif
        m_Port170024 = word;
        break;
    case 0170026:  // Sound On
#if !defined(PRODUCT)
        //DebugLogFormat(_T("%06o Sound ON\r\n"), m_pCPU->GetPC());
#endif
        m_okSoundOnOff = true;
        break;
    case 0170030:
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_TIMER) DebugLogFormat(_T("Timer OctVol SET %06o\r\n"), word);
#endif
        m_Port170030 = word & 037;
        break;
    case 0170032:  // Sound On/Off trigger
#if !defined(PRODUCT)
        //DebugLogFormat(_T("%06o Sound OFF\r\n"), m_pCPU->GetPC());
#endif
        m_okSoundOnOff = !m_okSoundOnOff;
        break;

        // Последовательный порт (отсутствует на реальной Немиге)
    case 0176500:  // Регистр состояния приемника
    case 0176501:
        m_Port176500 = (m_Port176500 & ~0100) | (word & 0100);  // Bit 6 only
        break;
    case 0176502:  // Регистр данных приемника
    case 0176503:  // недоступен по записи
        break;
    case 0176504:  // Регистр состояния источника
    case 0176505:
        if (((m_Port176504 & 0300) == 0200) && (word & 0100))
            m_pCPU->InterruptVIRQ(8, 0304);
        m_Port176504 = (m_Port176504 & ~0105) | (word & 0105);  // Bits 0,2,6
        break;
    case 0176506:  // Регистр данных источника
    case 0176507:  // нижние 8 бит доступны по записи
        m_Port176506 = word & 0xff;
        m_Port176504 &= ~128;  // Reset bit 7 (Ready)
        break;

    case 0177100:  // Floppy status
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetState(word);
        break;
    case 0177102:  // Floppy data
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->WriteData(word);
        break;
    case 0177104:  // Floppy command
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetCommand(word);
        break;
    case 0177106:
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetTimer(word);
        break;

    case 0177514:
#if !defined(PRODUCT)
        //DebugLogFormat(_T("Parallel SET STATE %06o\r\n"), word);
#endif
        m_Port177514 = (m_Port177514 & 0100277) | (word & 037600);
        break;
    case 0177516:
#if !defined(PRODUCT)
        //DebugLogFormat(_T("Parallel SET DATA %04x\r\n"), word);
#endif
        m_Port177516 = word;
        m_Port177514 &= ~0240;
        break;

    case 0177560:
    case 0177562:
    case 0177564:
    case 0177566:
        break;  //STUB
    case 0177574:
        m_Port177574 = word;
        break;
    case 0177576:
        break;  //STUB

    case 0177572:
        m_Port177572 = word;
        break;
    case 0177570:
        *(uint16_t*)(m_pRAM + m_Port177572 + m_Port177572) = word;
        break;

    default:
        m_pCPU->MemoryError();
        break;
    }
}


//////////////////////////////////////////////////////////////////////
// Emulator image
//  Offset Length
//       0     32 bytes  - Header
//      32    128 bytes  - Board status
//     160     32 bytes  - CPU status
//     192   3904 bytes  - RESERVED
//    4096   4096 bytes  - Main ROM image 4K
//    8192   8192 bytes  - RESERVED for extra 8K ROM
//   16384 131072 bytes  - RAM image 128K
//  147456     --        - END

void CMotherboard::SaveToImage(uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*) (pImage + 32);
    *pwImage++ = m_Configuration;
    pwImage += 6;  // RESERVED
    *pwImage++ = m_Port170006;
    *pwImage++ = m_Port170006wr;
    *pwImage++ = m_Port177572;
    *pwImage++ = m_Port177574;
    *pwImage++ = m_Port177514;
    *pwImage++ = m_Port177516;
    *pwImage++ = m_Port170020;
    *pwImage++ = m_Port170022;
    *pwImage++ = m_Port170024;
    *pwImage++ = m_Port170030;
    *pwImage++ = m_Timer1;
    *pwImage++ = m_Timer1div;
    *pwImage++ = m_Timer2;
    *pwImage++ = (uint16_t)m_okSoundOnOff;

    // CPU status
    uint8_t* pImageCPU = pImage + 160;
    m_pCPU->SaveToImage(pImageCPU);
    // ROM
    uint8_t* pImageRom = pImage + 4096;
    memcpy(pImageRom, m_pROM, 4096);
    // RAM
    uint8_t* pImageRam = pImage + 16384;
    memcpy(pImageRam, m_pRAM, 128 * 1024);
}
void CMotherboard::LoadFromImage(const uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*)(pImage + 32);
    m_Configuration = *pwImage++;
    pwImage += 6;  // RESERVED
    m_Port170006 = *pwImage++;
    m_Port170006wr = *pwImage++;
    m_Port177572 = *pwImage++;
    m_Port177574 = *pwImage++;
    m_Port177516 = *pwImage++;
    m_Port170020 = *pwImage++;
    m_Port170022 = *pwImage++;
    m_Port170024 = *pwImage++;
    m_Port170030 = *pwImage++;
    m_Timer1 = *pwImage++;
    m_Timer1div = *pwImage++;
    m_Timer2 = *pwImage++;
    m_okSoundOnOff = ((*pwImage++) != 0);

    // CPU status
    const uint8_t* pImageCPU = pImage + 160;
    m_pCPU->LoadFromImage(pImageCPU);

    // ROM
    const uint8_t* pImageRom = pImage + 4096;
    memcpy(m_pROM, pImageRom, 4096);
    // RAM
    const uint8_t* pImageRam = pImage + 16384;
    memcpy(m_pRAM, pImageRam, 128 * 1024);
}

//void CMotherboard::FloppyDebug(uint8_t val)
//{
////#if !defined(PRODUCT)
////    TCHAR buffer[512];
////#endif
///*
//m_floppyaddr=0;
//m_floppystate=FLOPPY_FSM_WAITFORLSB;
//#define FLOPPY_FSM_WAITFORLSB	0
//#define FLOPPY_FSM_WAITFORMSB	1
//#define FLOPPY_FSM_WAITFORTERM1	2
//#define FLOPPY_FSM_WAITFORTERM2	3
//
//*/
//	switch(m_floppystate)
//	{
//		case FLOPPY_FSM_WAITFORLSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr=val;
//				m_floppystate=FLOPPY_FSM_WAITFORMSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORMSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr|=val<<8;
//				m_floppystate=FLOPPY_FSM_WAITFORTERM1;
//			}
//			else
//			{
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORTERM1:
//			if(val==0xff)
//			{ //done
//				uint16_t par;
//				uint8_t trk,sector,side;
//
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr,0);
//
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(">>>>FDD Cmd %d "),(par>>8)&0xff);
////				DebugPrint(buffer);
////#endif
//                par=m_pFirstMemCtl->GetWord(m_floppyaddr+2,0);
//				side=par&0x8000?1:0;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Side %d Drv %d, Type %d "),par&0x8000?1:0,(par>>8)&0x7f,par&0xff);
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+4,0);
//				sector=(par>>8)&0xff;
//				trk=par&0xff;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Sect %d, Trk %d "),(par>>8)&0xff,par&0xff);
////				DebugPrint(buffer);
////				PrintOctalValue(buffer,m_pFirstMemCtl->GetWord(m_floppyaddr+6,0));
////				DebugPrint(_T("Addr "));
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+8,0);
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(" Block %d Len %d\n"),trk*20+side*10+sector-1,par);
////				DebugPrint(buffer);
////#endif
//
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//
//	}
//}


//////////////////////////////////////////////////////////////////////

void CMotherboard::DoSound(void)
{
    if (m_SoundGenCallback == NULL)
        return;

    uint16_t volume = (m_Port170030 >> 3) & 3;  // Громкость 0..3
    uint16_t octave = m_Port170030 & 7;  // Октава 1..7
    if (!m_okSoundOnOff || volume == 0 || octave == 0)
    {
        (*m_SoundGenCallback)(0, 0);
        return;
    }
    if (m_Timer1 > m_Port170022 / 2)
    {
        (*m_SoundGenCallback)(0, 0);
        return;
    }

    uint16_t sound = 0x1fff >> (3 - volume);
    (*m_SoundGenCallback)(sound, sound);
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == NULL)  // Reset callback
    {
        m_SoundGenCallback = NULL;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}

void CMotherboard::SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback)
{
    if (incallback == NULL || outcallback == NULL)  // Reset callbacks
    {
        m_SerialInCallback = NULL;
        m_SerialOutCallback = NULL;
        //TODO: Set port value to indicate we are not ready to translate
    }
    else
    {
        m_SerialInCallback = incallback;
        m_SerialOutCallback = outcallback;
        //TODO: Set port value to indicate we are ready to translate
    }
}

void CMotherboard::SetParallelOutCallback(PARALLELOUTCALLBACK outcallback)
{
    if (outcallback == NULL)  // Reset callback
    {
        m_Port177514 |= 0100000;  // Set Error flag
        m_ParallelOutCallback = NULL;
    }
    else
    {
        m_Port177514 &= ~0100000;  // Reset Error flag
        m_ParallelOutCallback = outcallback;
    }
}


//////////////////////////////////////////////////////////////////////

#if !defined(PRODUCT)

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, DWORD dwTrace)
{
    bool okHaltMode = pProc->IsHaltMode();

    uint16_t memory[4];
    int addrtype = ADDRTYPE_RAM;
    memory[0] = pBoard->GetWordView(address + 0 * 2, okHaltMode, true, &addrtype);
    if (!(addrtype == ADDRTYPE_RAM && (dwTrace & TRACE_CPURAM)) &&
        !(addrtype == ADDRTYPE_ROM && (dwTrace & TRACE_CPUROM)))
        return;
    memory[1] = pBoard->GetWordView(address + 1 * 2, okHaltMode, true, &addrtype);
    memory[2] = pBoard->GetWordView(address + 2 * 2, okHaltMode, true, &addrtype);
    memory[3] = pBoard->GetWordView(address + 3 * 2, okHaltMode, true, &addrtype);

    TCHAR bufaddr[7];
    PrintOctalValue(bufaddr, address);

    TCHAR instr[8];
    TCHAR args[32];
    DisassembleInstruction(memory, address, instr, args);
    TCHAR buffer[64];
    wsprintf(buffer, _T("%s: %s\t%s\r\n"), bufaddr, instr, args);
    //wsprintf(buffer, _T("%s %s: %s\t%s\r\n"), pProc->IsHaltMode() ? _T("HALT") : _T("USER"), bufaddr, instr, args);

    DebugLog(buffer);
}

#endif

//////////////////////////////////////////////////////////////////////

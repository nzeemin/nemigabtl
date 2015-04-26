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


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);
    m_pFloppyCtl = NULL;

    m_SoundGenCallback = NULL;
    m_ParallelOutCallback = NULL;

    // Allocate memory for RAM and ROM
    m_pRAM = (BYTE*) ::malloc(128 * 1024);  ::memset(m_pRAM, 0, 128 * 1024);
    m_pROM = (BYTE*) ::malloc(4 * 1024);  ::memset(m_pROM, 0, 4 * 1024);

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

void CMotherboard::SetConfiguration(WORD conf)
{
    m_Configuration = conf;

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 128 * 1024);
    ::memset(m_pROM, 0, 4 * 1024);

    //// Pre-fill RAM with "uninitialized" values
    //WORD * pMemory = (WORD *) m_pRAM;
    //WORD val = 0;
    //BYTE flag = 0;
    //for (DWORD i = 0; i < 128 * 1024; i += 2, flag--)
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
    }
    //if (m_pFloppyCtl != NULL /*&& (conf & BK_COPT_FDD) == 0*/)
    //{
    //    delete m_pFloppyCtl;  m_pFloppyCtl = NULL;
    //}
}

void CMotherboard::Reset () 
{
    m_pCPU->Stop();

    // Reset ports
    m_Port170006 = 0;
    m_Port177572 = 0;
    m_Port177514 = 0377;

    ResetDevices();

    m_pCPU->Start();
}

// Load 4 KB ROM image from the buffer
void CMotherboard::LoadROM(const BYTE* pBuffer)
{
    ::memcpy(m_pROM, pBuffer, 4096);
}

void CMotherboard::LoadRAM(int startbank, const BYTE* pBuffer, int length)
{
    ASSERT(pBuffer != NULL);
    ASSERT(startbank >= 0 && startbank < 15);
    int address = 8192 * startbank;
    ASSERT(address + length <= 128 * 1024);
    ::memcpy(m_pRAM + address, pBuffer, length);
}


// Floppy ////////////////////////////////////////////////////////////

BOOL CMotherboard::IsFloppyImageAttached(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->IsAttached(slot);
}

BOOL CMotherboard::IsFloppyReadOnly(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->IsReadOnly(slot);
}

BOOL CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return;
    m_pFloppyCtl->DetachImage(slot);
}

BOOL CMotherboard::IsFloppyEngineOn() const
{
    return m_pFloppyCtl->IsEngineOn();
}


// Работа с памятью //////////////////////////////////////////////////

WORD CMotherboard::GetRAMWord(WORD offset)
{
    return *((WORD*)(m_pRAM + offset)); 
}
WORD CMotherboard::GetHIRAMWord(WORD offset)
{
    return *((WORD*)(m_pRAM + 0xffff + offset));
}
BYTE CMotherboard::GetRAMByte(WORD offset) 
{ 
    return m_pRAM[offset]; 
}
BYTE CMotherboard::GetHIRAMByte(WORD offset) 
{ 
    DWORD dwOffset = (DWORD)0xffff + (DWORD)offset;
    return m_pRAM[dwOffset]; 
}
void CMotherboard::SetRAMWord(WORD offset, WORD word) 
{
    *((WORD*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetHIRAMWord(WORD offset, WORD word) 
{
    DWORD dwOffset = (DWORD)0xffff + (DWORD)offset;
    *((WORD*)(m_pRAM + dwOffset)) = word;
}
void CMotherboard::SetRAMByte(WORD offset, BYTE byte) 
{
    m_pRAM[offset] = byte; 
}
void CMotherboard::SetHIRAMByte(WORD offset, BYTE byte) 
{
    DWORD dwOffset = (DWORD)0xffff + (DWORD)offset;
    m_pRAM[dwOffset] = byte; 
}

WORD CMotherboard::GetROMWord(WORD offset)
{
    ASSERT(offset < 1024 * 64);
    return *((WORD*)(m_pROM + offset)); 
}
BYTE CMotherboard::GetROMByte(WORD offset) 
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
    m_pCPU->FireHALT();

    // Reset ports
    //TODO
}

void CMotherboard::ResetHALT()
{
    m_Port170006 = 0;
}

void CMotherboard::Tick50()  // 50 Hz timer
{
    //if ((m_Port177662wr & 040000) == 0)
    //{
        m_pCPU->TickIRQ2();
    //}
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

void CMotherboard::DebugTicks()
{
    m_pCPU->SetInternalTick(0);
    m_pCPU->Execute();
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Periodic();
}


/*
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.
12 МГц = 1 / 12000000 = 0.83(3) мкс
В каждый фрейм происходит:
* 160000 тиков ЦП - 8 раз за тик -- 4 МГц, 2.5 мкс
* 2 тика IRQ2 50 Гц, в 0-й и 10000-й тик фрейма
* 625 тиков FDD - каждый 32-й тик (300 RPM = 5 оборотов в секунду)
*/
BOOL CMotherboard::SystemFrame()
{
    int frameProcTicks = 4;
    //const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);
    int floppyTicks = 32;
    int teletypeTxCount = 0;

    int timerTicks = 0;

    for (int frameticks = 0; frameticks < 20000; frameticks++)
    {
        for (int procticks = 0; procticks < frameProcTicks; procticks++)  // CPU ticks
        {
            m_pCPU->Execute();
            if (m_pCPU->GetPC() == m_CPUbp)
                return FALSE;  // Breakpoint

            //timerTicks++;
            //if (timerTicks >= 128)
            //{
            //    TimerTick();  // System timer tick: 31250 Hz = 32uS (BK-0011), 23437.5 Hz = 42.67 uS (BK-0010)
            //    timerTicks = 0;
            //}
        }

        if (frameticks % 10000 == 0)
        {
            Tick50();  // 1/50 timer event
        }

        if (frameticks % floppyTicks == 0)  // FDD tick, every 64 uS
        {
            if (m_pFloppyCtl != NULL)
                m_pFloppyCtl->Periodic();
        }

        //if (frameticks % audioticks == 0)  // AUDIO tick
        //    DoSound();

        if (m_ParallelOutCallback != NULL)
        {
            if ((m_Port177514 & 0240) == 040)
            {
                m_Port177514 |= 0200;  // Set TR flag
                // Now printer waits for a next byte
                if (m_Port177514 & 0100)
                    m_pCPU->InterruptVIRQ(5, 0200);
            }
            else if ((m_Port177514 & 0240) == 0)
            {  // Byte is ready, print it
                (*m_ParallelOutCallback)((BYTE)(m_Port177516 & 0xff));
                m_Port177514 |= 040;  // Set Printer Acknowledge
            }
        }
    }

    return TRUE;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(BYTE scancode, BOOL okPressed)
{
    if (okPressed)  // Key released
    {
        m_Port170006 |= 02000;
        m_Port170006 |= scancode;
        m_pCPU->FireHALT();
        return;
    }
}


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
WORD CMotherboard::GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, int* pAddrType)
{
    WORD offset;
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
    case ADDRTYPE_HALT:
        return 0;
    case ADDRTYPE_DENY:
        return 0;  // This memory is inaccessible for reading
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

WORD CMotherboard::GetWord(WORD address, BOOL okHaltMode, BOOL okExec)
{
    WORD offset;
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
        //TODO: What to do if okExec == TRUE ?
        return GetPortWord(address);
    case ADDRTYPE_HALT:
        if (address == 0177560)
            m_Port170006 |= 010000;
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
            return GetRAMWord(offset & 0177776);
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
        return GetRAMWord(offset & 0177776);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

BYTE CMotherboard::GetByte(WORD address, BOOL okHaltMode)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMByte(offset);
    case ADDRTYPE_HIRAM:
        return GetHIRAMByte(offset);
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortByte(address);
    case ADDRTYPE_HALT:
        if (address == 0177560)
            m_Port170006 |= 010000;
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
            return GetRAMByte(offset);
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
        return GetRAMByte(offset);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(WORD address, BOOL okHaltMode, WORD word)
{
    WORD offset;

    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

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
    case ADDRTYPE_HALT:
        if (address == 0177560)
            m_Port170006 |= 010000;
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
        {
            SetRAMWord(offset & 0177776, word);
            return;
        }
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
        SetRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(WORD address, BOOL okHaltMode, BYTE byte)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

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
    case ADDRTYPE_HALT:
        if (address == 0177560)
            m_Port170006 |= 010000;
        else if (address == 0177562)
            m_Port170006 |= 020000;
        else if (address == 0177564)
        {
            SetRAMByte(offset, byte);
            return;
        }
        else if (address == 0177566)
            m_Port170006 |= 040000;
        m_pCPU->FireHALT();
        SetRAMByte(offset, byte);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

// Calculates video buffer start address, for screen drawing procedure
const BYTE* CMotherboard::GetVideoBuffer()
{
    return (m_pRAM + 0140000 + 0140000);
}

int CMotherboard::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset)
{
    if (address < 0160000)  // 000000-157777 -- RAM
    {
        *pOffset = address;
        return ADDRTYPE_RAM;
    }

    if (address < 0170000)  // 160000-167777 -- ROM
    {
        *pOffset = address - 0160000;
        return ADDRTYPE_ROM;
    }

    if (!okHaltMode && address >= 0177560 && address <= 0177567)
    {
        *pOffset = address;
        return ADDRTYPE_HALT;
    }

    if ((address >= 0170006 && address <= 0170013) ||
        //(address >= 0177560 && address <= 0177577) ||
        (address >= 0177514 && address <= 0177517) ||
        (address >= 0177570 && address <= 0177573) ||
        (address >= 0177100 && address <= 0177107))  // Ports
    {
        *pOffset = address;
        return ADDRTYPE_IO;
    }

    //if (okHaltMode && address >= 0177600)  // 177600-177777
    //{
    //    *pOffset = address - 010000;  // Располагаем эту область в ОЗУ "под" ПЗУ в 167600-167777
    //    return ADDRTYPE_RAM;
    //}

    *pOffset = address;
    return ADDRTYPE_RAM;
}

BYTE CMotherboard::GetPortByte(WORD address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (BYTE) GetPortWord(address);
}

WORD CMotherboard::GetPortWord(WORD address)
{
    switch (address)
    {
    case 0170006:
        return m_Port170006;

    case 0170010:  // Network
    case 0170012:
        return 0xffff;  //STUB

    case 0170020:  // Timer
    case 0170022:
    case 0170024:
    case 0170026:
    case 0170030:
    case 0170032:
        return 0;  //STUB

    case 0177100:  // Floppy status
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetState();
    case 0177102:  // Floppy data
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetData();
    case 0177104:  // Floppy command
        return 0;  //STUB
    case 0177106:  // Floppy timer
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
    //case 0177574:
    //case 0177576:
    //    return 0xffff;  //STUB

    case 0177572:  // Регистр адреса косвенной адресации
        return m_Port177572;
    case 0177570:
        return *(WORD*)(m_pRAM + m_Port177572 + m_Port177572);

    default: 
        m_pCPU->MemoryError();
        return 0;
    }

    return 0; 
}

// Read word from port for debugger
WORD CMotherboard::GetPortView(WORD address)
{
    switch (address)
    {
    case 0170010:  // Network
    case 0170012:
        return 0;  //STUB

    case 0170020:  // Timer
    case 0170022:
    case 0170024:
    case 0170026:
    case 0170030:
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
    case 0177564:
    case 0177566:
    case 0177570:
    case 0177574:
    case 0177576:
        return 0;  //STUB

    case 0177572:  // Регистр адреса косвенной адресации
        return m_Port177572;

    default:
        return 0;
    }
}

void CMotherboard::SetPortByte(WORD address, BYTE byte)
{
    WORD word;
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

void CMotherboard::SetPortWord(WORD address, WORD word)
{
    switch (address)
    {
    case 0170006:
        m_Port170006 = 0;
        m_pCPU->SetHaltMode((word & 3) != 0);
        break;  //STUB

    case 0170010:  // Network
    case 0170012:
        break;  //STUB

    case 0170020:  // Timer
    case 0170022:
    case 0170024:
    case 0170026:
    case 0170030:
    case 0170032:
        break;  //STUB

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
    	DebugLogFormat(_T("Parallel SET STATE %06o\r\n"), word);
#endif
        m_Port177514 = (m_Port177514 & 0100277) | (word & 037600);
        break;
    case 0177516:
#if !defined(PRODUCT)
    	DebugLogFormat(_T("Parallel SET DATA %04x\r\n"), word);
#endif
        m_Port177516 = word;
        m_Port177514 &= ~0240;
        break;

    case 0177560:
    case 0177562:
    case 0177564:
    case 0177566:
    case 0177574:
    case 0177576:
        break;  //STUB

    case 0177572:
        m_Port177572 = word;
        break;
    case 0177570:
        *(WORD*)(m_pRAM + m_Port177572 + m_Port177572) = word;
        break;

    default:
        m_pCPU->MemoryError();
        break;
    }
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format:
//   32 bytes  - Header
//   32 bytes  - Board status
//   32 bytes  - CPU status
//   64 bytes  - CPU memory/IO controller status
//   32 Kbytes - ROM image
//   64 Kbytes - RAM image

//void CMotherboard::SaveToImage(BYTE* pImage)
//{
//    // Board data
//    WORD* pwImage = (WORD*) (pImage + 32);
//    *pwImage++ = m_timer;
//    *pwImage++ = m_timerreload;
//    *pwImage++ = m_timerflags;
//    *pwImage++ = m_timerdivider;
//    *pwImage++ = (WORD) m_chan0disabled;
//
//    // CPU status
//    BYTE* pImageCPU = pImage + 64;
//    m_pCPU->SaveToImage(pImageCPU);
//    // PPU status
//    BYTE* pImagePPU = pImageCPU + 32;
//    m_pPPU->SaveToImage(pImagePPU);
//    // CPU memory/IO controller status
//    BYTE* pImageCpuMem = pImagePPU + 32;
//    m_pFirstMemCtl->SaveToImage(pImageCpuMem);
//    // PPU memory/IO controller status
//    BYTE* pImagePpuMem = pImageCpuMem + 64;
//    m_pSecondMemCtl->SaveToImage(pImagePpuMem);
//
//    // ROM
//    BYTE* pImageRom = pImage + 256;
//    memcpy(pImageRom, m_pROM, 32 * 1024);
//    // RAM planes 0, 1, 2
//    BYTE* pImageRam = pImageRom + 32 * 1024;
//    memcpy(pImageRam, m_pRAM[0], 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(pImageRam, m_pRAM[1], 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(pImageRam, m_pRAM[2], 64 * 1024);
//}
//void CMotherboard::LoadFromImage(const BYTE* pImage)
//{
//    // Board data
//    WORD* pwImage = (WORD*) (pImage + 32);
//    m_timer = *pwImage++;
//    m_timerreload = *pwImage++;
//    m_timerflags = *pwImage++;
//    m_timerdivider = *pwImage++;
//    m_chan0disabled = (BYTE) *pwImage++;
//
//    // CPU status
//    const BYTE* pImageCPU = pImage + 64;
//    m_pCPU->LoadFromImage(pImageCPU);
//    // PPU status
//    const BYTE* pImagePPU = pImageCPU + 32;
//    m_pPPU->LoadFromImage(pImagePPU);
//    // CPU memory/IO controller status
//    const BYTE* pImageCpuMem = pImagePPU + 32;
//    m_pFirstMemCtl->LoadFromImage(pImageCpuMem);
//    // PPU memory/IO controller status
//    const BYTE* pImagePpuMem = pImageCpuMem + 64;
//    m_pSecondMemCtl->LoadFromImage(pImagePpuMem);
//
//    // ROM
//    const BYTE* pImageRom = pImage + 256;
//    memcpy(m_pROM, pImageRom, 32 * 1024);
//    // RAM planes 0, 1, 2
//    const BYTE* pImageRam = pImageRom + 32 * 1024;
//    memcpy(m_pRAM[0], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(m_pRAM[1], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(m_pRAM[2], pImageRam, 64 * 1024);
//}

//void CMotherboard::FloppyDebug(BYTE val)
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
//				WORD par;
//				BYTE trk,sector,side;
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

    //BOOL bSoundBit = (m_Port177716tap & 0100) != 0;

    //if (bSoundBit)
    //    (*m_SoundGenCallback)(0x1fff,0x1fff);
    //else
    //    (*m_SoundGenCallback)(0x0000,0x0000);
}

//void CMotherboard::SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate)
//{
//    if (callback == NULL)  // Reset callback
//    {
//        m_TapeReadCallback = NULL;
//        m_nTapeSampleRate = 0;
//    }
//    else
//    {
//        m_TapeReadCallback = callback;
//        m_nTapeSampleRate = sampleRate;
//        m_TapeWriteCallback = NULL;
//    }
//}
//
//void CMotherboard::SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate)
//{
//    if (callback == NULL)  // Reset callback
//    {
//        m_TapeWriteCallback = NULL;
//        m_nTapeSampleRate = 0;
//    }
//    else
//    {
//        m_TapeWriteCallback = callback;
//        m_nTapeSampleRate = sampleRate;
//        m_TapeReadCallback = NULL;
//    }
//}

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

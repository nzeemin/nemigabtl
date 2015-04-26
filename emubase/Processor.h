/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.h
//

#pragma once

#include "Defines.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////


class CProcessor  // KM1801VM1 processor
{

public:  // Constructor / initialization
                CProcessor(CMotherboard* pBoard);
    void        FireHALT() { m_HALTrq = TRUE; }  // Fire HALT interrupt request, same as HALT command
    void        SetHaltMode(BOOL halt) { m_haltmode = halt; }
    void		MemoryError();
    void        SetInternalTick (WORD tick) { m_internalTick = tick; }

public:
    static void Init();  // Initialize static tables
    static void Done();  // Release memory used for static tables
protected:  // Statics
    typedef void ( CProcessor::*ExecuteMethodRef )();
    static ExecuteMethodRef* m_pExecuteMethodMap;
    static void RegisterMethodRef(WORD start, WORD end, CProcessor::ExecuteMethodRef methodref);

protected:  // Processor state
    int         m_internalTick;     // How many ticks waiting to the end of current instruction
    WORD        m_psw;              // Processor Status Word (PSW)
    WORD        m_R[8];             // Registers (R0..R5, R6=SP, R7=PC)
    BOOL        m_okStopped;        // "Processor stopped" flag
    BOOL        m_haltmode;         // TRUE = HALT mode, FALSE = USER mode
    BOOL        m_stepmode;         // Read TRUE if it's step mode
    BOOL        m_waitmode;			// WAIT

protected:  // Current instruction processing
    WORD        m_instruction;      // Curent instruction
    WORD        m_instructionpc;    // Address of the current instruction
    int         m_regsrc;           // Source register number
    int         m_methsrc;          // Source address mode
    WORD        m_addrsrc;          // Source address
    int         m_regdest;          // Destination register number
    int         m_methdest;         // Destination address mode
    WORD        m_addrdest;         // Destination address

protected:  // Interrupt processing
    BOOL        m_RPLYrq;           // Hangup interrupt pending
    BOOL        m_RSVDrq;           // Reserved instruction interrupt pending
    BOOL        m_TBITrq;           // T-bit interrupt pending
    BOOL        m_HALTrq;           // HALT command or HALT signal
    BOOL        m_RPL2rq;           // Double hangup interrupt pending
    BOOL		m_IRQ2rq;           // Timer event interrupt pending
    BOOL        m_BPT_rq;           // BPT command interrupt pending
    BOOL        m_IOT_rq;           // IOT command interrupt pending
    BOOL        m_EMT_rq;           // EMT command interrupt pending
    BOOL        m_TRAPrq;           // TRAP command interrupt pending
    int         m_virqrq;           // VIRQ pending
    WORD        m_virq[16];         // VIRQ vector
protected:
    CMotherboard* m_pBoard;

public:  // Register control
    WORD        GetPSW() const { return m_psw; }
    BYTE        GetLPSW() const { return LOBYTE(m_psw); }
    void        SetPSW(WORD word) { m_psw = word; }
    void        SetLPSW(BYTE byte)
    {
        m_psw = (m_psw & 0xFF00) | (WORD)byte;
    }
    WORD        GetReg(int regno) const { return m_R[regno]; }
    void        SetReg(int regno, WORD word) { m_R[regno] = word; }
    WORD        GetSP() const { return m_R[6]; }
    void        SetSP(WORD word) { m_R[6] = word; }
    WORD        GetPC() const { return m_R[7]; }
    void        SetPC(WORD word) { m_R[7] = word; }
    WORD        GetInstructionPC() const { return m_instructionpc; }  // Address of the current instruction

public:  // PSW bits control
    void        SetC(BOOL bFlag);
    WORD        GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(BOOL bFlag);
    WORD        GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(BOOL bFlag);
    WORD        GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(BOOL bFlag);
    WORD        GetZ() const { return (m_psw & PSW_Z) != 0; }
    WORD        GetHALT() const { return m_haltmode; }

public:  // Processor state
    // "Processor stopped" flag
    BOOL        IsStopped() const { return m_okStopped; }
    // HALT flag (TRUE - HALT mode, FALSE - USER mode)
    BOOL        IsHaltMode() const { return m_haltmode; }
public:  // Processor control
    void        Start();     // Start processor
    void        Stop();      // Stop processor
    void        TickIRQ2();  // IRQ2 signal
    void        InterruptVIRQ(int que, WORD interrupt);  // External interrupt via VIRQ signal
    void        Execute();   // Execute one instruction - for debugger only
    
public:  // Saving/loading emulator status (pImage addresses up to 32 bytes)
    void        SaveToImage(BYTE* pImage);
    void        LoadFromImage(const BYTE* pImage);

protected:  // Implementation
    void        FetchInstruction();      // Read next instruction
    void        TranslateInstruction();  // Execute the instruction
protected:  // Implementation - instruction processing
    WORD        CalculateOperAddr (int meth, int reg);
    WORD        CalculateOperAddrSrc (int meth, int reg);
    BYTE        GetByteSrc();
    BYTE        GetByteDest();
    void        SetByteDest(BYTE);
    WORD        GetWordSrc();
    WORD        GetWordDest();
    void        SetWordDest(WORD);
    WORD        GetDstWordArgAsBranch();
protected:  // Implementation - memory access
    WORD        GetWordExec(WORD address) { return m_pBoard->GetWordExec(address, IsHaltMode()); }
    WORD        GetWord(WORD address) { return m_pBoard->GetWord(address, IsHaltMode()); }
    void        SetWord(WORD address, WORD word) { m_pBoard->SetWord(address, IsHaltMode(), word); }
    BYTE        GetByte(WORD address) { return m_pBoard->GetByte(address, IsHaltMode()); }
    void        SetByte(WORD address, BYTE byte) { m_pBoard->SetByte(address, IsHaltMode(), byte); }

protected:  // PSW bits calculations
    BOOL static CheckForNegative(BYTE byte) { return (byte & 0200) != 0; }
    BOOL static CheckForNegative(WORD word) { return (word & 0100000) != 0; }
    BOOL static CheckForZero(BYTE byte) { return byte == 0; }
    BOOL static CheckForZero(WORD word) { return word == 0; }
    BOOL static CheckAddForOverflow(BYTE a, BYTE b);
    BOOL static CheckAddForOverflow(WORD a, WORD b);
    BOOL static CheckSubForOverflow(BYTE a, BYTE b);
    BOOL static CheckSubForOverflow(WORD a, WORD b);
    BOOL static CheckAddForCarry(BYTE a, BYTE b);
    BOOL static CheckAddForCarry(WORD a, WORD b);
    BOOL static CheckSubForCarry(BYTE a, BYTE b);
    BOOL static CheckSubForCarry(WORD a, WORD b);

protected:
    WORD		GetWordAddr (BYTE meth, BYTE reg);
    WORD		GetByteAddr (BYTE meth, BYTE reg);

protected:  // Implementation - instruction execution
    void        ExecuteUNKNOWN ();  // Нет такой инструкции - просто вызывается TRAP 10

    // Команды перечислены по книге "Микропроцессорный комплект БИС серии КР588" Таблица 2, стр.11-13
    // Одноадресные команды
    void        ExecuteCLR ();      // CLR(B),  0001
    void        ExecuteCOM ();      //  0001
    void        ExecuteINC ();      //  0001
    void        ExecuteDEC ();      //  0001
    void        ExecuteNEG ();      //  0001
    void        ExecuteTST ();      //  0001
    void        ExecuteASR ();      //  0001
    void        ExecuteASL ();      //  0001
    void        ExecuteROR ();      //  0001
    void        ExecuteROL ();      //  0001
    void        ExecuteADC ();      //  0001
    void        ExecuteSBC ();      //  0001
    void        ExecuteSXT ();      //  0002
    void        ExecuteSWAB ();     //  0002
    void        ExecuteMTPS ();     //  0002
    void        ExecuteMFPS ();     //  0002
    // Двухадресные команды
    void        ExecuteMOV ();      //  0001
    void        ExecuteCMP ();      //  0001
    void        ExecuteADD ();      //  0001
    void        ExecuteSUB ();      //  0001
    void        ExecuteBIT ();      //  0001
    void        ExecuteBIC ();      //  0001
    void        ExecuteBIS ();      //  0001
    void        ExecuteXOR ();      //  0002
    // Команды управления программой
    void        ExecuteBR ();       //  0002
    void        ExecuteBNE ();      //  0002
    void        ExecuteBEQ ();      //  0002
    void        ExecuteBPL ();      //  0002
    void        ExecuteBMI ();      //  0002
    void        ExecuteBVC ();      //  0002
    void        ExecuteBVS ();      //  0002
    //BCC == BHIS
    //BCS == BLO
    void        ExecuteBGE ();      //  0002
    void        ExecuteBLT ();      //  0002
    void        ExecuteBGT ();      //  0002
    void        ExecuteBLE ();      //  0002
    void        ExecuteBHI ();      //  0002
    void        ExecuteBLOS ();     //  0002
    void        ExecuteBHIS ();     //  0002
    void        ExecuteBLO ();      //  0002
    void        ExecuteJMP ();      //  0004
    void        ExecuteJSR ();      //  0004
    void        ExecuteRTS ();      //  0004
    void        ExecuteMARK ();     //  0002
    void        ExecuteSOB ();      //  0002
    // Команды прерывания программы
    void        ExecuteEMT ();      //  0004
    void        ExecuteTRAP ();     //  0004
    void        ExecuteIOT ();      //  0004
    void        ExecuteBPT ();      //  0004
    void        ExecuteRTI ();      //  0004
    void        ExecuteRTT ();      //  0004
    void        ExecuteHALT ();     //  0004
    void        ExecuteWAIT ();     //  0004
    void        ExecuteRESET ();    //  0004
    // Команды изменения признаков
    void        ExecuteCLC ();      //  0002
    void        ExecuteCLV ();      //  0002
    void        ExecuteCLVC ();
    void        ExecuteCLZ ();      //  0002
    void        ExecuteCLZC ();
    void        ExecuteCLZV ();
    void        ExecuteCLZVC ();
    void        ExecuteCLN ();      //  0002
    void        ExecuteCLNC ();
    void        ExecuteCLNV ();
    void        ExecuteCLNVC ();
    void        ExecuteCLNZ ();
    void        ExecuteCLNZC ();
    void        ExecuteCLNZV ();
    void        ExecuteCCC ();      //  0002
    void        ExecuteSEC ();      //  0002
    void        ExecuteSEV ();      //  0002
    void        ExecuteSEVC ();
    void        ExecuteSEZ ();      //  0002
    void        ExecuteSEZC ();
    void        ExecuteSEZV ();
    void        ExecuteSEZVC ();
    void        ExecuteSEN ();      //  0002
    void        ExecuteSENC ();
    void        ExecuteSENV ();
    void        ExecuteSENVC ();
    void        ExecuteSENZ ();
    void        ExecuteSENZC ();
    void        ExecuteSENZV ();
    void        ExecuteSCC ();      //  0002
    void        ExecuteNOP ();      //  0002
    // Команды расширенной арифметики
    void		ExecuteMUL ();      //  0003
    void		ExecuteDIV ();      //  0003
    void		ExecuteASH ();      //  0003
    void		ExecuteASHC ();     //  0003

};

// PSW bits control - implementation
inline void CProcessor::SetC (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_C; else m_psw &= ~PSW_C;
}
inline void CProcessor::SetV (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_V; else m_psw &= ~PSW_V;
}
inline void CProcessor::SetN (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_N; else m_psw &= ~PSW_N;
}
inline void CProcessor::SetZ (BOOL bFlag)
{
    if (bFlag) m_psw |= PSW_Z; else m_psw &= ~PSW_Z;
}

// PSW bits calculations - implementation
inline BOOL CProcessor::CheckAddForOverflow (BYTE a, BYTE b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cl,byte ptr [a]
        add cl,byte ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:                            
        pop cx
        popf
    }
    return bOverflow;
#else
    //WORD sum = a < 0200 ? (WORD)a + (WORD)b + 0200 : (WORD)a + (WORD)b - 0200;
    //return HIBYTE (sum) != 0;
    BYTE sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0200;
#endif
}
inline BOOL CProcessor::CheckAddForOverflow (WORD a, WORD b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cx,word ptr [a]
        add cx,word ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:                            
        pop cx
        popf
    }
    return bOverflow;
#else
    //DWORD sum =  a < 0100000 ? (DWORD)a + (DWORD)b + 0100000 : (DWORD)a + (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;
    WORD sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0100000;
#endif
}
//void        CProcessor::SetReg(int regno, WORD word) 

inline BOOL CProcessor::CheckSubForOverflow (BYTE a, BYTE b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cl,byte ptr [a]
        sub cl,byte ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:                            
        pop cx
        popf
    }
    return bOverflow;
#else
    //WORD sum = a < 0200 ? (WORD)a - (WORD)b + 0200 : (WORD)a - (WORD)b - 0200;
    //return HIBYTE (sum) != 0;
    BYTE sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0200;
#endif
}
inline BOOL CProcessor::CheckSubForOverflow (WORD a, WORD b)
{
#ifdef _M_IX86
    BOOL bOverflow = FALSE;
    _asm
    {
        pushf
        push cx
        mov cx,word ptr [a]
        sub cx,word ptr [b]
        jno end
        mov dword ptr [bOverflow],1
    end:                            
        pop cx
        popf
    }
    return bOverflow;
#else
    //DWORD sum =  a < 0100000 ? (DWORD)a - (DWORD)b + 0100000 : (DWORD)a - (DWORD)b - 0100000;
    //return HIWORD (sum) != 0;
    WORD sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0100000;
#endif
}
inline BOOL CProcessor::CheckAddForCarry (BYTE a, BYTE b)
{
    WORD sum = (WORD)a + (WORD)b;
    return HIBYTE (sum) != 0;
}
inline BOOL CProcessor::CheckAddForCarry (WORD a, WORD b)
{
    DWORD sum = (DWORD)a + (DWORD)b;
    return HIWORD (sum) != 0;
}
inline BOOL CProcessor::CheckSubForCarry (BYTE a, BYTE b)
{
    WORD sum = (WORD)a - (WORD)b;
    return HIBYTE (sum) != 0;
}
inline BOOL CProcessor::CheckSubForCarry (WORD a, WORD b)
{
    DWORD sum = (DWORD)a - (DWORD)b;
    return HIWORD (sum) != 0;
}


//////////////////////////////////////////////////////////////////////

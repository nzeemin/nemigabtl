/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Emubase.h  Header for use of all emubase classes
//

#pragma once

#include "Board.h"
#include "Processor.h"


//////////////////////////////////////////////////////////////////////


#define SOUNDSAMPLERATE  22050


//////////////////////////////////////////////////////////////////////
// Disassembler

// Disassemble one instruction of KM1801VM2 processor
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
int DisassembleInstruction(WORD* pMemory, WORD addr, TCHAR* sInstr, TCHAR* sArg);


//////////////////////////////////////////////////////////////////////
// CFloppy

#define FLOPPY_STATUS_RELOAD			01
#define FLOPPY_STATUS_INDEX 			04
#define FLOPPY_STATUS_TR00_WRPRT		010
#define FLOPPY_STATUS_OPFAILED			040
#define FLOPPY_STATUS_LOSTDATA	    	0100
#define FLOPPY_STATUS_TR    			0200

#define FLOPPY_OPER_NOOPERATION         0177777
#define FLOPPY_OPER_READ_TRACK          0
#define FLOPPY_OPER_WRITE_TRACK         040
#define FLOPPY_OPER_STEP_OUT            020
#define FLOPPY_OPER_STEP_IN             060

#define FLOPPY_RAWTRACKSIZE             3125    // Track length, bytes
#define FLOPPY_INDEXLENGTH              15      // Index mark length

struct CFloppyDrive
{
    FILE* fpFile;
    BOOL okReadOnly;    // Write protection flag
    WORD dataptr;       // Data offset within m_data - "head" position
    BYTE data[FLOPPY_RAWTRACKSIZE];  // Raw track image for the current track
    WORD datatrack;     // Track number of data in m_data array

public:
    CFloppyDrive();
    void Reset();
};

class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[8];  // Четыре привода по две стороны
    int  m_drive;       // Drive number: from 0 to 7; -1 if not selected
    CFloppyDrive* m_pDrive;  // Current drive; NULL if not selected
    WORD m_track;       // Track number: from 0 to 79
    WORD m_status;      // See FLOPPY_STATUS_XXX defines
    WORD m_datareg;     // Read mode data register
    WORD m_writereg;    // Write mode data register
    BOOL m_writeflag;   // Write mode data register has data
    WORD m_shiftreg;    // Write mode shift register
    BOOL m_shiftflag;   // Write mode shift register has data
    BOOL m_trackchanged;  // TRUE = m_data was changed - need to save it into the file
    BOOL m_timer;       // Floppy timer bit at port 177106
    int  m_timercount;  // Floppy timer counter
    BOOL m_motoron;     // Motor ON flag
    int  m_motorcount;  // Motor ON counter
    WORD m_operation;   // Operation code, see FLOPPY_OPER_XXX defines
    int  m_opercount;   // Operation counter - countdown or current operation stage

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();

public:
    BOOL AttachImage(int drive, LPCTSTR sFileName);
    void DetachImage(int drive);
    BOOL IsAttached(int drive) { return (m_drivedata[drive].fpFile != NULL); }
    BOOL IsReadOnly(int drive) { return m_drivedata[drive].okReadOnly; } // return (m_status & FLOPPY_STATUS_WRITEPROTECT) != 0; }
    BOOL IsEngineOn() const { return m_motoron; }
    WORD GetState();            // Reading port 177100 - status
    WORD GetData();             // Reading port 177102 - data
    WORD GetTimer();            // Reading port 177106 - timer
    WORD GetStateView() const { return m_status; }  // Get port 177100 value for debugger
    WORD GetDataView() const { return m_datareg; }  // Get port 177102 value for debugger
    void SetState(WORD data);   // Writing port 177100 - status
    void WriteData(WORD data);  // Writing port 177102 - data
    void SetCommand(WORD cmd);  // Writing port 177104 - commands
    void SetTimer(WORD value);  // Writing port 177106 - timer
    void Periodic();            // Rotate disk; call it each 64 us - 15625 times per second

protected:
    void PrepareTrack();
    void FlushChanges();  // If current track was changed - save it

};


//////////////////////////////////////////////////////////////////////

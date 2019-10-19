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
int DisassembleInstruction(uint16_t* pMemory, uint16_t addr, TCHAR* sInstr, TCHAR* sArg);


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

const uint8_t FLOPPY_TYPE_NONE = 0;
const uint8_t FLOPPY_TYPE_MD = 1;
const uint8_t FLOPPY_TYPE_MX = 2;

struct CFloppyDrive
{
    FILE* fpFile;
    bool okReadOnly;        // Write protection flag
    uint8_t floppytype;     // See FLOPPY_TYPE_XX constants
    uint16_t dataptr;       // Data offset within m_data - "head" position
    uint8_t data[FLOPPY_RAWTRACKSIZE];  // Raw track image for the current track
    uint16_t datatrack;     // Track number of data in m_data array

public:
    CFloppyDrive();
    void Reset();
};

class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[8];  // Четыре привода по две стороны
    int m_drive;            // Drive number: from 0 to 7; -1 if not selected
    CFloppyDrive* m_pDrive; // Current drive; NULL if not selected
    uint16_t m_track;       // Track number: from 0 to 79
    uint16_t m_status;      // See FLOPPY_STATUS_XXX defines
    uint16_t m_datareg;     // Read mode data register
    uint16_t m_writereg;    // Write mode data register
    bool m_writeflag;       // Write mode data register has data
    uint16_t m_shiftreg;    // Write mode shift register
    bool m_shiftflag;       // Write mode shift register has data
    bool m_trackchanged;  // TRUE = m_data was changed - need to save it into the file
    bool m_timer;       // Floppy timer bit at port 177106
    int  m_timercount;  // Floppy timer counter
    bool m_motoron;     // Motor ON flag
    int  m_motorcount;  // Motor ON counter
    uint16_t m_operation;   // Operation code, see FLOPPY_OPER_XXX defines
    int  m_opercount;   // Operation counter - countdown or current operation stage
    bool m_okTrace;         // Trace mode on/off

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();

public:
    bool AttachImage(int drive, LPCTSTR sFileName, uint8_t floppyType);
    void DetachImage(int drive);
    uint8_t GetFloppyType(int drive) { return m_drivedata[drive].floppytype; }
    bool IsReadOnly(int drive) { return m_drivedata[drive].okReadOnly; } // return (m_status & FLOPPY_STATUS_WRITEPROTECT) != 0; }
    bool IsEngineOn() const { return m_motoron; }
    uint16_t GetState();            // Reading port 177100 - status
    uint16_t GetData();             // Reading port 177102 - data
    uint16_t GetTimer();            // Reading port 177106 - timer
    uint16_t GetStateView() const { return m_status; }  // Get port 177100 value for debugger
    uint16_t GetDataView() const { return m_datareg; }  // Get port 177102 value for debugger
    void SetState(uint16_t data);   // Writing port 177100 - status
    void WriteData(uint16_t data);  // Writing port 177102 - data
    void SetCommand(uint16_t cmd);  // Writing port 177104 - commands
    void SetTimer(uint16_t value);  // Writing port 177106 - timer
    void Periodic();            // Rotate disk; call it each 64 us - 15625 times per second
    void SetTrace(bool okTrace) { m_okTrace = okTrace; }  // Set trace mode on/off

private:
    void PrepareTrack();
    void FlushChanges();  // If current track was changed - save it
};


//////////////////////////////////////////////////////////////////////

/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Floppy.cpp
// Floppy controller and drives emulation
// See defines in header file Emubase.h

#include "stdafx.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////

static void EncodeTrackData(const BYTE* pSrc, BYTE* data, WORD track, WORD side);
static BOOL DecodeTrackData(const BYTE* pRaw, BYTE* pDest, WORD track);

//////////////////////////////////////////////////////////////////////


CFloppyDrive::CFloppyDrive()
{
    fpFile = NULL;
    okReadOnly = FALSE;
    datatrack = 0;
    dataptr = 0;
}

void CFloppyDrive::Reset()
{
    //datatrack = 0;
    dataptr = 0;
}


//////////////////////////////////////////////////////////////////////

    
CFloppyController::CFloppyController()
{
    m_drive = -1;  m_pDrive = NULL;
    m_track = 0;
    m_timer = TRUE;  m_timercount = 0;
    m_motoron = FALSE;  m_motorcount = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = 0;
}

CFloppyController::~CFloppyController()
{
    for (int drive = 0; drive < 8; drive++)
        DetachImage(drive);
}

void CFloppyController::Reset()
{
#if !defined(PRODUCT)
    DebugLog(_T("Floppy RESET\r\n"));
#endif

    FlushChanges();

    m_drive = -1;  m_pDrive = NULL;
    m_track = 0;
    m_timer = TRUE;  m_timercount = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = FLOPPY_STATUS_TR;
}

BOOL CFloppyController::AttachImage(int drive, LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    // If image attached - detach one first
    if (m_drivedata[drive].fpFile != NULL)
        DetachImage(drive);

    // Open file
    m_drivedata[drive].okReadOnly = FALSE;
    m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("r+b"));
    if (m_drivedata[drive].fpFile == NULL)
    {
        m_drivedata[drive].okReadOnly = TRUE;
        m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("rb"));
    }
    if (m_drivedata[drive].fpFile == NULL)
        return FALSE;

    m_track = m_drivedata[drive].datatrack = 0;
    m_drivedata[drive].dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = FALSE;
    m_trackchanged = FALSE;
    m_status = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;

    PrepareTrack();

    return TRUE;
}

void CFloppyController::DetachImage(int drive)
{
    if (m_drivedata[drive].fpFile == NULL) return;

    FlushChanges();

    ::fclose(m_drivedata[drive].fpFile);
    m_drivedata[drive].fpFile = NULL;
    m_drivedata[drive].okReadOnly = FALSE;
    m_drivedata[drive].Reset();
}

//////////////////////////////////////////////////////////////////////

static WORD Flopy_LastStatus = 0177777;  //DEBUG
WORD CFloppyController::GetState(void)
{
    m_motorcount = 0;

    if (m_pDrive == NULL)
        return FLOPPY_STATUS_RELOAD;  // ��� ������� READY
    if (m_pDrive->fpFile == NULL)
        return FLOPPY_STATUS_RELOAD;  // ��� ������� READY

    if (m_pDrive->dataptr >= FLOPPY_RAWTRACKSIZE - FLOPPY_INDEXLENGTH)
        m_status |= FLOPPY_STATUS_INDEX;
    else
        m_status &= ~FLOPPY_STATUS_INDEX;  // �������� ��������� ���������

    WORD res = m_status;

//#if !defined(PRODUCT)
//    if (Flopy_LastStatus != m_status)
//    {
//        DebugLogFormat(_T("Floppy GET STATE %06o\r\n"), res);
//        Flopy_LastStatus = m_status;
//    }
//#endif

    return res;
}

void CFloppyController::SetTimer(WORD word)
{
    m_timer = ((word & 1) != 0);
    m_timer = TRUE;  m_timercount = 1;
    // ������ RELOAD ������������ ��� ������ � ������� �������, ���� � ���� ������� ������������� ������ READY
    if (m_pDrive != NULL && m_pDrive->fpFile != NULL)
        m_status &= ~FLOPPY_STATUS_RELOAD;
}
WORD CFloppyController::GetTimer()
{
    return m_timer ? 1 : 0;
}

void CFloppyController::SetCommand(WORD cmd)
{
#if !defined(PRODUCT)
	DebugLogFormat(_T("Floppy%d COMMAND %06o\r\n"), m_drive, cmd);
#endif

    m_motorcount = 0;

    BOOL okPrepareTrack = FALSE;  // ����� �� ��������� ������� � �����

    // ���������, �� �������� �� ������� ������
    int newdrive = ((cmd & 03) << 1) | ((cmd & 04) >> 2);
    if (m_drive != newdrive)
    {
        FlushChanges();

        ASSERT(newdrive >= 0 && newdrive < 8);
        m_drive = newdrive;
        m_pDrive = m_drivedata + m_drive;
        okPrepareTrack = TRUE;
#if !defined(PRODUCT)
        DebugLogFormat(_T("Floppy CURRENT DRIVE %d\r\n"), newdrive);
#endif
    }
    cmd &= ~7;  // ������� �� ������� ���������� � ������� �������

    // Copy new flags to m_flags
    m_motoron = ((cmd & 8) != 0);
    cmd &= 060;  // ��������� ������ ��� ��������

    if (okPrepareTrack)
        PrepareTrack();

    m_operation = cmd;  m_opercount = -1;

    if (m_operation == FLOPPY_OPER_WRITE_TRACK)
    {
        m_status &= ~FLOPPY_STATUS_TR;

        // ���� �������� ������, �� ��� TR00/WRPRT �������� ������� ������ �� ������
        if (m_pDrive != NULL && m_pDrive->okReadOnly)
            m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // ������ �� ������ -- WRPRT = 0
        else
            m_status |= FLOPPY_STATUS_TR00_WRPRT;
    }
    else if (m_operation == FLOPPY_OPER_READ_TRACK)
    {
        m_status &= ~FLOPPY_STATUS_TR;
    }
    else if (m_operation == FLOPPY_OPER_STEP_OUT)
    {
        // ������ ��� ���� �������� ������������ ������� ������� �������
        if (m_track == 0)
            m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // ������� �������, TR00 = 0
        else
            m_status |= FLOPPY_STATUS_TR00_WRPRT;
    }
}

void CFloppyController::SetState(WORD data)
{
#if !defined(PRODUCT)
	DebugLogFormat(_T("Floppy%d SET STATE %d OPER %06o\r\n"), m_drive, (int)data, m_operation);
#endif
    m_motorcount = 0;

    if (data & 1)  // Run the operation
    {
        switch (m_operation)
        {
        case FLOPPY_OPER_NOOPERATION:
            break;
        case FLOPPY_OPER_STEP_OUT:
        case FLOPPY_OPER_STEP_IN:
            m_status |= FLOPPY_STATUS_TR;  // ��������������� ������ ��� ��������� ��� ������ � ��� �����
            m_opercount = 2500 / 64;  // Track-to-track time less than 3 ms
            break;
        case FLOPPY_OPER_READ_TRACK:
        case FLOPPY_OPER_WRITE_TRACK:
            m_opercount = -2;  // Seek for the track start
            m_status &= ~FLOPPY_STATUS_TR;
            break;
        }
    }
    else  // Stop the current operation
    {
        m_opercount = 0;
        m_operation = FLOPPY_OPER_NOOPERATION;
        m_status &= ~FLOPPY_STATUS_TR;
    }
}

WORD CFloppyController::GetData(void)
{
    m_motorcount = 0;

    m_writeflag = m_shiftflag = FALSE;

    if (m_pDrive == NULL || m_pDrive->fpFile == NULL)
        return 0;

#if !defined(PRODUCT)
    WORD offset = m_pDrive->dataptr;
    if (offset >= 10 && (offset - 10) % 130 == 0)
        DebugLogFormat(_T("Floppy%d READ %02x POS%04d SC%02d TR%02d\r\n"), m_drive, m_datareg, offset, (offset - 10) / 130, m_track);
#endif

    m_status &= ~FLOPPY_STATUS_TR;  // TR ������������ ��� ������ �������� ������

    return m_datareg;
}

void CFloppyController::WriteData(WORD data)
{
#if !defined(PRODUCT)
    DebugLogFormat(_T("Floppy%d WRITE %02x POS%04d\r\n"), m_drive, data, m_pDrive->dataptr);  //DEBUG
#endif
    m_motorcount = 0;

    if (m_operation == FLOPPY_OPER_WRITE_TRACK &&
        m_opercount == -3 &&
        data == 0363)  // ������ ������
    {
        m_opercount = -4;  // ��������� ��������������� � ������ �� �������
        m_pDrive->dataptr = FLOPPY_RAWTRACKSIZE - 1;  //HACK: ����� ������ ��� � ����� ������ �������
    }
    if (m_operation == FLOPPY_OPER_WRITE_TRACK &&
        m_opercount == -4)
    {
        if (!m_writeflag && !m_shiftflag)  // Both registers are empty
        {
            m_shiftreg = data & 0xff;
            m_shiftflag = TRUE;
            //m_status |= FLOPPY_STATUS_MOREDATA;
        }
        else if (!m_writeflag && m_shiftflag)  // Write register is empty
        {
            m_writereg = data & 0xff;
            m_writeflag = TRUE;
            //m_status &= ~FLOPPY_STATUS_MOREDATA;
        }
        else if (m_writeflag && !m_shiftflag)  // Shift register is empty
        {
            m_shiftreg = m_writereg;
            m_shiftflag = m_writeflag;
            m_writereg = data & 0xff;
            m_writeflag = TRUE;
            //m_status &= ~FLOPPY_STATUS_MOREDATA;
        }
        else  // Both registers are not empty
        {
            m_writereg = data & 0xff;
        }
    }
}

void CFloppyController::Periodic()
{
    // Timer
    if (m_timercount > 0)
    {
        m_timercount += 64;
        if (m_timercount > 3000)
        {
            m_timer = FALSE;
            m_timercount = 0;
        }
    }

    if (!IsEngineOn()) return;  // ������� ������� ������ ���� ������� �����

    m_motorcount++;
    if (m_motorcount > 125000)  // 8 S = 8000000 uS; 8000000 uS / 64 uS = 125000
    {
        m_motoron = FALSE;  // Turn motor OFF by timeout
#if !defined(PRODUCT)
        DebugLog(_T("Floppy Motor OFF\n"));
#endif
        return;
    }

    // ������� ������� �� ���� ������� �����
    for (int drive = 0; drive < 8; drive++)
    {
        m_drivedata[drive].dataptr += 1;
        if (m_drivedata[drive].dataptr >= FLOPPY_RAWTRACKSIZE)
            m_drivedata[drive].dataptr = 0;
    }
//#if !defined(PRODUCT)
//    if (m_pDrive != NULL && m_pDrive->dataptr == 0)
//        DebugLogFormat(_T("Floppy Index\n"));
//#endif

    // ����� ������������ �������� �� ������� ������
    if (m_pDrive == NULL) return;
    if (!IsAttached(m_drive)) return;

    if (m_opercount == 0) return;  // ��� ������� ��������
    if (m_opercount == -1) return;  // �������� ������, �� ���� �� ��������

    if (m_opercount > 0)  // �������� � ��������
    {
        m_opercount--;
        if (m_opercount == 0)
        {
            if (m_operation == FLOPPY_OPER_STEP_IN)  // ��� � ������ �������
            {
                #if !defined(PRODUCT)
                DebugLogFormat(_T("Floppy%d STEP IN\r\n"), m_drive);
                #endif
                if (m_track < 82) { m_track++;  PrepareTrack(); }
            }
            else if (m_operation == FLOPPY_OPER_STEP_OUT)  // ��� �� ������ �������
            {
                #if !defined(PRODUCT)
                DebugLogFormat(_T("Floppy%d STEP OUT\r\n"), m_drive);
                #endif
                if (m_track >= 1) { m_track--;  PrepareTrack(); }
                // ������ ��� ���� �������� ������������ ������� ������� �������
                if (m_track == 0)
                {
                    m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // ������� �������, TR00 = 0
                    #if !defined(PRODUCT)
                    DebugLog(_T("Floppy TRACK 00\r\n"));
                    #endif
                }
                else
                    m_status |= FLOPPY_STATUS_TR00_WRPRT;
            }
        }
    }
    else if (m_opercount == -2)  // ����� ������ �������
    {
        if (m_operation == FLOPPY_OPER_READ_TRACK && m_pDrive->dataptr == 0)
        {
            m_datareg = m_pDrive->data[m_pDrive->dataptr];
            m_status |= FLOPPY_STATUS_TR;
            m_opercount = -3;  // �������� ������ � ��������
        }
        // ��� ������ ����� ������ ������� RUN ��� TR ������������ �� ����������� ������ �������
        else if (m_operation == FLOPPY_OPER_WRITE_TRACK && m_pDrive->dataptr == 0)
        {
            m_status |= FLOPPY_STATUS_TR;
            m_opercount = -3;  // ������ ��� ����������� ������� 363 � ������� ������
        }
    }
    else if (m_opercount == -3 || m_opercount == -4)  // �������� � ��������
    {
        if (m_operation == FLOPPY_OPER_READ_TRACK)  // ������ �����
        {
            m_datareg = m_pDrive->data[m_pDrive->dataptr];
            m_status |= FLOPPY_STATUS_TR;
        }
        else if (m_operation == FLOPPY_OPER_WRITE_TRACK)  // ����� �����
        {
            if (m_opercount == -4)
            {
                m_pDrive->data[m_pDrive->dataptr] = (BYTE)m_shiftreg;
                m_trackchanged = TRUE;
                m_shiftreg = m_writereg;  m_shiftflag = m_writeflag;  m_writeflag = FALSE;
            }
            m_status |= FLOPPY_STATUS_TR;
        }
    }
}

// Read track data from file and fill m_data
void CFloppyController::PrepareTrack()
{
    FlushChanges();

    if (m_pDrive == NULL) return;

#if !defined(PRODUCT)
    DebugLogFormat(_T("Floppy%d PREPARE TRACK %d\r\n"), m_drive, m_track);
#endif

    DWORD count;

    m_trackchanged = FALSE;
    m_pDrive->dataptr = 0;
    m_pDrive->datatrack = m_track;

    // Track has 23 sectors, 128 bytes each; track 0 has only 22 data sectors
    long foffset = 0;
    int sectors = 22;
    if (m_track > 0)
    {
        foffset = (m_track * 23 - 1) * 128;
        sectors = 23;
    }

    BYTE data[23 * 128];
    memset(data, 0, 23 * 128);

    if (m_pDrive->fpFile != NULL)
    {
        ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
        count = ::fread(&data, 1, sectors * 128, m_pDrive->fpFile);
        //TODO: �������� ������ ������
    }

    // Fill m_data array with data
    EncodeTrackData(data, m_pDrive->data, m_track, 0);

    //FILE* fpTrack = ::_tfopen(_T("RawTrack.bin"), _T("w+b"));
    //::fwrite(m_pDrive->data, 1, FLOPPY_RAWTRACKSIZE, fpTrack);
    //::fclose(fpTrack);

//    //DEBUG: Test DecodeTrackData()
//    BYTE data2[128*23];
//    BOOL parsed = DecodeTrackData(m_pDrive->data, data2, m_track);
//    ASSERT(parsed);
//    BOOL tested = TRUE;
//    for (int i = 0; i < 128*sectors; i++)
//        if (data[i] != data2[i])
//        {
//            tested = FALSE;
//            break;
//        }
//    ASSERT(tested);
}

void CFloppyController::FlushChanges()
{
    if (m_drive == -1) return;
    if (!IsAttached(m_drive)) return;
    if (!m_trackchanged) return;

#if !defined(PRODUCT)
    DebugLogFormat(_T("Floppy%d FLUSH\r\n"), m_drive);  //DEBUG
#endif

    // Decode track data from m_data
    BYTE data[128*23];  memset(data, 0, 128*23);
    BOOL decoded = DecodeTrackData(m_pDrive->data, data, m_pDrive->datatrack);

    if (decoded)  // Write to the file only if the track was correctly decoded from raw data
    {
        // Track has 23 sectors, 128 bytes each; track 0 has only 22 data sectors
        long foffset = 0;
        int sectors = 22;
        if (m_pDrive->datatrack > 0)
        {
            foffset = (m_pDrive->datatrack * 23 - 1) * 128;
            sectors = 23;
        }

//        // Check file length
//        ::fseek(m_pDrive->fpFile, 0, SEEK_END);
//        DWORD currentFileSize = ::ftell(m_pDrive->fpFile);
//        while (currentFileSize < (DWORD)(foffset + 5120))
//        {
//            BYTE datafill[512];  ::memset(datafill, 0, 512);
//            DWORD bytesToWrite = ((DWORD)(foffset + 5120) - currentFileSize) % 512;
//            if (bytesToWrite == 0) bytesToWrite = 512;
//            ::fwrite(datafill, 1, bytesToWrite, m_pDrive->fpFile);
//            //TODO: �������� �� ������ ������
//            currentFileSize += bytesToWrite;
//        }

        // Save data into the file
        ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
        DWORD dwBytesWritten = ::fwrite(&data, 1, 128*sectors, m_pDrive->fpFile);
        //TODO: �������� �� ������ ������
    }
    else {
#if !defined(PRODUCT)
        DebugLog(_T("Floppy FLUSH FAILED\r\n"));  //DEBUG
#endif
    }

    m_trackchanged = FALSE;
}


//////////////////////////////////////////////////////////////////////


WORD CalculateChecksum(const BYTE* buffer, int length)
{
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    WORD sum = 0;
    while (length > 0)
    {
        WORD src = *buffer;
        if (src & 0200) src |= 0177400;  // ���������� ��������� ����
        signed short dst = sum + src;
        sum = dst;

        buffer++;
        length--;
    }
    return sum;
}

// Fill data array and marker array with marked data
// pSrc array length is 23 * 128, for track 0 is 22 * 128
// data array length is 3125 == FLOPPY_RAWTRACKSIZE
static void EncodeTrackData(const BYTE* pSrc, BYTE* data, WORD track, WORD side)
{
    memset(data, 0, FLOPPY_RAWTRACKSIZE);
    DWORD count;
    int ptr = 0;

    // Track header, 10 bytes
    BYTE* checksumStart = data + ptr;
    data[ptr++] = 0363;  // Marker
    data[ptr++] = (BYTE)track;  // Track number
    data[ptr++] = 23;  // Sectors on the track
    data[ptr++] = 0;  // ???
    WORD firstSector = track * 23;
    data[ptr++] = (BYTE)(firstSector & 0xff);
    data[ptr++] = (BYTE)((firstSector & 0xff00) >> 8);
    data[ptr++] = 0xff;  // ???
    data[ptr++] = 0;  // ???
    WORD checksum = CalculateChecksum(checksumStart, 8);
    data[ptr++] = (checksum & 0xff);  data[ptr++] = ((checksum & 0xff00) >> 8);

    int sectors = 23;
    if (track == 0)  // Special sector 1 for track 0
    {
        sectors = 22;

        checksumStart = data + ptr;
        for (int i = 0; i < 80; i++)
            data[ptr++] = 23;
        for (int i = 0; i < 128 - 80; i++)
            data[ptr++] = 0xff;
        checksum = CalculateChecksum(checksumStart, 128);
        data[ptr++] = (checksum & 0xff);  data[ptr++] = ((checksum & 0xff00) >> 8);
    }
    for (int sect = 0; sect < sectors; sect++)
    {
        // Sector data
        checksumStart = data + ptr;
        for (count = 0; count < 128; count++)
            data[ptr++] = pSrc[(sect * 128) + count];
        checksum = CalculateChecksum(checksumStart, 128);
        data[ptr++] = (checksum & 0xff);  data[ptr++] = ((checksum & 0xff00) >> 8);
    }
    // Fill to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) data[ptr++] = 0x4e;
}

// Decode track data from raw data
// pRaw is array of FLOPPY_RAWTRACKSIZE bytes
// pDest is array of 128*23 bytes
// Returns: TRUE - decoded, FALSE - parse error
static BOOL DecodeTrackData(const BYTE* pRaw, BYTE* pDest, WORD track)
{
    WORD dataptr = 0;  // Offset in m_data array
    WORD destptr = 0;  // Offset in data array

    if (pRaw[dataptr++] != 0363) return FALSE;  // Marker not found
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Track Number
    if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] <= 23) dataptr++;  // Skip Sectors on Track
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip ??
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip First Sector
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip First Sector
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip ??
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip ??
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum

    for (int sect = 0; sect < 23/*TODO*/; sect++)  // Copy sectors
    {
        for (int count = 0; count < 128; count++)
        {
            if (track != 0 || sect != 0)
            {
                if (destptr >= 128*23)
                    return FALSE;  // End of track or error
                pDest[destptr++] = pRaw[dataptr];
            }
            dataptr++;
            if (dataptr >= FLOPPY_RAWTRACKSIZE)
                return FALSE;  // Something wrong
        }
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
    }
    if (dataptr >= FLOPPY_RAWTRACKSIZE)
        return FALSE;  // Something wrong

    return TRUE;
}


//////////////////////////////////////////////////////////////////////

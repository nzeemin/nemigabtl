/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Floppy.cpp
// Floppy controller and drives emulation
// See defines in header file Emubase.h

#include "stdafx.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////

static void EncodeTrackData(const uint8_t* pSrc, uint8_t* data, uint16_t track, uint16_t side);
static bool DecodeTrackData(const uint8_t* pRaw, uint8_t* pDest, uint16_t track);
static void EncodeTrackDataMX(const uint8_t* pSrc, uint8_t* data, uint16_t track, uint16_t side);
static bool DecodeTrackDataMX(const uint8_t* pRaw, uint8_t* pDest, uint16_t track);

//////////////////////////////////////////////////////////////////////


CFloppyDrive::CFloppyDrive()
{
    fpFile = NULL;
    okReadOnly = false;
    floppytype = FLOPPY_TYPE_NONE;
    datatrack = 0;
    dataptr = 0;
    memset(data, 0, sizeof(data));
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
    m_timer = true;  m_timercount = 0;
    m_motoron = false;  m_motorcount = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = false;
    m_trackchanged = false;
    m_status = 0;
    m_okTrace = false;
}

CFloppyController::~CFloppyController()
{
    for (int drive = 0; drive < 8; drive++)
        DetachImage(drive);
}

void CFloppyController::Reset()
{
    if (m_okTrace) DebugLog(_T("Floppy RESET\r\n"));

    FlushChanges();

    m_drive = -1;  m_pDrive = NULL;
    m_track = 0;
    m_timer = true;  m_timercount = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = false;
    m_trackchanged = false;
    m_status = FLOPPY_STATUS_TR;
}

bool CFloppyController::AttachImage(int drive, LPCTSTR sFileName, uint8_t floppyType)
{
    ASSERT(drive >= 0 && drive < 4);
    ASSERT(sFileName != NULL);
    ASSERT(floppyType == FLOPPY_TYPE_MD || floppyType == FLOPPY_TYPE_MX);
    if (floppyType == FLOPPY_TYPE_MX)
        ASSERT(drive == 0 || drive == 2);

    // If image attached - detach one first
    if (m_drivedata[drive].fpFile != NULL)
    {
        DetachImage(drive);
        if (floppyType == FLOPPY_TYPE_MX)
            DetachImage(drive + 1);
    }

    // Open file
    m_drivedata[drive].floppytype = floppyType;
    m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("r+b"));
    if (m_drivedata[drive].fpFile == NULL)
    {
        m_drivedata[drive].okReadOnly = true;
        m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("rb"));
    }
    if (m_drivedata[drive].fpFile == NULL)
        return false;

    // For MX drive, soft-attach the other side
    if (floppyType == FLOPPY_TYPE_MX)
    {
        m_drivedata[drive + 1].floppytype = FLOPPY_TYPE_MX;
        m_drivedata[drive + 1].okReadOnly = m_drivedata[drive].okReadOnly;
        m_drivedata[drive + 1].fpFile = m_drivedata[drive].fpFile;
    }

    m_track = m_drivedata[drive].datatrack = 0;
    m_drivedata[drive].dataptr = 0;
    m_datareg = m_writereg = m_shiftreg = 0;
    m_writeflag = m_shiftflag = false;
    m_trackchanged = false;
    m_status = 0;
    m_operation = FLOPPY_OPER_NOOPERATION; m_opercount = 0;

    PrepareTrack();

    return true;
}

void CFloppyController::DetachImage(int drive)
{
    ASSERT(drive >= 0 && drive < 8);

    // For MX drive, soft-detach other side first
    if (m_drivedata[drive].floppytype == FLOPPY_TYPE_MX && (drive & 1) == 0)
    {
        m_drivedata[drive + 1].floppytype = FLOPPY_TYPE_NONE;
        m_drivedata[drive + 1].fpFile = NULL;
    }

    m_drivedata[drive].floppytype = FLOPPY_TYPE_NONE;

    if (m_drivedata[drive].fpFile == NULL) return;

    FlushChanges();

    ::fclose(m_drivedata[drive].fpFile);
    m_drivedata[drive].fpFile = NULL;
    m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].Reset();
}

//////////////////////////////////////////////////////////////////////

static uint16_t Flopy_LastStatus = 0177777;  //DEBUG
uint16_t CFloppyController::GetState(void)
{
    m_motorcount = 0;

    if (m_pDrive == NULL)
        return FLOPPY_STATUS_RELOAD;  // Нет сигнала READY
    if (m_pDrive->fpFile == NULL)
        return FLOPPY_STATUS_RELOAD;  // Нет сигнала READY

    if (m_pDrive->dataptr >= FLOPPY_RAWTRACKSIZE - FLOPPY_INDEXLENGTH)
        m_status |= FLOPPY_STATUS_INDEX;
    else
        m_status &= ~FLOPPY_STATUS_INDEX;  // Проходим индексное отверстие

    uint16_t res = m_status;

//    if (m_okTrace && Flopy_LastStatus != m_status)
//    {
//        DebugLogFormat(_T("Floppy GET STATE %06o\r\n"), res);
//        Flopy_LastStatus = m_status;
//    }

    return res;
}

void CFloppyController::SetTimer(uint16_t word)
{
    m_timer = ((word & 1) != 0);
    m_timer = true;  m_timercount = 1;
    // Сигнал RELOAD сбрасывается при записи в регистр таймера, если к тому времени восстановился сигнал READY
    if (m_pDrive != NULL && m_pDrive->fpFile != NULL)
        m_status &= ~FLOPPY_STATUS_RELOAD;
}
uint16_t CFloppyController::GetTimer()
{
    return m_timer ? 1 : 0;
}

void CFloppyController::SetCommand(uint16_t cmd)
{
    if (m_okTrace) DebugLogFormat(_T("Floppy%d COMMAND %06o\r\n"), m_drive, cmd);

    m_motorcount = 0;

    bool okPrepareTrack = false;  // Нужно ли считывать дорожку в буфер

    // Проверить, не сменился ли текущий привод
    int newdrive = ((cmd & 03) << 1) | ((cmd & 04) >> 2);
    if (m_drive != newdrive)
    {
        FlushChanges();

        ASSERT(newdrive >= 0 && newdrive < 8);
        m_drive = newdrive;
        m_pDrive = m_drivedata + m_drive;
        okPrepareTrack = true;

        if (m_okTrace) DebugLogFormat(_T("Floppy CURRENT DRIVE %d\r\n"), newdrive);
    }
    cmd &= ~7;  // Убираем из команды информацию о текущем приводе

    // Copy new flags to m_flags
    m_motoron = ((cmd & 8) != 0);
    cmd &= 060;  // Оставляем только код операции

    if (okPrepareTrack)
        PrepareTrack();

    m_operation = cmd;  m_opercount = -1;

    if (m_operation == FLOPPY_OPER_WRITE_TRACK)
    {
        m_status &= ~FLOPPY_STATUS_TR;

        // Если операция ЗАПИСЬ, то бит TR00/WRPRT содержит признак защиты от записи
        if (m_pDrive != NULL && m_pDrive->okReadOnly)
            m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // Защита от записи -- WRPRT = 0
        else
            m_status |= FLOPPY_STATUS_TR00_WRPRT;
    }
    else if (m_operation == FLOPPY_OPER_READ_TRACK)
    {
        m_status &= ~FLOPPY_STATUS_TR;
    }
    else if (m_operation == FLOPPY_OPER_STEP_OUT)
    {
        // Только для этой операции выставляется признак нулевой дорожки
        if (m_track == 0)
            m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // Нулевая дорожка, TR00 = 0
        else
            m_status |= FLOPPY_STATUS_TR00_WRPRT;
    }
}

void CFloppyController::SetState(uint16_t data)
{
    if (m_okTrace) DebugLogFormat(_T("Floppy%d SET STATE %d OPER %06o\r\n"), m_drive, (int)data, m_operation);

    m_motorcount = 0;

    if (data & 1)  // Run the operation
    {
        switch (m_operation)
        {
        case FLOPPY_OPER_NOOPERATION:
            break;
        case FLOPPY_OPER_STEP_OUT:
        case FLOPPY_OPER_STEP_IN:
            m_status |= FLOPPY_STATUS_TR;  // Устанавливается всегда при операциях ШАГ ВПЕРЕД и ШАГ НАЗАД
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

uint16_t CFloppyController::GetData(void)
{
    m_motorcount = 0;

    m_writeflag = m_shiftflag = false;

    if (m_pDrive == NULL || m_pDrive->fpFile == NULL)
        return 0;

    uint16_t offset = m_pDrive->dataptr;
    if (m_okTrace && offset >= 10 && (offset - 10) % 130 == 0)
        DebugLogFormat(_T("Floppy%d READ %02x POS%04d SC%02d TR%02d\r\n"), m_drive, m_datareg, offset, (offset - 10) / 130 + 1, m_track);

    m_status &= ~FLOPPY_STATUS_TR;  // TR сбрасывается при чтении регистра данных

    return m_datareg;
}

void CFloppyController::WriteData(uint16_t data)
{
    uint16_t offset = m_pDrive->dataptr;
    if (m_okTrace && offset >= 10 && (offset - 10) % 130 == 0)
        DebugLogFormat(_T("Floppy%d WRITE %02x POS%04d SC%02d TR%02d\r\n"), m_drive, data, m_pDrive->dataptr, (offset - 10) / 130 + 1, m_track);

    m_motorcount = 0;

    if (m_operation == FLOPPY_OPER_WRITE_TRACK &&
        m_opercount == -3 &&
        data == 0363)  // Пришел маркер
    {
        m_opercount = -4;  // Переходим непосредственно к записи на дорожку
        m_pDrive->dataptr = FLOPPY_RAWTRACKSIZE - 1;  //HACK: чтобы маркер был в самом начале дорожки
    }
    if (m_operation == FLOPPY_OPER_WRITE_TRACK &&
        m_opercount == -4)
    {
        if (!m_writeflag && !m_shiftflag)  // Both registers are empty
        {
            m_shiftreg = data & 0xff;
            m_shiftflag = true;
            //m_status |= FLOPPY_STATUS_MOREDATA;
        }
        else if (!m_writeflag && m_shiftflag)  // Write register is empty
        {
            m_writereg = data & 0xff;
            m_writeflag = true;
            //m_status &= ~FLOPPY_STATUS_MOREDATA;
        }
        else if (m_writeflag && !m_shiftflag)  // Shift register is empty
        {
            m_shiftreg = m_writereg;
            m_shiftflag = m_writeflag;
            m_writereg = data & 0xff;
            m_writeflag = true;
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
            m_timer = false;
            m_timercount = 0;
        }
    }

    if (!IsEngineOn()) return;  // Вращаем дискеты только если включен мотор

    m_motorcount++;
    if (m_motorcount > 125000)  // 8 S = 8000000 uS; 8000000 uS / 64 uS = 125000
    {
        m_motoron = false;  // Turn motor OFF by timeout

        if (m_okTrace) DebugLog(_T("Floppy Motor OFF\n"));
        return;
    }

    // Вращаем дискеты во всех драйвах сразу
    for (int drive = 0; drive < 8; drive++)
    {
        m_drivedata[drive].dataptr += 1;
        if (m_drivedata[drive].dataptr >= FLOPPY_RAWTRACKSIZE)
            m_drivedata[drive].dataptr = 0;
    }

//    if (m_okTrace && m_pDrive != NULL && m_pDrive->dataptr == 0)
//        DebugLogFormat(_T("Floppy Index\n"));

    // Далее обрабатываем операции на текущем драйве
    if (m_pDrive == NULL) return;
    if (m_pDrive->fpFile == NULL) return;

    if (m_opercount == 0) return;  // Нет текущей операции
    if (m_opercount == -1) return;  // Операция задана, но пока не запущена

    if (m_opercount > 0)  // Операция в процессе
    {
        m_opercount--;
        if (m_opercount == 0)
        {
            if (m_operation == FLOPPY_OPER_STEP_IN)  // Шаг к центру дискеты
            {
                if (m_okTrace) DebugLogFormat(_T("Floppy%d STEP IN\r\n"), m_drive);

                if (m_track < 82) { m_track++;  PrepareTrack(); }
            }
            else if (m_operation == FLOPPY_OPER_STEP_OUT)  // Шаг от центра дискеты
            {
                if (m_okTrace) DebugLogFormat(_T("Floppy%d STEP OUT\r\n"), m_drive);

                if (m_track >= 1) { m_track--;  PrepareTrack(); }
                // Только для этой операции выставляется признак нулевой дорожки
                if (m_track == 0)
                {
                    m_status &= ~FLOPPY_STATUS_TR00_WRPRT;  // Нулевая дорожка, TR00 = 0

                    if (m_okTrace) DebugLog(_T("Floppy TRACK 00\r\n"));
                }
                else
                    m_status |= FLOPPY_STATUS_TR00_WRPRT;
            }
        }
    }
    else if (m_opercount == -2)  // Поиск начала дорожки
    {
        if (m_operation == FLOPPY_OPER_READ_TRACK && m_pDrive->dataptr == 0)
        {
            m_datareg = m_pDrive->data[m_pDrive->dataptr];
            m_status |= FLOPPY_STATUS_TR;
            m_opercount = -3;  // Операция чтения в процессе
        }
        // При ЗАПИСЬ после выдачи команды RUN бит TR сбрасывается до физического начала дорожки
        else if (m_operation == FLOPPY_OPER_WRITE_TRACK && m_pDrive->dataptr == 0)
        {
            m_status |= FLOPPY_STATUS_TR;
            m_opercount = -3;  // Теперь ждём поступления маркера 363 в регистр данных
        }
    }
    else if (m_opercount == -3 || m_opercount == -4)  // Операция в процессе
    {
        if (m_operation == FLOPPY_OPER_READ_TRACK)  // Читаем байты
        {
            m_datareg = m_pDrive->data[m_pDrive->dataptr];
            m_status |= FLOPPY_STATUS_TR;
        }
        else if (m_operation == FLOPPY_OPER_WRITE_TRACK)  // Пишем байты
        {
            if (m_opercount == -4)
            {
                m_pDrive->data[m_pDrive->dataptr] = (uint8_t)m_shiftreg;
                m_trackchanged = true;
                m_shiftreg = m_writereg;  m_shiftflag = m_writeflag;  m_writeflag = false;
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
    if (m_pDrive->fpFile == NULL) return;

    if (m_okTrace) DebugLogFormat(_T("Floppy%d PREPARE TRACK %d\r\n"), m_drive, m_track);

    uint32_t count;

    m_trackchanged = false;
    m_pDrive->dataptr = 0;
    m_pDrive->datatrack = m_track;

    uint8_t data[23 * 128];
    memset(data, 0, 23 * 128);

    if (m_pDrive->floppytype == FLOPPY_TYPE_MD)
    {
        // Track has 23 sectors, 128 bytes each; track 0 has only 22 data sectors
        long foffset = 0;
        int sectors = 22;
        if (m_track > 0)
        {
            foffset = (m_track * 23 - 1) * 128;
            sectors = 23;
        }
        if (m_pDrive->fpFile != NULL)
        {
            ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
            count = ::fread(&data, 1, sectors * 128, m_pDrive->fpFile);
            //TODO: Контроль ошибок чтения
        }

        // Fill m_data array with data
        EncodeTrackData(data, m_pDrive->data, m_track, 0);
    }
    else if (m_pDrive->floppytype == FLOPPY_TYPE_MX)
    {
        // Track has 11 sectors, 256 bytes each
        int side = m_drive & 1;
        long foffset = (m_track * 2 + side) * 11 * 256;
        ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
        count = ::fread(&data, 1, 11 * 256, m_pDrive->fpFile);

        // Fill m_data array with data
        EncodeTrackDataMX(data, m_pDrive->data, m_track, 0);
    }

    //FILE* fpTrack = ::_tfopen(_T("RawTrack.bin"), _T("w+b"));
    //::fwrite(m_pDrive->data, 1, FLOPPY_RAWTRACKSIZE, fpTrack);
    //::fclose(fpTrack);

//    //DEBUG: Test DecodeTrackData()
//    uint8_t data2[128*23];
//    bool parsed = DecodeTrackData(m_pDrive->data, data2, m_track);
//    ASSERT(parsed);
//    bool tested = true;
//    for (int i = 0; i < 128*sectors; i++)
//        if (data[i] != data2[i])
//        {
//            tested = false;
//            break;
//        }
//    ASSERT(tested);
}

void CFloppyController::FlushChanges()
{
    if (m_drive == -1) return;
    if (m_pDrive->fpFile == NULL) return;
    if (!m_trackchanged) return;

    if (m_okTrace) DebugLogFormat(_T("Floppy%d FLUSH\r\n"), m_drive);  //DEBUG

    //TCHAR filename[32];  wsprintf(filename, _T("rawtrack%02d.bin"), (int)m_pDrive->datatrack);
    //FILE* fpTrack = ::_tfopen(filename, _T("w+b"));
    //::fwrite(m_pDrive->data, 1, FLOPPY_RAWTRACKSIZE, fpTrack);
    //::fclose(fpTrack);

    // Decode track data from m_data
    uint8_t data[128 * 23];  memset(data, 0, 128 * 23);

    //TCHAR filename[32];  wsprintf(filename, _T("rawtrack%d-%02d.bin"), (int)m_drive, (int)m_pDrive->datatrack);
    //FILE* fpTrack = ::_tfopen(filename, _T("w+b"));
    //::fwrite(m_pDrive->data, 1, FLOPPY_RAWTRACKSIZE, fpTrack);
    //::fclose(fpTrack);

    if (m_pDrive->floppytype == FLOPPY_TYPE_MD)
    {
        bool decoded = DecodeTrackData(m_pDrive->data, data, m_pDrive->datatrack);
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
            // Save data into the file
            ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
            uint32_t dwBytesWritten = ::fwrite(&data, 1, 128 * sectors, m_pDrive->fpFile);
            //TODO: Проверка на ошибки записи
        }
        else
        {
            if (m_okTrace) DebugLog(_T("Floppy FLUSH MD FAILED\r\n"));  //DEBUG
        }
    }
    else if (m_pDrive->floppytype == FLOPPY_TYPE_MX)
    {
        bool decoded = DecodeTrackDataMX(m_pDrive->data, data, m_pDrive->datatrack);
        if (decoded)  // Write to the file only if the track was correctly decoded from raw data
        {
            // Track has 11 sectors, 256 bytes each
            int side = m_drive & 1;
            long foffset = (m_track * 2 + side) * 11 * 256;
            // Save data into the file
            ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
            uint32_t dwBytesWritten = ::fwrite(&data, 1, 256 * 11, m_pDrive->fpFile);
            //TODO: Проверка на ошибки записи
        }
        else
        {
            if (m_okTrace) DebugLog(_T("Floppy FLUSH MX FAILED\r\n"));  //DEBUG
        }
    }

//        // Check file length
//        ::fseek(m_pDrive->fpFile, 0, SEEK_END);
//        uint32_t currentFileSize = ::ftell(m_pDrive->fpFile);
//        while (currentFileSize < (uint32_t)(foffset + 5120))
//        {
//            uint8_t datafill[512];  ::memset(datafill, 0, 512);
//            uint32_t bytesToWrite = ((uint32_t)(foffset + 5120) - currentFileSize) % 512;
//            if (bytesToWrite == 0) bytesToWrite = 512;
//            ::fwrite(datafill, 1, bytesToWrite, m_pDrive->fpFile);
//            //TODO: Проверка на ошибки записи
//            currentFileSize += bytesToWrite;
//        }

    m_trackchanged = false;
}


//////////////////////////////////////////////////////////////////////


uint16_t CalculateChecksum(const uint8_t* buffer, int length)
{
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    uint16_t sum = 0;
    while (length > 0)
    {
        uint16_t src = *buffer;
        if (src & 0200) src |= 0177400;  // Расширение знакового бита
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
static void EncodeTrackData(const uint8_t* pSrc, uint8_t* data, uint16_t track, uint16_t /*side*/)
{
    memset(data, 0, FLOPPY_RAWTRACKSIZE);
    uint32_t count;
    int ptr = 0;

    // Track header, 10 bytes
    uint8_t* checksumStart = data + ptr;
    data[ptr++] = 0363;  // Marker
    data[ptr++] = (uint8_t)track;  // Track number
    data[ptr++] = 23;    // Sectors on the track
    data[ptr++] = 0;     // Byte used for XOR reading
    uint16_t firstSector = track * 23;
    data[ptr++] = (uint8_t)(firstSector & 0xff);
    data[ptr++] = (uint8_t)((firstSector & 0xff00) >> 8);
    data[ptr++] = 0xff;  // Byte used for XOR reading
    data[ptr++] = 0xff;  // Byte used for XOR reading
    uint16_t checksum = CalculateChecksum(checksumStart, 8);
    data[ptr++] = (checksum & 0xff);  data[ptr++] = ((checksum & 0xff00) >> 8);

    int sectors = 23;
    if (track == 0)  // Special sector 1 for track 0
    {
        sectors = 22;

        checksumStart = data + ptr;
        for (int i = 0; i < 80; i++)
            data[ptr++] = 23;
        data[ptr++] = 0x30;
        data[ptr++] = 0x07;
        for (int i = 0; i < 128 - 82; i++)
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
// Returns: true - decoded, false - parse error
static bool DecodeTrackData(const uint8_t* pRaw, uint8_t* pDest, uint16_t track)
{
    uint16_t dataptr = 0;  // Offset in m_data array
    uint16_t destptr = 0;  // Offset in data array

    if (pRaw[dataptr++] != 0363) return false;  // Marker not found
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
                if (destptr >= 128 * 23)
                    return false;  // End of track or error
                pDest[destptr++] = pRaw[dataptr];
            }
            dataptr++;
            if (dataptr >= FLOPPY_RAWTRACKSIZE)
                return false;  // Something wrong
        }
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
    }
    if (dataptr >= FLOPPY_RAWTRACKSIZE)
        return false;  // Something wrong

    return true;
}

// Fill data array and marker array with marked data for MX disk
// pSrc array length is 11 * 256
// data array length is 3125 == FLOPPY_RAWTRACKSIZE
static void EncodeTrackDataMX(const uint8_t* pSrc, uint8_t* data, uint16_t track, uint16_t /*side*/)
{
    memset(data, 0, FLOPPY_RAWTRACKSIZE);
    uint32_t count;
    int ptr = 0;

    data[ptr++] = 0363;
    data[ptr++] = 0;
    data[ptr++] = (uint8_t)track;

    for (int sect = 0; sect < 11; sect++)
    {
        // Sector data
        uint16_t checksum = 0;
        for (count = 0; count < 128; count++)
        {
            uint8_t lovalue = pSrc[(sect * 256) + count * 2];
            uint8_t hivalue = pSrc[(sect * 256) + count * 2 + 1];
            data[ptr++] = hivalue;
            data[ptr++] = lovalue;
            checksum += ((uint16_t)lovalue) | (((uint16_t)hivalue) << 8);
        }
        data[ptr++] = HIBYTE(checksum);
        data[ptr++] = LOBYTE(checksum);
    }
    data[ptr++] = 0x20;
    data[ptr++] = 0x4f;
    data[ptr++] = 0x54;
    data[ptr++] = 0x01;
    data[ptr++] = 0x00;
    data[ptr++] = (uint8_t)(track * 2);

    // Fill to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) data[ptr++] = 0xff;
}

// Decode track data from raw data for MX disk
// pRaw is array of FLOPPY_RAWTRACKSIZE bytes
// pDest is array of 11*256 = 2816 bytes
// Returns: true - decoded, false - parse error
static bool DecodeTrackDataMX(const uint8_t* pRaw, uint8_t* pDest, uint16_t /*track*/)
{
    uint16_t dataptr = 0;  // Offset in m_data array
    uint16_t destptr = 0;  // Offset in data array

    if (pRaw[dataptr++] != 0363) return false;  // Marker not found
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Track Number hi
    if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Track Number lo

    for (int sect = 0; sect < 11; sect++)  // Copy sectors
    {
        for (int count = 0; count < 128; count++)
        {
            uint8_t hivalue = pRaw[dataptr++];
            uint8_t lovalue = pRaw[dataptr++];
            pDest[destptr++] = lovalue;
            pDest[destptr++] = hivalue;
            if (dataptr >= FLOPPY_RAWTRACKSIZE)
                return false;  // Something wrong
        }
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;  // Skip Checksum
    }
    if (dataptr >= FLOPPY_RAWTRACKSIZE)
        return false;  // Something wrong

    return true;
}


//////////////////////////////////////////////////////////////////////

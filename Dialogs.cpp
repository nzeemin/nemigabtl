/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Dialogs.cpp

#include "stdafx.h"
#include <commdlg.h>
#include <commctrl.h>
#include "Dialogs.h"
#include "Emulator.h"
#include "Main.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InputBoxProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK LoadBinProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void Dialogs_DoLoadBinPrepare(HWND hDlg, LPCTSTR strFileName);
void Dialogs_DoLoadBinLoad(LPCTSTR strFileName);
BOOL InputBoxValidate(HWND hDlg);
INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

LPCTSTR m_strInputBoxTitle = NULL;
LPCTSTR m_strInputBoxPrompt = NULL;
WORD* m_pInputBoxValueOctal = NULL;


//////////////////////////////////////////////////////////////////////
// About Box

void ShowAboutBox()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), g_hwnd, AboutBoxProc);
}

INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            TCHAR buf[64];
            wsprintf(buf, _T("%S %S"), __DATE__, __TIME__);
            ::SetWindowText(::GetDlgItem(hDlg, IDC_BUILDDATE), buf);
            return (INT_PTR)TRUE;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


//////////////////////////////////////////////////////////////////////


BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strPrompt, WORD* pValue)
{
    m_strInputBoxTitle = strTitle;
    m_strInputBoxPrompt = strPrompt;
    m_pInputBoxValueOctal = pValue;
    INT_PTR result = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INPUTBOX), hwndOwner, InputBoxProc);
    if (result != IDOK)
        return FALSE;

    return TRUE;
}


INT_PTR CALLBACK InputBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetWindowText(hDlg, m_strInputBoxTitle);
            HWND hStatic = GetDlgItem(hDlg, IDC_STATIC);
            SetWindowText(hStatic, m_strInputBoxPrompt);
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);

            TCHAR buffer[8];
            _snwprintf_s(buffer, 8, _T("%06o"), *m_pInputBoxValueOctal);
            SetWindowText(hEdit, buffer);
            SendMessage(hEdit, EM_SETSEL, 0, -1);

            SetFocus(hEdit);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (! InputBoxValidate(hDlg))
                return (INT_PTR) FALSE;
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR) FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

BOOL InputBoxValidate(HWND hDlg) {
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);
    TCHAR buffer[8];
    GetWindowText(hEdit, buffer, 8);

    WORD value;
    if (! ParseOctalValue(buffer, &value))
    {
        MessageBox(NULL, _T("Please enter correct octal value."), _T("Input Box Validation"),
                MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        return FALSE;
    }

    *m_pInputBoxValueOctal = value;

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, LPCTSTR strDefExt, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.lpstrDefExt = strDefExt;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetSaveFileName(&ofn);
    return okResult;
}

BOOL ShowOpenDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetOpenFileName(&ofn);
    return okResult;
}


//////////////////////////////////////////////////////////////////////
// Create Disk Dialog

void ShowLoadBinDialog()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_LOADBIN), g_hwnd, LoadBinProc);
}

INT_PTR CALLBACK LoadBinProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            ::PostMessage(hDlg, WM_COMMAND, IDC_BUTTONBROWSE, 0);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTONBROWSE:
            {
                TCHAR bufFileName[MAX_PATH];
                BOOL okResult = ShowOpenDialog(g_hwnd,
                    _T("Select SAV file to load"),
                    _T("BK binary files (*.sav)\0*.sav\0All Files (*.*)\0*.*\0\0"),
                    bufFileName);
                if (! okResult) break;

                Dialogs_DoLoadBinPrepare(hDlg, bufFileName);
            }
            break;
        case IDOK:
            {
                TCHAR bufFileName[MAX_PATH];
                ::GetDlgItemText(hDlg, IDC_EDITFILE, bufFileName, sizeof(bufFileName) / sizeof(TCHAR));
                Dialogs_DoLoadBinLoad(bufFileName);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

void Dialogs_DoLoadBinPrepare(HWND hDlg, LPCTSTR strFileName)
{
    ::SetDlgItemText(hDlg, IDC_EDITFILE, NULL);

    // Open file for reading
    HANDLE hFile = CreateFile(strFileName,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load SAV file."));
        return;
    }
    
    // Load BIN header
    BYTE bufHeader[48];
	DWORD bytesRead;
	::ReadFile(hFile, bufHeader, 48, &bytesRead, NULL);
    if (bytesRead != 48)
    {
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load SAV file."));
        return;
    }

    WORD baseAddress = *((WORD*)(bufHeader + 040));
    WORD dataSize = *((WORD*)(bufHeader + 050)) + 2;
    WORD stackAddress = *((WORD*)(bufHeader + 042));

    // Set controls text
    TCHAR bufValue[8];
    ::SetDlgItemText(hDlg, IDC_EDITFILE, strFileName);
    PrintOctalValue(bufValue, baseAddress);
    ::SetDlgItemText(hDlg, IDC_EDITADDR, bufValue);
    PrintOctalValue(bufValue, dataSize);
    ::SetDlgItemText(hDlg, IDC_EDITSIZE, bufValue);
    PrintOctalValue(bufValue, stackAddress);
    ::SetDlgItemText(hDlg, IDC_EDITSTACK, bufValue);

    ::CloseHandle(hFile);
}

void Dialogs_DoLoadBinLoad(LPCTSTR strFileName)
{
    // Open file for reading
    HANDLE hFile = CreateFile(strFileName,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load SAV file."));
        return;
    }

    // Load BIN header
    BYTE bufHeader[48];
	DWORD bytesRead;
	::ReadFile(hFile, bufHeader, 48, &bytesRead, NULL);
    if (bytesRead != 48)
    {
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load SAV file."));
        return;
    }

    WORD baseAddress = *((WORD*)(bufHeader + 040));
    WORD dataSize = *((WORD*)(bufHeader + 050)) + 2;
    WORD stackAddress = *((WORD*)(bufHeader + 042));

    // Get file size
    DWORD bytesToRead = dataSize;
    WORD memoryBytes = (dataSize + 1) & 0xfffe;

    // Allocate memory
    BYTE* pBuffer = (BYTE*)::LocalAlloc(LPTR, memoryBytes);

    // Load file data
    ::SetFilePointer(hFile, 0, 0, 0/*FILE_BEGIN*/);
	::ReadFile(hFile, pBuffer, dataSize, &bytesRead, NULL);
    if (bytesRead != bytesToRead)
    {
        ::LocalFree(pBuffer);
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load SAV file."));
        return;
    }

    Emulator_Reset();
    Emulator_SetCPUBreakpoint(0162366);
    while (Emulator_SystemFrame()) ;  // Wait for the breakpoint

    // Copy data to the machine memory
    WORD address = 0;
    WORD* pData = (WORD*)(pBuffer + 0);
    while (address < memoryBytes)
    {
        WORD value = *pData++;
        g_pBoard->SetRAMWord(address, value);
        address += 2;
    }

    ::LocalFree(pBuffer);
    ::CloseHandle(hFile);

    g_pBoard->GetCPU()->SetSP(stackAddress);
    g_pBoard->GetCPU()->SetPC(baseAddress);
    g_pBoard->GetCPU()->SetPSW(0340);

    MainWindow_UpdateAllViews();
}

//////////////////////////////////////////////////////////////////////
// Settings Dialog

void ShowSettingsDialog()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS), g_hwnd, SettingsProc);
}

INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            HWND hVolume = GetDlgItem(hDlg, IDC_VOLUME);
            SendMessage(hVolume, TBM_SETRANGEMIN, 0, (LPARAM)0);
            SendMessage(hVolume, TBM_SETRANGEMAX, 0, (LPARAM)0xffff);
            SendMessage(hVolume, TBM_SETTICFREQ, 0x1000, 0);
            SendMessage(hVolume, TBM_SETPOS, TRUE, (LPARAM)Settings_GetSoundVolume());

            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                HWND hVolume = GetDlgItem(hDlg, IDC_VOLUME);
                DWORD volume = SendMessage(hVolume, TBM_GETPOS, 0, 0);
                Settings_SetSoundVolume((WORD)volume);
            }

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}


//////////////////////////////////////////////////////////////////////
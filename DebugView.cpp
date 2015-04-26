/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// DebugView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase\Board.h"
#include "emubase\Processor.h"


//////////////////////////////////////////////////////////////////////

// Colors
#define COLOR_RED   RGB(255,0,0)
#define COLOR_BLUE  RGB(0,0,255)


HWND g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // Debug View window handle
WNDPROC m_wndprocDebugToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDebugViewer = (HWND) INVALID_HANDLE_VALUE;

WORD m_wDebugCpuR[9];  // Old register values - R0..R7, PSW
BOOL m_okDebugCpuRChanged[9];  // Register change flags

void DebugView_DoDraw(HDC hdc);
BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged);
void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y);
void DebugView_DrawPorts(HDC hdc, CMotherboard* pBoard, int x, int y);
void DebugView_UpdateWindowText();


//////////////////////////////////////////////////////////////////////

BOOL DebugView_IsRegisterChanged(int regno)
{
    ASSERT(regno >= 0 && regno <= 8);
    return m_okDebugCpuRChanged[regno];
}

void DebugView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= DebugViewViewerWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_DEBUGVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void CreateDebugView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDebug = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
	DebugView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocDebugToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndDebug, GWLP_WNDPROC, PtrToLong(DebugViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDebug, &rcClient);

	m_hwndDebugViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DEBUGVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDebug, NULL, g_hInst, NULL);

    memset(m_wDebugCpuR, 255, sizeof(m_wDebugCpuR));
    memset(m_okDebugCpuRChanged, 1, sizeof(m_okDebugCpuRChanged));
}

LRESULT CALLBACK DebugViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK DebugViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            DebugView_DoDraw(hdc);  // Draw memory dump

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DebugView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM lParam)
{
    switch (vkey)
    {
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DebugView_UpdateWindowText()
{
	::SetWindowText(g_hwndDebug, _T("Debug"));
}


//////////////////////////////////////////////////////////////////////

// Update after Run or Step
void DebugView_OnUpdate()
{
    CProcessor* pCPU = g_pBoard->GetCPU();
    ASSERT(pCPU != NULL);

    // Get new register values and set change flags
    for (int r = 0; r < 8; r++) {
        WORD value = pCPU->GetReg(r);
        m_okDebugCpuRChanged[r] = (m_wDebugCpuR[r] != value);
        m_wDebugCpuR[r] = value;
    }
    WORD pswCPU = pCPU->GetPSW();
    m_okDebugCpuRChanged[8] = (m_wDebugCpuR[8] != pswCPU);
    m_wDebugCpuR[8] = pswCPU;
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DebugView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDebugPU = g_pBoard->GetCPU();
    ASSERT(pDebugPU != NULL);
    WORD* arrR = m_wDebugCpuR;
    BOOL* arrRChanged = m_okDebugCpuRChanged;

    //TextOut(hdc, cxChar * 1, 2 + 1 * cyLine, _T("CPU"), 3);

	DebugView_DrawProcessor(hdc, pDebugPU, cxChar * 2, 2 + 1 * cyLine, arrR, arrRChanged);

    // Draw stack for the current processor
    DebugView_DrawMemoryForRegister(hdc, 6, pDebugPU, 35 * cxChar, 2 + 0 * cyLine);

    DebugView_DrawPorts(hdc, g_pBoard, 57 * cxChar, 2 + 0 * cyLine);

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    if (::GetFocus() == m_hwndDebugViewer)
    {
        RECT rcClient;
        GetClientRect(m_hwndDebugViewer, &rcClient);
        DrawFocusRect(hdc, &rcClient);
    }
}

void DebugView_DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2)
{
    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x1, y2, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
}

void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = GetSysColor(COLOR_WINDOWTEXT);

    DebugView_DrawRectangle(hdc, x - cxChar, y - 8, x + cxChar + 31 * cxChar, y + 8 + cyLine * 12);

    // Registers
    for (int r = 0; r < 8; r++) {
        ::SetTextColor(hdc, arrRChanged[r] ? COLOR_RED : colorText);

        LPCTSTR strRegName = REGISTER_NAME[r];
        TextOut(hdc, x, y + r * cyLine, strRegName, (int) wcslen(strRegName));

        WORD value = arrR[r]; //pProc->GetReg(r);
        DrawOctalValue(hdc, x + cxChar * 3, y + r * cyLine, value);
        DrawHexValue(hdc, x + cxChar * 10, y + r * cyLine, value);
        DrawBinaryValue(hdc, x + cxChar * 15, y + r * cyLine, value);
    }
    ::SetTextColor(hdc, colorText);

    // PSW value
    ::SetTextColor(hdc, arrRChanged[8] ? COLOR_RED : colorText);
    TextOut(hdc, x, y + 9 * cyLine, _T("PS"), 2);
    WORD psw = arrR[8]; // pProc->GetPSW();
    DrawOctalValue(hdc, x + cxChar * 3, y + 9 * cyLine, psw);
    DrawHexValue(hdc, x + cxChar * 10, y + 9 * cyLine, psw);
    TextOut(hdc, x + cxChar * 15, y + 8 * cyLine, _T("       HP  TNZVC"), 16);
    DrawBinaryValue(hdc, x + cxChar * 15, y + 9 * cyLine, psw);

    ::SetTextColor(hdc, colorText);

    // Processor mode - HALT or USER
    BOOL okHaltMode = pProc->IsHaltMode();
    TextOut(hdc, x, y + 11 * cyLine, okHaltMode ? _T("HALT") : _T("USER"), 4);

    // "Stopped" flag
    BOOL okStopped = pProc->IsStopped();
    if (okStopped)
        TextOut(hdc, x + 6 * cxChar, y + 11 * cyLine, _T("STOP"), 4);

}

void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = GetSysColor(COLOR_WINDOWTEXT);

    WORD current = pProc->GetReg(reg);
    BOOL okExec = (reg == 7);

    // ������ �� ������ ���������� � �����
    WORD memory[16];
    for (int idx = 0; idx < 16; idx++) {
        int addrtype;
        memory[idx] = g_pBoard->GetWordView(
                current + idx * 2 - 14, pProc->IsHaltMode(), okExec, &addrtype);
    }

    WORD address = current - 14;
    for (int index = 0; index < 14; index++) {  // ������ ������
        // �����
        DrawOctalValue(hdc, x + 4 * cxChar, y, address);

        // �������� �� ������
        WORD value = memory[index];
        WORD wChanged = Emulator_GetChangeRamStatus(address);
        ::SetTextColor(hdc, (wChanged != 0) ? RGB(255,0,0) : colorText);
        DrawOctalValue(hdc, x + 12 * cxChar, y, value);
        ::SetTextColor(hdc, colorText);

        // ������� �������
        if (address == current) {
            TextOut(hdc, x + 2 * cxChar, y, _T(">>"), 2);
            ::SetTextColor(hdc, m_okDebugCpuRChanged[reg] ? COLOR_RED : colorText);
            TextOut(hdc, x, y, REGISTER_NAME[reg], 2);
            ::SetTextColor(hdc, colorText);
        }

        address += 2;
        y += cyLine;
    }
}

void DebugView_DrawPorts(HDC hdc, CMotherboard* pBoard, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

	TextOut(hdc, x, y, _T("Port"), 6);

    WORD value;
    y += cyLine;
    value = g_pBoard->GetPortView(0177660);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177572);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("himem addr"), 10);
    y += cyLine;
    //value = g_pBoard->GetPortView(0177662);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177662);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("keyb data"), 9);
    y += cyLine;
    value = g_pBoard->GetPortView(0177100);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177100);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("floppy state"), 12);
    y += cyLine;
    value = g_pBoard->GetPortView(0177102);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177102);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("floppy data"), 11);
    y += cyLine;
    value = g_pBoard->GetPortView(0177106);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177106);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("floppy timer"), 12);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177712);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177712);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("timer manage"), 12);
    y += cyLine;
    value = g_pBoard->GetPortView(0177514);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177514);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("parallel"), 8);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177716);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177716);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("system"), 6);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177130);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177130);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("floppy state"), 12);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177132);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177132);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("floppy data"), 11);
}


//////////////////////////////////////////////////////////////////////
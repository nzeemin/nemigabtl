/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// ScreenView.cpp

#include "stdafx.h"
#include <mmintrin.h>
#include <vfw.h>
#include "Main.h"
#include "Views.h"
#include "Emulator.h"
#include "util/BitmapFile.h"


//////////////////////////////////////////////////////////////////////

#define COLOR_BK_BACKGROUND RGB(115,115,115)


HWND g_hwndScreen = NULL;  // Screen View window handle

HDRAWDIB m_hdd = NULL;
BITMAPINFO m_bmpinfo;
HBITMAP m_hbmp = NULL;
DWORD * m_bits = NULL;
int m_cxScreenWidth = NEMIGA_SCREEN_WIDTH;
int m_cyScreenHeight = NEMIGA_SCREEN_HEIGHT;
BYTE m_ScreenKeyState[256];
int m_ScreenMode = 0;

void ScreenView_CreateDisplay();
void ScreenView_OnDraw(HDC hdc);
//BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed);

const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();


//////////////////////////////////////////////////////////////////////

void ScreenView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ScreenViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_SCREENVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_hdd = DrawDibOpen();
    ScreenView_CreateDisplay();
}

void ScreenView_Done()
{
    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    DrawDibClose( m_hdd );
}

void ScreenView_CreateDisplay()
{
    ASSERT(g_hwnd != NULL);

    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    HDC hdc = GetDC( g_hwnd );

    m_bmpinfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    m_bmpinfo.bmiHeader.biWidth = NEMIGA_SCREEN_WIDTH;
    m_bmpinfo.bmiHeader.biHeight = m_cyScreenHeight;
    m_bmpinfo.bmiHeader.biPlanes = 1;
    m_bmpinfo.bmiHeader.biBitCount = 32;
    m_bmpinfo.bmiHeader.biCompression = BI_RGB;
    m_bmpinfo.bmiHeader.biSizeImage = 0;
    m_bmpinfo.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biClrUsed = 0;
    m_bmpinfo.bmiHeader.biClrImportant = 0;

    m_hbmp = CreateDIBSection( hdc, &m_bmpinfo, DIB_RGB_COLORS, (void **) &m_bits, NULL, 0 );

    ReleaseDC( g_hwnd, hdc );
}

// Create Screen View as child of Main Window
void CreateScreenView(HWND hwndParent, int x, int y, int cxWidth)
{
    ASSERT(hwndParent != NULL);

    int xLeft = x;
    int yTop = y;
    int cyScreenHeight = 4 + m_cyScreenHeight + 4;
    int cyHeight = cyScreenHeight;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, NULL, g_hInst, NULL);

    // Initialize m_ScreenKeyState
    VERIFY(::GetKeyboardState(m_ScreenKeyState));
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            ScreenView_PrepareScreen();  //DEBUG
            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
        //case WM_KEYDOWN:
        //    //if ((lParam & (1 << 30)) != 0)  // Auto-repeats should be ignored
        //    //    return (LRESULT) TRUE;
        //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, TRUE);
        //    return (LRESULT) TRUE;
        //case WM_KEYUP:
        //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, FALSE);
        //    return (LRESULT) TRUE;
    case WM_SETCURSOR:
        if (::GetFocus() == g_hwndScreen)
        {
            SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

int ScreenView_GetScreenMode()
{
    return m_ScreenMode;
}
void ScreenView_SetScreenMode(int newMode)
{
    if (m_ScreenMode == newMode) return;

    m_ScreenMode = newMode;

    // Ask Emulator module for screen width and height
    int cxWidth, cyHeight;
    Emulator_GetScreenSize(newMode, &cxWidth, &cyHeight);
    m_cyScreenHeight = cyHeight;
    ScreenView_CreateDisplay();

    RECT rc;  ::GetWindowRect(g_hwndScreen, &rc);
    ::SetWindowPos(g_hwndScreen, NULL, 0, 0, rc.right - rc.left, 4 + cyHeight + 4, SWP_NOZORDER | SWP_NOMOVE);
}

void ScreenView_OnDraw(HDC hdc)
{
    if (m_bits == NULL) return;

    RECT rc;  ::GetClientRect(g_hwndScreen, &rc);
    int x = (rc.right - NEMIGA_SCREEN_WIDTH) / 2;

    DrawDibDraw(m_hdd, hdc,
            x, 4, -1, -1,
            &m_bmpinfo.bmiHeader, m_bits, 0, 0,
            m_cxScreenWidth, m_cyScreenHeight,
            0);

    // Empty border
    HBRUSH hBrush = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, x, rc.bottom, PATCOPY);
    PatBlt(hdc, x + NEMIGA_SCREEN_WIDTH, 0, rc.right, rc.bottom, PATCOPY);
    PatBlt(hdc, x, 0, NEMIGA_SCREEN_WIDTH, 4, BLACKNESS);
    PatBlt(hdc, x, rc.bottom - 4, NEMIGA_SCREEN_WIDTH, 4, BLACKNESS);
    ::SelectObject(hdc, hOldBrush);
    ::DeleteObject(hBrush);
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    ::ReleaseDC(g_hwndScreen, hdc);
}

void ScreenView_PrepareScreen()
{
    if (m_bits == NULL) return;

    Emulator_PrepareScreenRGB32(m_bits, m_ScreenMode);
}

void ScreenView_PutKeyEventToQueue(WORD keyevent)
{
    if (m_ScreenKeyQueueCount == KEYEVENT_QUEUE_SIZE) return;  // Full queue

    m_ScreenKeyQueue[m_ScreenKeyQueueTop] = keyevent;
    m_ScreenKeyQueueTop++;
    if (m_ScreenKeyQueueTop >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueTop = 0;
    m_ScreenKeyQueueCount++;
}
WORD ScreenView_GetKeyEventFromQueue()
{
    if (m_ScreenKeyQueueCount == 0) return 0;  // Empty queue

    WORD keyevent = m_ScreenKeyQueue[m_ScreenKeyQueueBottom];
    m_ScreenKeyQueueBottom++;
    if (m_ScreenKeyQueueBottom >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueBottom = 0;
    m_ScreenKeyQueueCount--;

    return keyevent;
}

const BYTE arrPcChar2NemigaChar[256] =    // Nemiga chars from PC chars
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0x03, 0000, 0000, 0000, 0000, 0177, 0015, 0000, 0000, 0x0c, 0015, 0000, 0000,
    /*1*/    0000, 0x11, 0000, 0x13, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*2*/    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    /*3*/    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    /*4*/    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    /*5*/    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    /*6*/    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    /*7*/    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*c*/    0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
    /*d*/    0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde, 0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
    /*e*/    0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
    /*f*/    0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde, 0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
};

void ScreenView_ScanKeyboard()
{
    if (! g_okEmulatorRunning) return;
    if (::GetFocus() == g_hwndScreen)
    {
        // Read current keyboard state
        BYTE keys[256];
        VERIFY(::GetKeyboardState(keys));

        BOOL okShift = ((keys[VK_SHIFT] & 128) != 0);
        BOOL okCtrl = ((keys[VK_CONTROL] & 128) != 0);
        HKL hkl = ::GetKeyboardLayout(0);

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) == (oldstate & 128))
                continue; // Key state not changed

            BYTE vkey = (BYTE) scan;
            WORD mapchars[2];
            int res = ToAsciiEx(vkey, MapVirtualKey(vkey, 0), keys, mapchars, 0, hkl);
            if (!res)
                continue;

            BYTE key = (BYTE)mapchars[0];

//#if !defined(PRODUCT)
//            if (newstate & 128)
//                DebugPrintFormat(_T("Key PC: 0x%0x %d %d 0x%0x\r\n"), vkey, okShift, okCtrl, (int)key);
//#endif

            key = arrPcChar2NemigaChar[key];
            if (key == 0)
                continue;

            BYTE pressed = (newstate & 128) | (okCtrl ? 64 : 0);
            WORD keyevent = MAKEWORD(key, pressed);
            ScreenView_PutKeyEventToQueue(keyevent);
        }

        // Save keyboard state
        ::memcpy(m_ScreenKeyState, keys, 256);
    }
}

void ScreenView_ProcessKeyboard()
{
    // Process next event in the keyboard queue
    WORD keyevent = ScreenView_GetKeyEventFromQueue();
    if (keyevent != 0)
    {
        BOOL pressed = ((keyevent & 0x8000) != 0);
        BOOL ctrl = ((keyevent & 0x4000) != 0);
        BYTE bkscan = LOBYTE(keyevent);

//#if !defined(PRODUCT)
//        DebugPrintFormat(_T("KeyEvent: 0x%0x %d %d\r\n"), bkscan, pressed, ctrl);
//#endif

        g_pBoard->KeyboardEvent(bkscan, pressed);
    }
}

// External key event - e.g. from KeyboardView
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed)
{
    ScreenView_PutKeyEventToQueue(MAKEWORD(keyscan, pressed ? 128 : 0));
}

BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);
    ASSERT(m_bits != NULL);

    DWORD* pBits = (DWORD*) ::malloc(NEMIGA_SCREEN_WIDTH * NEMIGA_SCREEN_HEIGHT * 4);
    const DWORD* colors = Emulator_GetPalette(/*m_ScreenMode*/);
    Emulator_PrepareScreenRGB32(pBits, m_ScreenMode);

    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    BOOL result = FALSE;
    if (sFileNameExt != NULL && _tcsicmp(sFileNameExt, _T(".png")) == 0)
        result = PngFile_SaveScreenshot(pBits, colors, sFileName);
    else
        result = BmpFile_SaveScreenshot(pBits, colors, sFileName);

    ::free(pBits);

    return result;
}


//////////////////////////////////////////////////////////////////////

﻿/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// KeyboardView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"

//////////////////////////////////////////////////////////////////////


#define COLOR_KEYBOARD_BACKGROUND   RGB(160,160,160)
#define COLOR_KEYBOARD_LITE         RGB(228,228,228)
#define COLOR_KEYBOARD_GRAY         RGB(160,160,160)
#define COLOR_KEYBOARD_DARK         RGB(80,80,80)
#define COLOR_KEYBOARD_RED          RGB(200,80,80)

#define KEYCLASSLITE 0
#define KEYCLASSGRAY 1
#define KEYCLASSDARK 2

#define KEYSCAN_NONE 255

HWND g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE * m_pKeyboardRom = NULL;
BYTE m_nKeyboardKeyPressed = KEYSCAN_NONE;  // Scan-code for the key pressed, or KEYSCAN_NONE
BYTE m_nKeyboardCharPressed = 0;

void KeyboardView_InvalidateIndicators();
void KeyboardView_OnDraw(HDC hdc);
int KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////

#define KEYEXTRA_CIF    0201
#define KEYEXTRA_DOP    0202
#define KEYEXTRA_SHIFT  0203
#define KEYEXTRA_RUS    0204
#define KEYEXTRA_LAT    0205
#define KEYEXTRA_RUSLAT 0206
#define KEYEXTRA_UPR    0207
#define KEYEXTRA_FPB    0210

#define KEYEXTRA_START  0220
#define KEYEXTRA_STOP   0221
#define KEYEXTRA_TIMER  0222

struct KeyboardKeys
{
    int x, y, w, h;
    int keyclass;
    LPCTSTR text;
    BYTE scan;
}
m_arrKeyboardKeys[] =
{
    {   2,  30, 25, 25, KEYCLASSLITE, _T("Ф1"),     0007 },
    {  27,  30, 25, 25, KEYCLASSLITE, _T("Ф2"),     0017 },
    {   2,  55, 25, 25, KEYCLASSLITE, _T("Ф3"),     0027 },
    {  27,  55, 25, 25, KEYCLASSLITE, _T("Ф4"),     0037 },
    {   2,  80, 25, 25, KEYCLASSLITE, _T("Ф5"),     0067 },
    {  27,  80, 25, 25, KEYCLASSLITE, _T("Ф6"),     0057 },
    {   2, 105, 25, 25, KEYCLASSLITE, _T("Ф7"),     0047 },
    {  27, 105, 25, 25, KEYCLASSLITE, _T("Ф8"),     0077 },
    {   2, 130, 25, 25, KEYCLASSLITE, _T("Ф9"),     0107 },
    {  27, 130, 25, 25, KEYCLASSLITE, _T("Ф10"),    0064 },

    {  56,  30, 29, 25, KEYCLASSGRAY, _T("ключ"),   0014 }, // Ключ
    {  85,  30, 27, 25, KEYCLASSLITE, _T("; +"),    0114 },
    { 112,  30, 27, 25, KEYCLASSLITE, _T("1 !"),    0124 }, // 1 !
    { 139,  30, 27, 25, KEYCLASSLITE, _T("2 \""),   0011 }, // 2 "
    { 166,  30, 27, 25, KEYCLASSLITE, _T("3 #"),    0121 }, // 3 #
    { 193,  30, 27, 25, KEYCLASSLITE, _T("4 \u00a4"), 0012 }, // 4
    { 220,  30, 27, 25, KEYCLASSLITE, _T("5 %"),    0122 }, // 5 %
    { 247,  30, 27, 25, KEYCLASSLITE, _T("6 &"),    0013 }, // 6 &
    { 274,  30, 27, 25, KEYCLASSLITE, _T("7 \'"),   0123 }, // 7 '
    { 301,  30, 27, 25, KEYCLASSLITE, _T("8 ("),    0113 }, // 8 (
    { 328,  30, 27, 25, KEYCLASSLITE, _T("9 )"),    0110 }, // 9 )
    { 355,  30, 27, 25, KEYCLASSLITE, _T("0"),      0010 }, // 0
    { 382,  30, 27, 25, KEYCLASSLITE, _T("- ="),    0120 }, // - =
    { 409,  30, 27, 25, KEYCLASSLITE, _T("~ "),     0016 }, // ~
    { 436,  30, 27, 25, KEYCLASSGRAY, _T("\u2190"), 0116 }, // Backspace

    {  70,  55, 30, 25, KEYCLASSGRAY, _T("ТАБ"),    0004 }, // TAB
    { 100,  55, 27, 25, KEYCLASSLITE, _T("ЙJ"),     0034 }, // Й J
    { 127,  55, 27, 25, KEYCLASSLITE, _T("ЦC"),     0001 }, // Ц C
    { 154,  55, 27, 25, KEYCLASSLITE, _T("УU"),     0031 }, // У U
    { 181,  55, 27, 25, KEYCLASSLITE, _T("КK"),     0111 }, // К K
    { 208,  55, 27, 25, KEYCLASSLITE, _T("ЕE"),     0002 }, // Е E
    { 235,  55, 27, 25, KEYCLASSLITE, _T("НN"),     0032 }, // Н N
    { 262,  55, 27, 25, KEYCLASSLITE, _T("ГG"),     0112 }, // Г G
    { 289,  55, 27, 25, KEYCLASSLITE, _T("Ш ["),    0003 }, // Ш [
    { 316,  55, 27, 25, KEYCLASSLITE, _T("Щ ]"),    0023 }, // Щ ]
    { 343,  55, 27, 25, KEYCLASSLITE, _T("ЗZ"),     0033 }, // З Z
    { 370,  55, 27, 25, KEYCLASSLITE, _T("ХH"),     0000 }, // Х H
    { 397,  55, 27, 25, KEYCLASSLITE, _T(": *"),    0030 }, // : *
    { 424,  55, 27, 25, KEYCLASSLITE, _T("Ъ }"),    0020 }, // Ъ }
    { 451,  55, 27, 25, KEYCLASSGRAY, _T("ПС"),     0006 }, // ПС

    {  82,  80, 30, 25, KEYCLASSGRAY, _T("УПР"), KEYEXTRA_UPR }, // УПР
    { 112,  80, 27, 25, KEYCLASSLITE, _T("ФF"),     0024 }, // Ф F
    { 139,  80, 27, 25, KEYCLASSLITE, _T("ЫY"),     0051 }, // Ы Y
    { 166,  80, 27, 25, KEYCLASSLITE, _T("ВW"),     0021 }, // В W
    { 193,  80, 27, 25, KEYCLASSLITE, _T("АA"),     0052 }, // А A
    { 220,  80, 27, 25, KEYCLASSLITE, _T("ПP"),     0022 }, // П P
    { 247,  80, 27, 25, KEYCLASSLITE, _T("РR"),     0042 }, // Р R
    { 274,  80, 27, 25, KEYCLASSLITE, _T("ОO"),     0053 }, // О O
    { 301,  80, 27, 25, KEYCLASSLITE, _T("ЛL"),     0043 }, // Л L
    { 328,  80, 27, 25, KEYCLASSLITE, _T("ДD"),     0073 }, // Д D
    { 355,  80, 27, 25, KEYCLASSLITE, _T("ЖV"),     0040 }, // Ж V
    { 382,  80, 27, 25, KEYCLASSLITE, _T("Э \\"),   0050 }, // Э backslash
    { 409,  80, 27, 25, KEYCLASSLITE, _T(". >"),    0070 }, // . >
    { 436,  80, 27, 25, KEYCLASSLITE, _T("Ё {"),    0076 }, // Ё {
    { 463,  80, 34, 25, KEYCLASSGRAY, _T("ВВОД"),   0046 }, // ВВОД

    { 100, 105, 27, 25, KEYCLASSGRAY, _T("\u2191"), KEYEXTRA_SHIFT }, // Shift
    { 127, 105, 27, 25, KEYCLASSLITE, _T("ЯQ"),     0044 }, // Я Q
    { 154, 105, 27, 25, KEYCLASSLITE, _T("Ч\u00ac"), 0071 }, // Ч ^
    { 181, 105, 27, 25, KEYCLASSLITE, _T("СS"),     0041 }, // С S
    { 208, 105, 27, 25, KEYCLASSLITE, _T("МM"),     0072 }, // М M
    { 235, 105, 27, 25, KEYCLASSLITE, _T("ИI"),     0061 }, // И I
    { 262, 105, 27, 25, KEYCLASSLITE, _T("ТT"),     0131 }, // Т T
    { 289, 105, 27, 25, KEYCLASSLITE, _T("ЬX"),     0062 }, // Ь X
    { 316, 105, 27, 25, KEYCLASSLITE, _T("БB"),     0132 }, // Б B
    { 343, 105, 27, 25, KEYCLASSLITE, _T("Ю@"),     0063 }, // Ю @
    { 370, 105, 27, 25, KEYCLASSLITE, _T(", <"),    0133 }, // ,
    { 397, 105, 27, 25, KEYCLASSLITE, _T("/ ?"),    0060 }, // /
    { 424, 105, 27, 25, KEYCLASSLITE, _T("_"),      0130 }, // _
    { 451, 105, 30, 25, KEYCLASSGRAY, _T("\u2191"), KEYEXTRA_SHIFT }, // Shift
    { 481, 105, 32, 25, KEYCLASSGRAY, _T("ФПБ"), KEYEXTRA_FPB }, // ФПБ

    {  73, 130, 27, 25, KEYCLASSGRAY, _T("РУС"), KEYEXTRA_RUS }, // РУС
    { 100, 130, 27, 25, KEYCLASSGRAY, _T("Р/Л"), KEYEXTRA_RUSLAT }, // Р/Л
    { 127, 130, 27, 25, KEYCLASSGRAY, _T("АЛТ"),    0101 }, // АЛТ
    { 154, 130, 215, 25, KEYCLASSDARK, NULL,         0102 }, // Space
    { 370, 130, 27, 25, KEYCLASSGRAY, _T("Р/Л"), KEYEXTRA_RUSLAT }, // Р/Л
    { 397, 130, 27, 25, KEYCLASSGRAY, _T("ЛАТ"), KEYEXTRA_LAT }, // ЛАТ
    { 424, 130, 27, 25, KEYCLASSGRAY, _T("ЗБ"),     0103 }, // ЗБ
    { 451, 130, 30, 25, KEYCLASSGRAY, _T("ДОП"), KEYEXTRA_DOP }, // ДОП

    { 516,  30, 25, 25, KEYCLASSGRAY, _T("циф"), KEYEXTRA_CIF }, // NumPad ЦИФ
    { 541,  30, 25, 25, KEYCLASSDARK, _T("фсд"),    0015 }, // NumPad ФСД СТОП
    { 566,  30, 25, 25, KEYCLASSDARK, _T("-"),      0125 }, // NumPad --
    { 516,  55, 25, 25, KEYCLASSDARK, _T("7"),      0036 }, // NumPad 7
    { 541,  55, 25, 25, KEYCLASSDARK, _T("8"),      0005 }, // NumPad 8 up
    { 566,  55, 25, 25, KEYCLASSDARK, _T("9"),      0115 }, // NumPad 9
    { 516,  80, 25, 25, KEYCLASSDARK, _T("4"),      0056 }, // NumPad 4 left
    { 541,  80, 25, 25, KEYCLASSDARK, _T("5"),      0025 }, // NumPad 5
    { 566,  80, 25, 25, KEYCLASSDARK, _T("6"),      0035 }, // NumPad 6 right
    { 516, 105, 25, 25, KEYCLASSDARK, _T("1"),      0075 }, // NumPad 1 КОН
    { 541, 105, 25, 25, KEYCLASSDARK, _T("2"),      0055 }, // NumPad 2 down
    { 566, 105, 25, 25, KEYCLASSDARK, _T("3"),      0135 }, // NumPad 3
    { 516, 130, 25, 25, KEYCLASSDARK, _T("0"),      0105 }, // NumPad 0 ВСТ
    { 541, 130, 25, 25, KEYCLASSDARK, _T("."),      0065 }, // NumPad . УДЛ
    { 566, 130, 25, 25, KEYCLASSDARK, _T("+"),      0045 }, // NumPad +

    { 400,   2, 20, 20, KEYCLASSGRAY, NULL, KEYEXTRA_START }, // ПУСК
    { 460,   2, 20, 20, KEYCLASSGRAY, NULL, KEYEXTRA_STOP  }, // ОСТ
    { 515,   2, 20, 20, KEYCLASSGRAY, NULL, KEYEXTRA_TIMER }, // ТАЙМЕР
};

const int m_nKeyboardKeysCount = sizeof(m_arrKeyboardKeys) / sizeof(KeyboardKeys);

struct KeyboardIndicator
{
    int x, y, w, h;
    LPCTSTR text;
    BOOL state;
}
m_arrKeyboardIndicators[] =
{
    {   140, 6, 12, 12, _T("РУС"),  FALSE },
    {   215, 6, 12, 12, _T("ВЕРХ"), TRUE },
    {   265, 6, 12, 12, _T("ЛАТ"),  TRUE },
    {   315, 6, 12, 12, _T("ЦИФ"),  TRUE },
    {   405, 6, 11, 11, _T(" ПУСК"), TRUE },
    {   465, 6, 11, 11, _T(" ОСТ"),  FALSE },
    {   520, 6, 11, 11, _T(" ТАЙМЕР"), FALSE },
};
const int m_nKeyboardIndicatorsCount = sizeof(m_arrKeyboardIndicators) / sizeof(KeyboardIndicator);

BOOL KeyboardView_IsLat() { return m_arrKeyboardIndicators[2].state; }
BOOL KeyboardView_IsNum() { return m_arrKeyboardIndicators[3].state; }

//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = KeyboardViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Init()
{
    HRSRC hres = ::FindResource(NULL, MAKEINTRESOURCE(IDR_NEMIGA_KEYB), RT_RCDATA);
    HGLOBAL hgData = ::LoadResource(NULL, hres);
    m_pKeyboardRom = (BYTE*) ::LockResource(hgData);
}

void KeyboardView_Done()
{
}

void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndKeyboard = CreateWindow(
            CLASSNAME_KEYBOARDVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK KeyboardViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            KeyboardView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        {
            POINT ptCursor;  ::GetCursorPos(&ptCursor);
            ::ScreenToClient(g_hwndKeyboard, &ptCursor);
            int keyindex = KeyboardView_GetKeyByPoint(ptCursor.x, ptCursor.y);
            LPCTSTR cursor = (keyindex == -1) ? IDC_ARROW : IDC_HAND;
            ::SetCursor(::LoadCursor(NULL, cursor));
        }
        return (LRESULT)TRUE;
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            WORD fwkeys = (WORD) wParam;

            int keyindex = KeyboardView_GetKeyByPoint(x, y);
            if (keyindex == -1) break;
            BYTE keyscan = (BYTE) m_arrKeyboardKeys[keyindex].scan;
            if (keyscan == KEYSCAN_NONE) break;

            if (keyscan < 128)
            {
                // Convert keyscan to key char
                BOOL ctrl = (fwkeys & MK_CONTROL) != 0;
                BOOL shift = (fwkeys & MK_SHIFT) != 0;
                int tableindex = 1 | (shift ? 4 : 0) | (ctrl ? 0 : 2) | (KeyboardView_IsLat() ? 0 : 8);  //TODO
                int index = tableindex * 128 + keyscan;
                m_nKeyboardCharPressed = m_pKeyboardRom[index];

//                DebugPrintFormat(_T("Keyboard key: 0x%0x\r\n"), m_nKeyboardCharPressed);

                // Fire keydown event
                ScreenView_KeyEvent(m_nKeyboardCharPressed, TRUE);
            }
            else
            {
                BOOL repaintIndicators = FALSE;
                switch (keyscan)
                {
                case KEYEXTRA_LAT:
                    m_arrKeyboardIndicators[0].state = FALSE;   // РУС
                    m_arrKeyboardIndicators[2].state = TRUE;    // ЛАТ
                    repaintIndicators = TRUE;
                    break;
                case KEYEXTRA_RUS:
                    m_arrKeyboardIndicators[0].state = TRUE;    // РУС
                    m_arrKeyboardIndicators[2].state = FALSE;   // ЛАТ
                    repaintIndicators = TRUE;
                    break;
                case KEYEXTRA_RUSLAT:
                    {
                        BOOL lat = KeyboardView_IsLat();
                        m_arrKeyboardIndicators[0].state = lat;     // РУС
                        m_arrKeyboardIndicators[2].state = !lat;    // ЛАТ
                        repaintIndicators = TRUE;
                    }
                    break;
                case KEYEXTRA_CIF:
                    m_arrKeyboardIndicators[3].state = TRUE;    // ЦИФ
                    repaintIndicators = TRUE;
                    break;
                case KEYEXTRA_DOP:
                    m_arrKeyboardIndicators[3].state = FALSE;   // ЦИФ
                    repaintIndicators = TRUE;
                    break;
                case KEYEXTRA_TIMER:
                    {
                        BOOL okOnOff = !m_arrKeyboardIndicators[6].state;  // ТАЙМЕР
                        m_arrKeyboardIndicators[6].state = okOnOff;
                        g_pBoard->SetTimer50OnOff(okOnOff != 0);
                        repaintIndicators = TRUE;
                    }
                    break;
                }

                if (repaintIndicators)
                    KeyboardView_InvalidateIndicators();
            }

            ::SetCapture(g_hwndKeyboard);

            // Draw focus frame for the key pressed
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, keyscan);
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

            // Remember key pressed
            m_nKeyboardKeyPressed = keyscan;
        }
        break;
    case WM_LBUTTONUP:
        if (m_nKeyboardKeyPressed != KEYSCAN_NONE)
        {
            // Fire keyup event and release mouse
            if (m_nKeyboardKeyPressed < 128)
                ScreenView_KeyEvent(m_nKeyboardCharPressed, FALSE);
            ::ReleaseCapture();

            // Draw focus frame for the released key
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

            m_nKeyboardKeyPressed = KEYSCAN_NONE;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void KeyboardView_InvalidateIndicators()
{
    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    m_nKeyboardBitmapLeft = (rc.right - 592) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - 160) / 2;
    if (m_nKeyboardBitmapTop < 0) m_nKeyboardBitmapTop = 0;
    if (m_nKeyboardBitmapTop > 16) m_nKeyboardBitmapTop = 16;

    for (int i = 0; i < m_nKeyboardIndicatorsCount; i++)
    {
        RECT rcRadio;
        rcRadio.left = m_nKeyboardBitmapLeft + m_arrKeyboardIndicators[i].x;
        rcRadio.top = m_nKeyboardBitmapTop + m_arrKeyboardIndicators[i].y;
        rcRadio.right = rcRadio.left + m_arrKeyboardIndicators[i].w;
        rcRadio.bottom = rcRadio.top + m_arrKeyboardIndicators[i].h;

        ::InvalidateRect(g_hwndKeyboard, &rcRadio, FALSE);
    }
}

void KeyboardView_OnDraw(HDC hdc)
{
    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    // Keyboard background
    HBRUSH hBkBrush = ::CreateSolidBrush(COLOR_KEYBOARD_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBkBrush);
    ::PatBlt(hdc, 0, 0, rc.right, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);

    if (m_nKeyboardKeyPressed != KEYSCAN_NONE)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    HBRUSH hbrLite = ::CreateSolidBrush(COLOR_KEYBOARD_LITE);
    HBRUSH hbrGray = ::CreateSolidBrush(COLOR_KEYBOARD_GRAY);
    HBRUSH hbrDark = ::CreateSolidBrush(COLOR_KEYBOARD_DARK);
    HBRUSH hbrRed = ::CreateSolidBrush(COLOR_KEYBOARD_RED);
    m_nKeyboardBitmapLeft = (rc.right - 592) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - 160) / 2;
    if (m_nKeyboardBitmapTop < 0) m_nKeyboardBitmapTop = 0;
    if (m_nKeyboardBitmapTop > 16) m_nKeyboardBitmapTop = 16;

    HFONT hfont = CreateDialogFont();
    HGDIOBJ hOldFont = ::SelectObject(hdc, hfont);
    ::SetBkMode(hdc, TRANSPARENT);

    // Draw keys
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;

        HBRUSH hbr = hBkBrush;
        COLORREF textcolor = COLOR_KEYBOARD_DARK;
        switch (m_arrKeyboardKeys[i].keyclass)
        {
        case KEYCLASSLITE: hbr = hbrLite; break;
        case KEYCLASSGRAY: hbr = hbrGray; break;
        case KEYCLASSDARK: hbr = hbrDark;  textcolor = COLOR_KEYBOARD_LITE; break;
        }
        HGDIOBJ hOldBrush = ::SelectObject(hdc, hbr);
        //rcKey.left++; rcKey.top++; rcKey.right--; rc.bottom--;
        ::PatBlt(hdc, rcKey.left, rcKey.top, rcKey.right - rcKey.left, rcKey.bottom - rcKey.top, PATCOPY);
        ::SelectObject(hdc, hOldBrush);

        //TCHAR text[10];
        //_sntprintf(text, sizeof(text) / sizeof(TCHAR) - 1, _T("%02x"), (int)m_arrKeyboardKeys[i].scan);
        LPCTSTR text = m_arrKeyboardKeys[i].text;
        if (text != NULL)
        {
            ::SetTextColor(hdc, textcolor);
            ::DrawText(hdc, text, wcslen(text), &rcKey, DT_NOPREFIX | DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }

        ::DrawEdge(hdc, &rcKey, BDR_RAISEDOUTER, BF_RECT);
    }

    // Draw indicators
    for (int i = 0; i < m_nKeyboardIndicatorsCount; i++)
    {
        RECT rcRadio;
        rcRadio.left = m_nKeyboardBitmapLeft + m_arrKeyboardIndicators[i].x;
        rcRadio.top = m_nKeyboardBitmapTop + m_arrKeyboardIndicators[i].y;
        rcRadio.right = rcRadio.left + m_arrKeyboardIndicators[i].w;
        rcRadio.bottom = rcRadio.top + m_arrKeyboardIndicators[i].h;

        HBRUSH hbr = m_arrKeyboardIndicators[i].state ? hbrRed : hbrDark;
        HGDIOBJ hOldBrush = ::SelectObject(hdc, hbr);
        ::PatBlt(hdc, rcRadio.left, rcRadio.top, rcRadio.right - rcRadio.left, rcRadio.bottom - rcRadio.top, PATCOPY);
        ::SelectObject(hdc, hOldBrush);

        ::DrawEdge(hdc, &rcRadio, BDR_SUNKENOUTER, BF_RECT);

        ::SetTextColor(hdc, COLOR_KEYBOARD_DARK);
        ::TextOut(hdc, rcRadio.right + 2, rcRadio.top, m_arrKeyboardIndicators[i].text, wcslen(m_arrKeyboardIndicators[i].text));
    }

    ::SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hfont));

    VERIFY(::DeleteObject(hbrLite));
    VERIFY(::DeleteObject(hbrGray));
    VERIFY(::DeleteObject(hbrDark));
    VERIFY(::DeleteObject(hbrRed));
    VERIFY(::DeleteObject(hBkBrush));
}

// Returns: index of key under the cursor position, or -1 if not found
int KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;

        if (x >= rcKey.left && x < rcKey.right && y >= rcKey.top && y < rcKey.bottom)
        {
            return i;
        }
    }
    return -1;
}

void Keyboard_DrawKey(HDC hdc, BYTE keyscan)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
        if (keyscan == m_arrKeyboardKeys[i].scan)
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////

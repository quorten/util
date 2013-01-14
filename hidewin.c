/* Hide an application window */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>
#include "exparray.h"
#include "hidewin.h"

typedef char* char_ptr;
EA_TYPE(HWND);
EA_TYPE(char_ptr);

/* Global variables */
HINSTANCE g_hInst;
HWND hListBox;
HFONT hFont;
HWND_array windowHwnds;
char_ptr_array windowNames;
unsigned listSel = 0;

BOOL CALLBACK EnumWinProc(HWND hwnd, LPARAM lParam)
{
	unsigned winTextLen;
	char* tstring;

	winTextLen = GetWindowTextLength(hwnd);
	if (winTextLen == 0)
		return TRUE;
	EA_APPEND(HWND, windowHwnds, hwnd);
	tstring = (char*)malloc(winTextLen + 1);
	GetWindowText(hwnd, tstring, winTextLen + 1);
	EA_APPEND(char_ptr, windowNames, tstring);

	return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		RECT rt;
		HDC hDC;
		GetClientRect(hwnd, &rt);
		hListBox = CreateWindowEx(NULL, "LISTBOX", "Null",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
			0, 0, rt.right, rt.bottom, hwnd, (HMENU)MAINLBOX, g_hInst, NULL);
		hDC = GetDC(hwnd);
		hFont = CreateFont(-MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72),
			0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
			ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH, "MS Sans Serif");
		ReleaseDC(hwnd, hDC);
		SendMessage(hListBox, WM_SETFONT, (WPARAM)hFont, FALSE);
		SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(M_UPDATE, 0), 0);
		break;
	}
	case WM_DESTROY:
		DestroyWindow(hListBox);
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	case WM_SIZE:
	{
		RECT rt;
		GetClientRect(hwnd, &rt);
		MoveWindow(hListBox, 0, 0, rt.right, rt.bottom, TRUE);
		break;
	}
	case WM_PAINT:
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case M_UPDATE:
			SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
			EA_CLEAR(windowHwnds);
			{
				unsigned i;
				for (i = 0; i < windowNames.len; i++)
					free(windowNames.d[i]);
			}
			EA_CLEAR(windowNames);
			EnumWindows(EnumWinProc, 0);
			{
				unsigned i;
				for (i = 0; i < windowHwnds.len; i++)
				{
					SendMessage(hListBox, LB_ADDSTRING, 0,
								(LPARAM)windowNames.d[i]);
					SendMessage(hListBox, LB_SETITEMDATA, i, (LPARAM)i);
				}
			}
			SendMessage(hListBox, LB_SETCURSEL, 0, 0);
			break;
		case M_SHOW:
			ShowWindow(windowHwnds.d[listSel], SW_SHOW);
			break;
		case M_HIDE:
			ShowWindow(windowHwnds.d[listSel], SW_HIDE);
			break;
		case M_REVEAL:
		{
			DWORD procID;
			HANDLE hProc;
			char filename[100];
			GetWindowThreadProcessId(windowHwnds.d[listSel], &procID);
			hProc = OpenProcess(PROCESS_QUERY_INFORMATION |
								PROCESS_VM_READ, FALSE, procID);
			GetModuleFileNameEx(hProc, NULL, filename, 100);
			CloseHandle(hProc);
			MessageBox(hwnd, filename, "Process File Name", MB_OK);
			break;
		}
		case M_EXIT:
			PostQuitMessage(0);
		case MAINLBOX:
			switch (HIWORD(wParam))
			{
			case LBN_SELCHANGE:
				listSel = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
				break;
			}
			break;
		}
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
					 LPSTR lpCmdLine, int nShowCmd)
{
	HWND hwnd;
	MSG msg;
	WNDCLASSEX wndClass;

	EA_INIT(HWND, windowHwnds, 16);
	EA_INIT(char_ptr, windowNames, 16);
	msg.wParam = 0;

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wndClass.lpszMenuName = (LPCTSTR)MAINMENU;
	wndClass.lpszClassName = "mainWindow";
	wndClass.hIconSm = NULL;

	if (!RegisterClassEx(&wndClass))
		goto cleanup;

	g_hInst = hInstance;

	hwnd = CreateWindowEx(NULL, "mainWindow", "Window Hider",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (!hwnd)
		goto cleanup;

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

cleanup:
	EA_DESTROY(HWND, windowHwnds);
	{
		unsigned i;
		for (i = 0; i < windowNames.len; i++)
			free(windowNames.d[i]);
	}
	EA_DESTROY(char_ptr, windowNames);
	return (int)msg.wParam;
}

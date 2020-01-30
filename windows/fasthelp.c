/* Performs a program automated help lookup feature */
/* Supports Windows text editing standards and Emacs text editing */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0410
#define _WIN32_WINDOWS 0x0410
#define WINVER 0x0410
#include <windows.h>
#include <shellapi.h> /* For flashy user interface */
#include "../bool.h"
#define MYWM_NOTIFYICON		(WM_APP+100) /* Flashy user interface again */

HWND helpWin = NULL;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam);
void GetHelp();
void UnregisterNotifyIcon(HWND hwnd);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
					 LPSTR lpCmdLine, int nShowCmd)
{
	/* Get the help window.  */
	EnumWindows(EnumWindowsProc, 0);
	if (helpWin == NULL)
	{
		if (lpCmdLine[0] != '-' || lpCmdLine[1] != 'e')
			MessageBox(NULL, "The Platform SDK window must be\n"
					   "open before using this program.",
					   "Fast Help", MB_ICONERROR);
		return 1;
	}

	/* Get our text to look up.  */
	if (lpCmdLine[0] == '-' && lpCmdLine[1] == 'e')
	{
		/* We will use Emacs copying user interface.  */
		INPUT copyInput[12];
		unsigned i = 0;

		OpenIcon(helpWin);
		SetForegroundWindow(helpWin);
		Sleep(250);

		/* Initialize common elements.  */
		while (i < 12)
		{
			copyInput[i].type = INPUT_KEYBOARD;
			copyInput[i].ki.wVk = 0;
			copyInput[i].ki.wScan = 0;
			copyInput[i].ki.dwFlags = 0;
			copyInput[i].ki.time = 0;
			copyInput[i].ki.dwExtraInfo = GetMessageExtraInfo();
			i++;
		}
		i = 0;

		/* First make sure the index window is selected.  */
		copyInput[i].ki.wVk = VK_CONTROL;
		i++;

		copyInput[i].ki.wVk = VK_MENU;
		i++;

		copyInput[i].ki.wVk = VK_F2;
		i++;

		copyInput[i].ki.wVk = VK_F2;
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		copyInput[i].ki.wVk = VK_MENU;
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		copyInput[i].ki.wVk = VK_CONTROL;
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;
		/* Control down */
		copyInput[i].ki.wVk = VK_CONTROL;
		i++;
		/* 'V' down */
		copyInput[i].ki.wVk = (WORD)'V';
		i++;
		/* 'V' up */
		copyInput[i].ki.wVk = (WORD)'V';
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;
		/* Control up */
		copyInput[i].ki.wVk = VK_CONTROL;
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;
		/* carriage-return down */
		copyInput[i].ki.wVk = (WORD)'\r';
		i++;
		/* carriage-return up */
		copyInput[i].ki.wVk = (WORD)'\r';
		copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		/* Send input */
		SendInput(12, copyInput, sizeof(INPUT));

		return 0;
	}

	/* Give hot key user interface.  */
	HWND hwnd;			/* Window handle */
	WNDCLASSEX wcex;	/* Window class */
	MSG msg;			/* Message structure */

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)101);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	hwnd = CreateWindowEx(NULL, "mainWindow", "Fast Help",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window position does not matter */
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window size does not matter */
		NULL, 0, hInstance, NULL);

	if (!hwnd)
		return 0;

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	TCHAR winText[28];
	GetWindowText(hwnd, winText, 28);
	if (CompareString(LOCALE_USER_DEFAULT, 0, winText, 27,
					  "Microsoft Platform SDK (R2)", 27) == CSTR_EQUAL)
	{
		/* We found the help window.  */
		helpWin = hwnd;
		return FALSE;
	}
	else
		return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam)
{
	static bool notifyIconOn = false;
	switch (uMsg)
	{
	case WM_CREATE:
		RegisterHotKey(hwnd, 1, NULL, VK_F11);
		break;
	case WM_DESTROY:
		if (notifyIconOn == true)
			UnregisterNotifyIcon(hwnd);
		UnregisterHotKey(hwnd, 1);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rt;
		BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rt);
		DrawText(ps.hdc,
			"Press F11 to perform fast help lookup at the caret.", 51, &rt,
			DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_HOTKEY:
		helpWin = NULL;
		EnumWindows(EnumWindowsProc, NULL);
		if (helpWin == NULL)
		{
			MessageBox(NULL, "The Platform SDK window must be\n"
					   "open when using this program.",
					   "Fast Help", MB_ICONERROR);
			DestroyWindow(hwnd);
		}
		else
			GetHelp();
		break;
	case WM_SIZE: /* Flashy user interface */
		if (wParam == SIZE_MINIMIZED)
		{
			NOTIFYICONDATA nid;
			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.uID = 1;
			nid.hWnd = hwnd;
			nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
			nid.uCallbackMessage = MYWM_NOTIFYICON;
			nid.hIcon =
				(HICON)LoadImage((HINSTANCE)GetWindowLongPtr(hwnd,
				GWLP_HINSTANCE), (LPCTSTR)101, IMAGE_ICON, 16, 16, LR_SHARED);
			lstrcpy(nid.szTip, "Fast Help");

			if (Shell_NotifyIcon(NIM_ADD, &nid))
			{
				ShowWindow(hwnd, SW_HIDE);
				notifyIconOn = true;
			}
		}
		break;
	case MYWM_NOTIFYICON: /* Flashy user interface again */
		switch (lParam)
		{
		case WM_LBUTTONDOWN:
			UnregisterNotifyIcon(hwnd);
			notifyIconOn = false;
			ShowWindow(hwnd, SW_SHOW);
			OpenIcon(hwnd);
			break;
		}
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GetHelp()
{
	/* We will use Windows copying user interface.  */
	INPUT copyInput[12];

	/* Initalize common elements.  */
	unsigned i = 0;
	while (i < 10)
	{
		copyInput[i].type = INPUT_KEYBOARD;
		copyInput[i].ki.wVk = 0;
		copyInput[i].ki.wScan = 0;
		copyInput[i].ki.dwFlags = 0;
		copyInput[i].ki.time = 0;
		copyInput[i].ki.dwExtraInfo = GetMessageExtraInfo();
		i++;
	}
	i = 0;

	/* Control down */
	copyInput[i].ki.wVk = VK_CONTROL;
	i++;

	/* left arrow down */
	copyInput[i].ki.wVk = VK_LEFT;
	i++;

	/* left arrow up */
	copyInput[i].ki.wVk = VK_LEFT;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* Shift down */
	copyInput[i].ki.wVk = VK_SHIFT;
	i++;

	/* right arrow down */
	copyInput[i].ki.wVk = VK_RIGHT;
	i++;

	/* right arrow up */
	copyInput[i].ki.wVk = VK_RIGHT;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* Shift up */
	copyInput[i].ki.wVk = VK_SHIFT;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* 'C' down */
	copyInput[i].ki.wVk = (WORD)'C';
	i++;

	/* 'C' up */
	copyInput[i].ki.wVk = (WORD)'C';
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* Control up */
	copyInput[i].ki.wVk = VK_CONTROL;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* Send the input.  */
	SendInput(10, copyInput, sizeof(INPUT));

	OpenIcon(helpWin);
	SetForegroundWindow(helpWin);
	Sleep(250);

	/*//Get the help lookup data
	char* helpData = NULL;
	if (OpenClipboard(NULL))
	{
		HANDLE clipMem = GetClipboardData(CF_TEXT);
		char* clipCont = (char*)GlobalLock(clipMem);
		if (clipCont != NULL)
		{
			helpData = (char*)malloc(strlen(clipCont) + 1);
			strcpy(helpData, clipCont);
		}
		GlobalUnlock(clipMem);
		CloseClipboard();
	}
	if (helpData == NULL)
		return;

	{ //Send the help lookup input
		unsigned numHelpLook = strlen(helpData) * 2 + 6;
		INPUT* helpLook = (INPUT*)malloc(sizeof(INPUT) * (numHelpLook + 2));

		//Initalize common elements
		i = 0;
		while (i < numHelpLook + 2)
		{
			helpLook[i].type = INPUT_KEYBOARD;
			helpLook[i].ki.wVk = 0;
			helpLook[i].ki.wScan = 0;
			helpLook[i].ki.dwFlags = 0;
			helpLook[i].ki.time = 0;
			helpLook[i].ki.dwExtraInfo = GetMessageExtraInfo();
			i++;
		}

		i = 0;
		//First make sure the index window is selected
		helpLook[i].ki.wVk = VK_CONTROL;
		i++;

		helpLook[i].ki.wVk = VK_MENU;
		i++;

		helpLook[i].ki.wVk = VK_F2;
		i++;

		helpLook[i].ki.wVk = VK_F2;
		helpLook[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		helpLook[i].ki.wVk = VK_MENU;
		helpLook[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		helpLook[i].ki.wVk = VK_CONTROL;
		helpLook[i].ki.dwFlags = KEYEVENTF_KEYUP;
		i++;

		i = 6;
		for (unsigned j = 0; i < numHelpLook; j++)
		{
			//Key down
			helpLook[i].ki.wVk = VkKeyScan(helpData[j]);
			i++;

			//Key up
			helpLook[i].ki.wVk = VkKeyScan(helpData[j]);
			helpLook[i].ki.dwFlags = KEYEVENTF_KEYUP;
			i++;
		}
		//Include the carriage-return
			//Key down
			helpLook[i].ki.wVk = VkKeyScan('\r');
			i++;

			//Key up
			helpLook[i].ki.wVk = VkKeyScan('\r');
			helpLook[i].ki.dwFlags = KEYEVENTF_KEYUP;
			i++;

		//Send input!
		SendInput(numHelpLook + 2, helpLook, sizeof(INPUT));
		free(helpData);
		free(helpLook);
	}*/

	/* Initialize common elements.  */
	i = 0;
	while (i < 12)
	{
		copyInput[i].type = INPUT_KEYBOARD;
		copyInput[i].ki.wVk = 0;
		copyInput[i].ki.wScan = 0;
		copyInput[i].ki.dwFlags = 0;
		copyInput[i].ki.time = 0;
		copyInput[i].ki.dwExtraInfo = GetMessageExtraInfo();
		i++;
	}
	i = 0;

	/* First make sure the index window is selected.  */
	copyInput[i].ki.wVk = VK_CONTROL;
	i++;

	copyInput[i].ki.wVk = VK_MENU;
	i++;

	copyInput[i].ki.wVk = VK_F2;
	i++;

	copyInput[i].ki.wVk = VK_F2;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	copyInput[i].ki.wVk = VK_MENU;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	copyInput[i].ki.wVk = VK_CONTROL;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;
	/* Control down */
	copyInput[i].ki.wVk = VK_CONTROL;
	i++;
	/* 'V' down */
	copyInput[i].ki.wVk = (WORD)'V';
	i++;
	/* 'V' up */
	copyInput[i].ki.wVk = (WORD)'V';
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;
	/* Control up */
	copyInput[i].ki.wVk = VK_CONTROL;
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;
	/* carriage-return down */
	copyInput[i].ki.wVk = (WORD)'\r';
	i++;
	/* carriage-return up */
	copyInput[i].ki.wVk = (WORD)'\r';
	copyInput[i].ki.dwFlags = KEYEVENTF_KEYUP;
	i++;

	/* Send input.  */
	SendInput(12, copyInput, sizeof(INPUT));
}

void UnregisterNotifyIcon(HWND hwnd)
{
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.uID = 1;
	nid.hWnd = hwnd;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

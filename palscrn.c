/* palscrn.cpp -- Correctly saves paletted color screens to a bitmap.  */
/* This code is not designed for video capture.  */
/* Compiling: Struct alignment must be set to 1 byte. See GetScreen().  */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h> /* For flashy user interface */
#ifdef BENCHMARK
#include <mmsystem.h>
#endif
#include <stdio.h> /* for sprintf() */
#include <string.h>
#include "bool.h"
#define MYWM_NOTIFYICON		(WM_APP+100) /* Flashy user interface again */

unsigned shotNum = 1; /* Screen shot number */

/* Flashy user interface again */
#define DISP_LEN 240
#define ERROR_LOG_MAX 65536
#define EL_ERROR 240
TCHAR dispText[DISP_LEN+ERROR_LOG_MAX+EL_ERROR];
LPTSTR errorLog = dispText;
LPTSTR elInsert = dispText;
BOOL elTruncMsg = FALSE;

#define EL_APPEND(args) \
	if (elInsert - errorLog < ERROR_LOG_MAX) \
	{ \
		sprintf args; \
		elInsert += strlen(elInsert); \
	}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam);
void GetScreen();
LPTSTR FmtErrorDesc(DWORD errorCode);
void UnregisterNotifyIcon(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
	LPSTR lpCmdLine, int nShowCmd)
{
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

	hwnd = CreateWindowEx(0, "mainWindow", "Palette Screener",
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool notifyIconOn = false;
	switch (uMsg)
	{
	case WM_CREATE:
		RegisterHotKey(hwnd, 1, 0, VK_SNAPSHOT);
		ZeroMemory(errorLog, sizeof(errorLog));
		/* MessageBox(NULL, "If this program crashes, never send "
			"information to Microsoft.", "Note", MB_OK | MB_ICONINFORMATION);

			We could use AddERExcludedApplication() instead.  In
			practice, this application has never crashed.  */
		strcpy(dispText, "\nPress Print Screen to take a picture and "
			"save as it a bitmap. Each bitmap is saved as scr*.bmp. '*' "
			"starts out as '00001' and is incremented each time you save a "
			"screen until it reaches '4294967295', then it goes back "
			"to '00000'. Files are never overwritten.\n\n"
			"Error Log: (Press C to copy)\n");
		errorLog += strlen(dispText);
		elInsert = errorLog;
		break;
	case WM_DESTROY:
		UnregisterHotKey(hwnd, 1);
		if (notifyIconOn == true)
		{
			MessageBox(NULL, "You didn't close this program the right way!",
				NULL, MB_OK | MB_ICONERROR);
			UnregisterNotifyIcon(hwnd);
		}
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rt;
		BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rt);
		if (elInsert - errorLog >= ERROR_LOG_MAX && !elTruncMsg)
		{
			elTruncMsg = TRUE;
			strcpy(elInsert, "Error log truncated.\n");
			elInsert += strlen(elInsert);
		}
		DrawText(ps.hdc, dispText,
			elInsert - dispText, &rt, DT_LEFT | DT_WORDBREAK);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_HOTKEY:
		if (wParam == 1)
		{
#ifdef BENCHMARK
			EL_APPEND((elInsert, "timestamp %u start\n", timeGetTime()));
			GetScreen();
			EL_APPEND((elInsert, "timestamp %u finish\n", timeGetTime()));
#else
			GetScreen();
#endif
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	case WM_KEYDOWN: /* Why bother with all this extra user interface
					    stuff? (Release version) */
		switch (wParam)
		{
		case 'C':
			if (OpenClipboard(hwnd))
			{
				HANDLE clipData = GlobalAlloc(GMEM_MOVEABLE,
					elInsert - errorLog + 1);
				memcpy(GlobalLock(clipData), errorLog,
					elInsert - errorLog + 1);
				GlobalUnlock(clipData);
				EmptyClipboard();
				SetClipboardData(CF_TEXT, clipData);
				CloseClipboard();
			}
			else
				MessageBox(hwnd, "Your data was not copied because "
					"the clipboard could not be opened.",
					NULL, MB_OK | MB_ICONERROR);
			break;
		}
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
			nid.hIcon = (HICON)LoadImage((HINSTANCE)GetWindowLong(hwnd,
				GWL_HINSTANCE), (LPCTSTR)101, IMAGE_ICON, 16, 16, LR_SHARED);
			lstrcpy(nid.szTip, "Palette Screener");

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

/* GetScreen(): The important part of this program.  */
void GetScreen()
{
	HDC hdc = GetDC(NULL);
	/* Note: In an older version of Palette Screener, we use to
	   capture the screen the conventional way by BitBlt()ing the
	   screen to a memory bitmap, but we no longer do that anymore for
	   two reasons:
	     1. It's slower (by ~0.4 seconds), probably because...
	     2. Getting paletted image colors correct is impossible due
            to inevitable format conversions.
       Thus, rather than capturing the screen the conventional way, we
       use GetCurrentObject() to get a direct handle to the screen's
       HBITMAP, then we pass that handle to GetDIBits().  */
	UINT numPalEntries = GetSystemPaletteEntries(hdc, 0, 0, NULL);
	/* Struct alignment must be set to 1 byte so that this structure
	   remains packed.  */
#pragma pack(push,1)
	struct
	{
		BITMAPFILEHEADER bfh;
		struct
		{
			BITMAPINFOHEADER bmiHeader;
			RGBQUAD bmiColors[256];
		} bmi;
	} h;
#pragma pack(pop)
	unsigned char* bits;

	ZeroMemory(&h.bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	h.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	/* First fill in BITMAPINFO, then get the bits.  */
	if (!GetDIBits(hdc, GetCurrentObject(hdc, OBJ_BITMAP),
		0, 0, NULL, (BITMAPINFO*)&h.bmi, DIB_RGB_COLORS))
	{
		LPTSTR errorDesc = FmtErrorDesc(GetLastError());
		EL_APPEND((elInsert,
			"scr%05u.bmp: GetDIBits() for BITMAPINFO failed: %s",
			shotNum));
		LocalFree(errorDesc);
		goto cleanup;
	}
	/* For 4-bit and 1-bit bitmaps, the system might not return the
	   correct number of palette entries when
	   GetSystemPaletteEntries() is called, but it will store the
	   palette when GetDIBits() is called.  */
	if (h.bmi.bmiHeader.biBitCount < 8 && numPalEntries == 0)
		numPalEntries = 1 << h.bmi.bmiHeader.biBitCount;
	if (h.bmi.bmiHeader.biBitCount < 8 &&
		h.bmi.bmiHeader.biBitCount != 4 &&
		h.bmi.bmiHeader.biBitCount != 1)
	{
		EL_APPEND((elInsert,
			"scr%05u.bmp: Warning: This program has not been tested with "
			"a color depth of %hu bits per pixel.\n", shotNum,
			h.bmi.bmiHeader.biBitCount));
	}
	/* if (h.bmi.bmiHeader.biBitCount == 16)
	{
		We will save bitmasks later, look below for
		  h.bmi.bmiHeader.biCompression == BI_BITFIELDS
	} */
	/* Note: We use to convert 32-bit bitmaps down to 24-bit bitmaps,
	   but we don't do that anymore because it slows down throughput
	   (by ~157 ms).  */
	/* if (h.bmi.bmiHeader.biBitCount == 32)
	{
		h.bmi.bmiHeader.biBitCount = 24;
		h.bmi.bmiHeader.biSizeImage =
			h.bmi.bmiHeader.biWidth * h.bmi.bmiHeader.biHeight * 3;
		h.bmi.bmiHeader.biCompression = BI_RGB;
	} */

	bits = (unsigned char *)malloc(h.bmi.bmiHeader.biSizeImage);
	if (bits == NULL)
	{
		EL_APPEND((elInsert,
			"scr%05u.bmp: Could not allocate memory.\n", shotNum));
		goto cleanup;
	}
	if (!GetDIBits(hdc, GetCurrentObject(hdc, OBJ_BITMAP),
		0, h.bmi.bmiHeader.biHeight, bits,
		(BITMAPINFO*)&h.bmi, DIB_RGB_COLORS))
	{
		LPTSTR errorDesc = FmtErrorDesc(GetLastError());
		EL_APPEND((elInsert, "scr%05u.bmp: GetDIBits() failed: %s",
			shotNum, errorDesc));
		LocalFree(errorDesc);
		goto cleanup;
	}
#ifdef BENCHMARK
	EL_APPEND((elInsert, "timestamp %u GetDIBits() finished\n",
		timeGetTime()));
#endif

	/* Now we can prepare to write out the actual bitmap file.  */
	{
		unsigned headerSize = sizeof(BITMAPFILEHEADER) +
			sizeof(BITMAPINFOHEADER);

		TCHAR fileName[100];
		DWORD bytesWritten; /* For WriteFile */
		HANDLE hFile;

		if (h.bmi.bmiHeader.biBitCount <= 8 && numPalEntries > 0)
			headerSize += sizeof(RGBQUAD) * numPalEntries;
		if ((h.bmi.bmiHeader.biBitCount == 16 ||
			 h.bmi.bmiHeader.biBitCount == 32) &&
			h.bmi.bmiHeader.biCompression == BI_BITFIELDS)
			headerSize += sizeof(RGBQUAD) * 3;

		h.bfh.bfType = 0x4D42; /* "BM" byteswapped */
		h.bfh.bfSize = headerSize + h.bmi.bmiHeader.biSizeImage;
		h.bfh.bfReserved1 = 0;
		h.bfh.bfReserved2 = 0;
		h.bfh.bfOffBits = headerSize;

		/* NOW we're really ready.  */
		sprintf(fileName, "scr%05u.bmp", shotNum);
		hFile = CreateFile(fileName,
			GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			EL_APPEND((elInsert,
				"scr%05u.bmp: Could not create a new file.\n",
				shotNum));
			goto cleanup;
		}
		WriteFile(hFile, &h, headerSize, &bytesWritten, NULL);
		if (bytesWritten != headerSize)
		{
			EL_APPEND((elInsert,
				"scr%05u.bmp: Could not write the bitmap headers.\n",
				shotNum));
			goto cleanup;
		}
		WriteFile(hFile, bits, h.bmi.bmiHeader.biSizeImage,
			&bytesWritten, NULL);
		if (bytesWritten != h.bmi.bmiHeader.biSizeImage)
		{
			EL_APPEND((elInsert,
				"scr%05u.bmp: Could not write the bitmap data.\n",
				shotNum));
			goto cleanup;
		}
		if (!CloseHandle(hFile))
		{
			EL_APPEND((elInsert,
				"scr%05u.bmp: Could not close the file.\n",
				shotNum));
			goto cleanup;
		} /* Now we're done.  */
	}

	/* Clean up */
cleanup:
	ReleaseDC(NULL, hdc);
	shotNum++;
	free(bits);
}

/* Note: This error description will normally have a newline on the
   end.  */
LPTSTR FmtErrorDesc(DWORD errorCode)
{
	LPTSTR lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL);

	return lpMsgBuf;
}

void UnregisterNotifyIcon(HWND hwnd)
{
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.uID = 1;
	nid.hWnd = hwnd;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

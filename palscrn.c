/* palscrn.cpp - Correctly saves 256 color screens to a bitmap */
/* Also saves 32 bpp and 16 bpp to bitmaps */
/* This code isn't optimized, so don't rely on using it for videos */
/* Compiling: Struct alignment must be set to 1 byte. See GetScreen(). */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h> /* For flashy user interface */
#include <string.h>
#include "bool.h"
#define MYWM_NOTIFYICON		(WM_APP+100) /* Flashy user interface again */

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void GetScreen();
void UnregisterNotifyIcon(HWND hwnd);

TCHAR errorLog[200]; /* Flashy user interface again */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
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

	hwnd = CreateWindowEx(NULL, "mainWindow", "Palette Screener",
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
		RegisterHotKey(hwnd, 1, NULL, VK_SNAPSHOT);
		ZeroMemory(errorLog, sizeof(errorLog));
		/* MessageBox(NULL, "If this program crashes, never send information to Microsoft.", "Note", MB_OK | MB_ICONINFORMATION); Release version only */
		break;
	case WM_DESTROY:
		UnregisterHotKey(hwnd, 1);
		if (notifyIconOn == true)
		{
			MessageBox(NULL, "You didn't close this program the right way!", NULL, MB_OK | MB_ICONERROR);
			UnregisterNotifyIcon(hwnd);
		}
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rt;
		TCHAR outText[1000];
		BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rt);
		wsprintf(outText, "\nPress Print Screen to take a picture and save as a bitmap. \
			Each bitmap is saved as screen*.bmp. '*' starts out as '1' and is incremented \
			each time you save a screen untill it reaches '4294967295', then it goes back \
			to '0'. Files are never overwritten.\n\nError Log: (Press C to copy)\n%s", errorLog);
		DrawText(ps.hdc, outText,
			strlen(outText), &rt, DT_LEFT | DT_WORDBREAK);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_HOTKEY:
		if (wParam == 1)
		{
			GetScreen();
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		}
		break;
	case WM_KEYDOWN: /* Why bother with all this extra user interface stuff? (Release version) */
		switch (wParam)
		{
		case 'C':
			if (OpenClipboard(hwnd))
			{
				HANDLE clipData = GlobalAlloc(GMEM_MOVEABLE, strlen(errorLog) + 1);
				memcpy(GlobalLock(clipData), errorLog, strlen(errorLog) + 1);
				GlobalUnlock(clipData);
				EmptyClipboard();
				SetClipboardData(CF_TEXT, clipData);
				CloseClipboard();
			}
			else
				MessageBox(hwnd, "Your data was not copied because the clipboard could not be opened.",
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
			nid.hIcon = (HICON)LoadImage((HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), (LPCTSTR)101, IMAGE_ICON, 16, 16, LR_SHARED);
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

/* GetScreen(): The important part of this program If the current
   display mode is 8 bpp, this function will copy the current realized
   system palette and the screen palette indices and save those to a
   file. If the screen is 16 bpp, this function will save a 16 bpp
   bitmap with the bitmasks. If the screen is 32 bpp, this function
   will get a 24 bpp DI bits then save a 24 bpp bitmap.  */
void GetScreen()
{
	int width = GetSystemMetrics(SM_CXSCREEN),
		height = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdc);
	HBITMAP saveBitmap = CreateCompatibleBitmap(hdc, width, height);
	PALETTEENTRY palette[256];
	struct /* Struct alignment must be set to 1 byte so... */
	{
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD bmiColors[256]; /* that this comes right after BITMAPINFOHEADER */
	} bmi;
	unsigned char* bits;
	SelectObject(hdcMem, saveBitmap);
	GetSystemPaletteEntries(hdc, 0, 256, palette);
	BitBlt(hdcMem, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
	ReleaseDC(NULL, hdc);

	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	/* Fill BITMAPINFO then get the bits */
	if (!GetDIBits(hdcMem, saveBitmap, 0, height, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS))
	{
		wsprintf(errorLog, "%sGetDIBits() for BITMAPINFO failed. ErrorCode: %i\r\n", errorLog, GetLastError());
		goto cleanup;
	}
	if (bmi.bmiHeader.biBitCount < 8)
		wsprintf(errorLog, "%sIs it even possible to set the screen to less than 8 bpp? This program isn't written to handle that.\r\n", errorLog);
	if (bmi.bmiHeader.biBitCount == 8)
	{
		/* We will use the palette entries for the RGBQUAD array */
		int i;
		for (i = 0; i < 256; i++)
		{
			bmi.bmiColors[i].rgbBlue = palette[i].peBlue;
			bmi.bmiColors[i].rgbGreen = palette[i].peGreen;
			bmi.bmiColors[i].rgbRed = palette[i].peRed;
			bmi.bmiColors[i].rgbReserved = 0;
		}
	}
	/*if (bmi.bmiHeader.biBitCount == 16)
	{
		We will later save bitmasks if (bmi.bmiHeader.biCompression == BI_BITFIELDS)
	}*/
	if (bmi.bmiHeader.biBitCount == 32)
	{
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biSizeImage = width * height * 3;
		bmi.bmiHeader.biCompression = BI_RGB;
	}
	bits = (unsigned char *)malloc(bmi.bmiHeader.biSizeImage);
	if (!GetDIBits(hdcMem, saveBitmap, 0, height, bits, (BITMAPINFO*)&bmi, DIB_RGB_COLORS))
	{
		wsprintf(errorLog, "%sGetDIBits() failed. ErrorCode: %i\r\n", errorLog, GetLastError());
		goto cleanup;
	}
	/* GetDIBits could have garbled the color table */
	if (bmi.bmiHeader.biBitCount == 8)
	{
		/* We will use the palette entries for the RGBQUAD array */
		int i;
		for (i = 0; i < 256; i++)
		{
			bmi.bmiColors[i].rgbBlue = palette[i].peBlue;
			bmi.bmiColors[i].rgbGreen = palette[i].peGreen;
			bmi.bmiColors[i].rgbRed = palette[i].peRed;
			bmi.bmiColors[i].rgbReserved = 0;
		}
	}

	/* FINALLY done setting up (not yet) */
	{
		BITMAPFILEHEADER bfh;
		unsigned headerSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

		TCHAR fileName[100];
		static unsigned shotNum = 1; /* Screen shot number */
		DWORD bytesWritten; /* For WriteFile */
		HANDLE hFile;

		if (bmi.bmiHeader.biBitCount == 8)
			headerSize += 1024;
		if (bmi.bmiHeader.biBitCount == 16 && bmi.bmiHeader.biCompression == BI_BITFIELDS)
			headerSize += 12;

		bfh.bfType = 0x4D42; /* "BM" byteswapped */
		bfh.bfSize = headerSize + bmi.bmiHeader.biSizeImage;
		bfh.bfReserved1 = 0;
		bfh.bfReserved2 = 0;
		bfh.bfOffBits = headerSize;

		bmi.bmiHeader.biXPelsPerMeter = 0;
		bmi.bmiHeader.biYPelsPerMeter = 0;

		/* NOW we're really ready */
		wsprintf(fileName, "screen%i.bmp", shotNum);
		shotNum++;
		hFile = CreateFile(fileName,
			GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			wsprintf(errorLog, "%sCould not create a new file.\r\n", errorLog);
			goto cleanup;
		}
		WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &bytesWritten, NULL);
		if (bytesWritten != sizeof(BITMAPFILEHEADER))
		{
			wsprintf(errorLog, "%sCould not write the bitmap file header.\r\n", errorLog);
			goto cleanup;
		}
		WriteFile(hFile, &bmi.bmiHeader, sizeof(BITMAPINFOHEADER), &bytesWritten, NULL);
		if (bytesWritten != sizeof(BITMAPINFOHEADER))
		{
			wsprintf(errorLog, "%sCould not write the bitmap info header.\r\n", errorLog);
			goto cleanup;
		}
		if (bmi.bmiHeader.biBitCount == 8)
		{
			WriteFile(hFile, bmi.bmiColors, sizeof(bmi.bmiColors), &bytesWritten, NULL);
			if (bytesWritten != sizeof(bmi.bmiColors))
			{
				wsprintf(errorLog, "%sCould not write the bitmap palette.\r\n", errorLog);
				goto cleanup;
			}
		}
		if (bmi.bmiHeader.biBitCount == 16 && bmi.bmiHeader.biCompression == BI_BITFIELDS)
		{
			WriteFile(hFile, bmi.bmiColors, sizeof(RGBQUAD) * 3, &bytesWritten, NULL);
			if (bytesWritten != sizeof(RGBQUAD) * 3)
			{
				wsprintf(errorLog, "%sCould not write the bitmasks.\r\n", errorLog);
				goto cleanup;
			}
		}
		WriteFile(hFile, bits, bmi.bmiHeader.biSizeImage, &bytesWritten, NULL);
		if (bytesWritten != bmi.bmiHeader.biSizeImage)
		{
			wsprintf(errorLog, "%sCould not write the bitmap data.\r\n", errorLog);
			goto cleanup;
		}
		if (!CloseHandle(hFile))
		{
			wsprintf(errorLog, "%sCould not close the file.\r\n", errorLog);
			goto cleanup;
		} /* Now we're done */
	}

	/* Clean up */
cleanup:
	free(bits);
	DeleteObject(saveBitmap);
	DeleteDC(hdcMem);
}

void UnregisterNotifyIcon(HWND hwnd)
{
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.uID = 1;
	nid.hWnd = hwnd;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

/* fast_palscrn.cpp - Correctly saves 256 color screens to a bitmap */
/* Also saves 32 bpp and 16 bpp to bitmaps */
/* This version has some user friendliness and all error handling removed */
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
		break;
	case WM_DESTROY:
		UnregisterHotKey(hwnd, 1);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rt;
		BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rt);
		DrawText(ps.hdc, "Press Print Screen to take a picture and save as a bitmap.",
			59, &rt, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_HOTKEY:
		GetScreen();
		break;
	case WM_SIZE: /* Flashy user interface */
		if (wParam == SIZE_MINIMIZED)
		{
			NOTIFYICONDATA nid;
			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.uID = 1;
			nid.hWnd = hwnd;
			nid.uFlags = NIF_ICON | NIF_MESSAGE;
			nid.uCallbackMessage = MYWM_NOTIFYICON;
			nid.hIcon = LoadIcon((HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), (LPCTSTR)101);

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
	GetDIBits(hdcMem, saveBitmap, 0, height, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
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
	if (bmi.bmiHeader.biBitCount == 32)
	{
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biSizeImage = width * height * 3;
		bmi.bmiHeader.biCompression = BI_RGB;
	}
	bits = (unsigned char *)malloc(bmi.bmiHeader.biSizeImage);
	GetDIBits(hdcMem, saveBitmap, 0, height, bits, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
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
		WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &bytesWritten, NULL);
		WriteFile(hFile, &bmi.bmiHeader, sizeof(BITMAPINFOHEADER), &bytesWritten, NULL);
		if (bmi.bmiHeader.biBitCount == 8)
		{
			WriteFile(hFile, bmi.bmiColors, sizeof(bmi.bmiColors), &bytesWritten, NULL);
		}
		if (bmi.bmiHeader.biBitCount == 16 && bmi.bmiHeader.biCompression == BI_BITFIELDS)
		{
			WriteFile(hFile, bmi.bmiColors, sizeof(RGBQUAD) * 3, &bytesWritten, NULL);
		}
		WriteFile(hFile, bits, bmi.bmiHeader.biSizeImage, &bytesWritten, NULL);
		CloseHandle(hFile); /* Now we're done */
	}

	/* Clean up */
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

/* Provides a convenient GUI for laying out some bits of an image
   signal left to right, top to bottom in a given window size.  */

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <stdlib.h>
#include "bool.h"

/* So, here's how to do it:

Provide a bitmap display of a fixed file name.  When the user hits the
'r' key, reread the file and update the display.  When the user
changes the size of the display, update the bitmap to draw into those
bits.  So maybe it would be best to keep the entire bitmap in memory,
then whenever the display size gets changed, the display is
repainted.  */

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam);
void ReadPalette();
bool UpdateGuessBits();
INT_PTR CALLBACK FrameGotoProc(HWND hDlg, UINT uMsg, WPARAM wParam,
							   LPARAM lParam);

/* Globals */
bool usePalette = false;
RGBQUAD palette[256];
bool exclaim = false;
unsigned char* guessBits = NULL; /* Holds the bits we are trying to decipher */
unsigned guessSize = 0;
unsigned fineSize = 128;
unsigned stepBytes = 1;
bool vertical = false;
HINSTANCE g_hInst;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
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
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Globally save the application instance.  */
	g_hInst = hInstance;

	hwnd = CreateWindowEx(0, "mainWindow", "Bit Guesser",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window position does not matter */
		CW_USEDEFAULT, CW_USEDEFAULT,
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam)
{
	static HBRUSH hbrGreen;

	switch (uMsg)
	{
	case WM_CREATE:
		hbrGreen = CreateSolidBrush(RGB(0, 255, 0));
		ReadPalette();
		UpdateGuessBits();
		break;
	case WM_DESTROY:
		DeleteObject(hbrGreen);
		free(guessBits);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		/* Redraw all bits, left to right within the given window size.  */
		RECT rt;
		PAINTSTRUCT ps;
		long x = 0, y = 0;
		long w_max, g_max;

		if (guessBits == NULL)
			break;
		BeginPaint(hwnd, &ps);

		GetClientRect(hwnd, &rt);
		if (rt.right < 128 && !vertical)
		{
			RECT nullEdge = { fineSize, 0, rt.right, rt.bottom };
			FillRect(ps.hdc, &nullEdge, hbrGreen);
			rt.right = fineSize;
		}
		w_max = rt.right * rt.bottom;
		g_max = guessSize;

		if (!vertical)
		{
			while (rt.right * y + x < w_max &&
				   (rt.right * y + x) * stepBytes < g_max)
			{
				int value = guessBits[(rt.right*y + x)*stepBytes];
				COLORREF color = (usePalette)
					? RGB(palette[value].rgbRed,
						  palette[value].rgbGreen,
						  palette[value].rgbBlue)
					: RGB(value, value, value);
				if (exclaim && value == 0x80)
					color = RGB(255, 0, 0);
				SetPixel(ps.hdc, x, y, color);
				x++;
				if (x == rt.right)
				{ x = 0; y++; }
			}
		}
		else
		{
			while (rt.bottom * x + y < w_max &&
				   (rt.bottom * x + y) * stepBytes < g_max)
			{
				int value = guessBits[(rt.bottom*x + y)*stepBytes];
				COLORREF color = (usePalette)
					? RGB(palette[value].rgbRed,
						  palette[value].rgbGreen,
						  palette[value].rgbBlue)
					: RGB(value, value, value);
				if (exclaim && value == 0x80)
					color = RGB(255, 0, 0);
				SetPixel(ps.hdc, x, y, color);
				y++;
				if (y == rt.bottom)
				{ y = 0; x++; }
			}
		}

		EndPaint(hwnd, &ps);
		break;
	}
	case WM_COMMAND:
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'R':
			UpdateGuessBits();
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'W':
			if (fineSize > 1)
			{
				fineSize--;
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		case 'E':
			if (fineSize < 128)
			{
				fineSize++;
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		case 'V':
			vertical = !vertical;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'S':
			if (stepBytes > 1)
			{
				stepBytes--;
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		case 'D':
			stepBytes++;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'A':
			stepBytes = 1;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'P':
			usePalette = !usePalette;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'I':
			exclaim = !exclaim;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ReadPalette()
{
	HANDLE hFile;
	DWORD totBytesRead = 0;

	hFile = CreateFile("orly.pal", GENERIC_READ, FILE_SHARE_READ,
					   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	do
	{
		DWORD bytesRead = 0;
		BOOL status;
		status = ReadFile(hFile, ((unsigned char*)palette) + totBytesRead,
						  sizeof(palette) - totBytesRead, &bytesRead, NULL);
		totBytesRead += bytesRead;
		if (!status)
			break;
	} while (totBytesRead < sizeof(palette));
	CloseHandle(hFile);
}

bool UpdateGuessBits()
{
	HANDLE hFile;
	DWORD totBytesRead = 0;

	if (guessBits != NULL)
		free(guessBits);
	guessBits = NULL;
	guessSize = 0;

	hFile = CreateFile("gdata", GENERIC_READ, FILE_SHARE_READ,
					   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	guessSize = GetFileSize(hFile, NULL);
	guessBits = (unsigned char*)malloc(guessSize);
	if (guessBits == NULL)
	{
		CloseHandle(hFile);
		guessSize = 0;
		return false;
	}

	do
	{
		DWORD bytesRead = 0;
		BOOL status;
		status = ReadFile(hFile, &guessBits[totBytesRead],
						  guessSize - totBytesRead, &bytesRead, NULL);
		totBytesRead += bytesRead;
		if (!status)
			break;
	} while (totBytesRead < guessSize);
	CloseHandle(hFile);

	if (totBytesRead < guessSize)
	{
		free(guessBits); guessBits = NULL;
		guessSize = 0;
		return false;
	}

	return true;
}

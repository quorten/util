/* Provides a convenient GUI for looking at Orly Mohawk bitmap
   images.  */

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <stdio.h> /* for sprintf */
#include <stdlib.h>
#include <string.h>
#include <commdlg.h>
#include "../bool.h"

#ifndef __GNUC__
#define __attribute__(params)
#endif

/* NOTE: MHK files are in big endian.  */
/* NOTE: However, preprocessed bitmaps for this program are in the
   order of the machine the programs were run on.  */
#define LITTLE_ENDIAN
#ifdef LITTLE_ENDIAN
#define ntohs(s) (((s & 0xff) << 8) | ((s & 0xff00) >> 8))
#define ntohl(s) (((s & 0xff) << 24) | ((s & 0xff000000) >> 24) | \
				  ((s & 0xff00) << 8) | ((s & 0xff0000) >> 8))
#define c_ntohs(s) s = ntohs(s)
#define c_ntohl(s) s = ntohl(s)
#else
#define ntohs(s) (s)
#define ntohl(s) (s)
#define c_ntohs(s)
#define c_ntohl(s)
#endif

/* at x=-3, y=66, s=97% */
/* #define MAG1 267
#define MAG2 200 */
/* at x=-38, y=75, s=54% */
/* #define MAG1 (6 + 362 / 2)
#define MAG2 (11 + 291 / 2) */
#define MAG1 (377 / 2)
#define MAG2 (317 / 2)

struct bitmap_header_tag
{
	unsigned short width;
	unsigned short height;
	unsigned short bytes_per_row;
	union
	{
		unsigned short compression;
		/* NOTE: Verify that your compiler packs the following to a short: */
		struct
		{
			unsigned short bpp : 3;
			unsigned short has_palette : 1;
			unsigned short second_cmp : 4;
			unsigned short prim_cmp : 4;
		};
	};
} __attribute__((packed));
typedef struct bitmap_header_tag bitmap_header;

struct tagRGBAPALENTRY {
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
} __attribute__((packed));
typedef struct tagRGBAPALENTRY RGBAPALENTRY;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam);
BOOL ReadFileAll(HANDLE hFile, LPVOID lpBuffer, DWORD numBytes);
BOOL GetFilenameDialog(char* filename, char* dlgTitle, HWND hwnd,
					   unsigned type);
void ReadDefaultPalette();
void ReadPalette(const char *filename);
void GenBkPattern();
void ReadPatterns();
bool SwitchImage(HWND hwnd);

/* Globals */
unsigned displayNum = 0;
struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bmi;
HDC hDCMem = NULL;
HBITMAP displayBm = NULL;
unsigned maxDWidth = 640, maxDHeight = 480;
unsigned dispWidth = 0, dispHeight = 0;
unsigned char *pats[132];
bool patLoaded = false;
/* Display checkerboard pattern for transparent areas of a doodle.  */
bool checker = false;
bool doodleView = false;
/* Note: the doodle OFFSET is stored in little endian.  */
struct {
	short dpadX; short dpadY;
	short scrX; short scrY; } __attribute__((packed)) tOFF =
	{ -9, -183, 0, 0 };
struct {
	unsigned short page_number; /* Zero-based page number */
	unsigned short background;
    /* Sprite library identifier
       16 = Orly and Lancelot
       17 = Creatures and Plants
       18 = People
       19 = Other things */
	unsigned short sprite_lib;
	unsigned short sprite_num; /* Index of sprite from library */
	unsigned short sound_id; /* 1 by default */
	short x;
	short y;
	unsigned short scale; /* 100% by default */
}  __attribute__((packed)) tPAG =
	{ 0, 0, 0, 0, 1, 0, 0, 100 };
bool useTPAG = false;
HINSTANCE g_hInst;
int bah = 0;

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
	wcex.hbrBackground = NULL /* (HBRUSH)(COLOR_WINDOW+1) */;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Globally save the application instance.  */
	g_hInst = hInstance;

	hwnd = CreateWindowEx(0, "mainWindow", "Orly Viewer",
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
	static HBRUSH hbrRed;

	switch (uMsg)
	{
	case WM_CREATE:
	{
		HDC hDC = GetDC(hwnd);
		hDCMem = CreateCompatibleDC(hDC);
		displayBm = CreateCompatibleBitmap(hDC, maxDWidth, maxDHeight);
		SelectObject(hDCMem, displayBm);
		ReleaseDC(hwnd, hDC);
		hbrRed = CreateSolidBrush(RGB(255, 0, 0));
		ReadDefaultPalette();
		ReadPatterns();
		SwitchImage(hwnd);
		break;
	}
	case WM_DESTROY:
		DeleteObject(hbrRed);
		DeleteDC(hDCMem);
		DeleteObject(displayBm);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		/* Redraw all bits, left to right within the given window size.  */
		RECT cRect;
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &cRect);
		if (displayBm == NULL || dispWidth == 0)
		{
			FillRect(ps.hdc, &cRect, hbrRed);
			EndPaint(hwnd, &ps);
			break;
		}
		BitBlt(ps.hdc, 0, 0, dispWidth, dispHeight, hDCMem, 0, 0, SRCCOPY);
		{
			RECT rt = { dispWidth, 0, cRect.right, dispHeight };
			FillRect(ps.hdc, &rt, GetSysColorBrush(COLOR_WINDOW));
		}
		{
			RECT rt = { 0, dispHeight, cRect.right, cRect.bottom };
			FillRect(ps.hdc, &rt, GetSysColorBrush(COLOR_WINDOW));
		}
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_COMMAND:
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_LEFT:
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				displayNum = 0;
				SwitchImage(hwnd);
				{
					char frameText[14+10+1];
					sprintf(frameText, "Orly Viewer - %u", displayNum);
					SetWindowText(hwnd, frameText);
				}
				InvalidateRect(hwnd, NULL, TRUE);
				break;
			}
			if (displayNum > 0)
				displayNum--;
			SwitchImage(hwnd);
			{
				char frameText[14+10+1];
				sprintf(frameText, "Orly Viewer - %u", displayNum);
				SetWindowText(hwnd, frameText);
			}
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case VK_RIGHT:
			displayNum++;
			SwitchImage(hwnd);
			{
				char frameText[14+10+1];
				sprintf(frameText, "Orly Viewer - %u", displayNum);
				SetWindowText(hwnd, frameText);
			}
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'P':
		{
			char filename[MAX_PATH];
			filename[0] = '\0';
			if (GetFilenameDialog(filename, "Load Palette", hwnd, 0))
			{
				ReadPalette(filename);
				SwitchImage(hwnd);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		}
		case 'D':
			doodleView = !doodleView;
			SwitchImage(hwnd);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'O':
		{
			char filename[MAX_PATH];
			filename[0] = '\0';
			if (GetFilenameDialog(filename, "Load tOFF", hwnd, 0))
			{
				FILE *fp = fopen(filename, "rb");
				if (fp == NULL)
					break;
				fseek(fp, 4, SEEK_SET);
				fread(&tOFF, sizeof(tOFF), 1, fp);
				fclose(fp);
				SwitchImage(hwnd);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		}
		case 'R':
			SwitchImage(hwnd);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'C':
			checker = !checker;
			GenBkPattern();
			SwitchImage(hwnd);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case 'G':
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				useTPAG = !useTPAG;
				if (!useTPAG)
				{
					tPAG.x = 0; tPAG.y = 0; tPAG.scale = 100;
				}
				SwitchImage(hwnd);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			else
			{
				char filename[MAX_PATH];
				filename[0] = '\0';
				if (GetFilenameDialog(filename, "Load tPAG", hwnd, 0))
				{
					FILE *fp = fopen(filename, "rb");
					if (fp == NULL)
						break;
					fread(&tPAG, sizeof(tPAG), 1, fp);
					fclose(fp);
					useTPAG = true;
					SwitchImage(hwnd);
					InvalidateRect(hwnd, NULL, TRUE);
				}
			}
			break;
		case VK_SPACE:
			bah++;
			if (bah >= 100)
				bah = 0;
			SwitchImage(hwnd);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL ReadFileAll(HANDLE hFile, LPVOID lpBuffer, DWORD numBytes)
{
	DWORD totBytesRead = 0;
	do
	{
		DWORD bytesRead = 0;
		BOOL status;
		status = ReadFile(hFile, ((BYTE*)lpBuffer) + totBytesRead,
						  numBytes - totBytesRead, &bytesRead, NULL);
		totBytesRead += bytesRead;
		if (!status)
			break;
		if (bytesRead == 0)
			break;
	} while (totBytesRead < numBytes);

	if (totBytesRead < numBytes)
		return FALSE;
	return TRUE;
}

/* Shows a dialog to open a file if "type" is zero, or shows a
   dialog to save the current file if "type" is one. */
BOOL GetFilenameDialog(char* filename, char* dlgTitle, HWND hwnd,
					   unsigned type)
{
	BOOL retval;
	TCHAR workDir[MAX_PATH];
	OPENFILENAME ofn;
	char* filterStr = "All Files\0*.*\0Mohawk Palettes (*.mpal)\0*.mpal\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = g_hInst;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	if (type == 0) /* Open */
		ofn.Flags = OFN_FILEMUSTEXIST;
	else
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	ofn.lpstrFilter = filterStr;
	ofn.lpstrDefExt = "mpal";
	ofn.lpstrTitle = dlgTitle;

	GetCurrentDirectory(MAX_PATH, workDir);
	switch (type)
	{
	case 0: /* Open */
		retval = GetOpenFileName(&ofn);
		break;
	case 1: /* Save */
		retval = GetSaveFileName(&ofn);
		break;
	default:
		retval = FALSE;
		break;
	}
	SetCurrentDirectory(workDir);
	return retval;
}

void ReadDefaultPalette()
{
	HANDLE hFile;
	DWORD totBytesRead = 0;
	unsigned i;

	ZeroMemory(&bmi, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 8;
	bmi.bmiHeader.biCompression = BI_RGB;

	/* Initialize a default grayscale palette, then try to read the
	   default palette file.  */

	for (i = 0; i < 256; i++)
	{
		bmi.bmiColors[i].rgbRed = i;
		bmi.bmiColors[i].rgbGreen = i;
		bmi.bmiColors[i].rgbBlue = i;
	}

	hFile = CreateFile("orly.pal", GENERIC_READ, FILE_SHARE_READ,
					   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile("../orly.pal", GENERIC_READ, FILE_SHARE_READ,
						   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return;
	}

	ReadFileAll(hFile, bmi.bmiColors, sizeof(bmi.bmiColors));
	CloseHandle(hFile);
}

void ReadPalette(const char *filename)
{
	HANDLE hFile;
	DWORD totBytesRead = 0;
	struct {
		unsigned short firstIdx;
		unsigned short numEntries;
	} __attribute__((packed)) palHeader;
	RGBAPALENTRY preProcPal[256];
	unsigned i;

	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
					   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	ReadFileAll(hFile, &palHeader, sizeof(palHeader));
	c_ntohs(palHeader.firstIdx);
	c_ntohs(palHeader.numEntries);
	ReadFileAll(hFile, preProcPal,
				sizeof(RGBAPALENTRY) * palHeader.numEntries);
	CloseHandle(hFile);

	for (i = 0; i < palHeader.numEntries; i++)
	{
		bmi.bmiColors[i+palHeader.firstIdx].rgbRed = preProcPal[i].r;
		bmi.bmiColors[i+palHeader.firstIdx].rgbGreen = preProcPal[i].g;
		bmi.bmiColors[i+palHeader.firstIdx].rgbBlue = preProcPal[i].b;
	}
}

void GenBkPattern()
{
	/* pats[0] is the background for transparent sections.  */
	if (!checker)
		memset(pats[0], 217, 64 * 64);
	else
	{
		unsigned i;
		for (i = 0; i < 64; i++)
		{
			unsigned j;
			for (j = 0; j < 64; j++)
			{
				if ((i & 0x10) ^ (j & 0x10))
					pats[0][i*64+j] = 51;
				else
					pats[0][i*64+j] = 50;
			}
		}
	}
}

void ReadPatterns()
{
	unsigned i;

	/* Generate the paintbrush patterns.  */
	unsigned char palPatTbl[33][3] =
		{ {0, 0, 0}, {47, 45, 46}, {45, 44, 100}, {98, 43, 96},
		  {77, 42, 79}, {66, 9, 96}, {91, 10, 14}, {66, 11, 92},
		  {10, 14, 80}, {80, 15, 14}, {96, 16, 131}, {16, 17, 19},
		  {17, 18, 20}, {22, 23, 129}, {141, 24, 22}, {26, 25, 30},
		  {146, 26, 25}, {175, 29, 142}, {56, 31, 32}, {152, 32, 149},
		  {39, 34, 152}, {40, 35, 34}, {41, 36, 28}, {40, 2, 68},
		  {67, 4, 3}, {4, 5, 9}, {185, 6, 80}, {6, 7, 104},
		  {104, 50, 20}, {169, 51, 161}, {173, 52, 167}, {191, 53, 159},
		  {63, 63, 1} };

	pats[0] = (unsigned char*)malloc(64 * 64);
	if (pats[0] == NULL)
		return;
	{
		FILE *fp = fopen("../PATO/1", "rb");
		if (fp == NULL)
			return;
		/* Skip the known 8 byte header.  */
		fseek(fp, 8, SEEK_SET);
		fread(pats[0], 64 * 64, 1, fp);
		fclose(fp);
	}
	for (i = 1; i <= 32; i++)
	{
		unsigned j;
		pats[i] = (unsigned char*)malloc(64 * 64);
		if (pats[i] == NULL)
			return;
		for (j = 0; j < 64; j++)
		{
			unsigned k;
			for (k = 0; k < 64; k++)
			{
				switch (pats[0][j*64+k])
				{
				case 1: pats[i][j*64+k] = palPatTbl[i][1]; break;
				case 2: pats[i][j*64+k] = palPatTbl[i][0]; break;
				case 255: pats[i][j*64+k] = palPatTbl[i][2]; break;
				}
			}
		}
	}

	/* Load the other patterns.  */
	for (i = 33; i <= 131; i++)
	{
		char filename[4+1+10+1];
		FILE *fp;
		sprintf(filename, "../PATO/%u", i);
		pats[i] = (unsigned char*)malloc(64 * 64);
		if (pats[i] == NULL)
			return;
		fp = fopen(filename, "rb");
		if (fp == NULL)
			continue;
		/* Skip the known 8 byte header.  */
		fseek(fp, 8, SEEK_SET);
		fread(pats[i], 64 * 64, 1, fp);
		fclose(fp);
	}

	GenBkPattern();

	patLoaded = true;
}

bool SwitchImage(HWND hwnd)
{
	FILE *fp;
	HANDLE hFile;
	bitmap_header head;
	char filename[] = "gdataNNNNNNNNNN";
	unsigned char *outBuffer;
	unsigned outSize = 0;

	dispWidth = 0; dispHeight = 0;

	if (displayNum >= 1)
		wsprintf(filename, "gdata%u", displayNum);
	else
		filename[5] = '\0';

	fp = fopen(filename, "rb");
	if (fp == NULL)
		return false;

	if (fread(&head, sizeof(head), 1, fp) != 1 ||
		head.compression != 0x2)
	{
		fclose(fp);
		return false;
	}

	/* tPAG.x = 0; tPAG.y = 0; tPAG.scale = 100; */

	/* Note: The bitmap bits should have been pre-DWORD aligned by
	   derle.  */
	if (useTPAG)
	{
		unsigned bytesPerRow = 320;
		dispWidth = 317; dispHeight = 377;
		outBuffer = (unsigned char*)malloc(bytesPerRow * dispHeight);
		memset(outBuffer, 0xff, bytesPerRow * dispHeight);
	}
	else
	{
		outSize = head.bytes_per_row * head.height;
		dispWidth = head.width; dispHeight = head.height;
		outBuffer = (unsigned char*)malloc(outSize);
	}
	if (outBuffer == NULL)
	{
		fclose(fp);
		dispWidth = 0; dispHeight = 0;
		return false;
	}

	{
		unsigned dividend = 100;
		unsigned quotient = tPAG.scale / 100;
		unsigned remainder = tPAG.scale % 100;
		unsigned y_dividend = 100;
		unsigned y_quotient = (tPAG.scale) / 100;
		unsigned y_remainder = (tPAG.scale) % 100;
		/* Initialize rem_accum so that when the actual accumulated
		   remainders is greater than or equal to 1/2, a new sample
		   will be plotted.  */
		unsigned rem_accum_y = bah /* dividend / 2 + dividend % 2 */;
		int y = tPAG.y + MAG1 +
			(((183 + tOFF.dpadY - MAG1) * (int)y_quotient) +
			 ((183 + tOFF.dpadY - MAG1) * (int)y_remainder) / (int)y_dividend);

		unsigned row;
		for (row = 0; row < head.height; row++)
		{
			int c;
			unsigned col;
			int x = tPAG.x + MAG2 +
				(((9 + tOFF.dpadX - MAG2) * (int)quotient) +
				 ((9 + tOFF.dpadX - MAG2) * (int)remainder) / (int)dividend);
			unsigned rem_accum_x = bah /* dividend / 2 + dividend % 2 */;
			unsigned old_rem_accum_y = rem_accum_y;
			int old_y = y;
			for (col = 0; col < head.bytes_per_row; col++)
			{
				unsigned bytesPerRow;
				unsigned i, j;
				unsigned old_rem_accum_x = rem_accum_x;
				int old_x = x;
				c = getc(fp);
				if (c == EOF)
					break;
				if (col >= head.width)
					continue;

				if (doodleView && patLoaded)
				{
					/* Perform proper pattern substitution.  */
					if (c == 0xff)
					{
						if (useTPAG && tPAG.background != 0 && !checker)
							goto patsub_skip; /* There's already a background */
						c = 0;
					}
					if (c <= 131)
					{
						unsigned addr =
							((row + tOFF.dpadY + 62) & 0x3f) * 64 +
							((col + tOFF.dpadX + 39) & 0x3f);
						c = pats[c][addr];
					}
					else /* pencil */
					{
						switch (c)
						{
						case 223: c = 63; break; /* black */
						case 193: c = 44; break; /* brown */
						case 197: c = 10; break; /* red */
						case 200: c = 15; break; /* orange */
						case 205: c = 24; break; /* green */
						case 210: c = 32; break; /* sky blue */
						case 211: c = 34; break; /* blue */
						case 215: c = 4; break; /* purple */
						case 216: c = 5; break; /* magenta */
						case 219: c = 50; break; /* white */
						}
					}
				}
			patsub_skip:

				if (useTPAG)
					bytesPerRow = 320;
				else
					bytesPerRow = head.bytes_per_row;

				/* Get ready, complicated StretchBlt algorithm.  */
				rem_accum_y = old_rem_accum_y;
				y = old_y;
				for (i = 0; i < y_quotient; i++)
				{
					rem_accum_x = old_rem_accum_x;
					x = old_x;
					for (j = 0; j < quotient; j++)
					{
						if (x >= 0 && y >= 0 &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
					}
					rem_accum_x += remainder;
					if (rem_accum_x >= dividend)
					{
						if (x >= 0 && y >= 0 &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
						rem_accum_x -= dividend;
					}
					y++;
				}
				rem_accum_y += y_remainder;
				if (rem_accum_y >= y_dividend)
				{
					rem_accum_x = old_rem_accum_x;
					x = old_x;
					for (j = 0; j < quotient; j++)
					{
						if (x >= 0 && y >= 0 &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
					}
					rem_accum_x += remainder;
					if (rem_accum_x >= dividend)
					{
						if (x >= 0 && y >= 0 &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
						rem_accum_x -= dividend;
					}
					y++;
					rem_accum_y -= y_dividend;
				}
			}
			if (c == EOF)
				break;
		}
	}
	fclose(fp);

	/* Load the background if necessary.  */
	if (useTPAG)
	{
		char filename[2+1+2+1+5+5+1];
		bitmap_header tpag_head;
		unsigned row;

		sprintf(filename, "../BG/gdata%hu", tPAG.background);
		fp = fopen(filename, "rb");
		if (fp == NULL)
			goto tpag_bg_done;

		fread(&tpag_head, sizeof(bitmap_header), 1, fp);

		for (row = 0; row < tpag_head.height; row++)
		{
			int c;
			unsigned col;
			for (col = 0; col < tpag_head.bytes_per_row; col++)
			{
				c = getc(fp);
				if (c == EOF)
					break;
				if (col >= tpag_head.width)
					continue;
				if (c != 0xff && 6 + row < dispHeight && 11 + col < dispWidth)
				{
					unsigned bytesPerRow;
					unsigned addr;
					if (useTPAG)
						bytesPerRow = 320;
					else
						bytesPerRow = head.bytes_per_row;
					addr = (6 + row) * bytesPerRow + (11 + col);
					if (outBuffer[addr] == 0xff)
						outBuffer[addr] = (unsigned char)c;
				}
			}
			if (c == EOF)
				break;
		}
		fclose(fp);
	}
tpag_bg_done:

	/* Load the sprite foreground if necessary.  */
	if (useTPAG)
	{
		char filename[2+1+4+5+1+5+5+1];
		bitmap_header tpag_head;

		unsigned dividend = 100;
		unsigned quotient = tPAG.scale / 100;
		unsigned remainder = tPAG.scale % 100;
		unsigned y_dividend = 100;
		unsigned y_quotient = (tPAG.scale) / 100;
		unsigned y_remainder = (tPAG.scale) % 100;
		/* unsigned y_dividend = 377 * 291 * 100;
		unsigned y_quotient = tPAG.scale * 317 * 362 / (377 * 291 * 100);
		unsigned y_remainder = tPAG.scale * 317 * 362 % (377 * 291 * 100); */
		/* Initialize rem_accum so that when the actual accumulated
		   remainders is greater than or equal to 1/2, a new sample
		   will be plotted.  */
		unsigned rem_accum_y = bah /* dividend / 2 + dividend % 2 */;
		/* 377, 317 */
		/* 362, 291 */
		/* +3 down, +1 right on input when less than 100% */
		int y = tPAG.y + MAG1 +
			(((-MAG1) * (int)y_quotient) +
			 ((-MAG1) * (int)y_remainder) / (int)y_dividend);

		unsigned row;

		sprintf(filename, "../PAG_%hu/gdata%hu",
				tPAG.sprite_lib, tPAG.sprite_num + 1);
		fp = fopen(filename, "rb");
		if (fp == NULL)
			goto tpag_done;

		fread(&tpag_head, sizeof(bitmap_header), 1, fp);

		for (row = 0; row < tpag_head.height; row++)
		{
			int c;
			unsigned col;
			int x = tPAG.x + MAG2 +
				(((-MAG2) * (int)quotient) +
				 ((-MAG2) * (int)remainder) / (int)dividend);
			unsigned rem_accum_x = bah /* dividend / 2 + dividend % 2 */;
			unsigned old_rem_accum_y = rem_accum_y;
			int old_y = y;
			for (col = 0; col < tpag_head.bytes_per_row; col++)
			{
				unsigned bytesPerRow;
				unsigned i, j;
				unsigned old_rem_accum_x = rem_accum_x;
				int old_x = x;
				c = getc(fp);
				if (c == EOF)
					break;
				if (col >= tpag_head.width)
					continue;
				/* if (c == 0xff || row >= dispHeight || col >= dispWidth)
					continue; */

				if (useTPAG)
					bytesPerRow = 320;
				else
					bytesPerRow = tpag_head.bytes_per_row;

				/* Get ready, complicated StretchBlt algorithm.  */
				rem_accum_y = old_rem_accum_y;
				y = old_y;
				for (i = 0; i < y_quotient; i++)
				{
					rem_accum_x = old_rem_accum_x;
					x = old_x;
					for (j = 0; j < quotient; j++)
					{
						if (x >= 0 && y >= 0 && c != 0xff &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
					}
					rem_accum_x += remainder;
					if (rem_accum_x >= dividend)
					{
						if (x >= 0 && y >= 0 && c != 0xff &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
						rem_accum_x -= dividend;
					}
					y++;
				}
				rem_accum_y += y_remainder;
				if (rem_accum_y >= y_dividend)
				{
					rem_accum_x = old_rem_accum_x;
					x = old_x;
					for (j = 0; j < quotient; j++)
					{
						if (x >= 0 && y >= 0 && c != 0xff &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
					}
					rem_accum_x += remainder;
					if (rem_accum_x >= dividend)
					{
						if (x >= 0 && y >= 0 && c != 0xff &&
							!(useTPAG && (x >= 317 || y >= 377)))
							outBuffer[y*bytesPerRow+x] = (unsigned char)c;
						x++;
						rem_accum_x -= dividend;
					}
					y++;
					rem_accum_y -= y_dividend;
				}
			}
			if (c == EOF)
				break;
		}
		fclose(fp);
	}
tpag_done:

	bmi.bmiHeader.biWidth = dispWidth;
	bmi.bmiHeader.biHeight = -dispHeight;
	bmi.bmiHeader.biSizeImage = outSize;

	if (dispWidth > maxDWidth || dispHeight > maxDHeight)
	{
		HDC hDC = GetDC(hwnd);
		maxDWidth = dispWidth; maxDHeight = dispHeight;
		DeleteObject(displayBm);
		displayBm = CreateCompatibleBitmap(hDC, maxDWidth, maxDHeight);
		SelectObject(hDCMem, displayBm);
		ReleaseDC(hwnd, hDC);
	}
	SetDIBits(hDCMem, displayBm, 0, dispHeight, outBuffer,
			  (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
	free(outBuffer);
	/* dispWidth = head.width; dispHeight = head.height; */
	return true;
}

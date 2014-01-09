/* Central windows startup stuff */
/* Windows GUI will be used for everything.  */

/* Clipper Drawer is a simple Windows program to help manually draw
   alpha masks for a given video frame.  The video frames should be
   TGA images at 704 x 480 resolution named "rtmpNNNN.tga", where NNNN
   corresponds to the frame number.  The generated TGA mask images
   will be grayscale RLE-compressed TGAs named "maskNNNN.tga", where
   NNNN is equal to the frame number that the mask was drawn for.

   Clipper Drawer has a very minimalistic GUI that uses only keys on
   the keyboard for drawing tools and only the mouse for actual
   drawing.  Clipper Drawer also does not support zooming.  Here are
   the commands:

   Left mouse button: Draw with the brush color.  If the brush color
   is gray, then the drawing color is black.

   Right mouse button: Draw white (transparent).

   S key: Save the current mask.

   Left arrow key: Save the current mask if modified and go to the
   previous frame (or more, see the number keys below).

   Right arrow key: Save the current mask if modified and go to the
   next frame (or more, see the number keys below).

   CTRL+Right arrow key: Bring up a dialog box to go to a specific
   frame number.

   D key: Toggle display of deinterlaced image.

   P key: Toggle whether the second image from deinterlacing is
   displayed or not.  Note that this program does not interlace
   separately drawn mask images.

   H key: Toggle whether the current background image file name is
   displayed or not.

   O key: Toggle whether "only the overlay" is displayed or whether
   the overlay is displayed on top of the background image.

   F key: Flood fill at the current mouse point with black if the mask
   color beneath the mouse in white (transparent).  Otherwise, flood
   fill with white.

   G key: Flood fill with the brush color (gray by default).

   L key: Each time this key is pressed, it causes a transition from
   one of the following modes to the next:
     1. Begin line drawing mode by setting the last line point to the
        current mouse point.
     2. Draw a line from the last line point to the current mouse
        point and set the current mouse point as the last line point.
        The color of the line is the current brush color if not gray,
        black otherwise.
     3. If the current mouse point is the same as the last line point,
        then line drawing mode ends.

   B key: Change the brush color.

   1-9 number keys: Set the line thickness for drawing to the given
   number of pixels.  Also sets the stride for going to the previous
   or next frame to this number.

   0 (zero) number key: Set the line thickness for drawing to 10
   pixels.  Also sets the stride for going to the previous or next
   frame to 10.

   Close button on window titlebar: Close the program immediately,
   discarding any unsaved changes without prompting.

*/

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include "bool.h"
#include "targa.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam);
bool SwitchBackImage(unsigned int number, unsigned int oldNum, HDC hDCMem,
					 HBITMAP dest, HBITMAP destI2, HBITMAP overlay,
					 LPBITMAPINFO bmi);
INT_PTR CALLBACK FrameGotoProc(HWND hDlg, UINT uMsg, WPARAM wParam,
							   LPARAM lParam);

/* Globals */
bool no2ndInter = true;
bool noDeinter = false;
unsigned char* emptyOvrly; /* Holds data to set up an empty overlay */
HINSTANCE g_hInst;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
					 LPSTR lpCmdLine, int nShowCmd)
{
	HWND hwnd;			/* Window handle */
	WNDCLASSEX wcex;	/* Window class */
	MSG msg;			/* Message structure */
	RECT windowRect;	/* Used to set client area size */
	DWORD dwStyle = WS_OVERLAPPED |	/* Window style: Normal window */
					WS_CAPTION |
					WS_SYSMENU |
					WS_MINIMIZEBOX; /* No maximize or resize */

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = NULL;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
	wcex.hbrBackground = NULL;	/* Background is later filled with image */
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Make sure client area is correct size */
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = 704;
	windowRect.bottom = 480;

	AdjustWindowRectEx(&windowRect, dwStyle, FALSE, NULL);

	/* Globally save the application instance.  */
	g_hInst = hInstance;

	hwnd = CreateWindowEx(NULL, "mainWindow", "Clipper drawer",
		dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window position does not matter */
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
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

static int number = 170; /* Pre-initialized current frame number */
static bool modified = false; /* Was the overlay modified? */

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							LPARAM lParam)
{
	static HDC hDCMem;
	static HDC hDCMemAnd;
	static HDC hDCMemXor;
	static BITMAPINFO bmi;
	static HBITMAP backImage;
	static HBITMAP backImageI2;
	static HBITMAP overlay;
	static HBITMAP andMask;
	static HBITMAP xorMask;
	static HPEN pen = NULL; /* used for custom pen sizes */
	static unsigned int stride = 1;
	static bool hideFrame = false;
	static bool onlyOverlay = false;
	static bool black = false;
	static bool lineDraw = false;
	static bool firstField = true;
	static POINT lastPos;
	static POINT stoPos; /* Previous "last" position */
	static COLORREF custCols[16] =
		{0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
		 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
		 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
		 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF};
	static COLORREF pickedCol = 0x00000000;
	static HBRUSH custHBr = NULL;
	static bool lBtnDown = false, rBtnDown = false;

	switch (uMsg)
	{
	case WM_CREATE:
	{
		HDC hDC;

		DialogBox(g_hInst, (LPCTSTR)102, NULL, (DLGPROC)FrameGotoProc);
		ZeroMemory(&bmi, sizeof(BITMAPINFO));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = 704;
		bmi.bmiHeader.biHeight = 480;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = 704 * 480 * 3;

		hDC = GetDC(hwnd);
		hDCMem = CreateCompatibleDC(hDC);
		hDCMemAnd = CreateCompatibleDC(hDC);
		hDCMemXor = CreateCompatibleDC(hDC);
		backImage = CreateCompatibleBitmap(hDC, 704, 480);
		backImageI2 = CreateCompatibleBitmap(hDC, 704, 480);
		overlay = CreateCompatibleBitmap(hDC, 704, 480);
		/* We want monochrome for the AND mask.  */
		andMask = CreateCompatibleBitmap(hDCMemAnd, 704, 480);
		xorMask = CreateCompatibleBitmap(hDC, 704, 480);
		ReleaseDC(hwnd, hDC);
		emptyOvrly = (unsigned char*)malloc(704 * 480 * 3);
		memset(emptyOvrly, 0xff, 704 * 480 * 3);
		if (!SwitchBackImage(number, 0, hDCMem, backImage, backImageI2,
							 overlay, &bmi))
		{
			MessageBox(hwnd, "Could not open background image.", NULL,
					   MB_OK | MB_ICONERROR);
			PostQuitMessage(1);
		}
		SelectObject(hDCMem, overlay);
		SelectObject(hDCMemAnd, andMask);
		SelectObject(hDCMemXor, xorMask);
		break;
	}
	case WM_DESTROY:
		DeleteObject(backImage);
		DeleteObject(backImageI2);
		DeleteObject(overlay);
		DeleteObject(andMask);
		DeleteObject(xorMask);
		DeleteObject(pen);
		DeleteObject(custHBr);
		DeleteDC(hDCMem);
		DeleteDC(hDCMemAnd);
		DeleteDC(hDCMemXor);
		free(emptyOvrly);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		if (onlyOverlay == true)
		{
			SelectObject(hDCMem, overlay);
			BitBlt(ps.hdc, 0, 0, 704, 480, hDCMem, 0, 0, SRCCOPY);
		}
		else
		{
			/* Prepare the AND mask.  */
			COLORREF oldBk;
			oldBk = GetBkColor(ps.hdc);
			SetBkColor(ps.hdc, (COLORREF)0x00808080);
			SetBkColor(hDCMemAnd, (COLORREF)0x00808080);
			BitBlt(hDCMemAnd, 0, 0, 704, 480, hDCMem, 0, 0, SRCCOPY);
			SetBkColor(ps.hdc, oldBk);
			/* Prepare the XOR mask.  */
			BitBlt(hDCMemXor, 0, 0, 704, 480, hDCMemAnd, 0, 0, SRCCOPY);
			BitBlt(hDCMemXor, 0, 0, 704, 480, hDCMem, 0, 0, SRCERASE);
			if (firstField == true)
				SelectObject(hDCMem, backImage);
			else
				SelectObject(hDCMem, backImageI2);
			BitBlt(ps.hdc, 0, 0, 704, 480, hDCMem, 0, 0, SRCCOPY);
			SelectObject(hDCMem, overlay);
			/* Transfer the AND mask.  */
			BitBlt(ps.hdc, 0, 0, 704, 480, hDCMemAnd, 0, 0, SRCAND);
			/* Transfer the XOR mask.  */
			BitBlt(ps.hdc, 0, 0, 704, 480, hDCMemXor, 0, 0, SRCINVERT);
			/* BitBlt(ps.hdc, 0, 0, 704, 480, hDCMem, 0, 0, SRCAND); */
		}
		if (hideFrame == false)
		{
			char str[20];
			wsprintf(str, "rtmp0%03d.tga", number);
			TextOut(ps.hdc, 0, 0, str, (int)strlen(str));
		}
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_COMMAND:
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'S':
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				SwitchBackImage(number, number, hDCMem, backImage,
								backImageI2, overlay, &bmi);
				InvalidateRect(hwnd, NULL, FALSE);
				MessageBox(hwnd, "Saved file.", "Notification",
						   MB_OK | MB_ICONINFORMATION);
			}
			break;
		case VK_LEFT:
		{
			unsigned int oldNum;
			if (firstField == false && !no2ndInter)
				firstField = true;
			else
			{
				oldNum = number;
				number -= stride;
				if (number < 1)
					number = 1;
				else
				{
					if (!no2ndInter)
						firstField = false;
					if (!SwitchBackImage(number, oldNum, hDCMem, backImage,
										 backImageI2, overlay, &bmi))
					{
						MessageBox(hwnd,
								   "Could not open background image.", NULL,
								   MB_OK | MB_ICONERROR);
						PostQuitMessage(1);
					}
				}
			}
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd); /* Force a screen update.  */
			break;
		}
		case VK_RIGHT:
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				unsigned int oldNum;
				oldNum = number;
				DialogBox(g_hInst, (LPCTSTR)102, hwnd,
						  (DLGPROC)FrameGotoProc);
				if (number != oldNum)
				{
					if (!SwitchBackImage(number, oldNum, hDCMem, backImage,
										 backImageI2, overlay, &bmi))
					{
						MessageBox(hwnd,
								   "Could not open background image.", NULL,
								   MB_OK | MB_ICONERROR);
						PostQuitMessage(1);
					}
				}
				InvalidateRect(hwnd, NULL, FALSE);
				UpdateWindow(hwnd); /* Force a screen update.  */
				break;
			}
			if (firstField == true && !no2ndInter)
				firstField = false;
			else
			{
				unsigned int oldNum;
				oldNum = number;
				number += stride;
				if (number > 702)
					number = 702;
				else
				{
					if (!no2ndInter)
						firstField = true;
					if (!SwitchBackImage(number, oldNum, hDCMem, backImage,
										 backImageI2, overlay, &bmi))
					{
						MessageBox(hwnd,
								   "Could not open background image.", NULL,
								   MB_OK | MB_ICONERROR);
						PostQuitMessage(1);
					}
				}
			}
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd); /* Force a screen update.  */
			break;
		case 'D':
			noDeinter = !noDeinter;
			no2ndInter = true;
			firstField = true;
			SwitchBackImage(number, number, hDCMem, backImage, backImageI2,
							overlay, &bmi);
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		case 'P':
			if (noDeinter)
			{
				noDeinter = false;
				InvalidateRect(hwnd, NULL, FALSE);
			}
			no2ndInter = !no2ndInter;
			if (no2ndInter)
				firstField = true;
			SwitchBackImage(number, number, hDCMem, backImage, backImageI2,
							overlay, &bmi);
			break;
		case 'H':
			hideFrame = !hideFrame;
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		case 'O':
			onlyOverlay = !onlyOverlay;
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		case 'F':
		{
			COLORREF color;
			HBRUSH hbr;
			modified = true;
			color = GetPixel(hDCMem, lastPos.x, lastPos.y);
			if (color == 0x00ffffff)
				hbr = (HBRUSH)GetStockObject(BLACK_BRUSH);
			else
				hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
			SelectObject(hDCMem, hbr);
			ExtFloodFill(hDCMem, lastPos.x, lastPos.y, color,
						 FLOODFILLSURFACE);
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		}
		case 'G':
		{
			HBRUSH hbr;
			modified = true;
			if (custHBr == NULL)
				hbr = (HBRUSH)GetStockObject(GRAY_BRUSH);
			else
				hbr = custHBr;
			SelectObject(hDCMem, hbr);
			ExtFloodFill(hDCMem, lastPos.x, lastPos.y,
				GetPixel(hDCMem, lastPos.x, lastPos.y), FLOODFILLSURFACE);
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		}
		case 'L':
			modified = true;
			if (lineDraw == false)
			{
				lineDraw = true;
				MoveToEx(hDCMem, lastPos.x, lastPos.y, NULL);
				stoPos.x = lastPos.x;
				stoPos.y = lastPos.y;
			}
			else
			{
				RECT updateRect;
				LineTo(hDCMem, lastPos.x, lastPos.y);
				MoveToEx(hDCMem, lastPos.x, lastPos.y, NULL);
				if (stoPos.x < lastPos.x)
				{
					updateRect.left = stoPos.x;
					updateRect.right = lastPos.x;
				}
				else
				{
					updateRect.right = stoPos.x;
					updateRect.left = lastPos.x;
				}
				if (stoPos.y < lastPos.y)
				{
					updateRect.bottom = lastPos.y;
					updateRect.top = stoPos.y;
				}
				else
				{
					updateRect.top = lastPos.y;
					updateRect.bottom = stoPos.y;
				}
				InflateRect(&updateRect, stride, stride);
				InvalidateRect(hwnd, &updateRect, FALSE);
				if (stoPos.x == lastPos.x && stoPos.y == lastPos.y)
				{
					COLORREF color;
					if (black)
						color = pickedCol;
					else
						color = 0x00ffffff;
					/* LineTo(hDCMem, lastPos.x, lastPos.y); */
					SetPixel(hDCMem, lastPos.x, lastPos.y, color);
					lineDraw = false;
				}
				stoPos.x = lastPos.x;
				stoPos.y = lastPos.y;
			}
			break;
		case 'B':
		{
			COLORREF lastCol = pickedCol;
			CHOOSECOLOR cInfo;
			if (pickedCol == 0x00000000)
				pickedCol = 0x00808080;
			cInfo.lStructSize = sizeof(CHOOSECOLOR);
			cInfo.hwndOwner = hwnd;
			cInfo.hInstance = NULL;
			cInfo.rgbResult = pickedCol;
			cInfo.lpCustColors = custCols;
			cInfo.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
			cInfo.lCustData = NULL;
			cInfo.lpfnHook = NULL;
			cInfo.lpTemplateName = NULL;
			if (ChooseColor(&cInfo))
			{
				pickedCol = cInfo.rgbResult;
				if (custHBr != NULL)
				{
					DeleteObject(custHBr);
					custHBr = NULL;
				}
				if (pickedCol != 0x00808080)
					custHBr = CreateSolidBrush(pickedCol);
				else
					pickedCol = 0x00000000;
			}
			else
				pickedCol = lastCol;
			break;
		}
		case '1': stride = 1; break; case '2': stride = 2; break;
		case '3': stride = 3; break; case '4': stride = 4; break;
		case '5': stride = 5; break; case '6': stride = 6; break;
		case '7': stride = 7; break; case '8': stride = 8; break;
		case '9': stride = 9; break; case '0': stride = 10; break;
		}
		break;
	case WM_LBUTTONDOWN:
		lBtnDown = true; /* We only want to draw if the mouse was
							initially clicked in this window.  */
		black = true;
		if (pen != NULL)
			DeleteObject(pen);
		pen = CreatePen(PS_SOLID, stride, pickedCol);
		SelectObject(hDCMem, pen);
		goto pointDraw;
	case WM_RBUTTONDOWN:
		rBtnDown = true;
		black = false;
		if (pen != NULL)
			DeleteObject(pen);
		pen = CreatePen(PS_SOLID, stride, 0x00ffffff);
		SelectObject(hDCMem, pen);
pointDraw:
		{
			POINT thisPos;
			RECT updateRect;
			modified = true;
			thisPos.x = LOWORD(lParam);
			thisPos.y = HIWORD(lParam);
			lastPos.x = thisPos.x;
			lastPos.y = thisPos.y;
			MoveToEx(hDCMem, thisPos.x, thisPos.y, NULL);
			if (black)
				SetPixel(hDCMem, thisPos.x, thisPos.y, pickedCol);
			else
				SetPixel(hDCMem, thisPos.x, thisPos.y, 0x00ffffff);
			updateRect.left = thisPos.x - 1;
			updateRect.top = thisPos.y - 1;
			updateRect.right = thisPos.x + 1;
			updateRect.bottom = thisPos.y + 1;
			InvalidateRect(hwnd, &updateRect, FALSE);
		}
		break;
	case WM_LBUTTONUP:
		lBtnDown = false;
		break;
	case WM_RBUTTONUP:
		rBtnDown = false;
		break;
	case WM_MOUSEMOVE:
		if (lBtnDown == true || rBtnDown == true)
		{
			POINT thisPos;
			RECT updateRect;
			thisPos.x = LOWORD(lParam);
			thisPos.y = HIWORD(lParam);
			LineTo(hDCMem, thisPos.x, thisPos.y);
			MoveToEx(hDCMem, thisPos.x, thisPos.y, NULL);
			if (black)
				SetPixel(hDCMem, thisPos.x, thisPos.y, pickedCol);
			else
				SetPixel(hDCMem, thisPos.x, thisPos.y, 0x00ffffff);
			if (lastPos.x < thisPos.x)
			{
				updateRect.left = lastPos.x;
				updateRect.right = thisPos.x;
			}
			else
			{
				updateRect.right = lastPos.x;
				updateRect.left = thisPos.x;
			}
			if (lastPos.y < thisPos.y)
			{
				updateRect.bottom = thisPos.y;
				updateRect.top = lastPos.y;
			}
			else
			{
				updateRect.top = thisPos.y;
				updateRect.bottom = lastPos.y;
			}
			InflateRect(&updateRect, stride, stride);
			InvalidateRect(hwnd, &updateRect, FALSE);
		}
		lastPos.x = LOWORD(lParam);
		lastPos.y = HIWORD(lParam);
		break;
	case WM_ACTIVATEAPP:
		if (wParam == FALSE)
		{
			lBtnDown = false;
			rBtnDown = false;
		}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool SwitchBackImage(unsigned int number, unsigned int oldNum,
					 HDC hDCMem, HBITMAP dest, HBITMAP destI2,
					 HBITMAP overlay, LPBITMAPINFO bmi)
{
	TargaImage backTGA;
	TCHAR str[4+12+4+1+50]; /* "50" is unnecessary padding */

	TgaInit(&backTGA);
	if (oldNum != 0 && modified)
	{
		/* Save the overlay.  */
		unsigned char* saveBits;
		saveBits = (unsigned char*)malloc(704 * 480 * 3);
		GetDIBits(hDCMem, overlay, 0, 480, saveBits, bmi, DIB_RGB_COLORS);
		TgaCreateImage(&backTGA, 704, 480, 24, saveBits);
		TgaConvertRGBToLum(&backTGA, false);
		wsprintf(str, "mask0%03d.tga", oldNum);
		TgaSave(&backTGA, str, true);
		TgaRelease(&backTGA);
	}
	modified = false;

	/* Load the new image.  */
	wsprintf(str, "rtmp0%03d.tga", number);
	if (!TgaLoad(&backTGA, str))
		return false;
	if (!noDeinter)
		TgaDeinterlace(&backTGA);

	SetDIBits(hDCMem, dest, 0, 480,
			  (noDeinter) ? backTGA.imageData : backTGA.deinter1,
			  bmi, DIB_RGB_COLORS);
	if (!no2ndInter)
		SetDIBits(hDCMem, destI2, 0, 480, backTGA.deinter2,
				  bmi, DIB_RGB_COLORS);
	TgaRelease(&backTGA);

	/* Load or create an overlay.  */
	wsprintf(str, "mask0%03d.tga", number);
	if (TgaLoad(&backTGA, str))
	{
		TgaConvertLumToRGB(&backTGA);
		SetDIBits(hDCMem, overlay, 0, 480, backTGA.imageData,
				  bmi, DIB_RGB_COLORS);
	}
	else
		/* Create a new overlay.  */
		SetDIBits(hDCMem, overlay, 0, 480, emptyOvrly, bmi,
				  DIB_RGB_COLORS);

	TgaRelease(&backTGA);
	return true;
}

INT_PTR CALLBACK FrameGotoProc(HWND hDlg, UINT uMsg, WPARAM wParam,
							   LPARAM lParam)
{
	HWND hwndOwner;
	RECT rc, rcDlg, rcOwner;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		/* Get the owner window and dialog box rectangles.  */
		if ((hwndOwner = GetParent(hDlg)) == NULL)
		{
			hwndOwner = GetDesktopWindow();
		}

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		/* Offset the owner and dialog box rectangles so that right
		   and bottom values represent the width and height, and then
		   offset the owner again to discard space taken up by the
		   dialog box.  */
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		/* The new position is the sum of half the remaining space and
		   the owner's original position.  */
		SetWindowPos(hDlg,
			HWND_TOP,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
			0, 0,          /* ignores size arguments */
			SWP_NOSIZE);

		{ /* Initialize the text of the frame number control.  */
			/* Note: `str' only needs to be 12 characters in size for
			   32-bit integers (-2147483648).  */
			TCHAR str[64];
			wsprintf(str, "%d", number);
			SetWindowText(GetDlgItem(hDlg, 1002), (LPCTSTR)str);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK)
			{
				char str[100];
				GetWindowText(GetDlgItem(hDlg, 1002), str, 100);
				number = atoi(str);
				if (number == 0)
					number = 170;
				if (number > 702)
					number = 702;
			}
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

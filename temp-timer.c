/* A temporary timer for timing things (replaces stopwatch) */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#include "bool.h"

bool bRunning = false;
bool bReady = false;
int startTime = 0;
int globalTime = 0;
float timeDiff = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		break;
	case WM_DESTROY:
	case WM_CLOSE:
	case WM_QUIT:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'S':
			startTime = timeGetTime();
			bRunning = true;
			break;
		case 'R':
			timeDiff = (globalTime - startTime) / 1000.0f;
			bRunning = false;
			break;
		}
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rt;
		BeginPaint(hwnd, &ps);
		rt.left = 0; rt.right = 200;
		rt.top = 50; rt.bottom = 200;
		DrawText(ps.hdc, "Press 'S' and 'R' to\nstart and stop the timer",
				 45, &rt, DT_CENTER);
		EndPaint(hwnd, &ps);
		break;
	}

	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine,
				   int nShowCmd)
{
	MSG msg;
	HWND hwnd;
	WNDCLASS wndclass;
	bool timeToQuit = false;

	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hInstance = hInst;
	wndclass.lpfnWndProc = WndProc;
	wndclass.lpszClassName = "short timer class";
	wndclass.lpszMenuName = NULL;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wndclass);

	hwnd = CreateWindow("short timer class", "short timer",
						WS_OVERLAPPEDWINDOW | WS_VISIBLE,
						CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
						NULL, NULL, hInst, NULL);

	while (!timeToQuit)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0))
				timeToQuit = true;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (bRunning) /* Draw time */
		{
			HDC hdc;
			char strTemp[100];
			hdc = GetDC(hwnd);
			globalTime = timeGetTime();
			timeDiff = (globalTime - startTime) / 1000.0f;
			sprintf(strTemp, "%f", timeDiff);
			TextOut(hdc, 50, 90, strTemp, strlen(strTemp));
			ReleaseDC(hwnd, hdc);
			bReady = true;
		}
	}
	return msg.wParam;
}

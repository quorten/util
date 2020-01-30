/* Send lots of automatic clicks */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
				   LPSTR lpCmdLine, int nShowCmd)
{
	Sleep(5000);
	while (!(GetAsyncKeyState(VK_F9) & 0x8000))
	{
		/* POINT pt; */
		INPUT clicks[2];
		clicks[0].type = INPUT_MOUSE;
		clicks[0].mi.dx = 0;
		clicks[0].mi.dy = 0;
		clicks[0].mi.mouseData = NULL;
		clicks[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		clicks[0].mi.time = 0;
		clicks[0].mi.dwExtraInfo = NULL;
		clicks[1].type = INPUT_MOUSE;
		clicks[1].mi.dx = 0;
		clicks[1].mi.dy = 0;
		clicks[1].mi.mouseData = NULL;
		clicks[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
		clicks[1].mi.time = 0;
		clicks[1].mi.dwExtraInfo = NULL;
		/* GetCursorPos(&pt); */
		SendInput(1, &clicks[0], sizeof(INPUT));
		Sleep(10);
		SendInput(1, &clicks[1], sizeof(INPUT));
	}
	return 0;
}

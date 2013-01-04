//Control Panel functions (Open with Control Panel!)

#include "cplstub.h"

LONG CALLBACK CPlApplet(HWND hwndCPL, UINT uMsg, LPARAM lParam1,
						LPARAM lParam2) 
{ 
	int i; 
	LPCPLINFO lpCPlInfo; 
 
	i = (int) lParam1; 
	i += 1;
 
	switch (uMsg)
	{ 
	case CPL_INIT:      /* first message, sent once */
		hInst = GetModuleHandle("cplstub.cpl");
		return TRUE; 
 
	case CPL_GETCOUNT:  /* second message, sent once */
		return NUM_SUBPROGS; 
		break; 
 
	case CPL_INQUIRE: /* third message, sent once per item,
						 parse resource data for Control Panel Icons */
		lpCPlInfo = (LPCPLINFO) lParam2; 
		lpCPlInfo->lData = 0; //Not now
		lpCPlInfo->idIcon = 100 + i;
		lpCPlInfo->idName = i * 3 - 2;
		lpCPlInfo->idInfo = i * 3 - 1;
		break; 

	case CPL_DBLCLK:    /* item icon double-clicked, either do
						   function or call program */
		if (bCallProgram[i-1])
		{
			char file[512];
			LoadString(NULL, i * 3, file, 512);
			ShellExecute(NULL, "open", file, NULL, NULL, SW_SHOWDEFAULT);
		}
		else
			ResponseFunc[i-1](lParam2);
		break; 
 
	case CPL_STOP:      /* sent once per item before CPL_EXIT */
		break; 
 
	case CPL_EXIT:    /* sent once before FreeLibrary is called */
		break; 

	default: 
		break; 
	} 
	return 0; 
}

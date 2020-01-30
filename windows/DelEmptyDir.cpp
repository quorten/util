// Recursive empty directory deleter - Win32 API version

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>

enum DIR_STATUSCODE {DIR_EMPTY, DIR_HAS_CONTENTS, UNKNOWN_ERROR};

namespace dirsearch // Forward Declarations
{
	DIR_STATUSCODE SearchDirectories(const char* dirName);
}

int main(int argc, char* argv[])
{
	char fullPath[500];
	GetCurrentDirectory(500, fullPath);
	dirsearch::SearchDirectories(fullPath);
	return 0;
}

namespace dirsearch {

using namespace std;

bool ListDirectoryContents(const char* targetDir, vector<string>& fileList);
void ErrorExit(LPTSTR lpszFunction);

DIR_STATUSCODE SearchDirectories(const char* dirName)
{
	if (!SetCurrentDirectory(dirName))
	{
		return UNKNOWN_ERROR;
		// ErrorExit("SetCurrentDirectory");
	}
	vector<string> fileList;
	if (!ListDirectoryContents(dirName, fileList))
	{
		return UNKNOWN_ERROR;
	}
	else
	{
		if (fileList.empty())
			return DIR_EMPTY;
		for (vector<string>::const_iterator iter = fileList.begin(); iter != fileList.end(); iter++)
		{
			char fullPath[500];
			wsprintf(fullPath, "%s\\%s", dirName, iter->c_str());
			if (SearchDirectories(fullPath) == DIR_EMPTY)
			{
				SetCurrentDirectory(dirName);
				if (!RemoveDirectory(iter->c_str()))
					ErrorExit("RemoveDirectory");
				// Do a recall to see if this directory is empty now
				if (SearchDirectories(dirName) == DIR_EMPTY)
					return DIR_EMPTY;
			}
		}
		return DIR_HAS_CONTENTS;
	}
}

bool ListDirectoryContents(const char* targetDir, vector<string>& fileList)
{
   WIN32_FIND_DATA FindFileData;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   char DirSpec[MAX_PATH + 1];  // directory specification
   DWORD dwError;

   // printf ("Target directory is %s.\n", targetDir);
   if (strlen(targetDir) + 3 < MAX_PATH)
   {
	strncpy (DirSpec, targetDir, strlen(targetDir)+1);
	strncat (DirSpec, "\\*", 3);
   }
   else
	   return false;

   hFind = FindFirstFile(DirSpec, &FindFileData);

   if (hFind == INVALID_HANDLE_VALUE) 
   {
      // printf ("Invalid file handle. Error is %u\n", GetLastError());
	   ErrorExit("FindFirstFile");
      return false;
   } 
   else 
   {
	   string filename = FindFileData.cFileName;
	   if (filename.find(".") != 0 && filename.find(".exe") == string::npos) // We don't want "." or ".."
		fileList.push_back(FindFileData.cFileName);
      while (FindNextFile(hFind, &FindFileData) != 0) 
      {
		filename = FindFileData.cFileName;
		if (filename.find(".") != 0 && filename.find(".exe") == string::npos) // We don't want "." or ".."
		fileList.push_back(FindFileData.cFileName);
      }
    
      dwError = GetLastError();
      FindClose(hFind);
      if (dwError != ERROR_NO_MORE_FILES) 
      {
         // printf ("FindNextFile error. Error is %u\n", dwError);
		  ErrorExit("FindNextFile");
         return false;
      }
   }
   return true;
}

void ErrorExit(LPTSTR lpszFunction) 
{ 
    TCHAR szBuf[80]; 
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    wsprintf(szBuf, 
        "%s failed with error %d: %s", 
        lpszFunction, dw, lpMsgBuf); 
 
    MessageBox(NULL, szBuf, "Error", MB_OK | MB_ICONERROR); 

    LocalFree(lpMsgBuf);
    DebugBreak();// ExitProcess(dw); 
}

} // END namespace dirsearch

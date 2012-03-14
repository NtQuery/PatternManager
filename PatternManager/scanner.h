
#include <tchar.h>

#define PATTERN_MAX_SIZE 2048
#define KEYNAME_MAX_SIZE 20

bool ScanPatternInFile(TCHAR * pattern);
BOOL CALLBACK DlgProcesslist(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

class Process {
public:
	DWORD PID;
	DWORD_PTR imageBase;
	DWORD entryPoint; //without imagebase
	DWORD imageSize;
	TCHAR filename[MAX_PATH];
	TCHAR fullPath[MAX_PATH];
};
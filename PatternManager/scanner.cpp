
#include <windows.h>
#include <stdio.h>
#include "scanner.h"
#include "resource.h"
#include <vector>
#include <tlhelp32.h>

LONGLONG getFileSize(HANDLE hFile);
void ScanPatternInMemory(BYTE * memory, DWORD memorySize,DWORD_PTR startOffset, TCHAR * pattern);
void WriteLog(const TCHAR * format, ...);
bool SelectFile(TCHAR * filePath);
bool SearchPatternInFile(const TCHAR * filePath, TCHAR * pattern);
DWORD findPattern(DWORD_PTR startOffset, DWORD size, const PBYTE pattern, const TCHAR * mask,DWORD_PTR startRelativeOffset);
void TextToByteMask(TCHAR * pattern, BYTE * bytePattern, TCHAR * mask,unsigned int maskLen);
bool SearchPatternInProcess(Process & process, TCHAR * pattern);

TCHAR logbuf[500] = {0};
extern TCHAR scannerLog[MAX_PATH];
extern TCHAR add_data_text[PATTERN_MAX_SIZE];
extern HINSTANCE hInst;

std::vector<Process> processList;

bool ScanPatternInFile(TCHAR * pattern)
{
	TCHAR targetFile[MAX_PATH] = {0};

	if (SelectFile(targetFile))
	{
		WriteLog(TEXT("\r\n------------------------------------------------------\r\n"));
		WriteLog(TEXT("File: %s\r\n"),targetFile);

		

		return SearchPatternInFile(targetFile,pattern);
	}

	return false;
}

bool SelectFile(TCHAR * filePath)
{
	OPENFILENAME ofn = {0};

	ofn.lStructSize     = sizeof(ofn);
	ofn.lpstrFile       = filePath;
	ofn.nMaxFile        = MAX_PATH;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	ofn.lpstrFilter     = TEXT("Executable (*.exe)\0*.exe\0Dynamic Link Library (*.dll)\0*.dll\0All files\0*.*\0");


	return (GetOpenFileName(&ofn) != 0);
}

bool SearchPatternInFile(const TCHAR * filePath, TCHAR * pattern)
{
	LONGLONG fileSize = 0;
	BYTE * fileBuffer = 0;
	DWORD lpNumberOfBytesRead = 0;
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	bool retValue = false;

	if (hFile == INVALID_HANDLE_VALUE)
	{
		MessageBox(0, TEXT("CreateFile GENERIC_READ FILE_SHARE_READ OPEN_EXISTING failed"), TEXT("Error"), MB_ICONERROR);
	}
	else
	{
		fileSize = getFileSize(hFile);

		if (fileSize == 0)
		{
			MessageBox(0, TEXT("getFileSize failed"), TEXT("Error"), MB_ICONERROR);
		}
		else
		{
			fileBuffer = (BYTE *)malloc((size_t)fileSize);

			if (!fileBuffer)
			{
				MessageBox(0, TEXT("malloc failed"), TEXT("Error"), MB_ICONERROR);
			}
			else
			{
				if (!ReadFile(hFile, fileBuffer, (DWORD)fileSize, &lpNumberOfBytesRead, 0))
				{
					MessageBox(0, TEXT("ReadFile failed"), TEXT("Error"), MB_ICONERROR);
				}
				else
				{
					ScanPatternInMemory(fileBuffer,lpNumberOfBytesRead,0,pattern);
					retValue = true;
				}

				free(fileBuffer);
			}
		}

		CloseHandle(hFile);
	}


	return retValue;
}

void ScanPatternInMemory(BYTE * memory, DWORD memorySize,DWORD_PTR startOffset, TCHAR * pattern)
{
	BYTE * bytePattern = 0;
	TCHAR * mask = 0;
	//DWORD_PTR address = 0, delta = 0, dwMemory = 0, dwMemorySize = 0;
	unsigned int len = _tcslen(pattern);
	if (len % 2)
		len++;

	len = (len/2) + 1;

	bytePattern = (BYTE *)malloc(len);
	mask = (TCHAR *)malloc(len * sizeof(TCHAR));

	ZeroMemory(mask,len * sizeof(TCHAR));
	ZeroMemory(bytePattern,len);

	TextToByteMask(pattern,bytePattern,mask,len * sizeof(TCHAR));


	WriteLog(TEXT("Scanning pattern %s memory size 0x%X\r\n"),pattern,memorySize);
	WriteLog(TEXT("------------------------------------------------------\r\n"));

	DWORD found = findPattern((DWORD_PTR)memory,memorySize,bytePattern,mask, startOffset);

	WriteLog(TEXT("Found %d times\r\n"),found);
	WriteLog(TEXT("------------------------------------------------------\r\n"));
	
	free(mask);
	free(bytePattern);
}

void TextToByteMask(TCHAR * pattern, BYTE * bytePattern, TCHAR * mask, unsigned int maskLen)
{
	TCHAR temp[3] = {0};
	unsigned int len = _tcslen(pattern);

	if (len % 2)
	{
		_tcscat_s(pattern,PATTERN_MAX_SIZE,TEXT("?"));
		len++;
	}

	for (unsigned int i = 0, k = 0; i < len; i+=2, k++)
	{
		if ((pattern[i] == TEXT('?')) || (pattern[i+1] == TEXT('?')))
		{
			_tcscat_s(mask,maskLen,TEXT("?"));
			bytePattern[k] = 0;
		}
		else
		{
			temp[0] = pattern[i];
			temp[1] = pattern[i+1];

			_tcscat_s(mask,maskLen,TEXT("x"));

			bytePattern[k] = (BYTE)_tcstoul(temp,0,16);
		}
	}
}

LONGLONG getFileSize(HANDLE hFile)
{
	LARGE_INTEGER lpFileSize = {0};

	if ((hFile != INVALID_HANDLE_VALUE) && (hFile != 0))
	{
		if (!GetFileSizeEx(hFile, &lpFileSize))
		{
			return 0;
		}
		else
		{
			return lpFileSize.QuadPart;
		}
	}
	else
	{
		return 0;
	}
}

DWORD findPattern(DWORD_PTR startOffset, DWORD size, const PBYTE pattern, const TCHAR * mask, DWORD_PTR startRelativeOffset)
{
	DWORD pos = 0;
	DWORD found = 0;
	size_t searchLen = _tcslen(mask) - 1;

	for(DWORD_PTR retAddress = startOffset; retAddress < startOffset + size; retAddress++)
	{
		if( *(BYTE*)retAddress == pattern[pos] || mask[pos] == TEXT('?') )
		{
			pos++;
			if(mask[pos] == 0x00)
			{
				if (startRelativeOffset)
				{
					WriteLog(TEXT("Found at VA 0x%08X\r\n"),retAddress - searchLen - startOffset + startRelativeOffset);
				}
				else
				{
					WriteLog(TEXT("Found at offset 0x%08X\r\n"),retAddress - searchLen - startOffset);
				}
				pos = 0;
				found++;
			}
			
		} else {
			pos = 0;
		}
	}

	return found;
}

void WriteLog(const TCHAR * format, ...)
{
	FILE * pFile;
	va_list va_alist;

	if (!format)
	{ 
		return;
	}

	ZeroMemory(logbuf, sizeof(logbuf));

	va_start (va_alist, format);
	_vsntprintf_s(logbuf, _countof(logbuf), _countof(logbuf) - 1, format, va_alist);
	va_end (va_alist);

	if (_tfopen_s(&pFile,scannerLog,L"a") == NULL)
	{
		_fputts(logbuf,pFile);
		fclose(pFile);
	}
}

SIZE_T getSizeOfImageProcess(HANDLE processHandle, DWORD_PTR moduleBase)
{
	SIZE_T sizeOfImage = 0;
	MEMORY_BASIC_INFORMATION lpBuffer = {0};

	do
	{
		moduleBase = (DWORD_PTR)((SIZE_T)moduleBase + lpBuffer.RegionSize);
		sizeOfImage += lpBuffer.RegionSize;

		if (!VirtualQueryEx(processHandle, (LPCVOID)moduleBase, &lpBuffer, sizeof(MEMORY_BASIC_INFORMATION)))
		{
			lpBuffer.Type = 0;
			sizeOfImage = 0;
		}
	} while (lpBuffer.Type == MEM_IMAGE);


	return sizeOfImage;
}

bool GetProcessList(HWND hwndDlg) 
{
	HWND hCombo = GetDlgItem(hwndDlg, IDC_COMBO_PROCESS);

	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
	MODULEENTRY32 me32 = {0};
	Process process;
	HANDLE hProcess = 0;

	processList.clear();
	processList.reserve(34);

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hProcessSnap == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if(!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);
		return false;
	}

	do
	{
		//filter process list
		if (pe32.th32ProcessID > 4)
		{
			process.PID = pe32.th32ProcessID;

			hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process.PID);
			if(hModuleSnap != INVALID_HANDLE_VALUE)
			{
				me32.dwSize = sizeof(MODULEENTRY32);

				Module32First(hModuleSnap, &me32);
				process.imageBase = (DWORD_PTR)me32.hModule;
				_tcscpy_s(process.fullPath, MAX_PATH, me32.szExePath);
				CloseHandle(hModuleSnap);

				hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, NULL, process.PID);
				if(hProcess)
				{
					process.imageSize = getSizeOfImageProcess(hProcess,process.imageBase);

					CloseHandle(hProcess);

					if (process.imageSize != 0)
					{
						_tcscpy_s(process.filename, MAX_PATH, pe32.szExeFile);

						processList.push_back(process);
					}

				}
			}
		}
	} while(Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	for (size_t i = 0; i < processList.size(); i++)
	{
		_stprintf_s(logbuf, _countof(logbuf),TEXT("0x%04X - %s - %s"),processList[i].PID,processList[i].filename,processList[i].fullPath);
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)logbuf);
	}

	return true;
}

bool ScanPatternInProcess(Process & process) 
{
	WriteLog(TEXT("\r\n------------------------------------------------------\r\n"));
	WriteLog(TEXT("Process:\r\n- PID: 0x%X\r\n- Filename: %s\r\n- Fullpath: %s\r\n- Imagebase: 0x%X\r\n- ImageSize 0x%X\r\n"),process.PID,process.filename,process.fullPath,process.imageBase,process.imageSize);

	return SearchPatternInProcess(process, add_data_text);
}

bool SearchPatternInProcess(Process & process, TCHAR * pattern)
{
	SIZE_T numberOfBytesRead = 0;
	BYTE * buffer = 0;

	HANDLE hProcess = OpenProcess(PROCESS_VM_READ, NULL, process.PID);
	if(!hProcess)
	{
		MessageBox(0, TEXT("OpenProcess with PROCESS_VM_READ rights failed"), TEXT("Error"), MB_ICONERROR);
		return false;
	}

	buffer = (BYTE *)malloc(process.imageSize);

	if (!buffer)
	{
		CloseHandle(hProcess);
		MessageBox(0, TEXT("malloc failed"), TEXT("Error"), MB_ICONERROR);
		return false;
	}

	if (!ReadProcessMemory(hProcess,(LPCVOID)process.imageBase,buffer,process.imageSize,&numberOfBytesRead))
	{
		free(buffer);
		CloseHandle(hProcess);
		MessageBox(0, TEXT("ReadProcessMemory failed"), TEXT("Error"), MB_ICONERROR);
		return false;
	}

	CloseHandle(hProcess);

	ScanPatternInMemory(buffer,numberOfBytesRead,process.imageBase,pattern);

	free(buffer);

	return true;
}

BOOL CALLBACK DlgProcesslist(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT index = 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
			GetProcessList(hwndDlg);
		}
		return TRUE;

	case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDC_BTN_CANCEL:
				{
					SendMessage(hwndDlg, WM_CLOSE, 0, 0);
				}
				return TRUE;

			case IDC_BTN_OK:
				{
					index = SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_PROCESS),CB_GETCURSEL,0,0);

					if (ScanPatternInProcess(processList.at(index)))
					{
						MessageBox(hwndDlg, TEXT("Process scanning finished"), TEXT("Success"), MB_ICONINFORMATION);
					}

					processList.clear();
					
					EndDialog(hwndDlg, 0);
				}
				return TRUE;
			}
		}
		return TRUE;
	}
	return FALSE;
}
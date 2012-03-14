#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <commctrl.h>
#include "resource.h"
#include "scanner.h"

#pragma comment(lib,"Comctl32.lib")

//#define _WIN32_IE          0x0500
//#define _WIN32_WINDOWS     0x0410
#define DEFAULT_RECT_WIDTH    150
#define DEFAULT_RECT_HEIGHT    30
#define IDH_RESTOREHIDE       400
#define IDI_TRAYICON          300
#define TRAY_EXIT             301
#define TRAY_RESTORE          302
#define MSG_MINTRAYICON (WM_USER+0)


#ifdef UNICODE
#define TEXT_TITLE "Pattern Manager MOD"
#else
#define TEXT_TITLE "Pattern Manager"
#endif

void FormatTextHex(TCHAR* text, bool byte_space);

HINSTANCE hInst = 0;
HWND hwndMain = 0;
NOTIFYICONDATA nid = {0};

bool category_edit=false;
bool stuff_changed=false;
bool new_category=false;
bool del_category=false;
bool no_categories=true;
bool startMinimized=false;
bool IsMinimized=false;
bool update_nibbles=false;

int number_pattern_selected=0;
int main_category_selected=0;
int edit_category_selected=0;
int main_pattern_selected=0;

TCHAR curdir[MAX_PATH]={0};
TCHAR datafile[MAX_PATH]={0};
TCHAR ini_file[MAX_PATH]={0};

TCHAR scannerLog[MAX_PATH]={0};

TCHAR add_data_text[PATTERN_MAX_SIZE]={0};
TCHAR add_name_text[255]={0};
TCHAR category_selected[255]={0};
TCHAR name_pattern_selected[255]={0};

TCHAR pattern_data[PATTERN_MAX_SIZE]={0};
TCHAR keyname[KEYNAME_MAX_SIZE]={0};

void ComparePatterns(TCHAR* old_pattern, TCHAR* new_pattern, TCHAR* updated_buffer, bool nibbles)
{
    //Initialize variables and get the pattern lengths
    updated_buffer[0]=0;
    int old_pattern_len=0, new_pattern_len=0, smallest_len=0;
    bool spaced_pattern=false,different_length=true;

    //Determine if a spaced pattern is used.
    if(old_pattern[2]==TEXT(' '))
        spaced_pattern=true;

    //Prepare the patterns for comparing.
    FormatTextHex(old_pattern, false);
    FormatTextHex(new_pattern, false);

    //Get patterns lengths and determine which one is the shortest.
    old_pattern_len=_tcslen(old_pattern);
    new_pattern_len=_tcslen(new_pattern);
    if(old_pattern_len>new_pattern_len)
        smallest_len=new_pattern_len;
    else if(old_pattern_len<new_pattern_len)
        smallest_len=old_pattern_len;
    else
    {
        smallest_len=old_pattern_len;
        different_length=false;
    }

    //Set-up the new pattern.
    for(int i=0; i<smallest_len; i++)
    {
        if(nibbles) //The easy part...
        {
            if(old_pattern[i]==new_pattern[i]) //No difference
                _stprintf_s(updated_buffer,PATTERN_MAX_SIZE, TEXT("%s%c"), updated_buffer, old_pattern[i]);
            else
                _stprintf_s(updated_buffer,PATTERN_MAX_SIZE, TEXT("%s?"), updated_buffer);
        }
        else //The hard part...
        {
            if((old_pattern[i]==new_pattern[i]) && (old_pattern[i+1]==new_pattern[i+1]))
                _stprintf_s(updated_buffer,PATTERN_MAX_SIZE, TEXT("%s%c%c"), updated_buffer, old_pattern[i], old_pattern[i+1]);
            else
                _stprintf_s(updated_buffer,PATTERN_MAX_SIZE, TEXT("%s??"), updated_buffer);
            i++;
        }
    }

    //Append the longer pattern.
    if(different_length)
    {
        if(smallest_len==new_pattern_len)
            _tcscat_s(updated_buffer,PATTERN_MAX_SIZE, old_pattern+smallest_len);
        else
            _tcscat_s(updated_buffer,PATTERN_MAX_SIZE, new_pattern+smallest_len);
    }

    //Re-format the pattern.
    FormatTextHex(updated_buffer, spaced_pattern);
}

bool isValidHex(TCHAR text)
{
	TCHAR keySpace[] = TEXT("0123456789ABCDEF?");

	for (unsigned int i = 0; i < _tcslen(keySpace); i++)
	{
		if (text == keySpace[i])
		{
			return true;
		}
	}

	return false;
}

void FormatTextHex(TCHAR* text, bool byte_space)
{
    TCHAR FormatTextHex_format[PATTERN_MAX_SIZE]=TEXT("");
    bool space_local=false;
    int len=_tcslen(text);
    for(int i=0; i<len; i++)
    {
        if(!byte_space)
        {
            if(isValidHex(text[i]))
                _stprintf_s(FormatTextHex_format,PATTERN_MAX_SIZE, TEXT("%s%c"), FormatTextHex_format, text[i]);
        }
        else
        {
            if(space_local)
            {
                if(isValidHex(text[i]))
                {
                    _stprintf_s(FormatTextHex_format,PATTERN_MAX_SIZE, TEXT("%s%c"), FormatTextHex_format, text[i]);
                    space_local=false;
                }
            }
            else
            {
                if(isValidHex(text[i]))
                {
                    _stprintf_s(FormatTextHex_format,PATTERN_MAX_SIZE, TEXT("%s %c"), FormatTextHex_format, text[i]);
                    space_local=true;
                }
            }
        }

    }
    if(!byte_space)
        _tcscpy_s(text,PATTERN_MAX_SIZE, FormatTextHex_format);
    else
        _tcscpy_s(text,PATTERN_MAX_SIZE, FormatTextHex_format+1);
}

void GetTrayWndRect(LPRECT lpTrayRect)
{
    HWND hShellTrayWnd=FindWindowEx(NULL,NULL,TEXT("Shell_TrayWnd"),NULL);
    if(hShellTrayWnd)
    {
        HWND hTrayNotifyWnd=FindWindowEx(hShellTrayWnd,NULL,TEXT("TrayNotifyWnd"),NULL);
        if(hTrayNotifyWnd)
        {
            GetWindowRect(hTrayNotifyWnd,lpTrayRect);
            return;
        }
    }
    APPBARDATA appBarData;
    appBarData.cbSize=sizeof(appBarData);
    if(SHAppBarMessage(ABM_GETTASKBARPOS,&appBarData))
    {
        switch(appBarData.uEdge)
        {
        case ABE_LEFT:
        case ABE_RIGHT:
            lpTrayRect->top=appBarData.rc.bottom-100;
            lpTrayRect->bottom=appBarData.rc.bottom-16;
            lpTrayRect->left=appBarData.rc.left;
            lpTrayRect->right=appBarData.rc.right;
            break;

        case ABE_TOP:
        case ABE_BOTTOM:
            lpTrayRect->top=appBarData.rc.top;
            lpTrayRect->bottom=appBarData.rc.bottom;
            lpTrayRect->left=appBarData.rc.right-100;
            lpTrayRect->right=appBarData.rc.right-16;
            break;
        }

        return;
    }
    hShellTrayWnd=FindWindowEx(NULL,NULL,TEXT("Shell_TrayWnd"),NULL);
    if(hShellTrayWnd)
    {
        GetWindowRect(hShellTrayWnd,lpTrayRect);
        if(lpTrayRect->right-lpTrayRect->left>DEFAULT_RECT_WIDTH)
            lpTrayRect->left=lpTrayRect->right-DEFAULT_RECT_WIDTH;
        if(lpTrayRect->bottom-lpTrayRect->top>DEFAULT_RECT_HEIGHT)
            lpTrayRect->top=lpTrayRect->bottom-DEFAULT_RECT_HEIGHT;

        return;
    }
    SystemParametersInfo(SPI_GETWORKAREA,0,lpTrayRect,0);
    lpTrayRect->left=lpTrayRect->right-DEFAULT_RECT_WIDTH;
    lpTrayRect->top=lpTrayRect->bottom-DEFAULT_RECT_HEIGHT;
}

void MinimizeToTray(HWND hwnd)
{
    WritePrivateProfileString(TEXT("Settings"), TEXT("startMinimized"), TEXT("1"), ini_file);
    RECT rcFrom,rcTo;
    GetWindowRect(hwnd,&rcFrom);
    GetTrayWndRect(&rcTo);
    DrawAnimatedRects(hwnd,IDANI_CAPTION,&rcFrom,&rcTo);
    startMinimized=true;
    ShowWindow(hwnd, SW_HIDE);
    IsMinimized=true;
}

void MinimizeToTray_thread(HWND hwnd)
{
    WritePrivateProfileString(TEXT("Settings"), TEXT("startMinimized"), TEXT("1"), ini_file);
    startMinimized=true;
    ShowWindow(hwnd, SW_HIDE);
    IsMinimized=true;
}

void RestoreFromTray(HWND hwnd)
{
    WritePrivateProfileString(TEXT("Settings"), TEXT("startMinimized"), TEXT("0"), ini_file);
    startMinimized=false;
    ShowWindow(hwnd, SW_SHOW);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    IsMinimized=false;
}

DWORD WINAPI MinimizeThread(LPVOID lparam)
{
    int counter=0;
    while(counter<40)
    {
        MinimizeToTray_thread(hwndMain);
        Sleep(25);
        counter++;
    }
    return 0;
}

BOOL FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void CreateDummyUnicodeFile(const TCHAR * file)
{
	//http://www.codeproject.com/Articles/9071/Using-Unicode-in-INI-files

	if (!FileExists(file))
	{
		// UTF16-LE BOM(FFFE)
		WORD wBOM = 0xFEFF;
		DWORD NumberOfBytesWritten;

		HANDLE hFile = CreateFile(file, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(hFile, &wBOM, sizeof(WORD), &NumberOfBytesWritten, NULL);
		//WriteFile(hFile, pszSectionB, (_tcslen(pszSectionB)+1)*(sizeof(TCHAR)), &NumberOfBytesWritten, NULL);
		CloseHandle(hFile);
	}
}

bool AddCategory(const TCHAR* name)
{
    _tcscpy_s(category_selected,_countof(category_selected), name);

#ifdef UNICODE
	CreateDummyUnicodeFile(datafile);
#endif

    if(!WritePrivateProfileString(name, TEXT("Patterns"), TEXT("0"), datafile))
        return false;
    stuff_changed=true;
    new_category=true;
    return true;
}

bool RemoveCategory(const TCHAR* name)
{
    if(!WritePrivateProfileSection(name, 0, datafile))
        return false;
    stuff_changed=true;
    return true;
}

bool EditCategory(const TCHAR* name, const TCHAR* newname)
{
    TCHAR category_data[65535]={0};
    if(!GetPrivateProfileSection(name, category_data, _countof(category_data), datafile))
        return false;
    if(!RemoveCategory(name))
        return false;
    if(!WritePrivateProfileSection(newname, category_data, datafile))
        return false;
    return true;
}

bool FixPatterns(const TCHAR* category)
{
    TCHAR newname[10]={0}, category_section[65535]={0};
    int len = GetPrivateProfileSection(category, category_section, _countof(category_section), datafile);
	int total_patterns = 0;
    if(!len)
        return false;
    TCHAR* keynamedata=category_section;
    for(int i=0,j=1,k=0,l=0; i<len; i++)
    {
        if(category_section[i])
        {
            k++;
        }
        else
        {
            keynamedata=category_section+i-k;
            if( memcmp(keynamedata, TEXT("Patterns"), sizeof(TCHAR) * 8) )
            {
                if( (keynamedata[11]!=TEXT('N')) && (keynamedata[11]!=TEXT('D')))
                {
                    MessageBox(0, TEXT("There was an error while fixing the pattern order, attemt it yourself..."), TEXT("Error!"), MB_ICONERROR);
                    ExitProcess(1);
                }
                if(l==2)
                {
                    j++;
                    l=0;
                }
                l++;
                _stprintf_s(newname,_countof(newname), TEXT("%.04d"), j);
                total_patterns=j;
                memcpy(keynamedata + 7, newname, sizeof(TCHAR) * 4);
            }
            k=0;
        }
    }
    if(!WritePrivateProfileSection(category, category_section, datafile))
        return false;
    _stprintf_s(newname,_countof(newname), TEXT("%d"), total_patterns);
    if(!WritePrivateProfileString(category, TEXT("Patterns"), newname, datafile))
        return false;
    return true;
}

bool AddPattern(const TCHAR* category, const TCHAR* name, const TCHAR* pattern)
{
    TCHAR pattern_number[6]={0};
    int new_number=0;
    if(!GetPrivateProfileString(category, TEXT("Patterns"), TEXT(""), pattern_number, 6, datafile))
        return false;

    _stscanf_s(pattern_number, TEXT("%d"), &new_number,_countof(pattern_number));
    new_number++;
    _stprintf_s(pattern_number,_countof(pattern_number), TEXT("%d"), new_number);

    if(!WritePrivateProfileString(category, TEXT("Patterns"), pattern_number, datafile))
        return false;

    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dName"), new_number);
    if(!WritePrivateProfileString(category, keyname, name, datafile))
        return false;


    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), new_number);
    if(!WritePrivateProfileString(category, keyname, pattern, datafile))
        return false;
    stuff_changed=true;
    return true;
}

bool RemovePattern(const TCHAR* category, int pattern_number)
{
    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dName"), pattern_number);
    if(!WritePrivateProfileString(category, keyname, 0, datafile))
        return false;

    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), pattern_number);
    if(!WritePrivateProfileString(category, keyname, 0, datafile))
        return false;

    if(!FixPatterns(category))
        return false;
    stuff_changed=true;
    return true;
}

bool EditPattern(const TCHAR* category, int pattern_number, const TCHAR* newname, const TCHAR* newdata)
{
    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dName"), pattern_number);
    if(!WritePrivateProfileString(category, keyname, newname, datafile))
        return false;

    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), pattern_number);
    if(!WritePrivateProfileString(category, keyname, newdata, datafile))
        return false;

    stuff_changed=true;
    return true;
}

bool ScanPatternInFileAction(const TCHAR* category, int number)
{
	_stprintf_s(keyname, KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number);

	GetPrivateProfileString(category, keyname, TEXT(""), pattern_data, _countof(pattern_data), datafile);

	return ScanPatternInFile(pattern_data);
}

void ScanProcessAction(HWND hwndDlg)
{
	_stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), main_pattern_selected+1);

	GetPrivateProfileString(category_selected, keyname, TEXT(""), add_data_text, _countof(add_data_text), datafile);

	DialogBox(hInst, MAKEINTRESOURCE(DLG_PROCESSLIST), hwndDlg, (DLGPROC)DlgProcesslist);
}

void CopyPattern(const TCHAR* category, int number)
{
    HGLOBAL hText;
    TCHAR* pText;
    _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number);
    GetPrivateProfileString(category, keyname, TEXT(""), pattern_data, _countof(pattern_data), datafile);

	unsigned int textLen = (_tcslen(pattern_data) + sizeof(TCHAR) + sizeof(TCHAR)) * sizeof(TCHAR);

    hText = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, textLen);
    pText = (TCHAR*)GlobalLock(hText);
    _tcscpy_s(pText,textLen, pattern_data);

    OpenClipboard(0);
    EmptyClipboard();

	UINT format = 0;

#ifdef UNICODE
	format = CF_UNICODETEXT;
#else
	format = CF_OEMTEXT;
#endif
	

    if(!SetClipboardData(format, hText))
    {
        MessageBeep(MB_ICONERROR);
        CloseClipboard();
        return;
    }
    MessageBeep(MB_ICONINFORMATION);
    CloseClipboard();
    return;
}

bool RefreshCategoryList(HWND listbox, bool maindlg)
{
    SendMessage(listbox, CB_RESETCONTENT, 0, 0);
    TCHAR category_list[PATTERN_MAX_SIZE]={0}, category_name[255]={0};
    int bufsize=GetPrivateProfileSectionNames(category_list, _countof(category_list), datafile);
    if(!bufsize)
    {
        no_categories=true;
        EnableWindow(GetDlgItem(hwndMain, IDC_BTN_COPY), FALSE);
        return false;
    }

    for(int i=0; i<bufsize; i++)
    {
        if(category_list[i])
        {
            _stprintf_s(category_name,_countof(category_name), TEXT("%s%c"), category_name, category_list[i]);
        }
        else
        {
            no_categories=false;
            SendMessage(listbox, CB_ADDSTRING, 0, (LPARAM)category_name);
            FixPatterns(category_name);
            memset(category_name, 0, 255);
        }
    }
    if(no_categories)
        EnableWindow(GetDlgItem(hwndMain, IDC_BTN_COPY), FALSE);
    else
        EnableWindow(GetDlgItem(hwndMain, IDC_BTN_COPY), TRUE);
    int select_now=0;
    if(maindlg==false && del_category==false)
        select_now=main_category_selected;

    else
    {
        del_category=false;
        main_category_selected=0;
    }


    if(!new_category)
    {
        SendMessage(listbox, CB_GETLBTEXT, select_now, (LPARAM)category_selected);
    }
    else
    {
        new_category=false;
        select_now=SendMessage(listbox, CB_FINDSTRING, (WPARAM)-1, (LPARAM)category_selected);
        if(select_now!=CB_ERR)
        {
            main_category_selected=select_now;
        }
        else
        {
            main_category_selected=0;
            select_now=0;
        }
    }
    SendMessage(listbox, CB_SETCURSEL, select_now, 0);
    return true;
}

bool RefreshPatternList(const TCHAR* category, HWND listbox, bool maindlg, HWND hwndDlg, bool othercategory)
{
    UINT addmsg=CB_ADDSTRING, setselmsg=CB_SETCURSEL, resetmsg=CB_RESETCONTENT;
    TCHAR pattern_name[255]={0}, pattern_data[PATTERN_MAX_SIZE]={0}, final_string[PATTERN_MAX_SIZE]={0}, pattern_bytes[PATTERN_MAX_SIZE]={0};
    int pattern_bytes_int=0;

    if(maindlg)
    {
        addmsg=LB_ADDSTRING;
        setselmsg=LB_SETCURSEL;
        resetmsg=LB_RESETCONTENT;
    }
    SendMessage(listbox, resetmsg, 0, 0);

    int total_patterns=GetPrivateProfileInt(category, TEXT("Patterns"), 0, datafile);
    if(!total_patterns)
        return false;

    for(int i=0; i!=total_patterns; i++)
    {
        _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dName"), (i+1));
        if(!GetPrivateProfileString(category, keyname, TEXT(""), pattern_name, 255, datafile))
            return false;
        if(maindlg)
        {
            _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), (i+1));
            if(!GetPrivateProfileString(category, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
                return false;
            _tcscpy_s(pattern_bytes,PATTERN_MAX_SIZE, pattern_data);
            FormatTextHex(pattern_bytes, false);
            pattern_bytes_int=_tcslen(pattern_bytes)/2;
            _stprintf_s(final_string,PATTERN_MAX_SIZE, TEXT("%.04d - %30s - %.03X - %s"), (i+1), pattern_name, pattern_bytes_int, pattern_data);
        }
        else
        {
            _tcscpy_s(final_string,PATTERN_MAX_SIZE, pattern_name);
        }
        SendMessage(listbox, addmsg, 0, (LPARAM)final_string);
    }
    int selectnow=0;
    if(maindlg)
    {
        main_pattern_selected=0;
        number_pattern_selected=0;
    }
    else
    {
        if(othercategory)
            selectnow=0;
        else
            selectnow=main_pattern_selected;
        number_pattern_selected=selectnow;
        SendMessage(listbox, CB_GETLBTEXT, (WPARAM)selectnow, (LPARAM)name_pattern_selected);
        if(hwndDlg)
        {
            _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), selectnow+1);
            if(!GetPrivateProfileString(category, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
                return false;
            SetDlgItemText(hwndDlg, IDC_EDT_NAME, name_pattern_selected);
            SetDlgItemText(hwndDlg, IDC_EDT_DATA, pattern_data);
        }
    }
    SendMessage(listbox, setselmsg, selectnow, 0);
    return true;
}

BOOL CALLBACK DlgAdd(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        memset(add_name_text, 0, 255 * sizeof(TCHAR));
        memset(add_data_text, 0, PATTERN_MAX_SIZE * sizeof(TCHAR));
        memset(category_selected, 0, 255 * sizeof(TCHAR));

        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
        if(category_edit)
        {
            EnableWindow(GetDlgItem(hwndDlg, IDC_EDT_DATA), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), FALSE);
            SetWindowText(hwndDlg, TEXT("Add Category"));
        }
        else
        {
            RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), false);
        }
        EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
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
        case IDC_COMBO_CATEGORY:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                int currentsel=SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETLBTEXT, (WPARAM)currentsel, (LPARAM)category_selected);
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_BTN_CANCEL:
        {
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
        }
        return TRUE;

        case IDC_BTN_OK:
        {
            if(category_edit)
            {
                if(AddCategory(add_name_text))
                {
                    MessageBox(hwndDlg, TEXT("Category added!"), TEXT("Success"), MB_ICONINFORMATION);
                    SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                }
                else
                {
                    MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                }
            }
            else
            {
                if(_tcslen(add_name_text)>30)
                    add_name_text[30]=0;
                if(add_data_text[2]==TEXT(' '))
                    FormatTextHex(add_data_text, true);
                else
                    FormatTextHex(add_data_text, false);
                if(AddPattern(category_selected, add_name_text, add_data_text))
                {
                    MessageBox(hwndDlg, TEXT("Pattern added!"), TEXT("Success"), MB_ICONINFORMATION);
                    SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                }
                else
                {
                    MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                }
            }
        }
        return TRUE;

        case IDC_EDT_NAME:
        {
            if(GetDlgItemText(hwndDlg, IDC_EDT_NAME, add_name_text, 255) && (GetDlgItemText(hwndDlg, IDC_EDT_DATA, add_data_text, PATTERN_MAX_SIZE) || category_edit==true))
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
            }
        }
        return TRUE;

        case IDC_EDT_DATA:
        {
            if(GetDlgItemText(hwndDlg, IDC_EDT_NAME, add_name_text, 255) && (GetDlgItemText(hwndDlg, IDC_EDT_DATA, add_data_text, PATTERN_MAX_SIZE) || category_edit==true))
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
            }
        }
        return TRUE;
        }
    }
    return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK DlgEdit(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        memset(add_name_text, 0, 255);
        memset(add_data_text, 0, PATTERN_MAX_SIZE);
        memset(category_selected, 0, 255);
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
        RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), false);
        if(category_edit)
        {
            EnableWindow(GetDlgItem(hwndDlg, IDC_EDT_DATA), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), FALSE);
            SetWindowText(hwndDlg, TEXT("Edit Category"));
        }
        else
        {
            RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), false, hwndDlg, false);
        }
        EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
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
        case IDC_COMBO_CATEGORY:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                int currentsel=SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETCURSEL, 0, 0);
                edit_category_selected=currentsel;
                SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETLBTEXT, (WPARAM)currentsel, (LPARAM)category_selected);
                if(!category_edit)
                {
                    number_pattern_selected=0;
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), false, hwndDlg, true);
                }
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_COMBO_PATTERN:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                //TCHAR keyname[255]={0}, pattern_data[PATTERN_MAX_SIZE]={0};
                number_pattern_selected=SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETLBTEXT, (WPARAM)number_pattern_selected, (LPARAM)name_pattern_selected);
                SetDlgItemText(hwndDlg, IDC_EDT_NAME, name_pattern_selected);
                
				_stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number_pattern_selected+1);
                if(GetPrivateProfileString(category_selected, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA, pattern_data);
                }
                else
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA, TEXT("Error!"));
                }
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_BTN_CANCEL:
        {
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
        }
        return TRUE;

        case IDC_EDT_NAME:
        {
            if(GetDlgItemText(hwndDlg, IDC_EDT_NAME, add_name_text, 255) && (GetDlgItemText(hwndDlg, IDC_EDT_DATA, add_data_text, PATTERN_MAX_SIZE) || (category_edit==true)))
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
            }
        }
        return TRUE;

        case IDC_EDT_DATA:
        {
            if(GetDlgItemText(hwndDlg, IDC_EDT_NAME, add_name_text, 255) && (GetDlgItemText(hwndDlg, IDC_EDT_DATA, add_data_text, PATTERN_MAX_SIZE) || (category_edit==true)))
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
            }
        }
        return TRUE;

        case IDC_BTN_OK:
        {
            if(category_edit)
            {
                if(MessageBox(hwndDlg, TEXT("Are you sure you want to change this category?"), category_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
                {
                    if(EditCategory(category_selected, add_name_text))
                    {
                        MessageBox(hwndDlg, TEXT("Category changed!"), TEXT("Success"), MB_ICONINFORMATION);
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                    else
                    {
                        MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                    }
                }
            }
            else
            {
                if(MessageBox(hwndDlg, TEXT("Are you sure you want to change this pattern?"), name_pattern_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
                {
                    if(_tcslen(add_name_text)>30)
                        add_name_text[30]=0;
                    if(add_data_text[2]==' ')
                        FormatTextHex(add_data_text, true);
                    else
                        FormatTextHex(add_data_text, false);
                    if(EditPattern(category_selected, number_pattern_selected+1, add_name_text, add_data_text))
                    {
                        MessageBox(hwndDlg, TEXT("Pattern changed!"), TEXT("Success"), MB_ICONINFORMATION);
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                    else
                    {
                        MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                    }
                }
            }
        }
        return TRUE;
        }
    }
    return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK DlgRemove(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), false);
        if(category_edit)
        {
            EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), FALSE);
            SetWindowText(hwndDlg, TEXT("Remove Category"));
        }
        else
        {
            RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), false, 0, false);
        }
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
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
        case IDC_COMBO_CATEGORY:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                int currentsel=SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETLBTEXT, (WPARAM)currentsel, (LPARAM)category_selected);
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_COMBO_PATTERN:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                number_pattern_selected=SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETLBTEXT, (WPARAM)number_pattern_selected, (LPARAM)name_pattern_selected);
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_BTN_OK:
        {
            if(category_edit)
            {
                if(MessageBox(hwndDlg, TEXT("Are you sure you want to remove this category?"), category_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
                {
                    if(RemoveCategory(category_selected))
                    {
                        MessageBox(hwndDlg, TEXT("Category removed!"), TEXT("Success"), MB_ICONINFORMATION);
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                    else
                    {
                        MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                    }
                }
            }
            else
            {
                if(MessageBox(hwndDlg, TEXT("Are you sure you want to remove this pattern?"), name_pattern_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
                {
                    if(RemovePattern(category_selected, number_pattern_selected+1))
                    {
                        MessageBox(hwndDlg, TEXT("Pattern removed!"), TEXT("Success"), MB_ICONINFORMATION);
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                    else
                    {
                        MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                    }
                }
            }
        }
        return TRUE;

        case IDC_BTN_CANCEL:
        {
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
        }
        return TRUE;
        }
    }
    return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK DlgUpdate(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
        RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), false);
        RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), false, 0, false);
        SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETLBTEXT, (WPARAM)number_pattern_selected, (LPARAM)add_name_text);
        //TCHAR keyname[255]={0}, pattern_data[PATTERN_MAX_SIZE]={0};

        _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number_pattern_selected+1);
        if(GetPrivateProfileString(category_selected, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
        {
            SetDlgItemText(hwndDlg, IDC_EDT_DATA1, pattern_data);
        }
        else
        {
            SetDlgItemText(hwndDlg, IDC_EDT_DATA, TEXT("Error!"));
        }
        update_nibbles=true;
        CheckDlgButton(hwndDlg, IDC_CHK_NIBBLES, update_nibbles);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), FALSE);
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
        case IDC_COMBO_CATEGORY:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                //TCHAR keyname[255]={0}, pattern_data[PATTERN_MAX_SIZE]={0};
                int currentsel=SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETCURSEL, 0, 0);
                edit_category_selected=currentsel;
                SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETLBTEXT, (WPARAM)currentsel, (LPARAM)category_selected);
                number_pattern_selected=0;
                RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_COMBO_PATTERN), false, 0, true);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETLBTEXT, (WPARAM)number_pattern_selected, (LPARAM)add_name_text);
                
				_stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number_pattern_selected+1);
                if(GetPrivateProfileString(category_selected, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA1, pattern_data);
                }
                else
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA, TEXT("Error!"));
                }
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_CHK_NIBBLES:
        {
            update_nibbles= (IsDlgButtonChecked(hwndDlg, IDC_CHK_NIBBLES) == TRUE);
            SetFocus(GetDlgItem(hwndDlg, IDC_EDT_DATA2));
        }
        return TRUE;

        case IDC_COMBO_PATTERN:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                //TCHAR keyname[255]={0}, pattern_data[PATTERN_MAX_SIZE]={0};
                number_pattern_selected=SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_PATTERN, CB_GETLBTEXT, (WPARAM)number_pattern_selected, (LPARAM)add_name_text);
                _stprintf_s(keyname,KEYNAME_MAX_SIZE, TEXT("Pattern%.04dData"), number_pattern_selected+1);
                if(GetPrivateProfileString(category_selected, keyname, TEXT(""), pattern_data, PATTERN_MAX_SIZE, datafile))
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA1, pattern_data);
                }
                else
                {
                    SetDlgItemText(hwndDlg, IDC_EDT_DATA, TEXT("Error!"));
                }
                SetFocus(GetDlgItem(hwndDlg, IDC_EDT_DATA2));
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_BTN_CANCEL:
        {
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
        }
        return TRUE;

        case IDC_BTN_OK:
        {
            if(MessageBox(hwndDlg, TEXT("Are you sure you want to update this pattern?"), name_pattern_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
            {
                if(EditPattern(category_selected, number_pattern_selected+1, add_name_text, add_data_text))
                {
                    MessageBox(hwndDlg, TEXT("Pattern updated!"), TEXT("Success"), MB_ICONINFORMATION);
                    SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                }
                else
                {
                    MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                }
            }
        }
        return TRUE;

        case IDC_EDT_DATA2:
        {
            TCHAR new_pattern_data[PATTERN_MAX_SIZE]={0},old_pattern_data[PATTERN_MAX_SIZE]={0};

            if(GetDlgItemText(hwndDlg, IDC_EDT_DATA2, new_pattern_data, PATTERN_MAX_SIZE) && GetDlgItemText(hwndDlg, IDC_EDT_DATA1, old_pattern_data, PATTERN_MAX_SIZE))
            {
                ComparePatterns(old_pattern_data, new_pattern_data, add_data_text, update_nibbles);
                SetDlgItemText(hwndDlg, IDC_EDT_DATA3, add_data_text);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_OK), TRUE);
                SetDlgItemText(hwndDlg, IDC_EDT_DATA3, TEXT(""));
            }
        }
        return TRUE;
        }
    }
    return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
		SetWindowText(hwndDlg,TEXT(TEXT_TITLE));

        TCHAR startmin[10]={0},hotkey[10]={0};
        hwndMain=hwndDlg;

        GetCurrentDirectory(_countof(curdir), curdir);
        _stprintf_s(datafile,_countof(datafile), TEXT("%s\\patterns.ini"), curdir);
        _stprintf_s(ini_file,_countof(ini_file), TEXT("%s\\settings.ini"), curdir);
		_stprintf_s(scannerLog,_countof(scannerLog), TEXT("%s\\scanner.log"), curdir);

        HICON hIcon=LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), true);
        FixPatterns(category_selected);
        RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
        if(!GetPrivateProfileString(TEXT("Settings"), TEXT("startMinimized"), TEXT(""), startmin, 10, ini_file))
        {
            WritePrivateProfileString(TEXT("Settings"), TEXT("startMinimized"), TEXT("0"), ini_file);
            _tcscpy_s(startmin,_countof(startmin), TEXT("0"));
        }
        if(!GetPrivateProfileString(TEXT("Settings"), TEXT("HotKey"), TEXT(""), hotkey, 10, ini_file))
        {
            WritePrivateProfileString(TEXT("Settings"), TEXT("HotKey"), TEXT("U"), ini_file);
            _tcscpy_s(hotkey,_countof(hotkey), TEXT("U"));
        }

        if(_tcscmp(startmin, TEXT("1"))==0)
            startMinimized=true;

        if(startMinimized)
            CreateThread(0, 0, MinimizeThread, 0, 0, 0);

        memset(&nid, 0, sizeof(nid));
        nid.cbSize=sizeof(NOTIFYICONDATA);
        nid.hWnd=hwndDlg;
        nid.uID=IDI_TRAYICON;
        nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
        nid.hIcon=hIcon;
        nid.uCallbackMessage=MSG_MINTRAYICON;
        _tcscpy_s(nid.szTip, TEXT(TEXT_TITLE));
        Shell_NotifyIcon(NIM_ADD, &nid);
        if(!RegisterHotKey(hwndDlg, IDH_RESTOREHIDE, MOD_ALT|MOD_CONTROL|MOD_WIN, VkKeyScan(_totlower(hotkey[0]))))
        {
			SetWindowText(hwndDlg, TEXT(""));
			HWND oldhwnd = FindWindow(0, TEXT(TEXT_TITLE));

			if (oldhwnd)
			{
				RestoreFromTray(oldhwnd);
			}
			
			ExitProcess(0);

        }
        if(no_categories)
            SendMessage(hwndDlg, WM_COMMAND, IDM_CATEGORY_ADD, 0);
    }
    return TRUE;

    case WM_CLOSE:
    {
        MinimizeToTray(hwndDlg);
    }
    return TRUE;

    case WM_HOTKEY:
    {
        switch(LOWORD(wParam))
        {
        case IDH_RESTOREHIDE:
        {
            if(IsMinimized)
                RestoreFromTray(hwndDlg);
            else
                MinimizeToTray(hwndDlg);
        }
        return TRUE;
        }
    }
    return TRUE;

    case MSG_MINTRAYICON:
    {
        if(wParam!=IDI_TRAYICON)
        {
            break;
        }
        else
        {
            switch(lParam)
            {
            case WM_LBUTTONUP:
            {
                if(IsMinimized)
                    RestoreFromTray(hwndDlg);
                else
                    MinimizeToTray(hwndDlg);
            }
            return TRUE;

            case WM_RBUTTONUP:
            {
                HMENU myMenu=NULL;
                myMenu=CreatePopupMenu();
                if(IsMinimized)
                    AppendMenu(myMenu, MF_STRING, TRAY_RESTORE, TEXT("Restore"));
                AppendMenu(myMenu, MF_STRING, TRAY_EXIT, TEXT("Exit"));
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                SetForegroundWindow(hwndDlg);
                UINT MenuItemClicked=TrackPopupMenu(myMenu, TPM_RETURNCMD | TPM_NONOTIFY, cursorPos.x, cursorPos.y, 0, hwndDlg, NULL);
                SendMessage(hwndDlg, WM_NULL, 0, 0);
                switch(MenuItemClicked)
                {
                case TRAY_RESTORE:
                {
                    RestoreFromTray(hwndDlg);
                }
                return TRUE;

                case TRAY_EXIT:
                {
                    if(MessageBox(hwndDlg, TEXT("Are you sure you want to exit Pattern Manager?"), TEXT("Exit?"), MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES)
                    {
                        memset(&nid, 0, sizeof(nid));
                        nid.cbSize=sizeof(NOTIFYICONDATA);
                        nid.hWnd=hwndDlg;
                        nid.uID=IDI_TRAYICON;
                        Shell_NotifyIcon(NIM_DELETE, &nid);
                        IsMinimized=false;
                        ExitProcess(0);
                    }
                }
                return TRUE;
                }
                return TRUE;
            }
            }
            break;
        }
    }
    return TRUE;

    case WM_COMMAND:
    {
        switch(LOWORD(wParam))
        {
        case IDM_FILE_HIDE:
        {
            if(IsMinimized)
                RestoreFromTray(hwndDlg);
            else
                MinimizeToTray(hwndDlg);
        }
        return TRUE;

        case IDM_FILE_EXIT:
        {
            if(MessageBox(hwndDlg, TEXT("Are you sure you want to exit Pattern Manager?"), TEXT("Exit?"), MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES)
            {
                memset(&nid, 0, sizeof(nid));
                nid.cbSize=sizeof(NOTIFYICONDATA);
                nid.hWnd=hwndDlg;
                nid.uID=IDI_TRAYICON;
                Shell_NotifyIcon(NIM_DELETE, &nid);
                IsMinimized=false;
                ExitProcess(0);
            }
        }
        return TRUE;

        case IDM_HELP_ABOUT:
        {
            MessageBox(hwndDlg, TEXT("Pattern Manager v0.2 Mod\n\nCoded by Mr. eXoDia // T.P.o.D.T 2012\nmr.exodia.tpodt@gmail.com\nhttp://www.tpodt.com\n\nSpecial thanks fly out to:\nLoki, LCF-AT and Av0id"), TEXT("About"), MB_ICONINFORMATION);
        }
        return TRUE;

        case IDC_BTN_COPY:
        {
            CopyPattern(category_selected, main_pattern_selected+1);
        }
        return TRUE;

		case IDM_PATTERN_PROCESSSCAN:
			{
				ScanProcessAction(hwndDlg);
			}
			return TRUE;
		case IDM_PATTERN_FILESCAN:
			{
				if (ScanPatternInFileAction(category_selected, main_pattern_selected+1))
				{
					MessageBox(hwndDlg, TEXT("File successful scanned"), TEXT("Success"), MB_ICONINFORMATION);
				}
			}
			return TRUE;

        case IDM_CATEGORY_ADD:
        {
            category_edit=true;
            stuff_changed=false;
            DialogBox(hInst, MAKEINTRESOURCE(DLG_ADD), hwndDlg, (DLGPROC)DlgAdd);
            if(stuff_changed)
            {
                RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), true);
                RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
            }
        }
        return TRUE;

        case IDM_CATEGORY_REMOVE:
        {
            category_edit=true;
            stuff_changed=false;
            DialogBox(hInst, MAKEINTRESOURCE(DLG_REMOVE), hwndDlg, (DLGPROC)DlgRemove);
            if(stuff_changed)
            {
                RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), true);
                RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
            }
        }
        return TRUE;

        case IDM_CATEGORY_EDIT:
        {
            if(!no_categories)
            {
                category_edit=true;
                stuff_changed=false;
                DialogBox(hInst, MAKEINTRESOURCE(DLG_EDIT), hwndDlg, (DLGPROC)DlgEdit);
                if(stuff_changed)
                {
                    RefreshCategoryList(GetDlgItem(hwndDlg, IDC_COMBO_CATEGORY), true);
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
                }
            }
            else
                MessageBeep(MB_ICONERROR);
        }
        return TRUE;

        case IDM_PATTERN_ADD:
        {
            if(!no_categories)
            {
                category_edit=false;
                stuff_changed=false;
                DialogBox(hInst, MAKEINTRESOURCE(DLG_ADD), hwndDlg, (DLGPROC)DlgAdd);
                if(stuff_changed)
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
            }
            else
                MessageBeep(MB_ICONERROR);
        }
        return TRUE;

        case IDM_PATTERN_REMOVE:
        {
            if(!no_categories)
            {
                category_edit=false;
                stuff_changed=false;
                DialogBox(hInst, MAKEINTRESOURCE(DLG_REMOVE), hwndDlg, (DLGPROC)DlgRemove);
                if(stuff_changed)
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
            }
            else
            {
                MessageBeep(MB_ICONERROR);
            }
        }
        return TRUE;

        case IDM_PATTERN_EDIT:
        {
            if(!no_categories)
            {
                category_edit=false;
                stuff_changed=false;
                DialogBox(hInst, MAKEINTRESOURCE(DLG_EDIT), hwndDlg, (DLGPROC)DlgEdit);
                if(stuff_changed)
                {
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
                }
            }
            else
            {
                MessageBeep(MB_ICONERROR);
            }
        }
        return TRUE;

        case IDM_PATTERN_UPDATE:
        {
            if(!no_categories)
            {
                category_edit=false;
                stuff_changed=false;
                DialogBox(hInst, MAKEINTRESOURCE(DLG_UPDATE), hwndDlg, (DLGPROC)DlgUpdate);
                if(stuff_changed)
                {
                    RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
                }
            }
            else
            {
                MessageBeep(MB_ICONERROR);
            }
        }
        return TRUE;

        case IDM_PATTERN_COPY:
        {
            if(!no_categories)
                SendMessage(hwndDlg, WM_COMMAND, IDC_BTN_COPY, 0);
            else
                MessageBeep(MB_ICONERROR);
        }
        return TRUE;

        case IDC_COMBO_CATEGORY:
        {
            switch(HIWORD(wParam))
            {
            case CBN_SELCHANGE:
            {
                int currentsel=SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hwndDlg, IDC_COMBO_CATEGORY, CB_GETLBTEXT, (WPARAM)currentsel, (LPARAM)category_selected);
                main_category_selected=currentsel;
                RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
            }
            return TRUE;
            }
        }
        return TRUE;

        case IDC_LIST_PATTERNS:
        {
            switch(HIWORD(wParam))
            {
            case LBN_DBLCLK:
            {
                HMENU myMenu=NULL;
                myMenu=CreatePopupMenu();
                AppendMenu(myMenu, MF_STRING, 5, TEXT("&Copy"));
                AppendMenu(myMenu, MF_STRING, 2, TEXT("&Update"));
                AppendMenu(myMenu, MF_STRING, 1, TEXT("&Add"));
                AppendMenu(myMenu, MF_STRING, 3, TEXT("&Remove"));
                AppendMenu(myMenu, MF_STRING, 4, TEXT("&Edit"));
				AppendMenu(myMenu, MF_STRING, 6, TEXT("&Scan File"));
				AppendMenu(myMenu, MF_STRING, 7, TEXT("&Scan Process"));
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                SetForegroundWindow(hwndDlg);
                UINT MenuItemClicked=TrackPopupMenu(myMenu, TPM_RETURNCMD | TPM_NONOTIFY, cursorPos.x, cursorPos.y, 0, hwndDlg, NULL);
                SendMessage(hwndDlg, WM_NULL, 0, 0);
                if(MenuItemClicked==NULL)
                    return TRUE;

                switch(MenuItemClicked)
                {
                case 1:
                {
                    SendMessage(hwndDlg, WM_COMMAND, IDM_PATTERN_ADD, 0);
                }
                return TRUE;

                case 2:
                {
                    SendMessage(hwndDlg, WM_COMMAND, IDM_PATTERN_UPDATE, 0);
                }
                return TRUE;

                case 3:
                {
                    TCHAR keyname[30]={0};
                    _stprintf_s(keyname,_countof(keyname), TEXT("Pattern%.04dName"), main_pattern_selected+1);
                    GetPrivateProfileString(category_selected, keyname, TEXT(""), name_pattern_selected, 255, datafile);
                    if(MessageBox(hwndDlg, TEXT("Are you sure you want to remove this pattern?"), name_pattern_selected, MB_YESNO|MB_ICONQUESTION)==IDYES)
                    {
                        if(RemovePattern(category_selected, main_pattern_selected+1))
                        {
                            MessageBox(hwndDlg, TEXT("Pattern removed!"), TEXT("Success"), MB_ICONINFORMATION);
                            RefreshPatternList(category_selected, GetDlgItem(hwndDlg, IDC_LIST_PATTERNS), true, 0, false);
                        }
                        else
                        {
                            MessageBox(hwndDlg, TEXT("Something went wrong..."), TEXT("Failure"), MB_ICONERROR);
                        }
                    }
                }
                return TRUE;

                case 4:
                {
                    SendMessage(hwndDlg, WM_COMMAND, IDM_PATTERN_EDIT, 0);
                }
                return TRUE;

                case 5:
                {
                    SendMessage(hwndDlg, WM_COMMAND, IDM_PATTERN_COPY, 0);
                }
                return TRUE;

				case 6:
					{
						if (ScanPatternInFileAction(category_selected, main_pattern_selected+1))
						{
							MessageBox(hwndDlg, TEXT("File successful scanned"), TEXT("Success"), MB_ICONINFORMATION);
						}
						
					}
					return TRUE;

				case 7:
					{
						ScanProcessAction(hwndDlg);
					}
					return TRUE;
                }
            }
            return TRUE;

            case LBN_SELCHANGE:
            {
                main_pattern_selected=SendDlgItemMessage(hwndDlg, IDC_LIST_PATTERNS, LB_GETCURSEL, 0, 0);
            }
            return TRUE;


            }
        }
        return TRUE;
        }
    }
    return TRUE;

    //case WM_NOTIFY:
    //{
    //  MessageBeep(MB_OK);
    /*switch(((LPNMHDR)lParam)->code)
    {
    case NM_RCLICK:
    {
        MessageBeep(MB_ICONERROR);
        asm("nop");
    }
    return TRUE;
    }
    return TRUE;*/
    /*switch(LOWORD(wParam))
    {
    case IDC_LIST_PATTERNS:
    {
        MessageBeep(MB_OK);
    }
    return TRUE;
    }*/
    //}
    //return TRUE;

    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    hInst=hInstance;
    InitCommonControls();
    return DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)DlgMain);
}

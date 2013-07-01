/*
	SteamCleaner Steam popup auto-closer.
    Copyright (C) 2013  Mitch Crane

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "windows.h"
#include "windowsx.h"
#include <shellapi.h>
#include <tchar.h>
#include <string>
#include <sstream>
#include <thread>
#include <regex>
#include <Shlobj.h>
#include <fstream>

#include "resource.h"

#include "Groups.h"

using namespace std;

#define MAX_HANDLES 256
#define MAX_TEXT 256
#define MAX_LOADSTRING 100
#define	WM_USER_SHELLICON WM_USER + 1

#define ID_MENU_ABOUT 100
#define ID_MENU_EXIT 140

// Global Variables:
HINSTANCE ghInst;
NOTIFYICONDATA nidApp;

WCHAR gszProcessName[] = L"steam.exe";

TCHAR gszTitle[MAX_LOADSTRING];
TCHAR gszWindowClass[MAX_LOADSTRING];
TCHAR gszApplicationToolTip[MAX_LOADSTRING];
HINSTANCE ghInstance;
HWND ghOpenWindow = 0;

// watcher thread vars
wstring gMatchString;
UINT gDelay;
bool gbStopThread = true;
bool gbSuspendThread= false;
DWORD gSteamPID = 0;

wstringstream gVersionString;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

DWORD FindProcess(WCHAR * ProcName);

// control subclass stuff
typedef LRESULT (__stdcall * CONTROLPROC) (HWND, UINT, WPARAM, LPARAM);
WNDPROC OldIntervalEditProc;
void SubClassControl(HWND hWnd, CONTROLPROC NewProc);
LRESULT CALLBACK IntervalEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

CGroups Groups;

struct EnumWindowsCallbackArgs
{
    EnumWindowsCallbackArgs( DWORD p ) : pid( p ), count(0) {  }
    const DWORD pid;
	int count;
	HWND handles[MAX_HANDLES];
};

static BOOL CALLBACK EnumWindowsCallback( HWND hnd, LPARAM lParam )
{
    EnumWindowsCallbackArgs *args = (EnumWindowsCallbackArgs *)lParam;
    DWORD windowPID;
    (void)::GetWindowThreadProcessId( hnd, &windowPID );
    if ( windowPID == args->pid ) {
		args->handles[args->count++] = hnd;
    }
    return TRUE;
}

bool MatchGroup(HWND hwnd)
{
	WCHAR wnd_text[MAX_TEXT];
	WCHAR * match_tail = L" - event started";
	wsmatch match;

	int tail_length, text_length, start_pos;

	text_length = GetWindowText(hwnd, wnd_text, MAX_TEXT-1);

	tail_length = wcsnlen(match_tail, MAX_TEXT);
	start_pos = text_length - tail_length;

	if (start_pos < 0 || wcsncmp(wnd_text + start_pos, match_tail, 255))
		return false;

	wnd_text[start_pos] = 0;

	for (int i = 0; i < Groups.Size(); i++)
	{
		// if matched we're done. if not keep trying
		if (!wcsncmp(wnd_text, Groups[i], MAX_TEXT))
			return false;
	}
	return true;
}


void watcher_thread() {

	static DWORD pid = 0;
	WINDOWINFO wi;

	try
	{
		while (!gbStopThread)
		{
			pid = gSteamPID;

			if (pid != 0 && !gbSuspendThread)
			{
				EnumWindowsCallbackArgs args(pid);
				EnumWindows(EnumWindowsCallback, (LPARAM) &args);
				for (int i = 0; i < args.count; i++)
				{
					GetWindowInfo(args.handles[i], &wi);
					if((wi.dwStyle & WS_VISIBLE) != 0)
					{
						if(MatchGroup(args.handles[i]))
							PostMessage(args.handles[i], WM_CLOSE, 0, 0);
					}
				}
				args.count = 0;
			}
#ifdef _DEBUG
			std::this_thread::sleep_for(std::chrono::seconds(2));
#else
			std::this_thread::sleep_for(std::chrono::seconds(gDelay));
#endif
		}
	}
	catch (...) { return; }
}

// thread to periodically refresh the steam pid
void get_pid_thread() {
	while (!gbStopThread)
	{
		gSteamPID = FindProcess(gszProcessName);
		if (gSteamPID == 0)
			std::this_thread::sleep_for(std::chrono::seconds(10));
		else
			std::this_thread::sleep_for(std::chrono::minutes(2));
	}
}

bool SavePrefs()
{
	wstring FilePath;
	LPWSTR wstrPrefsPath = 0;

	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &wstrPrefsPath);

	FilePath = wstrPrefsPath;
	FilePath += L"\\Steam Cleaner";
	BOOL bUsedDefChar = CreateDirectoryW(FilePath.data(), 0);
	FilePath += L"\\prefs.ini";

	std::wofstream output(FilePath);
	if (output != 0)
	{
		output << gDelay << endl;
		for (int i = 0; i < Groups.Size(); i++)
			output << Groups[i];
	}

	return true;
}

bool LoadPrefs()
{
	wstring FilePath;
	LPWSTR wstrPrefsPath = 0;

	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &wstrPrefsPath);

	FilePath = wstrPrefsPath;
	FilePath += L"\\Steam Cleaner";
	BOOL bUsedDefChar = CreateDirectoryW(FilePath.data(), 0);
	FilePath += L"\\prefs.ini";

	std::wifstream input(FilePath);
	if (input == 0)
	{
		gDelay = 20;
		if (!SavePrefs())
			return false;
		input.open(FilePath);
		if (input == 0)
			return false;
	}
	wstring s;
	getline(input, s);
	gDelay = _wtol(s.c_str());

	while (!input.eof())
	{
		getline(input, s);
		if (s.length() == 0)
			continue;
		Groups.Push(s.c_str(), s.length());
	}
	input.close();

	return true;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
					   _In_opt_ HINSTANCE hPrevInstance,
					   _In_ LPTSTR    lpCmdLine,
					   _In_ int       nCmdShow)
{
	LoadPrefs();

	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, gszTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SC, gszWindowClass, MAX_LOADSTRING);

	ghInstance = hInstance;
	MyRegisterClass(hInstance);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SC));

	gbStopThread = false;
	std::thread t1(get_pid_thread);
	std::thread t2(watcher_thread);

	// Main message loop:
	while (int ret = GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	CoUninitialize();

	gbSuspendThread = true;
	gbStopThread = true;
	Sleep(2000);
	t2.detach();
	t2.~thread();
	t1.detach();
	t1.~thread();

	return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_SC);
	wcex.lpszClassName	= gszWindowClass;
	wcex.hIconSm		= 0;//LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	ghInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(gszWindowClass, gszTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
		return FALSE;

	nidApp.cbSize = sizeof(NOTIFYICONDATA);
	nidApp.hWnd = (HWND) hWnd;
	nidApp.uID = IDI_SMALL; 
	nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nidApp.uCallbackMessage = WM_USER_SHELLICON;
	LoadString(hInstance, IDS_APPTOOLTIP,nidApp.szTip,MAX_LOADSTRING);
	nidApp.hIcon = LoadIcon(hInstance,(LPCTSTR)MAKEINTRESOURCE(IDI_SMALL));
	Shell_NotifyIcon(NIM_ADD, &nidApp); 

	return TRUE;
}

void MakegVersionString(HINSTANCE hInstance)
{
	DWORD dwVerInfoSize;
	DWORD dwHnd;
	VS_FIXEDFILEINFO *pFixedInfo;	// pointer to fixed file info structure
	UINT    uVersionLen;			// Current length of full version string
	WCHAR AppPathName[MAX_PATH];
	WCHAR AppName[MAX_LOADSTRING+1];

	ZeroMemory(AppName, (MAX_LOADSTRING+1) * sizeof(WCHAR));

	GetModuleFileName(hInstance, AppPathName, MAX_PATH);
	dwVerInfoSize = GetFileVersionInfoSize(AppPathName, &dwHnd);
	void *pBuffer = new char [dwVerInfoSize];
	if(pBuffer == 0)
		return;
	GetFileVersionInfo(AppPathName, dwHnd, dwVerInfoSize, pBuffer);
	VerQueryValue(pBuffer,_T("\\"),(void**)&pFixedInfo,(UINT *)&uVersionLen);

	LoadString(hInstance, IDS_APP_TITLE, AppName, MAX_LOADSTRING);
	gVersionString << AppName
		<< L" " << HIWORD (pFixedInfo->dwProductVersionMS)
		<< L"." << LOWORD (pFixedInfo->dwProductVersionMS)
		<< L"." << HIWORD (pFixedInfo->dwProductVersionLS);
	if(LOWORD (pFixedInfo->dwProductVersionLS) != 0)
		gVersionString << L"." << LOWORD (pFixedInfo->dwProductVersionLS);
}

void AppendTextToEditCtrl(HWND hWndEdit, LPCTSTR pszText)
{
   int nLength = Edit_GetTextLength(hWndEdit); 
   Edit_SetSel(hWndEdit, nLength, nLength);
   Edit_ReplaceSel(hWndEdit, pszText);
   Edit_ReplaceSel(hWndEdit, L"\r\n");
}

void InitIntervalEditBox(HWND hDlg)
{
	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_INTERVAL);
	SetWindowText(hEdit, to_wstring(gDelay).c_str());
}

void ReadIntervalEditBox(HWND hDlg)
{
	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_INTERVAL);
	WCHAR text[6];
	memset(text, 0, 6 * sizeof(WCHAR));
	GetWindowText(hEdit, text, 5);
	gDelay = _wtol(text);
}

void InitNamesEditBox(HWND hDlg)
{
	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_NAMES);

	for (int i = 0; i < Groups.Size(); i++)
		AppendTextToEditCtrl(hEdit, Groups[i]);
}

void ReadNamesEditBox(HWND hDlg)
{
	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_NAMES);
	WCHAR line[256];

	Groups.Clear();

	for (int i = 0; i < Edit_GetLineCount(hEdit); i++)
	{
		int c = Edit_GetLine(hEdit, i, line, 255);
		line[c] = L'\0';
		if (c != 0)
			Groups.Push(line, c);
	}
}

// Message handler for settings dialog
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		gbSuspendThread = true;
		ghOpenWindow = hDlg;
		SubClassControl(GetDlgItem(hDlg, IDC_EDIT_INTERVAL), IntervalEditProc);
		MakegVersionString(ghInstance);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_APP), gVersionString.str().data());
		InitIntervalEditBox(hDlg);
		InitNamesEditBox(hDlg);
		return (INT_PTR)TRUE;
	case WM_KEYDOWN:
		return (INT_PTR)FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			gbSuspendThread = false;
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		case ID_OK:
			ReadIntervalEditBox(hDlg);
			ReadNamesEditBox(hDlg);
			SavePrefs();
			gbSuspendThread = false;
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	case WM_DESTROY:
		ghOpenWindow = 0;
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT lpClickPoint;
	HMENU hPopMenu;
	wstring MenuString, TempString;
	int wmId, wmEvent;

	switch (message)
	{
	case WM_CREATE:
		return TRUE;
	case WM_USER_SHELLICON: 
		// systray msg callback 
		switch(LOWORD(lParam)) 
		{   
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			if (IsWindow(ghOpenWindow))
			{
				SetForegroundWindow(ghOpenWindow);
				return TRUE;
			}
			GetCursorPos(&lpClickPoint);
			hPopMenu = CreatePopupMenu();
			InsertMenu(hPopMenu,0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_MENU_ABOUT, _T("Settings..."));
			InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR|MF_BYPOSITION, 0, NULL);
			InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_MENU_EXIT, _T("Exit"));
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hPopMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_BOTTOMALIGN,lpClickPoint.x, lpClickPoint.y,0,hWnd,NULL);
			return TRUE; 
		}
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId)
		{
		case ID_MENU_ABOUT:
			DialogBox(ghInst, MAKEINTRESOURCE(IDD_DIALOG_ABOUT), hWnd, About);
			return TRUE;
		case ID_MENU_EXIT:
			Shell_NotifyIcon(NIM_DELETE,&nidApp);
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			DestroyWindow(hWnd);
			return TRUE;
		}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return FALSE;
}

// subclassing interval edit box so it only takes
// numerical input
void SubClassControl(HWND hWnd, CONTROLPROC NewProc)
{
	OldIntervalEditProc = (WNDPROC)GetWindowLong(hWnd, GWL_WNDPROC);
	SetWindowLong(hWnd, GWL_WNDPROC, (LONG_PTR)NewProc);
}

LRESULT CALLBACK IntervalEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg)
	{
	case WM_CHAR:
		if ((wParam >= 0x30 && wParam <= 0x39) || (wParam >= VK_BACK && wParam <= VK_DELETE))
		{
			if (GetWindowTextLength(hwnd) > 5)
				return 0;
			return CallWindowProc(OldIntervalEditProc, hwnd, msg, wParam, lParam);
		}
		return 0;
	}
	return CallWindowProc(OldIntervalEditProc, hwnd, msg, wParam, lParam);
}
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <locale.h>
#include <atlstr.h>
#include <tlhelp32.h>
#include <list>
#include <thread>
#include <Shlwapi.h>

#pragma warning(disable: 4996)
#pragma comment(lib, "Shlwapi.lib")

// 32bit, 64bit 확인
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL IsWow64();

// 프로그램 검색 및 삭제
bool GetInstalledProgram(WCHAR *regKeyPath, WCHAR* ProgramName);

// 프로세스 점검을 위한 파일 탐색
void ExitRelationProcess(TCHAR* folderPath);

// Process handle 가져오기
ULONG ProcIDFromWnd(HWND hwnd);
HWND GetWinHandle(ULONG pid);

// Child Window Get
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

// Main Window Get
BOOL CALLBACK WorkerProc(HWND hwnd, LPARAM lParam);

// 설치 종료 플래그
bool unInstallFinished = false;

// 설치 리부트 플래그
bool unInstallReboot = false;

// 설치 종료 이후 웹사이트 제거
bool usingThread = true;
int check_inteval = 1500;
void ExitUninstallAfterWeb();

// 설치 메인 윈도우 텍스트
TCHAR MainWindowText[1024];
HWND SubWindowHWND;

// MCAFEE HARDCODING
void UninstallMcafee(HWND hwnd);

// 설치 프로세스
bool IsFileExecuteUninstall(WCHAR* execPath, WCHAR* existspath);
bool IsNormalUninstall(WCHAR* execPath, WCHAR* existspath);

int main(int argc, char** argv)
{
	argv[1] = "XAMPP";

	WCHAR ProgramName[1024] = { '\0', };
	size_t org_len = strlen(argv[1]) + 1;
	size_t convertedChars = 0;
	setlocale(LC_ALL, "korean");

	mbstowcs_s(&convertedChars, ProgramName, org_len, argv[1], _TRUNCATE);

	GetInstalledProgram(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);
	GetInstalledProgram(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);
	
	getchar();
	system("pause");
	return 0;
}

bool GetInstalledProgram(WCHAR *regKeyPath, WCHAR* ProgramName)
{
	HKEY hUninstKey = NULL;
	HKEY hAppKey = NULL;
	WCHAR sAppKeyName[1024] = { '\0', };
	WCHAR sSubKey[1024] = { '\0', };
	WCHAR sDisplayName[1024] = { '\0', };
	WCHAR sUninstallString[1024] = { '\0', };
	WCHAR sQuietUninstallString[1024] = { '\0', };
	WCHAR sInstallLocation[1024] = { '\0', };
	long lResult = ERROR_SUCCESS;
	DWORD dwType = KEY_ALL_ACCESS;
	DWORD dwBufferSize = 0;
	DWORD dwDisplayBufSize = 0;
	DWORD dwsUninstallBufSize = 0;
	DWORD dwQuietUninstallBufSize = 0;
	DWORD dwInstallLocation = 0;
	bool regOpenResult = false;

	if (IsWow64())
		regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKeyPath, 0, KEY_READ | KEY_WOW64_64KEY, &hUninstKey) != ERROR_SUCCESS;
	else
		regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKeyPath, 0, KEY_READ, &hUninstKey) != ERROR_SUCCESS;

	if (regOpenResult)
		return false;

	for (DWORD dwIndex = 0; lResult == ERROR_SUCCESS; dwIndex++)
	{
		dwBufferSize = sizeof(sAppKeyName);
		if ((lResult = RegEnumKeyEx(hUninstKey, dwIndex, sAppKeyName,
			&dwBufferSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
		{
			wsprintf(sSubKey, L"%s\\%s", regKeyPath, sAppKeyName);

			if (IsWow64())
				regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hAppKey) != ERROR_SUCCESS;
			else
				regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ, &hAppKey) != ERROR_SUCCESS;

			if (regOpenResult)
			{
				RegCloseKey(hAppKey);
				RegCloseKey(hUninstKey);
				return false;
			}

			dwDisplayBufSize = sizeof(sDisplayName);
			dwsUninstallBufSize = sizeof(sUninstallString);
			dwQuietUninstallBufSize = sizeof(sQuietUninstallString);
			dwInstallLocation = sizeof(sInstallLocation);

			RegQueryValueEx(hAppKey, L"DisplayName", NULL, &dwType, (unsigned char*)sDisplayName, &dwDisplayBufSize);
			RegQueryValueEx(hAppKey, L"UninstallString", NULL, &dwType, (unsigned char*)sUninstallString, &dwsUninstallBufSize);
			RegQueryValueEx(hAppKey, L"QuietUninstallString", NULL, &dwType, (unsigned char*)sQuietUninstallString, &dwQuietUninstallBufSize);
			RegQueryValueEx(hAppKey, L"InstallLocation", NULL, &dwType, (unsigned char*)sInstallLocation, &dwInstallLocation);

			if (sDisplayName[0] != '\0' && wcscmp(sDisplayName, ProgramName) == 0) {
				wprintf(L"=====================================\n");
				wprintf(L"프로그램 명: %s\n", sDisplayName);

				wprintf(L"레지스트리 경로: %s\\%s\n", regKeyPath, sAppKeyName);

				if (sUninstallString[0] != '\0')
					wprintf(L"언인스톨 링크: %s\n", sUninstallString);

				if (sQuietUninstallString[0] != '\0')
					wprintf(L"백그라운드 언인스톨 링크: %s\n", sQuietUninstallString);
				wprintf(L"=====================================\n");

				wprintf(L"제거 시작\n");

				if (sAppKeyName[0] == '{' && sQuietUninstallString[0] == '\0')
				{
					// 응용프로그램 코드값으로 설치 되었을 때
					// 레지스트리 값을 이용하지 않고 응용프로그램 설치 고유 ID로 MsiExec를 통해 SLIENT 제거 진행
					WCHAR slientUninstallPath[1024] = L"MsiExec.exe /x";
					wcscat_s(slientUninstallPath, sAppKeyName);
					wcscat_s(slientUninstallPath, L" /quiet");
					
					if (IsFileExecuteUninstall(slientUninstallPath, sUninstallString))
						printf("제거 완료");
					else
					{
						// 정상 제거 실패 시 일반 삭제로 변경
						printf("제거 실패 [일반 제거로 변경]");
						IsNormalUninstall(slientUninstallPath, sUninstallString);
					}
				}
				else {
					// SLIENT 언인스톨 가능 시
					if (sQuietUninstallString[0] != '\0')
					{
						if (IsFileExecuteUninstall(sQuietUninstallString, sUninstallString))
							printf("제거 완료");
						else
							printf("제거 실패");
					}
					else if (sUninstallString[0] != '\0')
					{
						// MsiExec 사용 시 SLIENT 언인스톨로 변경
						if (wcsstr(sUninstallString, L"MsiExec.exe"))
						{
							sUninstallString[13] = 'x';
							wcscat_s(sUninstallString, L" /quiet");

							if (IsFileExecuteUninstall(sUninstallString, sUninstallString))
								printf("제거 완료");
							else
								printf("제거 실패");
						}
						else
						{
							if (IsNormalUninstall(sUninstallString, sUninstallString))
								printf("제거 완료");
							else
								printf("제거 실패");
						}
					}
				}

				// 삭제 프로세스 완료 후 익스플로러 체크
				ExitUninstallAfterWeb();
			}
			sDisplayName[0] = '\0';
			sUninstallString[0] = '\0';
			sQuietUninstallString[0] = '\0';

			RegCloseKey(hAppKey);
		}
	}

	RegCloseKey(hUninstKey);

	return true;
}

ULONG ProcIDFromWnd(HWND hwnd) 
{
	ULONG idProc;
	GetWindowThreadProcessId(hwnd, &idProc);
	return idProc;
}

HWND GetWinHandle(ULONG pid)  
{
	HWND tempHwnd = FindWindow(NULL, NULL);  

	while (tempHwnd != NULL)
	{
		if (GetParent(tempHwnd) == NULL) 
			if (pid == ProcIDFromWnd(tempHwnd))
				return tempHwnd;
		tempHwnd = GetWindow(tempHwnd, GW_HWNDNEXT); 
	}
	return NULL;
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {

	TCHAR lstFindText[21][15] = { _T("Next"), _T("다음"), _T("닫음"), _T("Uninstall"), _T("Remove"), _T("예"), _T("예(Y)") , _T("제거"), _T("마침"), _T("Yes"), _T("Close"), _T("Finish"), _T("세이프"), _T("확인"), _T("예(&Y)"), _T("완료"), _T("무시(&I)"), _T("OK")
								, _T("&Close"), _T("&Uninstall"), _T("Ok") };
	TCHAR lstFinishText[5][10] = { _T("Finish"), _T("닫기"), _T("제거되었습니다"), _T("마침"), _T("완료") };
	TCHAR lstRebootText[2][10] = { _T("Reboot"), _T("Restart") };
	TCHAR lstRebootFindText[5][10] = { _T("No"), _T("Later"), _T("later"), _T("나중") , _T("다음") };
	TCHAR ctrlText[512];
	TCHAR ctrlClass[512];
	CString findCtrlText;
	CString findCtrlClass;
	GetWindowText(hwnd, ctrlText, 512);
	GetClassName(hwnd, ctrlClass, 512);

	findCtrlText = (LPCTSTR)ctrlText;
	findCtrlClass = (LPCTSTR)ctrlClass;
	
	bool find_OK = false;
	for (int i = 0; i < sizeof(lstFinishText) / sizeof(lstFinishText[0]); i++)
	{
		if (findCtrlText.Find(lstFinishText[i]) >= 0)
			unInstallFinished = true;
	}

	for (int i = 0; i < sizeof(lstRebootFindText) / sizeof(lstRebootFindText[0]); i++)
	{
		if (findCtrlText.Find(lstRebootFindText[i]) >= 0)
			unInstallReboot = true;
	}

	if (!unInstallReboot)
	{
		for (int i = 0; i < sizeof(lstFindText) / sizeof(lstFindText[0]); i++)
		{
			if (findCtrlText.Find(lstFindText[i]) >= 0)
			{
				SendMessage(hwnd, BM_CLICK, 0, 0);
				break;
			}
			else if (findCtrlClass.Find(L"QWidget") >= 0)
			{
				// Caption 으로 찾지못하는 QWidget 버튼의 경우 key press 로 보냄
				// SetActiveWindow(GetActiveWindow());

				// YES 버튼
				PostMessage(hwnd, WM_KEYDOWN, 'y', 1);
				PostMessage(hwnd, WM_KEYUP, 'y', 1);

				// 엔터 버튼
				SendMessage(hwnd, WM_KEYDOWN, VK_RETURN, 1);
				SendMessage(hwnd, WM_KEYUP, VK_RETURN, 1);
			}
		}
	}
	else
	{
		for (int i = 0; i < sizeof(lstRebootFindText) / sizeof(lstRebootFindText[0]); i++)
		{
			if (findCtrlText.Find(lstRebootFindText[i]) >= 0)
			{
				SendMessage(hwnd, BM_CLICK, 0, 0);

				// init flag
				unInstallReboot = false;

				break;
			}
			else if (findCtrlClass.Find(L"QWidget") >= 0)
			{
				// Caption 으로 찾지못하는 QWidget 버튼의 경우 key press 로 보냄
				// SetActiveWindow(GetActiveWindow());

				// YES 버튼
				PostMessage(hwnd, WM_KEYDOWN, 'n', 1);
				PostMessage(hwnd, WM_KEYUP, 'n', 1);

				// Later 버튼
				PostMessage(hwnd, WM_KEYDOWN, 'l', 1);
				PostMessage(hwnd, WM_KEYUP, 'l', 1);

				// init flag
				unInstallReboot = false;
			}
		}
	}

	return TRUE;
}

BOOL CALLBACK WorkerProc(HWND hwnd, LPARAM lParam) {
	static TCHAR buffer[1024];
	GetWindowText(hwnd, buffer, 1024);

	CString distText = (LPCTSTR)buffer;
	CString sourceText = (LPCTSTR)MainWindowText;

	if (distText.Find(sourceText) >= 0) {
		SubWindowHWND = hwnd; 
	}

	return TRUE;
}

void ExitRelationProcess(TCHAR* folderPath)
{
	PathAddBackslash(folderPath);
	PathAddExtension(folderPath, L"*.exe");

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 ProcessEntry32;
	ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);
	
	WIN32_FIND_DATA find_data;
	HANDLE file_handle;

	file_handle = FindFirstFile(folderPath, &find_data);

	if (file_handle == INVALID_HANDLE_VALUE)
		return;

	do {
		CString process_file;
		process_file = (LPCTSTR)find_data.cFileName;

		BOOL bProcessFound = Process32First(hSnapshot, &ProcessEntry32);
		while (bProcessFound)
		{
			// 프로세스랑 파일명하고 매칭 되면
			if (process_file.Compare((LPCTSTR)ProcessEntry32.szExeFile) == 0)
			{
				HANDLE open_handle = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessEntry32.th32ProcessID);
				TerminateProcess(open_handle, ((UINT)-1));
			}
			bProcessFound = Process32Next(hSnapshot, &ProcessEntry32);
		}
	} while (FindNextFile(file_handle, &find_data));

	CloseHandle(hSnapshot);
}

void ExitUninstallAfterWeb()
{
	while (usingThread)
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		PROCESSENTRY32 ProcessEntry32;
		ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);

		int i = 0;

		Process32First(hSnapshot, &ProcessEntry32);

		while (Process32Next(hSnapshot, &ProcessEntry32) == TRUE)
		{
			// 인터넷 프로세스의 경우
			if (wcsstr(L"cmd.exe", (LPCTSTR)ProcessEntry32.szExeFile))
			{
				HANDLE open_handle = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessEntry32.th32ProcessID);
				TerminateProcess(open_handle, ((UINT)-1));
			}
		}
	}
}

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}

void UninstallMcafee(HWND hwnd)
{
	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE 버튼
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// 페이지 전환 딜레이
	Sleep(3000);

	while (hwnd != NULL)
	{
		if (IsWindow(hwnd))
		{
			SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(80, 500));
			SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(80, 500));
			SendMessage(hwnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(80, 500));
		}
		else break;
	}
}

bool IsFileExecuteUninstall(WCHAR* execPath, WCHAR* existspath)
{

	STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
	startup_info.dwFlags = STARTF_USESHOWWINDOW;
	startup_info.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION proc_info;
	BOOL proc_ret;

	proc_ret = CreateProcess(NULL, execPath, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

	std::thread bat_block_thread(&ExitUninstallAfterWeb);
	WaitForInputIdle(proc_info.hProcess, INFINITE);

	// 1.5초간 인터넷 실행 명령어 체크
	Sleep(check_inteval);

	usingThread = false;
	bat_block_thread.join();

	if (!PathFileExists(existspath))
		return true;
	else
		return false;
}

bool IsNormalUninstall(WCHAR* execPath, WCHAR* existspath)
{

	STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
	startup_info.dwFlags = STARTF_USESHOWWINDOW;
	startup_info.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION proc_info;
	BOOL proc_ret;

	HWND CHWND = GetForegroundWindow();
	proc_ret = CreateProcess(NULL, execPath, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

	if (proc_ret)
	{
		WaitForInputIdle(proc_info.hProcess, INFINITE);
		std::thread bat_block_thread(&ExitUninstallAfterWeb);

		HWND UninstallHWND = GetForegroundWindow();
		while (CHWND == UninstallHWND)
		{
			Sleep(100);
			UninstallHWND = GetForegroundWindow();
		}

		if (UninstallHWND != NULL)
		{
			TCHAR CLSNAME[1024];
			// 설치 제거 타이틀 가져오기
			GetWindowText(UninstallHWND, MainWindowText, 1024);
			GetClassName(UninstallHWND, CLSNAME, 1024);

			// Internet Explorer_Server 인지 체크
			HWND embWindow = FindWindowEx(UninstallHWND, NULL, L"Shell Embedding", NULL);

			if (embWindow != NULL)
			{
				HWND docWindow = FindWindowEx(embWindow, NULL, L"Shell DocObject View", NULL);
				HWND ieWindow = FindWindowEx(docWindow, NULL, L"Internet Explorer_Server", NULL);

				while (ieWindow == NULL)
				{
					ieWindow = FindWindowEx(docWindow, NULL, L"Internet Explorer_Server", NULL);
				}

				// WEB 기반 제거의 경우
				if (wcsstr(MainWindowText, L"McAfee  보안센터"))
				{
					Sleep(5000);
					UninstallMcafee(ieWindow);
				}
			}

			// 관련 프로세스 종료
			if (existspath[0] != '\0')
				ExitRelationProcess(existspath);

			// 타이틀로 끝날때 까지 찾기
			while (true)
			{
				if (!unInstallFinished)
				{
					HWND unintstallSub = FindWindow(NULL, MainWindowText);

					// 이름이 변경되어 못찾을 경우
					if (unintstallSub == NULL)
					{
						EnumWindows(WorkerProc, 0);

						if (SubWindowHWND == NULL)
							break;
						else
							EnumChildWindows(SubWindowHWND, EnumChildProc, 0);
					}
					else
					{
						// XAMPP 의 특별한 케이스로 하드코딩
						HWND TitleHard = FindWindow(NULL, L"Info");

						if (TitleHard != NULL)
							EnumChildWindows(TitleHard, EnumChildProc, 0);
						else
							EnumChildWindows(unintstallSub, EnumChildProc, 0);
					}

					Sleep(1000);
				}
				else break;
			}
		}
		// 1.5초간 인터넷 실행 명령어 체크
		Sleep(check_inteval);

		usingThread = false;
		bat_block_thread.join();

		if (!PathFileExists(existspath))
			return true;
		else
			return false;
	}
}
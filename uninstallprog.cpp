#include <Windows.h>
#include <stdio.h>
#include <string>
#include <locale.h>
#include <atlstr.h>
#include <tlhelp32.h>
#include <list>
#include <thread>

#pragma warning(disable: 4996)

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

int main(int argc, char** argv)
{
	argv[1] = "7-Zip 16.04 (x64)";

	WCHAR ProgramName[1024] = { '\0', };
	size_t org_len = strlen(argv[1]) + 1;
	size_t convertedChars = 0;
	setlocale(LC_ALL, "korean");

	mbstowcs_s(&convertedChars, ProgramName, org_len, argv[1], _TRUNCATE);

	GetInstalledProgram(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);
	GetInstalledProgram(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);

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

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKeyPath, 0, KEY_READ, &hUninstKey) != ERROR_SUCCESS)
		return false;

	for (DWORD dwIndex = 0; lResult == ERROR_SUCCESS; dwIndex++)
	{
		dwBufferSize = sizeof(sAppKeyName);
		if ((lResult = RegEnumKeyEx(hUninstKey, dwIndex, sAppKeyName,
			&dwBufferSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
		{
			wsprintf(sSubKey, L"%s\\%s", regKeyPath, sAppKeyName);
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ, &hAppKey) != ERROR_SUCCESS)
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

				if (sUninstallString[0] != '\0')
					wprintf(L"언인스톨 링크: %s\n", sUninstallString);

				if (sQuietUninstallString[0] != '\0')
					wprintf(L"백그라운드 언인스톨 링크: %s\n", sQuietUninstallString);
				wprintf(L"=====================================\n");

				wprintf(L"제거 시작\n");

				STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
				startup_info.dwFlags = STARTF_USESHOWWINDOW;
				startup_info.wShowWindow = SW_HIDE;

				PROCESS_INFORMATION proc_info;
				BOOL proc_ret;

				if (sAppKeyName[0] == '{')
				{
					// 응용프로그램 코드값으로 설치 되었을 때
					// 레지스트리 값을 이용하지 않고 응용프로그램 설치 고유 ID로 MsiExec를 통해 SLIENT 제거 진행
					WCHAR slientUninstallPath[1024] = L"MsiExec.exe /x";
					wcscat_s(slientUninstallPath, sAppKeyName);
					wcscat_s(slientUninstallPath, L" /quiet");

					proc_ret = CreateProcess(NULL, slientUninstallPath, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

					std::thread bat_block_thread(&ExitUninstallAfterWeb);
					WaitForInputIdle(proc_info.hProcess, INFINITE);

					// 1.5초간 인터넷 실행 명령어 체크
					Sleep(check_inteval);

					usingThread = false;
					bat_block_thread.join();
					printf("제거 완료");
				}
				else {
					// SLIENT 언인스톨 가능 시
					if (sQuietUninstallString[0] != '\0')
					{
						proc_ret = CreateProcess(NULL, sQuietUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

						std::thread bat_block_thread(&ExitUninstallAfterWeb);
						WaitForInputIdle(proc_info.hProcess, INFINITE);

						// 1.5초간 인터넷 실행 명령어 체크
						Sleep(check_inteval);

						usingThread = false;
						bat_block_thread.join();
						printf("제거 완료");
					}
					else if (sUninstallString[0] != '\0')
					{
						// MsiExec 사용 시 SLIENT 언인스톨로 변경
						if (wcsstr(sUninstallString, L"MsiExec.exe"))
						{
							sUninstallString[13] = 'x';
							wcscat_s(sUninstallString, L" /quiet");

							proc_ret = CreateProcess(NULL, sUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);
							
							std::thread bat_block_thread(&ExitUninstallAfterWeb);
							WaitForInputIdle(proc_info.hProcess, INFINITE);

							// 1.5초간 인터넷 실행 명령어 체크
							Sleep(check_inteval);

							usingThread = false;
							bat_block_thread.join();
							printf("제거 완료");
						}
						else
						{
							HWND CHWND = GetForegroundWindow();
							proc_ret = CreateProcess(NULL, sUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

							if (proc_ret)
							{
								std::thread bat_block_thread(&ExitUninstallAfterWeb);
								WaitForInputIdle(proc_info.hProcess, INFINITE);
								
								HWND UninstallHWND = GetForegroundWindow();
								while (CHWND == UninstallHWND)
								{
									Sleep(100);
									UninstallHWND = GetForegroundWindow();
								}
								if (UninstallHWND != NULL)
								{
									// 설치 제거 타이틀 가져오기
									GetWindowText(UninstallHWND, MainWindowText, 1024);

									// 관련 프로세스 종료
									if (sInstallLocation[0] != '\0')
										ExitRelationProcess(sInstallLocation);

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
								printf("제거 완료");

							}
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

	TCHAR lstFindText[10][10] = { _T("Next"), _T("다음"), _T("닫음"), _T("Uninstall"), _T("Remove"), _T("예") , _T("제거"), _T("마침"), _T("Yes"), _T("Close") };
	TCHAR lstFinishText[4][10] = { _T("Finish"), _T("닫기"), _T("제거되었습니다"), _T("마침") };
	TCHAR lstRebootText[2][10] = { _T("Reboot"), _T("Restart") };
	TCHAR lstRebootFindText[4][10] = { _T("No"), _T("Later"), _T("나중") , _T("다음") };
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
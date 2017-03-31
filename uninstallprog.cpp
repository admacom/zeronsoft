#include <Windows.h>
#include <stdio.h>
#include <string>
#include <locale.h>
#include <atlstr.h>

#define PRG_NAME_LENGTH 1024

/* BIT 체크 */
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

bool GetInstalledProgram(WCHAR *regKeyPath, WCHAR* ProgramName);
bool IsWindowsBit64();

ULONG ProcIDFromWnd(HWND hwnd);
HWND GetWinHandle(ULONG pid);

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

// 설치 종료 플래그
bool unInstallFinished = false;

int main(int argc, char** argv)
{
	argv[1] = "Oracle VM VirtualBox 5.1.18";

	WCHAR ProgramName[1024] = { '\0', };
	size_t org_len = strlen(argv[1]) + 1;
	size_t convertedChars = 0;
	setlocale(LC_ALL, "korean");

	mbstowcs_s(&convertedChars, ProgramName, org_len, argv[1], _TRUNCATE);

	GetInstalledProgram(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);
	GetInstalledProgram(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);

	system("pause");
}

bool IsWindowsBit64()
{
	BOOL bIsWow64 = FALSE;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "IsWow64Process");

	if (fnIsWow64Process != NULL)
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}

	return bIsWow64;
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
	long lResult = ERROR_SUCCESS;
	DWORD dwType = KEY_ALL_ACCESS;
	DWORD dwBufferSize = 0;
	DWORD dwDisplayBufSize = 0;
	DWORD dwsUninstallBufSize = 0;
	DWORD dwQuietUninstallBufSize = 0;

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

			RegQueryValueEx(hAppKey, L"DisplayName", NULL, &dwType, (unsigned char*)sDisplayName, &dwDisplayBufSize);
			RegQueryValueEx(hAppKey, L"UninstallString", NULL, &dwType, (unsigned char*)sUninstallString, &dwsUninstallBufSize);
 			RegQueryValueEx(hAppKey, L"QuietUninstallString", NULL, &dwType, (unsigned char*)sQuietUninstallString, &dwQuietUninstallBufSize);

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

				// SLIENT 언인스톨 가능 시
				if (sQuietUninstallString[0] != '\0')
					proc_ret = CreateProcess(NULL, sQuietUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);
				else if (sUninstallString[0] != '\0')
				{
					// MsiExec 사용 시 SLIENT 언인스톨로 변경
					if (wcsstr(sUninstallString, L"MsiExec.exe"))
					{
						 sUninstallString[13] = 'x';
						 wcscat_s(sUninstallString, L" /quiet");

						proc_ret = CreateProcess(NULL, sUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);
					}
					else
					{
						HWND proc_HWND = NULL;
						proc_ret = CreateProcess(sUninstallString, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

						if (proc_ret)
						{
							WaitForInputIdle(proc_info.hProcess, INFINITE);

							Sleep(1000);

							HWND UninstallHWND = GetWinHandle(proc_info.dwProcessId);

							if (UninstallHWND != NULL)
							{
								// 설치 제거 타이틀 가져오기
								TCHAR ctrlText[1024];
								GetWindowText(UninstallHWND, ctrlText, 1024);

								// 타이틀로 끝날때 까지 찾기
								while (true)
								{
									if (!unInstallFinished)
									{
										HWND unintstallSub = FindWindow(NULL, ctrlText);
										EnumChildWindows(unintstallSub, EnumChildProc, 0);

										Sleep(1000);
									}
									else break;
								}
							}

							printf("제거 완료");
							
						}
					}
				}
				else return false;
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

	TCHAR lstFindText[4][10] = { _T("Next"), _T("다음"), _T("Uninstall"), _T("Remove") };
	TCHAR lstFinishText[2][10] = { _T("Finish"), _T("닫기") };
	TCHAR ctrlText[512];
	CString findCtrlText;
	GetWindowText(hwnd, ctrlText, 512);

	findCtrlText = (LPCTSTR)ctrlText;

	bool find_OK = false;
	for (int i = 0; i < sizeof(lstFindText) / sizeof(lstFindText[0]); i++)
	{
		if (findCtrlText.Find(lstFinishText[i]) >= 0)
		{
			find_OK = true;
			unInstallFinished = true;
			break;
		}

		if (findCtrlText.Find(lstFindText[i]) >= 0)
		{
			find_OK = true;
			break;
		}
	}

	if (find_OK)
		SendMessage(hwnd, BM_CLICK, 0, 0);

	return TRUE;
}
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <locale.h>
#include <atlstr.h>
#include <tlhelp32.h>
#include <list>
#include <thread>

#pragma warning(disable: 4996)

// 32bit, 64bit Ȯ��
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL IsWow64();

// ���α׷� �˻� �� ����
bool GetInstalledProgram(WCHAR *regKeyPath, WCHAR* ProgramName);

// ���μ��� ������ ���� ���� Ž��
void ExitRelationProcess(TCHAR* folderPath);

// Process handle ��������
ULONG ProcIDFromWnd(HWND hwnd);
HWND GetWinHandle(ULONG pid);

// Child Window Get
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

// Main Window Get
BOOL CALLBACK WorkerProc(HWND hwnd, LPARAM lParam);

// ��ġ ���� �÷���
bool unInstallFinished = false;

// ��ġ ����Ʈ �÷���
bool unInstallReboot = false;

// ��ġ ���� ���� ������Ʈ ����
bool usingThread = true;
int check_inteval = 1500;
void ExitUninstallAfterWeb();

// ��ġ ���� ������ �ؽ�Ʈ
TCHAR MainWindowText[1024];
HWND SubWindowHWND;

// MCAFEE HARDCODING
void UninstallMcafee(HWND hwnd);

int main(int argc, char** argv)
{
	argv[1] = "HeidiSQL";

	WCHAR ProgramName[1024] = { '\0', };
	size_t org_len = strlen(argv[1]) + 1;
	size_t convertedChars = 0;
	setlocale(LC_ALL, "korean");

	mbstowcs_s(&convertedChars, ProgramName, org_len, argv[1], _TRUNCATE);

	GetInstalledProgram(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);
	GetInstalledProgram(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", ProgramName);

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
				wprintf(L"���α׷� ��: %s\n", sDisplayName);

				if (sUninstallString[0] != '\0')
					wprintf(L"���ν��� ��ũ: %s\n", sUninstallString);

				if (sQuietUninstallString[0] != '\0')
					wprintf(L"��׶��� ���ν��� ��ũ: %s\n", sQuietUninstallString);
				wprintf(L"=====================================\n");

				wprintf(L"���� ����\n");

				STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
				startup_info.dwFlags = STARTF_USESHOWWINDOW;
				startup_info.wShowWindow = SW_HIDE;

				PROCESS_INFORMATION proc_info;
				BOOL proc_ret;

				if (sAppKeyName[0] == '{')
				{
					// �������α׷� �ڵ尪���� ��ġ �Ǿ��� ��
					// ������Ʈ�� ���� �̿����� �ʰ� �������α׷� ��ġ ���� ID�� MsiExec�� ���� SLIENT ���� ����
					WCHAR slientUninstallPath[1024] = L"MsiExec.exe /x";
					wcscat_s(slientUninstallPath, sAppKeyName);
					wcscat_s(slientUninstallPath, L" /quiet");

					proc_ret = CreateProcess(NULL, slientUninstallPath, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

					std::thread bat_block_thread(&ExitUninstallAfterWeb);
					WaitForInputIdle(proc_info.hProcess, INFINITE);

					// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
					Sleep(check_inteval);

					usingThread = false;
					bat_block_thread.join();
					printf("���� �Ϸ�");
				}
				else {
					// SLIENT ���ν��� ���� ��
					if (sQuietUninstallString[0] != '\0')
					{
						proc_ret = CreateProcess(NULL, sQuietUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

						std::thread bat_block_thread(&ExitUninstallAfterWeb);
						WaitForInputIdle(proc_info.hProcess, INFINITE);

						// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
						Sleep(check_inteval);

						usingThread = false;
						bat_block_thread.join();
						printf("���� �Ϸ�");
					}
					else if (sUninstallString[0] != '\0')
					{
						// MsiExec ��� �� SLIENT ���ν���� ����
						if (wcsstr(sUninstallString, L"MsiExec.exe"))
						{
							sUninstallString[13] = 'x';
							wcscat_s(sUninstallString, L" /quiet");

							proc_ret = CreateProcess(NULL, sUninstallString, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);
							
							std::thread bat_block_thread(&ExitUninstallAfterWeb);
							WaitForInputIdle(proc_info.hProcess, INFINITE);

							// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
							Sleep(check_inteval);

							usingThread = false;
							bat_block_thread.join();
							printf("���� �Ϸ�");
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
									TCHAR CLSNAME[1024];
									// ��ġ ���� Ÿ��Ʋ ��������
									GetWindowText(UninstallHWND, MainWindowText, 1024);
									GetClassName(UninstallHWND, CLSNAME, 1024);

									// Internet Explorer_Server ���� üũ
									HWND embWindow = FindWindowEx(UninstallHWND, NULL, L"Shell Embedding", NULL);

									if (embWindow != NULL)
									{
										HWND docWindow = FindWindowEx(embWindow, NULL, L"Shell DocObject View", NULL);
										HWND ieWindow = FindWindowEx(docWindow, NULL, L"Internet Explorer_Server", NULL);

										while (ieWindow == NULL)
										{
											ieWindow = FindWindowEx(docWindow, NULL, L"Internet Explorer_Server", NULL);
										}

										// WEB ��� ������ ���
										if (wcsstr(MainWindowText, L"McAfee  ���ȼ���"))
										{
											Sleep(5000);
											UninstallMcafee(ieWindow);
										}
									}

									// ���� ���μ��� ����
									if (sInstallLocation[0] != '\0')
										ExitRelationProcess(sInstallLocation);

									// Ÿ��Ʋ�� ������ ���� ã��
									while (true)
									{
										if (!unInstallFinished)
										{
											HWND unintstallSub = FindWindow(NULL, MainWindowText);

											// �̸��� ����Ǿ� ��ã�� ���
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
												// XAMPP �� Ư���� ���̽��� �ϵ��ڵ�
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
								// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
								Sleep(check_inteval);

								usingThread = false;
								bat_block_thread.join();
								printf("���� �Ϸ�");

							}
						}
					}
				}

				// ���� ���μ��� �Ϸ� �� �ͽ��÷η� üũ
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

	TCHAR lstFindText[13][10] = { _T("Next"), _T("����"), _T("����"), _T("Uninstall"), _T("Remove"), _T("��"), _T("��(Y)") , _T("����"), _T("��ħ"), _T("Yes"), _T("Close"), _T("Finish"), _T("������") };
	TCHAR lstFinishText[4][10] = { _T("Finish"), _T("�ݱ�"), _T("���ŵǾ����ϴ�"), _T("��ħ") };
	TCHAR lstRebootText[2][10] = { _T("Reboot"), _T("Restart") };
	TCHAR lstRebootFindText[5][10] = { _T("No"), _T("Later"), _T("later"), _T("����") , _T("����") };
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
				// Caption ���� ã�����ϴ� QWidget ��ư�� ��� key press �� ����
				// SetActiveWindow(GetActiveWindow());

				// YES ��ư
				PostMessage(hwnd, WM_KEYDOWN, 'y', 1);
				PostMessage(hwnd, WM_KEYUP, 'y', 1);

				// ���� ��ư
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
				// Caption ���� ã�����ϴ� QWidget ��ư�� ��� key press �� ����
				// SetActiveWindow(GetActiveWindow());

				// YES ��ư
				PostMessage(hwnd, WM_KEYDOWN, 'n', 1);
				PostMessage(hwnd, WM_KEYUP, 'n', 1);

				// Later ��ư
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
			// ���μ����� ���ϸ��ϰ� ��Ī �Ǹ�
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
			// ���ͳ� ���μ����� ���
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
	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// TAB ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 1);
	PostMessage(hwnd, WM_KEYUP, VK_TAB, 1);

	// SPACE ��ư
	PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 1);
	PostMessage(hwnd, WM_KEYUP, VK_SPACE, 1);

	// ������ ��ȯ ������
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
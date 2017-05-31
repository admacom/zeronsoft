#include <Windows.h>
#include <stdio.h>
#include <string>
#include <locale.h>
#include <atlstr.h>
#include <tlhelp32.h>
#include <list>
#include <thread>
#include <Shlwapi.h>
#include <algorithm>
#include <Math.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <fstream>

#pragma warning(disable: 4996)
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Advapi32.lib")

// 32bit, 64bit Ȯ��
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL fnIsWor64();

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

// Popup Window Get
BOOL CALLBACK PopWorkerProc(HWND hwnd, LPARAM lParam);

// ��ġ ���� �÷���
bool unInstallFinished = false;

// ��ġ ����Ʈ �÷���
bool unInstallReboot = false;

// ��ġ ���� ���� ������Ʈ ����
bool usingThread = true;
int check_inteval = 2000;
void ExitUninstallAfterWeb();
BOOL AddToACL(PACL& pACL, WCHAR* AccountName, DWORD AccessOption);
BOOL ChangeFileSecurity(WCHAR* path);
void SetBlockInternetFromRegistry();
void RealaseBlockInternetFromRegistry();
void CommandExecute(WCHAR* programname, WCHAR* parameter);

// ��ġ ���� ������ �ؽ�Ʈ
TCHAR MainWindowText[1024];
HWND SubWindowHWND;
HWND UninstallHWND;
HWND UninstallPopupHWND;

// MCAFEE HARDCODING
void UninstallMcafee(HWND hwnd);

// ��ġ ���μ���
bool IsFileExecuteUninstall(WCHAR* execPath, WCHAR* existspath);
bool IsNormalUninstall(WCHAR* execPath, WCHAR* existspath);


// ��ġ�� ���α׷� ������ ��������
int TransverseDirectory(CString path);

// ��ġ ���� ����ü
struct InstallInfo
{
	char* InstallName;
	char* InstallPublisher;
	char* InstallDate;
	double InstallSize;
};

// ��ġ�� ��� ��������
std::list<InstallInfo> GetWindowsInstalledProgramList();

std::ofstream logFile_hwnd("c:\\file_hwnd_log.txt");

// Console ������ �޼��� �ޱ�
void RegisterMessageReceiverWindow();
LRESULT CALLBACK WndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

typedef struct DataInfo {
	WCHAR sData[512] = L"���ν�Ʈ��Ʈ, 11���� �������� ������";
}DINFO, *PINFO;

int main()
{
	RegisterMessageReceiverWindow();
	//GetWindowsInstalledProgramList();
	
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

	SetBlockInternetFromRegistry();

	if (fnIsWor64())
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

			if (fnIsWor64())
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

				wprintf(L"������Ʈ�� ���: %s\\%s\n", regKeyPath, sAppKeyName);

				if (sUninstallString[0] != '\0')
					wprintf(L"���ν��� ��ũ: %s\n", sUninstallString);

				if (sQuietUninstallString[0] != '\0')
					wprintf(L"��׶��� ���ν��� ��ũ: %s\n", sQuietUninstallString);
				wprintf(L"=====================================\n");

				wprintf(L"���� ����\n");

				if (sAppKeyName[0] == '{' && sQuietUninstallString[0] == '\0')
				{
					// �������α׷� �ڵ尪���� ��ġ �Ǿ��� ��
					// ������Ʈ�� ���� �̿����� �ʰ� �������α׷� ��ġ ���� ID�� MsiExec�� ���� SLIENT ���� ����
					WCHAR slientUninstallPath[1024] = L"MsiExec.exe /x";
					wcscat_s(slientUninstallPath, sAppKeyName);
					wcscat_s(slientUninstallPath, L" /quiet");
					
					if (IsFileExecuteUninstall(slientUninstallPath, sUninstallString))
						printf("���� �Ϸ�");
					else
					{
						// ���� ���� ���� �� �Ϲ� ������ ����
						printf("���� ���� [�Ϲ� ���ŷ� ����]");
						IsNormalUninstall(slientUninstallPath, sUninstallString);
					}
				}
				else {
					// SLIENT ���ν��� ���� ��
					if (sQuietUninstallString[0] != '\0')
					{
						if (IsFileExecuteUninstall(sQuietUninstallString, sUninstallString))
							printf("���� �Ϸ�");
						else
							printf("���� ����");
					}
					else if (sUninstallString[0] != '\0')
					{
						// MsiExec ��� �� SLIENT ���ν���� ����
						if (wcsstr(sUninstallString, L"MsiExec.exe"))
						{
							sUninstallString[13] = 'x';
							wcscat_s(sUninstallString, L" /quiet");

							if (IsFileExecuteUninstall(sUninstallString, sUninstallString))
								printf("���� �Ϸ�");
							else
								printf("���� ����");
						}
						else
						{
							if (IsNormalUninstall(sUninstallString, sUninstallString))
								printf("���� �Ϸ�");
							else
								printf("���� ����");
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
	RealaseBlockInternetFromRegistry();
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
	TCHAR getPidClass[512];
	HWND tempHwnd = FindWindow(NULL, NULL);  

	while (tempHwnd != NULL)
	{
		if (GetParent(tempHwnd) == NULL) 
			if (pid == ProcIDFromWnd(tempHwnd))
			{
				// TeamView Ÿ��Ʋ�� ��ġ���ϰ� ������ PID �� ������ ��찡 �־ ���� ó��
				// ���� TeamViewer ���� ��Ŀ��� �ش� Ÿ��Ʋ�ٹ�ưŬ������ ��� �ȵ�
				GetClassName(tempHwnd, getPidClass, 512);
				if (!wcsstr(getPidClass, L"TeamViewer_TitleBarButtonClass") && IsWindowVisible(tempHwnd))
				{
					return tempHwnd;
				}
			}
		tempHwnd = GetWindow(tempHwnd, GW_HWNDNEXT); 
	}
	return NULL;
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {

	TCHAR lstFindText[26][15] = { _T("Next"), _T("����"), _T("����"), _T("Uninstall"), _T("Remove"), _T("��"), _T("��(Y)") , _T("����"), _T("��ħ"), _T("Yes"), _T("Close"), _T("Finish"), _T("������"), _T("Ȯ��"), _T("��(&Y)"), _T("�Ϸ�"), _T("����(&I)"), _T("OK")
								, _T("&Close"), _T("&Uninstall"), _T("Ok"), _T("�ٽ� �õ�(&R)"), _T("&Next >"), _T("Yes to All"), _T("��ħ(&F)"), _T("��(&Y)") };
	TCHAR lstFinishText[6][10] = { _T("Finish"), _T("�ݱ�"), _T("���ŵǾ����ϴ�"), _T("�����Ͼ����ϴ�"), _T("��ħ"), _T("�Ϸ�") };
	TCHAR lstRebootText[3][10] = { _T("Reboot"), _T("Restart"), _T("����") };
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
	CString findWindowText[] = { "����", "���ν���", "Uninstall", "Remove" , "Delete", "Uninstallation", "Setup" };

	for (int i = 0; i < 7; i++)
	{
		if (distText.Find(findWindowText[i]) > -1 && IsWindowVisible(hwnd) && distText.MakeLower().Find(L"program uninstallation") == -1)
		{
			SubWindowHWND = hwnd;
			break;
		}
	}

	return TRUE;
}

BOOL CALLBACK PopWorkerProc(HWND hwnd, LPARAM lParam) {
	static TCHAR buffer[1024];
	GetWindowText(hwnd, buffer, 1024);

	CString distText = (LPCTSTR)buffer;
	CString sourceText = (LPCTSTR)MainWindowText;
	CString findWindowText[] = { "����", "���ν���", "Uninstall", "Remove" , "Delete", "Uninstallation", "Setup" };

	if (wcscmp(buffer, MainWindowText) != 0)
	{
		for (int i = 0; i < 7; i++)
		{
			if (distText.Find(findWindowText[i]) > -1 && distText.MakeLower().Find(L"program uninstallation") == -1)
			{
				UninstallPopupHWND = hwnd;
				break;
			}
		}
	}
	else
	{
		if (UninstallHWND != hwnd)
			UninstallPopupHWND = hwnd;
	}

	return TRUE;
}

BOOL CALLBACK TerminateProc(HWND hwnd, LPARAM lParam) {
	static TCHAR buffer[1024];
	GetWindowText(hwnd, buffer, 1024);

	CString distText = (LPCTSTR)buffer;
	CString sourceText = (LPCTSTR)MainWindowText;
	CString findWindowText[] = { "����", "���ν���", "Uninstall", "Remove" , "Delete", "Uninstallation", "Setup" };

	for (int i = 0; i < 7; i++)
	{
		if (distText.Find(L"http://") > -1)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		}
	}

	return TRUE;
}

void ExitRelationProcess(TCHAR* folderPath)
{
	// System32 �� ����
	if (wcsstr(folderPath, L"system32"))
		return;

	PathAddBackslash(folderPath);
	PathAddExtension(folderPath, L"*.exe");

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 ProcessEntry32;
	ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);
	
	WIN32_FIND_DATA find_data;
	HANDLE file_handle;

	TCHAR* real_path = _tcstok(folderPath, _T("\""));
	file_handle = FindFirstFile(real_path, &find_data);

	if (file_handle == INVALID_HANDLE_VALUE)
		return;

	do {
		CString process_file;
		process_file = (LPCTSTR)find_data.cFileName;

		BOOL bProcessFound = Process32First(hSnapshot, &ProcessEntry32);
		while (bProcessFound)
		{
			// ���μ����� ���ϸ��ϰ� ��Ī �Ǹ�
			if (process_file.Find(L"uninst") != 0)
			{
				if (process_file.Compare((LPCTSTR)ProcessEntry32.szExeFile) == 0)
				{
					HANDLE open_handle = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessEntry32.th32ProcessID);
					TerminateProcess(open_handle, ((UINT)-1));
				}
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

		EnumWindows(TerminateProc, 0);
	}
}

BOOL fnIsWor64()
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

	// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
	Sleep(check_inteval);

	usingThread = false;
	bat_block_thread.join();

	WCHAR real_path[1024] = L"";
	for (int i = wcslen(existspath); i > 0; i--)
	{
		if ((char)(*(existspath + i)) == ' ')
		{
			wcsncpy(real_path, existspath, i);
			break;
		}
	}

	if (wcslen(real_path) < 12)
	{
		if (!PathFileExists(existspath))
			return true;
		else
			return false;
	}
	else
	{
		if (!PathFileExists(real_path))
			return true;
		else
			return false;
	}
}

bool IsNormalUninstall(WCHAR* execPath, WCHAR* existspath)
{
	WCHAR *clone_path = new WCHAR[wcslen(existspath) + 1];
	wcscpy(clone_path, existspath);

	// ���� ���μ��� ����
	if (existspath[0] != '\0')
	{
		PathRemoveFileSpec(existspath);
		ExitRelationProcess(existspath);
	}

	STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
	startup_info.dwFlags = STARTF_USESHOWWINDOW;
	startup_info.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION proc_info;
	BOOL proc_ret;

	proc_ret = CreateProcess(NULL, clone_path, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &proc_info);

	if (proc_ret)
	{
		BOOL usingProcess = false;

		// PID ������ ã��
		WaitForInputIdle(proc_info.hProcess, INFINITE);
		std::thread bat_block_thread(&ExitUninstallAfterWeb);

		Sleep(2000);

		int execute_count = 0;

		while (UninstallHWND == NULL)
		{
			UninstallHWND = GetWinHandle(proc_info.dwProcessId);
			Sleep(200);

			// 5ȸ ��õ�
			if (execute_count > 5)
				break;
			else execute_count++;
		}

		// ��ã���� ���μ����� ã��
		if (UninstallHWND == NULL)
		{
			PathStripPath(clone_path);

			if (clone_path[wcslen(clone_path) - 1] == '\"')
			{
				clone_path[wcslen(clone_path) - 1] = '\0';
			}

			CString lst_proc_name[] = { "u_.exe", "_A.exe" };
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

			PROCESSENTRY32 ProcessEntry32;
			ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);

			Process32First(hSnapshot, &ProcessEntry32);

			CString execute_file_name = (LPCTSTR)clone_path;
			while (Process32Next(hSnapshot, &ProcessEntry32) == TRUE)
			{
				CString curr_proc_name = (LPCTSTR)ProcessEntry32.szExeFile;

				// �ڱ��ڽ� ���μ��� üũ
				if (execute_file_name.Find(curr_proc_name) > -1)
				{
					usingProcess = true;
					UninstallHWND = GetWinHandle(ProcessEntry32.th32ProcessID);
				}

				// �Ҵ�(����) �� ���μ��� üũ
				if (!usingProcess)
				{
					for (int i = 0; i < 2; i++)
					{
						if (curr_proc_name.Find(lst_proc_name[i]) > -1)
						{
							usingProcess = true;
							UninstallHWND = GetWinHandle(ProcessEntry32.th32ProcessID);
							break;
						}
					}
				}
				if (usingProcess)
					break;
			}
		}

		// ���μ����ε� ã�� ���ϸ� Ÿ��Ʋ ������ ã��
		if (UninstallHWND == NULL)
		{
			SubWindowHWND = NULL;
			EnumWindows(WorkerProc, 0);
			UninstallHWND = SubWindowHWND;
		}

		if (UninstallHWND != NULL)
		{
			// ���μ��� �ڵ��� ������ �ֻ������ �̵�
			BringWindowToTop(UninstallHWND);
			SetWindowPos(UninstallHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			SetForegroundWindow(UninstallHWND);

			TCHAR CLSNAME[1024];

			// ��ġ ���� Ÿ��Ʋ ��������
			GetWindowText(UninstallHWND, MainWindowText, 1024);
			GetClassName(UninstallHWND, CLSNAME, 1024);

			// ���� ��쿡 �ֻ�� â �ٽ� �������� (���÷��̾�)
			if (MainWindowText[0] == _T('\0'))
			{
				SubWindowHWND = NULL;
				EnumWindows(WorkerProc, 0);

				GetWindowText(SubWindowHWND, MainWindowText, 1024);
				GetClassName(SubWindowHWND, CLSNAME, 1024);
			}

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
					Sleep(8000);
					UninstallMcafee(ieWindow);
				}
			}

			while (true)
			{
				if (!unInstallFinished || (IsWindow(UninstallHWND) && IsWindowVisible(UninstallHWND)) || (IsWindow(UninstallPopupHWND) && IsWindowVisible(UninstallPopupHWND)))
				{
					BringWindowToTop(UninstallHWND);
					SetWindowPos(UninstallHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
					SetForegroundWindow(UninstallHWND);

					HWND TitleHard = FindWindow(NULL, L"Info");

					if (TitleHard != NULL)
						EnumChildWindows(TitleHard, EnumChildProc, 0);
					else
					{
						// �˾� üũ
						HWND fore_popup_hwnd = GetForegroundWindow();

						UninstallPopupHWND = NULL;
						EnumWindows(PopWorkerProc, 0);

						if (fore_popup_hwnd != UninstallHWND)
							EnumChildWindows(fore_popup_hwnd, EnumChildProc, 0);
						
						if (UninstallPopupHWND != NULL)
							EnumChildWindows(UninstallPopupHWND, EnumChildProc, 0);
						else
							EnumChildWindows(UninstallHWND, EnumChildProc, 0);
					}
					Sleep(1000);
				}
				else
				{
					// ���μ����� üũ�� ��� �ش� ���μ����� ����ִ��� �ڵ�Ӹ��ƴ϶� ���μ��������� �ѹ� �� üũ
					if (usingProcess)
					{
						usingProcess = false;

						CString lst_proc_name[] = { "u_.exe", "_A.exe" };
						HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

						PROCESSENTRY32 ProcessEntry32;
						ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);

						Process32First(hSnapshot, &ProcessEntry32);

						CString execute_file_name = (LPCTSTR)clone_path;
						while (Process32Next(hSnapshot, &ProcessEntry32) == TRUE)
						{
							CString curr_proc_name = (LPCTSTR)ProcessEntry32.szExeFile;

							// �ڱ��ڽ� ���μ��� üũ
							if (execute_file_name.Find(curr_proc_name) > -1)
							{
								usingProcess = true;
								UninstallHWND = GetWinHandle(ProcessEntry32.th32ProcessID);
							}

							// �Ҵ�(����) �� ���μ��� üũ
							if (!usingProcess)
							{
								for (int i = 0; i < 2; i++)
								{
									if (curr_proc_name.Find(lst_proc_name[i]) > -1)
									{
										usingProcess = true;
										UninstallHWND = GetWinHandle(ProcessEntry32.th32ProcessID);
										break;
									}
								}
							}
							if (usingProcess)
								break;
						}
					}
					else
						break;
				}
			}
		}
		// 1.5�ʰ� ���ͳ� ���� ��ɾ� üũ
		Sleep(check_inteval);

		usingThread = false;
		bat_block_thread.join();

		/* ��ġ �ڵ鷯 �ʱ�ȭ */
		UninstallHWND = NULL;
		UninstallPopupHWND = NULL;

		if (!PathFileExists(existspath))
			return true;
		else
			return false;
	}
}

std::list<InstallInfo> GetWindowsInstalledProgramList()
{
	std::list<InstallInfo> lstCollection;
	WCHAR* lstregKeyPath[2] = { L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" };

	for (int i = 0; i <2; i++)
	{
		HKEY hUninstKey = NULL;
		HKEY hAppKey = NULL;
		WCHAR sAppKeyName[1024] = { '\0', };
		WCHAR sSubKey[1024] = { '\0', };
		WCHAR sDisplayName[1024] = { '\0', };
		WCHAR sPublisher[1024] = { '\0', };
		WCHAR sInstallDate[1024] = { '\0', };
		WCHAR sInstallLocation[1024] = { '\0', };
		long lResult = ERROR_SUCCESS;
		DWORD dwType = KEY_ALL_ACCESS;
		DWORD dwBufferSize = 0;
		DWORD dwDisplayBufSize = 0;
		DWORD dwPublisherBufSize = 0;
		DWORD dwInstallDateBufSize = 0;
		DWORD dwInstallLocationBufSize = 0;
		bool regOpenResult = false;

		if (fnIsWor64())
			regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lstregKeyPath[i], 0, KEY_READ | KEY_WOW64_64KEY, &hUninstKey) != ERROR_SUCCESS;
		else
			regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lstregKeyPath[i], 0, KEY_READ, &hUninstKey) != ERROR_SUCCESS;

		if (regOpenResult)
		{
			RegCloseKey(hUninstKey);
			return lstCollection;
		}

		for (DWORD dwIndex = 0; lResult == ERROR_SUCCESS; dwIndex++)
		{
			dwBufferSize = sizeof(sAppKeyName);
			if ((lResult = RegEnumKeyEx(hUninstKey, dwIndex, sAppKeyName,
				&dwBufferSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
			{
				wsprintf(sSubKey, L"%s\\%s", lstregKeyPath[i], sAppKeyName);

				if (fnIsWor64())
					regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hAppKey) != ERROR_SUCCESS;
				else
					regOpenResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ, &hAppKey) != ERROR_SUCCESS;

				if (regOpenResult)
				{
					RegCloseKey(hAppKey);
					RegCloseKey(hUninstKey);
					return lstCollection;
				}

				dwDisplayBufSize = sizeof(sDisplayName);
				dwPublisherBufSize = sizeof(sPublisher);
				dwInstallDateBufSize = sizeof(sInstallDate);
				dwInstallLocationBufSize = sizeof(sInstallLocation);
				
				wsprintf(sDisplayName, L"");
				wsprintf(sPublisher, L"");
				wsprintf(sInstallDate, L"");
				wsprintf(sInstallLocation, L"");
				
				RegQueryValueEx(hAppKey, L"DisplayName", NULL, &dwType, (unsigned char*)sDisplayName, &dwDisplayBufSize);
				RegQueryValueEx(hAppKey, L"Publisher", NULL, &dwType, (unsigned char*)sPublisher, &dwPublisherBufSize);
				RegQueryValueEx(hAppKey, L"InstallDate", NULL, &dwType, (unsigned char*)sInstallDate, &dwInstallDateBufSize);
				RegQueryValueEx(hAppKey, L"InstallLocation", NULL, &dwType, (unsigned char*)sInstallLocation, &dwInstallLocationBufSize);
				
				// �ߺ� ó��
				USES_CONVERSION;
				if (sDisplayName[0] != '\0')
				{
					InstallInfo install_data = {};
					install_data.InstallName = W2A(sDisplayName);
					install_data.InstallPublisher = W2A(sPublisher);
					install_data.InstallDate = W2A(sInstallDate);
					
					if (sInstallLocation[0] != '\0')
					{
						int number = 2;
						int p = pow(10, number);
						double value = (double)((double)(TransverseDirectory(sInstallLocation) / 1024) / 1024);
						install_data.InstallSize = floor((value * p) + 0.5f) / p;
					}
					else
						install_data.InstallSize = 0;

					if (lstCollection.size() > 0)
					{
						bool find_install = false;
						char * conv_name = W2A(sDisplayName);

						for (std::list<InstallInfo>::iterator it = lstCollection.begin(); it != lstCollection.end(); ++it)
						{
							if (!strcmp(it->InstallName, install_data.InstallName))
							{
								find_install = true;
								break;
							}
						}
						if (!find_install)
							lstCollection.push_back(install_data);
					}
					else lstCollection.push_back(install_data);
				}
				RegCloseKey(hAppKey);
			}
		}

		RegCloseKey(hUninstKey);
	}

	std::list<InstallInfo>::iterator it = lstCollection.begin();

	while (it != lstCollection.end())
	{
		printf("%s -> %.2lfMB\n", it->InstallName, it->InstallSize);
		++it;
	}

	return lstCollection;
}

int TransverseDirectory(CString path)
{
	USES_CONVERSION;
	
	WIN32_FIND_DATA data;
	int size = 0;

	CString fname;

	if (path.Right(1) != _T("\\"))
		fname = path + "\\*.*";
	else
		fname = path + "*.*";

	HANDLE h = FindFirstFile(fname.GetBuffer(0), &data);
	if (h != INVALID_HANDLE_VALUE)
	{
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (strcmp(W2A(data.cFileName), ".") != 0 && strcmp(W2A(data.cFileName), "..") != 0)
				{
					fname = path + "\\" + data.cFileName;
					size += TransverseDirectory(fname);
				}
			}
			else
			{
				LARGE_INTEGER sz;
				sz.LowPart = data.nFileSizeLow;
				sz.HighPart = data.nFileSizeHigh;
				size += sz.QuadPart;
			}
		} while (FindNextFile(h, &data) != 0);
		FindClose(h);
	}
	return size;
}
void SetBlockInternetFromRegistry()
{
	HKEY hUninstKey = NULL;
	HKEY hAppKey = NULL;
	WCHAR sAppKeyName[1024] = { '\0', };
	WCHAR sSubKey[1024] = { '\0', };
	WCHAR sDefaultPath[1024] = { '\0', };
	long lResult = ERROR_SUCCESS;
	DWORD dwType = KEY_ALL_ACCESS;
	DWORD dwBufferSize = 0;
	DWORD dwDefaultPath = 0;
	bool regOpenResult = false;

	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Clients\\StartMenuInternet", 0, KEY_READ, &hUninstKey);

	for (DWORD dwIndex = 0; lResult == ERROR_SUCCESS; dwIndex++)
	{
		dwBufferSize = sizeof(sAppKeyName);
		if ((lResult = RegEnumKeyEx(hUninstKey, dwIndex, sAppKeyName, &dwBufferSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
		{
			wsprintf(sSubKey, L"%s\\%s\\shell\\open\\command", L"SOFTWARE\\WOW6432Node\\Clients\\StartMenuInternet", sAppKeyName);
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ, &hAppKey);

			if (regOpenResult)
			{
				RegCloseKey(hAppKey);
				RegCloseKey(hUninstKey);
				return;
			}

			dwDefaultPath = sizeof(sDefaultPath);

			RegQueryValueEx(hAppKey, NULL, NULL, &dwType, (unsigned char*)sDefaultPath, &dwDefaultPath);
			CString csDefaultPath = (LPCTSTR)sDefaultPath;

			csDefaultPath.Replace(L"\"", L"");

			if (csDefaultPath.Find(L"iexplore.exe") > -1)
			{
				// Everyone ���� �߰�
				WCHAR ALL_PATH[1024] = { '\0', };
				wsprintf(ALL_PATH, L"/f \"%s\"", sDefaultPath);
				CommandExecute(L"takeown", ALL_PATH);
				ChangeFileSecurity(sDefaultPath);
				_wchmod(sDefaultPath, 777);

				MoveFile(csDefaultPath, csDefaultPath + L"2");
			}
			else
			{
				// ũ��
				MoveFile(csDefaultPath, csDefaultPath + L"2");
			}
			RegCloseKey(hAppKey);
		}
	}
	RegCloseKey(hUninstKey);

	WCHAR *EDGE_PATH = L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe";
	if (PathFileExists(EDGE_PATH))
	{
		WCHAR ALL_PATH[1024] = { '\0', };
		wsprintf(ALL_PATH, L"/f \"%s\"", EDGE_PATH);
		CommandExecute(L"takeown", ALL_PATH);
		ChangeFileSecurity(EDGE_PATH);
		_wchmod(EDGE_PATH, 777);

		MoveFile(L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe", L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe2");
	}
}

void RealaseBlockInternetFromRegistry()
{
	HKEY hUninstKey = NULL;
	HKEY hAppKey = NULL;
	WCHAR sAppKeyName[1024] = { '\0', };
	WCHAR sSubKey[1024] = { '\0', };
	WCHAR sDefaultPath[1024] = { '\0', };
	long lResult = ERROR_SUCCESS;
	DWORD dwType = KEY_ALL_ACCESS;
	DWORD dwBufferSize = 0;
	DWORD dwDefaultPath = 0;
	bool regOpenResult = false;

	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Clients\\StartMenuInternet", 0, KEY_READ, &hUninstKey);

	for (DWORD dwIndex = 0; lResult == ERROR_SUCCESS; dwIndex++)
	{
		dwBufferSize = sizeof(sAppKeyName);
		if ((lResult = RegEnumKeyEx(hUninstKey, dwIndex, sAppKeyName, &dwBufferSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
		{
			wsprintf(sSubKey, L"%s\\%s\\shell\\open\\command", L"SOFTWARE\\WOW6432Node\\Clients\\StartMenuInternet", sAppKeyName);
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubKey, 0, KEY_READ, &hAppKey);

			if (regOpenResult)
			{
				RegCloseKey(hAppKey);
				RegCloseKey(hUninstKey);
				return;
			}

			dwDefaultPath = sizeof(sDefaultPath);

			RegQueryValueEx(hAppKey, NULL, NULL, &dwType, (unsigned char*)sDefaultPath, &dwDefaultPath);
			CString csDefaultPath = (LPCTSTR)sDefaultPath;

			csDefaultPath.Replace(L"\"", L"");

			if (csDefaultPath.Find(L"iexplore.exe") > -1)
			{
				// Everyone ���� �߰�
				WCHAR ALL_PATH[1024] = { '\0', };
				wsprintf(ALL_PATH, L"/f \"%s\"", sDefaultPath);
				CommandExecute(L"takeown", ALL_PATH);
				ChangeFileSecurity(wcscat(sDefaultPath, L"2"));
				_wchmod(sDefaultPath, 777);

				MoveFile(csDefaultPath + L"2", csDefaultPath);
			}
			else
			{
				// ũ��
				MoveFile(csDefaultPath + L"2", csDefaultPath);
			}
			RegCloseKey(hAppKey);
		}
	}
	RegCloseKey(hUninstKey);

	WCHAR *EDGE_PATH = L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe2";
	if (PathFileExists(EDGE_PATH))
	{
		WCHAR ALL_PATH[1024] = { '\0', };
		wsprintf(ALL_PATH, L"/f \"%s\"", EDGE_PATH);
		CommandExecute(L"takeown", ALL_PATH);

		ChangeFileSecurity(EDGE_PATH);
		_wchmod(EDGE_PATH, 777);

		MoveFile(L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe2", L"C:\\Windows\\SystemApps\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe\\MicrosoftEdge.exe");
	}
}

BOOL AddToACL(PACL& pACL, WCHAR* AccountName, DWORD AccessOption)
{
	WCHAR  sid[MAX_PATH] = { 0 };
	DWORD sidlen = MAX_PATH;
	WCHAR  dn[MAX_PATH] = { 0 };
	DWORD dnlen = MAX_PATH;
	SID_NAME_USE SNU;
	PACL temp = NULL;

	if (!LookupAccountName(NULL, AccountName, (PSID)sid, &sidlen, dn, &dnlen, &SNU))
		return FALSE;

	EXPLICIT_ACCESS ea = { 0 };
	ea.grfAccessPermissions = AccessOption;
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName = (LPTSTR)(PSID)sid;

	if (SetEntriesInAcl(1, &ea, pACL, &temp) != ERROR_SUCCESS)
		return FALSE;

	LocalFree(pACL);
	pACL = temp;

	return TRUE;
}

BOOL ChangeFileSecurity(WCHAR* path)
{
	BYTE SDBuffer[4096] = { 0 };
	DWORD SDLength = 4096, RC;
	SECURITY_DESCRIPTOR* SD = (SECURITY_DESCRIPTOR*)SDBuffer;

	if (!InitializeSecurityDescriptor(SD, SECURITY_DESCRIPTOR_REVISION))
		return FALSE;

	PACL pACL = (PACL)LocalAlloc(LMEM_FIXED, sizeof(ACL));

	if (!InitializeAcl(pACL, MAX_PATH, ACL_REVISION))
	{
		LocalFree(pACL);
		return FALSE;
	}

	DWORD AccessOption = GENERIC_ALL | GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;

	AddToACL(pACL, L"Everyone", AccessOption);

	if (!SetSecurityDescriptorDacl(SD, TRUE, pACL, TRUE))
	{
		LocalFree(pACL);
		return FALSE;
	}

	RC = SetFileSecurity(path, DACL_SECURITY_INFORMATION, SD);
	LocalFree(pACL);
	return RC;
}
void CommandExecute(WCHAR* programname, WCHAR* parameter)
{
	PVOID prevContext = NULL;
	Wow64DisableWow64FsRedirection(&prevContext);

	SHELLEXECUTEINFO startup_info;
	ZeroMemory(&startup_info, sizeof(SHELLEXECUTEINFO));
	startup_info.cbSize = sizeof(SHELLEXECUTEINFO);
	startup_info.lpFile = programname;
	startup_info.lpParameters = parameter;
	startup_info.nShow = SW_HIDE;
	startup_info.lpVerb = L"runas";
	startup_info.fMask = SEE_MASK_NOCLOSEPROCESS;

	int result2 = ShellExecuteEx(&startup_info);
	Wow64RevertWow64FsRedirection(prevContext);
}

void RegisterMessageReceiverWindow()
{
	WNDCLASSEX wc;
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra = 0;
	wc.cbClsExtra = 0;
	wc.hInstance = ::GetModuleHandle(NULL);//Get this from DLLMain 
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = _T("MSGWND");
	wc.hIcon = NULL;
	wc.hIconSm = NULL;
	wc.hCursor = NULL;
	ATOM atom = RegisterClassEx(&wc);

	CreateWindowEx(NULL, _T("MSGWND"), NULL, NULL, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
			TranslateMessage(&msg);

			DispatchMessage(&msg);
	}

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch (nMsg)
	{
	case WM_COPYDATA:
		COPYDATASTRUCT *pCopyData;
		pCopyData = (COPYDATASTRUCT*)lParam;

		PINFO cpInfo = (PINFO)pCopyData->lpData;

		GetInstalledProgram(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", cpInfo->sData);
		GetInstalledProgram(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", cpInfo->sData);
		break;
	}

	return DefWindowProc(hWnd, nMsg, wParam, lParam);
}
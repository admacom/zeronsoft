#include <wuapi.h>
#include <wuerror.h>
#include <iostream>
#include <fstream>
#include <atlconv.h>
#include <locale.h>
#include <list>

using namespace std;

ofstream logFile("c:\\log.txt");

HRESULT hr;
IUpdateSession* iUpdateSession;

void SearchUpdates();
void ExecuteUpdates(char* InstallName, char* gbn);
void InstallUpdates(IUpdateCollection* updateFilterList);
void DeleteUpdates(IUpdateCollection* updateFilterList);
void SetWSUSServerConn();
void SearchProgramHotfix();

// 32bit, 64bit 확인
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process_uninst;
BOOL IsWow64_uninst();
bool GetInstalledProgram_uninst(WCHAR *regKeyPath);

int main(int argc, char** argv)
{
	// WSUS 서버 설정
	SetWSUSServerConn();

	hr = CoInitialize(NULL); 
	hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&iUpdateSession);
	
	
		argv[1] = "S";
		argv[2] = "Windows 10 Version 1607 x64 기반 시스템용 Adobe Flash Player 보안 업데이트(KB4018483)";
	

	if (strcmp(argv[1], "") == 0) return 0;

	if (strcmp(argv[1], "S") == 0)
	{
		SearchProgramHotfix();
		SearchUpdates();
	}
	else
		ExecuteUpdates(argv[2], argv[1]);
	
	::CoUninitialize();

	system("pause");
}

void SearchUpdates()
{
	std::locale::global(std::locale("C"));

	IUpdateSearcher* searcher;
	ISearchResult* resultsInstall;
	ISearchResult* resultsUninstall;
	BSTR criteriaInstalled = SysAllocString(L"IsInstalled=1");
	BSTR criteriaUnInstalled = SysAllocString(L"IsInstalled=0");

	hr = iUpdateSession->CreateUpdateSearcher(&searcher);
	
	std::cout << "업데이트를 찾고 있습니다 ..." << endl << flush;
	logFile << "업데이트를 찾고 있습니다 ..." << endl;
	
	searcher->put_Online(VARIANT_TRUE);
	hr = searcher->Search(criteriaInstalled, &resultsInstall);

	hr = searcher->Search(criteriaUnInstalled, &resultsUninstall);

	SysFreeString(criteriaInstalled);
	SysFreeString(criteriaUnInstalled);

	switch (hr) {
	case S_OK:
		std::cout << "[조회된 데이터]" << endl << flush;
		logFile << "[조회된 데이터]" << endl;
		break;
	case WU_E_LEGACYSERVER:
		std::cout << "[서버를 찾을수 없다]" << endl;
		logFile << "[서버를 찾을수 없다]" << endl;
		return;
	case WU_E_INVALID_CRITERIA:
		std::cout << "[잘못된 검색 기준입니다]" << endl;
		logFile << "[잘못된 검색 기준입니다]" << endl;
		return;
	}

	IUpdateCollection* updateList;
	IUpdate* updateItem;
	BSTR updateName = L"";
	LONG updateSize;

	if (resultsInstall == NULL)
	{
		std::cout.flush();
		std::cout << "[WSUS 서버 접속 실패 (종료)]" << endl; 
		logFile << "[WSUS 서버 접속 실패 (종료)]" << endl;
		return;
	}
	/* Installed Items */
	resultsInstall->get_Updates(&updateList);
	updateList->get_Count(&updateSize);

	std::cout << "[설치된 업데이트 목록]" << endl;
	logFile << "[설치된 업데이트 목록]" << endl;
	for (LONG i = 0; i < updateSize; i++)
	{
		string pbstr;

		updateList->get_Item(i, &updateItem);
		updateItem->get_Title(&updateName);

		USES_CONVERSION;
		pbstr = OLE2A(updateName);
		
		std::cout << i << ", [" << pbstr.c_str() << "]" << endl;
		logFile << i << ", [" << pbstr.c_str() << "]" << endl;
	}

	/* UnInstalled Items */
	resultsUninstall->get_Updates(&updateList);
	updateList->get_Count(&updateSize);

	std::cout << "[미 설치된 업데이트 목록]" << endl;
	logFile << "[미 설치된 업데이트 목록]" << endl;
	for (LONG i = 0; i < updateSize; i++)
	{
		string pbstr;

		updateList->get_Item(i, &updateItem);
		updateItem->get_Title(&updateName);

		USES_CONVERSION;
		pbstr = OLE2A(updateName);
		
		std::cout << i << ", [" << pbstr.c_str() << "]" << endl;
		logFile << i << ", [" << pbstr.c_str() << "]" << endl;
	}

	logFile.close();
}

void ExecuteUpdates(char* InstallName, char* gbn)
{
	IUpdateSearcher* searcher;
	ISearchResult* results;
	BSTR criteria;

	if (strcmp(gbn, "U") == 0)
		criteria = SysAllocString(L"IsInstalled=0");
	else
		criteria = SysAllocString(L"IsInstalled=1");

	hr = iUpdateSession->CreateUpdateSearcher(&searcher);

	std::cout << "찾는 업데이트 명: " << InstallName << endl;
	logFile << "찾는 업데이트 명: " << InstallName << endl;

	std::cout << "업데이트를 찾고 있습니다 ..." << endl;
	logFile << "업데이트를 찾고 있습니다 ..." << endl;

	hr = searcher->Search(criteria, &results);
	SysFreeString(criteria);

	switch (hr) {
	case S_OK:
		break;
	case WU_E_LEGACYSERVER:
		std::cout << "서버를 찾을수 없다" << endl;
		return;
	case WU_E_INVALID_CRITERIA:
		std::cout << "잘못된 검색 기준입니다." << endl;
		return;
	}

	IUpdateCollection* updateList;
	IUpdateCollection* updateFilterList;
	IUpdate* updateItem;
	LONG updateSize;
	LONG InstallCount;

	USES_CONVERSION;
	BSTR search_update = SysAllocString(A2W(InstallName));

	results->get_Updates(&updateList);
	updateList->get_Count(&updateSize);
	updateList->Copy(&updateFilterList);

	if (updateSize == 0) {
		std::wcout << L"업데이트 할수 있는 목록이 없습니다." << endl;
		logFile << L"업데이트 할수 있는 목록이 없습니다." << endl;
	}

	updateFilterList->get_Count(&InstallCount);

	for (LONG i = 0; i < updateSize; i++) {
		LONG InstallInsertRet;
		BSTR updateName;
		updateFilterList->get_Item(i, &updateItem);
		updateItem->get_Title(&updateName);

		if (wcscmp(updateName, search_update) == 0)
		{
			updateFilterList->Clear();
			updateFilterList->Add(updateItem, &InstallInsertRet);
			break;
		}
		SysFreeString(updateName);
	}
	
	if (strcmp(gbn, "U") == 0)
		InstallUpdates(updateFilterList);
	else if (strcmp(gbn, "D") == 0)
		DeleteUpdates(updateFilterList);
	else
		return;
}
void InstallUpdates(IUpdateCollection* updateFilterList)
{
	LONG itemRet;
	BSTR InstallName;
	IUpdate* updateItem;
	IUpdateDownloader* updateDownloader;
	IDownloadResult* DownloaderRet;
	HRESULT hr_download = CoInitialize(NULL);
	VARIANT_BOOL isDownload;
	LONG updateSize;

	updateFilterList->get_Count(&updateSize);

	if (updateSize == 0)
	{
		std::cout << "[이미 설치 된 업데이트 입니다.]" << endl;
		return;
	}

	updateFilterList->get_Item(0, &updateItem);
	updateItem->get_Title(&InstallName);

	USES_CONVERSION;
	string pbstr = OLE2A(InstallName);

	std::cout << "[윈도우 업데이트 설치 진행] : [" << pbstr.c_str() << "]" << endl;
	updateItem->get_IsDownloaded(&isDownload);

	if (isDownload == VARIANT_FALSE)
	{
		iUpdateSession->CreateUpdateDownloader(&updateDownloader);
		updateDownloader->put_Updates(updateFilterList);

		hr_download = updateDownloader->Download(&DownloaderRet);

		switch (hr_download) {
		case S_OK:
			std::wcout << L"다운로드 성공" << endl;
			logFile << L"다운로드 성공" << endl;
			break;
		}
	}

	IUpdateInstaller* updateInstaller;
	IInstallationResult* InstallerRet;
	iUpdateSession->CreateUpdateInstaller(&updateInstaller);
	updateInstaller->put_Updates(updateFilterList);

	hr_download = updateInstaller->Install(&InstallerRet);

	switch (hr_download)
	{
		case S_OK:
			std::wcout << L"인스톨 성공" << endl;
			logFile << L"인스톨 성공" << endl;
			break;
		default:
			std::wcout << hr_download << endl;
			logFile << hr_download << endl;
			break;
	}

	SysFreeString(InstallName);
	std::wcout << L"업데이트 설치가 완료되었습니다." << endl;
}

void DeleteUpdates(IUpdateCollection* updateFilterList)
{
	BSTR unInstallName;
	VARIANT_BOOL UninstallAble;
	IUpdate* updateItem;
	IUpdateInstaller* updateUnInstaller;
	IInstallationResult* unInstallRet;
	IStringCollection* KBArticleID;
	HRESULT hr_uninstall = CoInitialize(NULL);
	LONG updateSize;

	updateFilterList->get_Count(&updateSize);

	if (updateSize == 0)
	{
		std::cout << "[이미 제거 된 업데이트 입니다.]" << endl;
		return;
	}

	updateFilterList->get_Item(0, &updateItem);
	updateItem->get_Title(&unInstallName);

	USES_CONVERSION;
	string pbstr = OLE2A(unInstallName);

	std::cout << "[윈도우 업데이트 제거 진행] : [" << pbstr.c_str() << "]" << endl;

	IUpdateInstaller* updateInstaller;
	IInstallationResult* InstallerRet;
	iUpdateSession->CreateUpdateInstaller(&updateInstaller);
	updateInstaller->put_Updates(updateFilterList);

	hr_uninstall = updateInstaller->Uninstall(&InstallerRet);

	switch (hr_uninstall)
	{
		case S_OK:
			std::wcout << L"언인스톨 성공" << endl;
			logFile << L"언인스톨 성공" << endl;
			break;
		default:
			std::wcout << hr_uninstall << endl;
			logFile << hr_uninstall << endl;
			break;
	}

	std::wcout << L"업데이트 제거가 완료되었습니다." << endl;

	SysFreeString(unInstallName);
}

void SetWSUSServerConn()
{
	HKEY hKey = NULL;
	HKEY hKeyT = NULL;
	DWORD dwDesc;
	WCHAR* regKeyPath = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
	WCHAR regKeyPathAU[53] = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\A"; 	regKeyPathAU[51] = 'U';
	WCHAR* regKeyPathCurrent = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate";
	WCHAR* regBuffer = { '\0', };
	bool regOpenResult = false; 
	WCHAR* strWSUSAddr = L"http://zeronsoft.iptime.org:8530";

	// VALUE DWORD
	DWORD dwZero = 0;
	DWORD dwOne = 1;
	DWORD dwThree = 1;
	DWORD dwFive = 5;
	DWORD dwSixteen = 16;

	SC_HANDLE svcManager = NULL;
	SC_HANDLE svcMain = NULL;
	SERVICE_STATUS svcStatus {};

	setlocale(LC_ALL, "korean");

	svcManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	svcMain = OpenService(svcManager, L"wuauserv", SERVICE_ALL_ACCESS);
	QueryServiceStatus(svcMain, &svcStatus);

	ChangeServiceConfig(svcMain, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	// 종료
	if (svcStatus.dwCurrentState && svcStatus.dwCurrentState != SERVICE_STOPPED)
	{
		ControlService(svcMain, SERVICE_CONTROL_STOP, &svcStatus);
		while (svcStatus.dwCurrentState != SERVICE_STOPPED)
		{
			Sleep(500);
			QueryServiceStatus(svcMain, &svcStatus);
		}
	}

	RegCreateKeyEx(HKEY_LOCAL_MACHINE, regKeyPath, 0, regBuffer, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDesc);
	RegSetValueEx(hKey, L"WUServer", 0, REG_SZ, (LPBYTE)strWSUSAddr, wcslen(strWSUSAddr) * 2);
	RegSetValueEx(hKey, L"WUStatusServer", 0, REG_SZ, (LPBYTE)strWSUSAddr, wcslen(strWSUSAddr) * 2);
	RegSetValueEx(hKey, L"UpdateServiceUrlAlternate", 0, REG_SZ, (LPBYTE)strWSUSAddr, wcslen(strWSUSAddr) * 2);
	RegSetValueEx(hKey, L"TargetGroupEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"ElevateNonAdmins", 0, REG_DWORD, (const BYTE*)&dwZero, 4);

	RegCloseKey(hKey);

	RegCreateKeyEx(HKEY_LOCAL_MACHINE, regKeyPathAU, 0, regBuffer, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDesc);
	RegSetValueEx(hKey, L"AUOptions", 0, REG_DWORD, (const BYTE*)&dwFive, 4);
	RegSetValueEx(hKey, L"AutoInstallMinorUpdates", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"AutomaticMaintenanceEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"DetectionFrequency", 0, REG_DWORD, (const BYTE*)&dwSixteen, 4);
	RegSetValueEx(hKey, L"DetectionFrequencyEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"NoAutoRebootWithLoggedOnUsers", 0, REG_DWORD, (const BYTE*)&dwZero, 4);
	RegSetValueEx(hKey, L"NoAutoUpdate", 0, REG_DWORD, (const BYTE*)&dwZero, 4);
	RegSetValueEx(hKey, L"RebootRelaunchTimeout", 0, REG_DWORD, (const BYTE*)&dwFive, 4);
	RegSetValueEx(hKey, L"RebootRelaunchTimeoutEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"RebootWarningTimeout", 0, REG_DWORD, (const BYTE*)&dwFive, 4);
	RegSetValueEx(hKey, L"RebootWarningTimeoutEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"RescheduleWaitTime", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"RescheduleWaitTimeEnabled", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	RegSetValueEx(hKey, L"ScheduledInstallDay", 0, REG_DWORD, (const BYTE*)&dwZero, 4);
	RegSetValueEx(hKey, L"ScheduledInstallTime", 0, REG_DWORD, (const BYTE*)&dwThree, 4);
	RegSetValueEx(hKey, L"UseWUServer", 0, REG_DWORD, (const BYTE*)&dwOne, 4);
	
	RegCloseKey(hKey);

	if (IsWow64_uninst())
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKeyPathCurrent, 0, KEY_READ | KEY_WOW64_64KEY | KEY_SET_VALUE, &hKey);
	else
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKeyPathCurrent, 0, KEY_READ | KEY_SET_VALUE, &hKey);

	RegDeleteValue(hKey, L"AccountDomainSid");
	RegDeleteValue(hKey, L"PingID");
	RegDeleteValue(hKey, L"SusClientId");
	RegDeleteValue(hKey, L"SusClientIdValidation");
	
	RegCloseKey(hKey);

	// 시작
	if (!svcStatus.dwCurrentState || svcStatus.dwCurrentState == SERVICE_STOPPED)
	{
		StartService(svcMain, 0, NULL);
		while (svcStatus.dwCurrentState == SERVICE_STOPPED)
		{
			Sleep(500);
			QueryServiceStatus(svcMain, &svcStatus);
		}
	}

	CloseServiceHandle(svcManager);
	CloseServiceHandle(svcMain);

	PVOID prevContext = NULL;
	Wow64DisableWow64FsRedirection(&prevContext);

	SHELLEXECUTEINFO startup_info;
	ZeroMemory(&startup_info, sizeof(SHELLEXECUTEINFO));
	startup_info.cbSize = sizeof(SHELLEXECUTEINFO);
	startup_info.lpFile = L"wuauclt.exe";
	startup_info.lpParameters = L"/resetauthorization /detectnow";
	startup_info.nShow = SW_SHOWMINIMIZED; 
	startup_info.lpVerb = L"runas";
	startup_info.fMask = SEE_MASK_NOCLOSEPROCESS;

	int result2 = ShellExecuteEx(&startup_info);
	Wow64RevertWow64FsRedirection(prevContext);

	Sleep(1000);
}

void SearchProgramHotfix()
{
	bool regOpenResult = false;

	std::wcout << L"[프로그램 내 설치 HOTFIX]" << endl;

	GetInstalledProgram_uninst(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
	GetInstalledProgram_uninst(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

	std::wcout << L"[프로그램 내 설치 HOTFIX]" << endl;
}

BOOL IsWow64_uninst()
{
	BOOL bIsWow64 = FALSE;
	fnIsWow64Process_uninst = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process_uninst)
	{
		if (!fnIsWow64Process_uninst(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}


bool GetInstalledProgram_uninst(WCHAR *regKeyPath)
{
	HKEY hUninstKey = NULL;
	HKEY hAppKey = NULL;
	WCHAR sAppKeyName[1024] = { '\0', };
	WCHAR sSubKey[1024] = { '\0', };
	WCHAR sDisplayName[1024] = { '\0', };
	long lResult = ERROR_SUCCESS;
	DWORD dwType = KEY_ALL_ACCESS;
	DWORD dwBufferSize = 0;
	DWORD dwDisplayBufSize = 0;
	bool regOpenResult = false;
	int _count = 1;

	if (IsWow64_uninst())
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

			if (IsWow64_uninst())
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
			RegQueryValueEx(hAppKey, L"DisplayName", NULL, &dwType, (unsigned char*)sDisplayName, &dwDisplayBufSize);

			// SELECT TOKEN 
			if (wcsstr(sDisplayName, L"KB"))
			{
				wprintf(L"%d, %s\n", _count, sDisplayName);
				_count++;
			}

			RegCloseKey(hAppKey);
		}
	}

	RegCloseKey(hUninstKey);
	
	return true;
}
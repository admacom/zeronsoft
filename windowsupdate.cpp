#include <wuapi.h>
#include <wuerror.h>
#include <iostream>
#include <fstream>
#include <atlconv.h>

using namespace std;

ofstream logFile("c:\\log.txt");

HRESULT hr;
IUpdateSession* iUpdateSession;

void SearchUpdates();
void ExecuteUpdates(char* InstallName, char* gbn);
void InstallUpdates(IUpdateCollection* updateFilterList);
void DeleteUpdates(IUpdateCollection* updateFilterList);

int main(int argc, char** argv)
{
	hr = CoInitialize(NULL); 
	hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&iUpdateSession);

	/*
		argv[1] = "S";
		argv[2] = "Windows 10 Version 1607 x64 기반 시스템용 Adobe Flash Player 보안 업데이트(KB4018483)";
	*/

	if (strcmp(argv[1], "") == 0) return 0;

	if (strcmp(argv[1], "S") == 0)
		SearchUpdates();
	else
		ExecuteUpdates(argv[2], argv[1]);
	
	::CoUninitialize();

	system("pause");
}

void SearchUpdates()
{
	IUpdateSearcher* searcher;
	ISearchResult* resultsInstall;
	ISearchResult* resultsUninstall;
	BSTR criteriaInstalled = SysAllocString(L"IsInstalled=1");
	BSTR criteriaUnInstalled = SysAllocString(L"IsInstalled=0");

	hr = iUpdateSession->CreateUpdateSearcher(&searcher);
	
	std::cout << "업데이트를 찾고 있습니다 ..." << endl;
	logFile << "업데이트를 찾고 있습니다 ..." << endl;
	
	searcher->put_Online(VARIANT_TRUE);
	hr = searcher->Search(criteriaInstalled, &resultsInstall);
	hr = searcher->Search(criteriaUnInstalled, &resultsUninstall);

	SysFreeString(criteriaInstalled);
	SysFreeString(criteriaUnInstalled);

	switch (hr) {
	case S_OK:
		std::cout << "[조회된 데이터]" << endl;
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
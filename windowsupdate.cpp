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
		argv[2] = "Windows 10 Version 1607 x64 ��� �ý��ۿ� Adobe Flash Player ���� ������Ʈ(KB4018483)";
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
	
	std::cout << "������Ʈ�� ã�� �ֽ��ϴ� ..." << endl;
	logFile << "������Ʈ�� ã�� �ֽ��ϴ� ..." << endl;
	
	searcher->put_Online(VARIANT_TRUE);
	hr = searcher->Search(criteriaInstalled, &resultsInstall);
	hr = searcher->Search(criteriaUnInstalled, &resultsUninstall);

	SysFreeString(criteriaInstalled);
	SysFreeString(criteriaUnInstalled);

	switch (hr) {
	case S_OK:
		std::cout << "[��ȸ�� ������]" << endl;
		logFile << "[��ȸ�� ������]" << endl;
		break;
	case WU_E_LEGACYSERVER:
		std::cout << "[������ ã���� ����]" << endl;
		logFile << "[������ ã���� ����]" << endl;
		return;
	case WU_E_INVALID_CRITERIA:
		std::cout << "[�߸��� �˻� �����Դϴ�]" << endl;
		logFile << "[�߸��� �˻� �����Դϴ�]" << endl;
		return;
	}

	IUpdateCollection* updateList;
	IUpdate* updateItem;
	BSTR updateName = L"";
	LONG updateSize;

	/* Installed Items */
	resultsInstall->get_Updates(&updateList);
	updateList->get_Count(&updateSize);

	std::cout << "[��ġ�� ������Ʈ ���]" << endl;
	logFile << "[��ġ�� ������Ʈ ���]" << endl;
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

	std::cout << "[�� ��ġ�� ������Ʈ ���]" << endl;
	logFile << "[�� ��ġ�� ������Ʈ ���]" << endl;
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

	std::cout << "ã�� ������Ʈ ��: " << InstallName << endl;
	logFile << "ã�� ������Ʈ ��: " << InstallName << endl;

	std::cout << "������Ʈ�� ã�� �ֽ��ϴ� ..." << endl;
	logFile << "������Ʈ�� ã�� �ֽ��ϴ� ..." << endl;

	hr = searcher->Search(criteria, &results);
	SysFreeString(criteria);

	switch (hr) {
	case S_OK:
		break;
	case WU_E_LEGACYSERVER:
		std::cout << "������ ã���� ����" << endl;
		return;
	case WU_E_INVALID_CRITERIA:
		std::cout << "�߸��� �˻� �����Դϴ�." << endl;
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
		std::wcout << L"������Ʈ �Ҽ� �ִ� ����� �����ϴ�." << endl;
		logFile << L"������Ʈ �Ҽ� �ִ� ����� �����ϴ�." << endl;
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
		std::cout << "[�̹� ��ġ �� ������Ʈ �Դϴ�.]" << endl;
		return;
	}

	updateFilterList->get_Item(0, &updateItem);
	updateItem->get_Title(&InstallName);

	USES_CONVERSION;
	string pbstr = OLE2A(InstallName);

	std::cout << "[������ ������Ʈ ��ġ ����] : [" << pbstr.c_str() << "]" << endl;
	updateItem->get_IsDownloaded(&isDownload);

	if (isDownload == VARIANT_FALSE)
	{
		iUpdateSession->CreateUpdateDownloader(&updateDownloader);
		updateDownloader->put_Updates(updateFilterList);

		hr_download = updateDownloader->Download(&DownloaderRet);

		switch (hr_download) {
		case S_OK:
			std::wcout << L"�ٿ�ε� ����" << endl;
			logFile << L"�ٿ�ε� ����" << endl;
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
			std::wcout << L"�ν��� ����" << endl;
			logFile << L"�ν��� ����" << endl;
			break;
		default:
			std::wcout << hr_download << endl;
			logFile << hr_download << endl;
			break;
	}

	SysFreeString(InstallName);
	std::wcout << L"������Ʈ ��ġ�� �Ϸ�Ǿ����ϴ�." << endl;
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
		std::cout << "[�̹� ���� �� ������Ʈ �Դϴ�.]" << endl;
		return;
	}

	updateFilterList->get_Item(0, &updateItem);
	updateItem->get_Title(&unInstallName);

	USES_CONVERSION;
	string pbstr = OLE2A(unInstallName);

	std::cout << "[������ ������Ʈ ���� ����] : [" << pbstr.c_str() << "]" << endl;

	IUpdateInstaller* updateInstaller;
	IInstallationResult* InstallerRet;
	iUpdateSession->CreateUpdateInstaller(&updateInstaller);
	updateInstaller->put_Updates(updateFilterList);

	hr_uninstall = updateInstaller->Uninstall(&InstallerRet);

	switch (hr_uninstall)
	{
		case S_OK:
			std::wcout << L"���ν��� ����" << endl;
			logFile << L"���ν��� ����" << endl;
			break;
		default:
			std::wcout << hr_uninstall << endl;
			logFile << hr_uninstall << endl;
			break;
	}

	std::wcout << L"������Ʈ ���Ű� �Ϸ�Ǿ����ϴ�." << endl;

	SysFreeString(unInstallName);
}
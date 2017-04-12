#include <winsock2.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <atlconv.h>
#include <pdh.h>

#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "pdh.lib")

typedef enum ETH_QUERY_TYPE
{
	EQT_CURRENT_BANDWIDTH = 0,
	EQT_BYTES_RECV_PER_SEC = 1,
	EQT_BYTES_SENT_PER_SEC = 2,
	EQT_PACKET_RECV_PER_SEC = 3,
	EQT_PACKET_SENT_PER_SEC = 4,
	EQT_COUNT = 5
} ETH_QUERY_TYPE;

typedef struct Eth
{
	char *m_pszName;
	HQUERY m_hQuery;
	HCOUNTER m_hCounter[EQT_COUNT];
	unsigned long m_ulBytesRecvPerSec;
	unsigned long m_ulBytesSentPerSec;
} Eth;

typedef struct EthList
{
	int m_nCount;
	Eth *m_pEth;
} EthList;

bool OpenEthList(EthList *ethList, WCHAR *searchNet);
bool QueryEthUsage(Eth *eth);
void CloseEthList(EthList *ethList);

int main()
{
	BOOL search_data = FALSE;
	PIP_ADAPTER_INFO pAdapterInfo;
	PIP_INTERFACE_INFO pInterfaceInfo;

	pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
	pInterfaceInfo = (IP_INTERFACE_INFO *)malloc(sizeof(IP_INTERFACE_INFO));

	ULONG adapterBufLen = sizeof(IP_ADAPTER_INFO);
	ULONG InterfaceBufLen = sizeof(IP_INTERFACE_INFO);

	if (GetAdaptersInfo(pAdapterInfo, &adapterBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *)malloc(adapterBufLen);
	}

	PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
	if (GetAdaptersInfo(pAdapterInfo, &adapterBufLen) == NO_ERROR) {
		while (pAdapter) {
			// 6 : 이더넷 어댑터
			// 71 : 무선 어댑터
			if (pAdapter->Type == 6 || pAdapter->Type == 71)
			{
				if (pAdapter->IpAddressList.IpAddress.String[0] != '0')
				{
					printf("====================== 연결된 서버 항목\n");
					printf("\t어댑터 명: \t%s\n", pAdapter->AdapterName);
					printf("\t어댑터 설명: \t%s\n", pAdapter->Description);
					printf("\tIP 주소: \t%s\n", pAdapter->IpAddressList.IpAddress.String);
					printf("\t서브넷 마스크: \t%s\n", pAdapter->IpAddressList.IpMask.String);
					printf("\t게이트웨이: \t%s\n", pAdapter->GatewayList.IpAddress.String);

					if (pAdapter->DhcpEnabled) {
						printf("\tDHCP 서버 사용여부: 사용\n");
						printf("\t\tDHCP 서버 주소: \t%s\n", pAdapter->DhcpServer.IpAddress.String);
						printf("\t\t연결 시간: %ld\n", pAdapter->LeaseObtained);
					}
					else {
						printf("\tDHCP 서버 사용여부: 미 사용\n");
					}
					search_data = TRUE;
					break;
				}
				pAdapter = pAdapter->Next;
			}
		}
	}
	else {
		printf("연결된 인터넷 정보를 가져올 수 없습니다.\n");
	}
	
	EthList ethList;
	USES_CONVERSION;

	for (int i = 0; i < strlen(pAdapter->Description); i++)
	{
		if (pAdapter->Description[i] == '(')
			pAdapter->Description[i] = '[';
		else if (pAdapter->Description[i] == ')')
			pAdapter->Description[i] = ']';
	}

	OpenEthList(&ethList, A2W(pAdapter->Description));

	if (search_data)
	{
		Eth *e = &ethList.m_pEth[0];
		while (true)
		{
			if (!QueryEthUsage(e))
				break;

			long _sendkbps = (e->m_ulBytesSentPerSec / 1024) * 8;
			long _receivekbps = (e->m_ulBytesRecvPerSec / 1024) * 8;
			
			if (_sendkbps > 1024)
				printf("보내기 %ldMbps\n", _sendkbps / 1024);
			else
				printf("보내기 %ldKbps\n", _sendkbps);

			if (_receivekbps > 1024)
				printf("받기 %ldMbps\n", _receivekbps / 1024);
			else
				printf("받기 %ldKbps\n", _receivekbps);

			if (_sendkbps + _receivekbps > 1024)
				printf("총 %ldMbps\n", (_sendkbps + _receivekbps) / 1024);
			else
				printf("총 %ldKbps\n", _sendkbps + _receivekbps);

			Sleep(1000);
		}
	}

	CloseEthList(&ethList);
	_CrtDumpMemoryLeaks();
}

bool OpenEthList(EthList *ethList, WCHAR *searchNet)
{
	int i = 0;
	bool bErr = false;
	WCHAR *szCur = NULL;
	WCHAR *szBuff = NULL;
	WCHAR *szNet = NULL;
	DWORD dwBuffSize = 0, dwNetSize = 0;
	WCHAR szCounter[256];

	if (ethList == NULL)
		return false;

	memset(ethList, 0x00, sizeof(EthList));

	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szBuff, &dwBuffSize, szNet, &dwNetSize, PERF_DETAIL_WIZARD, 0);

	szBuff = (WCHAR *)malloc(sizeof(WCHAR) * dwBuffSize);
	szNet = (WCHAR *)malloc(sizeof(WCHAR) * dwNetSize);

	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szBuff, &dwBuffSize, szNet, &dwNetSize, PERF_DETAIL_WIZARD, 0);

	szCur = szNet;
	ethList->m_nCount = 0;
	while (*szCur != 0)
	{
		if (wcsstr(szCur, searchNet))
		{
			ethList->m_pEth = (Eth *)malloc(sizeof(Eth));
			memset(ethList->m_pEth, 0x00, sizeof(Eth));

			ethList->m_nCount = 1;

			USES_CONVERSION;
			Eth *eth = &ethList->m_pEth[0];
			eth->m_pszName = W2A(szCur);

			if (PdhOpenQuery(NULL, 0, &eth->m_hQuery))
			{
				bErr = true;
				break;
			}

			wsprintf(szCounter, L"\\Network Interface(%s)\\Bytes Received/sec", A2W(eth->m_pszName));
			if (PdhAddCounter(eth->m_hQuery, szCounter, 0, &eth->m_hCounter[EQT_BYTES_RECV_PER_SEC]))
			{
				bErr = true;
				break;
			}

			wsprintf(szCounter, L"\\Network Interface(%s)\\Bytes Sent/sec", A2W(eth->m_pszName));
			if (PdhAddCounter(eth->m_hQuery, szCounter, 0, &eth->m_hCounter[EQT_BYTES_SENT_PER_SEC]))
			{
				bErr = true;
				break;
			}
		}
		szCur += lstrlen(szCur) + 1;
	}

	if (ethList->m_nCount <= 0)
	{
		free(szBuff);
		free(szNet);
		return false;
	}

	if (szBuff != NULL)
		free(szBuff);

	if (szNet != NULL)
		free(szNet);

	if (bErr)
		return false;

	return true;
}

bool QueryEthUsage(Eth *eth)
{
	HCOUNTER hCounter = NULL;

	PDH_FMT_COUNTERVALUE v;

	if (eth == NULL)
		return false;

	if (PdhCollectQueryData(eth->m_hQuery))
		return false;

	v.longValue = 0;
	PdhGetFormattedCounterValue(eth->m_hCounter[EQT_BYTES_RECV_PER_SEC], PDH_FMT_LONG, 0, &v);
	eth->m_ulBytesRecvPerSec = v.longValue;

	v.longValue = 0;
	PdhGetFormattedCounterValue(eth->m_hCounter[EQT_BYTES_SENT_PER_SEC], PDH_FMT_LONG, 0, &v);
	eth->m_ulBytesSentPerSec = v.longValue;

	return true;
}

void CloseEthList(EthList *ethList)
{
	if (ethList != NULL)
	{
		int i = 0, j = 0;
		for (i = 0; i < ethList->m_nCount; i++)
		{
			Eth *eth = &ethList->m_pEth[i];
			free(eth->m_pszName);

			for (j = 0; j < EQT_COUNT; j++)
			{
				if (eth->m_hCounter[j] != NULL)
				{
					PdhRemoveCounter(eth->m_hCounter[j]);
					eth->m_hCounter[j] = NULL;
				}
			}

			PdhCloseQuery(eth->m_hQuery);
			eth->m_hQuery = NULL;
		}

		free(ethList->m_pEth);
		memset(ethList, 0x00, sizeof(EthList));
	}
}
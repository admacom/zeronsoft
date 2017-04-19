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
	EQT_NET_COUNT = 5
} ETH_QUERY_TYPE;

typedef enum ETH_PROCESS_TYPE
{
	EQT_IO_READ_BYTES = 0,
	EQT_IO_WRITE_BYTES = 1,
	EQT_PROC_COUNT = 2
} ETH_PROCESS_TYPE;

typedef struct Eth_Net
{
	char *m_pszName;
	HQUERY m_hQuery;
	HCOUNTER m_hCounter[EQT_NET_COUNT];
	unsigned long m_ulBytesRecvPerSec;
	unsigned long m_ulBytesSentPerSec;
} Eth_Net;

typedef struct Eth_Proc
{
	char *m_pszName;
	HQUERY m_hQuery;
	HCOUNTER m_hCounter[EQT_PROC_COUNT];
	unsigned long m_ReadBytesperSec;
	unsigned long m_WriteBytesperSec;
} Eth_Proc;

typedef struct EthList
{
	int m_nCount_Net;
	int m_nCount_Proc;
	Eth_Net *m_pEth;
	Eth_Proc *m_pEth_Proc;
} EthList;

bool OpenEthList(EthList *ethList, WCHAR *searchNet);
bool QueryEthUsage(Eth_Net *eth);
bool QueryEthProcUsage(Eth_Proc *eth);
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
		Eth_Net *e = &ethList.m_pEth[0];
		while (true)
		{
			if (!QueryEthUsage(e))
				break;

			long _sendkbps = (e->m_ulBytesSentPerSec / 1024) * 8;
			long _receivekbps = (e->m_ulBytesRecvPerSec / 1024) * 8;

			printf("어댑터 명: %s\n", e->m_pszName);

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

			printf("===============PROCESS===============\n");
			for (int i = 0; i < ethList.m_nCount_Proc; i++)
			{
				Eth_Proc *e_p = &ethList.m_pEth_Proc[i];

				if (QueryEthProcUsage(e_p))
				{
					// 0 이상인것만 출력
					if (e_p->m_ReadBytesperSec + e_p->m_WriteBytesperSec > 0)
						printf("[%s.exe], 쓰기: %d B/s, 받기: %d B/s\n", e_p->m_pszName, e_p->m_WriteBytesperSec, e_p->m_ReadBytesperSec);
				}
			}
			printf("===============PROCESS===============\n");
			Sleep(3000);
		}
	}

	CloseEthList(&ethList);
	_CrtDumpMemoryLeaks();

	return 0;
}

bool OpenEthList(EthList *ethList, WCHAR *searchNet)
{
	bool bErr = false;
	WCHAR *szCur = NULL;
	WCHAR *szCur_Proc = NULL;
	WCHAR *szBuff = NULL;
	WCHAR *szNet = NULL;
	WCHAR *szProc = NULL;
	DWORD dwBuffSize = 0, dwNetSize = 0;
	DWORD dwProcBuffSize = 0, dwProcSize = 0;
	WCHAR szCounter[256];

	if (ethList == NULL)
		return false;

	memset(ethList, 0x00, sizeof(EthList));

	/* NETWORK INTERFACE */ 
	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szBuff, &dwBuffSize, szNet, &dwNetSize, PERF_DETAIL_WIZARD, 0);

	szBuff = (WCHAR *)malloc(sizeof(WCHAR) * dwBuffSize);
	szNet = (WCHAR *)malloc(sizeof(WCHAR) * dwNetSize);

	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szBuff, &dwBuffSize, szNet, &dwNetSize, PERF_DETAIL_WIZARD, 0);

	szCur = szNet;

	ethList->m_nCount_Net = 0;
	while (*szCur != 0)
	{
		if (wcsstr(szCur, searchNet))
		{
			ethList->m_pEth = (Eth_Net *)malloc(sizeof(Eth_Net));
			memset(ethList->m_pEth, 0x00, sizeof(Eth_Net));

			ethList->m_nCount_Net = 1;

			USES_CONVERSION;
			Eth_Net *eth = &ethList->m_pEth[0];
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

	if (ethList->m_nCount_Net <= 0)
	{
		free(szBuff);
		free(szNet);
	}


	/* PROCESS */
	PdhEnumObjectItems(NULL, NULL, L"Process", szBuff, &dwProcBuffSize, szProc, &dwProcSize, PERF_DETAIL_WIZARD, 0);

	szBuff = (WCHAR *)malloc(sizeof(WCHAR) * dwProcBuffSize);
	szProc = (WCHAR *)malloc(sizeof(WCHAR) * dwProcSize);

	PdhEnumObjectItems(NULL, NULL, L"Process", szBuff, &dwProcBuffSize, szProc, &dwProcSize, PERF_DETAIL_WIZARD, 0);

	szCur_Proc = szProc;
	int i = 0;

	while (*szCur_Proc != 0)
	{
		szCur_Proc += lstrlen(szCur_Proc) + 1;
		i++;
	}

	ethList->m_nCount_Proc = i;
	ethList->m_pEth_Proc = (Eth_Proc *)malloc(sizeof(Eth_Proc) * i);
	memset(ethList->m_pEth_Proc, 0x00, sizeof(Eth_Proc) * i);

	i = 0;
	szCur_Proc = szProc;
	while (*szCur_Proc != 0)
	{
		USES_CONVERSION;
		Eth_Proc *eth_proc = &ethList->m_pEth_Proc[i];
		eth_proc->m_pszName = _strdup(W2A(szCur_Proc));

		if (PdhOpenQuery(NULL, 0, &eth_proc->m_hQuery))
		{
			bErr = true;
			break;
		}

		wsprintf(szCounter, L"\\Process(%s)\\IO Read Bytes/sec", A2W(eth_proc->m_pszName));
		if (PdhAddCounter(eth_proc->m_hQuery, szCounter, 0, &eth_proc->m_hCounter[EQT_IO_READ_BYTES]))
		{
			bErr = true;
			break;
		}

		wsprintf(szCounter, L"\\Process(%s)\\IO Write Bytes/sec", A2W(eth_proc->m_pszName));
		if (PdhAddCounter(eth_proc->m_hQuery, szCounter, 0, &eth_proc->m_hCounter[EQT_IO_WRITE_BYTES]))
		{
			bErr = true;
			break;
		}

		szCur_Proc += lstrlen(szCur_Proc) + 1;
		i++;
	}

	if (ethList->m_nCount_Net <= 0)
	{
		free(szBuff);
		free(szNet);
	}

	if (szBuff != NULL)
		free(szBuff);

	if (szNet != NULL)
		free(szNet);

	if (szProc != NULL)
		free(szProc);

	if (bErr)
		return false;

	return true;
}

bool QueryEthUsage(Eth_Net *eth)
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

bool QueryEthProcUsage(Eth_Proc *eth)
{
	HCOUNTER hCounter = NULL;

	PDH_FMT_COUNTERVALUE v;

	if (eth == NULL)
		return false;

	if (eth->m_pszName  == NULL) return false;
	if (strcmp(eth->m_pszName, "_Total") == 0) return false;
	
	if (PdhCollectQueryData(eth->m_hQuery))
		return false;

	v.longValue = 0;
	PdhGetFormattedCounterValue(eth->m_hCounter[EQT_IO_READ_BYTES], PDH_FMT_LONG, 0, &v);
	eth->m_ReadBytesperSec = v.longValue;

	v.longValue = 0;
	PdhGetFormattedCounterValue(eth->m_hCounter[EQT_IO_WRITE_BYTES], PDH_FMT_LONG, 0, &v);
	eth->m_WriteBytesperSec = v.longValue;

	return true;
}

void CloseEthList(EthList *ethList)
{
	if (ethList != NULL)
	{
		int i = 0, j = 0;
		for (i = 0; i < ethList->m_nCount_Net; i++)
		{
			Eth_Net *eth = &ethList->m_pEth[i];
			free(eth->m_pszName);

			for (j = 0; j < EQT_NET_COUNT; j++)
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

		for (i = 0; i < ethList->m_nCount_Proc; i++)
		{
			Eth_Proc *eth = &ethList->m_pEth_Proc[i];
			free(eth->m_pszName);

			for (j = 0; j < EQT_NET_COUNT; j++)
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
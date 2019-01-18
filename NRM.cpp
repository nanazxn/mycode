// NRM.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "NRM.h"

#include "UI/MainFrm.h"
#include "NRMDoc.h"
#include "UI/NRMView.h"
#include "Include/Constant.h"
#include "UI/RCPropertySheet.h"
#include "Include\FunctionLibrary.h"
#include "Win32Tools\Exception.h"
#include "Win32Tools\Utils.h"
#include "Win32Tools\SharedCriticalSection.h"
#include "UI/CognitiveInfosDlg.h"
#include "UI/predictionview.h"
#include "Include/ChannelMetricComparator.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
using JetByteTools::Win32::Output;
using JetByteTools::Win32::CSharedCriticalSection;
using JetByteTools::Win32::CSocketServerUDP::IOPool;
/////////////////////////////////////////////////////////////////////////////
// CNRMApp


CTypeLibrary::CStoreinfo m_pRNRM1={0,0,"",0,FALSE};//zxn以下三个变量分别存储三个子网的信息
CTypeLibrary::CStoreinfo m_pRNRM2={0,0,"",0,FALSE};
CTypeLibrary::CStoreinfo m_pRNRM3={0,0,"",0,FALSE};
int Time_1s=0;//1s
BYTE m_RNRMUserused[3]={0,0,0};

BEGIN_MESSAGE_MAP(CNRMApp, CWinApp)
	//{{AFX_MSG_MAP(CNRMApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_APP_VIEW_COGNITIVE, OnCognitiveView)
	ON_COMMAND(ID_APP_VIEW_RNRM,OnRNRMView)
	ON_COMMAND(ID_APP_VIEW_PREDICTION, OnPredictionView)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
	ON_COMMAND(ID_APP_RUN,OnManagerRun)
	ON_COMMAND(ID_APP_STOP,OnManagerStop)

	ON_COMMAND(ID_COGNITIVE_LOADBALANCE,OnLoadBalance)
	ON_COMMAND(ID_COGNITIVE_RESERVATION,OnReservation)
	ON_COMMAND(ID_COGNITIVE_SERVICES_PREDICTION,OnServPrediction)

	ON_UPDATE_COMMAND_UI(ID_COGNITIVE_LOADBALANCE, OnUpdataLoadBalance)
	ON_UPDATE_COMMAND_UI(ID_COGNITIVE_RESERVATION, OnUpdataReservation)
	ON_UPDATE_COMMAND_UI(ID_COGNITIVE_SERVICES_PREDICTION, OnUpdataServPrediction)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNRMApp construction

CNRMApp::CNRMApp() :
	CBCGPWorkspace (TRUE /* m_bResourceSmartUpdate */),
m_pServerRNRM(NULL),
m_pServerBroad(NULL),
m_pRNRMIDs(NULL),
m_pRNRMQueue(NULL),
/*m_pRNRM1(NULL),//zxn
m_pRNRM2(NULL),
m_pRNRM3(NULL),*/
m_pMainFrame(NULL),
m_pViewNRM(NULL),
m_pViewCognitive(NULL),
m_pViewPrediction(NULL),
m_pBufCache(NULL),
m_bStart(FALSE),
m_bRunning(FALSE),
m_nActiveAP(0),
m_nQueryModeAuto(1),
m_nAPCommandIndex(0),
m_bArubaStateValid(FALSE),
MaxAvailability(0),
MinAvailability(0),
AverageAvailability(0),
m_bLoadBalance(FALSE),
m_bLoadBalanceflag(FALSE),
m_bReservation(FALSE),
m_IDlocal(0),
m_IDaim(0),
m_bswitch(FALSE),
m_bServPrediction(FALSE)
{
	// TODO: add construction code here,
	m_pLockFactory= new CSharedCriticalSection(47);
	m_bRNRMNum=0;
	m_pRNRMIDs=new BYTE[MAX_RNRM_NUM];
	memset(m_pRNRMIDs,0,MAX_RNRM_NUM);
	m_pRNRMQueue=new CTypeLibrary::CRNRMQueue;
	m_pRNRMQueue->next=NULL;
	m_pBufCache=new CBuffer;
	m_bParaTransOn=FALSE;
	m_nParaTransChanA=0;
	m_nParaTransChanB=0;
	InitializeCriticalSection(&m_csBufCache);
	InitializeCriticalSection(&m_csRNRMQueue);
}

CNRMApp::~CNRMApp()
{
	delete m_pLockFactory;
	m_pLockFactory=NULL;
	CTypeLibrary::CRNRMQueue *ptrQueue=m_pRNRMQueue->next;
	while(ptrQueue)
	{
		CTypeLibrary::CRNRMQueue *tempQueue=ptrQueue;
		CTypeLibrary::CRNRMService *ptrServ=&ptrQueue->RNRMService;
		while(ptrServ)
		{
			CTypeLibrary::CRNRMService *tempServ=ptrServ;
			ptrServ=ptrServ->next;
			delete tempServ;
		}
		ptrQueue=ptrQueue->next;
		delete tempQueue;
	}
	delete[] m_pRNRMIDs;
	delete m_pBufCache;
	delete m_pRNRMQueue;
/*	delete m_pRNRM1;//zxn
	delete m_pRNRM2;
	delete m_pRNRM3;*/
	DeleteCriticalSection(&m_csBufCache);
	DeleteCriticalSection(&m_csRNRMQueue);
};

/////////////////////////////////////////////////////////////////////////////
// The one and only CNRMApp object

CNRMApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CNRMApp initialization

BOOL CNRMApp::InitInstance()
{
	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("BCGP AppWizard-Generated Applications"));

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	SetRegistryBase (_T("Settings"));

	// Initialize all Managers for usage. They are automatically constructed
	// if not yet present
	InitContextMenuManager();
	InitKeyboardManager();

	// TODO: Remove this if you don't want extended tooltips:
	InitTooltipManager();

	CBCGPToolTipParams params;
	params.m_bVislManagerTheme = TRUE;

	theApp.GetTooltipManager ()->SetTooltipParams (
		BCGP_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS (CBCGPToolTipCtrl),
		&params);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CNRMDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CNRMView));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(SW_SHOWNORMAL);
	m_pMainWnd->UpdateWindow();

	//Added By Sunshine.
	m_pFirstView =((CFrameWnd*)m_pMainWnd)->GetActiveView();
	m_pMainFrame=(CMainFrame *)AfxGetMainWnd();
	m_pViewNRM=(CNRMView*)m_pMainFrame->GetActiveView();
	m_pViewCognitive=new CCognitiveView;
	m_pViewPrediction=new CPredictionView;

	CDocument* pDoc = ((CFrameWnd*)m_pMainWnd)->GetActiveDocument();

	CCreateContext context;
	context.m_pCurrentDoc = pDoc;
	UINT m_ID = AFX_IDW_PANE_FIRST + 1;
	CRect rect;

	m_pViewCognitive->Create(NULL, NULL, WS_CHILD, rect, m_pMainWnd, m_ID, &context);
	m_pViewCognitive->OnInitialUpdate();
	m_pViewPrediction->Create(NULL, NULL, WS_CHILD, rect, m_pMainWnd, m_ID, &context);
	m_pViewPrediction->OnInitialUpdate();
	m_Option.Load();
	m_bRunning=TRUE;
	m_setChannels2Alloc.insert(1);
	m_setChannels2Alloc.insert(6);
	m_setChannels2Alloc.insert(11);

	m_hEventAruba=CreateEvent(NULL, FALSE, FALSE, NULL);
	SetEvent(m_hEventAruba);
	::CreateThread(NULL,0,CFunctionLibrary::HandBufProc,NULL,0,NULL);
	::CreateThread(NULL,0,CFunctionLibrary::CheckRNRMQueueProc,NULL,0,NULL);
	::CreateThread(NULL,0,CFunctionLibrary::PeriodBroadProc,NULL,0,NULL);
	::CreateThread(NULL,0,CFunctionLibrary::DaemonThreadUSRP,NULL,0,NULL);
	::CreateThread(NULL,0,CFunctionLibrary::DaemonThreadAruba,NULL,0,NULL);
	::CreateThread(NULL,0,CFunctionLibrary::ServicesPrediction,NULL,0,NULL);
//	::CreateThread(NULL,0,CFunctionLibrary::SingleTransmission,NULL,0,NULL);    //yangrui 单路传输数据包
//	::CreateThread(NULL,0,CFunctionLibrary::DoubleTransmission1,NULL,0,NULL);    //yangrui 双路传输数据包
//	::CreateThread(NULL,0,CFunctionLibrary::DoubleTransmission2,NULL,0,NULL);    //yangrui 双路传输数据包
//	::CreateThread(NULL,0,CFunctionLibrary::QueryArubaProc,NULL,0,NULL);

	m_apStates[0].apName=CConstant::AP1;
	m_apStates[1].apName=CConstant::AP2;
	m_apStates[2].apName=CConstant::AP3;
	for(int i=0;i<3;i++)
	{
		m_apStates[i].apChanType=CConstant::CHANNELG;
	}
	((CMainFrame *)m_pMainWnd)->StartDraw();
	InitNetService();
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CNRMApp message handlers

int CNRMApp::ExitInstance()
{
	OnServerStop();
	BCGCBProCleanUp();
	CleanState();
	WSACleanup();
	m_Option.Save();
	m_bRunning=FALSE;
	return CWinApp::ExitInstance();
}

// App command to run the dialog
void CNRMApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

void CNRMApp::PreLoadState ()
{
	GetContextMenuManager()->AddMenu (_T("My menu"), IDR_CONTEXT_MENU);
	// TODO: add another context menus here
}


/////////////////////////////////////////////////////////////////////////////
// CNRMApp message handlers

void CNRMApp::OnManagerRun()
{
	CPropertySheetRC propSheet(CBCGPPropertySheet::PropSheetLook_List);
	propSheet.EnablePageHeader (HEADER_HEIGHT);
	propSheet.EnableVisualManagerStyle ();
	propSheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(IDCANCEL == propSheet.DoModal())
		return;
}

void CNRMApp::OnManagerStop()
{
	OnServerStop();
}

BOOL CNRMApp::InitNetService()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 )
	{
		CFunctionLibrary::ShowErrorMsg("WSAStartup",CConstant::WSAGETLASTERROR);
		return FALSE;
	}
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 )
	{
		CFunctionLibrary::ShowErrorMsg("WSACleanUp",CConstant::WSAGETLASTERROR);
		WSACleanup( );
		return FALSE;
	}
	return TRUE;
}

void CNRMApp::OnServerStop()
{
	m_bStart=FALSE;
	if (m_pServerRNRM != NULL)
	{
		CSocketServer *server = m_pServerRNRM;
		m_pServerRNRM = NULL;
		try
		{
			server->StopAcceptingConnections();
			m_pPool->WaitForShutdownToComplete();
			server->WaitForShutdownToComplete();
		}
		catch(const JetByteTools::Win32::CException &e)
		{
			Output(_T("Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage());
		}
		catch(...)
		{
			Output(_T("Unexpected exception"));
		}
		delete server;
	}
	if (m_pServerBroad != NULL)
	{
		CSocketServer *server = m_pServerBroad;
		m_pServerBroad = NULL;
		try
		{
			server->StopAcceptingConnections();
			m_pPool->WaitForShutdownToComplete();
			server->WaitForShutdownToComplete();
		}
		catch(const JetByteTools::Win32::CException &e)
		{
			Output(_T("Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage());
		}
		catch(...)
		{
			Output(_T("Unexpected exception"));
		}
		delete m_pPool;
		m_pPool = NULL;
		delete server;
	}
	Output(_T("Server stop succeed."));
}

void CNRMApp::OnServerRun()
{
	theApp.m_pMainFrame->OnCognitiveArubacontrol();
	theApp.m_pMainFrame->m_pArubaManagerDlg->ShowWindow(SW_HIDE);
	try
	{
		if(m_pPool==NULL)
		{
			m_pPool = new CIOPool();
			m_pPool->Start();
		}
		m_pServerBroad = new CSocketServer(
			*m_pLockFactory,
			*m_pPool,
			m_Option.m_ulIPBroad,
			m_Option.m_usPortBroad,
			5,					// max number of sockets to keep in the pool
			5,					// max number of buffers to keep in the pool
			3,
			MAX_UDP_SIZE);
		m_pServerBroad->Start();
		m_pServerBroad->StartAcceptingConnections();

		m_pServerRNRM = new CSocketServer(
			*m_pLockFactory,
			*m_pPool,
			m_Option.m_ulIPNRM,
			m_Option.m_usPortNRM,
			5,					// max number of sockets to keep in the pool
			5,					// max number of buffers to keep in the pool
			3,
			MAX_UDP_SIZE);
		m_pServerRNRM->Start();
		m_pServerRNRM->StartAcceptingConnections();
	}
	catch(const JetByteTools::Win32::CException &e)
	{
		Output(_T("Exception: ") + e.GetWhere() + _T(" - ") + e.GetMessage());
	}
	catch(...)
	{
		Output(_T("Unexpected exception"));
	}
	Output(_T("Server start succeed!"));
	m_bStart=TRUE;
}

BOOL CNRMApp::OnPacketRecvd(const BYTE *lpData, size_t len)
{
	CTypeLibrary::CPacketHead* pkh=reinterpret_cast<CTypeLibrary::CPacketHead *>(const_cast<BYTE *>(lpData));
/*	if(pkh->type==CConstant::COGNITIVE_SIGNALING)
	{
		CFunctionLibrary::Log(CConstant::LOG_SERVER, " COGNITIVE_SIGNALING RECVD");
	}
*/	ULONG ipTo;
	memcpy(&ipTo,&lpData[8],sizeof(ULONG));
	BOOL bRet=(ipTo==theApp.m_Option.m_ulIPNRM||ipTo==theApp.m_Option.m_ulIPBroad);
	if(bRet)
		m_pBufCache->AddBuffer(lpData,len);
	return bRet;
}

BOOL CNRMApp::OnNewRNRMJoin(const BYTE *lpData, size_t len)
{
	ASSERT(m_pRNRMQueue!=NULL);

	BYTE nId=INVALID_ADDR,nChan=INVALID_ADDR;
	ULONG ipRNRM=0;
	memcpy(&ipRNRM,&lpData[4],sizeof(int));
	int nRet=ResAlloc(ipRNRM,nId,nChan);
	int nLen=0;
	if(nRet!=ERROR_SUCCESS)
	{
		//CFunctionLibrary::ShowErrorMsg("CNRMApp::OnNewRNRMJoin::ResAlloc",CConstant::MYSQLERROR);
		//return FALSE;
	}
	memcpy(&nLen,&lpData[12],sizeof(ULONG));

	CTypeLibrary::CRNRMQueue *ptr=m_pRNRMQueue;
	while(ptr->next)
		ptr=ptr->next;

	CTypeLibrary::CRNRMQueue * newRNRM=new CTypeLibrary::CRNRMQueue;
	newRNRM->bCount=4;
	newRNRM->bID=nId;
	newRNRM->bChannel=nChan;
	memcpy(&newRNRM->bUserTotal,&lpData[PACKET_HEAD_SIZE+1],sizeof(BYTE));
	memcpy(&newRNRM->bUserUsed,&lpData[PACKET_HEAD_SIZE+2],sizeof(BYTE));
	memcpy(&newRNRM->bBandTotal,&lpData[PACKET_HEAD_SIZE+3],sizeof(BYTE));
	memcpy(&newRNRM->dBandUsed,&lpData[PACKET_HEAD_SIZE+4],sizeof(double));
	newRNRM->ulIPRNRM=ipRNRM;
	memcpy(&newRNRM->ulIPTRM,&lpData[PACKET_HEAD_SIZE+16],sizeof(ULONG));
	memcpy(&newRNRM->nDelay,&lpData[28],sizeof(int));
	memset(newRNRM->name,0,sizeof(newRNRM->name));
	int nPos=PACKET_HEAD_SIZE+20,nTemp=0;
	memcpy(&nTemp,&lpData[nPos],sizeof(int));
	nPos+=sizeof(int);
	memcpy(newRNRM->name,&lpData[nPos],nTemp);
	nPos+=nTemp;

	for(int m=0;m<CConstant::BP_SAMPLE_LEN;m++)
	{
		newRNRM->predicSample[m]=0;
	}
	for(int n=0;n<CConstant::BP_OUTPUT_NUM;n++)
	{
		newRNRM->predictOutput[n]=0;
	}

	newRNRM->next=NULL;
	ptr->next=newRNRM;

	int nSrvNum=(nLen-nPos)/sizeof(CTypeLibrary::CRNRMServicePara);
	memcpy(&newRNRM->RNRMService.service,&lpData[nPos],sizeof(CTypeLibrary::CRNRMServicePara));
	nPos+=sizeof(CTypeLibrary::CRNRMServicePara);
	newRNRM->RNRMService.next=NULL;

	CTypeLibrary::CRNRMService *ptrSrvTail=&newRNRM->RNRMService,*ptrSrvNew=NULL;
	for(int i=1;i<nSrvNum;i++)
	{
		ptrSrvNew=new CTypeLibrary::CRNRMService;
		memcpy(&ptrSrvNew->service,&lpData[nPos],sizeof(CTypeLibrary::CRNRMServicePara));
		nPos+=sizeof(CTypeLibrary::CRNRMServicePara);
		ptrSrvNew->next=NULL;
		ptrSrvTail->next=ptrSrvNew;
		ptrSrvTail=ptrSrvNew;
	}
	CTypeLibrary::CTopologyInfo info;
	memset(&info,0,sizeof(CTypeLibrary::CTopologyInfo));
	info.bID=nId;
	UINT nType=CConstant::RNRM_ADD_NEW;
	::SendMessage(m_pViewNRM->m_hWnd,CConstant::WM_RNRM_UPDATE,(WPARAM)&info,(LPARAM)nType);
	m_bRNRMNum++;

	char buf[MAX_UDP_SIZE];
	memset(buf,0,sizeof(buf));
	nPos=0;
	nTemp=CConstant::COGNITIVE_RNRM_ACCESS;
//	nTemp=CConstant::COGNITIVE_DOUBLELINK;
	if(nId==INVALID_ADDR)
		nTemp=CConstant::COGNITIVE_RNRM_REJECT;

	CTypeLibrary::PacketHeadRt pktHead={0,0,nTemp,0};
	memcpy(&buf[nPos],&pktHead,sizeof(CTypeLibrary::PacketHeadRt));
	nPos+=sizeof(CTypeLibrary::PacketHeadRt);
	memcpy(&buf[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));
	nPos+=sizeof(ULONG);
	memcpy(&buf[nPos],&ipRNRM,sizeof(ULONG));
	nPos+=sizeof(ULONG)+sizeof(int);
	memcpy(&buf[nPos++],&nId,sizeof(BYTE));
	memcpy(&buf[nPos++],&nChan,sizeof(BYTE));
	memcpy(&buf[12],&nPos,sizeof(int));
	CFunctionLibrary::SendToRNRM(ipRNRM,buf,nPos);
	CFunctionLibrary::Log(CConstant::LOG_SERVER,"RNRM Accessed: "+GetRNRMDesc(newRNRM->bID));
	return TRUE;
}


int CNRMApp::ResAlloc(ULONG ulAddr,BYTE &bId,BYTE &bChan)
{
	CTypeLibrary::CRNRMQueue *ptrQueue=m_pRNRMQueue->next;
	BOOL bFlag=TRUE;
	while(ptrQueue)
	{
		if(ptrQueue->ulIPRNRM==ulAddr)
			return CConstant::ERROR_RNRM_ALREADYEXIST;
		ptrQueue=ptrQueue->next;
	}
	for(int i=0;i<MAX_RNRM_NUM;i++)
		if(!m_pRNRMIDs[i])
		{
			m_pRNRMIDs[i]=1;
			bId=i+1;
			break;
		}
	if(m_setChannels2Alloc.size()>0)
	{
		set<int>::iterator it=m_setChannels2Alloc.begin();
		int nOptimalChannel=*it,nOptimalMetric=0;
		for(;it!=m_setChannels2Alloc.end();it++)
		{
			int nChannelAva=m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(*it,CConstant::CHANNEL_AVAILABILITY);
			if(nChannelAva>nOptimalMetric)
			{
				nOptimalMetric=nChannelAva;
				nOptimalChannel=*it;
			}
		}
		ASSERT(nOptimalChannel!=0);
		bChan=nOptimalChannel;
		m_setChannels2Alloc.erase(bChan);
	}
	else
		bChan=255;
	if(bId==255||bChan==255)
		return CConstant::ERROR_UNKNOWN;
	else
		return ERROR_SUCCESS;
}

BOOL CNRMApp::OnUpdateRNRMInfo(BYTE *lpData, int nLen)
{
    CString str,str1;//调试
  //  int a=800;
 //   a=sizeof(BOOL);
  //  str1.Format("%d",lpData[900]);
   // CFunctionLibrary::Log(CConstant::LOG_SERVER,str1);
  //  Time_1s++;
	EnterCriticalSection(&m_csRNRMQueue);
	Time_1s++;
	CTypeLibrary::PtrPacketHeaderCog ptrHeader=reinterpret_cast<CTypeLibrary::PtrPacketHeaderCog>(lpData);
	BYTE bID=INVALID_ID,bUserUsed=0,bUserTotal=0,bBandTotal=0,nChan=0;
	BYTE bMovelocalID=0,bMoveaimID=0,bReservelocalID=0,bReserveaimID=0,IDaim=0;//zxn
	int nDelay=0;
	BOOL loadbalanceFlag=FALSE;
	BOOL loadblancelocal=FALSE,loadblanceaim=FALSE,moveflaglocal=FALSE,reserveflagaim=FALSE;//zxn
	int flag=0;

	double dBandUsed=0;
	int nPos=sizeof(CTypeLibrary::PacketHeaderCog),nPos1=800,nPos2=800,nPos3=850;
	ULONG ipTRM;
	memcpy(&ipTRM,&lpData[nPos],sizeof(int));
	nPos+=sizeof(ULONG);
	bID=lpData[nPos++],nChan=lpData[nPos++];
	memcpy(&nDelay,&lpData[nPos],sizeof(int));
	nPos+=sizeof(int);
	bUserTotal=lpData[nPos++], bUserUsed=lpData[nPos++], bBandTotal=lpData[nPos++];
	memcpy(&dBandUsed,&lpData[nPos],sizeof(double));
	nPos+=sizeof(double);
	loadbalanceFlag=lpData[nPos];
	nPos+=sizeof(BOOL);

	loadblancelocal=lpData[nPos2];//zxn取负载均衡相应标志位
    nPos2+=sizeof(BOOL);
    loadblanceaim=lpData[nPos2];
    nPos2+=sizeof(BOOL);
    IDaim=lpData[nPos2];
    nPos2++;

    moveflaglocal=lpData[nPos2];////最后添加
    nPos2+=sizeof(BOOL);
    bMovelocalID=lpData[nPos2];
    nPos2++;
    bMoveaimID=lpData[nPos2];
    nPos2++;
    reserveflagaim=lpData[nPos2];
    nPos2+=sizeof(BOOL);
    bReservelocalID=lpData[nPos2];
    nPos2++;
    bReserveaimID=lpData[nPos2];
    nPos2++;

   /* moveflaglocal=lpData[nPos3];
    nPos3+=sizeof(BOOL);
    bMovelocalID=lpData[nPos3];
    nPos3++;
    bMoveaimID=lpData[nPos3];
    nPos3++;
    reserveflagaim=lpData[nPos3];
    nPos3+=sizeof(BOOL);
    bReservelocalID=lpData[nPos3];
    nPos3++;
    bReserveaimID=lpData[nPos3];
    nPos3++;*/

	CTypeLibrary::CRNRMQueue *ptrQueue=m_pRNRMQueue->next;
	CTypeLibrary::CRNRMQueue *ptrQueue1=m_pRNRMQueue->next;//zxn存放负载均衡开始结束的标志位
	BOOL bRet=FALSE;
	int i=0,flag1=0;
	while (ptrQueue)
	{
		if(ptrQueue->bID==bID)
		{
			ptrQueue->bCount++;
			ptrQueue->bUserTotal=bUserTotal;
			ptrQueue->bUserUsed=bUserUsed;
			ptrQueue->bBandTotal=bBandTotal;
			ptrQueue->dBandUsed=dBandUsed;
        //    loadblancelocal=TRUE;////测试用
        //    IDaim=2;
			ptrQueue->m_bLoadBalancelocal=loadblancelocal;//zxn
            ptrQueue->m_bLoadBalanceaim=loadblanceaim;
            ptrQueue->m_IDaim=IDaim;
            ptrQueue->m_bmoveflag=moveflaglocal;
            ptrQueue->bMovelocalID=bMovelocalID;
            ptrQueue->bMoveaimID=bMoveaimID;
            ptrQueue->m_breserveflag=reserveflagaim;
            ptrQueue->bReservelocalID=bReservelocalID;
            ptrQueue->bReserveaimID=bReserveaimID;
            if(m_RNRMUserused[i]!=ptrQueue->bUserUsed)////zxn
            {
                flag1=1;
            }
            m_RNRMUserused[i]=ptrQueue->bUserUsed;
            if(loadblancelocal==TRUE)
            {
                str.Format("ID:%d NetName:%s ",ptrQueue->bID,ptrQueue->name);
                str+="  TO  ";
                str1.Format("ID:%d",ptrQueue->m_IDaim);
                str+=str1;
                CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("正在进行负载均衡："));//+_T("子网：")+(CString)(pQueue1->bID)+_T("子网：")+(CString)(pQueue2->bID)));
                CFunctionLibrary::Log(CConstant::LOG_SERVER,str);
                m_IDlocal=ptrQueue->bID;
                m_IDaim=ptrQueue->m_IDaim;
                m_bswitch=TRUE;
                m_pViewNRM->Invalidate();

            }
            if(reserveflagaim==TRUE)
            {
                str.Format("ID:%d NetName:%s ",ptrQueue->bID,ptrQueue->name);
                str+="  TO  ";
                str1.Format("ID:%d",ptrQueue->bReserveaimID);
                str+=str1;
                CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("正在进行资源预留："));
                CFunctionLibrary::Log(CConstant::LOG_SERVER,str);
                m_IDlocal=ptrQueue->bID;
                m_IDaim=ptrQueue->bReserveaimID;
                m_bswitch=TRUE;
                flag=1;
                m_pViewNRM->Invalidate();
            }
            if(moveflaglocal==TRUE)
            {
                if(ptrQueue->bMoveaimID>0)
               {
                    str.Format("ID:%d NetName:%s ",ptrQueue->bID,ptrQueue->name);
                    str+="  TO  ";
                    str1.Format("ID:%d",ptrQueue->bMoveaimID);
                    str+=str1;
                    if(flag==1)
                    {
                        CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("资源预留完成，用户开始切换："));
                        flag=0;
                    }
                    else
                        CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("正在进行移动性切换："));
                    CFunctionLibrary::Log(CConstant::LOG_SERVER,str);
                    m_IDlocal=ptrQueue->bID;
                    m_IDaim=ptrQueue->bMoveaimID;
                    m_bswitch=TRUE;
                    m_pViewNRM->Invalidate();
               }
            }

			int location = ptrQueue->dNetCapacity[0];
			if(location<1 || location>10)
			{
				location = 1;
				ptrQueue->dNetCapacity[0] = 1;
			}
			else
			{
				ptrQueue->dNetCapacity[location] = ptrQueue->dBandUsed;
				ptrQueue->dNetLoad = 0.0;
				for(int m=0;m<10;m++)
				{
					ptrQueue->dNetLoad += ptrQueue->dNetCapacity[m];
				}
					ptrQueue->dNetLoad = ptrQueue->dNetLoad/10/21504;
				if(location == 10)
					ptrQueue->dNetCapacity[0] = 1;
				else
					ptrQueue->dNetCapacity[0] = location + 1;
			}
			ptrQueue->nDelay=nDelay;
			ptrQueue->m_bLoadBalanceFailure=loadbalanceFlag;
			for(int m=0;m<CConstant::BP_SAMPLE_LEN;m++)
			{
				if(m<CConstant::BP_SAMPLE_LEN-1)
					ptrQueue->predicSample[m]=ptrQueue->predicSample[m+1];
				else
					ptrQueue->predicSample[m]=ptrQueue->dBandUsed;
			}
			bRet=TRUE;

   /*         //zxn将存储的信息与最新的RNRM信息比较，然后再把最新的存储起来
            if(Time_1s==50)
            {
                Time_1s=0;
			if(bID==1)
            {
                if(m_pRNRM1.bUserUsed<bUserUsed)
                    m_pRNRM1.bUserchangeflag=1;
                else if(m_pRNRM1.bUserUsed>bUserUsed)//用户数比上一次减少
                    m_pRNRM1.bUserchangeflag=2;
                else if(m_pRNRM1.bUserUsed==bUserUsed)                                         //用户数不变
                    m_pRNRM1.bUserchangeflag=0;
                m_pRNRM1.bUserUsed=bUserUsed;//存储最新信息
                m_pRNRM1.bID=1;
                m_pRNRM1.banlanceflag=ptrQueue->m_bLoadBalancelocal;
            }
            if(bID==2)
            {
                if(m_pRNRM2.bUserUsed<bUserUsed)//网络用户数比上一次增加
                    m_pRNRM2.bUserchangeflag=1;
                else if(m_pRNRM2.bUserUsed>bUserUsed)//用户数比上一次减少
                    m_pRNRM2.bUserchangeflag=2;
                else                                          //用户数不变
                    m_pRNRM2.bUserchangeflag=0;
                m_pRNRM2.bUserUsed=bUserUsed;//存储最新信息
                m_pRNRM2.bID=2;
                m_pRNRM2.banlanceflag=ptrQueue->m_bLoadBalancelocal;
            }
            if(bID==3)
            {
                if(m_pRNRM3.bUserUsed<bUserUsed)//网络用户数比上一次增加
                    m_pRNRM3.bUserchangeflag=1;
                else if(m_pRNRM3.bUserUsed>bUserUsed)//用户数比上一次减少
                    m_pRNRM3.bUserchangeflag=2;
                else                                          //用户数不变
                    m_pRNRM3.bUserchangeflag=0;
                m_pRNRM3.bUserUsed=bUserUsed;//存储最新信息
                m_pRNRM3.bID=3;
                m_pRNRM3.banlanceflag=ptrQueue->m_bLoadBalancelocal;
            }
            }*/
			break;
		}
		i++;
		ptrQueue=ptrQueue->next;
	}
	if(flag1==1)////用户数变化更新界面
	{
	    m_pViewNRM->Invalidate();
	    flag1=0;
	}

	if(bRet==FALSE)
	{
		//ASSERT(FALSE);
		//ResRecover(bID);
		//测试：RNRM状态恢复
		CTypeLibrary::CRNRMQueue *ptr=m_pRNRMQueue;
		while(ptr->next)
			ptr=ptr->next;

		CTypeLibrary::CRNRMQueue * newRNRM=new CTypeLibrary::CRNRMQueue;
		ULONG ipRNRM=0;
		memcpy(&ipRNRM,&lpData[4],sizeof(int));
		newRNRM->bCount=4;
		newRNRM->bID=bID;
		//如果信道已被分配出去，则重新接入
		BOOL ChannelFlag=FALSE;
		if(m_setChannels2Alloc.count(nChan)!=0)
		{
			newRNRM->bChannel=nChan;
			m_setChannels2Alloc.erase(nChan);
		}
		else
		{
			ChannelFlag=TRUE;
			if(m_setChannels2Alloc.size()>0)
			{
				set<int>::iterator it=m_setChannels2Alloc.begin();
				int nOptimalChannel=*it,nOptimalMetric=0;
				for(;it!=m_setChannels2Alloc.end();it++)
				{
					int nChannelAva=m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(*it,CConstant::CHANNEL_AVAILABILITY);
					if(nChannelAva>nOptimalMetric)
					{
						nOptimalMetric=nChannelAva;
						nOptimalChannel=*it;
					}
				}
				ASSERT(nOptimalChannel!=0);
				nChan=nOptimalChannel;
				m_setChannels2Alloc.erase(nChan);
			}
			else
			{
				nChan=255;
				bID=INVALID_ADDR;
			}
		}

		memcpy(&newRNRM->bUserTotal,&bUserTotal,sizeof(BYTE));
		memcpy(&newRNRM->bUserUsed,&bUserUsed,sizeof(BYTE));
		memcpy(&newRNRM->bBandTotal,&bBandTotal,sizeof(BYTE));
		memcpy(&newRNRM->dBandUsed,&dBandUsed,sizeof(double));
		newRNRM->ulIPRNRM=ipRNRM;
		memcpy(&newRNRM->ulIPTRM,&ipTRM,sizeof(ULONG));
		memcpy(&newRNRM->nDelay,&nDelay,sizeof(int));

		memset(newRNRM->name,0,sizeof(newRNRM->name));
		int nTemp=0;
		memcpy(&nTemp,&lpData[nPos],sizeof(int));
		nPos+=sizeof(int);
		memcpy(newRNRM->name,&lpData[nPos],nTemp);
		nPos+=nTemp;

		for(int m=0;m<CConstant::BP_SAMPLE_LEN;m++)
		{
			newRNRM->predicSample[m]=0;
		}
		for(int n=0;n<CConstant::BP_SAMPLE_LEN-10;n++)
		{
			newRNRM->predictOutput[n]=0;
		}

		newRNRM->next=NULL;
		ptr->next=newRNRM;

		int nSrvNum=(nLen-nPos)/sizeof(CTypeLibrary::CRNRMServicePara);
		memcpy(&newRNRM->RNRMService.service,&lpData[nPos],sizeof(CTypeLibrary::CRNRMServicePara));
		nPos+=sizeof(CTypeLibrary::CRNRMServicePara);
		newRNRM->RNRMService.next=NULL;

		CTypeLibrary::CRNRMService *ptrSrvTail=&newRNRM->RNRMService,*ptrSrvNew=NULL;
		for(int i=1;i<nSrvNum;i++)
		{
			ptrSrvNew=new CTypeLibrary::CRNRMService;
			memcpy(&ptrSrvNew->service,&lpData[nPos],sizeof(CTypeLibrary::CRNRMServicePara));
			nPos+=sizeof(CTypeLibrary::CRNRMServicePara);
			ptrSrvNew->next=NULL;
			ptrSrvTail->next=ptrSrvNew;
			ptrSrvTail=ptrSrvNew;
		}

        loadblancelocal=lpData[nPos1];//zxn取负载均衡相应标志位
        nPos1+=sizeof(BOOL);
        loadblanceaim=lpData[nPos1];
        nPos1+=sizeof(BOOL);
        IDaim=lpData[nPos1];
        nPos1=nPos1+1;
        while (ptrQueue1)
        {
            if(ptrQueue1->bID==bID)
            {
                ptrQueue1->m_bLoadBalancelocal=loadblancelocal;
                ptrQueue1->m_bLoadBalanceaim=loadblanceaim;
                ptrQueue1->m_IDaim=IDaim;
            }
			ptrQueue1=ptrQueue1->next;
		}

		CTypeLibrary::CTopologyInfo info;
		memset(&info,0,sizeof(CTypeLibrary::CTopologyInfo));
		info.bID=bID;
		UINT nType=CConstant::RNRM_ADD_NEW;
		::SendMessage(m_pViewNRM->m_hWnd,CConstant::WM_RNRM_UPDATE,(WPARAM)&info,(LPARAM)nType);
		CString str;
		str.Format("RNRM recover : %s",newRNRM->name);
		CFunctionLibrary::Log(CConstant::LOG_SERVER,str);
		m_bRNRMNum++;

		if(ChannelFlag)
		{
			char buf[MAX_UDP_SIZE];
			memset(buf,0,sizeof(buf));
			nPos=0;
			nTemp=CConstant::COGNITIVE_RNRM_ACCESS;
			if(bID==INVALID_ADDR)
				nTemp=CConstant::COGNITIVE_RNRM_REJECT;

            CTypeLibrary::PacketHeadRt pktHead={0,0,nTemp,0};
			memcpy(&buf[nPos],&pktHead,sizeof(CTypeLibrary::PacketHeadRt));
			nPos+=sizeof(CTypeLibrary::PacketHeadRt);
			memcpy(&buf[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));
			nPos+=sizeof(ULONG);
			memcpy(&buf[nPos],&ipRNRM,sizeof(ULONG));
			nPos+=sizeof(ULONG)+sizeof(int);
			memcpy(&buf[nPos++],&bID,sizeof(BYTE));
			memcpy(&buf[nPos++],&nChan,sizeof(BYTE));
			memcpy(&buf[12],&nPos,sizeof(int));
			CFunctionLibrary::SendToRNRM(ipRNRM,buf,nPos);
			CFunctionLibrary::Log(CConstant::LOG_SERVER,"RNRM Accessed Recovery: "+GetRNRMDesc(newRNRM->bID));
		}
	}
	LeaveCriticalSection(&m_csRNRMQueue);
//	CNRMApp::BanlanceOutput();
	return TRUE;
}

BOOL CNRMApp::OnUSRPCogInfo(BYTE *lpData,int nLen)
{
	BOOL bRet=TRUE;
	m_pViewCognitive->UpdateChanInfo(&lpData[16],nLen-16);
	return bRet;
}

int GetLength(int index)
{
    int length=0;
    switch(index)
    {
    // / 以下信息均为1080p高清大小
    case 1:length=534475;break;     // / Billie
    case 2:length=398957;break;     // / NBA最前线
    case 3:length=465690;break;     // / Nobody
    case 4:length=253818;break;     // / Rolling In The Deep
    case 5:length=510101;break;     // / 浮夸-陈奕迅
    case 6:length=413882;break;     // / 冠军欧洲
    case 7:length=65865;break;      // / 红尘客栈-周杰伦
    case 8:length=114201;break;     // / 玖月奇迹-中国美
    case 9:length=61601;break;      // / 魔兽之亡灵序曲
    case 10:length=390643;break;    // / 松鼠、坚果和时间机器
    case 11:length=1932118;break;   // / 兄弟连（超过范围）
    case 12:length=2154261;break;   // / 王的盛宴（超过范围）
    case 13:length=228063;break;    // / 一千个伤心的理由-张学友
    case 14:length=342167;break;    // / 因为爱情-王菲
    }
    return length;
}

//char buf2[MAX_UDP_SIZE];
//memset(buf2,0,sizeof(buf2));
//int nPos2=0;
//int nTemp=CConstant::COGNITIVE_DOUBLELINK;
//
//CTypeLibrary::PacketHeadRt pktHead2= {0,0,nTemp,0};
//memcpy(&buf2[nPos2],&pktHead2,sizeof(CTypeLibrary::PacketHeadRt));  // / 包头
//nPos2+=sizeof(CTypeLibrary::PacketHeadRt);
//
//memcpy(&buf2[nPos2],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));      // / NRM的ip
//nPos2+=sizeof(ULONG);
//memcpy(&buf2[nPos2],&ipRNRM,sizeof(ULONG));                         // / RNRM的ip
//nPos2+=sizeof(ULONG)+sizeof(int);
//memcpy(&buf2[12],&nPos2,sizeof(int));
//CFunctionLibrary::SendToRNRM(ipRNRM,buf2,nPos2);
//CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("NRM发送双连接指令"));  // / 在窗口打印双连接发送指令log

BOOL CNRMApp::OnDoubleLink(BYTE *lpData,int nLen)      // / 接收连接请求，判断是否进行双连接，发送给RNRM是否进行双连接指令
{
	BOOL bRet=false;

    ULONG ipRNRM=0;
	memcpy(&ipRNRM,&lpData[4],sizeof(ULONG));      //源RNRMip
	ULONG ipTRM=0;
	memcpy(&ipTRM,&lpData[16],sizeof(ULONG));      //请求视频的TRMip
	int index=0;
//	index=10;
    memcpy(&index,&lpData[20],sizeof(int));        //请求视频序号(根据index确定视频大小，判断是否进行双连接传输)
    int length=GetLength(index);



	if(length>1000000)           // / 如果进行双连接，发送双连接指令给RNRM，格式为(包头+源RNRMip+目标RNRMip+TRM视频序号+TRMip)
//    if(1)
    {
//		CTypeLibrary::CRNRMQueue *ptrQueue=m_pRNRMQueue->next;              // / 找到dBandUsed最小的RNRM，将其ip地址记录下来发送给源RNRM
//        double minUserUsed=ptrQueue->dBandUsed;
        ULONG ObjipRNRM=6666;
////        while (ptrQueue)
////        {
////            if(ptrQueue->dBandUsed<=minUserUsed)
////            {
////                memcpy(&ObjipRNRM,&ptrQueue->ulIPRNRM,sizeof(ULONG));
////                minUserUsed=ptrQueue->dBandUsed;
////            }
////            ptrQueue=ptrQueue->next;
////        }

		char buf[MAX_UDP_SIZE];         // / NRM将双连接帧发送给RNRM
		memset(buf,0,sizeof(buf));
        int nPos=0;
        int nTemp=CConstant::COGNITIVE_DOUBLELINK;                           // / 包头(0-4位)
        CTypeLibrary::PacketHeadRt pktHead={0,0,nTemp,0};
        memcpy(&buf[nPos],&pktHead,sizeof(CTypeLibrary::PacketHeadRt));
        nPos+=sizeof(CTypeLibrary::PacketHeadRt);

        memcpy(&buf[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));        // / NRM的ip(4-8位)
		nPos+=sizeof(ULONG);
        memcpy(&buf[nPos],&ipRNRM,sizeof(ULONG));           // / 源RNRMip(8-12位)
        nPos+=sizeof(ULONG)+sizeof(int);
        memcpy(&buf[nPos],&ipTRM,sizeof(ULONG));            // / TRMip(16-20位)
        nPos+=sizeof(ULONG);
        memcpy(&buf[nPos],&index,sizeof(int));             // / 视频序号(20-24位)
        nPos+=sizeof(int);
        memcpy(&buf[nPos],&ObjipRNRM,sizeof(ULONG));        // / 目标RNRMip(24-28位)
        nPos+=sizeof(ULONG);
        memcpy(&buf[12],&nPos,sizeof(int));                 // / buf的长度(存放在第12-16位)

        CFunctionLibrary::SendToRNRM(ipRNRM,buf,nPos);
//        CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("NRM发送双连接指令"));  // / 在窗口打印双连接发送指令log
        bRet=true;
	}
	else						// / 如果不进行双连接，发送单连接指令给RNRM，格式为(包头+RNRMip+TRMip)
    {
        char buf[MAX_UDP_SIZE];
        memset(buf,0,sizeof(buf));
        int nPos=0;                     // / NRM将单连接帧发送给RNRM

        int nTemp=CConstant::COGNITIVE_SINGLELINK;                      // / 包头(0-4位)
        CTypeLibrary::PacketHeadRt pktHead={0,0,nTemp,0};
        memcpy(&buf[nPos],&pktHead,sizeof(CTypeLibrary::PacketHeadRt));
        nPos+=sizeof(CTypeLibrary::PacketHeadRt);

        memcpy(&buf[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));    // / NRM的ip(4-8位)
		nPos+=sizeof(ULONG);
        memcpy(&buf[nPos],&ipRNRM,sizeof(ULONG));                       // / RNRM的ip(8-12位)
        nPos+=sizeof(ULONG)+sizeof(int);
        memcpy(&buf[nPos],&ipTRM,sizeof(ULONG));                        // / TRM的ip(16-20位)
        nPos+=sizeof(ULONG);
        memcpy(&buf[nPos],&index,sizeof(int));                          // / 视频序号(20-24位)
        nPos+=sizeof(int);
        memcpy(&buf[12],&nPos,sizeof(int));                             // / buf的长度(存放在第12-16位)

        CFunctionLibrary::SendToRNRM(ipRNRM,buf,nPos);
//        CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("NRM发送单连接指令"));  // / 在窗口打印接收单连接指令log
        bRet=true;
    }

	return bRet;
}

BOOL CNRMApp::OnDoubleTransmission(BYTE *lpData,int nLen)
{
//    ::CreateThread(NULL,0,CFunctionLibrary::SingleTransmission,NULL,0,NULL);    //yangrui 单路传输数据包
    ::CreateThread(NULL,0,CFunctionLibrary::DoubleTransmission1,NULL,0,NULL);    //yangrui 双路传输数据包
	::CreateThread(NULL,0,CFunctionLibrary::DoubleTransmission2,NULL,0,NULL);    //yangrui 双路传输数据包

//    BYTE buf[MAX_UDP_SIZE];
//	int length=100000;
//	ULONG ulRNRM=0;
//	while (theApp.m_bRunning)
//	{
////	    CTypeLibrary::CRNRMQueue *ptrQueue=theApp.m_pRNRMQueue;//*ptrQueuePre=theApp.m_pRNRMQueue;
////		while(ptrQueue)
////		{
////			if(ptrQueue->bID==1)
////			{
////				ulRNRM=ptrQueue->ulIPRNRM;
////				break;
////            }
////			ptrQueue=ptrQueue->next;
////		}
//
//	    int index=1;
//	    if(index<length)
//        {
//            memset(buf,0,MAX_UDP_SIZE);
//            CTypeLibrary::PacketHeadRt pkt={0,0,CConstant::COGNITIVE_TRANSMISSION,0};
//            memcpy(buf,&pkt,sizeof(pkt));
//            int nPos=4;
//            memcpy(&buf[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));
//            nPos+=sizeof(ULONG);
//            ULONG ulRNRM=inet_addr("192.168.1.160");
//            memcpy(&buf[nPos],&ulRNRM,sizeof(ULONG));
//            nPos+=sizeof(ULONG)+sizeof(int);
//            memcpy(&buf[nPos],&index,sizeof(int));
//            nPos+=sizeof(int);
//            memcpy(&buf[12],&nPos,sizeof(int));
//            CFunctionLibrary::SendToRNRM(ulRNRM,buf,nPos);
////            SendToBroad(ulRNRM,buf,nPos);
//
//            Sleep(CConstant::PERIOD_TRANSMISSION);
//            CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("NRM单路数据包传输"));  // / 在窗口打印双连接发送指令log
//            index=index+1;
//        }
//	}
    return TRUE;
}

BOOL CNRMApp::OnParaTransMsgRecvd(BYTE *lpData,int nLen)
{
	BOOL bRet=TRUE;
	ULONG ulFrom,ulTo;
	int nPos=4;
	BYTE buf[MAX_UDP_SIZE];
	CTypeLibrary::CPacketHead packetHead={0,0,0,0};
	memcpy(&ulFrom,&lpData[nPos],sizeof(ULONG));
	nPos+=sizeof(ULONG);
	memcpy(&ulTo,&lpData[nPos],sizeof(ULONG));
	nPos+=sizeof(ULONG);
	memset(buf,0,MAX_UDP_SIZE);
	if(lpData[16]==CConstant::PARALLEL_TRANSMISSION_BEGIN)
	{
		theApp.m_bParaTransOn=TRUE;
		packetHead.type=CConstant::COGNITIVE_PARALLEL_TRANSMISSION;
		nPos=0;
		memcpy(&buf[nPos],&packetHead,sizeof(CTypeLibrary::CPacketHead));
		nPos+=sizeof(CTypeLibrary::CPacketHead);
		memcpy(&buf[nPos],&ulTo,sizeof(ULONG));
		nPos+=sizeof(ULONG);
		memcpy(&buf[nPos],&ulFrom,sizeof(ULONG));
		nPos+=sizeof(ULONG)+sizeof(int);
		buf[nPos++]=CConstant::PARALLEL_TRANSMISSION_BEGIN;

		EnterCriticalSection(&m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->m_csChanMetrics);
		vector<CTypeLibrary::CHANNELMETRICS> vecTemp;
		for(vector<CTypeLibrary::CHANNELMETRICS>::iterator it=m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->m_vecChanMetrics.begin();it!=m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->m_vecChanMetrics.end();it++)
		{
			if((*it).nChannelNum==1||(*it).nChannelNum==6||(*it).nChannelNum==11)
				vecTemp.push_back(*it);
		}
		std::sort(vecTemp.begin(),vecTemp.end(),CChannelMetricComparator(2));
		if(vecTemp.size()<3)
			return FALSE;
		BYTE nTop1Channel=vecTemp.at(0).nChannelNum;
		BYTE nTop2Channel=vecTemp.at(1).nChannelNum;
		LeaveCriticalSection(&m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->m_csChanMetrics);

		memcpy(&buf[nPos++],&nTop1Channel,sizeof(BYTE));
		theApp.m_nParaTransChanA=nTop1Channel;
		memcpy(&buf[nPos++],&nTop2Channel,sizeof(BYTE));
		theApp.m_nParaTransChanB=nTop2Channel;
		memcpy(&buf[12],&nPos,sizeof(int));
		CFunctionLibrary::SendToBroad(ulFrom,buf,nPos);
		theApp.m_ulParaTrans=ulFrom;
	}
	else if(lpData[16]==CConstant::PARALLEL_TRANSMISSION_END)
	{
		theApp.m_bParaTransOn=FALSE;
	}
	return bRet;
}

void CNRMApp::OnCheckParaTransInfo()
{
	if(m_bParaTransOn==FALSE||theApp.m_nParaTransChanA==0||theApp.m_nParaTransChanB==0)
		return;
	BOOL bRet=TRUE;
	ULONG ulFrom=theApp.m_Option.m_ulIPBroad,ulTo=theApp.m_ulParaTrans;
	int nPos=4;
	BYTE buf[MAX_UDP_SIZE];
	CTypeLibrary::CPacketHead packetHead={0,0,0,0};
	packetHead.type=CConstant::COGNITIVE_PARALLEL_TRANSMISSION;
	if(theApp.m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(theApp.m_nParaTransChanA,CConstant::CHANNEL_AVAILABILITY)<75&&theApp.m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(theApp.m_nParaTransChanB,CConstant::CHANNEL_AVAILABILITY)<75)
	{
		nPos=0;
		memcpy(&buf[nPos],&packetHead,sizeof(CTypeLibrary::CPacketHead));
		nPos+=sizeof(CTypeLibrary::CPacketHead);
		memcpy(&buf[nPos],&ulFrom,sizeof(ULONG));
		nPos+=sizeof(ULONG);
		memcpy(&buf[nPos],&ulTo,sizeof(ULONG));
		nPos+=sizeof(ULONG)+sizeof(int);
		buf[nPos++]=CConstant::PARALLEL_TRANSMISSION_DOUBLECHANNELINFO;
		memcpy(&buf[nPos++],&theApp.m_nParaTransChanA,sizeof(BYTE));
		memcpy(&buf[nPos++],&theApp.m_nParaTransChanB,sizeof(BYTE));
		memcpy(&buf[12],&nPos,sizeof(int));
		CFunctionLibrary::SendToBroad(ulTo,buf,nPos);
	}
	else
	{
		int nBetterChannel=theApp.m_nParaTransChanA;
		if(theApp.m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(theApp.m_nParaTransChanA,CConstant::CHANNEL_AVAILABILITY)<theApp.m_pMainFrame->m_pArubaManagerDlg->m_pArubaManager->GetChannelMetric(theApp.m_nParaTransChanB,CConstant::CHANNEL_AVAILABILITY))
			nBetterChannel=theApp.m_nParaTransChanB;
		nPos=0;
		memcpy(&buf[nPos],&packetHead,sizeof(CTypeLibrary::CPacketHead));
		nPos+=sizeof(CTypeLibrary::CPacketHead);
		memcpy(&buf[nPos],&ulFrom,sizeof(ULONG));
		nPos+=sizeof(ULONG);
		memcpy(&buf[nPos],&ulTo,sizeof(ULONG));
		nPos+=sizeof(ULONG)+sizeof(int);
		buf[nPos++]=CConstant::PARALLEL_TRANSMISSION_SINGLECHANNELINFO;
		memcpy(&buf[nPos++],&nBetterChannel,sizeof(BYTE));
		memcpy(&buf[12],&nPos,sizeof(int));
		CFunctionLibrary::SendToBroad(ulTo,buf,nPos);
	}
}

BOOL CNRMApp::ResRecover(BYTE bId,BYTE bChan)
{
	m_pRNRMIDs[bId-1]=0;
	if(m_setChannels2Alloc.count(bChan)==0)
		m_setChannels2Alloc.insert(bChan);
	CTypeLibrary::CRNRMQueue *ptrQueuePre=m_pRNRMQueue,*ptrQueue=m_pRNRMQueue->next;
	while(ptrQueue!=NULL)
	{
		if(ptrQueue->bID==bId)
		{
			CFunctionLibrary::Log(CConstant::LOG_SERVER,"RNRM Quit: "+GetRNRMDesc(ptrQueue->bID));
			ptrQueuePre->next=ptrQueue->next;
			CTypeLibrary::CTopologyInfo info;
			info.bID=ptrQueue->bID;
			delete ptrQueue;
			::SendMessage(theApp.m_pViewNRM->m_hWnd,CConstant::WM_RNRM_UPDATE,(WPARAM)&info,(LPARAM)CConstant::RNRM_DROP_INVALID);
			ptrQueue=ptrQueuePre->next;
			m_bRNRMNum--;
			break;
		}
		ptrQueuePre=ptrQueue;
		ptrQueue=ptrQueuePre->next;
	}
	return CConstant::ERROR_UNKNOWN;
}

void CNRMApp::OnCognitiveView()
{
	m_pFirstView =((CFrameWnd*)m_pMainWnd)->GetActiveView();
	if(m_pFirstView==m_pViewCognitive)
		return ;
	UINT temp = ::GetWindowLong(m_pViewCognitive->m_hWnd, GWL_ID);
	::SetWindowLong(m_pViewCognitive->m_hWnd, GWL_ID, ::GetWindowLong(m_pFirstView->m_hWnd, GWL_ID));
	::SetWindowLong(m_pFirstView->m_hWnd, GWL_ID, temp);
	m_pFirstView->ShowWindow(SW_HIDE);
	((CFrameWnd*)m_pMainWnd)->SetActiveView(m_pViewCognitive);
	((CFrameWnd*)m_pMainWnd)->RecalcLayout();
	m_pViewCognitive->ShowWindow(SW_SHOW);
	m_pViewCognitive->Invalidate();
}

void CNRMApp::OnRNRMView()
{
	m_pFirstView =((CFrameWnd*)m_pMainWnd)->GetActiveView();
	if(m_pFirstView==m_pViewNRM)
		return ;
	UINT temp = ::GetWindowLong(m_pViewNRM->m_hWnd, GWL_ID);
	::SetWindowLong(m_pViewNRM->m_hWnd, GWL_ID, ::GetWindowLong(m_pFirstView->m_hWnd, GWL_ID));
	::SetWindowLong(m_pFirstView->m_hWnd, GWL_ID, temp);
	m_pFirstView->ShowWindow(SW_HIDE);
	((CFrameWnd*)m_pMainWnd)->SetActiveView(m_pViewNRM);
	((CFrameWnd*)m_pMainWnd)->RecalcLayout();
	m_pViewNRM->ShowWindow(SW_SHOW);
	m_pViewNRM->Invalidate();
}

void CNRMApp::OnPredictionView()
{
	m_pFirstView =((CFrameWnd*)m_pMainWnd)->GetActiveView();
	if(m_pFirstView==m_pViewPrediction)
		return ;
	UINT temp = ::GetWindowLong(m_pViewPrediction->m_hWnd, GWL_ID);
	::SetWindowLong(m_pViewPrediction->m_hWnd, GWL_ID, ::GetWindowLong(m_pFirstView->m_hWnd, GWL_ID));
	::SetWindowLong(m_pFirstView->m_hWnd, GWL_ID, temp);
	m_pFirstView->ShowWindow(SW_HIDE);
	((CFrameWnd*)m_pMainWnd)->SetActiveView(m_pViewPrediction);
	((CFrameWnd*)m_pMainWnd)->RecalcLayout();
	m_pViewPrediction->ShowWindow(SW_SHOW);
	m_pViewPrediction->Invalidate();
}

CString CNRMApp::GetRNRMDesc(BYTE bId)
{
	CString strDesc;
	CTypeLibrary::CRNRMQueue *ptr=m_pRNRMQueue->next;
	while(ptr->next)
		ptr=ptr->next;
	BOOL bRet=FALSE;
	while(ptr)
	{
		if(ptr->bID==bId)
		{
			bRet=TRUE;
			break;
		}
		ptr=ptr->next;
	}
	if(bRet==TRUE)
	{
		in_addr addr1;
		in_addr addr2;
		addr1.s_addr=ptr->ulIPRNRM;
		addr2.s_addr=ptr->ulIPTRM;
		strDesc.Format("ID %d, Channel %d, User %d,Name %s, Delay %d.",ptr->bID,ptr->bChannel,
			ptr->bUserUsed,ptr->name,ptr->nDelay,inet_ntoa(addr1),inet_ntoa(addr2));
	}
	return strDesc;
}

void CNRMApp::OnLoadBalance()
{
/*
	CMenu menuWnd;
	CMenu *subMenu;
	UINT tempState;
	menuWnd.LoadMenu(IDR_MAINFRAME);
	subMenu=menuWnd.GetSubMenu(1);

	UINT state=subMenu->GetMenuState(ID_COGNITIVE_LOADBALANCE,MF_BYCOMMAND);
	ASSERT(state != 0xFFFFFFFF);

	if(state==MF_UNCHECKED)
	{
		tempState=subMenu->CheckMenuItem(ID_COGNITIVE_LOADBALANCE,MF_CHECKED | MF_BYCOMMAND);
		m_bLoadBalance=TRUE;
	}
	else if(state==MF_CHECKED)
	{
		tempState=subMenu->CheckMenuItem(ID_COGNITIVE_LOADBALANCE,MF_UNCHECKED | MF_BYCOMMAND);
		m_bLoadBalance=FALSE;
	}
*/
	if(!m_bLoadBalance)
		m_bLoadBalance=TRUE;
	else
		m_bLoadBalance=FALSE;
/*
	m_pViewNRM->m_dlgGridCtrlRNRM.RefreshRNRMInfos();
	double dAverageBandwidth=0.0;
	int nSize=0;
	CTypeLibrary::CRNRMQueue *pQueue=theApp.m_pRNRMQueue->next;
	while(pQueue)
	{
		dAverageBandwidth+=pQueue->dBandUsed;
		nSize++;
		pQueue=pQueue->next;
	}
	if(nSize<=0)
		return;
	dAverageBandwidth=dAverageBandwidth/nSize;
	pQueue=theApp.m_pRNRMQueue->next;
	while(pQueue)
	{
		BYTE buffer[MAX_BUFFER_SIZE];
		int nPos=0;
		double dLoadBalanced=pQueue->dBandUsed-dAverageBandwidth+10;
		CTypeLibrary::PacketHeadRt pkt={0,0,CConstant::COGNITIVE_LOAD_BALANCE,0};
		memcpy(&buffer[nPos],&pkt,sizeof(pkt));
		nPos+=sizeof(pkt);
		memcpy(&buffer[nPos],&theApp.m_Option.m_ulIPNRM,sizeof(ULONG));
		nPos+=sizeof(ULONG);
		ULONG ulIPRNRM=pQueue->ulIPRNRM;
		memcpy(&buffer[nPos],&ulIPRNRM,sizeof(ULONG));
		nPos+=sizeof(ULONG)+sizeof(int);
		memcpy(&buffer[nPos],&dLoadBalanced,sizeof(double));
		nPos+=sizeof(double);
		memcpy(&buffer[12],&nPos,sizeof(int));
		CFunctionLibrary::SendToRNRM(ulIPRNRM,buffer,nPos);
		pQueue=pQueue->next;
	}
*/
}

void CNRMApp::OnReservation()
{
	if(!m_bReservation)
		m_bReservation=TRUE;
	else
		m_bReservation=FALSE;
}

void CNRMApp::OnServPrediction()
{
	if(!m_bServPrediction)
		m_bServPrediction=TRUE;
	else
		m_bServPrediction=FALSE;
}

void CNRMApp::OnUpdataLoadBalance(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_bLoadBalance);
}

void CNRMApp::OnUpdataReservation(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_bReservation);
}

void CNRMApp::OnUpdataServPrediction(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_bServPrediction);
}


void CNRMApp::BanlanceOutput()   //zxn
{
 /*  if(m_pRNRM1.bUserchangeflag==2)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r1减少"));
    if(m_pRNRM1.bUserchangeflag==0)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r1不变"));
    if(m_pRNRM1.bUserchangeflag==1)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r1增加"));
   if(m_pRNRM2.bUserchangeflag==2)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r2减少"));
    if(m_pRNRM2.bUserchangeflag==0)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r2不变"));
    if(m_pRNRM2.bUserchangeflag==1)
         CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("r2增加"));*/
 /*//负载均衡、移动性切换、资源预留判断
    if((m_pRNRM1.bUserchangeflag==2||m_pRNRM2.bUserchangeflag==2||m_pRNRM3.bUserchangeflag==2)&&(m_pRNRM1.bUserchangeflag==1||m_pRNRM2.bUserchangeflag==1||m_pRNRM3.bUserchangeflag==1))
    {
       // if(m_pRNRM1.banlanceflag==TRUE||m_pRNRM2.banlanceflag==TRUE||m_pRNRM3.banlanceflag==TRUE)
       if(m_bLoadBalanceflag==TRUE)
       {
            CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("进行了负载均衡"));
            m_bLoadBalanceflag=FALSE;
       }
        else
             CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("进行移动性切换"));
    }*/

//判断RNRM是否进行了负载均衡，画箭头zxn
        CTypeLibrary::CRNRMQueue *pQueue1=theApp.m_pRNRMQueue->next;
        CString strbanlIDlocal,strbanlIDaim,strban;
        while(pQueue1)
        {
            int j=0;
         //   CTypeLibrary::CRNRMQueue *pQueue2=theApp.m_pRNRMQueue->next;
           //测试用 if(pQueue1->m_bLoadBalancelocal==TRUE&&pQueue1->m_bLoadBalanceaim==TRUE)
           if(pQueue1->m_bLoadBalancelocal==TRUE)
            {
                int i=0;
         // /       CTypeLibrary::CRNRMQueue *pQueue2=theApp.m_pRNRMQueue->next;
         // /      while(pQueue2)
          // /      {
                   // if((pQueue2->bID==pQueue1->m_IDaim)&&(pQueue2->m_bLoadBalanceaim==TRUE))
          // /          {
                               // myGrapics.DrawLine(ptAP[j].X,ptAP[j].Y,ptAP[i].X,ptAP[i].Y);


                        strban.Format("ID:%d NetName:%s ",pQueue1->bID,pQueue1->name);
                        strban+="  TO  ";
                        strbanlIDaim.Format("ID:%d",pQueue1->m_IDaim);
                        strban+=strbanlIDaim;
                        CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("负载均衡开始："));//+_T("子网：")+(CString)(pQueue1->bID)+_T("子网：")+(CString)(pQueue2->bID)));
                        CFunctionLibrary::Log(CConstant::LOG_SERVER,strban);
                        pQueue1->m_bLoadBalancelocal=FALSE;
                  // /      strbanlIDlocal=pQueue1->bID;
                  // /      strbanlIDaim=pQueue1->m_IDaim;
                    //    CFunctionLibrary::Log(CConstant::LOG_SERVER,"网络ID1To"+"网络ID2");
                        //CFunctionLibrary::Log(CConstant::LOG_SERVER,theApp.GetRNRMDesc(pQueue1->bID)+" To "+theApp.GetRNRMDesc(pQueue2->bID));
                    //   CFunctionLibrary::Log(CConstant::LOG_SERVER,strbanlIDlocal+" To "+strbanlIDaim);
         // /           }
          // /          pQueue2=pQueue2->next;
                }
          //  }
               else if(pQueue1->m_bLoadBalancelocal==FALSE)
                {
                    strbanlIDaim.Format("ID:%d",pQueue1->m_IDaim);
                    CFunctionLibrary::Log(CConstant::LOG_SERVER,strbanlIDaim);
                }
                 pQueue1=pQueue1->next;

           }
         //   else if(pQueue1->m_bLoadBalancelocal==FALSE)
          //      CFunctionLibrary::Log(CConstant::LOG_SERVER,_T("负载均衡结束："));
      // /      pQueue1=pQueue1->next;
    // /    }*/
}

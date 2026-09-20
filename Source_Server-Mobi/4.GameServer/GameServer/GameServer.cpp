#include "stdafx.h"
#include "Resource.h"
#include "VoiceChat.h"
#include "BloodCastle.h"
#include "CastleDeep.h"
#include "CastleSiege.h"
#include "CashShop.h"
#include "ChaosBox.h"
#include "ChaosCastle.h"
#include "Crywolf.h"
#include "CustomArena.h"
#include "CustomEventDrop.h"
#include "CustomOnlineLottery.h"
#include "CustomQuiz.h"
#include "DevilSquare.h"
#include "EventTvT.h"
#include "GameServer.h"
#include "GameMain.h"
#include "IllusionTemple.h"
#include "InvasionManager.h"
#include "JSProtocol.h"
#include "Message.h"
#include "MiniDump.h"
#include "Notice.h"
#include "Protect.h"
#include "QueueTimer.h"
#include "ServerDisplayer.h"
#include "ServerInfo.h"
#include "SocketManager.h"
#include "SocketManagerUdp.h"
#include "ThemidaSDK.h"
#include "Util.h"
#include "ReiDoMU.h"
#include "IpManager.h"
#include "Log.h"
#include "CustomAttack.h"
#include "CustomStore.h"
#include "OfflineMode.h"
#include "BossGuild.h"
#include "CTCMini.h"
#include "BattleSurvivor.h"
#include "FakeOnline.h"
#include "MapManager.h"
#include "MonsterManager.h"
#include "ItemOptionRate.h"
#include "ItemDrop.h"
#include "ItemLevel.h"

TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];
HINSTANCE hInst;
HWND hWnd;
HWND hWndComboBox;
HWND hWndComboBox1;
int Conectar = 0;


int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) // OK
{
	/*if(gProtect.ReadMainFile("..\\Data\\Hack\\keyword.enc") == 0)
	{
		MessageBox(0,"Licencia not found or invalid!","Error",MB_OK | MB_ICONERROR);
		ExitProcess(0);
	}*/

	VM_START

	CMiniDump::Start();

	LoadString(hInstance,IDS_APP_TITLE,szTitle,MAX_LOADSTRING);
	LoadString(hInstance,IDC_GAMESERVER,szWindowClass,MAX_LOADSTRING);

	MyRegisterClass(hInstance);

	if(InitInstance(hInstance,nCmdShow) == 0)
	{
		return 0;
	}

	SetLargeRand();

	gServerInfo.ReadStartupInfo("GameServerInfo",".\\Data\\GameServerInfo - Common.ini");

	#if(PROTECT_STATE==1)

	#if(GAMESERVER_UPDATE>=801)
		//gProtect.StartAuth(AUTH_SERVER_TYPE_S8_GAME_SERVER);
	#elif(GAMESERVER_UPDATE>=601)
		//gProtect.StartAuth(AUTH_SERVER_TYPE_S6_GAME_SERVER);
	#elif(GAMESERVER_UPDATE>=401)
		//gProtect.StartAuth(AUTH_SERVER_TYPE_S4_GAME_SERVER);
	#else
		//gProtect.StartAuth(AUTH_SERVER_TYPE_S2_GAME_SERVER);
	#endif

	#endif

	char buff[256];

	wsprintf(buff,"[%s] %s (ON: %d) %s",GAMESERVER_VERSION,gServerInfo.m_ServerName, gObjTotalUser,GAMESERVER_CLIENT);

	SetWindowText(hWnd,buff);

	gServerDisplayer.Init(hWnd);

	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2,2),&wsa) == 0)
	{
		if(gSocketManager.Start((WORD)gServerInfo.m_ServerPort) == 0)
		{
			LogAdd(LOG_RED,"Could not start KYANa Emulator");
		}
		else
		{
			GameMainInit(hWnd);

			JoinServerConnect(WM_JOIN_SERVER_MSG_PROC);

			DataServerConnect(WM_DATA_SERVER_MSG_PROC);

			gSocketManagerUdp.Connect(gServerInfo.m_ConnectServerAddress,(WORD)gServerInfo.m_ConnectServerPort);

			SetTimer(hWnd,WM_TIMER_1000,1000,0);

			SetTimer(hWnd,WM_TIMER_10000,10000,0);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER,100,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_MOVE,100,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_AI,100,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_AI_MOVE,100,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_EVENT,100,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_VIEWPORT,1000,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_FIRST,1000,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_CLOSE,1000,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MATH_AUTHENTICATOR,10000,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_ACCOUNT_LEVEL,60000,&QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_PICK_COMMAND,6000,&QueueTimerCallback);

			// 1Hz. Fast enough that walking into earshot is heard within a
			// second, and trivial next to the position traffic the server
			// already sends every client many times per second.
			gVoiceChat.Init();

			gQueueTimer.CreateTimer(QUEUE_TIMER_VOICE_CHAT,1000,&QueueTimerCallback);
		}
	}
	else
	{
		LogAdd(LOG_RED,"WSAStartup() failed with error: %d",WSAGetLastError());
	}

	gServerDisplayer.PaintAllInfo();

	gServerDisplayer.PaintName();

	SetTimer(hWnd,WM_TIMER_2000,2000,0);

	HACCEL hAccelTable = LoadAccelerators(hInstance,(LPCTSTR)IDC_GAMESERVER);

	MSG msg;

	while(GetMessage(&msg,0,0,0) != 0)
	{
		if(TranslateAccelerator(msg.hwnd,hAccelTable,&msg) == 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	CMiniDump::Clean();

	VM_END

	return msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) // OK
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance,(LPCTSTR)IDI_GAMESERVER);
	wcex.hCursor = LoadCursor(0,IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = (LPCSTR)IDC_GAMESERVER;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance,(LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance,int nCmdShow) // OK
{
	hInst = hInstance;

	// WS_CLIPCHILDREN is required by the log scrollbar: this window has no
	// WM_PAINT handler and instead paints imperatively through GetDC on a
	// 2-second timer, including a FillRect over the whole client area. Without
	// clipping, that fill reaches through the parent DC and blanks any child
	// control sitting in the client area until its next repaint - the status bar
	// only survives it because SetWindowName re-sends SB_SETPARTS/SB_SETTEXT
	// every cycle, and a scrollbar has no equivalent self-refresh.
	hWnd = CreateWindow(szWindowClass,szTitle,WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CLIPCHILDREN,CW_USEDEFAULT,0,980,750,0,0,hInstance,0);

	if(hWnd == 0)
	{
		return 0;
	}

	ShowWindow(hWnd,nCmdShow);
	UpdateWindow(hWnd);
	return 1;
}

// Relabels the Tools > Invasion menu from InvasionManager.dat's per-index
// EventName column (loaded into INVASION_INFO::AlertMessage by
// CInvasionManager::Load) instead of the "Start - <name>" strings that used
// to be baked into GameServer.rc at compile time. Renaming an event in the
// .dat and using "Reload > Reload Event" used to require a rebuild to see
// the new name reflected here; now it does not.
//
// Only IDM_INVASION0..16 have a real MENUITEM in GameServer.rc (17-21 are
// commented out there, despite Resource.h and the WM_COMMAND switch both
// still covering the full 0-21 range), so that is the range relabelled here.
// The submenu is found by content rather than by a hardcoded position in the
// top-level menu, so inserting or reordering another POPUP in GameServer.rc
// ahead of "Invasion" cannot silently point this at the wrong menu.
void RefreshInvasionMenu(HWND hwnd)
{
	static const int InvasionMenuId[] =
	{
		IDM_INVASION0, IDM_INVASION1, IDM_INVASION2, IDM_INVASION3,
		IDM_INVASION4, IDM_INVASION5, IDM_INVASION6, IDM_INVASION7,
		IDM_INVASION8, IDM_INVASION9, IDM_INVASION10, IDM_INVASION11,
		IDM_INVASION12, IDM_INVASION13, IDM_INVASION14, IDM_INVASION15,
		IDM_INVASION16,
	};

	const int InvasionMenuCount = sizeof(InvasionMenuId)/sizeof(InvasionMenuId[0]);

	HMENU hMenu = GetMenu(hwnd);

	if(hMenu == 0)
	{
		return;
	}

	HMENU hInvasionMenu = 0;

	for(int n=0;n < GetMenuItemCount(hMenu);n++)
	{
		HMENU hSub = GetSubMenu(hMenu,n);

		if(hSub != 0 && GetMenuItemID(hSub,0) == IDM_INVASION0)
		{
			hInvasionMenu = hSub;
			break;
		}
	}

	if(hInvasionMenu == 0)
	{
		return;
	}

	for(int n=0;n < InvasionMenuCount;n++)
	{
		char* lpEventName = gInvasionManager.m_InvasionInfo[n].AlertMessage;

		if(strlen(lpEventName) == 0)
		{
			continue;
		}

		char szMenuText[160];

		wsprintf(szMenuText,"Start - %s",lpEventName);

		ModifyMenuA(hInvasionMenu,InvasionMenuId[n],MF_BYCOMMAND | MF_STRING,InvasionMenuId[n],szMenuText);
	}

	DrawMenuBar(hwnd);
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam) // OK
{

	const char ClassName[] = "MainWindowClass";

	HWND hWndStatusBar;

	switch(message)
	{

		case WM_CREATE:

		{

            hWndStatusBar = CreateWindowEx(

            0,

            STATUSCLASSNAME,

            NULL,

            WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|CCS_BOTTOM,

            0,

            0,

            0,

            0,

            hWnd,

            (HMENU)IDC_STATUSBAR,

            (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE),

            NULL);

            int iStatusWidths[] = {190,270,360,450,580, -1};

            char text[256];

            SendMessage(hWndStatusBar, SB_SETPARTS, 6, (LPARAM)iStatusWidths);

			wsprintf(text, "KYANa Emulator %s - Update %d ", GAMESERVER_NAME, UPDATE_GAMING);

            SendMessage(hWndStatusBar, SB_SETTEXT, 0,(LPARAM)text);

			wsprintf(text, "OffStore: %d", gObjOffStore);

            SendMessage(hWndStatusBar, SB_SETTEXT, 1,(LPARAM)text);

			wsprintf(text, "OffAttack: %d", gObjOffAttack);

            SendMessage(hWndStatusBar, SB_SETTEXT, 2,(LPARAM)text);

			wsprintf(text, "Bots Buffer: %d", gObjTotalBot);

            SendMessage(hWndStatusBar, SB_SETTEXT, 3,(LPARAM)text);

			wsprintf(text, "Monsters: %d/%d", gObjTotalMonster,MAX_OBJECT_MONSTER);

            SendMessage(hWndStatusBar, SB_SETTEXT, 4,(LPARAM)text);

			SendMessage(hWndStatusBar, SB_SETTEXT, 5,(LPARAM)NULL);

            ShowWindow(hWndStatusBar, SW_SHOW);

			// Scrollbar for the main log panel. Positioned and given its range
			// by gServerDisplayer.UpdateLogScrollBar(), which runs on every
			// paint - here it is only created, since the client size is not
			// final yet and the displayer has not been Init'd with the hwnd.
			CreateWindowEx(0,"SCROLLBAR",NULL,WS_CHILD|WS_VISIBLE|SBS_VERT,
				0,0,0,0,hWnd,(HMENU)IDC_LOG_SCROLLBAR,
				(HINSTANCE)GetWindowLong(hWnd,GWL_HINSTANCE),NULL);

		}
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDM_ONLINEUSERS:
					DialogBox(hInst,(LPCTSTR)IDD_ONLINEUSER,hWnd,(DLGPROC)UserOnline);
					break;
				case IDM_MAPMONSTERINFO:
					DialogBox(hInst,(LPCTSTR)IDD_MAPMONSTERINFO,hWnd,(DLGPROC)MapMonsterInfo);
					break;
				case IDM_ABOUT:
					DialogBox(hInst,(LPCTSTR)IDD_ABOUTBOX,hWnd,(DLGPROC)About);
					break;
				case IDM_EXIT:
					if(MessageBox(0,"Are you sure to terminate KYANa Emulator?","Ask terminate server",MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						DestroyWindow(hWnd);
					}
					break;
				case IDM_FILE_ALLUSERLOGOUT:
					gObjAllLogOut();
					break;
				case IDM_FILE_ALLUSERDISCONNECT:
					gObjAllDisconnect();
					break;
				case IDM_FILE_1MINUTESERVERCLOSE:
					if(gCloseMsg == 0)
					{
						gCloseMsg = 1;
						gCloseMsgTime = 60;
						gNotice.GCNoticeSendToAll(0,0,0,0,0,0,gMessage.GetMessage(487));
					}
					break;
				case IDM_FILE_3MINUTESERVERCLOSE:
					if(gCloseMsg == 0)
					{
						gCloseMsg = 1;
						gCloseMsgTime = 180;
						gNotice.GCNoticeSendToAll(0,0,0,0,0,0,gMessage.GetMessage(488));
					}
					break;
				case IDM_FILE_5MINUTESERVERCLOSE:
					if(gCloseMsg == 0)
					{
						gCloseMsg = 1;
						gCloseMsgTime = 300;
						gNotice.GCNoticeSendToAll(0,0,0,0,0,0,gMessage.GetMessage(489));
					}
					break;
//==================================================================================================================================================
//FakeOnline_EMU
#if USE_FAKE_ONLINE == TRUE

				case ID_FAKEONLINE_RELOADDATA:
					s_FakeOnline.LoadFakeData(".\\AutoTrain.xml");
					break;
				case ID_FAKEONLINE_ADDFAKEONLINE:
				{
													if (gJoinServerConnection.CheckState() != 0 && gDataServerConnection.CheckState() != 0)
													{
														s_FakeOnline.RestoreFakeOnline();
														//s_FakeOnline.AccountsRestored = 2;
													}

													/*
													if (s_FakeOnline.AccountsRestored == 0)
													{
													s_FakeOnline.AccountsRestored = 1;
													}*/
				}
					break;
				case ID_FAKEONLINE_DELFAKEONLINE:
				{
													for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
													{
														if (gObjIsConnectedGP(n) != 0 && gObj[n].IsFakeOnline)
														{
															s_FakeOnline.OnAttackAlreadyConnected(&gObj[n]);

														}
													}
				}
					break;
#endif
//==================================================================================================================================================
				case IDM_RELOAD_RELOADCASHSHOP:
					gServerInfo.ReadCashShopInfo();
					// ReadCashShopInfo() only re-reads the legacy CashShopPackage/Product.txt
					// files and local ini settings - it never touches the DB-backed sellable
					// catalog (m_CashShopPackageInfo), which was only ever refreshed on the
					// DataServer handshake or a detected catalog-version change. That's the
					// "item shows on the shelf but buying it says not in stock until the
					// GameServer restarts" bug - force the same refresh a version change
					// already triggers automatically, so this reload actually fixes it.
					gCashShop.GDCashShopCatalogReqSend();
					break;
				case IDM_RELOAD_RELOADCHAOSMIX:
					gServerInfo.ReadChaosMixInfo();
					#if(CB_GETMIXRATE)
					gChaosBox.RefreshAllOpenMixRates();
					#endif
					break;
				case IDM_RELOAD_RELOADCHARACTER:
					gServerInfo.ReadCharacterInfo();
					break;
				case IDM_RELOAD_RELOADCOMMAND:
					gServerInfo.ReadCommandInfo();
					break;
				case IDM_RELOAD_RELOADCOMMON:
					gServerInfo.ReadCommonInfo();
					#if(CB_GETMIXRATE)
					gChaosBox.RefreshAllOpenMixRates();
					#endif
					break;
				case IDM_RELOAD_RELOADCUSTOM:
					gServerInfo.ReadCustomInfo();
					break;
				case IDM_RELOAD_RELOADEVENT:
					gServerInfo.ReadEventInfo();
					RefreshInvasionMenu(hWnd);
					break;
				case IDM_RELOAD_RELOADEVENTITEMBAG:
					gServerInfo.ReadEventItemBagInfo();
					break;
				case IDM_RELOAD_RELOADHACK:
					gServerInfo.ReadHackInfo();
					break;
				case IDM_RELOAD_RELOADITEM:
					gServerInfo.ReadItemInfo();
					break;
				case IDM_RELOAD_RELOADMESSAGE:
					gServerInfo.ReadMessageInfo();
					break;
				case IDM_RELOAD_RELOADMONSTER:
					gServerInfo.ReloadMonsterInfo();
					break;
				case IDM_RELOAD_RELOADMAP:
					gServerInfo.ReloadMapManagerInfo();
					break;
				case IDM_RELOAD_RELOADMOVE:
					gServerInfo.ReadMoveInfo();
					break;
				case IDM_RELOAD_RELOADNOTICE:
					gServerInfo.ReadNoticeInfo();
					break;
				case IDM_RELOAD_RELOADQUEST:
					gServerInfo.ReadQuestInfo();
					break;
				case IDM_RELOAD_RELOADSHOP:
					gServerInfo.ReadShopInfo();
					break;
				case IDM_RELOAD_RELOADSKILL:
					gServerInfo.ReadSkillInfo();
					break;
				case IDM_RELOAD_RELOADUTIL:
					gServerInfo.ReadUtilInfo();
					break;
				case IDM_RELOAD_RELOADBOTS: //MC bots
					gServerInfo.ReloadBotInfo(); //MC bots
					break;
				case IDM_RELOAD_RELOADALL:
					gServerInfo.ReloadAll();
					RefreshInvasionMenu(hWnd);
					#if(CB_GETMIXRATE)
					gChaosBox.RefreshAllOpenMixRates();
					#endif
					gCashShop.GDCashShopCatalogReqSend();
					break;
				case IDM_STARTONLINELOTTERY_LORENCIA:
					gCustomOnlineLottery.StartNow(0);
					break;
				case IDM_STARTONLINELOTTERY_DEVIAS:
					gCustomOnlineLottery.StartNow(2);
					break;
				case IDM_STARTONLINELOTTERY_NORIA:
					gCustomOnlineLottery.StartNow(3);
					break;
				case IDM_STARTONLINELOTTERY_ELBELAND:
					gCustomOnlineLottery.StartNow(51);
					break;
				case IDM_STARTBC:
					gBloodCastle.StartBC();
					break;
				case IDM_STARTDS:
					gDevilSquare.StartDS();
					break;
				case IDM_STARTCC:
					gChaosCastle.StartCC();
					break;
				case IDM_STARTIT:
					gIllusionTemple.StartIT();
					break;
				case IDM_STARTQUIZ:
					gCustomQuiz.StartQuiz();
					break;
				case IDM_STARTDROP:
					gCustomEventDrop.StartDrop();
					break;
				case IDM_STARTKING:
					gReiDoMU.StartKing();
					break;
				case IDM_STARTTVT:
					gTvTEvent.StartTvT();
					break;
#if(BOSS_GUILD == 1)
				case IDM_EVENTS_BOSSGUILD:
					gBossGuild.StartBossGuild();
					break;
#endif
#if(CTCMINI)
				case IDM_EVENTS_CTCMINI: gCTCMini.StartCTCMini();			break;
#endif
#if	BsvEvent
				case IDM_STARTBSV:
					gBsVEvent.StartBSV();
					break;
					#endif
				case IDM_INVASION0:
					gInvasionManager.StartInvasion(0);
					break;
				case IDM_INVASION1:
					gInvasionManager.StartInvasion(1);
					break;
				case IDM_INVASION2:
					gInvasionManager.StartInvasion(2);
					break;
				case IDM_INVASION3:
					gInvasionManager.StartInvasion(3);
					break;
				case IDM_INVASION4:
					gInvasionManager.StartInvasion(4);
					break;
				case IDM_INVASION5:
					gInvasionManager.StartInvasion(5);
					break;
				case IDM_INVASION6:
					gInvasionManager.StartInvasion(6);
					break;
				case IDM_INVASION7:
					gInvasionManager.StartInvasion(7);
					break;
				case IDM_INVASION8:
					gInvasionManager.StartInvasion(8);
					break;
				case IDM_INVASION9:
					gInvasionManager.StartInvasion(9);
					break;
				case IDM_INVASION10:
					gInvasionManager.StartInvasion(10);
					break;
				case IDM_INVASION11:
					gInvasionManager.StartInvasion(11);
					break;
				case IDM_INVASION12:
					gInvasionManager.StartInvasion(12);
					break;
				case IDM_INVASION13:
					gInvasionManager.StartInvasion(13);
					break;
				case IDM_INVASION14:
					gInvasionManager.StartInvasion(14);
					break;
				case IDM_INVASION15:
					gInvasionManager.StartInvasion(15);
					break;
				case IDM_INVASION16:
					gInvasionManager.StartInvasion(16);
					break;
				case IDM_INVASION17:
					gInvasionManager.StartInvasion(17);
					break;
				case IDM_INVASION18:
					gInvasionManager.StartInvasion(18);
					break;
				case IDM_INVASION19:
					gInvasionManager.StartInvasion(19);
					break;
				case IDM_INVASION20:
					gInvasionManager.StartInvasion(20);
					break;
				case IDM_INVASION21:
					gInvasionManager.StartInvasion(21);
					break;
				case IDM_CA0:
					gCustomArena.StartCustomArena(0);
					break;
				case IDM_CA1:
					gCustomArena.StartCustomArena(1);
					break;
				case IDM_CA2:
					gCustomArena.StartCustomArena(2);
					break;
				case IDM_CA3:
					gCustomArena.StartCustomArena(3);
					break;
				case IDM_CA4:
					gCustomArena.StartCustomArena(4);
					break;
				case IDM_CA5:
					gCustomArena.StartCustomArena(5);
					break;
				case IDM_CA6:
					gCustomArena.StartCustomArena(6);
					break;
				case IDM_CA7:
					gCustomArena.StartCustomArena(7);
					break;
				case IDM_CA8:
					gCustomArena.StartCustomArena(8);
					break;
				case IDM_CA9:
					gCustomArena.StartCustomArena(9);
					break;
				case IDM_CA10:
					gCustomArena.StartCustomArena(10);
					break;
				case IDM_CA11:
					gCustomArena.StartCustomArena(11);
					break;
				case IDM_CA12:
					gCustomArena.StartCustomArena(12);
					break;
				case IDM_CA13:
					gCustomArena.StartCustomArena(13);
					break;
				case IDM_STARTCS:
					#if(GAMESERVER_TYPE==1)
					gCastleSiege.StartCS();
					#endif
					break;
				case IDM_STARTCW:
					gCrywolf.StartCW();
					break;
				case IDM_STARTLD:
					gCastleDeep.StartLD();
					break;
				case IDM_LOGS_VIEW_COMMON:
				case IDM_LOGS_VIEW_CHAT:
				case IDM_LOGS_VIEW_EVENT:
				case IDM_LOGS_VIEW_REDEEM:
				case IDM_LOGS_VIEW_CHAOSMIX:
				case IDM_LOGS_VIEW_COMMAND:
				case IDM_LOGS_VIEW_SPINWHEEL:
				case IDM_LOGS_VIEW_MARKET:
				case IDM_LOGS_VIEW_CASHSHOP:
					{
						// The view ids are contiguous and in eLogCategory order,
						// so the category is just the offset from the first one.
						int category = LOWORD(wParam) - IDM_LOGS_VIEW_FIRST;

						gServerDisplayer.SetLogView(category);

						// Radio-style: exactly one view is ticked at a time.
						HMENU hMenu = GetMenu(hWnd);

						for(int n=IDM_LOGS_VIEW_FIRST;n <= IDM_LOGS_VIEW_LAST;n++)
						{
							CheckMenuItem(hMenu,n,MF_BYCOMMAND | ((n==LOWORD(wParam))?MF_CHECKED:MF_UNCHECKED));
						}

						// Auto Scroll is per view, so its tick has to follow the
						// view that was just selected rather than stay where the
						// previous view left it.
						CheckMenuItem(hMenu,IDM_LOGS_AUTOSCROLL,
							MF_BYCOMMAND | ((gServerDisplayer.GetLogAutoScroll()!=0)?MF_CHECKED:MF_UNCHECKED));

						gServerDisplayer.LogTextPaintNow();
					}
					break;
				case IDM_LOGS_AUTOSCROLL:
					{
						// No prior checkable-menu item existed in this project,
						// so the state lives on the displayer and the check mark
						// is driven from it rather than read back off the menu.
						int enable = ((gServerDisplayer.GetLogAutoScroll()!=0)?0:1);

						gServerDisplayer.SetLogAutoScroll(enable);

						CheckMenuItem(GetMenu(hWnd),IDM_LOGS_AUTOSCROLL,
							MF_BYCOMMAND | ((enable!=0)?MF_CHECKED:MF_UNCHECKED));

						gServerDisplayer.LogTextPaintNow();
					}
					break;
				case IDM_LOGS_SCROLLTOLATEST:
					gServerDisplayer.LogScrollToLatest();
					CheckMenuItem(GetMenu(hWnd),IDM_LOGS_AUTOSCROLL,MF_BYCOMMAND | MF_CHECKED);
					gServerDisplayer.LogTextPaintNow();
					break;
				case IDM_LOGS_CLEARLOG:
					gServerDisplayer.LogClear();
					CheckMenuItem(GetMenu(hWnd),IDM_LOGS_AUTOSCROLL,MF_BYCOMMAND | MF_CHECKED);
					gServerDisplayer.LogTextPaintNow();
					break;
				default:
					return DefWindowProc(hWnd,message,wParam,lParam);
			}
			break;
		// Scrollback input for the log panel. Repaints immediately through the
		// narrow log-only path - waiting for the 2-second timer would make the
		// thumb visibly lag the text.
		case WM_VSCROLL:
			if((HWND)lParam == GetDlgItem(hWnd,IDC_LOG_SCROLLBAR))
			{
				int visible = gServerDisplayer.GetLogVisibleRows();

				switch(LOWORD(wParam))
				{
					case SB_LINEUP:
						gServerDisplayer.LogScrollLines(-1);
						break;
					case SB_LINEDOWN:
						gServerDisplayer.LogScrollLines(1);
						break;
					case SB_PAGEUP:
						gServerDisplayer.LogScrollLines(-visible);
						break;
					case SB_PAGEDOWN:
						gServerDisplayer.LogScrollLines(visible);
						break;
					case SB_TOP:
						gServerDisplayer.LogScrollToPos(0);
						break;
					case SB_BOTTOM:
						gServerDisplayer.LogScrollToLatest();
						break;
					case SB_THUMBPOSITION:
					case SB_THUMBTRACK:
						{
							// HIWORD(wParam) is only 16 bits; read the real
							// 32-bit track position off the control instead so
							// a >65535 line history still drags correctly.
							SCROLLINFO si;
							memset(&si,0,sizeof(si));
							si.cbSize = sizeof(si);
							si.fMask = SIF_TRACKPOS;

							if(GetScrollInfo((HWND)lParam,SB_CTL,&si) != 0)
							{
								gServerDisplayer.LogScrollToPos(si.nTrackPos);
							}
							else
							{
								gServerDisplayer.LogScrollToPos(HIWORD(wParam));
							}
						}
						break;
					default:
						break;
				}

				CheckMenuItem(GetMenu(hWnd),IDM_LOGS_AUTOSCROLL,
					MF_BYCOMMAND | ((gServerDisplayer.GetLogAutoScroll()!=0)?MF_CHECKED:MF_UNCHECKED));

				gServerDisplayer.LogTextPaintNow();
			}
			break;
		case WM_MOUSEWHEEL:
			{
				int notches = (short)HIWORD(wParam) / WHEEL_DELTA;

				if(notches != 0)
				{
					gServerDisplayer.LogScrollLines(-notches * 3);

					CheckMenuItem(GetMenu(hWnd),IDM_LOGS_AUTOSCROLL,
						MF_BYCOMMAND | ((gServerDisplayer.GetLogAutoScroll()!=0)?MF_CHECKED:MF_UNCHECKED));

					gServerDisplayer.LogTextPaintNow();
				}
			}
			break;
		case WM_SIZE:
			// Nothing repaints this window except the 2-second timer, so a
			// resize would otherwise leave it blank white until the next tick.
			gServerDisplayer.Run();
			break;
		case WM_CLOSE:
			if (MessageBox(0, "Close KYANa Emulator?", "KYANa Emulator", MB_OKCANCEL) == IDOK)
			{
				DestroyWindow(hWnd);
			}
			break;
		case WM_TIMER:
			switch(wParam)
			{
				case WM_TIMER_1000:
					GJServerUserInfoSend();
					ConnectServerInfoSend();
					break;
				case WM_TIMER_2000:
					gObjCountProc();
					gServerDisplayer.Run();
					break;
				case WM_TIMER_10000:
					JoinServerReconnect(hWnd,WM_JOIN_SERVER_MSG_PROC);
					DataServerReconnect(hWnd,WM_DATA_SERVER_MSG_PROC);
					break;
			}
			break;
		case WM_JOIN_SERVER_MSG_PROC:
			JoinServerMsgProc(wParam,lParam);
			break;
		case WM_DATA_SERVER_MSG_PROC:
			DataServerMsgProc(wParam,lParam);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd,message,wParam,lParam);
	}

	return 0;
}

LRESULT CALLBACK About(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam) // OK
{
	switch(message)
	{
		case WM_INITDIALOG:
			return 1;
		case WM_COMMAND:
			if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg,LOWORD(wParam));
				return 1;
			}
			break;
	}

	return 0;
}

LRESULT CALLBACK UserOnline(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam) // OK
{
	
	switch(message)
	{
		case WM_INITDIALOG:
					

			hWndComboBox = GetDlgItem(hDlg, IDC_LIST1);

            if( !hWndComboBox )
            {
                MessageBox(hDlg,
                           "Could not create the combo box",
                           "Failed Control Creation",
                           MB_OK);
                return FALSE;
            }


			for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
			{
				if(gObj[n].Connected >= OBJECT_LOGGED && gObj[n].Type == OBJECT_USER)
				{
					    char fulltext[30]; 
						wsprintf(fulltext,"%s (%s)",gObj[n].Account,gObj[n].Name);

						int pos = SendMessage(hWndComboBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((LPCTSTR)fulltext));
						SendMessage(hWndComboBox, LB_SETITEMDATA, pos, (LPARAM) gObj[n].Account);
				}
			}

			return 1;
		case WM_COMMAND:
			if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg,LOWORD(wParam));
				return 1;
			}

			switch(LOWORD(wParam))
			{
			case IDC_BUTTONDC:

					int itemIndex = (int) SendMessage(hWndComboBox, LB_GETCURSEL, (WPARAM)0, (LPARAM) 0);
					
				    if (itemIndex == LB_ERR)
					{
						return 0;
					}

					// Getdata
					char* s = (char*)SendMessage(hWndComboBox, LB_GETITEMDATA, itemIndex, 0);

					for(int n = OBJECT_START_USER; n < MAX_OBJECT ; n++)
					{
						if(gObj[n].Connected >= OBJECT_LOGGED && strcmp(gObj[n].Account,s) == 0 )
						{
							LPOBJ lpObj = &gObj[n];

							gObjUserKill(lpObj->Index);

							gCustomAttack.OnAttackAlreadyConnected(lpObj);

							gCustomStore.OnPShopAlreadyConnected(lpObj);

							g_OfflineMode.OnHelperpAlreadyConnected(lpObj);

							CloseClient(lpObj->Index);

							MessageBox(hDlg, "Account disconnect sucefully", "Confirm", MB_OK);
							break;
						}
					}

				return 1;
			}
			break;
	}

	return 0;
}

// ----------------------------------------------------------------------
// Map/Monster Info debug dialog - Tools menu. Lets an admin pick a map,
// see every currently-spawned monster on it, and inspect that monster's
// item-drop configuration (default level-based pool, ItemDrop.txt
// overrides, and the full ItemOptionRate.txt percentage breakdown).
// ----------------------------------------------------------------------

static void MMI_RefreshMonsterList(HWND hDlg,int mapId)
{
	HWND hList = GetDlgItem(hDlg,IDC_LIST_MONSTERS);

	SendMessage(hList,LB_RESETCONTENT,0,0);

	for(int n=OBJECT_START_MONSTER;n < MAX_OBJECT_MONSTER;n++)
	{
		if(gObj[n].Connected == OBJECT_OFFLINE)
		{
			continue;
		}

		if(gObj[n].Type != OBJECT_MONSTER)
		{
			continue;
		}

		if(gObj[n].Map != mapId)
		{
			continue;
		}

		char* name = gObj[n].Name;

		if(name[0] == 0)
		{
			name = gMonsterManager.GetMonsterName(gObj[n].Class);
		}

		char text[128];

		wsprintf(text,"ObjectIndex: %d | Name: %s | Class: %d | Level: %d | Map: %d | PosX: %d | PosY: %d",
			gObj[n].Index,name,gObj[n].Class,gObj[n].Level,gObj[n].Map,gObj[n].X,gObj[n].Y);

		int pos = SendMessage(hList,LB_ADDSTRING,0,(LPARAM)text);
		SendMessage(hList,LB_SETITEMDATA,pos,(LPARAM)n);
	}

	char rateText[64];

	wsprintf(rateText,"Item Drop rate: %d%%",gMapManager.GetMapItemDropRate(mapId));
	SetDlgItemText(hDlg,IDC_STATIC_ITEMRATE,rateText);

	wsprintf(rateText,"Excellent Drop rate: %d%%",gMapManager.GetMapExcItemDropRate(mapId));
	SetDlgItemText(hDlg,IDC_STATIC_EXCRATE,rateText);

	wsprintf(rateText,"Set Item Drop rate: %d%%",gMapManager.GetMapSetItemDropRate(mapId));
	SetDlgItemText(hDlg,IDC_STATIC_SETRATE,rateText);

	ITEM_OPTION5_RATE_INFO* lpAncient = gItemOptionRate.GetRawOption5(0);

	int ancientPercent = 0;

	if(lpAncient != 0)
	{
		// Rate[0]=No Ancient, Rate[1]=Ancient1, Rate[2]=Ancient2 - combine
		// the two "got one" rows into a single summary percentage, there's
		// no single "Ancient Drop Rate" setting in this codebase otherwise.
		ancientPercent = (lpAncient->Rate[1]+lpAncient->Rate[2])/100;
	}

	wsprintf(rateText,"Random Ancient Drop Rate: %d%%",ancientPercent);
	SetDlgItemText(hDlg,IDC_STATIC_ANCRATE,rateText);

	wsprintf(rateText,"Socket Item Drop: %s",gMapManager.GetMapSocketItemDrop(mapId)?"Enabled":"Disabled");
	SetDlgItemText(hDlg,IDC_STATIC_SOCKET,rateText);
}

static void MMI_RefreshItemPanels(HWND hDlg,int monsterClass,int monsterLevel)
{
	HWND hDefaultItems = GetDlgItem(hDlg,IDC_LIST_DEFAULTITEMS);
	HWND hItemDrop = GetDlgItem(hDlg,IDC_LIST_ITEMDROP);

	SendMessage(hDefaultItems,LB_RESETCONTENT,0,0);
	SendMessage(hItemDrop,LB_RESETCONTENT,0,0);

	MONSTER_ITEM_INFO* lpItemInfo = gMonsterManager.GetMonsterItemInfo(monsterLevel);

	if(lpItemInfo != 0)
	{
		// This pool has no per-item weight of its own - GetMonsterItem() picks
		// uniformly at random across whichever of these are still eligible
		// once excellent/socket filtering is applied at drop time, so an even
		// 1/IndexCount is the honest number to show here (in hundredths of a
		// percent, so two decimal places print with %d only - wsprintf has no %f).
		int poolOddsHundredths = (lpItemInfo->IndexCount > 0) ? (10000/lpItemInfo->IndexCount) : 0;

		for(int n=0;n < lpItemInfo->IndexCount;n++)
		{
			int itemIndex = lpItemInfo->IndexTable[n];

			char text[144];

			wsprintf(text,"Item: %04d - %s | Pool Odds: %d.%02d%%",
				itemIndex,gItemLevel.GetItemName(itemIndex,0),
				poolOddsHundredths/100,poolOddsHundredths%100);

			SendMessage(hDefaultItems,LB_ADDSTRING,0,(LPARAM)text);
		}
	}

	const std::vector<ITEM_DROP_INFO>& dropList = gItemDrop.GetAllItemDropInfo();

	for(size_t n=0;n < dropList.size();n++)
	{
		const ITEM_DROP_INFO& info = dropList[n];

		if(info.MonsterClass != -1 && info.MonsterClass != monsterClass)
		{
			continue;
		}

		char rateText[24];

		if(info.DropRate < 0)
		{
			wsprintf(rateText,"100%% (guaranteed)");
		}
		else
		{
			// DropRate is parts-per-million (0-1,000,000) in ItemDrop.txt,
			// not a plain percent - divide by 10000 for whole percent,
			// remainder/100 for two decimal places.
			wsprintf(rateText,"%d.%02d%%",info.DropRate/10000,(info.DropRate%10000)/100);
		}

		char text[180];

		wsprintf(text,"Item: %04d - %s | DropRate: %s",
			info.Index,gItemLevel.GetItemName(info.Index,info.Level),rateText);

		SendMessage(hItemDrop,LB_ADDSTRING,0,(LPARAM)text);
	}
}

static void MMI_AddRateRow(HWND hList,const char* label,int regularRate,int excellentRate)
{
	// wsprintf (unlike CRT sprintf) does not support %f or width/precision
	// specifiers at all - raw values are hundredths of a percent, so split
	// into whole/tenths by hand and format with %d only.
	char text[96];

	wsprintf(text,"%s   Regular: %d.%d%%   Excellent: %d.%d%%",
		label,
		regularRate/100,(regularRate%100)/10,
		excellentRate/100,(excellentRate%100)/10);

	SendMessage(hList,LB_ADDSTRING,0,(LPARAM)text);
}

static void MMI_PopulateRateList(HWND hDlg)
{
	HWND hList = GetDlgItem(hDlg,IDC_LIST_RATES);

	SendMessage(hList,LB_RESETCONTENT,0,0);

	ITEM_OPTION0_RATE_INFO* lpLevelR = gItemOptionRate.GetRawOption0(0);
	ITEM_OPTION0_RATE_INFO* lpLevelE = gItemOptionRate.GetRawOption0(1);

	for(int n=0;n < MAX_ITEM_OPTION0_RATE;n++)
	{
		char label[16];
		wsprintf(label,"Level %d",n);
		MMI_AddRateRow(hList,label,lpLevelR?lpLevelR->Rate[n]:0,lpLevelE?lpLevelE->Rate[n]:0);
	}

	ITEM_OPTION1_RATE_INFO* lpSkillR = gItemOptionRate.GetRawOption1(0);
	ITEM_OPTION1_RATE_INFO* lpSkillE = gItemOptionRate.GetRawOption1(1);
	MMI_AddRateRow(hList,"No Skill",lpSkillR?lpSkillR->Rate[0]:0,lpSkillE?lpSkillE->Rate[0]:0);
	MMI_AddRateRow(hList,"Skill",lpSkillR?lpSkillR->Rate[1]:0,lpSkillE?lpSkillE->Rate[1]:0);

	ITEM_OPTION2_RATE_INFO* lpLuckR = gItemOptionRate.GetRawOption2(0);
	ITEM_OPTION2_RATE_INFO* lpLuckE = gItemOptionRate.GetRawOption2(1);
	MMI_AddRateRow(hList,"No Luck",lpLuckR?lpLuckR->Rate[0]:0,lpLuckE?lpLuckE->Rate[0]:0);
	MMI_AddRateRow(hList,"Luck",lpLuckR?lpLuckR->Rate[1]:0,lpLuckE?lpLuckE->Rate[1]:0);

	ITEM_OPTION3_RATE_INFO* lpOptR = gItemOptionRate.GetRawOption3(0);
	ITEM_OPTION3_RATE_INFO* lpOptE = gItemOptionRate.GetRawOption3(1);

	for(int n=0;n < MAX_ITEM_OPTION3_RATE;n++)
	{
		char label[16];
		wsprintf(label,"Option+%d",n*4);
		MMI_AddRateRow(hList,label,lpOptR?lpOptR->Rate[n]:0,lpOptE?lpOptE->Rate[n]:0);
	}

	ITEM_OPTION4_RATE_INFO* lpExcR = gItemOptionRate.GetRawOption4(0);
	ITEM_OPTION4_RATE_INFO* lpExcE = gItemOptionRate.GetRawOption4(1);
	MMI_AddRateRow(hList,"No Excellent",lpExcR?lpExcR->Rate[0]:0,lpExcE?lpExcE->Rate[0]:0);

	for(int n=1;n < MAX_ITEM_OPTION4_RATE;n++)
	{
		char label[16];
		wsprintf(label,"Excellent %d",n);
		MMI_AddRateRow(hList,label,lpExcR?lpExcR->Rate[n]:0,lpExcE?lpExcE->Rate[n]:0);
	}

	ITEM_OPTION5_RATE_INFO* lpAncR = gItemOptionRate.GetRawOption5(0);
	ITEM_OPTION5_RATE_INFO* lpAncE = gItemOptionRate.GetRawOption5(1);
	MMI_AddRateRow(hList,"No Ancient",lpAncR?lpAncR->Rate[0]:0,lpAncE?lpAncE->Rate[0]:0);
	MMI_AddRateRow(hList,"Ancient 1",lpAncR?lpAncR->Rate[1]:0,lpAncE?lpAncE->Rate[1]:0);
	MMI_AddRateRow(hList,"Ancient 2",lpAncR?lpAncR->Rate[2]:0,lpAncE?lpAncE->Rate[2]:0);

	ITEM_OPTION6_RATE_INFO* lpSockR = gItemOptionRate.GetRawOption6(0);
	ITEM_OPTION6_RATE_INFO* lpSockE = gItemOptionRate.GetRawOption6(1);
	MMI_AddRateRow(hList,"No Socket",lpSockR?lpSockR->Rate[0]:0,lpSockE?lpSockE->Rate[0]:0);

	for(int n=1;n < MAX_ITEM_OPTION6_RATE;n++)
	{
		char label[16];
		wsprintf(label,"Socket %d",n);
		MMI_AddRateRow(hList,label,lpSockR?lpSockR->Rate[n]:0,lpSockE?lpSockE->Rate[n]:0);
	}
}

LRESULT CALLBACK MapMonsterInfo(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam) // OK
{
	switch(message)
	{
		case WM_INITDIALOG:
			{
				HWND hCombo = GetDlgItem(hDlg,IDC_COMBO_MAP);

				for(std::map<int,MAP_MANAGER_INFO>::iterator it = gMapManager.m_MapManagerInfo.begin();
					it != gMapManager.m_MapManagerInfo.end();it++)
				{
					char text[64];

					wsprintf(text,"%d - %s",it->first,it->second.Name);

					int pos = SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)text);
					SendMessage(hCombo,CB_SETITEMDATA,pos,(LPARAM)it->first);
				}

				// Diagnostic for tracking down the "only Lorencia" report -
				// shows both counts so we can tell a load-side bug (map
				// count low) from a dialog-side bug (map count fine, combo
				// count low).
				char titleText[96];
				wsprintf(titleText,"Map/Monster Info (gMapManager=%d, combo=%d)",
					(int)gMapManager.m_MapManagerInfo.size(),(int)SendMessage(hCombo,CB_GETCOUNT,0,0));
				SetWindowText(hDlg,titleText);

				SendMessage(hCombo,CB_SETCURSEL,0,0);

				MMI_PopulateRateList(hDlg);

				if(gMapManager.m_MapManagerInfo.size() > 0)
				{
					MMI_RefreshMonsterList(hDlg,gMapManager.m_MapManagerInfo.begin()->first);
				}
			}
			return 1;

		case WM_COMMAND:

			if(LOWORD(wParam) == IDC_BUTTON_MMI_CLOSE || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg,LOWORD(wParam));
				return 1;
			}

			if(LOWORD(wParam) == IDC_COMBO_MAP && HIWORD(wParam) == CBN_SELCHANGE)
			{
				HWND hCombo = GetDlgItem(hDlg,IDC_COMBO_MAP);

				int sel = SendMessage(hCombo,CB_GETCURSEL,0,0);

				if(sel != CB_ERR)
				{
					int mapId = (int)SendMessage(hCombo,CB_GETITEMDATA,sel,0);

					MMI_RefreshMonsterList(hDlg,mapId);
				}

				return 1;
			}

			if(LOWORD(wParam) == IDC_LIST_MONSTERS && (HIWORD(wParam) == LBN_SELCHANGE || HIWORD(wParam) == LBN_DBLCLK))
			{
				HWND hList = GetDlgItem(hDlg,IDC_LIST_MONSTERS);

				int sel = SendMessage(hList,LB_GETCURSEL,0,0);

				if(sel != LB_ERR)
				{
					int objIndex = (int)SendMessage(hList,LB_GETITEMDATA,sel,0);

					if(objIndex >= 0 && objIndex < MAX_OBJECT_MONSTER)
					{
						MMI_RefreshItemPanels(hDlg,gObj[objIndex].Class,gObj[objIndex].Level);
					}
				}

				return 1;
			}

			break;
	}

	return 0;
}


#include "stdafx.h"
#include "NewUIManager.h"
#include "./Utilities/Log/ErrorReport.h"
#ifdef __ANDROID__
#include "Platform/MobileTime.h"
#include <typeinfo>
#include <cstdio>
#include <cstring>
#endif


using namespace SEASON3B;

#ifdef __ANDROID__
// TEMP profiling: filled in every frame by CNewUIManager::Render, rendered by
// the FPS overlay in ZzzScene.cpp.
char g_ProfUiTopWindows[176] = "uiwin -";

namespace
{
	// typeid().name() gives the Itanium mangled name, e.g.
	// "N8SEASON3B18CNewUIChatLogWindowE". Walk the <len><component> pairs and
	// keep the last component, so the overlay shows a readable class name.
	void StripUiTypeName(const char* mangled, char* out, size_t outSize)
	{
		out[0] = '\0';
		if (mangled == NULL || outSize == 0)
			return;

		const char* p = mangled;
		if (*p == 'N')
			++p;

		const char* best = NULL;
		size_t bestLen = 0;
		while (*p >= '0' && *p <= '9')
		{
			size_t len = 0;
			while (*p >= '0' && *p <= '9')
			{
				len = len * 10 + static_cast<size_t>(*p - '0');
				++p;
			}
			if (len == 0 || strlen(p) < len)
				break;
			best = p;
			bestLen = len;
			p += len;
		}

		if (best == NULL)
		{
			best = mangled;
			bestLen = strlen(mangled);
		}
		if (bestLen > 6 && strncmp(best, "CNewUI", 6) == 0)
		{
			best += 6;
			bestLen -= 6;
		}
		if (bestLen >= outSize)
			bestLen = outSize - 1;
		memcpy(out, best, bestLen);
		out[bestLen] = '\0';
	}
}
#endif

SEASON3B::CNewUIManager::CNewUIManager() 
{
	m_pActiveMouseUIObj = NULL;
	m_pActiveKeyUIObj = NULL;
#ifdef PBG_MOD_STAMINA_UI
	m_nShowUICnt =0;
#endif //PBG_MOD_STAMINA_UI
}

SEASON3B::CNewUIManager::~CNewUIManager() 
{ 
	RemoveAllUIObjs(); 
}

void SEASON3B::CNewUIManager::AddUIObj(DWORD dwKey, CNewUIObj* pUIObj)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi == m_mapUI.end())
	{
		m_vecUI.push_back(pUIObj);
		m_mapUI.insert( type_map_uibase::value_type(dwKey, pUIObj) );
	}
}

void SEASON3B::CNewUIManager::RemoveUIObj(DWORD dwKey)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi != m_mapUI.end())
	{
		type_vector_uibase::iterator vi = std::find(m_vecUI.begin(), m_vecUI.end(), (*mi).second);
		if(vi != m_vecUI.end())
		{
			m_vecUI.erase(vi);
		}
		m_mapUI.erase(mi);
	}
}

void SEASON3B::CNewUIManager::RemoveUIObj(CNewUIObj* pUIObj)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
	{
		if((*mi).second == pUIObj)
		{
			m_mapUI.erase(mi);
			break;
		}
	}
		
	type_vector_uibase::iterator vi = std::find(m_vecUI.begin(), m_vecUI.end(), pUIObj);
	if(vi != m_vecUI.end())
	{
		m_vecUI.erase(vi);
	}
}

void SEASON3B::CNewUIManager::RemoveAllUIObjs()
{
#if defined(_DEBUG)
	
	{
		unsigned int uiUIManageCNT = m_mapUI.size();
		
		type_map_uibase::iterator mi = m_mapUI.begin();
		for( ; mi != m_mapUI.end(); ++mi )
		{
			DWORD dwKey = (*mi).first;
			CNewUIObj* pUIObj = (*mi).second;
			if( pUIObj != NULL )
			{
				__TraceF(TEXT("UIKEY(%d) : mapUI \n"), uiUIManageCNT, dwKey);
			}
		}
		
		type_vector_uibase::iterator vi = m_vecUI.begin();
		for( ; vi < m_vecUI.end(); ++vi )
		{
			CNewUIObj* pUIObj = (*vi);
			if( pUIObj != NULL )
			{
				__TraceF(TEXT("vecUI \n"), uiUIManageCNT);
			}
		}
	}

#endif // defined(_DEBUG)
	m_vecUI.clear();
	m_mapUI.clear();
}

CNewUIObj* SEASON3B::CNewUIManager::FindUIObj(DWORD dwKey)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi != m_mapUI.end())
		return (*mi).second;
	return NULL;
}

bool SEASON3B::CNewUIManager::UpdateMouseEvent()
{
	m_pActiveMouseUIObj = NULL;

	std::sort(m_vecUI.begin(), m_vecUI.end(), CompareLayerDepthReverse);
	
	type_vector_uibase::iterator vi = m_vecUI.begin();
	vi = m_vecUI.begin();
	for(; vi != m_vecUI.end(); vi++)
	{
		if((*vi)->IsVisible()) 
		{

			CNewUIObj *obj_backup = (*vi);
			bool bResult = (*vi)->UpdateMouseEvent();
			
			type_vector_uibase::iterator vi2 = std::find(m_vecUI.begin(), m_vecUI.end(), obj_backup);
			if( vi2 != m_vecUI.end() )
			{
				vi = vi2;
			}
			else
			{
				break;
			}

			if( bResult == false )
			{
				m_pActiveMouseUIObj = *vi;
				return false;
			}

		}
	}

	return true;
}

bool SEASON3B::CNewUIManager::UpdateKeyEvent()
{
	m_pActiveKeyUIObj = NULL;
	std::sort(m_vecUI.begin(), m_vecUI.end(), CompareKeyEventOrder);
	
	type_vector_uibase::iterator vi = m_vecUI.begin();
	for(; vi != m_vecUI.end(); vi++)
	{
		HWND hRelatedWnd = (*vi)->GetRelatedWnd();
		if(NULL == hRelatedWnd)
		{
			hRelatedWnd = g_hWnd;
		}

		HWND hWnd = GetFocus();

		if((*vi)->IsEnabled() && hWnd == hRelatedWnd)
		{
			if(false == (*vi)->UpdateKeyEvent())
			{
				m_pActiveKeyUIObj = (*vi);
				return false;		//. stop calling UpdateKeyEvent functions
			}
		}
	}
	return true;
}

bool SEASON3B::CNewUIManager::Update()
{
	std::sort(m_vecUI.begin(),m_vecUI.end(), CompareLayerDepth);
	
	type_vector_uibase::iterator vi = m_vecUI.begin();
	for(; vi != m_vecUI.end(); vi++)
	{
		if((*vi)->IsEnabled()) 
		{
			if(false == (*vi)->Update())
			{
				return false;		//. stop calling Update functions
			}
		}
	}

	return true;
}

bool SEASON3B::CNewUIManager::Render()
{
	std::sort(m_vecUI.begin(),m_vecUI.end(), CompareLayerDepth);

#ifdef __ANDROID__
	// TEMP profiling: CNewUISystem::Render is ~13.5ms of a ~51ms frame, by far
	// the largest single bucket. Record the three most expensive windows so the
	// FPS overlay can name them (fopen-based logging never worked on device -
	// the cwd is not writable, same reason logcat is unavailable).
	double dbgTopMs[3] = { 0.0, 0.0, 0.0 };
	const char* dbgTopName[3] = { "-", "-", "-" };
	double dbgTotalMs = 0.0;
#endif

	type_vector_uibase::iterator vi = m_vecUI.begin();
	for(; vi != m_vecUI.end(); vi++)
	{
		if((*vi)->IsVisible())
		{
#ifdef __ANDROID__
			const uint64_t t0 = MU_MobilePerfNow();
			(*vi)->Render();
			const double ms = (static_cast<double>(MU_MobilePerfNow() - t0) * 1000.0) / static_cast<double>(MU_MobilePerfFrequency());
			dbgTotalMs += ms;
			const char* name = typeid(**vi).name();
			for (int slot = 0; slot < 3; ++slot)
			{
				if (ms > dbgTopMs[slot])
				{
					for (int shift = 2; shift > slot; --shift)
					{
						dbgTopMs[shift] = dbgTopMs[shift - 1];
						dbgTopName[shift] = dbgTopName[shift - 1];
					}
					dbgTopMs[slot] = ms;
					dbgTopName[slot] = name;
					break;
				}
			}
#else
			(*vi)->Render();
#endif
		}
	}

#ifdef __ANDROID__
	char n0[40], n1[40], n2[40];
	StripUiTypeName(dbgTopName[0], n0, sizeof(n0));
	StripUiTypeName(dbgTopName[1], n1, sizeof(n1));
	StripUiTypeName(dbgTopName[2], n2, sizeof(n2));
	snprintf(g_ProfUiTopWindows, sizeof(g_ProfUiTopWindows),
		"uiwin %.1f | %s %.1f | %s %.1f | %s %.1f",
		dbgTotalMs, n0, dbgTopMs[0], n1, dbgTopMs[1], n2, dbgTopMs[2]);
#endif

	return true;
}

CNewUIObj* SEASON3B::CNewUIManager::GetActiveMouseUIObj()
{ 
	return m_pActiveMouseUIObj; 
}

CNewUIObj* SEASON3B::CNewUIManager::GetActiveKeyUIObj()
{ 
	return m_pActiveKeyUIObj; 
}

void SEASON3B::CNewUIManager::ResetActiveUIObj()
{
	m_pActiveMouseUIObj = NULL;
	m_pActiveKeyUIObj = NULL;
}

bool SEASON3B::CNewUIManager::IsInterfaceVisible(DWORD dwKey)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL == pObj)
	{
		return false;
	}
	return pObj->IsVisible();
}

bool SEASON3B::CNewUIManager::IsInterfaceEnabled(DWORD dwKey)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL == pObj)
		return false;
	return pObj->IsEnabled();
}

void SEASON3B::CNewUIManager::ShowInterface(DWORD dwKey, bool bShow/* = true*/)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL != pObj)
		pObj->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableInterface(DWORD dwKey, bool bEnable/* = true*/)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL != pObj)
		pObj->Enable(bEnable);
}

void SEASON3B::CNewUIManager::ShowAllInterfaces(bool bShow/* = true*/)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
		(*mi).second->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableAllInterfaces(bool bEnable/* = true*/)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
		(*mi).second->Show(bEnable);
}

bool SEASON3B::CNewUIManager::CompareLayerDepth(INewUIBase* pObj1,INewUIBase* pObj2)
{ 
	return pObj1->GetLayerDepth() < pObj2->GetLayerDepth(); 
}

bool SEASON3B::CNewUIManager::CompareLayerDepthReverse(INewUIBase* pObj1,INewUIBase* pObj2)
{ 
	return pObj1->GetLayerDepth() > pObj2->GetLayerDepth(); 
}

bool SEASON3B::CNewUIManager::CompareKeyEventOrder(INewUIBase* pObj1,INewUIBase* pObj2)
{
	return pObj1->GetKeyEventOrder() > pObj2->GetKeyEventOrder();
}

#ifdef PBG_MOD_STAMINA_UI
int SEASON3B::CNewUIManager::GetShowUICnt()
{
	int m_nShowUICnt=0;
	// �Ϻ� Ư�� �������̽��� � �����ִ���
	for(int i=INTERFACE_PARTY; i<INTERFACE_CHARACTER+1; ++i)
	{
		if(IsInterfaceVisible(i))
			m_nShowUICnt++;
	}
	return m_nShowUICnt;
}
#endif //PBG_MOD_STAMINA_UI

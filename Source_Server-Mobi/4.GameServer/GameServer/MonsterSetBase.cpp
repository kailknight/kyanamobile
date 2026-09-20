// MonsterSetBase.cpp: implementation of the CMonsterSetBase class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MonsterSetBase.h"
#include "MapServerManager.h"
#include "MemScript.h"
#include "Util.h"

CMonsterSetBase gMonsterSetBase;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define MAX_MSB_COLUMN 16

static int MsbToNumber(const char* token) // OK
{
	// CMemScript treats a bare '*' as -1; keep that so any file using it for a
	// random direction still means the same thing here.
	if(strcmp(token,"*") == 0)
	{
		return -1;
	}

	return atoi(token);
}

CMonsterSetBase::CMonsterSetBase() // OK
{
	this->m_count = 0;
	this->m_overflow = 0;
}

CMonsterSetBase::~CMonsterSetBase() // OK
{

}

void CMonsterSetBase::Load(char* path) // OK
{
	this->m_count = 0;
	this->m_overflow = 0;

	this->LoadFile(path);
}

void CMonsterSetBase::LoadFolder(char* folderPath)
{
	this->m_count = 0;
	this->m_overflow = 0;

	this->LoadFolderRecursive(folderPath);

	LogAdd(LOG_BLUE,"[MonsterSetBase] Loaded %d spawn entries from %s",this->m_count,folderPath);

	if(this->m_overflow != 0)
	{
		LogAdd(LOG_RED,"[MonsterSetBase] %d entries did not fit and were dropped - raise MAX_MSB_MONSTER (currently %d)",this->m_overflow,MAX_MSB_MONSTER);
	}
}

void CMonsterSetBase::LoadFolderRecursive(char* folderPath)
{
	// Descends into subfolders (e.g. MonsterSetBase\Event\, \Invasion\)
	// instead of only loading the flat top-level *.txt files - those
	// subfolders' .txt files were previously never found at all, since
	// the old single-level FindFirstFileA("*.txt") scan explicitly
	// skipped anything with FILE_ATTRIBUTE_DIRECTORY.
	char szSearchAll[MAX_PATH];

	sprintf(szSearchAll,"%s\\*",folderPath);

	int files = 0;
	int entries = 0;

	WIN32_FIND_DATAA fd;

	HANDLE hFind = FindFirstFileA(szSearchAll,&fd);

	if(hFind == INVALID_HANDLE_VALUE)
	{
		LogAdd(LOG_RED,"[MonsterSetBase] Could not open folder (%s)",folderPath);
		return;
	}

	do
	{
		if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if(strcmp(fd.cFileName,".") == 0 || strcmp(fd.cFileName,"..") == 0)
			{
				continue;
			}

			char szSubFolderPath[MAX_PATH];

			sprintf(szSubFolderPath,"%s\\%s",folderPath,fd.cFileName);

			this->LoadFolderRecursive(szSubFolderPath);

			continue;
		}

		char* lpExt = strrchr(fd.cFileName,'.');

		if(lpExt == 0 || _stricmp(lpExt,".txt") != 0)
		{
			continue;
		}

		char szFilePath[MAX_PATH];

		sprintf(szFilePath,"%s\\%s",folderPath,fd.cFileName);

		int before = this->m_count;

		this->LoadFile(szFilePath);

		files++;
		entries += (this->m_count-before);
	}
	while(FindNextFileA(hFind,&fd));

	FindClose(hFind);

	// One line per directory, so "did MonsterSetBase\Event\ actually load?"
	// is answerable straight off the server log instead of by guesswork.
	LogAdd(LOG_BLACK,"[MonsterSetBase] %s : %d entries from %d file(s)",folderPath,entries,files);
}

int CMonsterSetBase::TokenizeLine(char* line,char* token[],int max) // OK
{
	char* comment = strstr(line,"//");

	if(comment != 0)
	{
		(*comment) = 0;
	}

	int count = 0;

	char* context = 0;

	for(char* tok = strtok_s(line," \t\r\n",&context); tok != 0; tok = strtok_s(0," \t\r\n",&context))
	{
		if(count >= max)
		{
			return max;
		}

		token[count++] = tok;
	}

	return count;
}

void CMonsterSetBase::LoadFile(char* path) // OK
{
	// Deliberately line-based rather than going through CMemScript. CMemScript
	// is a pure token stream with no notion of where a line ends, so it can
	// only ever accept one fixed column count per section - and the data files
	// use several: section 2 carries a trailing Quantity in Barracks and Land
	// of Trials, section 4 appears in Begin/End box form in Devil Square 2, and
	// InvasionSetBase.txt uses a section 5 the old parser did not know at all.
	// A column count CMemScript did not expect never failed loudly; it shifted
	// every following token by one and quietly turned the rest of the file into
	// garbage spawns. Dispatching on (section, column count) one line at a time
	// removes that whole failure mode, and parses the well-formed files exactly
	// as before.
	HANDLE file = CreateFile(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_ARCHIVE,0);

	if(file == INVALID_HANDLE_VALUE)
	{
		LogAdd(LOG_RED,MEM_SCRIPT_ERROR_CODE0,path);
		return;
	}

	DWORD size = GetFileSize(file,0);

	char* buff = new char[size+1];

	if(buff == 0)
	{
		CloseHandle(file);
		LogAdd(LOG_RED,MEM_SCRIPT_ERROR_CODE1,path);
		return;
	}

	DWORD read = 0;

	if(ReadFile(file,buff,size,&read,0) == 0)
	{
		delete[] buff;
		CloseHandle(file);
		LogAdd(LOG_RED,MEM_SCRIPT_ERROR_CODE2,path);
		return;
	}

	CloseHandle(file);

	buff[read] = 0;

	char text[512];
	char* token[MAX_MSB_COLUMN];

	int section = -1;
	int line = 0;
	int stored = 0;
	DWORD pos = 0;

	while(pos <= read)
	{
		DWORD start = pos;

		while(pos < read && buff[pos] != '\n')
		{
			pos++;
		}

		DWORD length = pos-start;

		pos++;

		line++;

		if(length >= sizeof(text))
		{
			length = sizeof(text)-1;
		}

		memcpy(text,&buff[start],length);

		text[length] = 0;

		int count = this->TokenizeLine(text,token,MAX_MSB_COLUMN);

		if(count == 0)
		{
			continue;
		}

		if(count == 1 && _stricmp(token[0],"end") == 0)
		{
			section = -1;
			continue;
		}

		if(section < 0)
		{
			if(count != 1)
			{
				LogAdd(LOG_RED,"[MonsterSetBase] Data outside of any section (%s line %d)",path,line);
				continue;
			}

			section = MsbToNumber(token[0]);
			continue;
		}

		MONSTER_SET_BASE_INFO info;

		memset(&info,0,sizeof(info));

		info.Type = section;
		info.MonsterClass = MsbToNumber(token[0]);
		info.Map = MsbToNumber(token[1]);
		info.Dis = MsbToNumber(token[2]);

		int spawn = 1;
		bool scatter = 0;

		if(count == 9 || count == 10)
		{
			// Monster Map Range BeginX BeginY EndX EndY Dir Quantity [Value]
			info.X = MsbToNumber(token[3]);
			info.Y = MsbToNumber(token[4]);
			info.TX = MsbToNumber(token[5]);
			info.TY = MsbToNumber(token[6]);
			info.Dir = MsbToNumber(token[7]);
			info.Box = 1;

			spawn = MsbToNumber(token[8]);

			if(count == 10)
			{
				info.Value = MsbToNumber(token[9]);
			}
		}
		else if(count == 6 || count == 7)
		{
			// Monster Map Range PosX PosY Dir [Quantity]
			info.X = MsbToNumber(token[3]);
			info.Y = MsbToNumber(token[4]);
			info.Dir = MsbToNumber(token[5]);

			if(count == 7)
			{
				// Section 5's seventh column is a Value, not a Quantity.
				if(section == 5)
				{
					info.Value = MsbToNumber(token[6]);
				}
				else
				{
					spawn = MsbToNumber(token[6]);
				}
			}

			// Section 2 jitters its anchor once per spawned copy, as before.
			scatter = (section == 2);
		}
		else
		{
			LogAdd(LOG_RED,"[MonsterSetBase] Section %d does not have a %d column layout (%s line %d)",section,count,path,line);
			continue;
		}

		if(spawn < 0)
		{
			spawn = 0;
		}

		for(int n=0;n < spawn;n++)
		{
			MONSTER_SET_BASE_INFO copy = info;

			if(scatter != 0)
			{
				copy.X = (info.X-3)+GetLargeRand()%7;
				copy.Y = (info.Y-3)+GetLargeRand()%7;
			}

			this->SetInfo(copy);

			stored++;
		}
	}

	delete[] buff;

	if(section >= 0)
	{
		LogAdd(LOG_RED,"[MonsterSetBase] Section %d is missing its 'end' (%s)",section,path);
	}

	if(stored == 0 && read != 0)
	{
		LogAdd(LOG_RED,"[MonsterSetBase] No spawn entries read from %s",path);
	}
}

void CMonsterSetBase::SetInfo(MONSTER_SET_BASE_INFO info) // OK
{
	if(this->m_count < 0 || this->m_count >= MAX_MSB_MONSTER)
	{
		// This used to be a silent return, which is how a full table came to
		// look like the folder scan not recursing: the subdirectories are
		// enumerated last, so once the top-level files had filled the array
		// every Event\ and Invasion\ entry landed here and disappeared.
		if(this->m_overflow++ == 0)
		{
			LogAdd(LOG_RED,"[MonsterSetBase] Table full at %d entries - everything from here on is being dropped",MAX_MSB_MONSTER);
		}

		return;
	}

	if(gMapServerManager.CheckMapServer(info.Map) == 0)
	{
		return;
	}

	info.Dir = ((info.Dir==-1)?(GetLargeRand()%8):info.Dir);

	this->m_MonsterSetBaseInfo[this->m_count++] = info;
}

bool CMonsterSetBase::GetPosition(int index,short map,short* ox,short* oy) // OK
{
	if(index < 0 || index >= MAX_MSB_MONSTER)
	{
		return 0;
	}

	MONSTER_SET_BASE_INFO* lpInfo = &this->m_MonsterSetBaseInfo[index];

	// The layout the line was actually written in wins over its section
	// number: Devil Square 2 writes Begin/End spawn boxes under section 4,
	// which is otherwise a fixed-position section. Types 1 and 3 always come
	// from the box form, so they land here exactly as they did before.
	if(lpInfo->Box != 0)
	{
		return this->GetBoxPosition(map,lpInfo->X,lpInfo->Y,lpInfo->TX,lpInfo->TY,ox,oy);
	}

	if(lpInfo->Type == 2)
	{
		return this->GetBoxPosition(map,(lpInfo->X-3),(lpInfo->Y-3),(lpInfo->X+3),(lpInfo->Y+3),ox,oy);
	}

	(*ox) = lpInfo->X;
	(*oy) = lpInfo->Y;
	return 1;
}

bool CMonsterSetBase::GetBoxPosition(int map,int x,int y,int tx,int ty,short* ox,short* oy) // OK
{
	for(int n=0;n < 100;n++)
	{
		int subx = tx-x;
		int suby = ty-y;

		subx = ((subx<1)?1:subx);
		suby = ((suby<1)?1:suby);

		subx = x+(GetLargeRand()%subx);
		suby = y+(GetLargeRand()%suby);

		if(gMap[map].CheckAttr(subx,suby,1) == 0 && gMap[map].CheckAttr(subx,suby,4) == 0 && gMap[map].CheckAttr(subx,suby,8) == 0)
		{
			(*ox) = subx;
			(*oy) = suby;
			return 1;
		}
	}

	return 0;
}

void CMonsterSetBase::SetBoxPosition(int index,int map,int x,int y,int tx,int ty) // OK
{
	if(index < 0 || index >= MAX_MSB_MONSTER)
	{
		return;
	}

	MONSTER_SET_BASE_INFO* lpInfo = &this->m_MonsterSetBaseInfo[index];

	lpInfo->Map = map;
	lpInfo->X = x;
	lpInfo->Y = y;
	lpInfo->TX = tx;
	lpInfo->TY = ty;
	lpInfo->Box = 1;
}

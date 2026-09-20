// MonsterSetBase.h: interface for the CMonsterSetBase class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "User.h"

// Was 10000, which the per-map MonsterSetBase\ split silently outgrew: the
// top-level *.txt files alone ask for ~12800 entries, so the table filled up
// partway through and SetInfo() dropped everything after it without a word.
// Because FindFirstFile enumerates the numeric filenames before the Event\ and
// Invasion\ subdirectories, "everything after it" was the whole of both
// subfolders - which looked exactly like the folder scan never recursing.
#define MAX_MSB_MONSTER 32000

#define OBJ_MAXMONSTER	(OBJ_STARTUSERINDEX-100)	//MC	//11500 //5800

struct MONSTER_SET_BASE_INFO
{
	int Type;
	int MonsterClass;
	int Map;
	int Dis;
	int X;
	int Y;
	int Dir;
	int TX;
	int TY;
	int Value;
	int Box;	// line was written in the Begin/End box form, whatever its section
};

class CMonsterSetBase
{
public:
	CMonsterSetBase();
	virtual ~CMonsterSetBase();
	void Load(char* path);
	void LoadFolder(char* folderPath);
	void SetInfo(MONSTER_SET_BASE_INFO info);
	bool GetPosition(int index,short map,short* ox,short* oy);
	bool GetBoxPosition(int map,int x,int y,int tx,int ty,short* ox,short* oy);
	void SetBoxPosition(int index,int map,int x,int y,int tx,int ty);
private:
	void LoadFolderRecursive(char* folderPath);
	void LoadFile(char* path);
	int TokenizeLine(char* line,char* token[],int max);
public:
	MONSTER_SET_BASE_INFO m_MonsterSetBaseInfo[MAX_MSB_MONSTER];
	MONSTER_SET_BASE_INFO m_Mp[OBJ_MAXMONSTER];//MC
	int m_count;
	int m_overflow;	// entries dropped because the table was already full
};

extern CMonsterSetBase gMonsterSetBase;

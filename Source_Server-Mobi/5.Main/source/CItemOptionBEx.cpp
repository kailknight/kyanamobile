// CItemOptionBEx.cpp: see CItemOptionBEx.h.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "zzzInfomation.h"
#include "zzzCharacter.h"
#include "zzzinventory.h"
#include "CharacterManager.h"
#include "CItemOptionBEx.h"

extern char TextList[60][512];
extern int  TextListColor[60];
extern int  TextBold[60];

CItemOptionBEx g_ItemOptionBEx;

namespace
{
	// TextList holds 60 lines and every other appender shares it. Stop short so the
	// lines that come after this one (and the sizing pass) still have room.
	const int kBExTextLimit = 56;

	// The server's ITEM_OPTION_* ids. The client has no copy of that enum, so they are
	// literals here - ItemOption.h on the server is the reference.
	enum
	{
		BEX_ADD_PHYSI_DAMAGE_BY_LEVEL = 93,
		BEX_ADD_MAGIC_DAMAGE_BY_LEVEL = 95,
		BEX_ADD_WING_HP               = 100,
		BEX_ADD_WING_MP               = 101,
		BEX_ADD_WING_LEADERSHIP       = 105,
		BEX_ADD_FENRIR1               = 106,
		BEX_ADD_FENRIR2               = 107,
		BEX_ADD_FENRIR3               = 108,
		BEX_ADD_DINORANT1             = 112,
		BEX_ADD_CURSE_DAMAGE_BY_LEVEL = 114,
		BEX_ADD_HP_BY_LEVEL           = 121,
		BEX_ADD_MP_BY_LEVEL           = 122,
		BEX_ADD_DAMAGE_BY_LEVEL       = 124,
		BEX_ADD_DEFENSE_BY_LEVEL      = 126,
	};

	// Recognised by the server but a no-op there (an empty case in CBInsertOption), so
	// showing them would advertise a bonus that does not exist.
	bool IsNoEffectOption(int option)
	{
		return option == BEX_ADD_FENRIR1 || option == BEX_ADD_FENRIR2 || option == BEX_ADD_FENRIR3 || option == BEX_ADD_DINORANT1;
	}

	// (level / Value) options. Summing two of these would be meaningless - 1 per 10
	// levels plus 1 per 5 levels is not 1 per 15 - so each row gets its own line.
	bool IsPerLevelOption(int option)
	{
		switch(option)
		{
			case BEX_ADD_PHYSI_DAMAGE_BY_LEVEL:
			case BEX_ADD_MAGIC_DAMAGE_BY_LEVEL:
			case BEX_ADD_CURSE_DAMAGE_BY_LEVEL:
			case BEX_ADD_HP_BY_LEVEL:
			case BEX_ADD_MP_BY_LEVEL:
			case BEX_ADD_DAMAGE_BY_LEVEL:
			case BEX_ADD_DEFENSE_BY_LEVEL:
				return true;
		}

		return false;
	}

	// What the row actually adds. Most options add Value as-is; the three wing
	// options run it through the same formula the server's CBInsertOption does, so a
	// Value of 5 shows as the 100 HP it really grants, not as "+5".
	int EffectOf(int option, int value)
	{
		switch(option)
		{
			case BEX_ADD_WING_HP:
			case BEX_ADD_WING_MP:
				return 50 + (value * 10);

			case BEX_ADD_WING_LEADERSHIP:
				return 10 + (value * 10);
		}

		return value;
	}

	// Wording follows what the server does with the number, not what the old .xml
	// comment said: "+N" is a flat amount, "+N%" is a percentage.
	void FormatLine(char* out, int option, int v)
	{
		switch(option)
		{
			case 80:  sprintf(out, "Physical damage +%d", v); break;
			case 81:  sprintf(out, "Wizardry damage +%d", v); break;
			case 82:  sprintf(out, "Defense success rate +%d", v); break;
			case 83:  sprintf(out, "Defense +%d", v); break;
			case 84:  sprintf(out, "Critical damage rate +%d%%", v); break;
			case 85:  sprintf(out, "HP recovery rate +%d%%", v); break;
			case 86:  sprintf(out, "Zen drop +%d%%", v); break;
			case 87:  sprintf(out, "Defense success rate +%d%%", v); break;
			case 88:  sprintf(out, "Damage reflect +%d%%", v); break;
			case 89:  sprintf(out, "Damage reduction +%d%%", v); break;
			case 90:  sprintf(out, "Max Mana +%d%%", v); break;
			case 91:  sprintf(out, "Max HP +%d%%", v); break;
			case 92:  sprintf(out, "Excellent damage rate +%d%%", v); break;
			case 93:  sprintf(out, "Physical damage +1 per %d levels", v); break;
			case 94:  sprintf(out, "Physical damage +%d%%", v); break;
			case 95:  sprintf(out, "Wizardry damage +1 per %d levels", v); break;
			case 96:  sprintf(out, "Wizardry damage +%d%%", v); break;
			case 97:  sprintf(out, "Attack speed +%d", v); break;
			case 98:  sprintf(out, "HP on monster kill +%d", v); break;
			case 99:  sprintf(out, "Mana on monster kill +%d", v); break;
			case 100: sprintf(out, "Max HP +%d (with wings)", v); break;
			case 101: sprintf(out, "Max Mana +%d (with wings)", v); break;
			case 102: sprintf(out, "Ignore defense rate +%d%%", v); break;
			case 103: sprintf(out, "Max AG +%d", v); break;
			case 104: sprintf(out, "Max AG +%d%%", v); break;
			case 105: sprintf(out, "Command +%d (with wings)", v); break;
			case 109: sprintf(out, "Full damage reflect rate +%d%%", v); break;
			case 110: sprintf(out, "Full HP restore on defense skill +%d%%", v); break;
			case 111: sprintf(out, "Full Mana restore on defense skill +%d%%", v); break;
			case 113: sprintf(out, "Curse damage +%d", v); break;
			case 114: sprintf(out, "Curse damage +1 per %d levels", v); break;
			case 115: sprintf(out, "Curse damage +%d%%", v); break;
			case 116: sprintf(out, "Double damage rate +%d%%", v); break;
			case 117: sprintf(out, "Experience +%d%%", v); break;
			case 118: sprintf(out, "Combo experience +%d%% (with pet)", v); break;
			case 119: sprintf(out, "Max HP +%d", v); break;
			case 120: sprintf(out, "Max Mana +%d", v); break;
			case 121: sprintf(out, "Max HP +1 per %d levels", v); break;
			case 122: sprintf(out, "Max Mana +1 per %d levels", v); break;
			case 123: sprintf(out, "Damage +%d", v); break;
			case 124: sprintf(out, "Damage +1 per %d levels", v); break;
			case 125: sprintf(out, "Damage +%d%%", v); break;
			case 126: sprintf(out, "Defense +1 per %d levels", v); break;
			case 127: sprintf(out, "Defense +%d%%", v); break;
			case 128: sprintf(out, "Max SD +%d", v); break;
			case 129: sprintf(out, "Max SD +%d%%", v); break;
			case 130: sprintf(out, "Physical damage +%d%%", v); break;
			case 131: sprintf(out, "Triple damage rate +%d%%", v); break;
			case 132: sprintf(out, "Double damage resistance +%d%%", v); break;
			case 133: sprintf(out, "Ignore defense resistance +%d%%", v); break;
			case 134: sprintf(out, "Ignore SD resistance +%d%%", v); break;
			case 135: sprintf(out, "Critical damage resistance +%d%%", v); break;
			case 136: sprintf(out, "Excellent damage resistance +%d%%", v); break;
			case 137: sprintf(out, "Stun resistance +%d%%", v); break;

			// An id added on the server after this client was built still shows up,
			// rather than silently vanishing from the tooltip.
			default:  sprintf(out, "Bonus option %d: +%d", option, v); break;
		}
	}

	int ItemLevelOf(const ITEM* ip)
	{
		return (ip->Level >> 3) & 15;
	}

	int ExcBitsOf(const ITEM* ip)
	{
		return ip->Option1 & 63;
	}

	int AppendLine(int TextNum, const char* text, int color, bool bold)
	{
		if(TextNum >= kBExTextLimit)
		{
			return TextNum;
		}

		sprintf(TextList[TextNum], "%s", text);
		TextListColor[TextNum] = color;
		TextBold[TextNum] = bold ? 1 : 0;

		return TextNum + 1;
	}

	// Emits one block: the summed flat/percent options in id order, then the per-level
	// ones one row each.
	int AppendOptions(int TextNum, const std::map<int, int>& summed, const std::vector<std::pair<int, int> >& perLevel, int color)
	{
		char line[128];

		for(std::map<int, int>::const_iterator it = summed.begin(); it != summed.end(); ++it)
		{
			FormatLine(line, it->first, it->second);
			TextNum = AppendLine(TextNum, line, color, false);
		}

		for(size_t n = 0; n < perLevel.size(); n++)
		{
			FormatLine(line, perLevel[n].first, perLevel[n].second);
			TextNum = AppendLine(TextNum, line, color, false);
		}

		return TextNum;
	}
}

CItemOptionBEx::CItemOptionBEx()
{
}

void CItemOptionBEx::Recv(BYTE* lpMsg)
{
	PMSG_ITEM_OPTION_BEX_RECV hdr;

	memcpy(&hdr, lpMsg, sizeof(hdr));

	// C2 length, big-endian: PSWMSG_HEAD::set() puts HIBYTE in size[0]. Spelled out
	// rather than MAKEWORD, which is a Windows macro and this file also builds for
	// Android.
	const int size = (((int)hdr.header.size[0]) << 8) | ((int)hdr.header.size[1]);

	if(size < (int)sizeof(hdr))
	{
		return;
	}

	if(hdr.start == 0)
	{
		m_Rows.clear();
	}

	// A chunk that does not continue exactly where the table ends is dropped, not
	// spliced in. TCP keeps order, so this only fires on a protocol bug - but then it
	// is the difference between a missing option and a wrong one.
	if(hdr.start != m_Rows.size())
	{
		return;
	}

	// Never trust the count further than the packet's own length, so a short or
	// malformed packet cannot walk off the end of the receive buffer.
	int count = hdr.count;
	const int fit = (size - (int)sizeof(hdr)) / (int)sizeof(PMSG_ITEM_OPTION_BEX_ROW_RECV);

	if(count > fit)
	{
		count = fit;
	}

	const BYTE* p = lpMsg + sizeof(hdr);

	for(int n = 0; n < count; n++)
	{
		// memcpy rather than a cast: the rows are packed at an 18-byte stride, so on
		// ARM most of them sit on addresses an int load would fault on.
		PMSG_ITEM_OPTION_BEX_ROW_RECV row;
		memcpy(&row, p + (n * sizeof(row)), sizeof(row));
		m_Rows.push_back(row);
	}
}

// CItemOption::InsertOptionBEX's test for one row, at a level the caller chooses.
bool CItemOptionBEx::RowMatches(const PMSG_ITEM_OPTION_BEX_ROW_RECV& row, int type, int level, int excBits) const
{
	if(type == -1)
	{
		return false;
	}

	if(type < (int)row.ItemMinIndex || type > (int)row.ItemMaxIndex)
	{
		return false;
	}

	if(!((level >= row.ItemLevelMin && level <= row.ItemLevelMax) || row.ItemLevelMin == -1))
	{
		return false;
	}

	if(!(excBits >= row.ItemExc || row.ItemExc == -1))
	{
		return false;
	}

	return true;
}

// CItemOption's CheckItemSlotEx, against what the hero is wearing right now. Mirrors it
// quirk for quirk: the named piece must itself match the row, then helm/armour/pants/
// gloves/boots sharing its index are counted - excellent only if Exc is 1, anything if
// Exc is -1, and (exactly as on the server) nothing at all if Exc is 0.
bool CItemOptionBEx::IsSetActive(const PMSG_ITEM_OPTION_BEX_ROW_RECV& row) const
{
	if(CharacterMachine == NULL || Hero == NULL)
	{
		return false;
	}

	const int triggerCat = row.ItemMinIndex / MAX_ITEM_INDEX;

	if(triggerCat < 7 || triggerCat > 11)
	{
		return false;
	}

	const ITEM* trigger = &CharacterMachine->Equipment[EQUIPMENT_HELM + (triggerCat - 7)];

	if(!RowMatches(row, trigger->Type, ItemLevelOf(trigger), ExcBitsOf(trigger)))
	{
		return false;
	}

	const int setIdx = trigger->Type % MAX_ITEM_INDEX;

	int pieces = 0;

	for(int slot = EQUIPMENT_HELM; slot <= EQUIPMENT_BOOTS; slot++)
	{
		const ITEM* p = &CharacterMachine->Equipment[slot];

		bool isSetPiece = false;

		for(int cat = 7; cat <= 11; cat++)
		{
			if(p->Type == (cat * MAX_ITEM_INDEX) + setIdx)
			{
				isSetPiece = true;
				break;
			}
		}

		if(!isSetPiece || p->Durability == 0)
		{
			continue;
		}

		if(!((row.ItemExc == 1 && ExcBitsOf(p) > 0) || row.ItemExc == -1))
		{
			continue;
		}

		if(!(row.ItemLevelMin <= ItemLevelOf(p) || row.ItemLevelMin == -1))
		{
			continue;
		}

		// Same-level rule, mirroring the server's CheckItemSlotEx (SetLevel): every
		// piece must be at exactly the level of the piece carrying the row, so four
		// +15 pieces and one +13 do not make a +13 set - they make no set.
		if(ItemLevelOf(p) != ItemLevelOf(trigger))
		{
			continue;
		}

		pieces++;
	}

	const int baseClass = gCharacterManager.GetBaseClass(Hero->Class);

	return ((baseClass == CLASS_DARK || baseClass == CLASS_RAGEFIGHTER) && pieces >= 4) || pieces >= 5;
}

std::vector<size_t> CItemOptionBEx::MatchSingleRows(const ITEM* ip, int level) const
{
	std::vector<size_t> out;

	const int excBits = ExcBitsOf(ip);

	for(size_t n = 0; n < m_Rows.size(); n++)
	{
		const PMSG_ITEM_OPTION_BEX_ROW_RECV& row = m_Rows[n];

		if(row.ItemSet == 1 || IsNoEffectOption(row.OptionIndex))
		{
			continue;
		}

		if(RowMatches(row, ip->Type, level, excBits))
		{
			out.push_back(n);
		}
	}

	return out;
}

// Set rows are shown on ANY piece of the set, so they are matched on the hovered
// piece's index within its category - not on the exact piece the row names.
std::vector<size_t> CItemOptionBEx::MatchSetRows(int hoverIdx, int level) const
{
	std::vector<size_t> out;

	for(size_t n = 0; n < m_Rows.size(); n++)
	{
		const PMSG_ITEM_OPTION_BEX_ROW_RECV& row = m_Rows[n];

		if(row.ItemSet != 1 || IsNoEffectOption(row.OptionIndex))
		{
			continue;
		}

		if(hoverIdx < (row.ItemMinIndex % MAX_ITEM_INDEX) || hoverIdx > (row.ItemMaxIndex % MAX_ITEM_INDEX))
		{
			continue;
		}

		if(!((level >= row.ItemLevelMin && level <= row.ItemLevelMax) || row.ItemLevelMin == -1))
		{
			continue;
		}

		out.push_back(n);
	}

	return out;
}

int CItemOptionBEx::RenderSingleBlock(int TextNum, const std::vector<size_t>& rows, const char* header, int headerColor, int lineColor) const
{
	std::map<int, int> summed;
	std::vector<std::pair<int, int> > perLevel;

	for(size_t k = 0; k < rows.size(); k++)
	{
		const PMSG_ITEM_OPTION_BEX_ROW_RECV& row = m_Rows[rows[k]];

		if(IsPerLevelOption(row.OptionIndex))
		{
			perLevel.push_back(std::make_pair((int)row.OptionIndex, row.OptionValue));
		}
		else
		{
			// Every matching row applies on the server, so they add up here too. That
			// is how the Divine Sword of Archangel's two overlapping blocks show as one
			// honest total instead of two lines that look like a duplicate.
			summed[row.OptionIndex] += EffectOf(row.OptionIndex, row.OptionValue);
		}
	}

	TextNum = AppendLine(TextNum, "\n", TEXT_COLOR_WHITE, false);
	TextNum = AppendLine(TextNum, header, headerColor, true);

	return AppendOptions(TextNum, summed, perLevel, lineColor);
}

// One block of set rows at one level. `preview` = a level the hovered piece has not
// reached: everything greyed, no "(active)" test.
int CItemOptionBEx::RenderSetBlock(int TextNum, const std::vector<size_t>& rows, int level, bool preview) const
{
	// key = (LvMin << 8) | (Exc & 0xFF): rows that need the same pieces share a header
	std::map<int, std::vector<size_t> > groups;

	for(size_t k = 0; k < rows.size(); k++)
	{
		const PMSG_ITEM_OPTION_BEX_ROW_RECV& row = m_Rows[rows[k]];

		groups[(row.ItemLevelMin << 8) | (row.ItemExc & 0xFF)].push_back(rows[k]);
	}

	const int baseClass = (Hero != NULL) ? gCharacterManager.GetBaseClass(Hero->Class) : -1;
	const int need = (baseClass == CLASS_DARK || baseClass == CLASS_RAGEFIGHTER) ? 4 : 5;

	for(std::map<int, std::vector<size_t> >::iterator g = groups.begin(); g != groups.end(); ++g)
	{
		const PMSG_ITEM_OPTION_BEX_ROW_RECV& first = m_Rows[g->second[0]];

		bool active = false;

		if(!preview)
		{
			for(size_t k = 0; k < g->second.size() && !active; k++)
			{
				active = IsSetActive(m_Rows[g->second[k]]);
			}
		}

		char header[128];

		if(first.ItemExc == 0)
		{
			// Mirrors the server: a set row with Exc 0 can never switch on.
			sprintf(header, "Set bonus - never active (misconfigured)");
		}
		else if(preview)
		{
			sprintf(header, "Set bonus at +%d: %d pieces%s", level, need, (first.ItemExc == 1) ? " excellent" : "");
		}
		else if(first.ItemLevelMin > 0)
		{
			sprintf(header, "Set bonus: %d pieces +%d%s%s", need, first.ItemLevelMin, (first.ItemExc == 1) ? " excellent" : "", active ? "  (active)" : "");
		}
		else
		{
			sprintf(header, "Set bonus: %d pieces%s%s", need, (first.ItemExc == 1) ? " excellent" : "", active ? "  (active)" : "");
		}

		const int headerColor = active ? TEXT_COLOR_GREEN : TEXT_COLOR_GRAY;
		const int lineColor = active ? TEXT_COLOR_BLUE : TEXT_COLOR_GRAY;

		TextNum = RenderSingleBlock(TextNum, g->second, header, headerColor, lineColor);
	}

	return TextNum;
}

int CItemOptionBEx::AttachToolTip(const ITEM* ip, int TextNum)
{
	if(ip == NULL || ip->Type == -1 || m_Rows.empty())
	{
		return TextNum;
	}

	const int level = ItemLevelOf(ip);

	// ---- Single-item rows ---------------------------------------------------------
	//
	// Three things can show, in this order:
	//   now        - rows live at the item's current level, in blue
	//   requires   - if NOTHING is live yet, the first level that unlocks something,
	//                greyed, so a +7 weapon whose bonuses start at +9 says so instead
	//                of looking like it has none
	//   next level - the first higher level where the bonuses differ from now, greyed
	//
	// "Differ" rather than "level + 1": a band like 9..12 gives the same thing at +10
	// as at +9, and a preview identical to the current block would just be noise.
	{
		const std::vector<size_t> now = MatchSingleRows(ip, level);

		int nextLevel = -1;
		std::vector<size_t> next;

		for(int lv = level + 1; lv <= 15; lv++)
		{
			std::vector<size_t> at = MatchSingleRows(ip, lv);

			if(!at.empty() && at != now)
			{
				nextLevel = lv;
				next = at;
				break;
			}
		}

		char header[64];

		if(!now.empty())
		{
			TextNum = RenderSingleBlock(TextNum, now, "Bonus options", TEXT_COLOR_YELLOW, TEXT_COLOR_BLUE);

			if(nextLevel != -1)
			{
				sprintf(header, "Next level +%d", nextLevel);
				TextNum = RenderSingleBlock(TextNum, next, header, TEXT_COLOR_GRAY, TEXT_COLOR_GRAY);
			}
		}
		else if(nextLevel != -1)
		{
			sprintf(header, "Bonus options - requires +%d", nextLevel);
			TextNum = RenderSingleBlock(TextNum, next, header, TEXT_COLOR_GRAY, TEXT_COLOR_GRAY);
		}
	}

	// ---- Set rows: on any armour piece of the set ----------------------------------
	const int hoverCat = ip->Type / MAX_ITEM_INDEX;

	if(hoverCat < 7 || hoverCat > 11)
	{
		return TextNum;
	}

	const int hoverIdx = ip->Type % MAX_ITEM_INDEX;

	// Same three cases as above, keyed on the hovered piece's level.
	const std::vector<size_t> now = MatchSetRows(hoverIdx, level);

	int nextLevel = -1;
	std::vector<size_t> next;

	for(int lv = level + 1; lv <= 15; lv++)
	{
		std::vector<size_t> at = MatchSetRows(hoverIdx, lv);

		if(!at.empty() && at != now)
		{
			nextLevel = lv;
			next = at;
			break;
		}
	}

	if(!now.empty())
	{
		TextNum = RenderSetBlock(TextNum, now, level, false);
	}

	if(nextLevel != -1)
	{
		TextNum = RenderSetBlock(TextNum, next, nextLevel, true);
	}

	return TextNum;
}

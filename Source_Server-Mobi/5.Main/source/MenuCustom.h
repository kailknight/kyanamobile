#pragma once

enum ButtonMenuCustom
{
	eButtonEventTime,
	eButtonVipShop,
	eButtonRanking,
	eButtonChangeClass,
	eButtonMarKet,
	eButtonDanhHieu,
	eButtonNapGame,
	eButtonJewelBank,
	eButtonVipChar,
	eButtonHuyDongExc,
	eButtonDoiMatKhau,
	eButtonMocNap,
	eButtonLockItem,
	eButtonVQ,
	eButtonRedeemCode,
	eButtonMaxValue,
};
// Buttons shown per page of the Features grid. The grid used to show every
// enabled button in one page (up to all 15 slots at once, 2 columns x ~8
// rows) - capped here so a full slot list pages instead of cramming.
constexpr int kCustomMenuButtonsPerPage = 12;

class cCustomMenu
{
public:
	cCustomMenu();
	~cCustomMenu();
	void GetCountButton();
	void ActionButton(int TypeButton);
	void Draw();

private:
	int m_CurrentPage;
};
extern cCustomMenu gCustomMenu;
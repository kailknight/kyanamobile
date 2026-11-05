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

	// Where a slot's button was drawn on the last frame (virtual 640x480
	// screen units). False when that button is not on screen right now - the
	// Android tutorial uses this to point at the real Events button.
	bool GetButtonRect(int slot, float* x, float* y, float* w, float* h) const;
	bool IsOpen() const;

private:
	int m_CurrentPage;
	float m_BtnRect[24][4];
	unsigned int m_BtnRectTick[24];
};
extern cCustomMenu gCustomMenu;
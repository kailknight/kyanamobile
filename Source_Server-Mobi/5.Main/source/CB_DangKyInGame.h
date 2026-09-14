#pragma once
#include "Protocol.h"
class CUITextInputBox;

class CB_DangKyInGame
{
	enum TYPE_INPUT_DKTK
	{
		Account,
		Pass,
		Snonumber,
		Phone,
		eMaxINPUT,
	};

	struct PMSG_REGISTER_MAIN_SEND
	{
		PSBMSG_HEAD header; // C1:D2:02
		BYTE TypeSend;
		char account[11];
		char password[21];
		char numcode[8];
		char sodienthoai[14];
	};
	enum eRecvKetQua
	{
		eDangKyThanhCong = 1,
		eTaiKhoanDaTonTai = 2,
		eDuLieuNhapKhongDung = 3,
		eDatLaiMatKhauThanhCong = 11,
		eThongTinBaoMatKhongChinhXac = 12,
	};
public:
	CB_DangKyInGame();
	~CB_DangKyInGame();
	bool RenderWindow(int X, int Y);
	void OpenOnOff();
	bool RequsetDKTK();

	// Same rate limit, validation, messages and packet as RequsetDKTK, but fed
	// text directly instead of reading the three CUITextInputBox controls - the
	// Android registration overlay keeps its own buffers. See the definition.
	bool SubmitRegistration(const char* accountText, const char* passText, const char* snoText);

	// MainInfo RegisterPinCode. Off: no PIN row on either form, and
	// SubmitRegistration sends a placeholder instead of validating one.
	static bool PinCodeEnabled();

	void RecvKQRegInGame(XULY_CGPACKET* lpMsg);
	void Clear();
	
protected:
	CUITextInputBox* CInputData[TYPE_INPUT_DKTK::eMaxINPUT];
	DWORD TimeSendRegTK;
	bool OpenDKTK;
};

extern CB_DangKyInGame* gCB_DangKyInGame;
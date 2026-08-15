#pragma once


class MainLoad
{
public:
	MainLoad();
	virtual ~MainLoad();
	bool Load();

	// Hands the contents of CBGetMain.bin / CBTextInfo.bin to the managers that
	// actually use it. Split out of Load() so Android, which cannot run the
	// Windows-only rest of Load(), can still do this half after reading the
	// files itself.
	void ApplyProtectData();
public:
}; extern MainLoad gMainLoad;
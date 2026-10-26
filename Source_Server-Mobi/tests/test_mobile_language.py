#!/usr/bin/env python3
"""Host regression for the real mobile GameConfig persistence implementation.

Run: python3 tests/test_mobile_language.py
Uses a temporary settings directory and separate processes to emulate cold starts.
Only the heavyweight game stdafx is replaced; language normalization comes from
MultiLanguage.cpp. This does not test UIKit or rendering on a device.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "5.Main/source"


class MobileLanguageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="mu-language-test-")
        cls.work = Path(cls.temp.name)
        config = (SOURCE / "GameConfig/GameConfig.cpp").read_text()
        config = config.replace('#include "stdafx.h"', "", 1)
        (cls.work / "GameConfig.cpp").write_text(config)
        multilingual = (SOURCE / "MultiLanguage.cpp").read_text()
        normalize = multilingual.split("void NormalizeLanguageCode(", 1)[1]
        normalize = "void NormalizeLanguageCode(" + normalize.split("\nconst char* LocalLanguageFile", 1)[0]
        (cls.work / "compat.h").write_text(
            "#include <cstring>\n#include <strings.h>\n"
            "using BYTE = unsigned char;\nusing DWORD = unsigned int;\n"
            "#define _stricmp strcasecmp\n"
        )
        harness = r'''
#include "GameConfig.h"
#include <cassert>
#include <string>
void SetSelectedLanguage(const char*);
NORMALIZE
int main(int argc, char** argv) {
    assert(argc == 3);
    GameConfig& config = GameConfig::GetInstance();
    config.Load();
    if (std::string(argv[1]) == "set") {
        SetSelectedLanguage(argv[2]);
        // Saving unrelated settings must not lose the language key.
        config.SetVolumeLevel(7);
        config.Save();
        const bool vi = strcasecmp(argv[2], "Vtm") == 0
            || strcasecmp(argv[2], "Vie") == 0 || strcasecmp(argv[2], "Vn") == 0;
        assert(config.GetLanguageSelection() == (vi ? L"Vtm" : L"Eng"));
    } else {
        assert(config.GetLanguageSelection() == (std::string(argv[2]) == "Vtm" ? L"Vtm" : L"Eng"));
    }
}
'''.replace("NORMALIZE", normalize)
        (cls.work / "main.cpp").write_text(harness)
        cls.binary = cls.work / "language-test"
        subprocess.run([
            os.environ.get("CXX", "clang++"), "-std=c++20", "-DMU_IOS=1",
            "-include", str(cls.work / "compat.h"),
            "-I", str(SOURCE / "GameConfig"),
            str(cls.work / "GameConfig.cpp"), str(cls.work / "main.cpp"),
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_game(self, action, language):
        subprocess.run([str(self.binary), action, language], cwd=self.work, check=True)

    def test_cold_start_round_trip_and_cancel(self):
        self.run_game("read", "Eng")
        for language in ("Vtm", "Eng", "Vie", "Vn", "eng", "invalid"):
            expected = "Vtm" if language in ("Vtm", "Vie", "Vn") else "Eng"
            self.run_game("set", language)
            self.run_game("read", expected)
            settings = (self.work / "mu_settings.cfg").read_text()
            self.assertIn("Language.Vi=" + ("1" if expected == "Vtm" else "0"), settings)
            self.assertIn("Audio.Volume=7", settings)
        # Pick Vietnamese, then cancel before a cold start, as either UI can do.
        self.run_game("set", "Vtm")
        self.run_game("set", "Eng")
        self.run_game("read", "Eng")

    def test_startup_load_order(self):
        runtime = (SOURCE / "android_main.cpp").read_text()
        load = runtime.index("GameConfig::GetInstance().Load();")
        selection = runtime.index("g_strSelectedML = g_aszMLSelection;", load)
        tables = runtime.index("new CMultiLanguage(g_strSelectedML)", selection)
        reload = runtime.index("gOther.Reload();", tables)
        self.assertLess(load, selection)
        self.assertLess(selection, tables)
        self.assertLess(tables, reload)
        other = (SOURCE / "Other.cpp").read_text()
        self.assertIn("mu_ini::ForgetCachedIniFiles();", other)
        self.assertIn("./Data/Custom/Text_%s.ini", other)


if __name__ == "__main__":
    unittest.main()

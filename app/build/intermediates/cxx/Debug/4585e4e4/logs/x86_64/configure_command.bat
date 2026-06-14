@echo off
"C:\\Users\\KYANa\\AppData\\Local\\Android\\Sdk\\cmake\\3.22.1\\bin\\cmake.exe" ^
  "-HE:\\MU\\MU Mobile\\Source_Server-Mobi\\android\\app\\src\\main\\cpp" ^
  "-DCMAKE_SYSTEM_NAME=Android" ^
  "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" ^
  "-DCMAKE_SYSTEM_VERSION=21" ^
  "-DANDROID_ABI=x86_64" ^
  "-DCMAKE_ANDROID_ARCH_ABI=x86_64" ^
  "-DANDROID_NDK=C:\\Users\\KYANa\\AppData\\Local\\Android\\Sdk\\ndk\\25.1.8937393" ^
  "-DCMAKE_ANDROID_NDK=C:\\Users\\KYANa\\AppData\\Local\\Android\\Sdk\\ndk\\25.1.8937393" ^
  "-DCMAKE_TOOLCHAIN_FILE=C:\\Users\\KYANa\\AppData\\Local\\Android\\Sdk\\ndk\\25.1.8937393\\build\\cmake\\android.toolchain.cmake" ^
  "-DCMAKE_MAKE_PROGRAM=C:\\Users\\KYANa\\AppData\\Local\\Android\\Sdk\\cmake\\3.22.1\\bin\\ninja.exe" ^
  "-DCMAKE_CXX_FLAGS=-std=c++20 -D__ANDROID__ -DANDROID -DNDEBUG -Wno-error -Wno-unknown-pragmas -fexceptions -frtti -DMU_ANDROID_DISABLE_LOG" ^
  "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=E:\\MU\\MU Mobile\\Source_Server-Mobi\\android\\app\\build\\intermediates\\cxx\\Debug\\4585e4e4\\obj\\x86_64" ^
  "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=E:\\MU\\MU Mobile\\Source_Server-Mobi\\android\\app\\build\\intermediates\\cxx\\Debug\\4585e4e4\\obj\\x86_64" ^
  "-DCMAKE_BUILD_TYPE=Debug" ^
  "-BE:\\MU\\MU Mobile\\Source_Server-Mobi\\android\\app\\.cxx\\Debug\\4585e4e4\\x86_64" ^
  -GNinja ^
  "-DANDROID_STL=c++_static" ^
  "-DANDROID_PLATFORM=android-21" ^
  "-DMU_ENABLE_BGFX=OFF"

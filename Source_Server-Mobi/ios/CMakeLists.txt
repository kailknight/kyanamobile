cmake_minimum_required(VERSION 3.22)

# SDL2_ttf's vendored FreeType declares cmake_minimum_required(2.8), which
# CMake 4 rejects outright; this is the documented override for that.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# iOS counterpart of android/app/src/main/cpp/CMakeLists.txt. Same shared
# client source, same exclusion list, built as an iOS app through Xcode.
# Run ./generate.sh rather than invoking this directly.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=iOS (use ./generate.sh).")
endif()

project(WorldOfKiraIOS C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CLIENT_ROOT      "${PROJECT_ROOT}/5.Main")
set(GAME_SRC_DIR     "${CLIENT_ROOT}/source")
set(THIRDPARTY_DIR   "${CLIENT_ROOT}/ThirdParty")
set(ANDROID_MAIN_DIR "${PROJECT_ROOT}/android/app/src/main")
set(DEPS_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(SOKOL_ROOT       "${THIRDPARTY_DIR}/sokol" CACHE PATH "Shared sokol checkout")

set(MU_IOS_TEAM_ID      "" CACHE STRING "Apple development team ID (Xcode > Settings > Accounts)")
set(MU_IOS_BUNDLE_ID    "com.worldofkira" CACHE STRING "App bundle identifier")
set(MU_IOS_BUILD_NUMBER "1" CACHE STRING "CFBundleVersion; also satisfies ClientBuild.h's MU_ANDROID_VERSION_CODE")
set(MU_IOS_VERSION      "1.0.${MU_IOS_BUILD_NUMBER}" CACHE STRING "CFBundleShortVersionString")
# Both off by default, as on Android (MU_ANDROID_DISABLE_LOG). For diagnosis:
#   ./generate.sh then cmake -S . -B build -DMU_IOS_LOGS=ON -DMU_IOS_PERF_LOG=ON
option(MU_IOS_LOGS "Client logging to the Xcode console" OFF)
option(MU_IOS_PERF_LOG "Periodic PERF/PERF_CPU/PERF_PHASE frame-time lines in the console" OFF)

foreach(REQUIRED IN ITEMS "${SOKOL_ROOT}/sokol_app.h" "${DEPS_DIR}/SDL2/CMakeLists.txt" "${DEPS_DIR}/SDL2_ttf/CMakeLists.txt")
    if(NOT EXISTS "${REQUIRED}")
        message(FATAL_ERROR "Missing ${REQUIRED} - run ./fetch_deps.sh (sokol is copied by hand, see its output).")
    endif()
endforeach()

# --- SDL2 + SDL2_ttf, static ------------------------------------------------
# The client uses SDL2 only for surfaces/RWops/SDL_GetTicks and SDL2_ttf for all
# text (Platform/AndroidGDI.cpp). sokol_app owns main() and the GL context, so
# SDL is never initialised and SDL2main is not linked.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL_AUDIO    OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC   OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR   OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI   OFF CACHE BOOL "" FORCE)
set(SDL_POWER    OFF CACHE BOOL "" FORCE)
set(SDL_RENDER   OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS_DIR}/SDL2" "${CMAKE_BINARY_DIR}/SDL2" EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON  CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL  OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS_DIR}/SDL2_ttf" "${CMAKE_BINARY_DIR}/SDL2_ttf" EXCLUDE_FROM_ALL)

# --- Game sources: same glob and exclusions as Android ----------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_SRC_DIR}/*.cpp")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*[Ww]inmain\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*MuEditor.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*imgui.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*GameShop/FileDownloader.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*ProtectSysKey.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Scenes.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*[/\\\\]Camera[/\\\\].*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*StackWalker.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Stack\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*TrayMode\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*UIWindows - Copy\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/Dump.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/Log/WindowsConsole\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/Log/ErrorReport\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/cryptopp.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*CSResourceManager\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/Math\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*ExternalObject/Leaf.*\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*DSplaysound\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*DSwaveIO\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*GCCertification\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*ExternalObject/detours\\.cpp$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*Utilities/mapm/.*DEMO.*\\.[Cc][Pp][Pp]$")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*WSctlc\\.cpp$")
# Editor backups that sit beside real sources.
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*\\.bak.*$")
list(REMOVE_DUPLICATES GAME_SOURCES)

set(IOS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IosAudio.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IosPlatform.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IosPreload.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IosPreloadCore.cpp"
    "${THIRDPARTY_DIR}/miniz/miniz.c"
)

# SOKOL_APP_IMPL lives in SokolRuntime.cpp; on iOS the implementation is UIKit
# Objective-C, so that one file compiles as Objective-C++.
set_source_files_properties("${GAME_SRC_DIR}/Platform/SokolRuntime.cpp"
    PROPERTIES COMPILE_OPTIONS "-xobjective-c++;-fobjc-arc")

# --- Bundled assets ---------------------------------------------------------
# Android ships ui/ and data/ inside the APK and copies them to its files dir on
# start (MuMainNativeActivity.copyAssetFile); iOS bundles the same folders under
# MuAssets/ and IosPlatform.mm copies them into Documents the same way.
file(GLOB_RECURSE BUNDLED_ASSETS CONFIGURE_DEPENDS "${ANDROID_MAIN_DIR}/assets/*")
foreach(ASSET IN LISTS BUNDLED_ASSETS)
    file(RELATIVE_PATH REL "${ANDROID_MAIN_DIR}/assets" "${ASSET}")
    get_filename_component(REL_DIR "${REL}" DIRECTORY)
    set_source_files_properties("${ASSET}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/MuAssets/${REL_DIR}")
endforeach()

set(PRELOAD_BANNERS
    "${ANDROID_MAIN_DIR}/res/mipmap-hdpi/banner_preload_1.png"
    "${ANDROID_MAIN_DIR}/res/mipmap-hdpi/banner_preload_2.png"
)
set_source_files_properties(${PRELOAD_BANNERS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

# AAC copies of the Ogg music tracks, which AVAudioPlayer cannot decode.
file(GLOB IOS_MUSIC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/Music/*.m4a")
set_source_files_properties(${IOS_MUSIC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/Music")

add_executable(WorldOfKira MACOSX_BUNDLE
    ${GAME_SOURCES}
    ${IOS_SOURCES}
    ${BUNDLED_ASSETS}
    ${PRELOAD_BANNERS}
    ${IOS_MUSIC}
)

list(LENGTH GAME_SOURCES GAME_SOURCE_COUNT)
message(STATUS "Game source files count: ${GAME_SOURCE_COUNT}")

# Same reason as Android: the build stamp in ClientBuild.cpp must not go stale.
add_custom_command(
    TARGET WorldOfKira PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${GAME_SRC_DIR}/ClientBuild.cpp"
    COMMENT "Refreshing build stamp timestamp in ClientBuild.cpp")

# iOS shims come first so <android/log.h>, <GLES3/gl32.h> and <EGL/egl.h>
# resolve to them; the Android compat headers (windows.h, gl/gl.h, ...) follow.
target_include_directories(WorldOfKira PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/compat_include"
    "${ANDROID_MAIN_DIR}/cpp/compat_include"
    "${CLIENT_ROOT}/dependencies/include"
    "${SOKOL_ROOT}"
    "${SOKOL_ROOT}/util"
    "${THIRDPARTY_DIR}"
    "${THIRDPARTY_DIR}/miniz"
)

# The game folders are quote-include only (-iquote). The client ships its own
# _types.h and util.h, and Darwin's system headers include <_types.h> and
# <util.h>: on a plain -I path the game's copies would be picked up inside the
# SDK. Android's libc has neither header, which is why this never came up there.
# Every game header is included with quotes, so nothing else changes.
foreach(GAME_INCLUDE_DIR IN ITEMS
        "${GAME_SRC_DIR}"
        "${GAME_SRC_DIR}/ExternalObject"
        "${GAME_SRC_DIR}/GameConfig"
        "${GAME_SRC_DIR}/GameShop"
        "${GAME_SRC_DIR}/OpenGL3"
        "${GAME_SRC_DIR}/Platform"
        "${GAME_SRC_DIR}/Scenes"
        "${GAME_SRC_DIR}/Time"
        "${GAME_SRC_DIR}/Translation"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(WorldOfKira PRIVATE "-iquote${GAME_INCLUDE_DIR}")
endforeach()

target_compile_definitions(WorldOfKira PRIVATE
    MU_IOS
    NDEBUG
    _NDEBUG
    _NO_EDITOR
    USE_OPENGL_ES
    SOKOL_GLES3
    GLES_SILENCE_DEPRECATION
    MU_ANDROID_VERSION_CODE=${MU_IOS_BUILD_NUMBER}
    $<$<NOT:$<BOOL:${MU_IOS_LOGS}>>:MU_ANDROID_DISABLE_LOG>
    $<$<BOOL:${MU_IOS_PERF_LOG}>:MU_ANDROID_PERF_LOG>
)

target_compile_options(WorldOfKira PRIVATE
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-unknown-pragmas>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-deprecated-declarations>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-format-security>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-multichar>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-c++11-narrowing>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-invalid-source-encoding>
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-fno-strict-aliasing>
    $<$<CONFIG:Release>:-O3>
)

target_link_libraries(WorldOfKira PRIVATE
    SDL2::SDL2-static
    SDL2_ttf::SDL2_ttf-static
    z
    "-framework UIKit"
    "-framework AVFoundation"
    "-framework GLKit"
    "-framework OpenGLES"
    "-framework QuartzCore"
    "-framework CoreText"
    "-framework CoreGraphics"
    "-framework Foundation"
)

set_target_properties(WorldOfKira PROPERTIES
    OUTPUT_NAME "WorldOfKira"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "World of Kira"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MU_IOS_BUILD_NUMBER}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MU_IOS_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MU_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${MU_IOS_TEAM_ID}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS "YES"
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI "YES"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH[variant=Debug] "YES"
    XCODE_GENERATE_SCHEME ON
    # Run from Xcode with the optimised build: the client is CPU-bound, and an
    # -O0 build of it is several times slower than what players get.
    XCODE_SCHEME_LAUNCH_CONFIGURATION "Release"
    XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Disabled"
)

cmake_minimum_required(VERSION 3.22)

# SDL2_ttf's vendored FreeType declares cmake_minimum_required(2.8), which
# CMake 4 rejects outright; this is the documented override for that.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# macOS counterpart of ios/CMakeLists.txt: the same shared client source and
# the same Apple glue (ios/src), built as a native Mac app. There is no
# anti-cheat in this build, as on Android and iOS. Run ./generate.sh.

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")

project(WorldOfKiraMac C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CLIENT_ROOT      "${PROJECT_ROOT}/5.Main")
set(GAME_SRC_DIR     "${CLIENT_ROOT}/source")
set(THIRDPARTY_DIR   "${CLIENT_ROOT}/ThirdParty")
set(ANDROID_MAIN_DIR "${PROJECT_ROOT}/android/app/src/main")
set(IOS_DIR          "${PROJECT_ROOT}/ios")
set(DEPS_DIR         "${IOS_DIR}/third_party")
set(SOKOL_ROOT       "${THIRDPARTY_DIR}/sokol" CACHE PATH "Shared sokol checkout")

set(MU_MAC_BUNDLE_ID    "com.worldofkira.mac" CACHE STRING "App bundle identifier")
set(MU_MAC_BUILD_NUMBER "29" CACHE STRING "CFBundleVersion; also satisfies ClientBuild.h's MU_ANDROID_VERSION_CODE")
set(MU_MAC_VERSION      "1.0.28" CACHE STRING "CFBundleShortVersionString")
option(MU_MAC_LOGS "Client logging to the console" OFF)

foreach(REQUIRED IN ITEMS "${SOKOL_ROOT}/sokol_app.h" "${DEPS_DIR}/SDL2/CMakeLists.txt" "${DEPS_DIR}/SDL2_ttf/CMakeLists.txt")
    if(NOT EXISTS "${REQUIRED}")
        message(FATAL_ERROR "Missing ${REQUIRED} - run ios/fetch_deps.sh once.")
    endif()
endforeach()

# --- SDL2 + SDL2_ttf, static (surfaces/RWops/ticks and all text rendering) ---
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

include("${IOS_DIR}/game_sources.cmake")

# Xcode does not treat ".Cpp" (capital C) as C++ source, so this one file was
# never compiled and the link failed on its three symbols. Protect.cpp is
# built for the Mac as well.
set_source_files_properties("${GAME_SRC_DIR}/Utilities/CCRC32.Cpp" PROPERTIES LANGUAGE CXX)

set(MAC_SOURCES
    "${IOS_DIR}/src/IosAudio.mm"
    "${IOS_DIR}/src/IosPlatform.mm"
    "${IOS_DIR}/src/IosPreload.mm"
    "${IOS_DIR}/src/IosPreloadCore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MacVoice.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MacScreenshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MacDiag.mm"
    "${THIRDPARTY_DIR}/miniz/miniz.c"
)

# SOKOL_APP_IMPL lives in SokolRuntime.cpp; sokol's macOS backend is Cocoa
# Objective-C, so that one file compiles as Objective-C++.
set_source_files_properties("${GAME_SRC_DIR}/Platform/SokolRuntime.cpp"
    PROPERTIES COMPILE_OPTIONS "-xobjective-c++;-fobjc-arc")
set_source_files_properties("${IOS_DIR}/src/IosPreload.mm" PROPERTIES OBJECT_DEPENDS "${IOS_DIR}/src/MacPreloadOverlay.inc")

# --- Bundled assets: the same ui/ and data/ the APK carries ------------------
file(GLOB_RECURSE BUNDLED_ASSETS CONFIGURE_DEPENDS "${ANDROID_MAIN_DIR}/assets/*")
foreach(ASSET IN LISTS BUNDLED_ASSETS)
    file(RELATIVE_PATH REL "${ANDROID_MAIN_DIR}/assets" "${ASSET}")
    get_filename_component(REL_DIR "${REL}" DIRECTORY)
    set_source_files_properties("${ASSET}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/MuAssets/${REL_DIR}")
endforeach()

set(PRELOAD_BANNERS
    "${ANDROID_MAIN_DIR}/res/mipmap-hdpi/banner_preload_1.jpg"
    "${ANDROID_MAIN_DIR}/res/mipmap-hdpi/banner_preload_2.jpg"
)
set_source_files_properties(${PRELOAD_BANNERS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

# AAC copies of the Ogg music tracks, which AVAudioPlayer cannot decode.
file(GLOB MAC_MUSIC CONFIGURE_DEPENDS "${IOS_DIR}/assets/Music/*.m4a")
set_source_files_properties(${MAC_MUSIC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/Music")

add_executable(WorldOfKira MACOSX_BUNDLE
    ${GAME_SOURCES}
    ${MAC_SOURCES}
    ${BUNDLED_ASSETS}
    ${PRELOAD_BANNERS}
    ${MAC_MUSIC}
)

list(LENGTH GAME_SOURCES GAME_SOURCE_COUNT)
message(STATUS "Game source files count: ${GAME_SOURCE_COUNT}")

add_custom_command(
    TARGET WorldOfKira PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${GAME_SRC_DIR}/ClientBuild.cpp"
    COMMENT "Refreshing build stamp timestamp in ClientBuild.cpp")

# The macOS shims (GL, EGL) come first, then the iOS ones for the Android
# headers the shared code still names (log.h, input.h, system_properties.h),
# then the Android compat headers (windows.h, gl/gl.h, ...).
target_include_directories(WorldOfKira PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/compat_include"
    "${IOS_DIR}/compat_include"
    "${ANDROID_MAIN_DIR}/cpp/compat_include"
    "${CLIENT_ROOT}/dependencies/include"
    "${SOKOL_ROOT}"
    "${SOKOL_ROOT}/util"
    "${THIRDPARTY_DIR}"
    "${THIRDPARTY_DIR}/miniz"
)

# Quote-only game include folders, for the same reason as on iOS: the game
# ships _types.h and util.h, which Darwin's own headers also include.
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
        "${IOS_DIR}/src")
    target_compile_options(WorldOfKira PRIVATE "-iquote${GAME_INCLUDE_DIR}")
endforeach()

# MU_IOS selects the shared "Apple port" code paths (sokol, no Win32, no JNI);
# MU_MACOS marks what differs on a desktop.
target_compile_definitions(WorldOfKira PRIVATE
    MU_IOS
    MU_MACOS
    NDEBUG
    _NDEBUG
    _NO_EDITOR
    USE_OPENGL_ES
    GL_SILENCE_DEPRECATION
    MU_ANDROID_VERSION_CODE=${MU_MAC_BUILD_NUMBER}
    $<$<NOT:$<BOOL:${MU_MAC_LOGS}>>:MU_ANDROID_DISABLE_LOG>
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
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework AVFoundation"
    "-framework CoreAudio"
    "-framework IOKit"
    "-framework QuartzCore"
    "-framework CoreText"
    "-framework CoreGraphics"
    "-framework Foundation"
)

set_target_properties(WorldOfKira PROPERTIES
    OUTPUT_NAME "WorldOfKira"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "World of Kira"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${MU_MAC_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MU_MAC_BUILD_NUMBER}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MU_MAC_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MU_MAC_BUNDLE_ID}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS "YES"
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI "YES"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_GENERATE_SCHEME ON
)

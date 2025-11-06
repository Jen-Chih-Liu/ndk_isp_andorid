@echo off
echo ===========================================
echo  NDK USB Tool Build Script (Simple)
echo ===========================================

REM -----------------------------------------------------------------
REM (1) 請修改此處為您 NDK 的實際路徑
set NDK_ROOT=C:\android-ndk-r27d
REM -----------------------------------------------------------------

REM (2) 設定目標架構 (64位元 Android) 和 API 等級
set ANDROID_ABI=arm64-v8a
set ANDROID_PLATFORM=android-12

REM (3) 設定 CMake 和 Ninja 的路徑
set CMAKE_PATH=C:\Program Files\CMake\bin\cmake.exe
set NINJA_PATH=C:\ninja-win\ninja.exe

REM (4) 設定建置資料夾
set BUILD_DIR=%~dp0build

REM (5) ★ 關鍵修改 ★
REM    將 SOURCE_DIR 指向 CMakeLists.txt 所在的位置
set SOURCE_DIR=%~dp0app\src

echo NDK Path: %NDK_ROOT%
echo Build Dir: %BUILD_DIR%

REM (6) 建立建置資料夾 (如果不存在)
if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%"
)

REM (7) 進入建置資料夾
cd /d "%BUILD_DIR%"

REM (8) 執行 CMake (產生建置檔案)
echo Running CMake...
call "%CMAKE_PATH%" ^
    -DCMAKE_TOOLCHAIN_FILE="%NDK_ROOT%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI=%ANDROID_ABI% ^
    -DANDROID_PLATFORM=%ANDROID_PLATFORM% ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%" ^
    -G "Ninja" ^
    "%SOURCE_DIR%"

REM (9) 執行 Ninja (編譯)
echo Running Ninja (Compile)...
call "%NINJA_PATH%"

if %ERRORLEVEL% equ 0 (
    echo ===========================================
    echo  Build SUCCESS!
    echo  Your executables are in: %BUILD_DIR%
    echo ===========================================
) else (
    echo ===========================================
    echo  Build FAILED!
    echo ===========================================
)

cd /d %~dp0
pause
@echo off
setlocal EnableExtensions

cd /d "%~dp0"

set "EGE_CONFIGURATION=Debug"
set "EGE_VS_PRESET=auto"
set "EGE_ACTION=build"

:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="Debug" (
    set "EGE_CONFIGURATION=Debug"
) else if /I "%~1"=="Release" (
    set "EGE_CONFIGURATION=Release"
) else if /I "%~1"=="vs2022" (
    set "EGE_VS_PRESET=vs2022"
) else if /I "%~1"=="vs2026" (
    set "EGE_VS_PRESET=vs2026"
) else if /I "%~1"=="auto" (
    set "EGE_VS_PRESET=auto"
) else if /I "%~1"=="generate" (
    set "EGE_ACTION=generate"
) else if /I "%~1"=="build" (
    set "EGE_ACTION=build"
) else if /I "%~1"=="rebuild" (
    set "EGE_ACTION=rebuild"
) else if /I "%~1"=="clean" (
    set "EGE_ACTION=clean"
) else if /I "%~1"=="help" (
    goto usage
) else if /I "%~1"=="--help" (
    goto usage
) else if /I "%~1"=="/?" (
    goto usage
) else (
    echo [ERROR] Unknown argument: %~1
    echo.
    goto usage_error
)

shift
goto parse_args

:args_done
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found in PATH.
    echo Install CMake 3.24 or newer, then open a new terminal.
    exit /b 1
)

if /I "%EGE_VS_PRESET%"=="auto" call :detect_visual_studio
if errorlevel 1 exit /b %errorlevel%

if /I "%EGE_CONFIGURATION%"=="Debug" (
    set "EGE_BUILD_PRESET=debug"
) else (
    set "EGE_BUILD_PRESET=release"
)

if /I "%EGE_VS_PRESET%"=="vs2026" (
    set "EGE_BUILD_PRESET=%EGE_BUILD_PRESET%-vs2026"
)

echo [EGE] Configure preset : %EGE_VS_PRESET%
echo [EGE] Configuration    : %EGE_CONFIGURATION%
echo [EGE] Action           : %EGE_ACTION%
echo.

set "EGE_BUILD_DIRECTORY=build\%EGE_VS_PRESET%"
set "EGE_NEEDS_CONFIGURE=0"

if /I "%EGE_ACTION%"=="generate" set "EGE_NEEDS_CONFIGURE=1"
if not exist "%EGE_BUILD_DIRECTORY%\CMakeCache.txt" set "EGE_NEEDS_CONFIGURE=1"

if "%EGE_NEEDS_CONFIGURE%"=="1" (
    echo [EGE] Configuring and generating the Visual Studio solution...
    cmake --preset "%EGE_VS_PRESET%"
    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configuration failed.
        exit /b 1
    )
) else (
    echo [EGE] Reusing the configured build directory:
    echo       %EGE_BUILD_DIRECTORY%
)

if /I "%EGE_ACTION%"=="generate" (
    echo.
    echo [EGE] Visual Studio solution generated:
    if exist "%EGE_BUILD_DIRECTORY%\EduGameEngine.slnx" (
        echo       %EGE_BUILD_DIRECTORY%\EduGameEngine.slnx
    ) else (
        echo       %EGE_BUILD_DIRECTORY%\EduGameEngine.sln
    )
    exit /b 0
)

if /I "%EGE_ACTION%"=="clean" (
    echo [EGE] Cleaning %EGE_CONFIGURATION% build outputs...
    cmake --build --preset "%EGE_BUILD_PRESET%" --target clean
    exit /b %errorlevel%
)

if /I "%EGE_ACTION%"=="rebuild" (
    echo [EGE] Rebuilding Engine...
    cmake --build --preset "%EGE_BUILD_PRESET%" --target Engine --clean-first
) else (
    echo [EGE] Building Engine...
    cmake --build --preset "%EGE_BUILD_PRESET%" --target Engine
)

if errorlevel 1 (
    echo.
    echo [ERROR] Engine build failed.
    exit /b 1
)

echo.
echo [EGE] Build completed successfully:
echo       %EGE_BUILD_DIRECTORY%\bin\%EGE_CONFIGURATION%\Engine.exe
exit /b 0

:detect_visual_studio
set "EGE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "EGE_VS_MAJOR="

if exist "%EGE_VSWHERE%" (
    for /f "usebackq tokens=1 delims=." %%V in (`"%EGE_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do (
        set "EGE_VS_MAJOR=%%V"
    )
)

if "%EGE_VS_MAJOR%"=="18" (
    set "EGE_VS_PRESET=vs2026"
    exit /b 0
)

if "%EGE_VS_MAJOR%"=="17" (
    set "EGE_VS_PRESET=vs2022"
    exit /b 0
)

echo [ERROR] Visual Studio 2022 or 2026 with the C++ workload was not found.
echo You can also choose explicitly: build.bat Debug vs2022
exit /b 1

:usage
echo Edu Game Engine build helper
echo.
echo Usage:
echo   build.bat [Debug^|Release] [auto^|vs2022^|vs2026] [build^|generate^|rebuild^|clean]
echo.
echo Examples:
echo   build.bat
echo   build.bat Release
echo   build.bat Debug vs2026 generate
echo   build.bat Release vs2022 rebuild
exit /b 0

:usage_error
call :usage
exit /b 1

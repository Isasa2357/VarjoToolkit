@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"

if not defined VARJO_SDK_ROOT (
    set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
)

set "BUILD_DIR=%ROOT_DIR%\out\build\coverage"
set "REPORT_DIR=%ROOT_DIR%\out\coverage"
set "CONFIG=Debug"
set "EXCLUDE_REGEX=VarjoToolkitCoreSmokeTest"

set "OCC_EXE=%ProgramFiles%\OpenCppCoverage\OpenCppCoverage.exe"
if not exist "%OCC_EXE%" set "OCC_EXE=%ProgramFiles(x86)%\OpenCppCoverage\OpenCppCoverage.exe"
if not exist "%OCC_EXE%" (
    echo [ERROR] OpenCppCoverage.exe was not found.
    echo         Install OpenCppCoverage or set PATH so it can be found at the default location.
    exit /b 1
)

for /f "delims=" %%I in ('where ctest 2^>nul') do (
    if not defined CTEST_EXE set "CTEST_EXE=%%I"
)
if not defined CTEST_EXE (
    echo [ERROR] ctest.exe was not found in PATH.
    exit /b 1
)

if not defined VSINSTALLDIR (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%I"
    )
)

if defined VSINSTALLDIR (
    if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if not exist "%VARJO_SDK_ROOT%\include\Varjo.h" (
    echo [ERROR] Varjo SDK include directory was not found: %VARJO_SDK_ROOT%\include
    echo         Set VARJO_SDK_ROOT before running this script.
    exit /b 1
)

if not exist "%VARJO_SDK_ROOT%\lib\VarjoLib.lib" (
    echo [ERROR] VarjoLib.lib was not found: %VARJO_SDK_ROOT%\lib\VarjoLib.lib
    echo         Set VARJO_SDK_ROOT before running this script.
    exit /b 1
)

set "EXPERIMENTAL_ARG=-DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=OFF"
if exist "%VARJO_SDK_ROOT%\include_experimental\Varjo_mr_experimental.h" (
    set "EXPERIMENTAL_ARG=-DVARJO_EXPERIMENTAL_INCLUDE_DIR=%VARJO_SDK_ROOT%\include_experimental -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON"
)

rmdir /s /q "%BUILD_DIR%" 2>nul
rmdir /s /q "%REPORT_DIR%" 2>nul
mkdir "%REPORT_DIR%" >nul 2>nul

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_INCLUDE_DIR="%VARJO_SDK_ROOT%\include" ^
  -DVARJO_LIBRARY="%VARJO_SDK_ROOT%\lib\VarjoLib.lib" ^
  %EXPERIMENTAL_ARG% ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 exit /b 1

"%OCC_EXE%" ^
  --sources "%ROOT_DIR%\src" ^
  --sources "%ROOT_DIR%\include" ^
  --excluded_sources "%ROOT_DIR%\out" ^
  --excluded_sources "%ROOT_DIR%\tests" ^
  --excluded_sources "%ROOT_DIR%\samples" ^
  --cover_children ^
  --export_type html:"%REPORT_DIR%\html" ^
  --export_type cobertura:"%REPORT_DIR%\coverage.xml" ^
  -- "%CTEST_EXE%" --test-dir "%BUILD_DIR%" -C %CONFIG% -E %EXCLUDE_REGEX% --output-on-failure
if errorlevel 1 exit /b 1

echo [OK] Coverage report written to:
echo      %REPORT_DIR%\html\index.html
echo      %REPORT_DIR%\coverage.xml
exit /b 0

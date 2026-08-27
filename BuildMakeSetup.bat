@echo off
setlocal

REM ======================================================================
REM  BuildMakeSetup.bat - Windows x64 + x86 static release build + package
REM
REM  Produces:
REM    build\release\Release\MultidocViewer.wlx64     (x64 built plugin)
REM    build\release-x86\Release\MultidocViewer.wlx   (x86 built plugin)
REM    dist\release\MultidocViewer.wlx64               (packaged x64)
REM    dist\release\MultidocViewer.wlx                 (packaged x86)
REM    dist\wlx-multidoc-viewer-Win-YYYYMMDD.zip       (bundle)
REM
REM  Can be run from a plain cmd console (no Developer prompt needed):
REM  Visual Studio is located automatically via vswhere.
REM ======================================================================

set "ROOT=%~dp0"
cd /d "%ROOT%"

REM --- 1. Locate MSVC and initialize the x64 host environment --------
set "vsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%p in (`"%vsWhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSSetup=%%p"
if not defined VSSetup (
    echo [ERROR] Visual Studio 2022 with C++ tools not found.
    exit /b 1
)
call "%VSSetup%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 ( echo [ERROR] vcvarsall.bat failed.& exit /b 1 )

REM --- 2. Tell vcpkg which Visual Studio to use (avoids re-detection) ---
set "VCPKG_VISUAL_STUDIO_PATH=%VSSetup%"

REM --- 3. Refuse to run if another vcpkg is active -------------------
tasklist /FI "IMAGENAME eq vcpkg.exe" 2>nul | find /i "vcpkg.exe" >nul && (
    echo [ERROR] Another vcpkg.exe is running. Close it and re-run.
    exit /b 1
)

REM --- 4. Configure + build the x64 and x86 static releases ----------
cmake --preset windows-x64-release || ( echo [ERROR] configure x64 failed& exit /b 1 )
cmake --build --preset windows-release  || ( echo [ERROR] build x64 failed& exit /b 1 )

cmake --preset windows-x86-release || ( echo [ERROR] configure x86 failed& exit /b 1 )
cmake --build --preset windows-x86-release || ( echo [ERROR] build x86 failed& exit /b 1 )

REM --- Package both into dist\release (outside the build tree); the wlx
REM        and wlx64 plugins sit at the archive root, next to pluginst.inf
set "OUT=%ROOT%dist\release"
if exist "%OUT%" rmdir /S /Q "%OUT%"
mkdir "%OUT%"

copy /Y "build\release\Release\MultidocViewer.wlx64" "%OUT%\" >nul || (
    echo [ERROR] x64 plugin .wlx64 not found.& exit /b 1 )
copy /Y "build\release-x86\Release\MultidocViewer.wlx"  "%OUT%\" >nul || (
    echo [ERROR] x86 plugin .wlx not found.& exit /b 1 )

copy /Y "pluginst.inf" "%OUT%\" >nul

REM --- compress the package (zip goes OUTSIDE the packaged dir so it
REM        doesn't try to archive itself / self-lock; remove any previous
REM        build's archive first so repeated runs don't collide) --------
for /f %%d in ('powershell.exe -nologo -noprofile -command "(Get-Date).ToString('yyyyMMdd')"') do set "STAMP=%%d"
set "ZIP=%ROOT%dist\wlx-multidoc-viewer-Win-%STAMP%.zip"
if exist "%ZIP%" del /F /Q "%ZIP%"
powershell.exe -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::CreateFromDirectory('%OUT%', '%ZIP%'); }"
if errorlevel 1 ( echo [ERROR] zip failed& exit /b 1 )

echo.
echo SUCCESS
echo   plugin x64: %OUT%\MultidocViewer.wlx64
echo   plugin x86: %OUT%\MultidocViewer.wlx
echo   bundle:     %ZIP%
endlocal
exit /b 0

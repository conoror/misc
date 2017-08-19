@echo off
@echo off

rem Two echo offs just in case UTF-8 with byte order marker (github...)

setlocal enableextensions enabledelayedexpansion

set ppversion=15
set ppurl=https://raw.githubusercontent.com/conoror/misc/master/wupatch/pullpatches.bat

set msdownloadpath=http://download.windowsupdate.com
set mypathname=%~f0
set mydirname=%~dp0
set wmicpath=%windir%\System32\wbem\wmic.exe
set bitsadmin=%windir%\System32\bitsadmin.exe

if not exist "%wmicpath%" (
    echo Error: Need wmic.exe to function properly...
    goto :EOF
)

if not exist "%bitsadmin%" (
    echo Error: Need bitsadmin.exe to download files
    goto :EOF
)

if "X%~1"=="X" (
    echo Windows 7 64 bit Patch checker, Conor O'Rourke 2017, Public domain
    echo.
    echo Usage: Supply a single parameter to this script:
    echo    check      Check and download updates
    echo    help       More help
    echo    updateme   Attempts to pull a new version of this script
    echo    version    Prints the version number for use with update
    echo.
    goto :EOF
)

if "%~1"=="updateme" goto selfupdating
if "%~1"=="version" (
    echo %ppversion%
    goto :EOF
)

if "%~1"=="help" (
    echo This script checks to see if you have the July 2016 rollup
    echo and its dependencies, the service stack updates. If you
    echo do not, it offers to download them.
    echo This will speed up the Windows Update search from hours
    echo or even days to minutes.
    echo If you do have this rollup, the script offers to download
    echo any Security ONLY quality updates that are pending.
    echo.
    echo It is perfectly fine to run this script on a brand new
    echo installation of Windows 7 SP1. After the July 2016 rollup
    echo you should then scan for updates.
    echo If you do not want telemetry and other monitoring software
    echo from Microsoft, DO NOT select Security Monthly rollups:
    echo.
    echo     November, 2016 Security Monthly Quality Rollup...
    echo.    ^^^^^^^^^^^ !!!NO NO NO!!! ^^^^^^^^^^^^^^^^^^^^^^
    echo.
    echo Right click and hide this update! Then install the rest
    echo and then rerun this script to manually pull the security
    echo only updates
    echo.
    goto :EOF
)

if NOT "%~1"=="check" (
    echo Only parameters allowed are: check, updateme, version, help
    goto :EOF
)

set winversion=None
ver | find "6.1." > NUL && set winversion=windows6.1

if NOT "%winversion%"=="windows6.1" (
    echo Error: You are not using Windows 7
    goto :EOF
)

set winarch=x86
"%wmicpath%" COMPUTERSYSTEM GET SystemType | find /i "x64" > NUL && set winarch=x64

if NOT "%winarch%"=="x64" (
    echo Error: You are not using Windows 7 64 bit
    goto :EOF
)

echo Using Windows 7 64 bit and following the guide at:
echo    https://support.microsoft.com/en-us/help/22801
echo.
echo Searching for patches

set patchlist=

for /F "usebackq delims=- tokens=2,3" %%a in (`findstr /R "^/./msdownload/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%"`) do (
    set patchname=%%a
    echo.!patchname! | findstr /I /R "^kb" > NUL
    if errorlevel 1 set patchname=%%b
    set patchlist=!patchlist! !patchname!
)

if "X%patchlist%"=="X" (
    echo There are no patches applicable to your operating system
    goto :EOF
)

echo Patches %patchlist% | findstr /I "kb" > NUL
if errorlevel 1 (
    echo Problem with patchlist, possibly a scripting problem
    goto :EOF
)

echo Complete patchlist: %patchlist%
echo Scanning Windows database for patches...takes a minute...

set foundlist=

for /f "usebackq" %%a in (`^""%wmicpath%" qfe get hotfixid ^| findstr /I "%patchlist%"^"`) do (
    set foundlist=!foundlist! %%a
)

REM Start with the servicing stack prerequisites and filter them

set sstacklist=kb3020369 kb3177467
set ihavesspatch=
set updatelist=
set missinglist=

for %%a in (%sstacklist%) do (
    echo.%foundlist% | find /I "%%a" > NUL
    if NOT errorlevel 1 set ihavesspatch=!ihavesspatch! %%a
)

for %%a in (%patchlist%) do (
    echo %sstacklist% | find /I "%%a" > NUL
    if errorlevel 1 set updatelist=!updatelist! %%a
)

for %%a in (%updatelist%) do (
    echo.%foundlist% | find /I "%%a" > NUL
    if errorlevel 1 set missinglist=!missinglist! %%a
)

REM Decide what to download next

if "X%ihavesspatch%"=="X" (
    echo You do not have any of the servicing stacks...
    call :downloadfile kb3177467
    goto :EOF
)

if "X%missinglist%"=="X" (
    echo All current updates are present. You are done...
    goto :EOF
)

echo You are missing: %missinglist%
call :downloadfile %missinglist%
echo Done
goto :EOF



:downloadfile

if "X%~1"=="X" (
    echo Download file routine called with nothing to download
    echo Oh dear....
    goto :EOF
)

echo Next patch to download is: %~1

set pullurl=
for /F "usebackq" %%a in (`findstr /R "^/./msdownload/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%" ^| findstr /I "%1"`) do (
    set pullurl=%%a
)

if "X%pullurl%"=="X" (
    echo Something went wrong finding that patch
    goto :EOF
)

set pullurl=%msdownloadpath%%pullurl%

set pullurllocal=

for /F "usebackq delims=_ tokens=1" %%A in (`echo %pullurl%`) do (
    set pullurllocal=%%~nxA.msu
)

if "X%pullurllocal%"=="X" (
    echo Something went wrong separating the name!
    goto :EOF
)

echo The download url is: %pullurl%
echo Download to file: %pullurllocal%

set /p downloadyn=Would you like me to download those patches for you?

echo.%downloadyn% | findstr /I "y" > NUL
if errorlevel 1 (
    echo That means no, exiting now
    goto :EOF
)

echo Downloading patches into current directory

"%bitsadmin%" /transfer "Wantsomepatches" "%pullurl%" "%mydirname%%pullurllocal%"

echo.
echo Download complete. You can check the file is genuine in properties:
echo Right click file and click properties, check the file has a digital
echo signatures tab and check the signature by highlighting one and clicking
echo details.
echo.
echo Then install the file by double clicking on it...
echo.

echo %1 | findstr /I "kb3172605 kb3172614" > NUL
if NOT errorlevel 1 (
    echo This is the July 2016 rollup.
    echo You should scan for updates after install and reboot
    echo If you do not want telemetry and other monitoring software
    echo from Microsoft, DO NOT select Security Monthly rollups
)

goto :EOF



:selfupdating

echo Attemping to update this batch file!
echo Current version: %ppversion%

"%bitsadmin%" /transfer "Wantsomepatches" "%ppurl%" "%mydirname%pullpatches.new.bat"

if not exist "%mydirname%pullpatches.new.bat" (
    echo The download of pullpatches.new.bat did not happen
    goto :EOF
)

set newppversion=

for /F "usebackq" %%a in (`"%mydirname%pullpatches.new.bat" version`) do (
    set newppversion=%%a
)

if "X%newppversion%"=="X" (
    echo Something went wrong getting the version number!
    goto :EOF
)

echo Downloaded version is version %newppversion%

if %newppversion% LEQ %ppversion% (
    echo There is no later version than the one you have
    del "%mydirname%pullpatches.new.bat"
    goto :EOF
)

echo Newer version discovered. Updating as nicely as possible...
echo.
(
    cmd /C move /-Y "%mydirname%pullpatches.new.bat" "%mydirname%pullpatches.bat"
    exit /B
)

goto :EOF


Windows database follows from this point. These are not valid batch commands!
The http part is elided for brevity I suppose.

Servicing stack prerequisites, just need one of these
-----------------------------------------------------

/d/msdownload/update/software/updt/2015/04/windows6.1-kb3020369-x64_5393066469758e619f21731fc31ff2d109595445.msu
/d/msdownload/update/software/crup/2016/08/windows6.1-kb3177467-x64_42467e48b4cfeb44112d819f50b0557d4f9bbb2f.msu


July 2016 Rollup: KB3172605
---------------------------

/d/msdownload/update/software/updt/2016/09/windows6.1-kb3172605-x64_2bb9bc55f347eee34b1454b50c436eb6fd9301fc.msu


Security only Updates for Oct,Nov,Dec 2016
------------------------------------------

/d/msdownload/update/software/secu/2016/10/windows6.1-kb3192391-x64_8acd94d8d268a6507c2852b0d9917f4ae1349b6c.msu
/c/msdownload/update/software/secu/2016/11/windows6.1-kb3197867-x64_6f8f45a5706eeee8ac05aa16fa91c984a9edb929.msu
/c/msdownload/update/software/secu/2016/12/windows6.1-kb3205394-x64_71d0c657d24bc852f074996c32987fb936c07774.msu


Security only Updates for Jan-Jul 2017
--------------------------------------

/c/msdownload/update/software/secu/2017/01/windows6.1-kb3212642-x64_f3633176091129fc428d899c93545bdc7821e689.msu
/d/msdownload/update/software/secu/2017/02/windows6.1-kb4012212-x64_2decefaa02e2058dcd965702509a992d8c4e92b3.msu
/c/msdownload/update/software/secu/2017/03/windows6.1-kb4015546-x64_4ff5653990d74c465d48adfba21aca6453be99aa.msu
/c/msdownload/update/software/secu/2017/05/windows6.1-kb4019263-x64_d64d8b6f91434754fdd2a552d8732c95a6e64f30.msu
/c/msdownload/update/software/secu/2017/06/windows6.1-kb4022722-x64_ee5b5fae02d1c48dbd94beaff4d3ee4fe3cd2ac2.msu
/c/msdownload/update/software/secu/2017/07/windows6.1-kb4025337-x64_c013b7fcf3486a0f71c4f58fc361bfdb715c4e94.msu


Security only and IE11 (IE is cumulative) Updates for August 2017
-----------------------------------------------------------------

/c/msdownload/update/software/secu/2017/07/ie11-windows6.1-kb4034733-x64_f2e92d2ce145e064731ce72cf2d10910d81a50ff.msu
/c/msdownload/update/software/secu/2017/07/windows6.1-kb4034679-x64_ccabab6aefd6c16454fac39163ae5abc2f4f1303.msu


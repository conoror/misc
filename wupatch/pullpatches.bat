@echo off
@echo off

rem Two echo offs just in case UTF-8 with byte order marker (github...)

setlocal enableextensions enabledelayedexpansion

set ppversion=7
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
    echo Patch checker, Conor O'Rourke 2016, Public domain
    echo Original concept from: http://wu.krelay.de/en/ with
    echo massive inputs from askwoody.com. Grateful thanks to both
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
    echo Only parameters allowed are: check, update, version, help
    goto :EOF
)

set winversion=
ver | find "6.1." > NUL && set winversion=windows6.1
ver | find "6.3." > NUL && set winversion=windows8.1

if "X%winversion%"=="X" (
    echo Error: Windows version is not 7 or 8.1. Sorry...
    goto :EOF
)

if "%winversion%"=="windows6.1" (
    echo Using Windows 7 and following the guide at:
    echo    https://support.microsoft.com/en-us/help/22801
)

if "%winversion%"=="windows8.1" (
    echo Using Windows 8.1 and following the guide at:
    echo    https://support.microsoft.com/en-gb/help/24717
)

set winarch=x86
"%wmicpath%" COMPUTERSYSTEM GET SystemType | find /i "x64" > NUL && set winarch=x64

echo Windows architecture is: %winarch%
echo.
echo Searching for patches

set patchlist=

for /F "usebackq delims=- tokens=2" %%a in (`findstr /R "^/./msdownload/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%"`) do (
    set patchlist=!patchlist! %%a
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

echo Found patches: %foundlist%

REM Start with the servicing stack prerequisites and filter them

set sstacklist=kb3020369 kb3177467 kb3021910 kb3173424
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

    if "%winversion%"=="windows6.1" call :downloadfile kb3177467
    if NOT "%winversion%"=="windows6.1" call :downloadfile kb3173424

    goto :EOF
)

if "X%missinglist%"=="X" (
    echo All current updates are present. You are done...
    goto :EOF
)

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

/d/msdownload/update/software/updt/2015/04/windows6.1-kb3020369-x86_82e168117c23f7c479a97ee96c82af788d07452e.msu
/d/msdownload/update/software/updt/2015/04/windows6.1-kb3020369-x64_5393066469758e619f21731fc31ff2d109595445.msu

/d/msdownload/update/software/crup/2016/08/windows6.1-kb3177467-x86_7fa40e58f6a8e56eb78b09502e5c8c6c1acf0158.msu
/d/msdownload/update/software/crup/2016/08/windows6.1-kb3177467-x64_42467e48b4cfeb44112d819f50b0557d4f9bbb2f.msu

/c/msdownload/update/software/updt/2015/04/windows8.1-kb3021910-x86_7e70173bec00c3d4fe3b0b8cba17b095b4ed2d20.msu
/c/msdownload/update/software/updt/2015/04/windows8.1-kb3021910-x64_e291c0c339586e79c36ebfc0211678df91656c3d.msu

/d/msdownload/update/software/crup/2016/06/windows8.1-kb3173424-x86_fcf7142a388a08fde7c54f23e84450f18a8aaec5.msu
/d/msdownload/update/software/crup/2016/06/windows8.1-kb3173424-x64_9a1c9e0082978d92abee71f2cfed5e0f4b6ce85c.msu


July 2016 Rollup: 3172605 for W7, 3172614 for W8.1
--------------------------------------------------

/d/msdownload/update/software/updt/2016/09/windows6.1-kb3172605-x86_ae03ccbd299e434ea2239f1ad86f164e5f4deeda.msu
/d/msdownload/update/software/updt/2016/09/windows6.1-kb3172605-x64_2bb9bc55f347eee34b1454b50c436eb6fd9301fc.msu
/c/msdownload/update/software/updt/2016/07/windows8.1-kb3172614-x86_d11c233c8598b734de72665e0d0a3f2ef007b91f.msu
/c/msdownload/update/software/updt/2016/07/windows8.1-kb3172614-x64_e41365e643b98ab745c21dba17d1d3b6bb73cfcc.msu


Security only Updates for October 2016: 3192391 for W7, 3192392 for W8.1
------------------------------------------------------------------------

/d/msdownload/update/software/secu/2016/10/windows6.1-kb3192391-x86_a9d1e3f0dea012e3a331930bc1cd975005827cb6.msu
/d/msdownload/update/software/secu/2016/10/windows6.1-kb3192391-x64_8acd94d8d268a6507c2852b0d9917f4ae1349b6c.msu
/d/msdownload/update/software/secu/2016/10/windows8.1-kb3192392-x86_e585ffbfd1059b9b2c3a1c89da3b316145dc9d36.msu
/c/msdownload/update/software/secu/2016/10/windows8.1-kb3192392-x64_2438d75e9a0f6e38e61f992e70c832bc706a2b27.msu


Security only Updates for November 2016: 3197867 for W7, 3197873 for W8.1
-------------------------------------------------------------------------

/c/msdownload/update/software/secu/2016/11/windows6.1-kb3197867-x86_2313232edda5cca08115455d91120ab3790896ba.msu
/c/msdownload/update/software/secu/2016/11/windows6.1-kb3197867-x64_6f8f45a5706eeee8ac05aa16fa91c984a9edb929.msu
/c/msdownload/update/software/secu/2016/11/windows8.1-kb3197873-x86_b906109f30b735290a431fdc8397249cfcc3e84b.msu
/d/msdownload/update/software/secu/2016/11/windows8.1-kb3197873-x64_cd0325f40c0d25960e462946f6b736aa7c0ed674.msu


Security only Updates for December 2016: 3205394 for W7, 3205400 for W8.1
-------------------------------------------------------------------------

/d/msdownload/update/software/secu/2016/12/windows6.1-kb3205394-x86_e477192f301b1fbafc98deb94af80c6e94231e54.msu
/c/msdownload/update/software/secu/2016/12/windows6.1-kb3205394-x64_71d0c657d24bc852f074996c32987fb936c07774.msu
/c/msdownload/update/software/secu/2016/11/windows8.1-kb3205400-x86_4529e446e7e929ee665579bdd5c23aa091ab862e.msu
/c/msdownload/update/software/secu/2016/11/windows8.1-kb3205400-x64_ad92909c51b52a6890932a8c5bd32059a102ec65.msu


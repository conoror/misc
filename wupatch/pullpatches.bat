@echo off

rem if you see this, you downloaded this file as utf-8 with a byte order marker!

@echo off

setlocal enableextensions enabledelayedexpansion

set ppversion=5
set ppurl=https://raw.githubusercontent.com/conoror/misc/master/wupatch/pullpatches.bat

set msdownloadpath=https://download.microsoft.com/
set mypathname=%~f0
set mydirname=%~dp0
set wmicpath=%windir%\System32\wbem\wmic.exe
set bitsadmin=%windir%\System32\bitsadmin.exe

if not exist %wmicpath% (
    echo Error: Need wmic.exe to function properly...
    goto :EOF
)

echo Patch checker, Conor O'Rourke 2016, Public domain
echo Patch lists from: http://wu.krelay.de/en/ with
echo massive help from askwoody.com. Grateful thanks to both
echo.
echo This script checks to see if you have the July 2016 rollup
echo and its dependencies (the service stack updates). If you
echo do not, it offers to download them.
echo This will speed up the Windows Update search from days to
echo minutes.
echo If you do have this rollup, the script offers to download
echo the latest Security ONLY quality update.
echo.
echo It is perfectly fine to run this script on a brand new
echo installation of Windows 7 SP1. You should then scan for
echo updates and you will get about 70 results in the search.
echo If you do not want telemetry and other ignorant software
echo from Microsoft, DO NOT select Security Monthly rollups:
echo.
echo     "November, 2016 Security Monthly Quality Rollup..."
echo.    ^^^^^^^^^^^ !!!NO NO NO!!! ^^^^^^^^^^^^^^^^^^^^^^^^
echo.
echo Right click and hide this update! Then install the rest
echo and then rerun this script.
echo .

pause

set winversion=
ver | find "6.1." > NUL && set winversion=Windows6.1
ver | find "6.3." > NUL && set winversion=Windows8.1

if "X%winversion%"=="X" (
    echo .
    echo Error: Windows is not 7 or 8.1
    echo        Sorry, cannot do Vista or Windows 8.0
    pause
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
%wmicpath% COMPUTERSYSTEM GET SystemType | find /i "x64" > NUL && set winarch=x64

echo Windows architecture is: %winarch%
echo .
echo Searching for patches

set patchlist=

for /F "usebackq delims=- tokens=6" %%a in (`findstr /R "^./download/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%"`) do (
    set patchlist=!patchlist! %%a
)

if "X%patchlist%"=="X" (
    echo There are no patches applicable to your operating system
    goto :EOF
)

echo Patches %patchlist% | findstr "KB" > NUL
if errorlevel 1 (
    echo Problem with patchlist, possibly a scripting problem
    goto :EOF
)

echo Complete patchlist: %patchlist%

echo Scanning Windows database for patches...(takes a minute)

set foundlist=

for /f "usebackq" %%a in (`%wmicpath% qfe get hotfixid ^| findstr "%patchlist%"`) do (
    set foundlist=!foundlist! %%a
)

echo Found patches: %foundlist%

set missinglist=

for %%a in (%patchlist%) do (
    echo Patch %foundlist% | find "%%a" > NUL
    if errorlevel 1 set missinglist=!missinglist! %%a
)

set haveslist=
set haverlist=
set haveulist=

if "X%foundlist%" == "X" goto havenothing

for /F "usebackq" %%a in (`findstr /R "^./download/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%" ^| findstr /I "%foundlist%"`) do (
    echo %%a | findstr /R "^U/download/" > NUL
    if NOT errorlevel 1 set haveulist=%%a
    echo %%a | findstr /R "^R/download/" > NUL
    if NOT errorlevel 1 set haverlist=%%a
    echo %%a | findstr /R "^S/download/" > NUL
    if NOT errorlevel 1 set haveslist=%%a
)

if NOT "X%haveulist%"=="X" (
    echo You have the current Security only rollup. You're done!
    goto :EOF
)

if NOT "X%haverlist%"=="X" (
    echo You have the July 2016 rollup. You need the current Security only rollup...
    call :downloadfile U
    goto :EOF
)

if NOT "X%haveslist%"=="X" (
    echo You have one of the servicing stacks. You need the July 2016 rollup...
    call :downloadfile R
    goto :EOF
)

:havenothing

echo You do not have anything so far. Start with the servicing stack...
call :downloadfile S
goto :EOF


:downloadfile

if "X%missinglist%"=="X" (
    echo All patches present! Should not happen!
    goto :EOF
)

for /F "usebackq" %%a in (`findstr /R "^%1/download/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%" ^| findstr /I "%missinglist%"`) do (
    set pullurl=%%a
)

if "X%pullurl%"=="X" (
    echo Something went wrong finding that patch
    goto :EOF
)

set pullurl=%msdownloadpath%%pullurl:~2%

set pullurllocal=

for /F "usebackq" %%a in (`echo %pullurl%`) do (
    set pullurllocal=%%~nxa
)

if "X%pullurllocal%"=="X" (
    echo Something went wrong separating the name!
    goto :EOF
)

echo The download url is: %pullurl%

if not exist %bitsadmin% (
    echo Need bitsadmin.exe to download files so skipping this part
    goto :EOF
)

set /p downloadyn=Would you like me to download those patches for you?

if "X%downloadyn%"=="X" (
    echo Defaulting to No
    goto :EOF
)

echo %downloadyn% | findstr /I "y" > NUL
if errorlevel 1 (
    echo That means no, exiting now
    goto :EOF
)

echo Downloading patches into current directory

%bitsadmin% /transfer "Wantsomepatches" "%pullurl%" "%mydirname%%pullurllocal%"

echo Download complete. You can check the file is genuine in properties
echo (right click file and click properties). Check the file has a digital
echo signatures tab and check the signature by highlighting one and clicking
echo details.
echo Then install the file by double clicking on it...
echo.

goto :EOF


Windows database follows from this point. These are not valid batch commands!

Windows 7 Servicing stack and July rollup

S/download/C/0/8/C0823F43-BFE9-4147-9B0A-35769CBBE6B0/Windows6.1-KB3020369-x86.msu
S/download/5/D/0/5D0821EB-A92D-4CA2-9020-EC41D56B074F/Windows6.1-KB3020369-x64.msu
S/download/D/7/A/D7A954C7-DC1F-4339-99BC-CEFDC09A8661/Windows6.1-KB3177467-x86.msu
S/download/2/4/7/247BDD8A-6AAE-4466-B137-3B2918D0CEAB/Windows6.1-KB3177467-x64.msu

R/download/5/6/0/560504D4-F91A-4DEB-867F-C713F7821374/Windows6.1-KB3172605-x64.msu
R/download/C/D/5/CD5DE7B2-E857-4BD4-AA9C-6B30C3E1735A/Windows6.1-KB3172605-x86.msu

Windows 8.1 Servicing stack and July rollup

S/download/2/B/8/2B832205-A313-45A4-9356-DF5E47B70663/Windows8.1-KB3021910-x86.msu
S/download/6/1/5/615B8D87-A02C-485E-B9B5-D6F4AEB52D78/Windows8.1-KB3021910-x64.msu
S/download/4/5/F/45F8AA2A-1C72-460A-B9E9-83D3966DDA46/Windows8.1-KB3173424-x86.msu
S/download/D/B/4/DB4B93B5-5E6B-4FC4-85A9-0C0FC82DF07F/Windows8.1-KB3173424-x64.msu

R/download/E/5/8/E5864645-6391-4D75-BB2C-7D7F05EF7D13/Windows8.1-KB3172614-x86.msu
R/download/3/0/D/30DB904F-EA28-4CE9-A4C8-1BD660D43607/Windows8.1-KB3172614-x64.msu

Windows Security only Updates (October)

U/download/3/3/B/33B629EC-C0A9-4A33-B2B1-23C6E3FE46F1/Windows6.1-KB3192391-x86.msu
U/download/A/6/8/A6891813-73D9-4326-994A-F2ED2E9776FD/Windows6.1-KB3192391-x64.msu
U/download/B/B/7/BB798446-200E-4F36-8602-F5C7226E5B45/Windows8.1-KB3192392-x86.msu
U/download/4/5/E/45E91587-52DA-4760-AFD1-1BC742026DF0/Windows8.1-KB3192392-x64.msu


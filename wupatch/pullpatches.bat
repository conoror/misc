@echo off

rem if you see this, you downloaded this file as utf-8 with a byte order marker!

@echo off

setlocal enableextensions enabledelayedexpansion

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
echo Patch lists from: http://wu.krelay.de/en/ with grateful thanks
echo.
echo This script just checks a defined list of patches and offers
echo to download them. Nothing else is done. Once you have the
echo list downloaded, stop Windows update (control panel/services)
echo and execute each .msu file by double clicking on it.
echo If you have more than one, don't reboot!
echo.
echo Each .msu file should be signed so you can check that in
echo properties if you do not trust this script to get it right.
echo.
echo Once complete, reboot and rerun this script to check that
echo all patches actually went in....
echo.


set winversion=
ver | find "6.0." > NUL && set winversion=windows6.0
ver | find "6.1." > NUL && set winversion=windows6.1
ver | find "6.2." > NUL && set winversion=windows8.0
ver | find "6.3." > NUL && set winversion=windows8.1

if "X%winversion%"=="X" (
    echo .
    echo Error: Windows is not Vista, 7, 8.0 or 8.1
    pause
    goto :EOF
)

if "%winversion%"=="windows6.0" echo Using Windows Vista
if "%winversion%"=="windows6.1" echo Using Windows 7
if "%winversion%"=="windows8.0" echo Using Windows 8
if "%winversion%"=="windows8.1" echo Using Windows 8.1

set winarch=x86
%wmicpath% COMPUTERSYSTEM GET SystemType | find /i "x64" > NUL && set winarch=x64

echo Windows architecture is: %winarch%


echo Searching for patches

set patchlist=

for /F "usebackq delims=- tokens=6" %%a in (`findstr /I "download/" "%mypathname%" ^| findstr /I "%winversion%" ^| findstr /I "%winarch%"`) do (
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

echo You need all of: %patchlist%


echo Scanning for patches...(takes a minute)

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

if "X%missinglist%"=="X" (
    echo All patches present! You're set - go and check for updates now...
    goto :EOF
)

if NOT "X%missinglist%"=="X" echo Missing patches: %missinglist%

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

for %%a in (%missinglist%) do call :downloadfile %%a

echo Complete. Stop Windows Update service now and run each of the downloads...

goto :EOF


:downloadfile


echo Downloading %1 into %mydirname%

for /F "usebackq" %%a in (`findstr /I "%winversion%" "%mypathname%" ^| findstr /I "%winarch%" ^| findstr "%1"`) do (
    set pullurl=%%a
)

if "X%pullurl%"=="X" (
    echo Something went wrong finding that patch
    goto :EOF
)

set pullurl=%msdownloadpath%%pullurl%

%bitsadmin% /transfer "Wantsomepatches" "%pullurl%" "%mydirname%%winversion%-%1-%winarch%.msu"

goto :EOF



Windows database follows from this point. These are not valid batch commands!

download/B/7/C/B7CD3A70-1EA7-486A-9585-F6814663F1A9/Windows6.1-KB3138612-x64.msu
download/A/4/8/A48BBC7A-8045-4ED7-A43F-FB5C9B686183/Windows6.1-KB3109094-x64.msu
download/3/3/F/33FFD46F-ED25-49FE-B89F-EC6269568F9B/Windows6.1-KB3164033-x64.msu
download/0/4/A/04A4348A-EB14-461C-B5B1-25EEBB1A24B8/Windows6.1-KB3078601-x64.msu
download/8/D/7/8D75A16B-5BC0-457A-BE97-A93566AB82D6/Windows6.1-KB3145739-x64.msu
download/B/E/D/BEDBE8C8-8EF2-4D8C-B55C-7D1554CA9500/Windows6.1-KB3168965-x64.msu
download/4/C/2/4C24EA5E-F116-4E63-A02D-5231A3A1C56A/Windows6.1-KB3177725-x64.msu


download/E/4/7/E47FB37E-7443-4047-91F7-16DDDCF2955C/Windows6.1-KB3138612-x86.msu
download/A/2/7/A2783780-2A3B-4A35-A55E-71B0EAF79E0E/Windows6.1-KB3109094-x86.msu
download/7/F/B/7FB02E92-9FDC-4E1D-9CC7-B8104880E4A3/Windows6.1-KB3164033-x86.msu
download/0/7/5/075F0E65-7F09-4554-B8FA-4F52D6E22172/Windows6.1-KB3078601-x86.msu
download/C/E/9/CE982A9D-C4C4-4355-B87A-1A72CCD0CC73/Windows6.1-KB3145739-x86.msu
download/4/2/0/420492DE-F382-4F18-839E-D4ADD8BF33E7/Windows6.1-KB3168965-x86.msu
download/6/D/F/6DF4A2B2-AE88-455A-8032-6B7283BF4052/Windows6.1-KB3177725-x86.msu


download/B/9/6/B968899A-D54A-459B-801C-C1D47D48D0A2/Windows6.0-KB3109094-x64.msu
download/2/F/A/2FA544A7-08F8-4044-A8FD-23385FA203DC/Windows6.0-KB3164033-x64.msu
download/E/1/A/E1A67C3D-3538-4DDA-95E9-18A97D3F32D1/Windows6.0-KB3078601-x64.msu
download/D/B/2/DB21281A-994D-478E-A8FC-447A9C2B6620/Windows6.0-KB3145739-x64.msu
download/5/4/0/54068A06-163A-4DE8-B4D6-C2818F614E22/Windows6.0-KB3168965-x64.msu
download/1/8/3/183A5301-2B28-42A9-9F41-D81F7A7C877A/Windows6.0-KB3177725-x64.msu


download/1/C/7/1C7E0909-12A9-4C63-9F97-8958670BED55/Windows6.0-KB3109094-x86.msu
download/6/5/4/6549D38D-EFD6-41B4-8084-614035F7F923/Windows6.0-KB3164033-x86.msu
download/5/7/F/57F9EE3A-31FB-401D-BB02-B26558BFAF55/Windows6.0-KB3078601-x86.msu
download/2/0/A/20A89FFB-83FB-409C-9A25-7CD2B765F9D6/Windows6.0-KB3145739-x86.msu
download/D/0/9/D09B0719-8905-4B0C-BED0-317AAFD970BE/Windows6.0-KB3168965-x86.msu
download/2/9/2/2926E12F-4B0F-4C17-9044-869A6908C82E/Windows6.0-KB3177725-x86.msu


download/8/8/A/88AFE5D4-0021-4384-9D64-5411257CCC5B/Windows8.1-KB3138615-x64.msu
download/9/6/4/964EE585-03DC-441A-AA99-6A39BA731869/Windows8.1-KB3138615-x86.msu

------------September Patches for Windows Vista and 7----------------

download/E/1/6/E16C6588-378C-4E32-AE8E-00E0EEAA390F/Windows6.0-KB3185911-x86.msu
download/8/E/5/8E5248EC-C0D6-4837-897B-C2CE426D6C88/Windows6.0-KB3185911-x64.msu

download/6/E/8/6E884981-1D7B-4423-A845-56B805E59036/Windows6.1-KB3185911-x86.msu
download/2/B/C/2BC5C880-2920-47F1-8A64-4B1F1C1A8737/Windows6.1-KB3185911-x64.msu


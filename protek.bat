@echo off

:: reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" | findstr /r ^VS[0-9][0-9][0-9]COMNTOOLS
:: put this:
:: @SET VS_BUILD_TOOLS_ENVIRONMENT_CONFIGURED="BRAVO"
:: at the end of batch file of environment script

if not defined AXPROTECTOR_SDK ( 
	goto :nosdk 
) 
 
set CURRENT_PATH=%cd%

:protek
echo encryption of executable
set AXPROTECTOR=%AXPROTECTOR_SDK%\bin\AxProtector.exe

"%AXPROTECTOR%" protek.wbc

:nosdk
echo "error"
exit /b -1

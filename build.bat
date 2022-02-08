@echo off

:: reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" | findstr /r ^VS[0-9][0-9][0-9]COMNTOOLS
:: put this:
:: @SET VS_BUILD_TOOLS_ENVIRONMENT_CONFIGURED="BRAVO"
:: at the end of batch file of environment script

if defined VS_BUILD_TOOLS_ENVIRONMENT_CONFIGURED ( 
	goto :clean 
) 
 
:configure
echo configuring environment.
pushd %VS140COMNTOOLS%
call VsMSBuildCmd.bat
call VsDevCmd.bat
@SET VS_BUILD_TOOLS_ENVIRONMENT_CONFIGURED="BRAVO"

popd
set defines=-D_USING_V110_SDK71_ -DSUBSYSTEM_CONSOLE -DDEBUG_OUTPUT
set link_options=/link /FILEALIGN:512 /OPT:REF /OPT:ICF /INCREMENTAL:NO /subsystem:console,5.01
set libs=gdiplus.lib user32.lib Gdi32.lib ws2_32.lib Wininet.lib

:clean
rmdir /s /q bin

:build
if not exist bin ( mkdir bin )
echo starting build...
:: echo "building proc-meminfo.exe..."
:: cl /Ox src/get-memoryinformation.cpp %defines% %link_options% %libs% /out:bin\proc-meminfo.exe >nul
:: echo "building ex.exe..."
:: cl /Ox src/explorer.cpp %defines% %link_options% %libs% /out:bin\ex.exe >nul
echo "building screenshot.exe..."
cl /Ox src/screenshot.cpp %defines% %link_options% %libs% /out:bin\screenshot.exe >nul
:: echo "building adminshell.exe..."
:: cl /Ox src/adminshell.cpp %defines% %link_options% %libs% /out:bin\adminshell.exe
:: echo "building mem-enumheap.exe..."
:: cl /Ox src/mem-enumheap.cpp %defines% %link_options% %libs% /out:bin\mem-enumheap.exe >nul
::echo "building sudo.exe..."
::cl /Ox src/sudo.cpp %defines% %link_options% %libs% /out:bin\sudo.exe

copy bin\*.exe C:\MyApps\System-Tools
echo "Exporting apps to C:\MyApps\SystemTools"
del /F /Q *.obj

@echo off

REM - CALL "$(SolutionDir)scripts\postbuild-win.bat" "$(TargetExt)" "$(BINARY_NAME)" "$(Platform)" "$(COPY_VST2)" "$(TargetPath)" "$(VST2_32_PATH)" "$(VST2_64_PATH)" "$(VST3_32_PATH)" "$(VST3_64_PATH)" "$(AAX_32_PATH)" "$(AAX_64_PATH)" "$(BUILD_DIR)" "$(VST_ICON)" "$(AAX_ICON)" "
REM $(CREATE_BUNDLE_SCRIPT)"

REM set FORMAT=%1
REM set NAME=%2
REM set PLATFORM=%3
REM set COPY_VST2=%4
REM set BUILT_BINARY=%5
REM set VST2_32_PATH=%6
REM set VST2_64_PATH=%7 
REM set VST3_32_PATH=%8
REM set VST3_64_PATH=%9
REM shift
REM shift 
REM shift
REM shift
REM shift 
REM shift
REM set AAX_32_PATH=%4
REM set AAX_64_PATH=%5
REM set BUILD_DIR=%6
REM set VST_ICON=%7
REM set AAX_ICON=%8
REM set CREATE_BUNDLE_SCRIPT=%9
REM 
REM echo POSTBUILD SCRIPT VARIABLES -----------------------------------------------------
REM echo FORMAT %FORMAT% 
REM echo NAME %NAME% 
REM echo PLATFORM %PLATFORM% 
REM echo COPY_VST2 %COPY_VST2% 
REM echo BUILT_BINARY %BUILT_BINARY% 
REM echo VST2_32_PATH %VST2_32_PATH% 
REM echo VST2_64_PATH %VST2_64_PATH% 
REM echo VST3_32_PATH %VST3_32_PATH% 
REM echo VST3_64_PATH %VST3_64_PATH% 
REM echo BUILD_DIR %BUILD_DIR%
REM echo VST_ICON %VST_ICON% 
REM echo AAX_ICON %AAX_ICON% 
REM echo CREATE_BUNDLE_SCRIPT %CREATE_BUNDLE_SCRIPT%
REM echo END POSTBUILD SCRIPT VARIABLES -----------------------------------------------------
REM 
REM if %PLATFORM% == "Win32" (
REM   if %FORMAT% == ".exe" (
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.exe
REM   )
REM 
REM   if %FORMAT% == ".dll" (
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.dll
REM   )
REM   
REM   if %FORMAT% == ".dll" (
REM     if %COPY_VST2% == "1" (
REM       echo copying 32bit binary to 32bit VST2 Plugins folder ... 
REM       copy /y %BUILT_BINARY% %VST2_32_PATH%
REM     ) else (
REM       echo not copying 32bit VST2 binary
REM     )
REM   )
REM   
REM   if %FORMAT% == ".vst3" (
REM     echo copying 32bit binary to VST3 BUNDLE ..
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.vst3 %VST_ICON% %FORMAT%
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.vst3\Contents\x86-win
REM     if exist %VST3_32_PATH% ( 
REM       echo copying VST3 bundle to 32bit VST3 Plugins folder ...
REM       call %CREATE_BUNDLE_SCRIPT% %VST3_32_PATH%\%NAME%.vst3 %VST_ICON% %FORMAT%
REM       xcopy /E /H /Y %BUILD_DIR%\%NAME%.vst3\Contents\*  %VST3_32_PATH%\%NAME%.vst3\Contents\
REM     )
REM   )
REM   
REM   if %FORMAT% == ".aaxplugin" (
REM     echo copying 32bit binary to AAX BUNDLE ..
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.aaxplugin\Contents\Win32
REM     echo copying 32bit bundle to 32bit AAX Plugins folder ... 
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
REM     xcopy /E /H /Y %BUILD_DIR%\%NAME%.aaxplugin\Contents\* %AAX_32_PATH%\%NAME%.aaxplugin\Contents\
REM   )
REM )
REM 
REM if %PLATFORM% == "x64" (
REM   if not exist "%ProgramFiles(x86)%" (
REM     echo "This batch script fails on 32 bit windows... edit accordingly"
REM   )
REM 
REM   if %FORMAT% == ".exe" (
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.exe
REM   )
REM 
REM   if %FORMAT% == ".dll" (
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.dll
REM   )
REM   
REM   if %FORMAT% == ".dll" (
REM     if %COPY_VST2% == "1" (
REM       echo copying 64bit binary to 64bit VST2 Plugins folder ... 
REM       copy /y %BUILT_BINARY% %VST2_64_PATH%
REM     ) else (
REM       echo not copying 64bit VST2 binary
REM     )
REM   )
REM   
REM   if %FORMAT% == ".vst3" (
REM     echo copying 64bit binary to VST3 BUNDLE ...
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.vst3 %VST_ICON% %FORMAT%
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.vst3\Contents\x86_64-win
REM     if exist %VST3_64_PATH% (
REM       echo copying VST3 bundle to 64bit VST3 Plugins folder ...
REM       call %CREATE_BUNDLE_SCRIPT% %VST3_64_PATH%\%NAME%.vst3 %VST_ICON% %FORMAT%
REM       xcopy /E /H /Y %BUILD_DIR%\%NAME%.vst3\Contents\*  %VST3_64_PATH%\%NAME%.vst3\Contents\
REM     )
REM   )
REM   
REM   if %FORMAT% == ".aaxplugin" (
REM     echo copying 64bit binary to AAX BUNDLE ...
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
REM     copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.aaxplugin\Contents\x64
REM     echo copying 64bit bundle to 64bit AAX Plugins folder ... 
REM     call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
REM     xcopy /E /H /Y %BUILD_DIR%\%NAME%.aaxplugin\Contents\* %AAX_64_PATH%\%NAME%.aaxplugin\Contents\
REM   )
REM )
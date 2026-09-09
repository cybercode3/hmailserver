@echo off
setlocal

set HMS_LIBS=%~1
set OUT_DIR=%~2
set TARGET=%~3
set SCRIPT_DIR=%~dp0

NET STOP hMailServer

xcopy /F /Y "%HMS_LIBS%\openssl-3.5.8\out64\bin\libcrypto-3-x64.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%HMS_LIBS%\openssl-3.5.8\out64\bin\libssl-3-x64.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%HMS_LIBS%\postgresql-15.19\Release\libpq\*.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%HMS_LIBS%\libmariadb-3.4.9\build64\libmariadb\RelWithDebInfo\libmariadb.dll" "%OUT_DIR%"
if errorlevel 1 exit /b 1

REM The backup/restore feature launches this; libraries\build-7zip.ps1 fetches it.
xcopy /F /Y "%HMS_LIBS%\7zip-26.03\7za.exe" "%OUT_DIR%"
if errorlevel 1 exit /b 1

xcopy /F /Y "%SCRIPT_DIR%..\..\..\installation\Extras\public_suffix_list.dat" "%OUT_DIR%"
if errorlevel 1 exit /b 1

"%TARGET%" /Register
if errorlevel 1 exit /b 1

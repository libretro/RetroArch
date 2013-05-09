@echo off

for /f "tokens=* delims=" %%f in ('type rarch_main.xui') do CALL :DOREPLACE "%%f"

GOTO :EOF
:DOREPLACE
SET INPUT=%*
SET OUTPUT=%INPUT:RetroArchMain=RetroArchControls%

for /f "tokens=* delims=" %%g in ('ECHO %OUTPUT%') do ECHO %%~g>>rarch_controls.xui

EXIT /b

:EOF

@echo off

for /f "tokens=* delims=" %%f in ('type rarch_main.xui') do CALL :DOREPLACE "%%f"

GOTO :EOF
:DOREPLACE
SET INPUT=%*
SET OUTPUT=%INPUT:RetroArchMain=RetroArchSettings%

for /f "tokens=* delims=" %%g in ('ECHO %OUTPUT%') do ECHO %%~g>>rarch_settings.xui

EXIT /b

:EOF

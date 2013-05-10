@echo off

for /f "tokens=* delims=" %%f in ('type rarch_main.xui') do CALL :DOREPLACE "%%f"

GOTO :EOF
:DOREPLACE
SET INPUT=%*
SET OUTPUT=%INPUT:RetroArchMain=RetroArchAudioOptions%

for /f "tokens=* delims=" %%g in ('ECHO %OUTPUT%') do ECHO %%~g>>rarch_audio_options.xui

EXIT /b

:EOF

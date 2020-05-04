@echo off
cd source
for /f %%f in ('dir *.c *.h /b/s') do (
	echo.%%f | findstr /C:"\\shaders\\">nul || (clang-format -i %%f)
)
cd ..

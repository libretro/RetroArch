# Microsoft Developer Studio Project File - Name="RetroArch" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=RetroArch - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "RetroArch.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "RetroArch.mak" CFG="RetroArch - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "RetroArch - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "RetroArch - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "RetroArch - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /w /W0 /GX /O2 /I "../../../../libretro-common/include" /I "../../../../libretro-common/include/compat/msvc" /I "../../../../libretro-common/include/compat/zlib" /I "../../../../deps" /I "../../../../deps/stb" /I "$(ProgramFiles)/Microsoft Platform SDK/include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D _WIN32_WINNT=0x0351 /D "RARCH_INTERNAL" /D "HAVE_CC_RESAMPLER" /D "HAVE_GRIFFIN" /D "HAVE_ZLIB" /D "HAVE_RPNG" /D "HAVE_RJPEG" /D "HAVE_RBMP" /D "HAVE_RTGA" /D "HAVE_IMAGEVIEWER" /D "HAVE_XMB" /D "HAVE_DYLIB" /D "HAVE_NETWORK_CMD" /D "HAVE_COMMAND" /D "HAVE_STDIN_CMD" /D "HAVE_THREADS" /D "HAVE_DYNAMIC" /D "HAVE_OVERLAY" /D "HAVE_RGUI" /D "HAVE_MENU" /D "HAVE_7ZIP" /D "HAVE_MATERIALUI" /D "HAVE_LIBRETRODB" /D "HAVE_ONLINE_UPDATER" /D "HAVE_UPDATE_CORES" /D "HAVE_UPDATE_ASSETS" /D "HAVE_STB_FONT" /D "__STDC_CONSTANT_MACROS" /YX /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /subsystem:console /verbose /machine:I386
# SUBTRACT LINK32 /nologo

!ELSEIF  "$(CFG)" == "RetroArch - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /w /W0 /Gm /GX /ZI /Od /I "../../../../libretro-common/include" /I "../../../../libretro-common/include/compat/msvc" /I "../../../../libretro-common/include/compat/zlib" /I "../../../../deps" /I "../../../../deps/stb" /I "$(ProgramFiles)/Microsoft Platform SDK/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D _WIN32_WINNT=0x0351 /D "RARCH_INTERNAL" /D "HAVE_CC_RESAMPLER" /D "HAVE_GRIFFIN" /D "HAVE_ZLIB" /D "HAVE_RPNG" /D "HAVE_RJPEG" /D "HAVE_RBMP" /D "HAVE_RTGA" /D "HAVE_IMAGEVIEWER" /D "HAVE_XMB" /D "HAVE_DYLIB" /D "HAVE_NETWORK_CMD" /D "HAVE_COMMAND" /D "HAVE_STDIN_CMD" /D "HAVE_THREADS" /D "HAVE_DYNAMIC" /D "HAVE_OVERLAY" /D "HAVE_RGUI" /D "HAVE_MENU" /D "HAVE_7ZIP" /D "HAVE_MATERIALUI" /D "HAVE_GDI" /D "HAVE_LIBRETRODB" /D "HAVE_ONLINE_UPDATER" /D "HAVE_UPDATE_CORES" /D "HAVE_UPDATE_ASSETS" /D "HAVE_STB_FONT" /D "__STDC_CONSTANT_MACROS" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "RetroArch - Win32 Release"
# Name "RetroArch - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\griffin\griffin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\griffin\griffin_cpp.cpp

!IF  "$(CFG)" == "RetroArch - Win32 Release"

# ADD CPP /D WINVER=0x0400 /D _WIN32_WINNT=0x0400

!ELSEIF  "$(CFG)" == "RetroArch - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project

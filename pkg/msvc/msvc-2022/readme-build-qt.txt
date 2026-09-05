If you are using the MSVC 2017 project file, you must define the environment varaible QtDirectory.  This should point to the directory with the version number.
For example: QtDirectory=C:\qt\5.10.1

Two ways to define the variable:

Windows:
  Control Panel > System
  Click Advanced System Settings
  Go to Advanced Tab
  Click Environment Variables button
  Create a new variable (either user or system is okay), name it QtDirectory, set the value to your QT directory.
  Restart all instances of Visual Studio.  You may have to close it, wait 15 seconds, then kill the process.
Visual Studio:
  View > Other Windows > Property Manager
  Open the x64 build configuration
  Double click on Microsoft.Cpp.x64.user
  Go to User Macros
  Add a new macro, name it QtDirectory, and set the value to your QT directory.
  Open the Win32 build configuration
  Double click on Microsoft.Cpp.Win32.user
  Go to User Macros
  Add a new macro, name it QtDirectory, and set the value to your QT directory.

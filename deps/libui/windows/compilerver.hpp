// 9 june 2015

// Visual Studio (Microsoft's compilers)
// VS2013 is needed for va_copy().
#ifdef _MSC_VER
#if _MSC_VER < 1800
#error Visual Studio 2013 or higher is required to build libui.
#endif
#endif

// LONGTERM MinGW

// other compilers can be added here as necessary

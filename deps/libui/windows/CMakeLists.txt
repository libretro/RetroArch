# 3 june 2016

list(APPEND _LIBUI_SOURCES
	windows/alloc.cpp
	windows/area.cpp
	windows/areadraw.cpp
	windows/areaevents.cpp
	windows/areascroll.cpp
	windows/areautil.cpp
	windows/box.cpp
	windows/button.cpp
	windows/checkbox.cpp
	windows/colorbutton.cpp
	windows/colordialog.cpp
	windows/combobox.cpp
	windows/container.cpp
	windows/control.cpp
	windows/d2dscratch.cpp
	windows/datetimepicker.cpp
	windows/debug.cpp
	windows/draw.cpp
	windows/drawmatrix.cpp
	windows/drawpath.cpp
	windows/drawtext.cpp
	windows/dwrite.cpp
	windows/editablecombo.cpp
	windows/entry.cpp
	windows/events.cpp
	windows/fontbutton.cpp
	windows/fontdialog.cpp
	windows/form.cpp
	windows/graphemes.cpp
	windows/grid.cpp
	windows/group.cpp
	windows/init.cpp
	windows/label.cpp
	windows/main.cpp
	windows/menu.cpp
	windows/multilineentry.cpp
	windows/parent.cpp
	windows/progressbar.cpp
	windows/radiobuttons.cpp
	windows/separator.cpp
	windows/sizing.cpp
	windows/slider.cpp
	windows/spinbox.cpp
	windows/stddialogs.cpp
	windows/tab.cpp
	windows/tabpage.cpp
	windows/text.cpp
	windows/utf16.cpp
	windows/utilwin.cpp
	windows/window.cpp
	windows/winpublic.cpp
	windows/winutil.cpp
	windows/resources.rc
)
set(_LIBUI_SOURCES ${_LIBUI_SOURCES} PARENT_SCOPE)

list(APPEND _LIBUI_INCLUDEDIRS
	windows
)
set(_LIBUI_INCLUDEDIRS _LIBUI_INCLUDEDIRS PARENT_SCOPE)

# Windows won't link resources in static libraries; we need to provide the libui.res file in this case.
set(_LIBUINAME libui PARENT_SCOPE)
if(NOT BUILD_SHARED_LIBS)
	set(_LIBUI_STATIC_RES ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libui.res PARENT_SCOPE)
endif()
macro(_handle_static)
	# TODO this full path feels hacky
	add_custom_command(
		TARGET libui POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:libui,BINARY_DIR>/CMakeFiles/libui.dir/windows/resources.rc.* ${_LIBUI_STATIC_RES}
		COMMENT "Copying libui.res")
endmacro()

# notice that usp10 comes before gdi32
# TODO prune this list
set(_LIBUI_LIBS
	user32 kernel32 usp10 gdi32 comctl32 uxtheme msimg32 comdlg32 d2d1 dwrite ole32 oleaut32 oleacc uuid
PARENT_SCOPE)

if(NOT MSVC)
	if(BUILD_SHARED_LIBS)
		message(FATAL_ERROR
			"Sorry, but libui for Windows can currently only be built as a static library with MinGW. You will need to either build as a static library or switch to MSVC."
		)
	endif()
endif()

# 3 june 2016

list(APPEND _LIBUI_SOURCES
	darwin/alloc.m
	darwin/area.m
	darwin/areaevents.m
	darwin/autolayout.m
	darwin/box.m
	darwin/button.m
	darwin/checkbox.m
	darwin/colorbutton.m
	darwin/combobox.m
	darwin/control.m
	darwin/datetimepicker.m
	darwin/debug.m
	darwin/draw.m
	darwin/drawtext.m
	darwin/editablecombo.m
	darwin/entry.m
	darwin/fontbutton.m
	darwin/form.m
	darwin/grid.m
	darwin/group.m
	darwin/image.m
	darwin/label.m
	darwin/main.m
	darwin/map.m
	darwin/menu.m
	darwin/multilineentry.m
	darwin/progressbar.m
	darwin/radiobuttons.m
	darwin/scrollview.m
	darwin/separator.m
	darwin/slider.m
	darwin/spinbox.m
	darwin/stddialogs.m
	darwin/tab.m
	darwin/text.m
	darwin/util.m
	darwin/window.m
	darwin/winmoveresize.m
)
set(_LIBUI_SOURCES ${_LIBUI_SOURCES} PARENT_SCOPE)

list(APPEND _LIBUI_INCLUDEDIRS
	darwin
)
set(_LIBUI_INCLUDEDIRS _LIBUI_INCLUDEDIRS PARENT_SCOPE)

set(_LIBUINAME libui PARENT_SCOPE)
if(NOT BUILD_SHARED_LIBS)
	set(_LIBUINAME libui-temporary PARENT_SCOPE)
endif()
# thanks to Mr-Hide in irc.freenode.net/#cmake
macro(_handle_static)
	set_target_properties(${_LIBUINAME} PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
	set(_aname $<TARGET_FILE:${_LIBUINAME}>)
	set(_lname libui-combined.list)
	set(_oname libui-combined.o)
	add_custom_command(
		OUTPUT ${_oname}
		COMMAND
			nm -m ${_aname} | sed -E -n "'s/^[0-9a-f]* \\([A-Z_]+,[a-z_]+\\) external //p'" > ${_lname}
		COMMAND
			ld -exported_symbols_list ${_lname} -r -all_load ${_aname} -o ${_oname}
		COMMENT "Removing hidden symbols")
	add_library(libui STATIC ${_oname})
	# otherwise cmake won't know which linker to use
	set_target_properties(libui PROPERTIES
		LINKER_LANGUAGE C)
	set(_aname)
	set(_lname)
	set(_oname)
endmacro()

set(_LIBUI_LIBS
	objc "-framework Foundation" "-framework AppKit"
PARENT_SCOPE)

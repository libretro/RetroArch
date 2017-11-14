# 3 june 2016

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)

list(APPEND _LIBUI_SOURCES
	unix/alloc.c
	unix/area.c
	unix/box.c
	unix/button.c
	unix/cellrendererbutton.c
	unix/checkbox.c
	unix/child.c
	unix/colorbutton.c
	unix/combobox.c
	unix/control.c
	unix/datetimepicker.c
	unix/debug.c
	unix/draw.c
	unix/drawmatrix.c
	unix/drawpath.c
	unix/drawtext.c
	unix/editablecombo.c
	unix/entry.c
	unix/fontbutton.c
	unix/form.c
	unix/future.c
	unix/graphemes.c
	unix/grid.c
	unix/group.c
	unix/image.c
	unix/label.c
	unix/main.c
	unix/menu.c
	unix/multilineentry.c
	unix/progressbar.c
	unix/radiobuttons.c
	unix/separator.c
	unix/slider.c
	unix/spinbox.c
	unix/stddialogs.c
	unix/tab.c
	unix/text.c
	unix/util.c
	unix/window.c
)
set(_LIBUI_SOURCES ${_LIBUI_SOURCES} PARENT_SCOPE)

list(APPEND _LIBUI_INCLUDEDIRS
	unix
)
set(_LIBUI_INCLUDEDIRS _LIBUI_INCLUDEDIRS PARENT_SCOPE)

set(_LIBUINAME libui PARENT_SCOPE)
if(NOT BUILD_SHARED_LIBS)
	set(_LIBUINAME libui-temporary PARENT_SCOPE)
endif()
macro(_handle_static)
	set_target_properties(${_LIBUINAME} PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
	set(_aname $<TARGET_FILE:${_LIBUINAME}>)
	set(_oname libui-combined.o)
	add_custom_command(
		OUTPUT ${_oname}
		COMMAND
			ld -r --whole-archive ${_aname} -o ${_oname}
		COMMAND
			objcopy --localize-hidden ${_oname}
		COMMENT "Removing hidden symbols")
	add_library(libui STATIC ${_oname})
	# otherwise cmake won't know which linker to use
	set_target_properties(libui PROPERTIES
		LINKER_LANGUAGE C)
	set(_aname)
	set(_oname)
endmacro()

# TODO the other variables don't work?
set(_LIBUI_CFLAGS
	${GTK_CFLAGS}
PARENT_SCOPE)

set(_LIBUI_LIBS
	${GTK_LDFLAGS} m ${CMAKE_DL_LIBS}
PARENT_SCOPE)

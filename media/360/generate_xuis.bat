@echo off
cd sd

@echo "Creating sd/rarch_video_options.xui ..."
call :subroutine_a
@echo "Creating sd/rarch_settings.xui ..."
call :subroutine_b
@echo "Creating sd/rarch_shader_browser.xui ..."
call :subroutine_c
@echo "Creating sd/rarch_filebrowser.xui ..."
call :subroutine_d
@echo "Creating sd/rarch_controls.xui ..."
call :subroutine_e
@echo "Creating sd/rarch_libretrocore_browser.xui ..."
call :subroutine_f
@echo "Creating sd/rarch_audio_options.xui ..."
call :subroutine_g
@echo "Creating sd/rarch_core_options.xui ..."
call :subroutine_h
@echo "Creating sd/rarch_load_game_history.xui ..."
call :subroutine_i

cd ../
cd hd

@echo "Creating hd/rarch_video_options.xui ..."
call :subroutine_a
@echo "Creating hd/rarch_settings.xui ..."
call :subroutine_b
@echo "Creating hd/rarch_shader_browser.xui ..."
call :subroutine_c
@echo "Creating hd/rarch_filebrowser.xui ..."
call :subroutine_d
@echo "Creating hd/rarch_controls.xui ..."
call :subroutine_e
@echo "Creating hd/rarch_libretrocore_browser.xui ..."
call :subroutine_f
@echo "Creating hd/rarch_audio_options.xui ..."
call :subroutine_g
@echo "Creating hd/rarch_core_options.xui ..."
call :subroutine_h
@echo "Creating hd/rarch_load_game_history.xui ..."
call :subroutine_i
goto :eof

:subroutine_a
del rarch_video_options.xui 2>NUL
call rarch_video_options.bat
goto :eof

:subroutine_b
del rarch_settings.xui 2>NUL
call rarch_settings.bat
goto :eof

:subroutine_c
del rarch_shader_browser.xui 2>NUL
call rarch_shader_browser.bat
goto :eof

:subroutine_d
del rarch_filebrowser.xui 2>NUL
call rarch_filebrowser.bat
goto :eof

:subroutine_e
del rarch_controls.xui 2>NUL
call rarch_controls.bat
goto :eof

:subroutine_f
del rarch_libretrocore_browser.xui 2>NUL
call rarch_libretrocore_browser.bat
goto :eof

:subroutine_g
del rarch_audio_options.xui 2>NUL
call rarch_audio_options.bat
goto :eof

:subroutine_h
del rarch_core_options.xui 2>NUL
call rarch_core_options.bat
goto :eof

:subroutine_i
del rarch_load_game_history.xui 2>NUL
call rarch_load_game_history.bat
goto :eof

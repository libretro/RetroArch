/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>
#include "../../configuration.h"
#include "../menu_hash.h"

const char *menu_hash_to_str_es(uint32_t hash)
{

   switch (hash)
   {
      case MENU_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "Buscar contenido";
      case MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "Solucionar problemas de vídeo/sonido";
      case MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "Cambiar el mando virtual sobreimpuesto";
      case MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "¿Qué es un núcleo?";
      case MENU_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "Cargando contenidos";
      case MENU_LABEL_VALUE_HELP_LIST:
         return "Ayuda";
      case MENU_LABEL_VALUE_HELP_CONTROLS:
         return "Controles básicos del menú";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS:
         return "Controles básicos del menú";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "Desplazar hacia arriba";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "Desplazar hacia abajo";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "Confirmar/Aceptar";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "Retroceder";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "Valores predeterminados";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "Información";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "Alternar menú";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "Abandonar";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "Alternar teclado";
      case MENU_LABEL_VALUE_OPEN_ARCHIVE:
         return "Abrir archivo como una carpeta";
      case MENU_LABEL_VALUE_LOAD_ARCHIVE:
         return "Cargar archivo con un núcleo";
      case MENU_LABEL_VALUE_INPUT_BACK_AS_MENU_TOGGLE_ENABLE:
         return "Permitir alternar Back como menú";
      case MENU_LABEL_VALUE_INPUT_MENU_TOGGLE_GAMEPAD_COMBO:
         return "Combo para alternar mando con menú";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "Ocultar lo superpuesto del menú";
      case MENU_VALUE_LANG_POLISH:
         return "Polaco";
      case MENU_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Cargar superposición preferida automáticamente";
      case MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Actualizar archivos de información de núcleos";
      case MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Descargar contenido";
      case MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "(Escanear esta carpeta)";
      case MENU_LABEL_VALUE_SCAN_FILE:
         return "Escanear archivo";
      case MENU_LABEL_VALUE_SCAN_DIRECTORY:
         return "Escanear carpeta";
      case MENU_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Añadir contenido";
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Información";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Usar reproductor de medios integrado";
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menú rápido";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Cargar contenido";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Preguntar";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacidad";
      case MENU_VALUE_HORIZONTAL_MENU: /* Don't change. Breaks everything. (Would be: "Menú horizontal") */
         return "Horizontal Menu";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "No se ha encontrado una configuración.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "No hay contadores de rendimiento.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Controlador";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configuración";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Núcleo";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Vídeo";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Registros";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Guardado";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Rebobinado";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_1:
         return "Asignar esquina superior izquierda";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_2:
         return "Asignar esquina inferior derecha";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Truco";
      case MENU_VALUE_USER:
         return "Usuario";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Activar música del sistema";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Bloquear fotogramas";
      case MENU_LABEL_VALUE_INPUT_BIND_MODE:
         return "Modo de asignación";
      case MENU_LABEL_VALUE_AUTOCONFIG_DESCRIPTOR_LABEL_SHOW:
         return "Mostrar etiquetas de descripción del autoconfigurado";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Mostrar etiquetas de descripción de la entrada del núcleo";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Ocultar descripciones sin asignar de la entrada del núcleo";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostrar mensajes en pantalla";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Fuente de mensajes en pantalla";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Tamaño de mensajes en pantalla";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posición X de mensajes en pantalla";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posición Y de mensajes en pantalla";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Activar filtros por software";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro de parpadeo";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "(Carpeta de contenido)";
      case MENU_VALUE_UNKNOWN:
         return "Desconocido";
      case MENU_VALUE_DONT_CARE:
         return "No importa";
      case MENU_VALUE_LINEAR:
         return "Lineal";
      case MENU_VALUE_NEAREST:
         return "Más cercano";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "(Predeterminada)";
      case MENU_VALUE_DIRECTORY_NONE:
         return "(Ninguna)";
      case MENU_VALUE_NOT_AVAILABLE:
         return "No disponible";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Carpeta de reasignación de entrada";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Carpeta de autoconfiguración de dispositivo de entrada";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Carpeta de configuración de grabación";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Carpeta de salida de grabación";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Carpeta de capturas de pantalla";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Carpeta de listas de reproducción";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Carpeta de partidas guardadas";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Carpeta de guardados rápidos";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandos stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Controlador de vídeo";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Activar grabación";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Activar grabación de GPU";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "Carpeta de grabación";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Usar carpeta de salida";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configuración de grabación";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Activar grabación con filtros";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Carpeta de descargas";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Carpeta de recursos";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Carpeta de fondos de pantalla dinámicos";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Carpeta de carátulas";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Carpeta del navegador de archivos";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Carpeta de configuración";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Carpeta de información del núcleo";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Carpeta de núcleos";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Carpeta de cursores";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Carpeta de bases de datos de contenidos";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Carpeta de sistema/BIOS";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Carpeta de archivos de trucos";
      case MENU_LABEL_VALUE_EXTRACTION_DIRECTORY:
         return "Carpeta de extracción";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Carpeta de filtros de sonido";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Carpeta de shaders de vídeo";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Carpeta de filtros de vídeo";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Carpeta de superimposiciones";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Carpeta de teclados superimpuestos";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Intercambiar entrada en red";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Permitir espectadores en red";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Dirección IP";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Puerto TCP/UDP para juego en red";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Activar juego en red";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Retraso de fotogramas en red";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Activar cliente en red";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostrar pantalla de inicio";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Color de títulos del menú";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Color de entrada resaltada del menú";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostrar fecha y hora";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Buclar datos hilados";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Color de entrada normal del menú";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostrar ajustes avanzados";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Soporte para teclado";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Soporte táctil";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Mostrar nombre del núcleo";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Activar anulación de PPP";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Anular PPP";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspender salvapantallas";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Desactivar composición de escritorio";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Detenerse en segundo plano";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Ejecutar al inicio la IU ayudante";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra de menús";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Acción para asociar tipos de archivo";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandos de red";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Puerto de comandos de red";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Activar historial";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Tamaño del historial";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Tasa de fotogramas estimada del monitor";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Anular al cerrar núcleo";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "No ejecutar automáticamente un núcleo";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limitar velocidad de ejecución máxima";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocidad de ejecución máxima";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Cargar automáticamente archivos de reasignación";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Cantidad de velocidad reducida";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuración por núcleo";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Cargar automáticamente archivos de anulación";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Guardar configuración al salir";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtrado bilineal por hardware";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma de vídeo";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permitir rotación";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronización de GPU por hardware";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervalo de alternado de VSync";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync/Sincronía vertical";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Vídeo multinúcleo";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotación";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Permitir capturas de pantalla de GPU";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Recortar Overscan (Reinicio)";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Índice de proporción de aspecto";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Proporción de aspecto automática";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forzar proporción de aspecto";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Tasa de actualización";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forzar anulación del FBO sRGB";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Pantalla completa en ventana";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Utilizar modo PAL60";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Filtro contra parpadeos";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Asignar ancho de interfaz visual";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Insertar fotogramas negros";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Sincronizar fotogramas de CPU por hardware";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordenar partidas guardadas por carpetas";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordenar guardados rápidos por carpetas";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Pantalla completa";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Escala de ventana";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Escala integral";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contadores de rendimiento";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Nivel de registro del núcleo";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Verbosidad del registro";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Cargar guardado rápido automáticamente";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Indizar automáticamente guardados rápidos";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Guardado rápido automático";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervalo de autoguardados SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "No sobrescribir SaveRAM al cargar un guardado rápido";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Activar contexto compartido por HW";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Reiniciar RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nombre de usuario";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Idioma";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Permitir cámara";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Permitir ubicación";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pausar al activar el menú";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostrar teclado superpuesto";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostrar superimposición";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Índice del monitor";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Retraso de fotogramas";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo de deberes";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Período de turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Margen de ejes de entrada";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Permitir reasignar controles";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "N.º de usuarios máximos";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Activar autoconfiguración";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Frecuencia de salida de sonido (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Corte máximo de sincronía de sonido";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Pasadas de trucos";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Guardar archivo de reasignación del núcleo";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Guardar archivo de reasignación del juego";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Aplicar cambios en trucos";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Aplicar cambios en shaders";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Activar rebobinado";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Seleccionar de una colección";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Seleccionar archivo y detectar núcleo";
      case MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Seleccionar archivo descargado y detectar núcleo";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Cargar archivos recientes";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Activar sonido";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Mostrar velocidad de fotogramas";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Silenciar sonido";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volumen de sonido (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Activar sincronía de sonido";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Delta de control de frecuencia de sonido";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Pasadas del shader";
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Cargar configuración";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Nivel de detalle del rebobinado";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Cargar archivo de reasignación";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Guardar archivo de reasignación como...";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Proporción personalizada";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "(Utilizar esta carpeta)";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Ejecutar contenido";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opciones del disco del núcleo";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opciones del núcleo";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opciones de trucos del núcleo";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Cargar archivo de trucos";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Guardar archivo de trucos como...";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Contadores del núcleo";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturar pantalla";
      case MENU_LABEL_VALUE_RESUME:
         return "Reanudar";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Índice del disco";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contadores del frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Asignar imagen de disco";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Estado de la bandeja del disco";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "No hay listas de reproducción.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No hay información del núcleo.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No hay opciones del núcleo.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No hay núcleos.";
      case MENU_VALUE_NO_CORE:
         return "Sin núcleo";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gestor de bases de datos";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gestor de cursores";
      case MENU_VALUE_MAIN_MENU: /* Don't change. Breaks everything. (Would be: "Menú principal") */
         return "Main Menu"; 
      case MENU_LABEL_VALUE_SETTINGS:
         return "Ajustes";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Abandonar RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Ayuda";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Guardar configuración nueva";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Reiniciar contenido";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Actualizador de núcleos";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL de núcleos de Buildbot";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL de recursos de Buildbot";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "Seguir navegación: horizontalmente";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_VERTICAL:
         return "Seguir navegación: verticalmente";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtrar por extensiones compatibles";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Extraer automáticamente el archivo descargado";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Información del sistema";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Actualizador en línea";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Información del núcleo";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "No se ha encontrado la carpeta.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "No hay elementos.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Cargar núcleo";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Seleccionar archivo";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Cerrar contenido";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Ajustes de bases de datos";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Guardado rápido";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Carga rápida";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Reanudar contenido";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Controlador de entrada";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Controlador de sonido";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Controlador de joypad";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Controlador de remuestreo de sonido";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Controlador de grabación";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Controlador de menú";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Controlador de cámara";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Controlador de ubicación";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "No se ha podido leer el archivo comprimido.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Escala de superimposición";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Preajuste de superimposición";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Retraso de sonido (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Controlador de sonido";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Preajuste de teclado superimpuesto";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacidad de la superimposición";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Fondo del menú";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Fondo de pantalla dinámico";
      case MENU_LABEL_VALUE_BOXART:
         return "Mostrar carátula";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opciones de reasignación de entrada para el núcleo";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opciones de shaders";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Previsualizar parámetros de shaders";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Parámetros de shaders del menú";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Guardar preajuste de shaders como...";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No hay parámetros de shaders.";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Cargar preajuste de shaders";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtro de vídeo";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin DSP de sonido";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Iniciando descarga: ";
      case MENU_VALUE_SECONDS:
         return "segundos";
      case MENU_VALUE_OFF: //Not changed. Would be "SÍ"
         return "OFF";
      case MENU_VALUE_ON: //Not changed. Would be "NO"
         return "ON";
      case MENU_LABEL_VALUE_UPDATE_ASSETS:
         return "Actualizar recursos";
      case MENU_LABEL_VALUE_UPDATE_CHEATS:
         return "Actualizar trucos";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Actualizar perfiles de autoconfiguración";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "Actualizar perfiles de autoconfiguración (HID)";
      case MENU_LABEL_VALUE_UPDATE_DATABASES:
         return "Actualizar bases de datos";
      case MENU_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Actualizar sobreimposiciones";
      case MENU_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Actualizar shaders Cg";
      case MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Actualizar shaders GLSL";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nombre del núcleo";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Etiqueta del núcleo";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nombre del sistema";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Fabricante del sistema";
      case MENU_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categorías";
      case MENU_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autores";
      case MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permisos";
      case MENU_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licencia(s)";
      case MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Extensiones compatibles";
      case MENU_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Notas del núcleo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Fecha de compilado";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Versión de Git";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Características de CPU";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Identificador del frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nombre del frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "S.O. del frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Nivel de RetroRating";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Fuente de alimentación";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "No hay una fuente";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Cargando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Cargada";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Descargando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Controlador de contexto de vídeo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Mostrar ancho métrico (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Mostrar alto métrico (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Mostrar PPP métricos";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Soporte de LibretroDB";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Soporte de sobreimposiciones";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Soporte de interfaz de comandos";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Soporte de interfaz de comandos en red";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Soporte de Cocoa";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Soporte de PNG (RPNG)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Soporte de SDL1.2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Soporte de SDL2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Soporte de OpenGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Soporte de OpenGL ES";
      case MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Soporte de multinúcleo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Soporte de KMS/EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Soporte de Udev";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Soporte de OpenVG";
      case MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Soporte de EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Soporte de X11";
      case MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Soporte de Wayland";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Soporte de XVideo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Soporte de ALSA";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Soporte de OSS";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Soporte de OpenAL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Soporte de OpenSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Soporte de RSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Soporte de RoarAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Soporte de JACK";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Soporte de PulseAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Soporte de DirectSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Soporte de XAudio2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Soporte de Zlib";
      case MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Soporte de 7zip";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Soporte de librerías dinámicas";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Soporte de Cg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Soporte de GLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Soporte de HLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Soporte de parseo XML libxml2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Soporte de imágenes SDL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Soporte de render-to-texture OpenGL/Direct3D (shaders multipasos)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Soporte de FFmpeg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Soporte de CoreText";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Soporte de FreeType";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Soporte de juego en red (peer-to-peer)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Soporte de Python (soporte de scripts para shaders)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Soporte de Video4Linux2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Soporte de Libusb";
      case MENU_LABEL_VALUE_YES:
         return "Sí";
      case MENU_LABEL_VALUE_NO:
         return "No";
      case MENU_VALUE_BACK:
         return "BACK";
      case MENU_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Resolución de pantalla";
      case MENU_VALUE_DISABLED:
         return "Desactivado";
      case MENU_VALUE_PORT:
         return "Puerto";
      case MENU_VALUE_NONE:
         return "Ninguno";
      case MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Desarrollador";
      case MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Distribuidora";
      case MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Descripción";
      case MENU_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nombre";
      case MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origen";
      case MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franquicia";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Mes de lanzamiento";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Año de lanzamiento";
      case MENU_VALUE_TRUE:
         return "Activado";
      case MENU_VALUE_FALSE:
         return "Desactivado";
      case MENU_VALUE_MISSING:
         return "Desaparecido";
      case MENU_VALUE_PRESENT:
         return "Presente";
      case MENU_VALUE_OPTIONAL:
         return "Opcional";
      case MENU_VALUE_REQUIRED:
         return "Necesario";
      case MENU_VALUE_STATUS:
         return "Estado";
      case MENU_LABEL_VALUE_AUDIO_SETTINGS:
         return "Sonido";
      case MENU_LABEL_VALUE_INPUT_SETTINGS:
         return "Entrada";
      case MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Textos en pantalla (OSD)";
      case MENU_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Sobreimposición";
      case MENU_LABEL_VALUE_MENU_SETTINGS:
         return "Menú";
      case MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Multimedia";
      case MENU_LABEL_VALUE_UI_SETTINGS:
         return "Interfaz de usuario";
      case MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Navegador de archivos del menú";
      case MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Actualizador";
      case MENU_LABEL_VALUE_NETWORK_SETTINGS:
         return "Red";
      case MENU_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Lista de reproducción";
      case MENU_LABEL_VALUE_USER_SETTINGS:
         return "Usuario";
      case MENU_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Carpeta";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Grabación";
      case MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "No hay información disponible.";
      case MENU_LABEL_VALUE_INPUT_USER_BINDS:
         return "Asignaciones de entrada del usuario %u";
      case MENU_VALUE_LANG_ENGLISH:
         return "Inglés";
      case MENU_VALUE_LANG_JAPANESE:
         return "Japonés";
      case MENU_VALUE_LANG_FRENCH:
         return "Francés";
      case MENU_VALUE_LANG_SPANISH:
         return "Español";
      case MENU_VALUE_LANG_GERMAN:
         return "Alemán";
      case MENU_VALUE_LANG_ITALIAN:
         return "Italiano";
      case MENU_VALUE_LANG_DUTCH:
         return "Holandés";
      case MENU_VALUE_LANG_PORTUGUESE:
         return "Portugués";
      case MENU_VALUE_LANG_RUSSIAN:
         return "Ruso";
      case MENU_VALUE_LANG_KOREAN:
         return "Coreano";
      case MENU_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Chino (Tradicional)";
      case MENU_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Chino (Simplificado)";
      case MENU_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_VALUE_LEFT_ANALOG:
         return "Analógico izquierdo";
      case MENU_VALUE_RIGHT_ANALOG:
         return "Analógico derecho";
      case MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Asignaciones de teclas rápidas";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Aumento de fotogramas";
      case MENU_VALUE_SEARCH:
         return "Buscar:";
      case MENU_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Utilizar visualizador de imágenes integrado";
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_es(uint32_t hash, char *s, size_t len)
{
   uint32_t driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   switch (hash)
   {
      case MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
         snprintf(s, len,
               "RetroArch utiliza un formato único para\n"
               "sincronizar vídeo y sonido que necesita ser\n"
               "calibrado con la tasa de actualización de tu\n"
               "monitor para obtener el mejor rendimiento.\n"
               " \n"
               "Si notas cortes de sonido o en la imagen,\n"
               "lo normal es que necesites calibrar estos\n"
               "ajustes. Aquí van algunas opciones:\n"
               " \n"
               "a) Ve a '%s' -> '%s' y activa\n"
               "'Vídeo multinúcleo'. En este modo la tasa\n"
               "de refresco es irrelevante, habrá más fps,\n"
               "pero la imagen podría ser menos fluida.\n"
               "b) Ve a '%s' -> '%s' y busca\n"
               "'%s'. Deja que se ejecute durante\n"
               "2048 fotogramas y selecciona Aceptar.",
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO)
               );
         break;
      case MENU_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
         snprintf(s, len,
               "Para escanear contenidos ve a '%s' y\n"
               "selecciona '%s' o %s'.\n"
               " \n"
               "Esto comparará los archivos con las entradas\n"
               "en la base de datos.\n"
               "Si hay una coincidencia, añadirá una entrada\n"
               "en una colección.\n"
               " \n"
               "Entonces podrás acceder fácilmente al contenido\n"
               "si vas a '%s' ->\n"
               "'%s'\n"
               "en vez de pasar por el navegador de archivos\n"
               "constantemente.\n"
               " \n"
               "NOTA: El contenido de algunos núcleos podría\n"
               "no ser localizable. Entre los ejemplos están\n"
               "PlayStation, MAME, FBA, y puede que otros."
               ,
               menu_hash_to_str(MENU_LABEL_VALUE_ADD_CONTENT_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_SCAN_DIRECTORY),
               menu_hash_to_str(MENU_LABEL_VALUE_SCAN_FILE),
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST)
               );
         break;
      case MENU_LABEL_VALUE_MENU_CONTROLS_PROLOG:
         snprintf(s, len,
               "Puedes usar estos controles en tu mando\n"
               "o teclado para controlar el menú: \n"
               " \n"
               );
         break;
      case MENU_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
         strlcpy(s, "Extrayendo, espera, por favor...\n", len);
         break;
      case MENU_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "Bienvenido a RetroArch\n"
               "\n"
               "Para más información dirígete a Ayuda.\n"
               );
         break;
      case MENU_LABEL_INPUT_DRIVER:
         driver_hash = menu_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               snprintf(s, len,
                     "Controlador de entrada udev. \n"
                     " \n"
                     "Este controlador puede funcionar sin X. \n"
                     " \n"
                     "Utiliza la API más reciente para joypads \n"
                     "evdec para dar compatibilidad con joysticks. \n"
                     "Permite conexión en caliente y force \n"
                     "feedback (si lo admite el dispositivo). \n"
                     " \n"
                     "El controlador lee los eventos evdev para \n"
                     "dar compatibilidad con teclados. También \n"
                     "es compatible con retrollamadas de teclado, \n"
                     "ratones y pantallas táctiles. \n"
                     " \n"
                     "La mayoría de las distros tienen los nodos \n"
                     "/dev/input en modo root-only (modo 600). \n"
                     "Puedes configurar una regla udev que los haga \n"
                     "accesibles fuera de la raíz."
                     );
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "Controlador de entrada linuxraw. \n"
                     " \n"
                     "Este controlador necesita de un TTY activo. \n"
                     "Los eventos de teclado se leen directamente \n"
                     "desde el TTY, lo que es más simple pero no tan \n"
                     "flexible como udev. \n"
                     "No es compatible con ratones, etc. \n"
                     " \n"
                     "Este controlador utiliza la antigua API de \n"
                     "joysticks (/dev/input/js*).");
               break;
            default:
               snprintf(s, len,
                     "Controlador de entrada.\n"
                     " \n"
                     "El controlador de vídeo podría forzar \n"
                     "el uso de un controlador de entrada \n"
                     "distinto.");
               break;
         }
         break;
      case MENU_LABEL_LOAD_CONTENT:
         snprintf(s, len,
               "Cargar contenido. \n"
               "Buscar contenido. \n"
               " \n"
               "Para cargar contenidos necesitas \n"
               "un 'núcleo' y un archivo de contenido.\n"
               " \n"
               "Para controlar el lugar donde el menú \n"
               "empieza a buscar contenidos, cambia \n"
               "la opción 'Carpeta del navegador de \n"
               "archivos'. En caso de que no esté \n"
               "configurada, empezará desde la raíz.\n"
               " \n"
               "El navegador filtrará las extensiones \n"
               "del último núcleo seleccionado en \n"
               "'Cargar núcleo' y lo utilizará al \n"
               "cargar un contenido."
               );
         break;
      case MENU_LABEL_CORE_LIST:
         snprintf(s, len,
               "Cargar núcleo. \n"
               " \n"
               "Busca una implementación de núcleo \n"
               "para libretro. El navegador empezará \n"
               "desde la ruta de tu carpeta de núcleos.\n"
               "Si está en blanco, empezará desde \n"
               "la raíz.\n"
               " \n"
               "Si la carpeta de núcleos es una carpeta,\n"
               "el menú la utilizará como carpeta \n"
               "base. Si la carpeta de núcleos es una \n"
               "ruta completa, empezará en la carpeta \n"
               "donde se encuentre el archivo.");
         break;
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "Cargar contenido del historial. \n"
               " \n"
               "Cuando se cargan contenidos, estos y \n"
               "las combinaciones de núcleos de libretro \n"
               "se guardan en el historial. \n"
               " \n"
               "El historial se guarda en un archivo en la \n"
               "misma carpeta que el archivo de configura- \n"
               "ción de RetroArch. Si no se ha cargado un \n"
               "archivo de configuración al iniciar, no se \n"
               "guardará ni cargará el historial, y la \n"
               "opción no existirá en el menú principal."
               );
         break;
      case MENU_LABEL_VIDEO_DRIVER:
         driver_hash = menu_hash_calculate(settings->video.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_VIDEO_DRIVER_GL:
               snprintf(s, len,
                     "Controlador de vídeo OpenGL. \n"
                     " \n"
                     "Este controlador permite que los núcleos \n"
                     "libretro GL se utilicen, además de las \n"
                     "implementaciones renderizadas por soft-\n"
                     "ware del núcleo.\n"
                     " \n"
                     "El rendimiento de las implementaciones \n"
                     "por software y libretro GL dependen \n"
                     "del controlador GL que tenga tu \n"
                     "tarjeta gráfica.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL2:
               snprintf(s, len,
                     "Controlador de vídeo SDL 2.\n"
                     " \n"
                     "Este es un controlador de vídeo por \n"
                     "software SDL 2.\n"
                     " \n"
                     "El rendimiento para las implementaciones \n"
                     "libretro por software depende de la \n"
                     "implementación SDL de tu plataforma.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL1:
               snprintf(s, len,
                     "Controlador de vídeo SDL.\n"
                     " \n"
                     "Este es un controlador de vídeo por \n"
                     "software SDL 1.2.\n"
                     " \n"
                     "Su rendimiento es considerado inferior \n"
                     "a lo óptimo. Utilízalo únicamente como \n"
                     "último recurso.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_D3D:
               snprintf(s, len,
                     "Controlador de vídeo Direct3D. \n"
                     " \n"
                     "El rendimiento de los núcleos \n"
                     "que rendericen por software dependerá \n"
                     "del controlador D3D de tu tarjeta \n"
                     "gráfica.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_EXYNOS:
               snprintf(s, len,
                     "Controlador de vídeo Exynos-G2D. \n"
                     " \n"
                     "Este es un controlador de vídeo Exynos \n"
                     "de bajo nivel. Utiliza el bloque G2D \n"
                     "del SoC Exynos de Samsung para las \n"
                     "operaciones de blit. \n"
                     " \n"
                     "El rendimiento de los núcleos \n"
                     "renderizados por software debería \n"
                     "ser óptimo.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SUNXI:
               snprintf(s, len,
                     "Controlador de vídeo Sunxi-G2D. \n"
                     " \n"
                     "Este es un controlador de vídeo Sunxi \n"
                     "de bajo nivel. Utiliza el bloque G2D \n"
                     "de todos los SoC Allwinner.");
               break;
            default:
               snprintf(s, len,
                     "Controlador de vídeo actual.");
               break;
         }
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin de sonido DSP.\n"
               " Procesa el sonido antes de enviarlo \n"
               "al controlador."
               );
         break;
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         driver_hash = menu_hash_calculate(settings->audio.resampler);

         switch (driver_hash)
         {
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
               snprintf(s, len,
                     "Implementación SINC en ventana.");
               break;
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
               snprintf(s, len,
                     "Implementación de cosenos complejos.");
               break;
         }
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Cargar preajustes de shaders. \n"
               " \n"
               " Carga un preajuste "
#ifdef HAVE_CG
               "Cg"
#endif
#ifdef HAVE_GLSL
#ifdef HAVE_CG
               "/"
#endif
               "GLSL"
#endif
#ifdef HAVE_HLSL
#if defined(HAVE_CG) || defined(HAVE_HLSL)
               "/"
#endif
               "HLSL"
#endif
               " directamente. \n"
               "El menú de shaders se actualizará. \n"
               " \n"
               "Si el CGP utiliza métodos de escalado \n"
               "complejos (por ejemplo, escalado de \n"
               "origen, el mismo factor para X/Y), podría \n"
               "no mostrar un factor de escalado correcto \n"
               "en el menú."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "La escala de esta pasada. \n"
               " \n"
               "The scale factor accumulates, i.e. 2x \n"
               "for first pass and 2x for second pass \n"
               "will give you a 4x total scale. \n"
               " \n"
               "If there is a scale factor for last \n"
               "pass, the result is stretched to \n"
               "screen with the filter specified in \n"
               "'Default Filter'. \n"
               " \n"
               "If 'Don't Care' is set, either 1x \n"
               "scale or stretch to fullscreen will \n"
               "be used depending if it's not the last \n"
               "pass or not."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         snprintf(s, len,
               "Shader Passes. \n"
               " \n"
               "RetroArch allows you to mix and match various \n"
               "shaders with arbitrary shader passes, with \n"
               "custom hardware filters and scale factors. \n"
               " \n"
               "This option specifies the number of shader \n"
               "passes to use. If you set this to 0, and use \n"
               "Apply Shader Changes, you use a 'blank' shader. \n"
               " \n"
               "The Default Filter option will affect the \n"
               "stretching filter.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Shader Parameters. \n"
               " \n"
               "Modifies current shader directly. Will not be \n"
               "saved to CGP/GLSLP preset file.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Shader Preset Parameters. \n"
               " \n"
               "Modifies shader preset currently in menu."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Path to shader. \n"
               " \n"
               "All shaders must be of the same \n"
               "type (i.e. CG, GLSL or HLSL). \n"
               " \n"
               "Set Shader Directory to set where \n"
               "the browser starts to look for \n"
               "shaders."
               );
         break;
      case MENU_LABEL_CONFIG_SAVE_ON_EXIT:
         snprintf(s, len,
               "Saves config to disk on exit.\n"
               "Useful for menu as settings can be\n"
               "modified. Overwrites the config.\n"
               " \n"
               "#include's and comments are not \n"
               "preserved. \n"
               " \n"
               "By design, the config file is \n"
               "considered immutable as it is \n"
               "likely maintained by the user, \n"
               "and should not be overwritten \n"
               "behind the user's back."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nThis is not not the case on \n"
               "consoles however, where \n"
               "looking at the config file \n"
               "manually isn't really an option."
#endif
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Hardware filter for this pass. \n"
               " \n"
               "If 'Don't Care' is set, 'Default \n"
               "Filter' will be used."
               );
         break;
      case MENU_LABEL_AUTOSAVE_INTERVAL:
         snprintf(s, len,
               "Autosaves the non-volatile SRAM \n"
               "at a regular interval.\n"
               " \n"
               "This is disabled by default unless set \n"
               "otherwise. The interval is measured in \n"
               "seconds. \n"
               " \n"
               "A value of 0 disables autosave.");
         break;
      case MENU_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Input Device Type. \n"
               " \n"
               "Picks which device type to use. This is \n"
               "relevant for the libretro core itself."
               );
         break;
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
         snprintf(s, len,
               "Sets log level for libretro cores \n"
               "(GET_LOG_INTERFACE). \n"
               " \n"
               " If a log level issued by a libretro \n"
               " core is below libretro_log level, it \n"
               " is ignored.\n"
               " \n"
               " DEBUG logs are always ignored unless \n"
               " verbose mode is activated (--verbose).\n"
               " \n"
               " DEBUG = 0\n"
               " INFO  = 1\n"
               " WARN  = 2\n"
               " ERROR = 3"
               );
         break;
      case MENU_LABEL_STATE_SLOT_INCREASE:
      case MENU_LABEL_STATE_SLOT_DECREASE:
         snprintf(s, len,
               "State slots.\n"
               " \n"
               " With slot set to 0, save state name is *.state \n"
               " (or whatever defined on commandline).\n"
               "When slot is != 0, path will be (path)(d), \n"
               "where (d) is slot number.");
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "Apply Shader Changes. \n"
               " \n"
               "After changing shader settings, use this to \n"
               "apply changes. \n"
               " \n"
               "Changing shader settings is a somewhat \n"
               "expensive operation so it has to be \n"
               "done explicitly. \n"
               " \n"
               "When you apply shaders, the menu shader \n"
               "settings are saved to a temporary file (either \n"
               "menu.cgp or menu.glslp) and loaded. The file \n"
               "persists after RetroArch exits. The file is \n"
               "saved to Shader Directory."
               );
         break;
      case MENU_LABEL_INPUT_BIND_DEVICE_ID:
         snprintf(s, len,
               "Input Device. \n"
               " \n"
               "Picks which gamepad to use for user N. \n"
               "The name of the pad is available."
               );
         break;
      case MENU_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Toggles menu.");
         break;
      case MENU_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Toggles mouse grab.\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
         break;
      case MENU_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Cycles through disk images. Use after \n"
               "ejecting. \n"
               " \n"
               " Complete by toggling eject again.");
         break;
      case MENU_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
         snprintf(s, len,
               "CPU-based video filter.");
#else
         snprintf(s, len,
               "CPU-based video filter.\n"
               " \n"
               "Path to a dynamic library.");
#endif
         break;
      case MENU_LABEL_AUDIO_DEVICE:
         snprintf(s, len,
               "Override the default audio device \n"
               "the audio driver uses.\n"
               "This is driver dependent. E.g.\n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA wants a PCM device."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS wants a path (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK wants portnames (e.g. system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound wants an IP address to an RSound \n"
               "server."
#endif
               );
         break;
      case MENU_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Toggles eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
         break;
      case MENU_LABEL_ENABLE_HOTKEY:
         snprintf(s, len,
               "Enable other hotkeys.\n"
               " \n"
               " If this hotkey is bound to either keyboard, \n"
               "joybutton or joyaxis, all other hotkeys will \n"
               "be disabled unless this hotkey is also held \n"
               "at the same time. \n"
               " \n"
               "This is useful for RETRO_KEYBOARD centric \n"
               "implementations which query a large area of \n"
               "the keyboard, where it is not desirable that \n"
               "hotkeys get in the way.");
         break;
      case MENU_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Core Directory. \n"
               " \n"
               "A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case MENU_LABEL_VIDEO_REFRESH_RATE_AUTO:
         snprintf(s, len,
               "Refresh Rate Auto.\n"
               " \n"
               "The accurate refresh rate of our monitor (Hz).\n"
               "This is used to calculate audio input rate with \n"
               "the formula: \n"
               " \n"
               "audio_input_rate = game input rate * display \n"
               "refresh rate / game refresh rate\n"
               " \n"
               "If the implementation does not report any \n"
               "values, NTSC defaults will be assumed for \n"
               "compatibility.\n"
               " \n"
               "This value should stay close to 60Hz to avoid \n"
               "large pitch changes. If your monitor does \n"
               "not run at 60Hz, or something close to it, \n"
               "disable VSync, and leave this at its default.");
         break;
      case MENU_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case MENU_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "Fastforward ratio."
               " \n"
               "The maximum rate at which content will\n"
               "be run when using fast forward.\n"
               " \n"
               " (E.g. 5.0 for 60 fps content => 300 fps \n"
               "cap).\n"
               " \n"
               "RetroArch will go to sleep to ensure that \n"
               "the maximum rate will not be exceeded.\n"
               "Do not rely on this cap to be perfectly \n"
               "accurate.");
         break;
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
         break;
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
         snprintf(s, len,
               "Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
         break;
      case MENU_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
         break;
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         snprintf(s, len,
               "Audio rate control.\n"
               " \n"
               "Setting this to 0 disables rate control.\n"
               "Any other value controls audio rate control \n"
               "delta.\n"
               " \n"
               "Defines how much input rate can be adjusted \n"
               "dynamically.\n"
               " \n"
               " Input rate is defined as: \n"
               " input rate * (1.0 +/- (rate control delta))");
         break;
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
         snprintf(s, len,
               "Maximum audio timing skew.\n"
               " \n"
               "Defines the maximum change in input rate.\n"
               "You may want to increase this to enable\n"
               "very large changes in timing, for example\n"
               "running PAL cores on NTSC displays, at the\n"
               "cost of inaccurate audio pitch.\n"
               " \n"
               " Input rate is defined as: \n"
               " input rate * (1.0 +/- (max timing skew))");
         break;
      case MENU_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Toggles to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case MENU_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case MENU_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Increases audio volume.");
         break;
      case MENU_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Decreases audio volume.");
         break;
      case MENU_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Enable or disable frontend \n"
               "performance counters.");
         break;
      case MENU_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Cores can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Auto Load State\n"
               "is enabled.");
         break;
      case MENU_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case MENU_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Video V-Sync.\n");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
         break;
      case MENU_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case MENU_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Take screenshot.");
         break;
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Sets how many milliseconds to delay\n"
               "after VSync before running the core.\n"
               "\n"
               "Can reduce latency at cost of\n"
               "higher risk of stuttering.\n"
               " \n"
               "Maximum is 15.");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
         snprintf(s, len,
               "Sets how many frames CPU can \n"
               "run ahead of GPU when using 'GPU \n"
               "Hard Sync'.\n"
               " \n"
               "Maximum is 3.\n"
               " \n"
               " 0: Syncs to GPU immediately.\n"
               " 1: Syncs to previous frame.\n"
               " 2: Etc ...");
         break;
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(s, len,
               "Inserts a black frame inbetween \n"
               "frames.\n"
               " \n"
               "Useful for 120 Hz monitors who want to \n"
               "play 60 Hz material with eliminated \n"
               "ghosting.\n"
               " \n"
               "Video refresh rate should still be \n"
               "configured as if it is a 60 Hz monitor \n"
               "(divide refresh rate by 2).");
         break;
      case MENU_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Save Configuration on Exit' is enabled.\n");
         break;
      case MENU_LABEL_CORE_SPECIFIC_CONFIG:
         snprintf(s, len,
               "Load up a specific config file \n"
               "based on the core being used.\n");
         break;
      case MENU_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Toggles fullscreen.");
         break;
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case MENU_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pause gameplay when window focus \n"
               "is lost.");
         break;
      case MENU_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
         break;
      case MENU_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Savefile Directory. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Assets Directory. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Dynamic Wallpapers Directory. \n"
               " \n"
               " The place to store wallpapers that will \n"
               "be loaded dynamically by the menu depending \n"
               "on context.");
         break;
      case MENU_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         snprintf(s, len,
               "Defines axis threshold.\n"
               " \n"
               "How far an axis must be tilted to result\n"
               "in a button press.\n"
               " Possible values are [0.0, 1.0].");
         break;
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len, 
               "Turbo period.\n"
               " \n"
               "Describes speed of which turbo-enabled\n"
               "buttons toggle."
               );
         break;
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case MENU_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Allow or disallow camera access by \n"
               "cores.");
         break;
      case MENU_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Allow or disallow location services \n"
               "access by cores.");
         break;
      case MENU_LABEL_TURBO:
         snprintf(s, len,
               "Turbo enable.\n"
               " \n"
               "Holding the turbo while pressing another \n"
               "button will let the button enter a turbo \n"
               "mode where the button state is modulated \n"
               "with a periodic signal. \n"
               " \n"
               "The modulation stops when the button \n"
               "itself (not turbo button) is released.");
         break;
      case MENU_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Enable/disable on-screen keyboard.");
         break;
      case MENU_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Mute/unmute audio.");
         break;
      case MENU_LABEL_REWIND:
         snprintf(s, len,
               "Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
         break;
      case MENU_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nKilling it in any hard way (SIGKILL, \n"
               "etc) will terminate without saving\n"
               "RAM, etc. On Unix-likes,\n"
               "SIGINT/SIGTERM allows\n"
               "a clean deinitialization."
#endif
               );
         break;
      case MENU_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Loads state.");
         break;
      case MENU_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Saves state.");
         break;
      case MENU_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay flip users.");
         break;
      case MENU_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Increment cheat index.\n");
         break;
      case MENU_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrement cheat index.\n");
         break;
      case MENU_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Applies previous shader in directory.");
         break;
      case MENU_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Applies next shader in directory.");
         break;
      case MENU_LABEL_RESET:
         snprintf(s, len,
               "Reset the content.\n");
         break;
      case MENU_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Toggle between paused and non-paused state.");
         break;
      case MENU_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Toggle cheat index.\n");
         break;
      case MENU_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case MENU_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Hold for slowmotion.");
         break;
      case MENU_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame advance when content is paused.");
         break;
      case MENU_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Toggle between recording and not.");
         break;
      case MENU_LABEL_L_X_PLUS:
      case MENU_LABEL_L_X_MINUS:
      case MENU_LABEL_L_Y_PLUS:
      case MENU_LABEL_L_Y_MINUS:
      case MENU_LABEL_R_X_PLUS:
      case MENU_LABEL_R_X_MINUS:
      case MENU_LABEL_R_Y_PLUS:
      case MENU_LABEL_R_Y_MINUS:
         snprintf(s, len,
               "Axis for analog stick (DualShock-esque).\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case MENU_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
         snprintf(s, len,
               "RetroArch by itself does nothing. \n"
               " \n"
               "To make it do things, you need to \n"
               "load a program into it. \n"
               "\n"
               "We call such a program 'Libretro core', \n"
               "or 'core' in short. \n"
               " \n"
               "To load a core, select one from\n"
               "'Load Core'.\n"
               " \n"
#ifdef HAVE_NETWORKING
               "You can obtain cores in several ways: \n"
               "* Download them by going to\n"
               "'%s' -> '%s'.\n"
               "* Manually move them over to\n"
               "'%s'.",
               menu_hash_to_str(MENU_LABEL_VALUE_ONLINE_UPDATER),
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
               "You can obtain cores by\n"
               "manually moving them over to\n"
               "'%s'.",
               menu_hash_to_str(MENU_LABEL_VALUE_ONLINE_UPDATER),
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
               );
         break;
      case MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
         snprintf(s, len,
               "You can change the virtual gamepad overlay\n"
               "by going to '%s' -> '%s'."
               " \n"
               "From there you can change the overlay,\n"
               "change the size and opacity of the buttons, etc.\n"
               " \n"
               "NOTE: By default, virtual gamepad overlays are\n"
               "hidden when in the menu.\n"
               "If you'd like to change this behavior,\n"
               "you can set '%s' to false.",
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_OVERLAY_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
               );
      default:
         if (s[0] == '\0')
            strlcpy(s, menu_hash_to_str(MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

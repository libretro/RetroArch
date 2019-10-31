/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Alfredo Monclus
 *  Copyright (C) 2019 - Víctor González Fraile
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../verbosity.h"

#ifdef RARCH_INTERNAL
#include "../configuration.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_es_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
    {
       snprintf(s, len,
             "PENDIENTE: Rellenar este mensaje."
             );
       return 0;
    }
    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
    {
       unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

       switch (idx)
       {
          case RARCH_FAST_FORWARD_KEY:
             snprintf(s, len,
                   "Alterna entre la velocidad normal y\n"
                   "el avance rápido."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "Mantén pulsado este botón para usar\n"
                   "el avance rápido."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Alterna la cámara lenta.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Mantén pulsado este botón para usar\n"
                   "la cámara lenta.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "Alterna el uso de la pausa.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "Avanza fotogramas mientras el contenido\n"
                   "está pausado.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "Aplica el siguiente shader de la carpeta.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "Aplica el shader anterior de la carpeta.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "Trucos.");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "Reinicia el contenido.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "Captura la pantalla.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "Silencia o no el audio.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "Alterna el teclado en pantalla.");
             break;
          case RARCH_FPS_TOGGLE:
             snprintf(s, len,
                   "Muestra o no el contador de fotogramas\n"
                   "por segundo.");
             break;
          case RARCH_SEND_DEBUG_INFO:
             snprintf(s, len,
                   "Envía información de diagnóstico de\n"
                   "tu dispositivo y la configuración de\n"
                   "RetroArch a nuestros servidores para\n"
                   "su posterior análisis.");
             break;
          case RARCH_NETPLAY_HOST_TOGGLE:
             snprintf(s, len,
                   "Activa o desactiva el servidor de juego en red.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Cambia entre los modos de jugador o espectador\n"
                   "en el juego en red.");
             break;
          case RARCH_ENABLE_HOTKEY:
             snprintf(s, len,
                   "Activa otras teclas rápidas.\n"
                   " \n"
                   "Si esta tecla rápida está asignada a un\n"
                   "teclado, un botón o un eje de un mando,\n"
                   "el resto de teclas rápidas se desactivarán\n"
                   "a menos que esta tecla sea pulsada\n"
                   "al mismo tiempo.\n"
                   " \n"
                   "Por otro lado, el usuario puede desactivar\n"
                   "todas las teclas rápidas del teclado.");
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                     "Aumenta el volumen del audio.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                     "Disminuye el volumen del audio.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                     "Cambia a la siguiente superposición.\n"
                     " \n"
                     "Se expandirá sobre la pantalla.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                     "Alterna el botón de expulsión\n"
                     "de discos.\n"
                     " \n"
                     "Se utiliza para contenidos\n"
                     "que utilicen varios discos.");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                   "Cambia de imagen de disco.\n"
                   "Utiliza esta opción después de expulsar\n"
                   "un disco.\n"
                   " \n"
                   "Termina la operación volviendo\n"
                   "a pulsar el botón de expulsión.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                   "Permite o no capturar el ratón.\n"
                   " \n"
                   "Al capturar el ratón, RetroArch lo ocultará\n"
                   "y mantendrá el puntero del ratón dentro de\n"
                   "la ventana para que la entrada relativa\n"
                   "del ratón funcione mejor.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                     "Activa o desactiva la prioridad al juego.\n"
                     " \n"
                     "Cuando el juego tiene prioridad, RetroArch desactivará\n"
                     "las teclas rápidas y mantendrá el puntero del ratón\n"
                     "en el interior de la ventana.");
             break;
          case RARCH_MENU_TOGGLE:
             snprintf(s, len,
                   "Muestra u oculta el menú.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "Carga el guardado rápido.");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "Alterna entre los modos de pantalla\n"
                   "completa y ventana.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
               "Asigna una tecla para abandonar\n"
               "RetroArch limpiamente.\n"
               " \n"
               "Si cierras el programa de cualquier\n"
               "forma brusca (SIGKILL, etc.) no se\n"
               "guardarán los progresos, la RAM, etc.\n"
#ifdef __unix__
               "\nEn sistemas Unix, SIGINT/SIGTERM\n"
               "permite un cierre limpio."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
               "Posiciones de guardados rápidos.\n"
               " \n"
               "Si se selecciona la posición 0, el nombre\n"
               "del guardado rápido será *.state (o lo que\n"
               "esté definido en la línea de comandos).\n"
               " \n"
               "Si la ranura es un valor distinto a 0,\n"
               "la ruta será (path)(d), siendo (d)\n"
               "el número de la posición.");
             break;
          case RARCH_SAVE_STATE_KEY:
             snprintf(s, len,
                   "Guarda rápidamente la partida.");
             break;
          case RARCH_REWIND:
             snprintf(s, len,
                     "Mantén pulsado este botón para rebobinar.\n"
                     " \n"
                     "Para que este botón funcione, debes tener\n"
                     "activada la opción de rebobinar.");
             break;
          case RARCH_BSV_RECORD_TOGGLE:
             snprintf(s, len,
               "Activa o desactiva la grabación.");
             break;
          default:
             if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             break;
       }

       return 0;
    }

    switch (msg)
    {
        case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            snprintf(s, len,
                    "Los datos para iniciar sesión con tu\n"
                    "cuenta de Retro Achievements.\n"
                    " \n"
                    "Visita retroachievements.org para registrarte\n"
                    "de forma gratuita.\n"
                    " \n"
                    "Una vez te hayas registrado, tendrás que\n"
                    "introducir tu nombre de usuario y tu\n"
                    "contraseña en RetroArch.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len,
                    "El nombre de usuario de tu cuenta\n"
                    "de Retro Achievements.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len,
                    "La contraseña de tu cuenta\n"
                    "de Retro Achievements.");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            snprintf(s, len,
                    "Traduce el menú y todos los mensajes en\n"
                    "pantalla al idioma que hayas seleccionado.\n"
                    " \n"
                    "Es necesario reiniciara para aplicar\n"
                    "los cambios.\n"
                    " \n"
                    "Nota: Puede que todos los idiomas no\n"
                    "estén implementados.\n"
                    " \n"
                    "Si un idioma no está implementado,\n"
                    "se traducirá al inglés por defecto.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            snprintf(s, len,
			        "Cambia la fuente de letra utilizada\n"
                    "en los mensajes en pantalla.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "Carga automáticamente las opciones del núcleo\n"
                    "específicas para cada contenido.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Carga automáticamente las configuraciones\n"
                    "personalizadas.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Carga automáticamente los archivos\n"
                    "de reasignación de entrada.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Ordena los guardados rápidos mediante\n"
                    "carpetas con el nombre del núcleo libretro\n"
                    "que se utilice.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Ordena los guardados rápidos mediante\n"
                    "carpetas con el nombre del núcleo libretro\n"
                    "que se utilice.");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Abandona el menú y vuelve al contenido.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "Reinicia el contenido desde el principio.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "Cierra el contenido y lo descarga de la memoria.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Si se ha cargado un guardado rápido,\n"
                    "el contenido volverá al estado anterior\n"
                    "a dicha carga.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Si se ha sobrescrito un guardado rápido,\n"
                    "este volverá a su estado anterior.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Captura la pantalla en una imagen.\n"
                    " \n"
                    "La imagen se almacenará dentro de la carpeta\n"
                    "de capturas de pantalla.");
            break;
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Agrega esta entrada a tus favoritos.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "Inicia el contenido.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "Muestra información adicional de los\n"
                    "metadatos del contenido.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Archivo de configuración.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Archivo comprimido.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Archivo de configuración de grabaciones.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Archivos de base de datos de cursores.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Archivo de configuración.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Selecciona esta opción para buscar contenidos\n"
                     "en la carpeta actual.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Selecciona esta opción para asignar esta carpeta.");
            break;
        case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de la base de datos de contenidos.\n"
                     " \n"
                     "La ruta de la carpeta con la base de datos\n"
                     "de contenidos.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de miniaturas.\n"
                     " \n"
                     "Almacena los archivos de las miniaturas.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Carpeta de información de núcleos.\n"
                     " \n"
                     "La carpeta donde se buscará la información\n"
                     "de los núcleos libretro.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de listas de reproducción.\n"
                     " \n"
                     "En esta carpeta se guardarán todos los\n"
                     "archivos de listas de reproducción.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Algunos núcleos son capaces de apagarse.\n"
                     " \n"
                     "Si desactivas esta opción, al apagar\n"
                     "el núcleo se cerrará también RetroArch.\n"
                     " \n"
                     "De lo contrario, en vez de cerrar RetroArch\n"
                     "se cargará un núcleo vacío, de esta forma\n"
                     "podrás permanecer dentro del menú.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Algunos núcleos pueden necesitar archivos\n"
                     "de firmware o de BIOS.\n"
                     " \n"
                     "Si desactivas esta opción, el núcleo\n"
                     "intentará arrancar aunque falten dichos\n"
                     "archivos.");
            break;
        case MENU_ENUM_LABEL_PARENT_DIRECTORY:
            snprintf(s, len,
                     "Vuelve a la carpeta superior.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            snprintf(s, len,
                     "Abre la configuración de permisos de Windows\n"
                     "para activar la característica\n"
                     "broadFileSystemAccess.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER:
           snprintf(s, len,
                     "Abre el selector de archivos del sistema\n"
                     "para acceder a carpetas adicionales.");
           break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
            snprintf(s, len,
                     "Archivo de preset de shaders.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
            snprintf(s, len,
                     "Archivo de shader.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
            snprintf(s, len,
                     "Archivo de reasignación de controles.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
            snprintf(s, len,
                     "Archivo de trucos.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
            snprintf(s, len,
                     "Archivo de superposición.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
            snprintf(s, len,
                     "Archivo de base de datos.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
            snprintf(s, len,
                     "Archivo de fuente TrueType.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
            snprintf(s, len,
                     "Archivo de texto simple.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
            snprintf(s, len,
                     "Vídeo.\n"
                     " \n"
                     "Selecciónalo para abrirlo con el reproductor\n"
                     "de vídeo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Música.\n"
                     " \n"
                     "Selecciónala para abrirla con el reproductor\n"
                     "de música.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Archivo de imagen.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Imagen.\n"
                     " \n"
                     "Selecciónala para abrirla con el visor\n"
                     "de imágenes.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Núcleo libretro.\n"
                     " \n"
                     "Al seleccionar este núcleo lo asociarás\n"
                     "con el juego.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Núcleo libretro.\n"
                     " \n"
                     "Selecciona este archivo para que RetroArch\n"
                     "cargue este núcleo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Carpeta.\n"
                     " \n"
                     "Selecciónala para abrirla.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de caché.\n"
                     " \n"
                     "Los contenidos descomprimidos por RetroArch\n"
                     "se guardarán temporalmente en esta carpeta.");
            break;
        case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
            snprintf(s, len,
                     "Si esta opción está activada, todos los\n"
                     "contenidos cargados en RetroArch se agregarán\n"
                     "automáticamente al historial reciente.");
            break;
        case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Carpeta del explorador de archivos.\n"
                     " \n"
                     "Indica la carpeta de inicio para el explorador\n"
                     "de archivos del menú.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influencia la forma de sondear la entrada\n"
                     "en RetroArch.\n"
                     " \n"
                     "Temprano: El sondeo se realizará antes de\n"
                     "procesar el fotograma.\n"
                     "Normal: El sondeo se realizará cuando se\n"
                     "solicite.\n"
                     "Tardío: El sondeo se realizará tras la\n"
                     "primera petición de estado de entrada\n"
                     "de cada fotograma.\n"
                     " \n"
                     "«Temprano» o «Tardío» pueden reducir la\n"
                     "latencia según tu configuración.\n"
                     "Esta opción será ignorada durante el\n"
                     "juego en red.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
            snprintf(s, len,
                     "Oculta las descripciones de entrada que\n"
                     "no estén configuradas en el núcleo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            snprintf(s, len,
                     "La frecuencia de actualización de tu monitor.\n"
                     "Se utiliza para calcular la frecuencia de\n"
                     "audio más apropiada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            snprintf(s, len,
                     "Fuerza la desactivación del soporte FBO sRGB.\n"
                     "Algunos controladores OpenGL de Intel para\n"
                     "Windows dan problemas de vídeo al activar el\n"
                     "soporte de FBO sRGB.");
            break;
        case MENU_ENUM_LABEL_AUDIO_ENABLE:
            snprintf(s, len,
                     "Activa la salida de audio.");
            break;
        case MENU_ENUM_LABEL_AUDIO_SYNC:
            snprintf(s, len,
                     "Sincroniza el audio con el vídeo (recomendado).");
            break;
        case MENU_ENUM_LABEL_AUDIO_LATENCY:
            snprintf(s, len,
                     "Latencia de audio deseada en milisegundos.\n"
                     "Si el controlador de audio no es capaz de\n"
                     "generar dicha latencia, este valor podría\n"
                     "ser ignorado.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            snprintf(s, len,
                     "Permite que los núcleos puedan rotar la imagen.\n"
                     "Si esta opción está desactivada, las peticiones\n"
                     "de rotación serán reconocidas pero ignoradas.\n"
                     "Útil en aquellas configuraciones donde se\n"
                     "pueda rotar el monitor de forma manual.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
            snprintf(s, len,
                     "Muestra las descripciones de entrada que\n"
                     "estén configuradas en el núcleo en lugar\n"
                     "de las predeterminadas.");
            break;
        case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            snprintf(s, len,
                     "El número de entradas que contendrá la lista\n"
                     "de reproducción con el historial de contenidos.");
            break;
        case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
            snprintf(s, len,
                     "Usar el modo en ventana al cambiar\n"
                     "a pantalla completa.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
            snprintf(s, len,
                     "Cambia el tamaño de la fuente de los\n"
                     "mensajes en pantalla.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
            snprintf(s, len,
                     "Aumenta automáticamente el número de la posición\n"
                     "con cada guardado rápido, lo que generará varios\n"
                     "archivos.\n"
                     "Al cargar el contenido, se seleccionará la\n"
                     "posición de guardado con el valor más alto\n"
                     "disponible (el último guardado rápido).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Muestra la velocidad de fotogramas\n"
                     "por segundo actual.");
            break;
        case MENU_ENUM_LABEL_MEMORY_SHOW:
            snprintf(s, len,
                     "Incorpora la información con el consumo\n"
                     "actual y total de memoria al contador\n"
                     "de FPS/fotogramas.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            snprintf(s, len,
                     "Muestra los mensajes en pantalla.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Indica la posición de los mensajes en pantalla.\n"
                     "Los valores utilizan el rango [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Activa o desactiva la superposición actual.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Oculta la superposición actual en el menú.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Muestra las pulsaciones de los botones del mando\n"
                      "o las teclas del teclado en la superposición.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Selecciona el puerto en el que detectar la entrada\n"
                      "del controlador y mostrarla en la superposición.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Ruta a la superposición de entrada.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Opacidad de la superposición.");
            break;
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE:
            snprintf(s, len,
                     "Activa o desactiva el diseño de vídeo actual.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
            snprintf(s, len,
                     "Ruta del diseño de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW:
            snprintf(s, len,
                     "Los diseños pueden contener múltiples vistas.\n"
                     "Selecciona una.");
            break;
#endif
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Tiempo de espera para asignar una función\n"
                     "de entrada (en segundos).\n"
                     "Indica los segundos que se esperarán hasta\n"
                     "pasar a la siguiente asignación.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Tiempo de pulsación para asignar una\n"
               "función de entrada (en segundos).\n"
               "Indica los segundos que deben pasar al\n"
               "mantener una entrada para asignarla.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_SCALE:
            snprintf(s, len,
                     "Escala de la superposición.");
            break;
        case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            snprintf(s, len,
                     "Frecuencia de muestreo de la salida\n"
                     "de audio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            snprintf(s, len,
                     "Activa esta opción para que los núcleos renderizados\n"
                     "por hardware tengan un contexto privado.\n"
                     "Evita tener que asumir cambios en el estado del\n"
                     "hardware entre fotogramas.");
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Carga un núcleo.\n"
                     " \n"
                     "Busca una implementación de un núcleo\n"
                     "libretro. La posición inicial del explorador\n"
                     "dependerá de la ruta de la carpeta de\n"
                     "núcleos. Si está en blanco, empezará\n"
                     "desde la raíz.\n"
                     " \n"
                     "Si la carpeta de núcleos es una carpeta,\n"
                     "se utilizará como posición inicial.\n"
                     "Si la carpeta es una ruta completa,\n"
                     "la posición será la carpeta donde se\n"
                     "encuentre el archivo.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "Puedes utilizar los siguientes controles,\n"
                     "tanto con el mando como con el teclado,\n"
                     "para controlar el menú:\n"
                     " \n");
            break;
        case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
            snprintf(s, len,
                     "Te damos la bienvenida a RetroArch.\n");
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            /* Arreglo para saltarse la limitación de 509 caracteres por cadena. */
            char u[501];
            const char *t =
                     "RetroArch utiliza un formato único para\n"
                     "sincronizar el audio y el vídeo. Necesita\n"
                     "calibrarse con la tasa de refresco del monitor\n"
                     "para obtener los mejores resultados.\n"
                     " \n"
                     "Si notas cortes en el audio o la imagen,\n"
                     "lo normal es que necesites calibrar estos ajustes.\n"
                     "Prueba algunas de las siguientes opciones:\n"
                     " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) Ve a «%s» -> «%s» y activa\n"
                     "«Vídeo multihilos». En este modo la tasa\n"
                     "de refresco es irrelevante, aumentarán los FPS\n"
                     "pero la imagen será menos fluida.\n"
                     " \n"
                     "b) Ve a «%s» -> «%s» y comprueba\n"
                     "«%s». Deja que se ejecute durante 2048\n"
                     "fotogramas y luego selecciona Aceptar.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcpy(s, t, len);
            strlcat(s, u, len);
        }
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
            snprintf(s, len,
                     "Para escanear contenidos ve a «%s»\n"
                     "y selecciona «%s» o «%s».\n"
                     " \n"
                     "Los archivos serán contrastados con las entradas\n"
                     "en la base de datos. Si hay una coincidencia,\n"
                     "se añadirá una entrada en una lista de reproducción.\n"
                     " \n"
                     "Podrás acceder fácilmente a estos contenidos\n"
                     "a través de «%s» -> «%s»\n"
                     "en vez de tener que pasar por el explorador\n"
                     "de archivos constantemente.\n"
                     " \n"
                     "NOTA: El contenido de algunos núcleos podría\n"
                     "no ser localizable.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
            snprintf(s, len,
                     "Te damos la bienvenida a RetroArch.\n"
                     " \n"
                     "Extrayendo recursos, espera, por favor.\n"
                     "Este proceso puede llevar un tiempo...\n"
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.input_driver : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                     snprintf(s, len,
                           "Controlador de entrada udev.\n"
                           " \n"
                           "Utiliza la API evdev más reciente para dar\n"
                           "soporte para mandos. Permite hotplug\n"
                           "(conexión en caliente) y force feedback\n"
                           "(fuerza de respuesta).\n"
                           " \n"
                           "El controlador lee los eventos evdev para ser\n"
                           "compatible con teclados. También es compatible\n"
                           "con callbacks de teclado, ratones y pantallas\n"
                           "táctiles.\n"
                           " \n"
                           "La mayoría de las distros tienen los nodos\n"
                           "/dev/input en modo solo root (modo 600).\n"
                           "Puedes configurar una regla udev que los haga\n"
                           "accesibles a otros usuarios."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "Controlador de entrada linuxraw.\n"
                           " \n"
                           "Este controlador necesita un TTY activo.\n"
                           "Los eventos de teclado se leen directamente\n"
                           "desde el TTY, lo que es más simple pero\n"
                           "no tan flexible como udev.\n"
                           "No es compatible con ratones, etc.\n"
                           " \n"
                           "Este controlador utiliza la antigua API de mandos\n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Controlador de entrada.\n"
                           " \n"
                           "El controlador de vídeo podría forzar el uso\n"
                           "de un controlador de entrada distinto.");
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "Carga contenidos.\n"
                     "Busca contenidos.\n"
                     " \n"
                     "Para cargar contenidos necesitas un\n"
                     "«núcleo» y un archivo de contenido.\n"
                     " \n"
                     "Para controlar el lugar donde el menú\n"
                     "empezará a buscar contenidos, cambia\n"
                     "la carpeta del explorador de archivos.\n"
                     "En caso de que no esté configurada,\n"
                     "empezará a buscar desde la raíz.\n"
                     " \n"
                     "El explorador filtrará las extensiones\n"
                     "del último núcleo seleccionado en\n"
                     "Cargar núcleo y lo utilizará al cargar\n"
                     "un contenido."
            );
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            snprintf(s, len,
                     "Carga el contenido del historial.\n"
                     " \n"
                     "Cuando se cargan contenidos, estos y\n"
                     "las combinaciones de núcleos de libretro\n"
                     "se guardan en el historial.\n"
                     " \n"
                     "El historial se guarda en un archivo de la\n"
                     "carpeta donde está el archivo de configuración\n"
                     "de RetroArch. Si no se ha cargado un archivo\n"
                     "al iniciar RetroArch, no se guardará ni cargará\n"
                     "el historial, y la opción desaparecerá del\n"
                     "menú principal."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            snprintf(s, len,
                     "Controlador de vídeo actual.");

            if (string_is_equal(settings->arrays.video_driver, "gl"))
            {
                snprintf(s, len,
                  "Controlador de vídeo OpenGL.\n"
                  " \n"
                  "Este controlador permite utilizar núcleos OpenGL\n"
                  "de libretro, además de las implementaciones\n"
                  "renderizadas por software.\n"
                  " \n"
                  "(El rendimiento de las implementaciones libretro\n"
                  "por software y OpenGL dependen del controlador\n"
                  "OpenGL de la tarjeta gráfica.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
            {
                snprintf(s, len,
                  "Controlador de vídeo SDL 2.\n"
                  " \n"
                  "Este es un controlador de vídeo por software.\n"
                  " \n"
                  "El rendimiento para las implementaciones libretro\n"
                  "por software depende de la implementación SDL\n"
                  "de la plataforma.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
            {
                snprintf(s, len,
                  "Controlador de vídeo SDL 1.2.\n"
                  " \n"
                  "Este es un controlador de vídeo por software.\n"
                  " \n"
                  "Su rendimiento es considerado inferior.\n"
                  "Utilízalo únicamente como último recurso.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                  "Controlador de vídeo Direct3D.\n"
                  " \n"
                  "El rendimiento de los núcleos que rendericen\n"
                  "por software dependerá del controlador D3D\n"
                  "de tu tarjeta gráfica.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
            {
                snprintf(s, len,
                  "Controlador de vídeo Exynos-G2D.\n"
                  " \n"
                  "Este es un controlador de vídeo Exynos\n"
                  "de bajo nivel. Utiliza el bloque G2D\n"
                  "del SoC Exynos de Samsung para las\n"
                  "operaciones de blit.\n"
                  " \n"
                  "El rendimiento de los núcleos renderizados\n"
                  "por software debería ser óptimo.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                  "Controlador de vídeo de DRM simple.\n"
                  " \n"
                  "Este es un controlador de vídeo que\n"
                  "usa libdrm para escalado por hardware\n"
                  "mediante las superposiciones de la\n"
                  "GPU.\n");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
            {
                snprintf(s, len,
                  "Controlador de vídeo Sunxi-G2D.\n"
                  " \n"
                  "Este es un controlador de vídeo Sunxi\n"
                  "de bajo nivel. Utiliza el bloque G2D\n"
                  "de los SoC Allwinner.");
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            snprintf(s, len,
               "Plugin de audio DSP.\n"
               "Procesa el audio antes de enviarlo al controlador."
            );
            break;
        case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                     "Implementación de ventana SINC.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                  strlcpy(s,
                     "Implementación de cosenos complejos.", len);
               else if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            }
            break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION: snprintf(s, len, "Asignar CRT");
			break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER: snprintf(s, len, "Asignar CRT SUPER");
			break;

        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
               "Carga presets de shaders.\n"
               " \n"
               "Carga un preset de shaders directamente.\n"
               "El menú de shaders se actualizará.\n"
               " \n"
               "Si el CGP utiliza métodos de escalado complejos\n"
               "(por ejemplo, escalado de origen, el mismo factor\n"
               "para X/Y), el menú podría no mostrar un factor\n"
               "de escalado correcto."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
               "La escala de esta pasada.\n"
               " \n"
               "El factor de escala es acumulativo,\n"
               "por ejemplo: 2x para el primer pase y\n"
               "2x para el segundo dará una escala\n"
               "total de 4x.\n"
               " \n"
               "Si el último pase tiene un factor de\n"
               "escala, el resultado se estirará por\n"
               "toda la pantalla con el filtro\n"
               "predeterminado.\n"
               " \n"
               "Si has seleccionado «No importa», se\n"
               "utilizará o bien la escala 1x o se\n"
               "estirará a pantalla completa dependiendo\n"
               "de si es el último pase o no."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
               "Pases de shaders.\n"
               " \n"
               "RetroArch permite combinar diversos shaders\n"
               "con pasadas arbitrarias, filtros personales\n"
               "de hardware y factores de escala.\n"
               " \n"
               "Esta opción especifica la cantidad de pasadas\n"
               "de shaders a utilizar. Si seleccionas 0 y\n"
               "luego «Aplicar cambios en shaders»,\n"
               "utilizarás un shader «en blanco».\n"
               " \n"
               "La opción Filtro predeterminado cambiará\n"
               "el filtro de escalado.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
               "Parámetros de shaders.\n"
               " \n"
               "Modifica directamente el shader actual.\n"
               "No se guardará en el preset CGP/GLSLP.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            snprintf(s, len,
               "Parámetros del preset de shaders.\n"
               " \n"
               "Modifica el preset de shaders que\n"
               "se encuentra actualmente en el menú."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
               "Ruta al shader.\n"
               " \n"
               "Todos los shaders deben ser del mismo tipo\n"
               "(por ejemplo: CG, GLSL o HLSL).\n"
               " \n"
               "Modifica la carpeta de shaders para indicar\n"
               "la ruta en la que el navegador comenzará\n"
               "a buscar shaders."
            );
            break;
        case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            snprintf(s, len,
               "Determina el orden de carga de los\n"
               "archivos de configuración.");
            break;
        case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            snprintf(s, len,
               "Guarda la configuración al salir.\n"
               "Es útil para el menú, ya que los ajustes\n"
               "pueden ser modificados. Sobrescribirá\n"
               "la configuración existente.\n"
               " \n"
               "No se preservarán los #include y\n"
               "los comentarios.\n"
               " \n"
               "El archivo de configuración se considera\n"
               "inmutable por diseño ya que es probable\n"
               "que el usuario lo controle, por lo que\n"
               "no debería ser sobrescrito sin el\n"
               "consentimiento del usuario."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nSin embargo, este no suele ser el caso\n"
               "en consolas, donde no es posible examinar\n"
               "el archivo de configuración de forma manual."
#endif
            );
            break;
        case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
            snprintf(s, len, "¿Seguro que quieres salir?");
            break;
        case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            snprintf(s, len, "Mostrar archivos y carpetas ocultos.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            snprintf(s, len,
               "Filtro de hardware para esta pasada.\n"
               " \n"
               "Si se ha seleccionado «No importa»,\n"
               "se utilizará el predeterminado."
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
               "Guarda automáticamente la SaveRAM\n"
               "no volátil en intervalos regulares.\n"
               " \n"
               "Esta opción está desactivada por\n"
               "defecto a menos que se haya configurado\n"
               "lo contrario. El intervalo se mide\n"
               "en segundos.\n"
               " \n"
               "Si utilizas 0, desactivarás el\n"
               "guardado automático.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
               "Tipo de dispositivo de entrada.\n"
               " \n"
               "Selecciona el tipo de dispositivo a\n"
               "utilizar. Afecta al núcleo libretro."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
               "Ajusta el nivel de registro de los\n"
               "núcleos libretro (GET_LOG_INTERFACE).\n"
               " \n"
               " Si el nivel de un registro de un núcleo\n"
               " libretro está por debajo del nivel\n"
               " indicado en libretro_log, será ignorado.\n"
               " \n"
               " Los registros DEBUG siempre son ignorados\n"
               " a menos que esté activado el modo\n"
               " de verbosidad (--verbose).\n"
               " \n"
               " DEBUG = 0\n"
               " INFO  = 1\n"
               " WARN  = 2\n"
               " ERROR = 3"
            );
            break;
        case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
        case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
            snprintf(s, len,
               "Posiciones de guardados rápidos.\n"
               " \n"
               " Si se selecciona la posición 0, el nombre\n"
               " del guardado rápido será *.state (o lo que\n"
               " esté definido en la línea de comandos).\n"
               "Si la ranura es un valor distinto a 0,\n"
               "la ruta será (path)(d), siendo (d)\n"
               "el número de la posición.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
               "Aplica los cambios en los shaders.\n"
               " \n"
               "Una vez hayas cambiado los ajustes de\n"
               "shaders puedes utilizar esta opción\n"
               "para aplicar los cambios.\n"
               " \n"
               "Los cambios en los shaders es una operación\n"
               "que consume bastantes recursos, así que\n"
               "han de hacerse de forma explícita.\n"
               " \n"
               "Al aplicar shaders, se guarda su configuración\n"
               "en un archivo temporal (menu.cgp o menu.glslp)\n"
               "y se cargan. El archivo persistirá al\n"
               "abandonar RetroArch y se guardará en la carpeta\n"
               "Shader."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Busca cambios en los archivos de shaders.\n"
                     " \n"
                     "Una vez hayas guardado los cambios\n"
                     "de un shader del disco, el shader\n"
                     "se recompilará automáticamente y se\n"
                     "aplicará al contenido ejecutado."
            );
            break;
        case MENU_ENUM_LABEL_MENU_TOGGLE:
            snprintf(s, len,
               "Muestra u oculta el menú.");
            break;
        case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
               "Permite o no capturar el ratón.\n"
               " \n"
               "Al capturar el ratón, RetroArch lo ocultará\n"
               "y mantendrá el puntero del ratón dentro de\n"
               "la ventana para que la entrada relativa\n"
               "del ratón funcione mejor.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Activa o desactiva la prioridad al juego.\n"
                     " \n"
                     "Cuando el juego tiene prioridad, RetroArch desactivará\n"
                     "las teclas rápidas y mantendrá el puntero del ratón\n"
                     "en el interior de la ventana.");
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
               "Cambia de imagen de disco.\n"
               "Utiliza esta opción después de expulsar\n"
               "un disco.\n"
               " \n"
               "Termina la operación volviendo\n"
               "a pulsar el botón de expulsión.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
            snprintf(s, len,
                  "Filtro de vídeo basado en la CPU.");
#else
            snprintf(s, len,
                     "Filtro de vídeo basado en la CPU.\n"
                     " \n"
                     "Ruta a una librería dinámica.");
#endif
            break;
        case MENU_ENUM_LABEL_AUDIO_DEVICE:
            snprintf(s, len,
                     "Anula el dispositivo de audio predeterminado\n"
                     "que utiliza el controlador de audio.\n"
                     "Esta opción depende del controlador.\n"
                     "Por ejemplo:\n"
#ifdef HAVE_ALSA
            " \n"
            "ALSA necesita un dispositivo PCM."
#endif
#ifdef HAVE_OSS
            " \n"
            "OSS necesita una ruta (por ejemplo:\n"
            "/dev/dsp)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK necesita nombres de puertos\n"
            "(por ejemplo: system:playback1\n"
            ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
            " \n"
            "RSound necesita una dirección IP\n"
            "de un servidor RSound."
#endif
            );
            break;
        case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                     "Alterna el botón de expulsión\n"
                     "de discos.\n"
                     " \n"
                     "Se utiliza para contenidos\n"
                     "que utilicen varios discos.");
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Activa otras teclas rápidas.\n"
                     " \n"
                     "Si esta tecla rápida está asignada a un\n"
                     "teclado, un botón o un eje de un mando,\n"
                     "el resto de teclas rápidas se desactivarán\n"
                     "a menos que esta tecla sea pulsada\n"
                     "al mismo tiempo.\n"
                     " \n"
                     "Esto sirve para implementaciones centradas\n"
                     "en RETRO_KEYBOARD, que ocupan una gran\n"
                     "porción del teclado y en las que no conviene\n"
                     "que las teclas rápidas interfieran con\n"
                     "el funcionamiento normal.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Activa el rebobinado.\n"
                     " \n"
                     "Esta función afecta al rendimiento,\n"
                     "así que está desactivada por defecto.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
            snprintf(s, len,
                     "Aplica el truco nada más activarlo.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
            snprintf(s, len,
                     "Aplica automáticamente los trucos nada más\n"
                     "cargar el juego.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            snprintf(s, len,
               "Carpeta de núcleos.\n"
               " \n"
               "La carpeta donde se buscarán las\n"
               "implementaciones de núcleos libretro.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            /* Work around C89 limitations */
            /* Arreglo para saltarse la limitación de 509 caracteres por cadena. */
            snprintf(s, len,
                     "Frecuencia de actualización automática.\n"
                     " \n"
                     "La frecuencia de actualización precisa del\n"
                     "monitor (en Hz). Se utiliza para calcular\n"
                     "la frecuencia de entrada de audio con esta\n"
                     "fórmula:\n"
                     " \n"
                     "audio_input_rate (frecuencia de entrada\n"
                     "de audio) = velocidad de entrada de juego\n"
                     "* frecuencia de actualización de pantalla\n"
                     "/ frecuencia de actualización de juego\n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     " \n"
                     "Si la implementación no indica un valor, se\n"
                     "asumirá de forma predeterminada el sistema\n"
                     "NTSC por motivos de compatibilidad.\n"
                     " \n"
                     "Este valor debería ser lo más similar a 60 Hz\n"
                     "para evitar cambios en el tono. Si tu monitor\n"
                     "no puede funcionar a una frecuencia igual o\n"
                     "similar a 60 Hz, desactiva la sincronía\n"
                     "vertical y deja esta opción en su valor\n"
                     "predeterminado.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            snprintf(s, len,
                     "Asigna la frecuencia de actualización\n"
                     "sondeada.\n"
                     " \n"
                     "Cambia la frecuencia de actualización al valor\n"
                     "obtenido a través del controlador de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Fuerza una rotación concreta\n"
                     "de la pantalla.\n"
                     " \n"
                     "La rotación se añade a las rotaciones\n"
                     "que indique el núcleo libretro (ver\n"
                     "Rotación de vídeo).");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE:
            snprintf(s, len,
                     "Resolución en pantalla completa.\n"
                     " \n"
                     "Si se indica 0 se utilizará la resolución\n"
                     "del entorno.");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Ajusta la proporción del avance rápido."
                     " \n"
                     "Indica la velocidad máxima a la que\n"
                     "se ejecutará el contenido durante el\n"
                     "avance rápido.\n"
                     " \n"
                     "(Por ejemplo, 5.0 para un contenido a\n"
                     "60 FPS => máximo de 300 FPS).\n"
                     " \n"
                     "RetroArch pasará a segundo plano\n"
                     "para asegurarse de que no se supere\n"
                     "la velocidad máxima. Este límite\n"
                     "no es completamente preciso.");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Sincroniza los FPS al contenido.\n"
                     " \n"
                     "Esta opción es la equivalente a forzar una velocidad\n"
                     "de 1x en el avance rápido. No habrá alteraciones\n"
                     "respecto a la frecuencia de actualización\n"
                     "solicitada por el núcleo ni control dinámico\n"
                     "de la frecuencia de sonido.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Monitor preferido.\n"
                     " \n"
                     "0 (predeterminado) significa que no hay\n"
                     "preferencia alguna y de 1 en adelante\n"
                     "(siendo 1 el primer monitor) sugiere a\n"
                     "RetroArch que utilice ese monitor específico.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Obliga a recortar los fotogramas\n"
                     "con overscan.\n"
                     " \n"
                     "El comportamiento exacto de esta\n"
                     "opción depende de la implementación\n"
                     "del núcleo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Limita el escalado del vídeo\n"
                     "a números enteros.\n"
                     " \n"
                     "El tamaño base depende de la geometría\n"
                     "que indique el sistema y de la relación\n"
                     "de aspecto.\n"
                     " \n"
                     "Si no se ha configurado la opción\n"
                     "Forzar proporción, los valores X/Y\n"
                     "se escalarán de forma independiente\n"
                     "utilizando números enteros.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Volumen de audio expresado en dB.\n"
                     " \n"
                     "0 dB es el volumen normal sin ganancia\n"
                     "aplicada. La ganancia se puede controlar\n"
                     "en tiempo real con los botones de\n"
                     "Subir volumen/Bajar volumen.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Control de la frecuencia de audio.\n"
                     " \n"
                     "Si seleccionas 0, desactivarás el control\n"
                     "de la frecuencia. Cualquier otro valor\n"
                     "cambiará el delta de control de la\n"
                     "frecuencia de audio.\n"
                     " \n"
                     "Define cuánta frecuencia de entrada puede\n"
                     "ajustarse de forma dinámica.\n"
                     " \n"
                     " La frecuencia de entrada se define como:\n"
                     " frecuencia de entrada * (1.0 +/-\n"
                     " (delta de control de frecuencia))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
               "Variación máxima en la sincronía de\n"
               "audio.\n"
               " \n"
               "Define la variación máxima de la\n"
               "frecuencia de entrada. Podrías aumentar\n"
               "el valor para cambiar la sincronía, por\n"
               "ejemplo, si ejecutas núcleos PAL en\n"
               "monitores NTSC; a cambio de alterar el\n"
               "tono de audio.\n"
               " \n"
               " La frecuencia de entrada se define como:\n"
               " frecuencia de entrada * (1.0 +/-\n"
               " (variación máxima de sincronía))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Cambia a la siguiente superposición.\n"
                     " \n"
                     "Se expandirá sobre la pantalla.");
            break;
        case MENU_ENUM_LABEL_LOG_VERBOSITY:
            snprintf(s, len,
                     "Activa o desactiva el nivel de\n"
                     "verbosidad de la interfaz.");
            break;
        case MENU_ENUM_LABEL_VOLUME_UP:
            snprintf(s, len,
                     "Aumenta el volumen del audio.");
            break;
        case MENU_ENUM_LABEL_VOLUME_DOWN:
            snprintf(s, len,
                     "Disminuye el volumen del audio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Fuerza la desactivación de la composición.\n"
                     "Actualmente solo funciona en Windows Vista/7.");
            break;
        case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            snprintf(s, len,
                     "Activa o desactiva los contadores\n"
                     "de rendimiento de la interfaz.");
            break;
        case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            snprintf(s, len,
                     "Carpeta del sistema.\n"
                     " \n"
                     "Asigna la carpeta «system».\n"
                     "Los núcleos pueden utilizarla para cargar\n"
                     "BIOS, configuraciones específicas de\n"
                     "un sistema, etc.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            snprintf(s, len,
                     "Guarda automáticamente un guardado rápido\n"
                     "al cerrar RetroArch.\n"
                     " \n"
                     "RetroArch cargará al arrancar cualquier\n"
                     "guardado rápido que se encuentre en esa ruta\n"
                     "si la opción «Cargar guardado rápido\n"
                     "automáticamente» está activada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Utiliza un controlador de vídeo multihilos.\n"
                     " \n"
                     "Esta opción podría mejorar el rendimiento\n"
                     "a costa de ganar retraso y perder fluidez\n"
                     "de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_VSYNC:
            snprintf(s, len,
                     "Sincronía vertical de la señal de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            snprintf(s, len,
                     "Intenta sincronizar la CPU y la GPU\n"
                     "de una forma más estricta.\n"
                     " \n"
                     "Puede reducir el retraso a costa\n"
                     "de perder rendimiento.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Nivel de detalle del rebobinado.\n"
                     " \n"
                     "Cuando se rebobina un número determinado\n"
                     "de fotogramas, puedes rebobinar varios\n"
                     "fotogramas a la vez, aumentando la\n"
                     "velocidad de rebobinado.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            snprintf(s, len,
                     "Tamaño del búfer de rebobinado (en MB).\n"
                     " \n"
                     "La cantidad de memoria a reservar para\n"
                     "la función de rebobinado. Al aumentar\n"
                     "este valor se incrementará la longitud\n"
                     "del historial de rebobinado.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            snprintf(s, len,
                     "Tamaño de los pasos del búfer de\n"
                     "rebobinado (en MB).\n"
                     " \n"
                     "Cada vez que modifiques el valor del tamaño\n"
                     "del búfer de rebobinado a través de esta\n"
                     "interfaz, cambiará utilizando este valor.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT:
            snprintf(s, len,
                     "Captura la pantalla.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Ajusta la cantidad de retraso en milisegundos\n"
                     "posteriores a la sincronía vertical con\n"
                     "la que se ejecutará el núcleo.\n"
                     " \n"
                     "Puede reducir el retraso a costa de aumentar\n"
                     "la probabilidad de perder fluidez.\n"
                     " \n"
                     "El valor máximo es 15.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_DELAY:
            snprintf(s, len,
                     "Ajusta la cantidad de milisegundos de retraso\n"
                     "previos a la carga automática de shaders.\n"
                     " \n"
                     "Esto puede evitar defectos gráficos provocados\n"
                     "por aplicaciones de captura de pantalla, como\n"
                     "el software para streaming.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "Ajusta la cantidad de fotogramas que puede\n"
                     "adelantar la CPU a la GPU al utilizar\n"
                     "«Sincronía estricta de CPU».\n"
                     " \n"
                     "El valor máximo es 3.\n"
                     " \n"
                     " 0: Sincroniza inmediatamente a GPU.\n"
                     " 1: Sincroniza con el fotograma anterior.\n"
                     " 2: Etcétera...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Introduce fotogramas negros entre fotogramas.\n"
                     " \n"
                     "Es útil para los monitores que funcionan\n"
                     "a 120 Hz y quieren jugar a contenidos a 60 Hz\n"
                     "eliminando el efecto ghosting.\n"
                     " \n"
                     "La frecuencia de actualización de vídeo\n"
                     "aún debe configurarse como si utilizaras\n"
                     "un monitor de 60 Hz (divide la frecuencia\n"
                     "de actualización entre 2).");
            break;
        case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            snprintf(s, len,
                     "Muestra la pantalla de inicio en el menú.\n"
                     "Esta pantalla se desactiva automáticamente\n"
                     "cuando se ha visto por primera vez.\n"
                     " \n"
                     "Esta opción solo se actualizará dentro\n"
                     "de la configuración si «Guardar\n"
                     "configuración al salir» está activado.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
            snprintf(s, len,
                     "Activa o desactiva el modo de\n"
                     "pantalla completa.");
            break;
        case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            snprintf(s, len,
                     "Impide que la SaveRAM sea sobrescrita\n"
                     "al cargar un guardado rápido.\n"
                     " \n"
                     "Podría provocar defectos en los\n"
                     "juegos.");
            break;
        case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
            snprintf(s, len,
                     "Pausa el juego cuando la ventana\n"
                     "pasa a estar en segundo plano.");
            break;
        case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            snprintf(s, len,
                     "Si es posible, las capturas de pantalla\n"
                     "utilizarán la imagen de la GPU con los\n"
                     "shaders ya aplicados.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de capturas de pantalla.\n"
                     " \n"
                     "En esta carpeta se guardarán\n"
                     "las capturas de pantalla."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            snprintf(s, len,
                     "Intervalo de cambio de la\n"
                     "sincronía vertical.\n"
                     " \n"
                     "Utiliza un intervalo personalizado\n"
                     "para la sincronía vertical. Configúralo\n"
                     "para reducir a la mitad la frecuencia\n"
                     "de actualización del monitor.");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de partidas guardadas.\n"
                     " \n"
                     "Almacena todas las partidas guardadas\n"
                     "(*.srm) en esta carpeta. Incluye los\n"
                     "archivos relacionados, como .bsv, .rt,\n"
                     ".psrm, etcétera.\n"
                     " \n"
                     "Esta carpeta será ignorada si se utilizan\n"
                     "explícitamente las opciones por línea\n"
                     "de comandos.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de guardados rápidos.\n"
                     " \n"
                     "Almacena todos los guardados rápidos\n"
                     "(*.state) en esta carpeta.\n"
                     " \n"
                     "Esta carpeta será ignorada si se utilizan\n"
                     "explícitamente las opciones por línea\n"
                     "de comandos.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de recursos.\n"
                     " \n"
                     "Esta ubicación es donde las interfaces\n"
                     "de menús buscarán de forma predeterminada\n"
                     "los recursos a cargar, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de fondos dinámicos de pantalla.\n"
                     " \n"
                     "Aquí se almacenan los fondos de pantalla\n"
                     "que el menú cargará de forma dinámica\n"
                     "según el contexto.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            snprintf(s, len,
                     "Proporción de la cámara lenta.\n"
                     " \n"
                     "Al usar la cámara lenta, el contenido\n"
                     "se ralentizará según este factor.");
            break;
        case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            snprintf(s, len,
                     "Define el umbral de los ejes.\n"
                     " \n"
                     "Indica la distancia que debe alcanzar un eje\n"
                     "para que el botón se considere presionado.\n"
                     "Los valores disponibles son [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            snprintf(s, len,
                     "Duración del turbo.\n"
                     " \n"
                     "Indica la velocidad con la que se\n"
                     "pulsan los botones en el modo turbo.\n"
                     " \n"
                     "Los valores corresponden a fotogramas."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            snprintf(s, len,
                     "Cambia el ciclo de trabajo.\n"
                     " \n"
                     "Describe la duración de un periodo de pulsación\n"
                     "con turbo (en fotogramas)."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
            snprintf(s, len, "Activa el soporte táctil.");
            break;
        case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
            snprintf(s, len, "Utiliza la pantalla frontal\n"
                     "en lugar de la trasera.");
            break;
        case MENU_ENUM_LABEL_MOUSE_ENABLE:
            snprintf(s, len, "Permite usar el ratón dentro del menú.");
            break;
        case MENU_ENUM_LABEL_POINTER_ENABLE:
            snprintf(s, len, "Permite usar la pantalla táctil dentro del menú.");
            break;
        case MENU_ENUM_LABEL_MENU_WALLPAPER:
            snprintf(s, len, "Ruta de la imagen que se usará\n"
                     "como fondo de pantalla.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
            snprintf(s, len,
                     "Indica si se debe volver al principio\n"
                     "de una lista tras llegar a su final.");
            break;
        case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
            snprintf(s, len,
                     "Si desactivas esta opción, el juego\n"
                     "seguirá en marcha en segundo plano\n"
                     "mientras estás en el menú.");
            break;
        case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            snprintf(s, len,
                     "Suspende el protector de pantalla.\n"
                     "El controlador de vídeo puede no respetar\n"
                     "este ajuste.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Ajusta el modo del cliente de juego en red\n"
                     "para el usuario actual. Si esta opción está\n"
                     "desactivada, se utilizará el modo servidor.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "Cantidad de fotogramas retrasados para\n"
                     "el juego en red.\n"
                     " \n"
                     "Un valor elevado mejorará el rendimiento\n"
                     "a costa de aumentar la latencia.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Indica si las partidas de juego en red serán\n"
                     "anunciadas de forma pública.\n"
                     " \n"
                     "Si esta opción está desactivada, los clientes\n"
                     "no podrán acceder mediante la sala pública,\n"
                     "sino que deberán conectarse manualmente.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Indica si se debe empezar el juego en red\n"
                     "en modo espectador.\n"
                     " \n"
                     "Si esta opción está activada, el juego en red\n"
                     "comenzará en modo espectador. Es posible cambiar\n"
                     "el modo a posteriori.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Permite las conexiones en modo esclavo.\n"
                     " \n"
                     "Los clientes en modo esclavo consumen\n"
                     "pocos recursos de CPU en ambos lados,\n"
                     "pero tendrán más problemas de latencia\n"
                     "de red.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Impide las conexiones que no utilicen\n"
                     "el modo esclavo.\n"
                     " \n"
                     "No se recomienda su uso salvo en redes\n"
                     "muy rápidas con máquinas poco potentes.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Ejecuta el juego en red en un modo que no\n"
                     "utilice guardados rápidos.\n"
                     " \n"
                     "La activación de esta opción requiere de\n"
                     "una red muy rápida, pero no se realizarán\n"
                     "rebobinados, de modo que no habrá tirones\n"
                     "durante la sesión.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                     "Indica la frecuencia (en fotogramas) con la\n"
                     "que el servidor del juego en red verificará\n"
                     "que está sincronizado con el cliente.\n"
                     " \n"
                     "Este valor no tiene efectos visibles en la\n"
                     "mayoría de los núcleos y puede ser ignorado.\n"
                     "En el caso de los núcleos no deterministas,\n"
                     "este valor determina las veces en las que los\n"
                     "clientes de juego en red serán sincronizados.\n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "En el caso de los núcleos con fallos, asignar\n"
                     "un valor que no sea cero provocará problemas\n"
                     "graves de rendimiento.\n"
                     "Selecciona cero para no hacer comprobaciones.\n"
                     "Este valor es usado únicamente por el servidor\n"
                     "de juego en red.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "El número de fotogramas de retraso en la entrada\n"
                     "que utilizará el juego en red para ocultar la\n"
                     "latencia de red.\n"
                     " \n"
                     "Esta opción retrasa la entrada local en el\n"
                     "juego en red para que el fotograma ejecutado\n"
                     "sea más próximo a los fotogramas que se\n"
                     "reciben a través de la red, reduciendo los\n"
                     "tirones y el consumo de la CPU, pero\n"
                     "genera un retraso en la entrada visible.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "El rango de fotogramas de retraso de entrada\n"
                     "que puede utilizar el juego en red para\n"
                     "camuflar la latencia de red.\n"
                     " \n"
                     "Si esta opción está activada, el juego en red\n"
                     "ajustará de forma dinámica el número de\n"
                     "fotogramas de retraso en la entrada para\n"
                     "equilibrar el tiempo de la CPU; el retraso\n"
                     "de entrada y la latencia de red. Esto reduce\n"
                     "los tirones y el consumo de la CPU, pero\n"
                     "genera un retraso en la entrada impredecible.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            snprintf(s, len,
                     "Al alojar un servidor, se intentará buscar\n"
                     "conexiones de forma pública a través de\n"
                     "Internet mediante UPnP u otras tecnologías\n"
                     "pensadas para salir de redes locales.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "Al alojar una sesión de juego en red, se\n"
                     "retransmitirá la conexión a un servidor\n"
                     "intermediario (man-in-the-middle) para\n"
                     "sortear cortafuegos o problemas de NAT/\n"
                     "UPnP.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            snprintf(s, len,
                     "Especifica el servidor intermediario (man-\n"
                     "in-the-middle) para el juego en red. Un servidor\n"
                     "que esté físicamente más cerca de ti puede\n"
                     "tener menos latencia.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Indica la cantidad máxima de imágenes en\n"
                     "swapchain. Esto indica al controlador de vídeo\n"
                     "que utilice un búfer de vídeo concreto.\n"
                     " \n"
                     "Búfer simple: 1\n"
                     "Búfer doble:  2\n"
                     "Búfer triple: 3\n"
                     " \n"
                     "Si seleccionas el búfer más adecuado,\n"
                     "podrás alterar la latencia.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            snprintf(s, len,
                     "Suaviza la imagen con un filtro bilineal.\n"
                     "Desactiva esta opción si utilizas shaders.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            snprintf(s, len,
                     "Muestra la fecha o la hora actuales\n"
                     "en el menú.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_STYLE:
           snprintf(s, len,
                     "Indica el formato de la fecha o la hora.");
           break;
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Muestra la carga actual de la batería\n"
                     "en el menú.");
            break;
        case MENU_ENUM_LABEL_CORE_ENABLE:
            snprintf(s, len,
                     "Muestra el núcleo cargado actualmente\n"
                     "en el menú.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            snprintf(s, len,
                     "Activa el juego en red utilizando\n"
                     "el modo servidor.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            snprintf(s, len,
                     "Activa el juego en red utilizando\n"
                     "el modo cliente.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            snprintf(s, len,
                     "Desconecta una sesión activa de juego en red.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Busca sesiones de juego en red en la red local.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Este ajuste está relacionado con\n"
                     "el juego en red.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Carga fondos de pantalla de forma dinámica\n"
                     "según el contexto.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
            snprintf(s, len,
                     "La dirección URL al directorio del\n"
                     "actualizador de núcleos del buildbot libretro.");
            break;
        case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
            snprintf(s, len,
                     "La dirección URL al directorio del\n"
                     "actualizador de recursos del buildbot libretro.");
            break;
        case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
            snprintf(s, len,
                     "Esta opción reemplaza las asignaciones de entrada\n"
                     "con las reasignaciones del núcleo actual.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de superposiciones.\n"
                     " \n"
                     "Define la carpeta que contiene las\n"
                     "superposiciones.");
            break;
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY:
            snprintf(s, len,
                     "Carpeta de diseños de vídeo.\n"
                     " \n"
                     "Define la carpeta que contiene los\n"
                     "diseños de vídeo.");
            break;
#endif
        case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            snprintf(s, len,
                     "Indica el número máximo de usuarios\n"
                     "que puede tener RetroArch.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            snprintf(s, len,
                     "Extrae automáticamente los contenidos de\n"
                     "los archivos comprimidos que se hayan\n"
                     "descargado.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            snprintf(s, len,
                     "Filtra los archivos que se muestren\n"
                     "por sus extensiones.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            snprintf(s, len,
                     "El nombre de usuario de la persona que esté\n"
                     "utilizando RetroArch. Este nombre se utilizará\n"
                     "en las partidas de juego en red.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "El puerto de la dirección IP del servidor.\n"
                     "Puede ser TCP o UDP.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Activa o desactiva el modo espectador\n"
                     "para el juego en red.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            snprintf(s, len,
                     "La dirección del servidor al que quieres\n"
                     "conectarte.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "La contraseña del servidor de juego en red.\n"
                     "Solo se utiliza en el modo servidor.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "La contraseña del servidor de juego en red\n"
                     "para acceder con solo privilegios de\n"
                     "espectador. Solo se utiliza en el modo\n"
                     "servidor.");
            break;
        case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
            snprintf(s, len,
                     "Activa la interfaz de comandos stdin.");
            break;
        case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
            snprintf(s, len,
                     "Inicia el asistente de la interfaz al arrancar\n"
                     "(si está disponible).");
            break;
        case MENU_ENUM_LABEL_MENU_DRIVER:
            snprintf(s, len,
                     "Selecciona el controlador de menú\n"
                     "a utilizar.");
            break;
        case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            snprintf(s, len,
                     "Selecciona la combinación de botones del mando\n"
                     "para abrir o cerrar el menú.\n"
                     " \n"
                     "0: Desactivar\n"
                     "1: L + R + Cruceta abajo a la vez.\n"
                     "2: L3 + R3 a la vez.\n"
                     "3: Start + Select a la vez.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len,
                    "Permite que cualquier usuario pueda controlar\n"
                    "el menú.\n"
                    " \n"
                    "Si esta opción está desactivada, solo podrá\n"
                    "ser controlado por el usuario 1.");
            break;
        case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            snprintf(s, len,
                     "Activa la detección automática de\n"
                     "dispositivos de entrada.\n"
                     " \n"
                     "Intentará configurar de forma automática\n"
                     "los mandos que se conecten, de forma\n"
                     "similar al sistema Plug-and-play.");
            break;
        case MENU_ENUM_LABEL_CAMERA_ALLOW:
            snprintf(s, len,
                     "Permite el acceso a la cámara\n"
                     "por parte de los núcleos.");
            break;
        case MENU_ENUM_LABEL_LOCATION_ALLOW:
            snprintf(s, len,
                     "Permite el uso de los servicios\n"
                     "de localización por parte de los\n"
                     "núcleos.");
            break;
        case MENU_ENUM_LABEL_TURBO:
            snprintf(s, len,
                     "Activa la función de turbo.\n"
                     " \n"
                     "Si mantienes pulsado el botón de turbo\n"
                     "mientras pulsas otro botón, este último\n"
                     "entrará en un modo turbo en el que el\n"
                     "estado del botón se modula mediante una\n"
                     "señal periódica.\n"
                     " \n"
                     "La modulación se detiene cuando se deja\n"
                     "de pulsar el botón original (no el botón\n"
                     "de turbo).");
            break;
        case MENU_ENUM_LABEL_OSK_ENABLE:
            snprintf(s, len,
                     "Activa el teclado en pantalla.");
            break;
        case MENU_ENUM_LABEL_AUDIO_MUTE:
            snprintf(s, len,
                     "Silencia o no el audio.");
            break;
        case MENU_ENUM_LABEL_REWIND:
            snprintf(s, len,
                     "Mantén pulsado este botón para rebobinar.\n"
                     " \n"
                     "Para que este botón funcione, debes tener\n"
                     "activada la opción de rebobinar.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
               "Asigna una tecla para abandonar\n"
               "RetroArch limpiamente."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nSi cierras el programa de cualquier\n"
               "forma brusca (SIGKILL, etc.) no se\n"
               "guardarán los progresos, la RAM, etc.\n"
               "En sistemas Unix, SIGINT/SIGTERM\n"
               "permite un cierre limpio."
#endif
            );
            break;
        case MENU_ENUM_LABEL_LOAD_STATE:
            snprintf(s, len,
                     "Carga el guardado rápido.");
            break;
        case MENU_ENUM_LABEL_SAVE_STATE:
            snprintf(s, len,
                     "Guarda rápidamente la partida.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                   "Cambia entre los modos de jugador o espectador\n"
                   "en el juego en red.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
            snprintf(s, len,
                     "Aumenta el índice de trucos.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
            snprintf(s, len,
                     "Disminuye el índice de trucos.");
            break;
        case MENU_ENUM_LABEL_SHADER_PREV:
            snprintf(s, len,
                     "Aplica el shader anterior de la carpeta.");
            break;
        case MENU_ENUM_LABEL_SHADER_NEXT:
            snprintf(s, len,
                     "Aplica el siguiente shader de la carpeta.");
            break;
        case MENU_ENUM_LABEL_RESET:
            snprintf(s, len,
                     "Reinicia el contenido.");
            break;
        case MENU_ENUM_LABEL_PAUSE_TOGGLE:
            snprintf(s, len,
                     "Alterna el estado en pausa.");
            break;
        case MENU_ENUM_LABEL_CHEAT_TOGGLE:
            snprintf(s, len,
                     "Alterna el índice de trucos.");
            break;
        case MENU_ENUM_LABEL_CHEAT_IDX:
            snprintf(s, len,
                     "Muestra la posición en el índice de la lista.");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
            snprintf(s, len,
                     "Bitmask de la dirección cuando el tamaño\n"
                     "de la búsqueda de memoria es menor a 8 bits.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
            snprintf(s, len,
                     "Indica la cantidad de veces en las que\n"
                     "se aplicará el truco.\n"
                     "Usar con las otras dos opciones de iteración\n"
                     "para cubrir regiones grandes de memoria.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
            snprintf(s, len,
                     "Tras cada «Número de iteraciones»,\n"
                     "se incrementará la dirección de memoria\n"
                     "con este valor multiplicado por el tamaño\n"
                     "de búsqueda de memoria.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
            snprintf(s, len,
                     "Tras cada «Número de iteraciones»,\n"
                     "se incrementará el valor con esta cantidad.");
            break;
        case MENU_ENUM_LABEL_CHEAT_MATCH_IDX:
            snprintf(s, len,
                     "Selecciona una coincidencia.");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            snprintf(s, len,
                     "Busca en la memoria para crear trucos nuevos.");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
            snprintf(s, len,
                     "Izquierda/Derecha para cambiar el bit-size.");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
            snprintf(s, len,
                     "Izquierda/Derecha para cambiar el valor.");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_LT:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_GT:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQ:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS:
            snprintf(s, len,
                     "Izquierda/Derecha para cambiar el valor.");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
            snprintf(s, len,
                     "Izquierda/Derecha para cambiar el valor.");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_VIEW_MATCHES:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_CREATE_OPTION:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_DELETE_OPTION:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN:
            snprintf(s, len,
                     "Big endian    : 258 = 0x0102\n"
                     "Little endian : 258 = 0x0201");
            break;
        case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
            snprintf(s, len,
                     "Mantén pulsado este botón para avanzar\n"
                     "rápidamente. Suéltalo para volver a la\n"
                     "velocidad normal.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
            snprintf(s, len,
                     "Mantén pulsado este botón para ir\n"
                     "a cámara lenta.");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "Avanza un fotograma mientras\n"
                     "el contenido esté en pausa.");
            break;
        case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
            snprintf(s, len,
               "Activa o desactiva la grabación.");
            break;
        case MENU_ENUM_LABEL_L_X_PLUS:
        case MENU_ENUM_LABEL_L_X_MINUS:
        case MENU_ENUM_LABEL_L_Y_PLUS:
        case MENU_ENUM_LABEL_L_Y_MINUS:
        case MENU_ENUM_LABEL_R_X_PLUS:
        case MENU_ENUM_LABEL_R_X_MINUS:
        case MENU_ENUM_LABEL_R_Y_PLUS:
        case MENU_ENUM_LABEL_R_Y_MINUS:
            snprintf(s, len,
                     "El eje de un mando analógico\n"
                     "(estilo DualShock).\n"
                     " \n"
                     "Se asigna como siempre, sin embargo, si se\n"
                     "asigna un eje analógico real, puede leerse\n"
                     "de forma analógica.\n"
                     " \n"
                     "El eje X positivo es hacia la derecha.\n"
                     "El eje Y positivo es hacia abajo.");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "RetroArch, por sí solo, no hace nada.\n"
                     " \n"
                     "Para que haga algo necesitas cargar\n"
                     "programas que llamamos «núcleos libretro»,\n"
                     "o núcleos a secas.\n"
                     " \n"
                     "Para cargar un núcleo, selecciona uno\n"
                     "en «Cargar núcleo».\n"
                     " \n"
#ifdef HAVE_NETWORKING
                     "Puedes conseguir núcleos de varias formas:\n"
                     "* Descárgalos en:\n"
                     "«%s» -> «%s».\n"
                     "* Cópialos manualmente a:\n"
                     "«%s».",
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                     "Puedes conseguir núcleos si los copias\n"
                     "en la carpeta «%s».",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
            snprintf(s, len,
                     "Puedes cambiar la superposición del mando\n"
                     "virtual si vas a «%s» ->\n"
                     "«%s».\n"
                     " \n"
                     "Allí podrás cambiar la superposición,\n"
                     "el tamaño y opacidad de sus botones, etc.\n"
                     " \n"
                     "NOTA: Las superposiciones de mandos\n"
                     "virtuales están ocultas de forma\n"
                     "predeterminada dentro del menú.\n"
                     "Si quieres cambiar este comportamiento,\n"
                     "desactiva «%s».",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "Elige un color para el fondo de los\n"
                     "mensajes en pantalla.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "Ajusta el componente rojo del color\n"
                     "de fondo de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "Ajusta el componente verde del color\n"
                     "de fondo de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "Ajusta el componente azul del color\n"
                     "de fondo de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "Ajusta la opacidad del color de fondo\n"
                     "de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0.0 a 1.0.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "Ajusta el componente rojo del color de\n"
                     "los textos de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "Ajusta el componente verde del color de\n"
                     "los textos de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "Ajusta el componente azul del color de\n"
                     "los textos de los mensajes en pantalla.\n"
                     "Los valores válidos son de 0 a 255.");
            break;
        case MENU_ENUM_LABEL_MIDI_DRIVER:
            snprintf(s, len,
                     "Selecciona el controlador MIDI.");
            break;
        case MENU_ENUM_LABEL_MIDI_INPUT:
            snprintf(s, len,
                     "Asigna el dispositivo de entrada (específico\n"
                     "para el controlador).\n"
                     "La entrada de sonido MIDI se desactivará\n"
                     "al desactivar esta opción. También se puede\n"
                     "introducir el nombre del dispositivo de\n"
                     "forma manual.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Asigna el dispositivo de salida (específico\n"
                     "para el controlador).\n"
                     "La salida de sonido MIDI se desactivará\n"
                     "al desactivar esta opción. También se puede\n"
                     "introducir el nombre del dispositivo de\n"
                     "forma manual.\n"
                     " \n"
                     "Al activar la salida MIDI y si el núcleo y\n"
                     "el juego o aplicación son compatibles con la\n"
                     "salida MIDI, algunos o todos los sonidos\n"
                     "(según el programa) serán generados por el\n"
                     "dispositivo MIDI.\n"
                     "Si se utiliza el controlador MIDI «vacío»,\n"
                     "esos sonidos no se reproducirán.");
            break;
        case MENU_ENUM_LABEL_MIDI_VOLUME:
            snprintf(s, len,
                     "Ajusta el volumen maestro del dispositivo\n"
                     "de salida.");
            break;
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}
#endif

#ifdef HAVE_MENU
static const char *menu_hash_to_str_es_label_enum(enum msg_hash_enums msg)
{
   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      static char hotkey_lbl[128] = {0};
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;
      snprintf(hotkey_lbl, sizeof(hotkey_lbl), "input_hotkey_binds_%d", idx);
      return hotkey_lbl;
   }

   switch (msg)
   {
#include "msg_hash_lbl.h"
      default:
#if 0
         RARCH_LOG("Sin implementar: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_es(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_es_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_es.h"
        default:
#if 0
            RARCH_LOG("Sin implementar: [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }

    return "null";
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Alfredo Monclus
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
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../configuration.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif

int menu_hash_get_help_es_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   uint32_t driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   switch (msg)
   {
      case MENU_ENUM_LABEL_CORE_LIST:
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
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
         snprintf(s, len,
               "Puedes usar estos controles en tu mando\n"
               "o teclado para controlar el menú: \n"
               " \n"
               );
         break;
      case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "Bienvenido a RetroArch\n"
               "\n"
               "Para más información ve al menú \n"
              "de Ayuda.\n"
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
         {
            /* Work around C89 limitations */
            char u[501];

            const char * t =
                  "RetroArch utiliza un formato único para \n"
                  "sincronizar vídeo y sonido que necesita \n"
                  "calibrarse con la frecuencia de \n"
                  "actualización de tu monitor para obtener \n"
                  "el mejor rendimiento. \n"
                  " \n"
                  "Si notas cortes de sonido o en la imagen,\n"
                  "lo normal es que necesites calibrar estos\n"
                  "ajustes. Aquí van algunas opciones:\n"
                  " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                  "a) Ve a '%s' -> '%s' y activa\n"
                  "'Vídeo por hilos'. En este modo la tasa\n"
                  "de refresco es irrelevante, habrá más fps,\n"
                  "'Vídeo multinúcleo'. En este modo la \n"
                  "frecuencia es irrelevante, habrá más fps,\n"
                  "pero la imagen podría ser menos fluida.\n"
                  "b) Ve a '%s' -> '%s' y busca\n"
                  "'%s'. Deja que se ejecute durante\n"
                  "2048 fotogramas y selecciona Aceptar.",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO)
                     );
            strlcpy(s, t, len);
            strlcat(s, u, len);
         }
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
         snprintf(s, len,
               "Para escanear contenidos ve a '%s' \n"
               "y selecciona '%s' o \n"
               "'%s'.\n"
               " \n"
               "Esto comparará los archivos con las entradas en \n"
               "la base de datos. Si hay una coincidencia, \n"
               "añadirá una entrada en una colección.\n"
               " \n"
               "Entonces podrás acceder fácilmente al contenido\n"
               "si vas a '%s' ->\n"
               "'%s'\n"
               "en vez de tener que pasar por el navegador \n"
               "de archivos constantemente.\n"
               " \n"
               "NOTA: El contenido de algunos núcleos podría\n"
               "no ser localizable. Entre los ejemplos están\n"
               "PlayStation, MAME, FBA, y puede que otros."
               ,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST)
               );
         break;
      case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
         strlcpy(s, "Extrayendo, espera, por favor...\n", len);
         break;
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         if (settings)
            driver_hash = msg_hash_calculate(settings->arrays.input_driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               {
                  /* Work around C89 limitations */
                  char u[501];
                  char t[501];

                  strlcpy(t,
                        "Controlador de entrada udev. \n"
                        " \n"
                        "Este controlador puede funcionar sin X. \n"
                        " \n"
                        "Utiliza la API más reciente para joypads \n"
                        "evdec para dar compatibilidad con joysticks. \n"
                        "Permite conexión en caliente y force \n"
                        "feedback (si lo admite el dispositivo). \n",
                        sizeof(t));
                  strlcpy(u,
                        " \n"
                        "El controlador lee los eventos evdev para \n"
                        "dar compatibilidad con teclados. También \n"
                        "es compatible con retrollamadas de teclado, \n"
                        "ratones y pantallas táctiles. \n"
                        " \n"
                        "La mayoría de las distros tienen los nodos \n"
                        "/dev/input en modo root-only (modo 600). \n"
                        "Puedes configurar una regla udev que los haga \n"
                        "accesibles fuera de la raíz.", sizeof(u)
                        );

                  strlcpy(s, t, len);
                  strlcat(s, u, len);
               }
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
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
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
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
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
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "Controlador de vídeo actual.");

         if (string_is_equal_fast(settings->arrays.video_driver, "gl", 2))
         {
            snprintf(s, len,
                  "Controlador de vídeo OpenGL. \n"
                  " \n"
                  "Este controlador permite que los núcleos \n"
                  "libretro GL se utilicen, además de las \n"
                  "implementaciones renderizadas por\n"
                  "software del núcleo.\n"
                  " \n"
                  "El rendimiento de las implementaciones \n"
                  "por software y libretro GL dependen \n"
                  "del controlador GL que tenga tu \n"
                  "tarjeta gráfica.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "sdl2", 4))
         {
            snprintf(s, len,
                  "Controlador de vídeo SDL 2.\n"
                  " \n"
                  "Este es un controlador de vídeo por \n"
                  "software SDL 2.\n"
                  " \n"
                  "El rendimiento para las implementaciones \n"
                  "libretro por software depende de la \n"
                  "implementación SDL de tu plataforma.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "sdl1", 4))
         {
            snprintf(s, len,
                  "Controlador de vídeo SDL.\n"
                  " \n"
                  "Este es un controlador de vídeo por \n"
                  "software SDL 1.2.\n"
                  " \n"
                  "Su rendimiento es considerado inferior \n"
                  "a lo óptimo. Utilízalo únicamente como \n"
                  "último recurso.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "d3d", 3))
         {
            snprintf(s, len,
                  "Controlador de vídeo Direct3D. \n"
                  " \n"
                  "El rendimiento de los núcleos que \n"
                  "rendericen por software dependerá \n"
                  "del controlador D3D de tu tarjeta \n"
                  "gráfica.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "exynos", 6))
         {
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
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "drm", 3))
         {
            snprintf(s, len,
                  "Controlador de vídeo de DRM simple. \n"
                  " \n"
                  "Este es un controlador de vídeo que \n"
                  "usa libdrm para escalado por hardware \n"
                  "mediante los overlays de la GPU. \n"
                  " \n"
                  "El blitting se hace por software.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "sunxi", 5))
         {
            snprintf(s, len,
                  "Controlador de vídeo Sunxi-G2D. \n"
                  " \n"
                  "Este es un controlador de vídeo Sunxi \n"
                  "de bajo nivel. Utiliza el bloque G2D \n"
                  "de todos los SoC Allwinner.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin de sonido DSP.\n"
               "Procesa el sonido antes de enviarlo \n"
               "al controlador."
               );
         break;
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         if (settings)
            driver_hash = msg_hash_calculate(settings->arrays.audio_resampler);

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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Cargar preajustes de shaders. \n"
               " \n"
               " Carga directamente un preajuste "
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
               ". \n"
               "El menú de shaders se actualizará. \n"
               " \n"
               "Si el CGP utiliza métodos de escalado \n"
               "complejos (por ejemplo, escalado de \n"
               "origen, el mismo factor para X/Y), podría \n"
               "no mostrar un factor de escalado correcto \n"
               "en el menú."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "La escala de esta pasada. \n"
               " \n"
               "El factor de escala es acumulativo, \n"
               "por ejemplo: 2x para el primer pase y \n"
               "2x para el segundo dará una escala \n"
               "total de 4x. \n"
               " \n"
               "Si el último pase tiene un factor de \n"
               "escala, el resultado se estirará por \n"
               "toda la pantalla con el filtro espe- \n"
               "cificado en 'Filtro predeterminado'. \n"
               " \n"
               "Si has seleccionado 'No importa', se \n"
               "utilizará o bien la escala 1x o se \n"
               "estirará a pantalla completa en función \n"
               "de si es el último pase o no."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
         snprintf(s, len,
               "Pases de shaders. \n"
               " \n"
               "RetroArch permite mezclar diversos shaders \n"
               "con pasadas arbitrarias, filtros persona- \n"
               "lizados de hardware y factores de escala. \n"
               " \n"
               "Esta opción especifica la cantidad de pasadas \n"
               "de shaders a utilizar. Si seleccionas 0 y \n"
               "luego 'Aplicar cambios en shaders', \n"
               "utilizarás un shader 'en blanco'. \n"
               " \n"
               "La opción filtro predeterminado afectará \n"
               "al filtro de estiramiento.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Parámetros de shaders. \n"
               " \n"
               "Modifica directamente el shader actual. \n"
               "No se guardará en el preajuste CGP/GLSLP.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Parámetros del preajuste de shaders. \n"
               " \n"
               "Modifica el preajuste de shaders que \n"
               "se encuentra actualmente en el menú."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Ruta al shader. \n"
               " \n"
               "Todos los shaders deben ser del \n"
               "mismo tipo (por ejemplo: CG, GLSL \n"
               "o HLSL). \n"
               " \n"
               "Modifica la carpeta de shaders para \n"
               "indicar la ruta en la que el navegador \n"
               "comenzará a buscar shaders."
               );
         break;
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
         snprintf(s, len,
               "Guarda la configuración al salir.\n"
               "Es útil para el menú, ya que los \n"
               "ajustes pueden ser modificados. \n"
               "Sobrescribirá la configuración. \n"
               " \n"
               "No se preservarán los #include \n"
               "y los comentarios. \n"
               " \n"
               "El archivo de configuración se \n"
               "considera inmutable por diseño \n"
               "ya que es probable que el usuario \n"
               "lo controle, y no debería ser \n"
               "sobrescrito sin que el usuario \n"
               "no se entere."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nSin embargo, no suele ser el caso \n"
               "en consolas, donde no es posible \n"
               "examinar el archivo de configuración \n"
               "de forma manual."
#endif
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Filtro de hardware para esta pasada. \n"
               " \n"
               "Si se ha seleccionado 'No importa', \n"
               "se utilizará el filtro predeterminado."
               );
         break;
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
         snprintf(s, len,
               "Guarda automáticamente la SaveRAM \n"
               "no volátil en intervalos regulares.\n"
               " \n"
               "Esta opción está desactivada por \n"
               "defecto a menos que se haya configurado \n"
               "lo contrario. El intervalo se mide \n"
               "en segundos. \n"
               " \n"
               "Si utlizas 0, desactivarás el \n"
               "guardado automático.");
         break;
      case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Tipo de dispositivo de entrada. \n"
               " \n"
               "Selecciona el tipo de dispositivo a \n"
               "utilizar. Es relevante para el núcleo \n"
               "de libretro."
               );
         break;
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
         snprintf(s, len,
               "Ajusta el nivel de registro de los \n"
               "núcleos de libretro (GET_LOG_INTERFACE). \n"
               " \n"
               " Si un nivel de registro indicado por \n"
               " un núcleo de libretro está por debajo \n"
               " del nivel indicado en libretro_log, \n"
               " será ignorado.\n"
               " \n"
               " Los registros DEBUG siempre son ignorados \n"
               " a menos que esté activado el modo \n"
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
               "Ranuras de guardados rápidos.\n"
               " \n"
               " Si se selecciona la ranura 0, el nombre \n"
               " del guardado rápido es *.state \n"
               " (o lo que esté definido en la línea \n"
               "de comandos).\n"
               "Si la ranura es != 0, la ruta será (path)(d), \n"
               "siendo (d) el número de ranura.");
         break;
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "Aplicar cambios en shaders. \n"
               " \n"
               "Después de cambiar los ajustes de shaders, \n"
               "utiliza esta opción para aplicar los \n"
               "cambios. \n"
               " \n"
               "Los cambios en los shaders es una \n"
               "operación que ocupa bastante memoria, \n"
               "así que han de hacerse de forma \n"
               "explícita. \n"
               " \n"
               "Al aplicar shaders, se guarda su configuración \n"
               "a un archivo temporal (menu.cgp ormenu.glslp) \n"
               "y se cargan. El archivo persistirá al \n"
               "abandonar RetroArch. El archivo se guarda en \n"
               "la carpeta Shader."
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
               "Al capturar el ratón, RetroArch lo oculta \n"
               "y mantiene el puntero del ratón dentro de \n"
               "la ventana para que la entrada relativa \n"
               "del ratón funcione mejor.");
         break;
      case MENU_ENUM_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Rota entre las imágenes de disco. \n"
               "Utiliza esta opción después de \n"
               "expulsar un disco. \n"
               " \n"
               " Termina la operación volviendo \n"
               " a conmutar el botón de expulsión.");
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
               "Anula el dispositivo de sonido \n"
               "predeterminado que utiliza el \n"
               "controlador de sonido.\n"
               "Esta opción depende del contro- \n"
               "lador. Por ejemplo:\n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA necesita un dispositivo PCM."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS necesita una ruta (por ejemplo: \n"
               "/dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK necesita nombres de puertos \n"
               "(por ejemplo: system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound necesita una dirección IP \n"
               "de un servidor RSound."
#endif
               );
         break;
      case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Alterna el botón de expulsión \n"
               "de discos.\n"
               " \n"
               "Se utiliza para contenidos \n"
               "que utilicen varios discos.");
         break;
      case MENU_ENUM_LABEL_ENABLE_HOTKEY:
         snprintf(s, len,
               "Activa otras teclas rápidas.\n"
               " \n"
               "Si esta tecla rápida está asignada a un \n"
               "teclado, un botón o un eje de un joystick, \n"
               "el resto de teclas rápidas se desactivarán \n"
               "a menos que esta tecla se mantenga pulsada \n"
               "al mismo tiempo. \n"
               " \n"
               "Esto sirve para implementaciones centradas \n"
               "en RETRO_KEYBOARD, que ocupan una gran \n"
               "porción del teclado y no es una buena idea \n"
               "que las teclas rápidas interfieran con \n"
               "el funcionamiento normal.");
         break;
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Activa el rebobinado.\n"
               " \n"
               "Esto consume rendimiento, así que \n"
               "está desactivado por defecto.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Carpeta de núcleos. \n"
               " \n"
               "Una carpeta donde se buscarán las \n"
               "implementaciones de núcleos libretro.");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         {
            /* Work around C89 limitations */
            char u[501];
            char t[501];

            strlcpy(t,
                  "Frecuencia de actualización automática.\n"
                  " \n"
                  "La frecuencia de actualización precisa del \n"
                  "monitor (en Hz). Se utiliza para calcular \n"
                  "la frecuencia de entrada de sonido con esta \n"
                  "fórmula: \n"
                  " \n"
                  "audio_input_rate = veloc. de entrada de juego \n"
                  "* frecuencia de actualización de pantalla / \n"
                  "frecuencia de actualización de juego \n"
                  " \n", sizeof(t));
            strlcpy(u,
                  "Si la implementación no indica un valor, se \n"
                  "asumirá de forma predeterminada el sistema \n"
                  "NTSC por compatibilidad.\n"
                  " \n"
                  "Este valor debería ser lo más similar a 60Hz \n"
                  "para evitar cambios en el tono. Si tu \n"
                  "monitor no funciona a 60Hz o similar, \n"
                  "desactiva la sincronía vertical y deja \n"
                  "esta opción en su valor predeterminado.", sizeof(u));
            strlcpy(s, t, len);
            strlcat(s, u, len);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Fuerza una rotación concreta \n"
               "de la pantalla.\n"
               " \n"
               "La rotación se añade a las rotaciones \n"
               "que indique el núcleo libretro (ver \n"
               "Permitir rotación de vídeo).");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Resolución en pantalla completa.\n"
               " \n"
               "Si se indica 0, se utilizará la \n"
               "resolución del entorno.\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "Proporción de aceleración."
               " \n"
               "Indica la velocidad máxima a la que \n"
               "se ejecutará el contenido mientras \n"
               "es acelerado.\n"
               " \n"
               " (Por ejemplo, 5.0 para un contenido \n"
               "a 60 FPS => Máximo de 300 FPS).\n"
               " \n"
               "RetroArch pasará a segundo plano \n"
               "para asegurarse de que no se supere \n"
               "la velocidad máxima. Este límite \n"
               "no es completamente preciso.");
         break;
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Monitor preferido.\n"
               " \n"
               "0 (predeterminado) significa que no hay \n"
               "preferencia por un monitor en concreto, \n"
               "y de 1 en adelante (siendo 1 el primer \n"
               "monitor) sugiere a RetroArch que \n"
               "utilice ese monitor específico.");
         break;
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Obliga a recortar los fotogramas \n"
               "con overscan.\n"
               " \n"
               "El comportamiento exacto de esta \n"
               "opción depende de la implementación \n"
               "del núcleo.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         snprintf(s, len,
               "Solo escala el vídeo con números \n"
               "enteros.\n"
               " \n"
               "El tamaño base depende de la geometría \n"
               "que indique el sistema y de la \n"
               "proporción de aspecto.\n"
               " \n"
               "Si no se ha configurado la opción \n"
               "Forzar proporción, los valores X/Y \n"
               "se escalarán de forma independiente \n"
               "según números enteros.");
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Volumen de sonido expresado en dB.\n"
               " \n"
               "0 dB es el volumen normal, sin ganancia \n"
               "aplicada. La ganancia se puede controlar \n"
               "en tiempo real con los botones de \n"
               "Subir volumen / Bajar volumen.");
         break;
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         snprintf(s, len,
               "Control de la frecuencia de sonido.\n"
               " \n"
               "Si seleccionas 0, desactivarás el control \n"
               "de la frecuencia. Cualquier otro valor \n"
               "cambiará el delta de control de la \n"
               "frecuencia de sonido.\n"
               " \n"
               "Define cuánta frecuencia de entrada puede \n"
               "ajustarse de forma dinámica.\n"
               " \n"
               " La frecuencia de entrada se define como: \n"
               " frecuencia de entrada * (1.0 +/- \n"
               "(delta de control de frecuencia))");
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         snprintf(s, len,
               "Variación máxima en la sincronía de \n"
               "sonido.\n"
               " \n"
               "Define la variación máxima de la \n"
               "frecuencia de entrada. Podrías aumentar \n"
               "el valor para cambiar la sincronía, por \n"
               "ejemplo, si ejecutas núcleos PAL en \n"
               "monitores NTSC, a cambio de tener un \n"
               "tono de sonido impreciso.\n"
               " \n"
               " La frecuencia de entrada se define como: \n"
               " frecuencia de entrada * (1.0 +/- \n"
               "(variación máxima de sincronía))");
         break;
      case MENU_ENUM_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Cambia a la siguiente superposición.\n"
               " \n"
               "Se expande alrededor.");
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Activa o desactiva el nivel de \n"
               "verbosidad del frontend.");
         break;
      case MENU_ENUM_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Aumenta el volumen del sonido.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Disminuye el volumen del sonido.");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Desactiva por la fuerza la composición.\n"
               "Actualmente solo funciona en \n"
               "Windows Vista/7.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Activa o desactiva los contadores \n"
               "de rendimiento del frontend.");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "Carpeta de sistemas. \n"
               " \n"
               "Asigna la carpeta 'system'.\n"
               "Los núcleos pueden buscar dentro \n"
               "de ella para cargar BIOS, \n"
               "configuraciones específicas para \n"
               "un sistema, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Guarda automáticamente un guardado rápido \n"
               "al terminar la ejecución de RetroArch.\n"
               " \n"
               "RetroArch cargará automáticamente cualquier \n"
               "guardado rápido que se encuentre en esa ruta \n"
               "al arrancar si la opción 'Cargar guardado \n"
               "rápido automáticamente' está activada.");
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Utilizar un controlador de vídeo \n"
               "por hilos.\n"
               " \n"
               "Esta opción podría mejorar el rendimiento \n"
               "a costa de ganar retraso y perder fluidez \n"
               "de vídeo.");
         break;
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Sincronía vertical para vídeo.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Intenta sincronizar la CPU y la GPU \n"
               "de una forma más estricta.\n"
               " \n"
               "Puede reducir el retraso a costa \n"
               "de perder rendimiento.");
         break;
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Nivel de detalle del rebobinado.\n"
               " \n"
               "Cuando se rebobina un número determinado \n"
               "de fotogramas, puedes rebobinar varios \n"
               "fotogramas a la vez, aumentando la \n"
               "velocidad de rebobinado.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Capturar la pantalla.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Ajusta la cantidad de retraso en \n"
               "milisegundos después de la sincronía \n"
               "vertical antes de ejecutar el núcleo.\n"
               "\n"
               "Puede reducir el retraso a costa de \n"
               "aumentar la probabilidad de perder \n"
               "fluidez.\n"
               " \n"
               "El valor máximo es 15.");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
         snprintf(s, len,
               "Ajusta la cantidad de fotogramas \n"
               "que puede adelantar la CPU a la GPU \n"
               "al utilizar 'Sincronía estricta \n"
               "de CPU'.\n"
               " \n"
               "El valor máximo es 3.\n"
               " \n"
               " 0: Sincroniza inmediatamente a GPU.\n"
               " 1: Sincroniza con el fotograma anterior.\n"
               " 2: Etc...");
         break;
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(s, len,
               "Introduce un fotograma negro \n"
               "entre fotogramas.\n"
               " \n"
               "Es útil para los monitores que \n"
               "funcionan a 120 Hz y quieren jugar \n"
               "a material a 60 Hz eliminando el \n"
               "ghosting.\n"
               " \n"
               "La frecuencia de actualización de vídeo \n"
               "aún debe configurarse como si utilizaras \n"
               "un monitor de 60 Hz (divide la frecuencia \n"
               "de actualización entre 2).");
         break;
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Muestra la pantalla de inicio en el menú.\n"
               "Se desactiva automáticamente cuando se \n"
               "ha visto por primera vez.\n"
               " \n"
               "Esta opción solo se actualiza dentro \n"
               "de la configuración si 'Guardar \n"
               "configuración al salir' está activado.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len,
                "Activa o desactiva el modo \n"
                "pantalla completa.");
         break;
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Impide que la SaveRAM sea sobrescrita \n"
               "mientras se carga un guardado rápido.\n"
               " \n"
               "Podría provocar defectos en los \n"
               "juegos.");
         break;
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pausa el juego cuando la ventana \n"
               "pasa a estar en segundo plano.");
         break;
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Si es posible, las capturas de \n"
               "pantalla se realizan del material \n"
               "pos-shaders de GPU.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Carpeta de capturas de pantalla. \n"
               " \n"
               "En esta carpeta se guardarán \n"
               "las capturas de pantalla."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "Intervalo de cambio de la \n"
               "sincronía vertical.\n"
               " \n"
               "Utiliza un intervalo personalizado para \n"
               "la sincronía vertical. Utiliza este \n"
               "parámetro para reducir a la mitad \n"
               "la frecuencia de actualización \n"
               "del monitor.");
         break;
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Carpeta de partidas guardadas. \n"
               " \n"
               "Almacena todas las partidas guardadas \n"
               "(*.srm) en esta carpeta. Incluye los \n"
               "archivos relacionados, como .bsv, .rt, \n"
               ".psrm, etcétera.\n"
               " \n"
               "Esta carpeta será ignorada si se \n"
               "utilizan explícitamente las opciones por \n"
               "línea de comandos.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Carpeta de guardados rápidos. \n"
               " \n"
               "Almacena todos los guardados rápidos \n"
               "(*.state) en esta carpeta.\n"
               " \n"
               "Esta carpeta será ignorada si se \n"
               "utilizan explícitamente las opciones por \n"
               "línea de comandos.");
         break;
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Carpeta de recursos. \n"
               " \n"
               "Esta ubicación es donde las interfaces \n"
               "de menús buscan de forma predeterminada \n"
               "los recursos a cargar, etc.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Carpeta de fondos dinámicos de pantalla. \n"
               " \n"
               "Aquí se almacenan los fondos de pantalla \n"
               "que el menú cargará de forma dinámica \n"
               "según el contexto.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Cantidad de velocidad reducida."
               " \n"
               "Al reducir la velocidad, el contenido \n"
               "se ralentizará según este factor.");
         break;
      case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
         snprintf(s, len,
               "Define el margen de los ejes.\n"
               " \n"
               "Indica la distancia mínima que debe \n"
               "recorrer un eje para que provoque \n"
               "una pulsación del botón.\n"
               "Los valores posibles son [0.0, 1.0].");
         break;
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len,
               "Período de turbo.\n"
               " \n"
               "Describe la velocidad con la que se \n"
               "pulsan los botones en el modo turbo."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Activa la autoconfiguración de \n"
               "entrada. \n"
               " \n"
               "Intentará configurar de forma \n"
               "automática los mandos que se \n"
               "conecten en cualquier momento, \n"
               "como el sistema Plug-and-play.");
         break;
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Permite o no el acceso a la cámara \n"
               "por parte de los núcleos.");
         break;
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Permite o no los servicios de \n"
               "localización por parte de los núcleos.");
         break;
      case MENU_ENUM_LABEL_TURBO:
         snprintf(s, len,
               "Permite usar el turbo.\n"
               " \n"
               "Si mantienes pulsado el botón de turbo \n"
               "mientras pulsas otro botón, este último \n"
               "entrará en un modo turbo en el que el \n"
               "estado del botón se modula mediante una \n"
               "señal periódica. \n"
               " \n"
               "La modulación se detiene cuando se deja \n"
               "de pulsar el botón original (no el botón \n"
               "de turbo).");
         break;
      case MENU_ENUM_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Activa o desactiva el teclado \n"
               "en pantalla.");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Silencia o no el sonido.");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "Mantén pulsado este botón para rebobinar.\n"
               " \n"
               "Para que este botón funcione, debes tener \n"
               "activada la opción de rebobinar.");
         break;
      case MENU_ENUM_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Asigna una tecla para abandonar \n"
               "RetroArch limpiamente."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nSi cierras el programa con \n"
               "cualquier forma brusca (SIGKILL, \n"
               "etc.) no se guardará la RAM, etc. \n"
               "En sistemas Unix, SIGINT/SIGTERM \n"
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
      case MENU_ENUM_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Cambia los usuarios en red.");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Aumenta el índice de trucos.\n");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Disminuye el índice de trucos.\n");
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
               "Reinicia el contenido.\n");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Alterna el estado en pausa.");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Alterna el índice de trucos.\n");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Mantén pulsado este botón para avanzar \n"
               "rápidamente. Suéltalo para desactivar \n"
               "esta función.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Mantén pulsado este botón para \n"
               "ir a cámara lenta.");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Avanza un fotograma mientras \n"
               "el contenido esté en pausa.");
         break;
      case MENU_ENUM_LABEL_MOVIE_RECORD_TOGGLE:
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
               "El eje de un joystick analógico \n"
               "(estilo DualShock).\n"
               " \n"
               "Se asigna como siempre, sin embargo, si se \n"
               "asigna un eje analógico real, puede leerse \n"
               "de forma analógica.\n"
               " \n"
               "El eje X positivo es hacia la derecha. \n"
               "El eje Y positivo es hacia abajo.");
         break;
      case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
         snprintf(s, len,
               "RetroArch, por si solo, no hace nada. \n"
               " \n"
               "Para que haga algo necesitas cargar \n"
               "un programa en él. \n"
               "\n"
               "Llamamos a estos programas 'núcleos \n"
               "libretro', o 'núcleos' para abreviar. \n"
               " \n"
               "Para cargar un núcleo, selecciona uno \n"
               "en 'Cargar núcleo'. \n"
               " \n"
#ifdef HAVE_NETWORKING
               "Puedes conseguir núcleos de varias formas: \n"
               "* Descárgalos en:\n"
               "'%s' -> '%s'.\n"
               "* Cópialos manualmente a:\n"
               "'%s'.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
               "Puedes conseguir núcleos si los\n"
               "trasladas a mano a la carpeta\n"
               "'%s'.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
         snprintf(s, len,
               "Puedes cambiar la superposición del \n"
               "mando virtual si vas a '%s' \n"
               "-> '%s'."
               " \n"
               "Desde ahí puedes cambiar la superposición, \n"
               "el tamaño y opacidad de sus botones, etc.\n"
               " \n"
               "NOTA: Las superposiciones de mandos \n"
               "virtuales están ocultas de forma \n"
               "predeterminada si estás dentro del menú. \n"
               "Si quieres cambiar este comportamiento, \n"
               "cambia '%s' a Desactivado/false.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
               );
         break;
      case MSG_UNKNOWN:
      default:
         if (s[0] == '\0')
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

const char *msg_hash_to_str_es(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "msg_hash_es.h"
      default:
         break;
   }

   return "null";
}

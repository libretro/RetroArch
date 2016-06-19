/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../msg_hash.h"
#include <stdint.h>
#include <string.h>

#include <retro_assert.h>
#include <compat/strl.h>

#include "../../configuration.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("áÁâãçéêíÍóõú")==12+1 ? 1 : -1];

const char *msg_hash_to_str_pt(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MSG_UNKNOWN:
         return "Desconhecido";
      case MSG_RECEIVED:
         return "recebido";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Comando desconhecido";
      case MSG_SENDING_COMMAND:
         return "Enviando comando";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Índice de disco inválido.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Falha ao remover disco da bandeja.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Disco removido da bandeja.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "bandeja de disco virtual.";
      case MSG_FAILED_TO:
         return "Falha ao";
      case MSG_TO:
         return "para";
      case MSG_SAVING_RAM_TYPE:
         return "Salvando tipo de RAM";
      case MSG_SAVING_STATE:
         return "Salvando estado";
      case MSG_LOADING_STATE:
         return "Carregando estado";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Falha ao carregar vídeo";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Falha ao carregar conteúdo";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Incapaz de ler arquivo de conteúdo";
      case MSG_GRAB_MOUSE_STATE:
         return "Obter estado do mouse";
      case MSG_PAUSED:
         return "Pausado.";
      case MSG_UNPAUSED:
         return "Despausado.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Falha ao carregar overlay.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Falha ao desativar mudo.";
      case MSG_AUDIO_MUTED:
         return "Áudio mudo.";
      case MSG_AUDIO_UNMUTED:
         return "Áudio normal.";
      case MSG_RESET:
         return "Reiniciar";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Falha ao carregar estado de";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Falha ao salvar estado em";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Falha ao salvar SRAM";
      case MSG_STATE_SIZE:
         return "Tamanho do estado";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Bloqueando Sobrescrição de SRAM";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "O core não suporta savestates.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Estado salvo no slot";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Salvo com sucesso em";
      case MSG_BYTES:
         return "bytes";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Diretório de configurações não definido. Incapaz de salvar.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Ignorando carregamento de SRAM.";
      case MSG_APPENDED_DISK:
         return "Disco anexado";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Iniciando reprodução de vídeo.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Falha ao remover arquivo temporário";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Removendo conteúdo temporário";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Estado carregado do slot";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Incapaz de processar arquivo ZIP.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Exame de diretório concluído";
      case MSG_SCANNING:
         return "Examinando";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Redirecionando cheat para";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirecionando save para";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirecionando savestate para";
      case MSG_SHADER:
         return "Shader";
      case MSG_APPLYING_SHADER:
         return "Aplicando shader";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Falha ao aplicar shader.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Iniciando gravação de vídeo em";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Falha ao iniciar gravação de vídeo.";
      case MSG_STATE_SLOT:
         return "Slot de estado";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Reiniciando gravação devido a reinício de driver.";
      case MSG_SLOW_MOTION:
         return "Câmera lenta.";
      case MSG_SLOW_MOTION_REWIND:
         return "Retrocesso em câmera lenta.";
      case MSG_REWINDING:
         return "Retrocedendo.";
      case MSG_REWIND_REACHED_END:
         return "Final do buffer de retrocesso atingido.";
      case MSG_TASK_FAILED:
         return "Falhou";
      case MSG_DOWNLOADING:
         return "Baixando";
      case MSG_EXTRACTING:
         return "Extraindo";
      case MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD:
         return "Iniciar RetroPad Remoto";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Escanear este Diretório>";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "Escanear Arquivo";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "Escanear Diretório";
      case MENU_ENUM_LABEL_VALUE_START_CORE:
         return "Iniciar Core";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "Informação";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Usar Player Interno"; /* TODO/FIXME */
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menu Rápido";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Carregar Conteúdo";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "Carregar Arquivo";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "Abrir Arquivo";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "Ask";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Configurações de Privacidade";
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu"; /* FIXME - don't edit this yet. */
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nenhuma definição encontrada.";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Nenhum medidor de desempenho.";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "Drivers";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configurações";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "Vídeo";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "Registro de Dados";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "Saves";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "Retrocesso";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "Cheat";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "Usuário";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Ativar Sistema BGM";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroTeclado";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Quadros de Blocos de Áudio";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Mostrar Rótulos de Entradas de Core";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Esconder Descritores de Entradas sem Uso";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostrar Mensagem de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Fonte da Mensagem de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Tamanho da Mensagem de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posição X da Mensagem de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posição Y da Mensagem de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Ativar Filtro de Suavização";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro de Cintilação";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<Diretório de Conteúdo>";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "Desconhecido";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "Tanto faz";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "Linear";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "Nearest";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<Padrão>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<Nenhum>";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Remapeamentos de Controladores";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Autoconfigurações de Dispositivos de Entrada";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Configurações de Gravações";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Gravações";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Capturas de Telas";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Históricos";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Saves";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestates";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandos stdin";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver de Vídeo";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "Ativar Gravação";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Ativar Gravação por GPU";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH: /* FIXME/UPDATE */
         return "Caminho da Gravação";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Diretório de Saída";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "Configurações de Gravação";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Ativar Filtro Pós-Gravação";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Recursos (Assets) de Cores"; /* FIXME/UPDATE */
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Recursos (Assets)";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Papéis de Parede Dinâmicos";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Navegação";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Configurações";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Informações de Cores";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Cores";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Cursores";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Databases de Conteúdo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System/BIOS";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Cheats";
      case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY: /* UPDATE/FIXME */
         return "Descompactação";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Filtros de Áudio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Shaders";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Filtros de Vídeo";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlays";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Overlays de Teclado";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Trocar Entradas de Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Ativar Espectador de Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Endereço IP";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Portas TCP/UDP de Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "Ativar Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Quadros de Retardo de Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "Ativar Cliente de Netplay";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostrar Tela de Início";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "Cor do Menu Título";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Cor de Realce do Menu Inicial";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostrar Hora / Data";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Ativar Runloop de Threads de Dados";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Cor do Menu Inicial";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostrar Configurações Avançadas";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "Suporte a Mouse";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "Suporte a Touch";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "Mostrar Nome dos Cores";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Ativar Sobreposição de DPI";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Sobreposição de DPI";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspender Proteção de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Desativar Desktop Composition";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Não Rodar em Background";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Ativar UI Companion ao Iniciar";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra de Menu (Dica)";
      case MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE:
         return "Ação para Arquivos Compactados";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandos de Rede";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta para Comandos de Rede";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Ativar Lista de Histórico";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Tamanho da Lista de Histórico";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Taxa de Atualização de Quadros Estimada";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Desligar Core Dummy On";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Não Iniciar Cores Automaticamente";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limitar Velocidade Máxima de Execução";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocidade Máxima de Execução";
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Carregar Automaticamente Arquivos Remapeados";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Taxa de Câmera Lenta";
      case MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuração por Core";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Carregar Automaticamente Arquivos de Sobreposição";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Salvar Configuração ao Sair";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtragem Bilinear por Hardware";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma de Vídeo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permitir Rotação";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronizar GPU com CPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervalo de Permuta do Sincronismo Vertical";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "Sincronismo Vertical";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "Vídeo em Threads";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotação";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Ativar Captura de Tela via GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Descartar Overscan (Recarregue)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Índice de Relações de Aspecto";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Relação de Aspecto Automática";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forçar Relação de Aspecto";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Taxa de Atualização de Tela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forcar Desativação de sRGB FBO";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Modo Tela Cheia em Janela";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "Usar Modo PAL60";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Eliminar Cintilação";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Definir Largura de Tela VI";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserção de Quadro Negro";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Quadros de Sincronização entre GPU e CPU";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordenar Saves em Pastas";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordenar Savestates em Pastas";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Usar Modo Tela Cheia";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "Variar Escala em Janela";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Escala em Degraus Inteiros";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contadores de Desempenho";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Nível de Registro de Core";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "Detalhamento de Registro";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Carregar Savestate Automaticamente";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Índice Automático de Savestates";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Savestate Automático";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervalo de Gravação Automática de SaveRAM";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Não Sobrescrever SaveRAM ao Carregar Savestate";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Ativar Contexto Compartilhado de Hardware";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "Reiniciar RetroArch";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nome de Usuário";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "Idioma";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "Autorizar Câmera";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "Autorizar Localização";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pausar Quando o Menu for Ativado";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostrar Overlay de Teclado";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostrar Overlay";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Índice de Monitores";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Retardo de Quadro";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo de Trabalho";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Período de Turbo";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Limiar de Eixo do Controlador";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Ativar Remapeamentos";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "Usuários Máximos";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Ativar Autoconfiguração";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Taxa de Amostragem de Áudio (KHz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Distorção Máxima de Sincronização de Áudio";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Códigos de Cheat";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Salvar Remapeamento de Core";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Salvar Remapeamento de Jogo";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Aplicar Alterações de Cheats";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Aplicar Alterações de Shaders";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "Ativar Retrocesso";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Selecionar da Coleção";
      case MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST:
         return "Selecionar Arquivo e Detectar Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Selecionar do Histórico";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "Ativar Áudio";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "Mostrar Taxa de Quadros";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "Silenciar Áudio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume de Áudio (dB)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "Ativar Sincronismo de Áudio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Variação Máxima de Taxa de Áudio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Número de Shaders";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "Carregar Configuração";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "Granularidade do Retrocesso";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carregar Remapeamento";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "Relação de Aspecto Personalizada";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usar este diretório>";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Iniciar Conteúdo";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS:    /* UPDATE/FIXME */
         return "Opções de Disco do Core";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "Opções";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Cheats";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carregar Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salvar Cheat Como";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "Contadores de Core";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturar Tela";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "Retomar";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "Índice de Discos";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contadores do Frontend";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Anexar Imagem de Disco";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Estado do Drive de Disco";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Histórico vazio.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nenhuma informação de core disponível.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nenhuma opção de core disponível.";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nenhum core disponível.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "Nenhum Core";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "Gerenciador de Databases";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "Gerenciador de Cursores";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU: /* TODO/FIXME - translate */
         return "Main Menu";
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "Definições";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "Sair do RetroArch";
      case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         return "Desligar";
      case MENU_ENUM_LABEL_VALUE_REBOOT:
         return "Reiniciar";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "Ajuda";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Salvar Nova Configuração";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "Reiniciar";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Atualização de Cores";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL Buildbot de Cores";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL Buildbot de Recursos (Assets)";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navegação Circular";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtrar por Extensões Suportadas";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Autoextrair Arquivos Baixados";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informação de Sistema";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "Atualização Online";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "Informação de Core";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Diretório não encontrado.";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "Nenhum item.";
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "Carregar Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT:
         return "Selecionar Arquivo";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "Fechar";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "Databases";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "Salvar Savestate";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "Carregar Savestate";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "Retomar";
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "Driver de Controlador";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver de Áudio";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver de Joypad";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver do Amostrador de Áudio";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "Driver de Gravação";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "Driver de Menu";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver de Câmera";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver de Localização";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapaz de ler arquivo compactado.";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "Interpolação de Overlay";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "Predefinições de Overlay";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "Latência de Áudio (ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "Dispositivo de Áudio";
      case MENU_ENUM_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Predefinições de Overlay de Teclado";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacidade de Overlay";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "Papel de Parede do Menu";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Papel de Parede Dinâmico";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS: /* UPDATE/FIXME */
         return "Opções de Remapeamento de Controlador de Core";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS:
         return "Shaders";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Parâmetros de Shader em Uso";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menu de Parâmetros de Shader";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salvar Predefinições de Shader Como";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nenhum parâmetro de shader disponível.";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carregar Predefinições de Shader";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "Filtro de Vídeo";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin de DSP de Áudio";
      case MENU_ENUM_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Iniciando download: ";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "segundos";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "OFF";
      case MENU_ENUM_LABEL_VALUE_ON:
         return "ON";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "Atualizar Recursos (Assets)";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "Atualizar Cheats";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Atualizar Perfis de Autoconfiguração";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "Atualizar Databases";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Atualizar Overlays";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Atualizar Shaders Cg";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Atualizar Shaders GLSL";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nome do core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Rótulo do core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nome do sistema";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Fabricante do sistema";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categorias";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autores";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permissões";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licença(s)";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Extensões suportadas";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Notas do core";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data do build";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Versão do git";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Atributos da CPU";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Indentificador do frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nome do frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "OS do frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Nível de RetroRating";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Fonte de energia";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Nenhuma fonte";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Carregando";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Carregado";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Descarregando";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Driver de contexto de vídeo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Mostrar largura (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Mostrar altura (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Mostrar DPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Suporte a LibretroDB";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Suporte a Overlay";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Suporte a interface de comandos";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Suporte a interface de comandos de rede";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Suporte a Cocoa";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Suporte a PNG (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Suporte a SDL1.2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Suporte a SDL2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Suporte a OpenGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Suporte a OpenGL ES";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Suporte a Threading";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Suporte a KMS/EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Suporte a Udev";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Suporte a OpenVG";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Suporte a EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Suporte a X11";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Suporte a Wayland";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Suporte a XVideo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Suporte a ALSA";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Suporte a OSS";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Suporte a OpenAL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Suporte a OpenSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Suporte a RSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Suporte a RoarAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Suporte a JACK";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Suporte a PulseAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Suporte a DirectSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Suporte a XAudio2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Suporte a Zlib";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Suporte a 7zip";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Suporte a bibliotecas dinâmicas";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Suporte a Cg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Suporte a GLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Suporte a HLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Suporte a análise XML libxml2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Suporte a imagem SDL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Suporte a OpenGL/Direct3D render-to-texture (multi-pass shaders)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Suporte a FFmpeg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Suporte a CoreText";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Suporte a FreeType";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Suporte a Netplay (peer-to-peer)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Suporte a Python (script em shaders)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Suporte a Video4Linux2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Suporte a Libusb";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "Sim";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "Não";
      case MENU_ENUM_LABEL_VALUE_BACK:
         return "VOLTAR";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Resolução de Tela";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "Desativado";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "Porta";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "Nenhum";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Desenvolvedor";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Editora";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Descrição";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nome";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origem";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franquia";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Mês de Lançamento";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Ano de Lançamento";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "Verdadeiro";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "Falso";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "Faltando";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "Presente";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "Opcional";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "Obrigatório";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "Status";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "Áudio";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "Entradas";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Informações de Tela";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Overlay em Tela";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "Menu";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Reprodução de Mídia"; /* UPDATE/FIXME */
      case MENU_ENUM_LABEL_VALUE_UI_SETTINGS:
         return "Interface de Usuário";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu de Navegação";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Atualização de Core"; /* UPDATE/FIXME */
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "Rede";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Histórico";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "Usuário";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Diretórios";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "Gravação";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Nenhuma informação disponível.";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "Usuário %u";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "Inglês";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "Japonês";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "Francês";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "Espanhol";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "Alemão";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "Italiano";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "Holandês";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE:
         return "Português";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "Russo";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "Coreano";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Chinês (Tradicional)";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Chinês (Simplificado)";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "Analógico Esquerdo";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "Analógico Direito";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Associação de Teclas de Atalho";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Definições do Limitador de Quadros";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "Busca:";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Usar Visualizador de Imagens Interno";
      default:
         break;
   }

   return "null";
}

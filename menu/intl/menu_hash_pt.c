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

#include <stdint.h>
#include <string.h>

#include <retro_assert.h>
#include <compat/strl.h>

#include "../menu_hash.h"
#include "../../configuration.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("áÁâãçéêíÍóõú")==12+1 ? 1 : -1];

const char *menu_hash_to_str_pt(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Informação";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Usar Player Interno"; /* TODO/FIXME */
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menu Rápido";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Carregar Conteúdo";
      case MENU_LABEL_VALUE_LOAD_ARCHIVE:
         return "Carregar Arquivo";
      case MENU_LABEL_VALUE_OPEN_ARCHIVE:
         return "Abrir Arquivo";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Ask";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Configurações de Privacidade";
      case MENU_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu"; /* FIXME - don't edit this yet. */
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nenhuma definição encontrada.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Nenhum medidor de desempenho.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Drivers";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configurações";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Vídeo";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Registro de Dados";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Saves";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Retrocesso";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Cheat";
      case MENU_VALUE_USER:
         return "Usuário";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Ativar Sistema BGM";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroTeclado";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Quadros de Blocos de Áudio";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Mostrar Rótulos de Entradas de Core";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Esconder Descritores de Entradas sem Uso";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostrar Mensagem de Tela";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Fonte da Mensagem de Tela";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Tamanho da Mensagem de Tela";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posição X da Mensagem de Tela";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posição Y da Mensagem de Tela";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Ativar Filtro de Suavização";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro de Cintilação";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Diretório de Conteúdo>";
      case MENU_VALUE_UNKNOWN:
         return "Desconhecido";
      case MENU_VALUE_DONT_CARE:
         return "Tanto faz";
      case MENU_VALUE_LINEAR:
         return "Linear";
      case MENU_VALUE_NEAREST:
         return "Nearest";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Padrão>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Nenhum>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Remapeamentos de Controladores";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Autoconfigurações de Dispositivos de Entrada";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Configurações de Gravações";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Gravações";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Capturas de Telas";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Históricos";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Saves";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestates";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandos stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver de Vídeo";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Ativar Gravação";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Ativar Gravação por GPU";
      case MENU_LABEL_VALUE_RECORD_PATH: /* FIXME/UPDATE */
         return "Caminho da Gravação";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Diretório de Saída";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configurações de Gravação";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Ativar Filtro Pós-Gravação";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Recursos (Assets) de Cores"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Recursos (Assets)";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Papéis de Parede Dinâmicos";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Navegação";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Configurações";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Informações de Cores";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Cores";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Cursores";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Databases de Conteúdo";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System/BIOS";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Cheats";
      case MENU_LABEL_VALUE_CACHE_DIRECTORY: /* UPDATE/FIXME */
         return "Descompactação";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Filtros de Áudio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Shaders";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Filtros de Vídeo";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlays";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Overlays de Teclado";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Trocar Entradas de Netplay";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Ativar Espectador de Netplay";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Endereço IP";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Portas TCP/UDP de Netplay";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Ativar Netplay";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Quadros de Retardo de Netplay";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Ativar Cliente de Netplay";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostrar Tela de Início";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Cor do Menu Título";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Cor de Realce do Menu Inicial";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostrar Hora / Data";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Ativar Runloop de Threads de Dados";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Cor do Menu Inicial";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostrar Configurações Avançadas";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Suporte a Mouse";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Suporte a Touch";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Mostrar Nome dos Cores";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Ativar Sobreposição de DPI";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Sobreposição de DPI";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspender Proteção de Tela";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Desativar Desktop Composition";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Não Rodar em Background";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Ativar UI Companion ao Iniciar";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra de Menu (Dica)";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Ação para Arquivos Compactados";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandos de Rede";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta para Comandos de Rede";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Ativar Lista de Histórico";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Tamanho da Lista de Histórico";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Taxa de Atualização de Quadros Estimada";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Desligar Core Dummy On";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Não Iniciar Cores Automaticamente";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limitar Velocidade Máxima de Execução";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocidade Máxima de Execução";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Carregar Automaticamente Arquivos Remapeados";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Taxa de Câmera Lenta";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuração por Core";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Carregar Automaticamente Arquivos de Sobreposição";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Salvar Configuração ao Sair";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtragem Bilinear por Hardware";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma de Vídeo";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permitir Rotação";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronizar GPU com CPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervalo de Permuta do Sincronismo Vertical";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Sincronismo Vertical";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Vídeo em Threads";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotação";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Ativar Captura de Tela via GPU";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Descartar Overscan (Recarregue)";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Índice de Relações de Aspecto";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Relação de Aspecto Automática";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forçar Relação de Aspecto";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Taxa de Atualização de Tela";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forcar Desativação de sRGB FBO";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Modo Tela Cheia em Janela";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Usar Modo PAL60";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Eliminar Cintilação";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Definir Largura de Tela VI";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserção de Quadro Negro";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Quadros de Sincronização entre GPU e CPU";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordenar Saves em Pastas";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordenar Savestates em Pastas";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Usar Modo Tela Cheia";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Variar Escala em Janela";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Escala em Degraus Inteiros";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contadores de Desempenho";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Nível de Registro de Core";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Detalhamento de Registro";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Carregar Savestate Automaticamente";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Índice Automático de Savestates";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Savestate Automático";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervalo de Gravação Automática de SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Não Sobrescrever SaveRAM ao Carregar Savestate";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Ativar Contexto Compartilhado de Hardware";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Reiniciar RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nome de Usuário";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Idioma";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Autorizar Câmera";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Autorizar Localização";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pausar Quando o Menu for Ativado";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostrar Overlay de Teclado";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostrar Overlay";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Índice de Monitores";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Retardo de Quadro";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo de Trabalho";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Período de Turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Limiar de Eixo do Controlador";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Ativar Remapeamentos";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Usuários Máximos";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Ativar Autoconfiguração";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Taxa de Amostragem de Áudio (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Distorção Máxima de Sincronização de Áudio";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Códigos de Cheat";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Salvar Remapeamento de Core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Salvar Remapeamento de Jogo";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Aplicar Alterações de Cheats";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Aplicar Alterações de Shaders";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Ativar Retrocesso";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Selecionar da Coleção";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Selecionar Arquivo e Detectar Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Selecionar do Histórico";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Ativar Áudio";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Mostrar Taxa de Quadros";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Silenciar Áudio";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume de Áudio (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Ativar Sincronismo de Áudio";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Variação Máxima de Taxa de Áudio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Número de Shaders";
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Carregar Configuração";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Granularidade do Retrocesso";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carregar Remapeamento";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Relação de Aspecto Personalizada";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usar este diretório>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Iniciar Conteúdo";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opções de Disco do Core";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opções do Core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opções de Cheat do Core";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carregar Cheat";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salvar Cheat Como";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Contadores de Core";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturar Tela";
      case MENU_LABEL_VALUE_RESUME:
         return "Retomar";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Índice de Discos";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contadores do Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Anexar Imagem de Disco";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Estado do Drive de Disco";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Histórico vazio.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nenhuma informação de core disponível.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nenhuma opção de core disponível.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nenhum core disponível.";
      case MENU_VALUE_NO_CORE:
         return "Nenhum Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gerenciador de Databases";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gerenciador de Cursores";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Definições";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Sair do RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Ajuda";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Salvar Nova Configuração";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Reiniciar Conteúdo";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Atualização de Cores";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL Buildbot de Cores";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL Buildbot de Recursos (Assets)";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navegação Circular";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtrar por Extensões Suportadas";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Autoextrair Arquivos Baixados";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informação de Sistema";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Atualização Online";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informação de Core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Diretório não encontrado.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Nenhum item.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Carregar Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Selecionar Arquivo";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Fechar Conteúdo";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Databases";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Salvar Savestate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Carregar Savestate";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Retomar Conteúdo";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Driver de Controlador";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver de Áudio";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver de Joypad";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver do Amostrador de Áudio";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Driver de Gravação";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Driver de Menu";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver de Câmera";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver de Localização";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapaz de ler arquivo compactado.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Interpolação de Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Predefinições de Overlay";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Latência de Áudio (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Dispositivo de Áudio";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Predefinições de Overlay de Teclado";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacidade de Overlay";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Papel de Parede do Menu";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Papel de Parede Dinâmico";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS: /* TODO/FIXME */
         return "Opções de Remapeamento de Controlador de Core";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opções de Shaders";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Parâmetros de Shader em Uso";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menu de Parâmetros de Shader";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salvar Predefinições de Shader Como";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nenhum parâmetro de shader disponível.";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carregar Predefinições de Shader";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtro de Vídeo";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin de DSP de Áudio";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Iniciando download: ";
      case MENU_VALUE_SECONDS:
         return "segundos";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      case MENU_LABEL_VALUE_UPDATE_ASSETS:
         return "Atualizar Recursos (Assets)";
      case MENU_LABEL_VALUE_UPDATE_CHEATS:
         return "Atualizar Cheats";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Atualizar Perfis de Autoconfiguração";
      case MENU_LABEL_VALUE_UPDATE_DATABASES:
         return "Atualizar Databases";
      case MENU_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Atualizar Overlays";
      case MENU_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Atualizar Shaders Cg";
      case MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Atualizar Shaders GLSL";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nome do core";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Rótulo do core";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nome do sistema";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Fabricante do sistema";
      case MENU_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categorias";
      case MENU_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autores";
      case MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permissões";
      case MENU_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licença(s)";
      case MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Extensões suportadas";
      case MENU_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Notas do core";
      case MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data do build";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Versão do git";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Atributos da CPU";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Indentificador do frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nome do frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "OS do frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Nível de RetroRating";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Fonte de energia";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Nenhuma fonte";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Carregando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Carregado";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Descarregando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Driver de contexto de vídeo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Mostrar largura (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Mostrar altura (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Mostrar DPI";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Suporte a LibretroDB";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Suporte a Overlay";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Suporte a interface de comandos";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Suporte a interface de comandos de rede";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Suporte a Cocoa";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Suporte a PNG (RPNG)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Suporte a SDL1.2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Suporte a SDL2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Suporte a OpenGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Suporte a OpenGL ES";
      case MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Suporte a Threading";
      case MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Suporte a KMS/EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Suporte a Udev";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Suporte a OpenVG";
      case MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Suporte a EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Suporte a X11";
      case MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Suporte a Wayland";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Suporte a XVideo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Suporte a ALSA";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Suporte a OSS";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Suporte a OpenAL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Suporte a OpenSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Suporte a RSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Suporte a RoarAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Suporte a JACK";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Suporte a PulseAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Suporte a DirectSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Suporte a XAudio2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Suporte a Zlib";
      case MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Suporte a 7zip";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Suporte a bibliotecas dinâmicas";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Suporte a Cg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Suporte a GLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Suporte a HLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Suporte a análise XML libxml2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Suporte a imagem SDL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Suporte a OpenGL/Direct3D render-to-texture (multi-pass shaders)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Suporte a FFmpeg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Suporte a CoreText";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Suporte a FreeType";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Suporte a Netplay (peer-to-peer)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Suporte a Python (script em shaders)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Suporte a Video4Linux2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Suporte a Libusb";
      case MENU_LABEL_VALUE_YES:
         return "Sim";
      case MENU_LABEL_VALUE_NO:
         return "Não";
      case MENU_VALUE_BACK:
         return "VOLTAR";
      case MENU_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Resolução de Tela";
      case MENU_VALUE_DISABLED:
         return "Desativado";
      case MENU_VALUE_PORT:
         return "Porta";
      case MENU_VALUE_NONE:
         return "Nenhum";
      case MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Desenvolvedor";
      case MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Editora";
      case MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Descrição";
      case MENU_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nome";
      case MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origem";
      case MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franquia";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Mês de Lançamento";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Ano de Lançamento";
      case MENU_VALUE_TRUE:
         return "Verdadeiro";
      case MENU_VALUE_FALSE:
         return "Falso";
      case MENU_VALUE_MISSING:
         return "Faltando";
      case MENU_VALUE_PRESENT:
         return "Presente";
      case MENU_VALUE_OPTIONAL:
         return "Opcional";
      case MENU_VALUE_REQUIRED:
         return "Obrigatório";
      case MENU_VALUE_STATUS:
         return "Status";
      case MENU_LABEL_VALUE_AUDIO_SETTINGS:
         return "Áudio";
      case MENU_LABEL_VALUE_INPUT_SETTINGS:
         return "Entradas";
      case MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Informações de Tela";
      case MENU_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Overlay em Tela";
      case MENU_LABEL_VALUE_MENU_SETTINGS:
         return "Menu";
      case MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Reprodução de Mídia"; /* UPDATE/FIXME */
      case MENU_LABEL_VALUE_UI_SETTINGS:
         return "Interface de Usuário";
      case MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu de Navegação";
      case MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Atualização de Core"; /* UPDATE/FIXME */
      case MENU_LABEL_VALUE_NETWORK_SETTINGS:
         return "Rede";
      case MENU_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Histórico";
      case MENU_LABEL_VALUE_USER_SETTINGS:
         return "Usuário";
      case MENU_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Diretórios";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Gravação";
      case MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Nenhuma informação disponível.";
      case MENU_LABEL_VALUE_INPUT_USER_BINDS:
         return "Usuário %u";
      case MENU_VALUE_LANG_ENGLISH:
         return "Inglês";
      case MENU_VALUE_LANG_JAPANESE:
         return "Japonês";
      case MENU_VALUE_LANG_FRENCH:
         return "Francês";
      case MENU_VALUE_LANG_SPANISH:
         return "Espanhol";
      case MENU_VALUE_LANG_GERMAN:
         return "Alemão";
      case MENU_VALUE_LANG_ITALIAN:
         return "Italiano";
      case MENU_VALUE_LANG_DUTCH:
         return "Holandês";
      case MENU_VALUE_LANG_PORTUGUESE:
         return "Português";
      case MENU_VALUE_LANG_RUSSIAN:
         return "Russo";
      case MENU_VALUE_LANG_KOREAN:
         return "Coreano";
      case MENU_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Chinês (Tradicional)";
      case MENU_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Chinês (Simplificado)";
      case MENU_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_VALUE_LEFT_ANALOG:
         return "Analógico Esquerdo";
      case MENU_VALUE_RIGHT_ANALOG:
         return "Analógico Direito";
      case MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Associação de Teclas de Atalho";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Definições do Limitador de Quadros";
      case MENU_VALUE_SEARCH:
         return "Busca:";
      case MENU_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Usar Visualizador de Imagens Interno";
      default:
         break;
   }
 
   return "null";
}

int menu_hash_get_help_pt(uint32_t hash, char *s, size_t len)
{
   uint32_t driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   /* If this one throws errors, stop sledgehammering square pegs into round holes and */
   /* READ THE COMMENTS at the top of the file. */ (void)sizeof(force_iso_8859_1);

   switch (hash)
   {
      case MENU_LABEL_INPUT_DRIVER:
         driver_hash = menu_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               snprintf(s, len,
                     "Driver de entrada udev. \n"
                     " \n"
                     "Esse driver funciona sem o X. \n"
                     " \n"
                     "Ele usa a recente API de joypads \n"
                     "evdev para supporte a joystick. \n"
                     "Suporta Hot-Swap e force feedback \n"
                     "(se suportado pelo dispositivo). \n"
                     " \n"
                     "O driver lê os eventos evdev para suporte a \n"
                     "teclado. Suporta também callback de teclado, \n"
                     "mouses e touchpads. \n"
                     " \n"
                     "Em geral, na maioria das distribuições, os nós \n"
                     "/dev/input são root-only (modo 600). Mas você pode \n"
                     "definir uma regra udev para dar acesso a non-roots."
                     );
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "Driver de Entrada linuxraw. \n"
                     " \n"
                     "Esse driver requer um TTY ativo. Eventos de \n"
                     "teclado são lidos diretamente do TTY, tornando-o \n"
                     "simples, mas não tão flexível quanto o udev. \n" "Mouses, etc, não são suportados. \n"
                     " \n"
                     "Esse driver usa a antiga API de joysticks \n"
                     "(/dev/input/js*).");
               break;
            default:
               snprintf(s, len,
                     "Driver de Entrada.\n"
                     " \n"
                     "Dependendo do driver de vídeo, pode ser necessário \n"
                     "forçar um driver de entrada diferente.");
               break;
         }
         break;
      case MENU_LABEL_LOAD_CONTENT:
         snprintf(s, len,
               "Carregar Conteúdo. \n"
               "Busca conteúdo. \n"
               " \n"
               "Para carregar conteúdo, você precisa de \n"
               "um core libretro para usar, e um arquivo \n"
               "de conteúdo. \n"
               " \n"
               "Para controlar onde o menu começa a \n"
               "buscar conteúdo, defina o Diretório \n"
               "de Navegação. Se não estiver definido, \n"
               "o Retroarch começará no diretório raiz. \n"
               " \n"
               "O navegador vai filtrar pelas extensões \n"
               "do mais recente core definido em 'Core', \n"
               "e o usará quando o conteúdo estiver \n"
               "carregado."
               );
         break;
      case MENU_LABEL_CORE_LIST:
         snprintf(s, len,
               "Carregar Core. \n"
               " \n"
               "Busca uma implementação de um core \n"
               "libretro. Onde a busca inicia depende \n"
               "do caminho do seu Diretório de Cores. \n"
               "Se não definido, começará no raiz. \n"
               " \n"
               "Se o Diretório de Cores estiver definido, \n"
               "o menu o usará como pasta inicial. Se for um \n"
               "caminho completo, ele começará na pasta onde \n"
               "o arquivo estiver.");
         break;
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "Carregando conteúdo do histórico. \n"
               " \n"
               "Ao carregar conteúdos, suas combinações com \n"
               "cores são salvas no histórico. \n"
               " \n"
               "O histórico é salvo em um arquivo no mesmo \n"
               "diretório do arquivo de configuração. Se nenhuma \n"
               "configuração tiver sido carregada, o histórico \n"
               "não será salvo ou carregado e não vai existir no \n"
               "menu principal."
               );
         break;
      case MENU_LABEL_VIDEO_DRIVER:
         driver_hash = menu_hash_calculate(settings->video.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_VIDEO_DRIVER_GL:
               snprintf(s, len,
                     "Driver de Vídeo OpenGL. \n"
                     " \n"
                     "Esse driver permite o uso de cores libretro GL  \n"
                     "em adição às implementações de cores de \n"
                     "renderização por software.\n"
                     " \n"
                     "O desempenho das implementações dos cores de\n"
                     "renderização por software e libretro GL \n"
                     "depende do driver GL instalado em sua \n"
                     "placa de vídeo.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL2:
               snprintf(s, len,
                     "Driver de Vídeo SDL 2.\n"
                     " \n"
                     "Esse é um driver de vídeo SDL 2 de \n"
                     "renderização por software.\n"
                     " \n"
                     "O desempenho das implementações dos cores de \n"
                     "renderização por software depende da \n"
                     "implementação SDL de sua plataforma.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL1:
               snprintf(s, len,
                     "Driver de Vídeo SDL.\n"
                     " \n"
                     "Esse é um driver de vídeo SDL 1.2 de \n"
                     "renderização por software.\n"
                     " \n"
                     "O desemprenho é considerado subótimo. \n"
                     "Considere seu uso apenas em último caso.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_D3D:
               snprintf(s, len,
                     "Driver de Vídeo Direct3D. \n"
                     " \n"
                     "O desempenho das implementações dos cores de\n"
                     "renderização por software depende do driver \n"
                     "D3D instalado em sua placa de vídeo.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_EXYNOS:
               snprintf(s, len,
                     "Driver de Vídeo Exynos-G2D. \n"
                     " \n"
                     "Esse é um driver de vídeo Exynos de baixo nível. \n"
                     "Usa o bloco G2D do SoC Samsung Exynos \n"
                     "para operações de blit. \n"
                     " \n"
                     "O desempenho para cores de renderização por \n"
                     "software deve ser ótimo.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SUNXI:
               snprintf(s, len,
                     "Driver de Vídeo Sunxi-G2D. \n"
                     " \n"
                     "Esse é um driver de vídeo Sunxi de baixo nível. \n"
                     "Usa o bloco G2D dos SoCs Allwinner.");
               break;
            default:
               snprintf(s, len,
                     "Driver de Vídeo em uso.");
               break;
         }
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin de DSP de Áudio.\n"
               "Processa áudio antes de ser enviado ao \n"
               "driver."
               );
         break;
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         driver_hash = menu_hash_calculate(settings->audio.resampler);

         switch (driver_hash)
         {
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
               snprintf(s, len,
                     "Implementação Windowed SINC.");
               break;
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
               snprintf(s, len,
                     "Implementação Convoluted Cosine.");
               break;
         }
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Carregar Predefinições de Shader. \n"
               " \n"
               " Carregar predefinições em "
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
               " diretamente. \n"
               "O menu de shaders é atualizado de acordo. \n"
               " \n"
               "Se o CGP usar métodos de interpolação complexos, \n"
               "(fator de escala diferente para X e Y) o fator \n"
               "de escala mostrado no menu poderá não ser \n"
               "correto."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "Escala para este passo. \n"
               " \n"
               "O fator de escala se acumula, i.e. 2x \n"
               "para o primeiro passo e 2x para o segundo \n"
               "vai lhe fornecer uma escala total de 4x. \n"
               " \n"
               "Se houver um fator de escala no último \n"
               "passo, o resultado será esticado na tela \n"
               "com o filtro especificado em 'Filtro \n"
               "Padrão'. \n"
               " \n"
               "Se 'Tanto faz' estiver definido, a escala \n"
               "de 1x ou o esticamento para tela cheia serão \n"
               "usados dependendo se o primeiro foi ou não \n"
               "definido no último passo."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         snprintf(s, len,
               "Número de Shaders. \n"
               " \n"
               "O RetroArch permite que você combine vários \n"
               "shaders com número arbitrário de passos, filtros \n"
               "de hardware e fatores de escala personalizados. \n"
               " \n"
               "Essa opção especifica o número de passos a usar. \n"
               "Se for definido como 0 e usada a opção Aplicar \n"
               "Alterações de Shaders, será usado um shader vazio. \n"
               " \n"
               "A opção Filtro Padrão irá afetar o filtro \n"
               "de esticamento.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Parâmetros de Shaders. \n"
               " \n"
               "Modifica o shader em uso diretamente. Não será \n"
               "salvo no arquivo de predefinições CGP/GLSLP.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Parâmetros de Predefinições de Shader. \n"
               " \n"
               "Modifica as predefinições de shader em uso no menu."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Caminho do shader. \n"
               " \n"
               "Todos os shaders devem ser do mesmo \n"
               "tipo (i.e. CG, GLSL ou HLSL). \n"
               " \n"
               "Defina o Diretório de Shaders para indicar \n"
               "onde o buscador começa a procurar pelos \n"
               "shaders."
               );
         break;
      case MENU_LABEL_CONFIG_SAVE_ON_EXIT:
         snprintf(s, len,
               "Salva configuração ao sair. Útil para\n"
               "o menu, pois as definições podem ser\n"
               "modificadas. Sobrescreve a configuração.\n"
               " \n"
               "#includes e comentários não são \n"
               "preservados. \n"
               " \n"
               "Por design, o arquivo de configuração \n"
               "é considerado imutável, pois ele é \n"
               "provavelmente mantido pelo usuário, \n"
               "e não deve ser sobrescrito sem o \n"
               "seu conhecimento."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nPorém, isso não funciona assim nos \n"
               "consoles, pois abrir o arquivo de \n"
               "configuração manualmente não é \n"
               "realmente uma opção."
#endif
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Filtro de hardware para este passo. \n"
               " \n"
               "Se 'Tanto faz' estiver definido, o 'Filtro \n"
               "Padrão' será usado."
               );
         break;
      case MENU_LABEL_AUTOSAVE_INTERVAL:
         snprintf(s, len,
               "Salva automaticamente a SRAM \n"
               "em intervalos regulares.\n"
               " \n"
               "Está desativado por padrão. O intervalo é \n"
               "medido em segundos. \n"
               " \n"
               "Um valor de 0 desativa o salvamento \n"
               "automático.");
         break;
      case MENU_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Tipo de Dispositivo de Entrada. \n"
               " \n"
               "Escolhe o dispositivo a usar. Isso é \n"
               "relevante para o core libretro."
               );
         break;
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
         snprintf(s, len,
               "Define o nível de registro para os cores \n"
               "(GET_LOG_INTERFACE). \n"
               " \n"
               " Se o nível de registro de um core \n"
               " libretro estiver abaixo de libretro_log, \n"
               " esse será ignorado.\n"
               " \n"
               " Registros DEBUG são sempre ignorados a menos \n"
               " que o modo prolixo seja ativado (--verbose).\n"
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
               "Slot de Savestates.\n"
               " \n"
               " Com o slot definido em 0, o nome do Savestate \n"
               " será *.state (ou o que estiver definido em commandline).\n"
               "Se diferente de 0, o nome será (caminho)(d), \n"
               "em que (d) é o número do slot.");
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "Aplicar Alterações de Shaders. \n"
               " \n"
               "Após alterar definições de shaders, use-o para \n"
               "aplicar as mudanças. \n"
               " \n"
               "Mudar definições de shaders é uma operação \n"
               "computacionalmente cara e deve ser \n"
               "realizada explicitamente. \n"
               " \n"
               "Quando se aplicam shaders, as definições do menu \n"
               "de shaders são salvas em um arquivo temporário \n"
               "(menu.cgp ou menu.glslp) e carregadas. O arquivo \n"
               "persiste ao fechar o RetroArch. Ele é salvo \n"
               "no Diretório de Shaders."
               );
         break;
      case MENU_LABEL_INPUT_BIND_DEVICE_ID:
         snprintf(s, len,
               "Dispositivo de Entrada. \n"
               " \n"
               "Escolhe um gamepad para o usuário N. \n"
               "O nome do pad está disponível."
               );
         break;
      case MENU_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Alterna menu.");
         break;
      case MENU_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Alterna uso de mouse.\n"
               " \n"
               "Quando o mouse é usado, RetroArch o esconde e \n"
               "mantém o seu ponteiro dentro da janela para \n"
               "permitir que a entrada relativa do mouse \n"
               "funcione melhor.");
         break;
      case MENU_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Circula por imagens de discos. Usar \n"
               "após ejetar. \n"
               " \n"
               " Finaliza ao usar ejetar novamente.");
         break;
      case MENU_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
         snprintf(s, len,
               "Filtro de vídeo baseado em CPU.");
#else
         snprintf(s, len,
               "Filtro de vídeo baseado em CPU.\n"
               " \n"
               "Caminho para uma biblioteca dinâmica.");
#endif
         break;
      case MENU_LABEL_AUDIO_DEVICE:
         snprintf(s, len,
               "Sobrepõe-se ao dispositivo de áudio padrão \n"
               "que está em uso.\n"
               "É dependente do driver. \n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA precisa de um dispositivo PCM."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS precisa de um caminho (ex.: /dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK precisa de portas (ex.: system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound precisa de um endereço IP para \n"
               "servidor RSound."
#endif
               );
         break;
      case MENU_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Alterna ejeção para discos.\n"
               " \n"
               "Usado para conteúdos multidiscos.");
         break;
      case MENU_LABEL_ENABLE_HOTKEY:
         snprintf(s, len,
               "Ativar outras hotkeys.\n"
               " \n"
               " Se esta hotkey é usada por teclado, joybutton \n"
               "ou joyaxis, todas as outras hotkeys serão \n"
               "desativadas a menos que esta hotkey esteja sendo \n"
               "usada ao mesmo tempo. \n"
               " \n"
               "Isso é útil para implementações RETRO_KEYBOARD que \n"
               "consultam uma grande área do teclado, cujo caminho \n"
               "deve estar livre das hotkeys.");
         break;
      case MENU_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Ativa retrocesso.\n"
               " \n"
               "Essa opção causa uma perda de desempenho, \n"
               "por isso está desativada por padrão.");
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Diretórios de Cores. \n"
               " \n"
               "Um diretório onde são buscadas as \n"
               "implementações de cores libretro.");
         break;
      case MENU_LABEL_VIDEO_REFRESH_RATE_AUTO:
         {
            /* Work around C89 limitations */
            char u[501];
            char t[501];
            snprintf(u, sizeof(u),
                  "Taxa de Atualização Automática.\n"
                  " \n"
                  "A taxa de atualização exata de nosso monitor (Hz).\n"
                  "É usada para calcular a taxa de entrada de áudio \n"
                  "com a fórmula: \n"
                  " \n"
                  "audio_input_rate = game input rate * display \n"
                  "refresh rate / game refresh rate\n"
                  " \n");
            snprintf(t, sizeof(t),
                  "Se a implementação não informar valores, \n"
                  "valores NTSC serão assumidos por questão de \n"
                  "compatibilidade.\n"
                  " \n"
                  "Esse valor deve ficar próximo de 60Hz para \n"
                  "evitar grande mudanças de pitch. Se o monitor \n"
                  "não rodar a 60Hz, ou algo próximo a isso, desative\n"
                  "o VSync, e deixe-o com valores padrão.");
            strlcat(s, u, len);
            strlcat(s, t, len);
         }
         break;
      case MENU_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Força uma certa rotação da tela. \n"
               " \n"
               "A rotação é adicionada a outras definidas\n"
               "por conjuntos de cores (veja Permitir\n"
               "Rotação de Vídeo).");
         break;
      case MENU_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Resolução de tela cheia.\n"
               " \n"
               "Resolução 0 usa a resolução \n"
               "do ambiente.\n");
         break;
      case MENU_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "Taxa de Avanço Rápido."
               " \n"
               "A taxa máxima na qual o conteúdo será\n"
               "executado ao se usar o Avanço Rápido.\n"
               " \n"
               " (Ex.: 5.0 para conteúdo 60 fps => 300 fps \n"
               "máximo).\n"
               " \n"
               "RetroArch entra em modo sleep para assegurar \n"
               "que a taxa máxima não será excedida.\n"
               "Não confie que esse teto tenha exatidão \n"
               "perfeita.");
         break;
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Preferência de monitor.\n"
               " \n"
               "0 (padrão) significa nenhum monitor é \n"
               "preferido, 1 e demais (1 é o primeiro \n"
               "monitor), sugere ao RetroArch usar esse \n"
               "monitor em particular.");
         break;
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Força o descarte de quadros overscanned. \n"
               " \n"
               "O comportamento exato dessa opção é \n"
               "específico da implementação do core.");
         break;
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
         snprintf(s, len,
               "Só interpola vídeo em escalas múltiplas \n"
               "inteiras da resolução nativa.\n"
               " \n"
               "O tamanho base depende da geometria e da \n"
               "relação de aspecto informadas pelo sistema.\n"
               " \n"
               "Se Forçar Aspecto não estiver definida, X/Y \n"
               "serão escalonados em inteiros independentemente.");
         break;
      case MENU_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Volume de Áudio, em dB.\n"
               " \n"
               " 0 dB é o volume normal. Nenhum ganho aplicado.\n"
               "O ganho pode ser controlado em execução com \n"
               "Aumentar Volume / Baixar Volume.");
         break;
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         snprintf(s, len,
               "Controle de taxa de áudio.\n"
               " \n"
               "Definindo como 0 desativa o controle de taxa.\n"
               "Outros valores controlam a variação da taxa \n"
               "de áudio.\n"
               " \n"
               "Define quanto de taxa de entrada pode ser \n"
               "regulada dinamicamente.\n"
               " \n"
               " Taxa de entrada é definida como: \n"
               " input rate * (1.0 +/- (rate control delta))");
         break;
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
         snprintf(s, len,
               "Distorção de áudio máxima.\n"
               " \n"
               "Define a máxima variação da taxa de entrada.\n"
               "Você pode querer aumentá-la para obter grandes\n"
               "variações no compasso, por exemplo, ao rodar\n"
               "cores PAL em telas NTSC, ao custo de um pitch\n"
               "de áudio inexato.\n"
               " \n"
               " A taxa de entrada é definida como: \n"
               " input rate * (1.0 +/- (max timing skew))");
         break;
      case MENU_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Alterna para o próximo overlay.\n"
               " \n"
               "Navegação circular.");
         break;
      case MENU_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Ativa ou desativa nível de prolixidade \n"
               "do frontend.");
         break;
      case MENU_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Aumenta o volume de áudio.");
         break;
      case MENU_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Baixa o volume de áudio.");
         break;
      case MENU_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Desativa composition à força.\n"
               "Válido somente para Windows Vista/7 atualmente.");
         break;
      case MENU_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Ativa ou desativa contadores de desempenho \n"
               "do frontend.");
         break;
      case MENU_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "Diretório system. \n"
               " \n"
               "Define o diretório 'system'.\n"
               "Cores podem consultar esse diretório\n"
               "para carregar BIOS, configurações\n"
               "específicas de sistemas, etc.");
         break;
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Automaticamente salva um Savestate ao fechar \n"
               "o RetroArch.\n"
               " \n"
               "RetroArch carregará automaticamente qualquer\n"
               "Savestate com esse caminho ao iniciar se 'Carregar\n"
               "Savestate Automaticamente' estiver ativado.");
         break;
      case MENU_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Usa driver de vídeo em thread.\n"
               " \n"
               "Usando isso pode melhorar o desempenho ao \n"
               "possível custo de latência e mais engasgos \n"
               "de vídeo.");
         break;
      case MENU_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Sincronismo Vertical de vídeo.\n");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Tenta sincronizar CPU com GPU via \n"
               "hardware.\n"
               " \n"
               "Pode reduzir a latência ao custo de \n"
               "desempenho.");
         break;
      case MENU_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Granularidade do retrocesso.\n"
               " \n"
               " Ao retroceder um número definido de \n"
               "quadros, você pode retroceder vários \n"
               "quadros por vez, aumentando a velocidade \n"
               "de retrocesso.");
         break;
      case MENU_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Tira uma foto da tela.");
         break;
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Define quantos milissegundos retardar \n"
               "após o VSync antes de executar o core.\n"
               "\n"
               "Pode reduzir a latência ao custo de\n"
               "um maior risco de engasgo de vídeo.\n"
               " \n"
               "O valor máximo é 15.");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
         snprintf(s, len,
               "Define quantos quadros a CPU pode rodar \n"
               "adiante da GPU com a opção 'Sincronismo \n"
               "de GPU via Hardware' ativada.\n"
               " \n"
               "O valor máximo é 3.\n"
               " \n"
               " 0: Sincroniza com GPU de imediato.\n"
               " 1: Sincroniza com quadro anterior.\n"
               " 2: Etc ...");
         break;
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(s, len,
               "Insere um quadro preto entre quadros. \n"
               " \n"
               "Útil para monitores de 120 Hz ao rodar \n"
               "material de 60 Hz com eliminação do efeito \n"
               "'ghosting'.\n"
               " \n"
               "A taxa de atualização de vídeo deve ainda \n"
               "ser configurada como se fosse um monitor de \n"
               "60 Hz (divida a taxa de atualização por 2).");
         break;
      case MENU_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Mostra a tela inicial no menu.\n"
               "É definida automaticamente como falso quando\n"
               "vista pela primeira vez.\n"
               " \n"
               "É atualizada na configuração apenas quando a\n"
               "opção 'Salvar Configuração ao Sair' está ativada.\n");
         break;
      case MENU_LABEL_CORE_SPECIFIC_CONFIG:
         snprintf(s, len,
               "Carrega uma configuração específica baseada \n"
               "no core que está sendo usado.\n");
         break;
      case MENU_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Alterna tela cheia.");
         break;
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Previne SRAM de ser sobrescrita ao \n"
               "carregar Savestates.\n"
               " \n"
               "Pode potencialmente levar a jogos bugados.");
         break;
      case MENU_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pausa a jogatina quando o foco da janela \n"
               "é perdido.");
         break;
      case MENU_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Captura material gráfico de saída da \n"
               "GPU se estiver disponível.");
         break;
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Diretório de Capturas de Tela. \n"
               " \n"
               "Diretório para guardar as capturas de tela."
               );
         break;
      case MENU_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "Intervalo de Troca de VSync.\n"
               " \n"
               "Usa um intervalo de troca personalizado. \n"
               "Use-e para reduzir à metade a taxa de \n"
               "atualização do monitor.");
         break;
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Diretório de Saves. \n"
               " \n"
               "Salva todos os arquivos de save (*.srm) nesse \n"
               "diretório. Isso inclui arquivos relacionados \n"
               "como .bsv, .rt, .psrm, etc...\n"
               " \n"
               "Pode ser sobreposto por opções explícitas de\n"
               "linha de comando.");
         break;
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Diretório de Savestates. \n"
               " \n"
               "Salva todos os Savestates (*.state) nesse \n"
               "diretório.\n"
               " \n"
               "Pode ser sobreposto por opções explícitas de\n"
               "linha de comando.");
         break;
      case MENU_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Diretório de Recursos (Assets). \n"
               " \n"
               " Essa localização é consultada quando se \n"
               "tenta buscar pelo menu recursos (assets) \n"
               "carregáveis, etc.");
         break;
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Diretório de Papéis de Parede Dinâmicos. \n"
               " \n"
               " O lugar para armazenar papéis de parede que \n"
               "serão carregados dinamicamente pelo menu \n"
               "dependendo do contexto.");
         break;
      case MENU_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Taxa de câmera lenta."
               " \n"
               "Quando ativado, o conteúdo rodará em velocidade\n"
               "reduzida por um fator.");
         break;
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         snprintf(s, len,
               "Define o limiar de eixo.\n"
               " \n"
               "O quanto deve ser torcido um eixo para\n"
               "resultar em um botão pressionado.\n"
               " Valores possíveis são [0.0, 1.0].");
         break;
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len, 
               "Período de turbo.\n"
               " \n"
               "Descreve a velocidade na qual se alternam\n"
               "os botões com turbo ativado."
               );
         break;
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Ativa autodetecção de entrada.\n"
               " \n"
               "Tentará autoconfigurar joypads \n"
               "em um estilo Plug-and-Play.");
         break;
      case MENU_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Autorizar ou desautorizar o acesso da câmera \n"
               "pelos cores.");
         break;
      case MENU_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Autorizar ou desautorizar o acesso de \n"
               "serviços de localização pelos cores.");
         break;
      case MENU_LABEL_TURBO:
         snprintf(s, len,
               "Ativar turbo.\n"
               " \n"
               "Segurando o turbo enquanto se pressiona outro \n"
               "botão permitirá que o botão entre em modo \n"
               "turbo em que o seu estado será modulado com \n"
               "um sinal periódico. \n"
               " \n"
               "A modulação pára quando o próprio botão \n"
               "(não é o botão de turbo) é solto.");
         break;
      case MENU_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Ativar/desativar teclado na tela.");
         break;
      case MENU_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Ligar/desligar áudio.");
         break;
      case MENU_LABEL_REWIND:
         snprintf(s, len,
               "Segure o botão para retroceder.\n"
               " \n"
               "Retrocesso deve estar ativado.");
         break;
      case MENU_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Tecla para sair corretamente do RetroArch."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nFechando-o de outra forma mais agressiva \n"
               "(SIGKILL, etc) sairá sem salvar RAM, etc.\n"
               "Em sistemas baseados em Unix,\n"
               "SIGINT/SIGTERM permite um\n"
               "fechamento correto."
#endif
               );
         break;
      case MENU_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Carrega Savestates.");
         break;
      case MENU_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Salva Savestates.");
         break;
      case MENU_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay inverte usuários.");
         break;
      case MENU_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Incrementa o índice de cheats.\n");
         break;
      case MENU_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrementa o índice de cheats.\n");
         break;
      case MENU_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Aplica o shader anterior no diretório.");
         break;
      case MENU_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Aplica o próximo shader no diretório.");
         break;
      case MENU_LABEL_RESET:
         snprintf(s, len,
               "Reinicia o conteúdo.\n");
         break;
      case MENU_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Alterna estado de pausado e não pausado.");
         break;
      case MENU_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Alterna índice de cheats.\n");
         break;
      case MENU_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Segure para avanço rápido. Soltando o botão \n"
               "desativa o avanço rápido.");
         break;
      case MENU_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Segure para câmera lenta.");
         break;
      case MENU_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "O quadro avança quando o conteúdo está pausado.");
         break;
      case MENU_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Alterna entre estar gravando ou não.");
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
               "Eixo para o analógico (esquema DualShock).\n"
               " \n"
               "Associa normalmente, porém, se um analógico real \n"
               "é associado, pode ser lido como um analógico\n"
               "verdadeiro. \n"
               " \n"
               "Eixo positivo X é para direita. \n"
               "Eixo positivo Y é para baixo.");
         break;
      default:
         if (s[0] == '\0')
            strlcpy(s, menu_hash_to_str(MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

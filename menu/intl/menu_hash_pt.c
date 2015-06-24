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

#include "../menu_hash.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
extern const char encoding_test[sizeof("ø")==2 ? 1 : -1];

const char *menu_hash_to_str_pt(uint32_t hash)
{
   switch (hash)
   {
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
         return "<Dir. de Conteúdo>";
      case MENU_VALUE_UNKNOWN:
         return "Desconhecido";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Padrão>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Nenhum>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Dir. de Remapeamento de Controladores";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Dir. de Config. Automática de Dispositivos de Entrada";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Dir. de Config. de Gravação";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Dir. de Gravações";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Dir. de Capturas de Telas";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Dir. de Listas de Jogos";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Dir. de Saves";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Dir. de Savestates";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandos stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver de Vídeo";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Ativar Gravação";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Ativar Gravação na GPU";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "Caminho da Gravação";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Usar Dir. de Gravações";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configurar Gravação";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Ativar Filtro Pós-Gravação";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Dir. de Recursos (Assets) dos Cores";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Dir. de Recursos (Assets)";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dir. de Papéis de Parede Dinâmicos";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Dir. de Boxarts";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Dir. de Navegação";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Dir. de Configurações";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Dir. de Informações de Cores";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Dir. de Cores";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Dir. de Cursores";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Dir. de Base de Dados de Conteúdos";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Diretório System";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Dir. de Arquivos Cheat";
      case MENU_LABEL_VALUE_EXTRACTION_DIRECTORY:
         return "Dir. de Descompactação";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Dir. de Filtros de Áudio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Dir. de Shaders de Vídeo";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Dir. de Filtros de Vídeo";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Dir. de Overlays";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Dir. de Overlay de Teclado";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Trocar Controlador de Netplay";
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
      case MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE:
         return "Juntar Subgrupos";
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
         return "Modo Archive";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandos de Rede";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta para Ações de Rede";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Ativar Lista de Histórico";
      case MENU_LABEL_CONTENT_HISTORY_SIZE:
         return "Content History Size";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Tamanho da Lista de Histórico";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Taxa de Atualização de Quadros Estimada";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Desligar Core Dummy On";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Não Iniciar Cores Automaticamente";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
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
         return "Índice de Proporções de Tela";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Proporção de Tela Automática";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forçar Proporção de Tela";
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
         return "Fixar Largura de Tela VI";
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
         return "Interpolação em Janela";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Interpolação em Múltiplos Inteiros";
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
         return "Permitir Câmera";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Permitir Localização";
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
         return "Carregar Conteúdo (Coleção)"; /* FIXME */
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Carregar Conteúdo (Detectar Core)"; /* FIXME */
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Carregar Conteúdo (Histórico)";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Ativar Áudio";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Mostrar Taxa de Quadros";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Desligar Áudio";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume de Áudio (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Ativar Sincronismo de Áudio";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Variação Máxima de Taxa de Áudio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Número de Shaders";
      case MENU_LABEL_VALUE_CONFIGURATIONS: /* FIXME */
         return "Arquivos de Configuração";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Granularidade do Retrocesso";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carregar Remapeamento";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Salvar Remapeamento Como";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Proporção de Tela Personalizada";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usar este diretório>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Iniciar Conteúdo";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opções de Discos do Core";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opções de Core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opções de Cheat do Core";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carregar Cheat";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salvar Cheat Como";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Contadores de Cores";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturar Tela";
      case MENU_LABEL_VALUE_RESUME:
         return "Retomar";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Índice de Discos";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contadores de Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Anexar Imagem de Disco";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Estado do Drive de Disco";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Lista de Jogos Vazia.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nenhuma Informação de Core Disponível.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nenhuma Opção de Core Disponível.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nenhum Core Disponível.";
      case MENU_VALUE_NO_CORE:
         return "Nenhum Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gerenciador de Base de Dados";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gerenciador de Cursores";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Configurações de Gravação";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Configurações";
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
         return "URL de Cores - Buildbot";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL de Recursos (Assets) - Buildbot";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "Navegação Horizontal Circular";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_VERTICAL:
         return "Navegação Vertical Circular";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Navegador - Filtrar por Extensões Suportadas";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Extrair Arquivos Baixados Automaticamente";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informação de Sistema";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informação de Core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Diretório Não Encontrado.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Nenhum Item.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Carregar Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Carregar Conteúdo";  /* FIXME */
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Descarregar Core"; /* FIXME */
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Gerenciamento Avançado";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Salvar Savestate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Carregar Savestate";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Retomar Conteúdo";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Configurações de Drivers";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Driver de Controladores";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver de Áudio";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver de Joypad";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver de Amostragem de Áudio";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Driver de Gravação";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Driver de Menu";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver de Câmera";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver de Localização";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapaz de Ler Arquivo Comprimido.";
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
      case MENU_LABEL_VALUE_BOXART:
         return "Mostrar Boxart";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opções de Remapeamento de Controlador de Core";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opções de Shaders";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salvar Predefinições de Shader Como";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nenhum Parâmetro de Shader Disponível.";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carregar Predefinições de Shader";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtros de Vídeo";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin de DSP de Áudio";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Iniciando Download: ";
      case MENU_VALUE_SECONDS:
         return "Segundos";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }
 
   return "null";
}

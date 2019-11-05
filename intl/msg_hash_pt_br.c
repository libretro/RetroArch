/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_pt_br_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
    {
       snprintf(s, len,
             "TODO/FIXME - Fill in message here."
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
                   "Alternar entre Avanço Rápido e \n"
                   "velocidade normal."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "Manter pressionado para Avanço Rápido. \n"
                   " \n"
                   "Soltar o botão desativa o Avanço Rápido."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Alternar câmera lenta.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Manter pressionado para Câmera Lenta.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "Alternar estado pausado e não-pausado.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "Avanço de quadro do conteúdo pausado.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "Aplicar o próximo Shader do diretório.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "Aplicar o Shader anterior do diretório.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "Trapaças.");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "Reiniciar o conteúdo.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "Capturar tela.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "Áudio mudo/não-mudo.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "Alternar o teclado virtual.");
             break;
          case RARCH_FPS_TOGGLE:
             snprintf(s, len,
                   "Alternar contador de quadros por segundo.");
             break;
          case RARCH_SEND_DEBUG_INFO:
             snprintf(s, len,
                   "Envia informações de diagnóstico sobre o seu dispositivo e a configuração do RetroArch aos nossos servidores para análise.");
             break;
          case RARCH_NETPLAY_HOST_TOGGLE:
             snprintf(s, len,
                   "Ativar ou desativar a hospedagem de jogo em rede.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Alternar modo jogador ou espectador do Jogo em Rede.");
             break;
          case RARCH_ENABLE_HOTKEY:
             snprintf(s, len,
                   "Habilitar outras teclas de atalho. \n"
                   " \n"
                   "Se essa tecla de atalho estiver vinculada \n"
                   "ao teclado ou botão / eixo de joypad, todas \n"
                   "as outras teclas de atalho serão desabilitadas \n"
                   "a menos que essa tecla de atalho também esteja \n"
                   "pressionada ao mesmo tempo. \n"
                   " \n"
                   "Como alternativa, todas as teclas de atalho \n"
                   "para teclado podem ser desativadas pelo usuário.");
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                   "Aumentar o volume do áudio.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                   "Diminuir o volume do áudio.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                   "Mudar para a próxima Sobreposição.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                   "Alternar ejeção de disco. \n"
                   " \n"
                   "Usado para conteúdo em vários discos. ");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                   "Alternar pelas imagens de disco. Utilizado. \n"
                   "após a ejeção. \n"
                   " \n"
                   "Concluído após alternar novamente a ejeção.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                   "Alternar captura de Mouse. \n"
                   " \n"
                   "Quando o Mouse é capturado, o RetroArch oculta \n"
                   "o cursor do Mouse, e mantém o Mouse dentro \n"
                   "da janela para permitir que a entrada de Mouse \n"
                   "relativa funcione melhor.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Alternar o foco do jogo.\n"
                   " \n"
                   "Quando um jogo tem foco, o RetroArch irá \n"
                   "desabilitar as teclas de atalho e manter \n"
                   "o cursor do mouse dentro da janela.");
             break;
          case RARCH_MENU_TOGGLE:
                snprintf(s, len, "Mostrar ou ocultar o menu.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "Carregar Estado de Jogo.");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "Alternar tela cheia.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
                         "Tecla para sair do RetroArch de modo limpo. \n"
                   " \n"
                                "Encerrá-lo de forma brusca (SIGKILL, etc.) irá \n"
                                "terminar o RetroArch sem salvar RAM, etc."
#ifdef __unix__
                        "\nEm Unix-likes, SIGINT/SIGTERM permitem \n"
                        "uma desinicialização limpa."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "Compartimentos de Estado de Jogo.\n"
                   " \n"
                   "Com o compartimento é definido em 0, o nome do \n"
                   "Estado de Jogo é *.state \n"
                   "(ou a definição da linha de comando). \n"
                   " \n"
                   "Quando o compartimento não for 0, o caminho será \n"
                   "(caminho)(n), onde (n) é o número do compartimento.");
             break;
          case RARCH_SAVE_STATE_KEY:
             snprintf(s, len,
                         "Salvar Estado de Jogo.");
             break;
          case RARCH_REWIND:
             snprintf(s, len,
                   "Manter o botão pressionado para \n"
                   "Rebobinar. \n"
                   " \n"
                   "Rebobinar precisa estar habilitado.");
             break;
          case RARCH_BSV_RECORD_TOGGLE:
             snprintf(s, len,
                   "Alternar entre gravando ou não.");
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
            snprintf(s, len, "Detalhes de acesso para \n"
                    "sua conta RetroAchievements. \n"
                    " \n"
                    "Visite retroachievements.org e inscreva-se \n"
                    "em uma conta gratuita. \n"
                    " \n"
                    "Após o registro, você precisa \n"
                    "cadastrar o nome de usuário e a senha \n"
                    "no RetroArch.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len, "Nome de usuário da conta RetroAchievements.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len, "Senha da conta RetroAchievements.");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            snprintf(s, len, "Altera o idioma do menu e todas as mensagens \n"
                    "na tela de acordo com o idioma selecionado \n"
                    "aqui. \n"
                    " \n"
                    "Requer a reinicialização para que as \n"
                    "alterações entrem em vigor. \n"
                    " \n"
                    "OBS: nem todos os idiomas podem estar atualmente \n"
                    "implementados. \n"
                    " \n"
                    "No caso de um idioma não estar implementado, \n"
                    "retorna para Inglês.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            snprintf(s, len, "Mudar a fonte que é utilizada \n"
                    "para o texto da Exibição na Tela.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "Autocarregar opções de núcleo específicas do conteúdo.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Autocarregar configurações de redefinição.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Autocarregar arquivos de remapeamento de entrada.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Ordenar Estados de Jogo em pastas \n"
                    "com o nome do núcleo Libretro utilizado.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Ordenar Jogos-Salvos em pastas \n"
                    "com o nome do núcleo Libretro utilizado.");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Sair do menu e retornar \n"
                    "ao conteúdo.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "Reiniciar o conteúdo do começo.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "Fechar e descarregar o conteúdo \n"
                    "da memória.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Se um Estado de Jogo foi carregado, o \n"
                    "conteúdo irá retornar ao estado anterior \n"
                    "ao carregamento.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Se um Estado de Jogo for sobrescrito, ele irá \n"
                    "retornar ao Estado de Jogo anterior.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Capturar a tela. \n"
                    " \n"
                    "As imagens capturadas serão armazenadas dentro \n"
                    "do Diretório de Captura de Telas.");
            break;
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Adicionar o item aos seus Favoritos.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "Iniciar o conteúdo.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "Exibir informação adicional \n"
                    "de metadados sobre o conteúdo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Arquivo de configuração.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Arquivo comprimido.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Arquivo de configuração de gravação.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Arquivo de cursor da base de dados.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Arquivo de configuração.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Selecione para analisar o diretório \n"
                             "atual por conteúdo.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Selecione para definir esse como o diretório.");
            break;
        case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            snprintf(s, len,
                     "Diretório da Base de Dados de Conteúdo. \n"
                             " \n"
                             "Caminho para o diretório \n"
                             "da base de dados de conteúdo.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Miniaturas. \n"
                             " \n"
                             "Para armazenar arquivos de miniaturas.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Diretório de Informações de Núcleos. \n"
                             " \n"
                             "Um diretório por onde procurar \n"
                             "informação sobre os núcleos Libretro.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Lista de Reprodução. \n"
                             " \n"
                             "Salva todos os arquivos de \n"
                             "lista de reprodução neste diretório.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Alguns núcleos podem ter \n"
                             "um recurso de desligamento. \n"
                             " \n"
                             "Se esta opção for deixada desabilitada, \n"
                             "selecionar o procedimento de desligamento \n"
                             "irá fazer com que o RetroArch seja encerrado. \n"
                             " \n"
                             "Habilitar esta opção irá carregar um \n"
                             "núcleo modelo, permanecendo dentro do menu \n"
                             "e o RetroArch não irá ser encerrado.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Alguns núcleos podem precisar de \n"
                             "arquivos de firmware ou BIOS. \n"
                             " \n"
                             "Se esta opção estiver desabilitada, \n"
                             "será feito uma tentativa de carregar \n"
                             "mesmo que o firmware esteja faltando. \n");
            break;
        case MENU_ENUM_LABEL_PARENT_DIRECTORY:
            snprintf(s, len,
                     "Retornar ao diretório superior.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            snprintf(s, len,
                     "Abra as configurações de permissão do Windows \n"
                     "para ativar o recurso broadFileSystemAccess.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER:
           snprintf(s, len,
                     "Abra o seletor de arquivos do sistema para \n"
                     "acessar os diretórios adicionais.");
           break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
            snprintf(s, len,
                     "Arquivo de predefinição de Shader.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
            snprintf(s, len,
                     "Arquivo de Shader.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
            snprintf(s, len,
                     "Arquivo de remapeamento de controles.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
            snprintf(s, len,
                     "Arquivo de Trapaça.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
            snprintf(s, len,
                     "Arquivo de Sobreposição.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
            snprintf(s, len,
                     "Arquivo de base de dados.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
            snprintf(s, len,
                     "Arquivo de fonte TrueType.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
            snprintf(s, len,
                     "Arquivo comum.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
            snprintf(s, len,
                     "Vídeo. \n"
                             " \n"
                             "Selecione para abrir este arquivo \n"
                             "com o reprodutor de vídeo");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Música. \n"
                             " \n"
                             "Selecione para abrir este arquivo \n"
                             "com o reprodutor de música.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Arquivo de imagem.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Imagem. \n"
                             " \n"
                             "Selecione para abrir este arquivo \n"
                             "com o visualizador de imagens.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Núcleo Libretro. \n"
                             " \n"
                             "Selecionar esta opção associa este \n"
                             "núcleo com o jogo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Núcleo Libretro. \n"
                             " \n"
                             "Selecione este arquivo para que o \n"
                             "RetroArch carregue este núcleo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Diretório. \n"
                             " \n"
                             "Selecione para abrir esse diretório.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Cache. \n"
                             " \n"
                             "Conteúdo descomprimido pelo RetroArch será \n"
                             "temporariamente extraído para este diretório.");
            break;
        case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
            snprintf(s, len,
                     "Se habilitado, todo o conteúdo carregado \n"
                             "no RetroArch será automaticamente \n"
                             "adicionado a lista de histórico recente.");
            break;
        case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Diretório do Navegador de Arquivos. \n"
                             " \n"
                             "Define o diretório inicial para o \n"
                             "navegador de arquivos do menu. \n");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influencia como a chamada seletiva da \n"
                             "entrada é feita dentro do RetroArch. \n"
                             " \n"
                             "Mais cedo - A chamada da entrada é realizada \n"
                             "antes do quadro ser processado. \n"
                             "Normal - A chamada da entrada é realizada \n"
                             "quando a chamada é requisitada. \n"
                             "Mais tarde - A chamada seletiva é realizada \n"
                             "no primeiro estado de entrada requisitado \n"
                             "pelo quadro. \n"
                             " \n"
                             "Definir como 'Cedo' ou 'Tarde' pode resultar \n"
                             "em menos latência, \n"
                             "dependendo da sua configuração.\n\n"
                             "Será ignorado no Jogo em Rede."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
            snprintf(s, len,
                     "Ocultar descritores de entrada que não foram \n"
                             "definidos pelo núcleo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            snprintf(s, len,
                     "Taxa de atualização de vídeo do seu monitor. \n"
                             "Utilizado para calcular uma taxa de \n"
                             "entrada de áudio adequada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            snprintf(s, len,
                     "Desabilitar a força o suporte sRGB FBO. \n"
                             "Alguns drivers Intel OpenGL no Windows \n"
                             "possuem problemas de vídeo com o recurso \n"
                             "sRGB FBO se estiver habilitado.");
            break;
        case MENU_ENUM_LABEL_AUDIO_ENABLE:
            snprintf(s, len,
                     "Habilitar saída de áudio.");
            break;
        case MENU_ENUM_LABEL_AUDIO_SYNC:
            snprintf(s, len,
                     "Sincronizar áudio (recomendado).");
            break;
        case MENU_ENUM_LABEL_AUDIO_LATENCY:
            snprintf(s, len,
                     "Latência de áudio desejada, em milissegundos. \n"
                             "Pode não ser honrado se o driver de áudio \n"
                             "não puder prover a latência desejada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            snprintf(s, len,
                     "Permitir que os núcleos definam a rotação. \n"
                             "Se for falso, pedidos de rotação são honrados, \n"
                             "porém ignorados. \n"
                             "Útil para configurações onde se rotaciona \n"
                             "manualmente a tela.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
            snprintf(s, len,
                     "Exibir os descritores de entrada definidos \n"
                             "pelo núcleo em vez dos padrões.");
            break;
        case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            snprintf(s, len,
                     "Número de itens para manter na \n"
                             "lista de reprodução recente.");
            break;
        case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
            snprintf(s, len,
                     "Utilizar ou não o modo janela quando \n"
                             "em tela cheia.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
            snprintf(s, len,
                     "Tamanho da fonte para mensagens na tela.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
            snprintf(s, len,
                     "Automaticamente aumenta o índice de box \n"
                             "a cada salvamento, gerando vários arquivos \n"
                             "de Estado de Jogo. \n"
                             "Quando o conteúdo for carregado, o box \n"
                             "de Estado de Jogo será o do valor existente \n"
                             "mais alto (último Estado de Jogo).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Habilitar a exibição de quadros \n"
                             "por segundos.");
            break;
        case MENU_ENUM_LABEL_MEMORY_SHOW:
            snprintf(s, len,
                     "Inclui a exibição da memória atual \n"
                             "uso/total com FPS/Quadros.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            snprintf(s, len,
                     "Exibir ou ocultar mensagens na tela.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Deslocamento de onde as mensagens \n"
                             "serão colocadas na tela. Os valores aceitos \n"
                             "são na faixa de [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Habilitar ou desabilitar \n"
                     "a Sobreposição atual.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Ocultar a exibição de Sobreposição \n"
                             "dentro do menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Exibir comandos do teclado/controle na \n"
                            "sobreposição.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Seleciona a porta de escuta dos comandos do controle \n"
                            "a serem exibidos na sobreposição.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Caminho para predefinição de Sobreposição.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Opacidade da Sobreposição.");
            break;
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE:
            snprintf(s, len,
                     "Ativar ou desativar o esquema de vídeo atual.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
            snprintf(s, len,
                     "Caminho para o esquema de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW:
            snprintf(s, len,
                     "Os esquemas podem conter várias visualizações. \n"
                     "Selecione uma visualização.");
            break;
#endif
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Tempo limite para vínculo de entrada \n"
                             "(em segundos). \n"
                             "Quantos segundos aguardar até prosseguir \n"
                             "para o próximo vínculo.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Tempo de pressionamento do vínculo de entrada (em segundos). \n"
               "Quantidade de segundos para manter uma entrada para vinculá-la.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_SCALE:
            snprintf(s, len,
                     "Escala da Sobreposição.");
            break;
        case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            snprintf(s, len,
                     "Taxa de amostragem da saída de áudio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            snprintf(s, len,
                     "Defina como verdadeiro se os núcleos \n"
                             "renderizados por hardware devem ter seu \n"
                             "próprio contexto privado. \n"
                             "Evita ter que assumir mudanças de \n"
                             "estado de hardware entre quadros."
            );
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Carregar Núcleo. \n"
                             " \n"
                             "Procurar por uma implementação de núcleo \n"
                             "Libretro. Aonde o navegador de arquivos \n"
                             "inicia depende do seu caminho \n"
                             "Diretório de Núcleo. \n"
                             "Se não definido, inicia na raiz. \n"
                             " \n"
                             "Se Diretório de Núcleo estiver definido, o menu \n"
                             "irá utilizar o caminho como local inicial. \n"
                             "Se Diretório de Núcleo for um caminho completo, \n"
                             "ele irá iniciar na pasta do aonde o arquivo está.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "Você pode utilizar os seguintes controles \n"
                             "abaixo tanto no seu Gamepad quanto no \n"
                             "teclado a fim de controlar o menu: \n"
                             " \n"
            );
            break;
        case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
            snprintf(s, len,
                     "Bem-vindo ao RetroArch \n"
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                    "O RetroArch utiliza uma forma única de \n"
                            "sincronização de áudio/video aonde ele \n"
                            "precisa ser calibrado pela taxa de \n"
                            "atualização da sua tela para um melhor \n"
                            "resultado no desempenho. \n"
                            " \n"
                            "Se você experimentar qualquer estalido \n"
                            "no áudio ou rasgo de vídeo, normalmente \n"
                            "isto significa que você precisa calibrar \n"
                            "as configurações. Algumas escolhas abaixo: \n"
                            " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) Vá para '%s' -> '%s', e habilite \n"
                             "'Vídeo Paralelizado'. A taxa de atualização \n"
                             "não irá importar neste modo, a taxa de \n"
                             "quadros será maior, mas o vídeo será \n"
                             "menos fluído. \n"
                             "b) Vá para '%s' -> '%s', e observe \n"
                             "'%s'. Deixe executar até \n"
                             "2048 quadros, então pressione 'OK'.",
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
                     "Para analisar por conteúdo, vá para \n"
                             "'%s' e selecione \n"
                             "'%s' ou '%s'. \n"
                             " \n"
                             "Os arquivos serão comparados com as \n"
                             "entradas do banco de dados. \n"
                             "Se houver uma correspondência, um registro \n"
                             "será adicionado à uma lista de reprodução. \n"
                             " \n"
                             "Você poderá então acessar facilmente este \n"
                             "conteúdo indo até \n"
                             "'%s' -> '%s'\n"
                             "em vez de ter que utilizar o \n"
                             "navegador de arquivos todas as vezes.\n"
                             " \n"
                             "OBS: Conteúdo para alguns núcleos pode ainda \n"
                             "não ser reconhecido.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
            snprintf(s, len,
                     "Bem-vindo ao RetroArch\n"
                             "\n"
                             "Extraindo recursos, por favor aguarde. \n"
                             "Isso pode levar algum tempo...\n"
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.input_driver : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                     snprintf(s, len,
                           "Driver de entrada udev. \n"
                           " \n"
                           "Utiliza a recente API evdev joypad \n"
                           "para suporte a Joystick. Suporta \n"
                           "hotplugging e force feedback. \n"
                           " \n"
                           "O driver lê eventos evdev para suporte \n"
                           "a teclado. Também suporta keyboard callback, \n"
                           "Mouses e Touchpads. \n"
                           " \n"
                           "Por padrão na maioria das distros, \n"
                           "nodes /dev/input são somente root (mode 600). \n"
                           "Você pode criar uma regra udev para torná-los \n"
                           "acessíveis para não root."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "Driver de entrada linuxraw. \n"
                           " \n"
                           "Este driver requer um TTY ativo. Eventos de \n"
                           "teclado são lidos diretamente do TTY o que \n"
                           "o torna simples, mas não tão flexível, \n"
                           "quanto udev. \n"
                           "Mouses, etc, não são suportados de nenhum \n"
                           "modo. \n"
                           " \n"
                           "Este driver utiliza a antiga API de Joystick \n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Driver de entrada.\n"
                           " \n"
                           "Dependendo do driver de vídeo, pode \n"
                           "forçar um driver de entrada diferente.");
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "Carregar Conteúdo. \n"
                             "Navegar por conteúdo. \n"
                             " \n"
                             "Para carregar conteúdo, você precisa de \n"
                             "um 'Núcleo' para utilizar, e um arquivo \n"
                             "de conteúdo."
                             " \n"
                             "Para controlar aonde o menu inicia \n"
                             "a procura pelo conteúdo, configure \n"
                             "'Diretório do Navegador de Arquivos'. \n"
                             "Se não estiver configurado, ele irá \n"
                             "iniciar na raiz. \n"
                             " \n"
                             "O navegador irá filtrar as \n"
                             "extensões para o último núcleo definido \n"
                             "em 'Carregar Núcleo', o qual será utilizado \n"
                             "quando o conteúdo for carregado."
            );
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            snprintf(s, len,
                     "Carregando conteúdo do histórico. \n"
                             " \n"
                             "Ao carregar conteúdo, combinações de núcleo \n"
                             "Libretro e conteúdo são salvas no histórico. \n"
                             " \n"
                             "O histórico é salvo em um arquivo no mesmo \n"
                             "diretório do arquivo de configuração do \n"
                             "RetroArch. \n"
                             "Se nenhum arquivo de configuração foi  \n"
                             "carregado na inicialização, o histórico \n"
                             "não será salvo ou carregado, e não irá \n"
                             "existir no menu principal."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            snprintf(s, len,
                     "Driver de vídeo atual.");

            if (string_is_equal(settings->arrays.video_driver, "gl"))
            {
                snprintf(s, len,
                         "Driver de vídeo OpenGL. \n"
                                 " \n"
                                 "Este driver permite utilizar núcleos \n"
                                 "Libretro GL, além de implementações \n"
                                 "de núcleo renderizados por software.\n"
                                 " \n"
                                 "O desempenho de implementações de núcleo \n"
                                 "Libretro GL ou renderizados por software \n"
                                 "é dependente do driver GL de sua placa de vídeo.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
            {
                snprintf(s, len,
                         "Driver de vídeo SDL 2.\n"
                                 " \n"
                                 "Este é um driver de vídeo SDL 2 renderizado \n"
                                 "por software.\n"
                                 " \n"
                                 "O desempenho para implementações de núcleo \n"
                                 "libretro renderizados por software é dependente \n"
                                 "da implementação SDL da sua plataforma.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
            {
                snprintf(s, len,
                         "Driver de vídeo SDL. \n"
                                 " \n"
                                 "Este é um driver de vídeo SDL 1.2 renderizado \n"
                                 "por software.\n"
                                 " \n"
                                 "O desempenho é considerado medíocre. \n"
                                 "Cosidere utilizar apenas como último recurso.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                         "Driver de vídeo Direct3D. \n"
                                 " \n"
                                 "O desempenho de núcleos renderizados por \n"
                                 "software depende do driver D3D de base da\n"
                                 "sua placa de vídeo).");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
            {
                snprintf(s, len,
                         "Driver de vídeo Exynos-G2D. \n"
                                 " \n"
                                 "Este é um driver de vídeo Exynos de baixo-nível. \n"
                                 "Utiliza o bloco G2D em SoCs Samsung Exynos \n"
                                 "para operações blit. \n"
                                 " \n"
                                 "O desempenho de núcleos renderizados por \n"
                                 "por hardware deve ser ótimo.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                         "Driver de vídeo Plain DRM. \n"
                                 " \n"
                                 "Este é um driver de vídeo de baixo-nível \n"
                                 "usando libdrm para escala por hardware \n"
                                 "utilizando sobreposições de GPU.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
            {
                snprintf(s, len,
                         "Driver de vídeo Sunxi-G2D. \n"
                                 " \n"
                                 "Este é um driver de vídeo Sunxi de baixo-nível. \n"
                                 "Utiliza o bloco G2D de SoCs Allwinner.");
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            snprintf(s, len,
                     "Plugin DSP de áudio. \n"
                             "Processa o áudio antes que seja \n"
                             "enviado para o driver."
            );
            break;
        case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                        "Implementação SINC em janelas.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                  strlcpy(s,
                        "Implementação de cossenos complicados.", len);
               else if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            }
            break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION: snprintf(s, len, "DEFINIR CRT");
			break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER: snprintf(s, len, "DEFINIR CRT SUPER");
			break;

        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
                     "Carregar Predefinição de Shader. \n"
                             " \n"
                             "Carregar uma predefinição de Shader \n"
                             "diretamente."
                             "O menu de Shader é atualizado adequadamente. \n"
                             " \n"
                             "Se o CGP usa métodos de escala que não sejam \n"
                             "simples, (e.x. escala da origem, mesmo fator de \n"
                             "escala para X/Y), o fator de escala exibido \n"
                             "no menu pode não estar correto."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
                     "Escala para este estágio. \n"
                             " \n"
                             "O fator de escala acumula, e.x. 2x para o \n"
                             "primeiro estágio e 2x para o segundo \n"
                             "estágio resulta numa escala total de 4x. \n"
                             " \n"
                             "Se houver um fator de escala para o último \n"
                             "estágio, o resultado é alongado na \n"
                             "tela com o filtro especificado em \n"
                             "'Filtro Padrão'. \n"
                             " \n"
                             "Se 'Não Importa' estiver definido, escala \n"
                             "de 1x ou alongar para tela cheia será \n"
                             "utilizado dependendo se é o último estágio \n"
                             "ou não."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
                     "Estágios de Shader. \n"
                             " \n"
                             "O RetroArch permite a você misturar e \n"
                             "combinar vários Shaders com um número \n"
                             "arbitrário de estágios de Shader, \n"
                             "com filtros de hardware personalizados \n"
                             "e fatores de escala.\n"
                             " \n"
                             "Esta opção especifica o número de estágios \n"
                             "de Shader a ser utilizado. Se você definir \n"
                             "isto como 0, e utilizar Aplicar Alterações \n"
                             "de Shader, você irá utilizar um Shader \n"
                             "'em branco'."
                             " \n"
                             "A opção Filtro Padrão terá efeito no \n"
                             "filtro de alongamento.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
                     "Parâmetros de Shader. \n"
                             " \n"
                             "Modifica diretamente o Shader atual. \n"
                             "Ele não será salvo no arquivo de \n"
                             "predefinição CGP/GLSLP.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            snprintf(s, len,
                     "Parâmetros de Predefinição de Shader. \n"
                             " \n"
                             "Modifica a predefinição de Shader \n"
                             "atualmente usada no menu."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
                     "Caminho para o Shader. \n"
                             " \n"
                             "Todos os Shaders devem ser do mesmo \n"
                             "tipo (e.x. CG, GLSL ou HLSL). \n"
                             " \n"
                             "Defina o Diretório de Shaders para estipular \n"
                             "onde o navegador de arquivos começa a busca \n"
                             "pelos Shaders."
            );
            break;
        case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            snprintf(s, len,
                     "Determina como arquivos de configuração \n"
                             "são carregados e priorizados.");
            break;
        case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            snprintf(s, len,
                     "Salvar configuração em disco ao sair. \n"
                             "Útil para o menu, já que as configurações \n"
                             "podem ser modificadas. \n"
                             "Sobrescreve a configuração. \n"
                             " \n"
                             "#inclusões e comentários não são \n"
                             "preservados. \n"
                             " \n"
                             "Por design, o arquivo de configuração é \n"
                             "considerado imutável já que é \n"
                             "provavelmente mantido pelo usuário, \n"
                             "e não deve ser sobrescrito \n"
                             "por trás das costas dele."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
            "\nContudo, este não é o caso em \n"
            "consoles, onde olhar o arquivo \n"
            "de configuração manualmente \n"
            "não é realmente uma opção."
#endif
            );
            break;
        case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
            snprintf(s, len, "Tem certeza que deseja sair?");
            break;
        case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            snprintf(s, len, "Exibir arquivos e \n"
                    "pastas ocultos.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            snprintf(s, len,
                     "Filtro de hardware para esse estágio. \n"
                             " \n"
                             "Se 'Não importa' estiver definido, \n"
                             "'Filtro Padrão' será utilizado."
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
                     "Salva automaticamente a SRAM \n"
                             "não volátil em um intervalo regular. \n"
                             " \n"
                             "Isso é desativado por padrão a menos \n"
                             "que seja definido de outra forma. \n"
                             "O intervalo é medido em segundos. \n"
                             " \n"
                             "O valor 0 desativa o salvamento automático.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Tipo de Dispositivo de Entrada. \n"
                             " \n"
                             "Escolhe qual tipo de dispositivo utilizar. \n"
                             "Isto é relevante para o núcleo Libretro."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Define o nível de registro de eventos dos \n"
                             "núcleos Libretro. (GET_LOG_INTERFACE) \n"
                             " \n"
                             " Se o nível do registro de evento emitido \n"
                             " pelo núcleo Libretro for abaixo do nível \n"
                             " libretro_log, ele é ignorado.\n"
                             " \n"
                             " Registros DEBUG são sempre ignorados a menos \n"
                             " que o modo verboso esteja ativado (--verbose).\n"
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
                     "Boxes de Estado de Jogo.\n"
                             " \n"
                             "Com o box definido em 0, o nome do \n"
                             "Estado de Jogo é *.state \n"
                             "(ou a definição da linha de comando) \n"
                             "Quando o box não for 0, o caminho será \n"
                             "(caminho)(n), onde (n) é o número do box.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Aplicar Alterações de Shader. \n"
                             " \n"
                             "Após alterar as configurações de Shader, \n"
                             "utilize essa opção para aplicar as alterações. \n"
                             " \n"
                             "Alterar as configurações de Shader é uma \n"
                             "operação relativamente trabalhosa, então \n"
                             "deve ser feita de modo explícito. \n"
                             " \n"
                             "Ao aplicar os Shaders, as configurações de \n"
                             "Shader do menu são salvas em um dos arquivos \n"
                             "temporários (menu.cgp ou menu.glslp) \n"
                             "e carregadas. \n"
                             "O arquivo permanece salvo no Diretório \n"
                             "de Shaders após o RetroArch encerrar."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Ver arquivos de shader para mudanças. \n"
                     " \n"
                     "Depois de salvar as alterações em um shader no disco, \n"
                     "ele será automaticamente recompilado \n"
                     "e aplicado ao conteúdo em execução."
            );
            break;
        case MENU_ENUM_LABEL_MENU_TOGGLE:
            snprintf(s, len,
                     "Alternar o menu.");
            break;
        case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                     "Alternar captura de Mouse.\n"
                             " \n"
                             "Quando o Mouse é capturado, o RetroArch oculta \n"
                             "o cursor do Mouse, e mantém o Mouse dentro \n"
                             "da janela para permitir que a entrada de Mouse \n"
                             "relativa funcione melhor.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Alternar o foco do jogo.\n"
                             " \n"
                             "Quando um jogo tem foco, o RetroArch irá \n"
                             "desabilitar as teclas de atalho e manter \n"
                             "o cursor do mouse dentro da janela.");
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
                     "Alternar pelas imagens de disco. Utilizado \n"
                             "após a ejeção. \n"
                             " \n"
                             "Concluído após alternar novamente a ejeção.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
            snprintf(s, len,
                  "Filtro de vídeo por CPU.");
#else
            snprintf(s, len,
                     "Filtro de vídeo por CPU.\n"
                             " \n"
                             "Caminho para uma biblioteca dinâmica.");
#endif
            break;
        case MENU_ENUM_LABEL_AUDIO_DEVICE:
            snprintf(s, len,
                     "Substitui o dispositivo de áudio padrão \n"
                             "utilizado pelo driver de áudio. \n"
                             "Isto depende do driver. \n"
#ifdef HAVE_ALSA
            " \n"
            "ALSA quer um dispositivo PCM."
#endif
#ifdef HAVE_OSS
            " \n"
            "OSS quer um caminho (ex: /dev/dsp)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK quer nomes de porta (ex: system:playback1 \n"
            ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
            " \n"
            "RSound quer um endereço IP para um \n"
            "servidor RSound."
#endif
            );
            break;
        case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                     "Alternar ejeção de disco.\n"
                             " \n"
                             "Usado para conteúdo em vários discos.");
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Habilitar outras teclas de atalho. \n"
                             " \n"
                             "Se essa tecla de atalho estiver vinculada \n"
                             "ao teclado ou botão / eixo de joypad, todas \n"
                             "as outras teclas de atalho serão desabilitadas \n"
                             "a menos que essa tecla de atalho também esteja \n"
                             "pressionada ao mesmo tempo. \n"
                             " \n"
                             "Isso é útil para implementações com foco \n"
                             "RETRO_KEYBOARD que consultam uma grande \n"
                             "parte do teclado, quando não é desejável \n"
                             "que as teclas de atalho atrapalhem.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Habilitar Voltar Atrás. \n"
                             " \n"
                             "Causa impacto no desempenho, por \n"
                             "isso vem desligado por padrão.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
            snprintf(s, len,
                     "Aplicar trapaça imediatamente após a alternância.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
            snprintf(s, len,
                     "Aplicar trapaças automaticamente quando o jogo for carregado.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            snprintf(s, len,
                     "Diretório de Núcleo. \n"
                             " \n"
                             "Um diretório aonde buscar por \n"
                             "implementações de núcleo Libretro.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            snprintf(s, len,
                     "Taxa de Atualização Automática.\n"
                             " \n"
                             "A precisa taxa de atualização do monitor (Hz). \n"
                             "É usado para calcular a taxa de entrada de áudio \n"
                             "com a fórmula: \n"
                             " \n"
                             "taxa_ent_áudio = taxa de ent. do jogo * \n"
                             "taxa de atualiz. / taxa de atualiz. do jogo \n"
                             " \n"
                             "Se a implementação não reportar um valor, o padrão \n"
                             "NTSC será usado para garantir compatibilidade\n"
                             " \n"
                             "O valor deve ficar próximo a 60 Hz para evitar grandes \n"
                             "alterações de tom. Se o seu monitor não rodar a 60Hz, \n"
                             "desabilite o VSync e deixe o padrão.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            snprintf(s, len,
                     "Definir Taxa de Atualização Encontrada\n"
                             " \n"
                            "Define a taxa de atualização para o valor\n"
                            "real pesquisado no driver de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Forçar uma rotação específica \n"
                             "da tela. \n"
                             " \n"
                             "A rotação é adicionada a rotação que o \n"
                             "núcleo Libretro define (consulte Permitir \n"
                             "Rotação).");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE:
            snprintf(s, len,
                     "Resolução em tela cheia.\n"
                             " \n"
                             "Resolução definida para 0 \n"
                             "usa a resolução do ambiente.\n");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Taxa de Avanço Rápido."
                             " \n"
                             "A taxa máxima na qual o conteúdo será \n"
                             "executado quando utilizado o Avanço Rápido. \n"
                             " \n"
                             "(ex: 5.0x para conteúdos em 60fps = 300 \n"
                             "fps máx). \n"
                             " \n"
                             "O RetroArch entra em 'sleep' para garantir \n"
                             "que a taxa máxima não seja excedida. \n"
                             "Não confie na precisão dessa limitação..");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Sincronizar com a Taxa de Quadros Exata do Conteúdo.\n"
                             " \n"
                             "Esta opção é o equivalente de forçar a velocidade x1\n"
                             "enquanto ainda permite o avanço rápido.\n"
                             "Sem desvio da taxa de atualização solicitada pelo núcleo\n"
                             "e sem Controle de Taxa Dinâmica do som).");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Dar preferência a qual monitor. \n"
                             " \n"
                             "0 (padrão) significa que nenhum é preferido \n"
                             "em particular, 1 e acima disso (1 sendo \n"
                             "o primeiro monitor), sugere ao RetroArch \n"
                             "para usar aquele monitor em especial.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Forçar o recorte de quadros em \n"
                             "overscan. \n"
                             " \n"
                             "O comportamento exato desta opção \n"
                             "é específico da implementação do núcleo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Apenas dimensiona o vídeo em valores \n"
                             "inteiros.\n"
                             " \n"
                             "O tamanho de base depende da geometria \n"
                             "relatada pelo sistema e da proporção de tela.\n"
                             " \n"
                             "Se a função 'Forçar Proporção' não estiver \n"
                             "definida, X / Y serão dimensionados em \n"
                             "valores inteiros independentemente.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Volume de áudio, expresso em dB.\n"
                             " \n"
                             "0dB é volume normal, nenhum ganho é aplicado. \n"
                             "O ganho pode ser controlado na execução com \n"
                             "as teclas de controle de volume.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Controle dinâmico de taxa de áudio. \n"
                             " \n"
                             "Definido para 0 desativa o controle de taxa. \n"
                             "Qualquer outro valor controla o delta \n"
                             "do ajuste de taxa.\n"
                             " \n"
                             "Define o quanto a taxa de entrada pode ser \n"
                             "ajustada dinamicamente. \n"
                             " \n"
                             " A taxa de entrada é definida como: \n"
                             "taxa de ent * (1.0 +/- (delta ajus de taxa))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Desvio máximo de tempo de áudio. \n"
                             " \n"
                             "Define a mudança máxima na taxa de saída \n"
                             "de áudio. Se aumentado habilita grandes \n"
                             "mudanças no sincronismo ao custo de \n"
                             "imprecisão na tonalidade de áudio \n"
                             "(ex: rodando núcleos PAL em modo NTSC). \n"
                             " \n"
                             "A taxa de entrada é definida como: \n"
                             "taxa de ent * (1.0 +/- (desv máx de tempo))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Mudar para a próxima sobreposição.\n"
                             " \n"
                             "Circula pela opções.");
            break;
        case MENU_ENUM_LABEL_LOG_VERBOSITY:
            snprintf(s, len,
                     "Habilitar ou desabilitar o nível \n"
                             "de verbosidade do frontend.");
            break;
        case MENU_ENUM_LABEL_VOLUME_UP:
            snprintf(s, len,
                     "Aumentar o volume de áudio.");
            break;
        case MENU_ENUM_LABEL_VOLUME_DOWN:
            snprintf(s, len,
                     "Diminuir o volume de áudio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Desligar a composição a força. \n"
                             "Válido somente no Windows Vista/7 por enquanto.");
            break;
        case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            snprintf(s, len,
                     "Habilitar ou desabilitar os contadores \n"
                             "de desempenho do frontend.");
            break;
        case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Sistema. \n"
                             " \n"
                             "Define o diretório de 'sistema'. \n"
                             "Os núcleos podem consultar este \n"
                             "diretório para carregar arquivos de BIOS, \n"
                             "configurações específicas do sistema, etc.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            snprintf(s, len,
                     "Cria automaticamente um Estado de Jogo \n"
                             "no final da execução do RetroArch. \n"
                             " \n"
                             "O RetroArch irá carregar automaticamente \n"
                             "qualquer Estado de Jogo com esse caminho \n"
                             "na inicialização se a função \n"
                             "'Autocarregar Estado de Jogo' \n"
                             "estiver habilitada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Usar vídeo paralelizado. \n"
                             " \n"
                             "Utilizar esta função pode melhorar o \n"
                             "desempenho ao custo de latência e mais \n"
                             "engasgamento de vídeo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_VSYNC:
            snprintf(s, len,
                     "V-Sync de vídeo.\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            snprintf(s, len,
                     "Tentar a sincronia rígida entre \n"
                             "CPU e GPU. \n"
                             " \n"
                             "Reduz a latência ao custo de \n"
                             "desempenho.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Níveis da Rebobinamento.\n"
                             " \n"
                             " Ao rebobinar o número definido de quadros, \n"
                             "você pode rebobinar vários quadros por vez, \n"
                             "aumentando a velocidade de rebobinamento.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            snprintf(s, len,
                     "Tamanho do Buffer da Rebobinamento (MB).\n"
                             " \n"
                             " A quantidade de memória em MB a ser reservada  \n"
                             "para  rebobinamento. Aumentar este valor aumenta  \n"
                             "o comprimento do histórico de rebobinagem.\n");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            snprintf(s, len,
                     "Tamanho do Intervalo de Ajuste do Buffer (MB).\n"
                             " \n"
                             " Cada vez que você aumentar ou diminuir o valor do \n"
                             "tamanho do buffer de rebobinamento por meio  \n"
                             "dessa interface, ele será alterado por esse valor.\n");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT:
            snprintf(s, len,
                     "Capturar tela.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Define quantos milissegundos atrasar \n"
                             "após o V-Sync antes de rodar o núcleo. \n"
                             "\n"
                             "Reduz a latência ao custo de maior \n"
                             "risco de engasgamento de vídeo. \n"
                             " \n"
                             "O máximo é 15.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_DELAY:
            snprintf(s, len,
                     "Define por quantos milissegundos o carregamento automático\n"
                             "de shaders será atrasado.\n"
                             "\n"
                             "Pode solucionar problemas gráficos devido ao uso de \n"
                             "software de 'captura de tela', como software de streaming.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "Define quantos quadros a CPU pode rodar \n"
                             "à frente da GPU quando utilizando o \n"
                             "recurso 'Sincronização Rígida de GPU'. \n"
                             " \n"
                             "O máximo é 3. \n"
                             " \n"
                             " 0: Sincroniza na GPU imediatamente. \n"
                             " 1: Sincroniza no quadro anterior. \n"
                             " 2: Etc...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Inserir um quadro opaco entre quadros.\n"
                             " \n"
                             "Útil para usuários com telas de 120Hz que \n"
                             "desejam jogar conteúdos em 60Hz \n"
                             "para eliminar efeito de sombra. \n"
                             " \n"
                             "A taxa de atualização de vídeo ainda deve \n"
                             "ser configurada como se fosse uma tela de \n"
                             "60Hz (dividir a taxa de atualização por 2).");
            break;
        case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            snprintf(s, len,
                     "Exibir a tela inicial no menu.\n"
                             "É automaticamente definido como falso após \n"
                             "o programa iniciar pela primeira vez. \n"
                             " \n"
                             "Somente é atualizado no arquivo de \n"
                             "configuração se 'Salvar Configuração ao Sair' \n"
                             "estiver habilitado.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
            snprintf(s, len, "Alternar tela cheia.");
            break;
        case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            snprintf(s, len,
                     "Bloqueia a SRAM de ser sobrescrita \n"
                             "ao carregar um Estado de Jogo. \n"
                             " \n"
                             "Pode causar problemas no jogo.");
            break;
        case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
            snprintf(s, len,
                     "Pausar o jogo quando o foco da janela \n"
                             "for perdido.");
            break;
        case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            snprintf(s, len,
                     "Capturas de tela com Shader \n"
                             "de GPU, se disponível.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Capturas de Tela. \n"
                             " \n"
                             "Diretório para armazenar as capturas de tela."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            snprintf(s, len,
                     "Intervalo de Troca da Sincronia Vertical \n"
                             "(V-Sync). \n"
                             " \n"
                             "Usa um intervalo de troca personalizado \n"
                             "para V-Sync. Defina para reduzir \n"
                             "efetivamente a taxa de atualização \n"
                             "do monitor pela metade.");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Jogos-Salvos. \n"
                             " \n"
                             "Salvar todos os Jogos-Salvos (*.srm) neste \n"
                             "diretório. Isso inclui arquivos relacionados, \n"
                             "como .bsv, .rt, .psrm, etc... \n"
                             " \n"
                             "Será substituído por opções definidas \n"
                             "explicitamente na linha de comando.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Estados de Jogo. \n"
                             " \n"
                             "Salvar todos os Estados de Jogo \n"
                             "(*.state) neste diretório. \n"
                             " \n"
                             "Será substituído por opções definidas \n"
                             "explicitamente na linha de comando.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Recursos. \n"
                             " \n"
                             "Esta localização é consultada por padrão \n"
                             "quando a interface do menu tenta procurar \n"
                             "por recursos carregáveis, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Planos de Fundo Dinâmicos. \n"
                             " \n"
                             "O local para armazenar planos de fundo \n"
                             "dinamicamente carregados pelo menu \n"
                             "dependendo do contexto.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            snprintf(s, len,
                     "Taxa de Câmera Lenta."
                             " \n"
                             "Quando está em Câmera Lenta, o conteúdo será \n"
                             "diminuído pelo fator especificado/definido.");
            break;
        case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            snprintf(s, len,
                     "Define a zona morta do controle analógico. \n"
                             " \n"
                             "Até que ponto um eixo deve ser \n"
                             "movido para resultar em um botão pressionado. \n"
                             "Os valores aceitos são entre [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            snprintf(s, len,
                     "Período do turbo.\n"
                             " \n"
                             "Descreve o período quando os botões \n"
                             "com turbo habilitado são alternados. \n"
                             " \n"
                             "Os números são descritos em quadros."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            snprintf(s, len,
                     "Ciclo de trabalho.\n"
                             " \n"
                             "Descreve quão longo deve ser o período \n"
                             "de um botão com turbo habilitado. \n"
                             " \n"
                             "Os números são descritos em quadros."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
            snprintf(s, len, "Habilitar suporte a toque.");
            break;
        case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
            snprintf(s, len, "Usar toque frontal ao invés de traseiro.");
            break;
        case MENU_ENUM_LABEL_MOUSE_ENABLE:
            snprintf(s, len, "Habilitar controle do menu por Mouse.");
            break;
        case MENU_ENUM_LABEL_POINTER_ENABLE:
            snprintf(s, len, "Habilitar controle do menu por toque.");
            break;
        case MENU_ENUM_LABEL_MENU_WALLPAPER:
            snprintf(s, len,
                    "Selecionar uma imagem para definir como \n"
                             "plano de fundo.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
            snprintf(s, len,
                     "Voltar ao início e/ou final \n"
                             "se o limite da lista for alcançado \n"
                             "horizontalmente ou verticalmente.");
            break;
        case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
            snprintf(s, len,
                     "Se desativado, o jogo continuará sendo \n"
                             "executado em segundo plano quando o \n"
                             "menu estiver ativo.");
            break;
        case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            snprintf(s, len,
                     "Desativar o protetor de tela. É uma sugestão \n"
                             "que não precisa necessariamente ser \n"
                             "honrada pelo driver de vídeo.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Modo cliente do Jogo em Rede para o usuário atual. \n"
                             "Será modo 'Servidor' se estiver desabilitado.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "A quantidade de atraso de quadros para \n"
                             "utilizar no Jogo em Rede. \n"
                             " \n"
                             "Aumentar este valor irá aumentar o \n"
                             "desempenho, mas introduz mais latência.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Define se os jogos de Jogo em Rede são anunciados \n"
                             "publicamente. \n"
                             " \n"
                             "Se definido como falso, os clientes \n"
                             "deverão conectar manualmente em vez \n"
                             "de usar o lobby público.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Define se o Jogo em Rede deve iniciar em \n"
                             "modo espectador. \n"
                             " \n"
                             "Se definido como verdadeiro, o Jogo em Rede estará \n"
                             "em modo espectador no começo. Sempre é possível \n"
                             "alterar o modo mais tarde.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Define se conexões em modo escravo são \n"
                             "permitidas. \n"
                             " \n"
                             "Clientes em modo escravo requerem muito pouco \n"
                             "poder de processamento em ambos os lados, mas \n"
                             "irão sofrer significamente da latência de rede.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Define se conexões que não estão \n"
                            "em modo escravo são proibidas. \n"
                             " \n"
                             "Não recomendado, exceto para redes muito \n"
                             "rápidas com máquinas muito lentas. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Define se o Jogo em Rede deve executar em um modo \n"
                             "que não utilize Estados de Jogo. \n"
                             " \n"
                             "Se definido como verdadeiro, uma rede muito \n"
                             "rápida é necessária, mas Voltar Atrás não é \n"
                             "permitido, então não haverá oscilação \n"
                             "no Jogo em Rede.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "Frequência em quadros na qual o Jogo em Rede \n"
                             "irá verificar se o anfitrião e o cliente \n"
                             "estão sincronizados. \n"
                             " \n"
                             "Na maioria dos núcleos, este valor não terá efeito \n"
                             "e pode ser ignorado. Com núcleos não determinísticos, \n"
                             "este valor define a frequência com que os pares\n"
                             "serão sincronizados. Em núcleos defeituosos, valores \n"
                             "diferentes de zero irão causar problemas de desempenho \n"
                             "Defina como zero para desativar verificações\n"
                             "Este valor só é usado no anfitrião do Jogo em Rede. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "O número de quadros de latência de entrada \n"
                     "que pode ser usado pelo Jogo em Rede para \n"
                     "mascarar a latência de rede. \n"
                     " \n"
                     "Quando usando Jogo em Rede, esta opção atrasa a \n"
                     "entrada local, de modo que o quadro em \n"
                     "execução fique mais próximo do quadro \n"
                     "recebido pela rede. Isso reduz a oscilação \n"
                     "e torna o Jogo em Rede menos intenso para a CPU, \n"
                     "mas ao custo de atraso perceptível na entrada. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "O intervalo de quadros de latência \n"
                     "de entrada que pode ser usado pelo Jogo em Rede \n"
                     "para mascarar a latência da rede. \n"
                     "\n"
                     "Se habilitado, o Jogo em Rede irá ajustar o número \n"
                     "de quadros de latência de entrada \n"
                     "dinamicamente para balancear tempo de CPU, \n"
                     "latência de entrada e latência de rede. Isso \n"
                     "reduz a oscilação e torna o Jogo em Rede menos intensivo \n"
                     "para a CPU, mas ao custo de atraso imprevisível \n"
                     "na entrada.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            snprintf(s, len,
                     "Quando hospedando, tentar escutar por \n"
                             "conexões da internet pública usando UPnP \n"
                             "ou tecnologias similares para escapar de LANs. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "Quando hospedando, transmitir a conexão \n"
                             "através de um servidor 'homem no meio' \n"
                             "para contornar problemas de firewall \n"
                             "ou NAT/UPnP. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            snprintf(s, len,
                     "Especifica o servidor homem no meio \n"
                             "para usar no jogo em rede. Um servidor localizado \n"
                             "mais perto de você pode ter menos latência. \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Máximo de imagens na cadeia de trocas. \n"
                             "Isto pode informar ao driver de vídeo para \n"
                             "usar um modo de buffer específico. \n"
                             " \n"
                             "Buffer único - 1\n"
                             "Buffer duplo - 2\n"
                             "Buffer triplo - 3\n"
                             " \n"
                             "Definir o modo de buffer correto pode \n"
                             "ter um grande impacto na latência.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            snprintf(s, len,
                     "Suaviza a imagem com filtragem bilinear. \n"
                             "Deve ser desativado se estiver usando Shaders.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            snprintf(s, len,
                     "Exibir data e/ou hora atuais dentro do menu.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_STYLE:
           snprintf(s, len,
              "Estilo para mostrar a data atual e/ou a hora.");
           break;
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Exibir o nível atual de bateria dentro do menu.");
            break;
        case MENU_ENUM_LABEL_CORE_ENABLE:
            snprintf(s, len,
                     "Exibir o nome do núcleo atual dentro do menu.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            snprintf(s, len,
                     "Habilitar o Jogo em Rede no \n"
                     "modo anfitrião (servidor).");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            snprintf(s, len,
                     "Habilitar o Jogo em Rede no modo cliente.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            snprintf(s, len,
                     "Desconectar de uma conexão de Jogo em Rede ativa.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Buscar por e conectar aos anfitriões \n"
                     "de Jogo em Rede na rede local.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Configurações relacionadas ao Jogo em Rede.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Carregar dinamicamente um novo plano de \n"
                             "fundo dependendo do contexto.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
            snprintf(s, len,
                     "URL para o diretório de atualização \n"
                             "de núcleos no buildbot do Libreto.");
            break;
        case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
            snprintf(s, len,
                     "URL para o diretório de atualizações \n"
                             "de recursos no buildbot do Libretro.");
            break;
        case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
            snprintf(s, len,
                     "Se habilitado, substitui os vínculos da \n"
                             "entrada com os vínculos remapeados \n"
                             "definidos para o núcleo atual.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Sobreposições. \n"
                             " \n"
                             "Define um diretório onde as sobreposições \n"
                             "são mantidas para fácil acesso.");
            break;
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY:
            snprintf(s, len,
                     "Diretório do esquema de vídeo. \n"
                             " \n"
                             "Define um diretório onde os esquemas de vídeo \n"
                             "são mantidos para facilitar o acesso.");
            break;
#endif
        case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            snprintf(s, len,
                     "Número máximo de usuários \n"
                             "suportados pelo RetroArch.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            snprintf(s, len,
                     "Após o download, extrair automaticamente \n"
                             "os arquivos contidos nos arquivos \n"
                             "comprimidos baixados.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            snprintf(s, len,
                     "Filtrar arquivos em exibição \n"
                             "por extensões suportadas.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            snprintf(s, len,
                     "O nome de usuário da pessoa executando \n"
                             "o RetroArch. \n"
                             "Será utilizado para jogos online.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "A porta do endereço de IP do anfitrião. \n"
                             "Pode ser ou uma porta TCP ou uma porta UDP.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Habilitar ou desabilitar o modo espectador \n"
                             "para o usuário durante o Jogo em Rede.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            snprintf(s, len,
                     "O endereço do anfitrião a se conectar.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "A senha para conectar ao anfitrião de Jogo em Rede \n"
                             "Utilizado apenas em modo anfitrião.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "A senha para conectar ao anfitrião \n"
                             "de Jogo em Rede apenas com privilégios de \n"
                             "espectador. Utilizado apenas em modo \n"
                             "anfitrião.");
            break;
        case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
            snprintf(s, len,
                     "Habilitar interface de comando stdin.");
            break;
        case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
            snprintf(s, len,
                     "Rodar o Companheiro de Interface de Usuário \n"
                             "na inicialização (se disponível).");
            break;
        case MENU_ENUM_LABEL_MENU_DRIVER:
            snprintf(s, len, "Driver de menu a ser utilizado.");
            break;
        case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            snprintf(s, len,
                     "Combinação de botões do Gamepad \n"
                             "para alternar o menu. \n"
                             " \n"
                             "0 - Nenhuma \n"
                             "1 - Pressionar L + R + Y + Direcional \n"
                             "para baixo simultaneamente. \n"
                             "2 - Pressionar L3 + R3 simultaneamente. \n"
                             "3 - Pressionar Start + Select simultaneamente.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len, "Permitir controlar o menu a qualquer usuário. \n"
                    " \n"
                    "Se desabilitado, apenas o Usuário 1 \n"
                    "poderá controlar o menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            snprintf(s, len,
                     "Habilitar a detecção automática de entrada. \n"
                             " \n"
                             "Vai tentar autoconfigurar \n"
                             "Joypads no estilo 'Plug-and-Play'.");
            break;
        case MENU_ENUM_LABEL_CAMERA_ALLOW:
            snprintf(s, len,
                     "Permitir ou negar acesso a câmera \n"
                             "pelos núcleos.");
            break;
        case MENU_ENUM_LABEL_LOCATION_ALLOW:
            snprintf(s, len,
                     "Permitir ou negar acesso aos \n"
                             "serviços de localização pelos núcleos.");
            break;
        case MENU_ENUM_LABEL_TURBO:
            snprintf(s, len,
                     "Habilitar turbo.\n"
                             " \n"
                             "Manter o turbo pressionado enquanto \n"
                             "pressiona outro botão faz com que o \n"
                             "botão entre em modo turbo, onde o estado \n"
                             "do botão é modulado com um sinal periódico. \n"
                             " \n"
                             "A modulação termina quando o botão em si \n"
                             "mesmo (não o botão de turbo) for liberado.");
            break;
        case MENU_ENUM_LABEL_OSK_ENABLE:
            snprintf(s, len,
                     "Habilitar/desabilitar teclado virtual.");
            break;
        case MENU_ENUM_LABEL_AUDIO_MUTE:
            snprintf(s, len,
                     "Áudio mudo/não-mudo.");
            break;
        case MENU_ENUM_LABEL_REWIND:
            snprintf(s, len,
                     "Manter o botão pressionado para \n"
                     "Voltar Atrás. \n"
                             " \n"
                             "Voltar Atrás precisa estar habilitado.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "Tecla para sair do RetroArch de modo limpo."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
                            "\nEncerrá-lo de forma brusca (SIGKILL, \n"
                            "etc) irá terminar sem salvar \n"
                            "RAM, etc. Em Unix-likes, \n"
                            "SIGINT/SIGTERM permitem \n"
                            "uma desinicialização limpa."
#endif
            );
            break;
        case MENU_ENUM_LABEL_LOAD_STATE:
            snprintf(s, len,
                     "Carregar Estado de Jogo.");
            break;
        case MENU_ENUM_LABEL_SAVE_STATE:
            snprintf(s, len,
                     "Salvar Estado de Jogo.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                     "Alternar modo jogador/espectador do Jogo em Rede.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
            snprintf(s, len,
                     "Aumentar o índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
            snprintf(s, len,
                     "Diminuir o índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_SHADER_PREV:
            snprintf(s, len,
                     "Aplicar o Shader anterior no diretório.");
            break;
        case MENU_ENUM_LABEL_SHADER_NEXT:
            snprintf(s, len,
                     "Aplicar o próximo Shader do diretório.");
            break;
        case MENU_ENUM_LABEL_RESET:
            snprintf(s, len,
                     "Reiniciar o conteúdo. \n");
            break;
        case MENU_ENUM_LABEL_PAUSE_TOGGLE:
            snprintf(s, len,
                     "Alternar estado pausado e não-pausado.");
            break;
        case MENU_ENUM_LABEL_CHEAT_TOGGLE:
            snprintf(s, len,
                     "Alternar índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_CHEAT_IDX:
            snprintf(s, len,
                     "Posição do índice na lista.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
            snprintf(s, len,
                     "Bitmask o endereço quando o tamanho da busca da memória for < 8-bit.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
            snprintf(s, len,
                     "O número de vezes que a trapaça será aplicada.\nUse com as outras duas opções de iteração para afetar grandes áreas de memória.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
            snprintf(s, len,
                     "Após cada 'Número de Iterações', o Endereço de Memória será aumentado por este número multiplicado pelo 'Tamanho da Pesquisa na Memória'.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
            snprintf(s, len,
                     "Após cada 'Número de Iterações', o Valor será aumentado por esse valor.");
            break;
        case MENU_ENUM_LABEL_CHEAT_MATCH_IDX:
            snprintf(s, len,
                     "Selecionar a coincidência para visualizar.");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            snprintf(s, len,
                     "Verificar a memória para criar novas trapaças");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
            snprintf(s, len,
                     "Esquerda/Direita para alterar o tamanho do bit\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
            snprintf(s, len,
                     "Esquerda/Direita para alterar o valor\n");
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
                     "Esquerda/Direita para alterar o valor\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
            snprintf(s, len,
                     "Esquerda/Direita para alterar o valor\n");
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
                     "Manter pressionado para Avanço Rápido \n"
                             "Soltar o botão desativa o Avanço Rápido.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
            snprintf(s, len,
                     "Manter pressionado para Câmera Lenta.");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "Avanço de quadro quando o conteúdo \n"
                     "estiver pausado.");
            break;
        case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
            snprintf(s, len,
                     "Alternar entre gravando ou não.");
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
                     "Eixos para controle analógico (tipo DualShock). \n"
                             " \n"
                             "Vinculado como de costume, contudo, \n"
                             "se um eixo analógico real \n"
                             "estiver vinculado, ele pode ser lido \n"
                             "como um analógico verdadeiro. \n"
                             " \n"
                             "Eixo X positivo é direita. \n"
                             "Eixo Y positivo é para baixo.");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "O RetroArch sozinho não faz nada. \n"
                            " \n"
                            "Para fazê-lo funcionar, você precisa \n"
                            "carregar um programa nele. \n"
                            "\n"
                            "Nós chamamos esses programas de \n"
                            "'Núcleo Libretro', ou 'núcleo' para encurtar. \n"
                            " \n"
                            "Para carregar um núcleo, selecione um \n"
                            "em 'Carregar Núcleo'.\n"
                            " \n"
#ifdef HAVE_NETWORKING
                    "Você pode obter núcleos de várias formas: \n"
                    "* Baixando-os, indo em \n"
                    "'%s' -> '%s'.\n"
                    "* Movendo-os manualmente para \n"
                    "'%s'.",
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                            "Você pode obter núcleos \n"
                            "movendo-os manualmente para \n"
                            "'%s'.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
            snprintf(s, len,
                     "Você pode alterar a sobreposição de \n"
                             "gamepad virtual indo em \n"
                             "'%s' -> '%s'. \n"
                             " \n"
                             "De lá você pode alterar a sobreposição, \n"
                             "mudar o tamanho e opacidade dos botões, etc. \n"
                             " \n"
                             "OBS: Por padrão, as sobreposições de gamepad \n"
                             "virtual ficam ocultas quando dentro do menu. \n"
                             "Se você quiser alterar este comportamento, \n"
                             "você pode definir '%s' como falso.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "Ativa uma cor de fundo para a exibição da tela (OSD).");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "Define o valor vermelho da cor de fundo da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "Define o valor verde da cor de fundo da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "Define o valor azul da cor de fundo da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "Define a opacidade da cor de fundo da exibição da tela (OSD). Valores válidos estão entre 0.0 e 1.0.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "Define o valor vermelho da cor do texto da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "Define o valor verde da cor do texto da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "Define o valor azul da cor do texto da exibição da tela (OSD). Valores válidos estão entre 0 e 255.");
            break;
        case MENU_ENUM_LABEL_MIDI_DRIVER:
            snprintf(s, len,
                     "Driver MIDI a ser utilizado.");
            break;
        case MENU_ENUM_LABEL_MIDI_INPUT:
            snprintf(s, len,
                     "Define o dispositivo de entrada (driver específico).\n"
                     "Quando definido para \"Off\", a entrada MIDI será desativada.\n"
                     "O nome do dispositivo também pode ser digitado.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Define o dispositivo de saída (driver específico).\n"
                     "Quando definido para \"Off\", a saída MIDI será desativada.\n"
                     "O nome do dispositivo também pode ser digitado.\n"
                     " \n"
                     "Quando a saída MIDI é ativada e o núcleo e o jogo suportam a saída MIDI,,\n"
                     "Quando a saída MIDI é ativada e o núcleo e o jogo suportam a saída MIDI,\n"
                     "No caso do driver MIDI \"null\", isso significa que esses sons não serão audíveis.");
            break;
        case MENU_ENUM_LABEL_MIDI_VOLUME:
            snprintf(s, len,
                     "Define o volume principal do dispositivo de saída.");
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
static const char *menu_hash_to_str_pt_br_label_enum(enum msg_hash_enums msg)
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
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_pt_br(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_pt_br_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_pt_br.h"
        default:
#if 0
            RARCH_LOG("Unimplemented: [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }

    return "null";
}

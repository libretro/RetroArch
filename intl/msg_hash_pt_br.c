/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include "../configuration.h"
#include "../verbosity.h"

int menu_hash_get_help_pt_br_enum(enum msg_hash_enums msg, char *s, size_t len) {
    uint32_t driver_hash = 0;
    settings_t *settings = config_get_ptr();

    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN) {
        unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

        switch (idx) {
            case RARCH_FAST_FORWARD_KEY:
                snprintf(s, len,
                         "Alterna entre Avanço Rápido e \n"
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
            case RARCH_PAUSE_TOGGLE:
                snprintf(s, len,
                         "Alterna entre estado pausado e não-pausado.");
                break;
            case RARCH_FRAMEADVANCE:
                snprintf(s, len,
                         "Avanço de quadro quando o conteúdo estiver pausado.");
                break;
            case RARCH_SHADER_NEXT:
                snprintf(s, len,
                         "Aplica o próximo Shader no diretório.");
                break;
            case RARCH_SHADER_PREV:
                snprintf(s, len,
                         "Aplica o Shader anterior no diretório.");
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
                         "Captura a tela.");
                break;
            case RARCH_MUTE:
                snprintf(s, len,
                         "Áudio mudo/não-mudo.");
                break;
            case RARCH_OSK:
                snprintf(s, len,
                         "Alternar o teclado virtual.");
                break;
            case RARCH_NETPLAY_FLIP:
                snprintf(s, len,
                         "Inverte usuários do Netplay.");
                break;
            case RARCH_NETPLAY_GAME_WATCH:
                snprintf(s, len,
                         "Alterna modos jogador/espectador do Netplay.");
                break;
            case RARCH_SLOWMOTION:
                snprintf(s, len,
                         "Manter pressionado para Câmera Lenta.");
                break;
            case RARCH_ENABLE_HOTKEY:
                snprintf(s, len,
                     "Habilitar outras teclas de atalho. \n"
                             " \n"
                             "Se essa tecla de atalho estiver vinculada ao teclado, \n"
                             "botão ou eixo de joypad, todas as outras teclas de atalho \n"
                             "serão desabilitadas a menos que essa tecla de atalho também \n"
                             "esteja pressionada ao mesmo tempo. \n"
                             " \n"
                             "Isso é útil para implementações com foco RETRO_KEYBOARD \n"
                             "que consultam uma grande parte do teclado, \n"
                             "quando não é desejável que as teclas de atalho \n"
                             "atrapalhem.");
                break;
            case RARCH_VOLUME_UP:
                snprintf(s, len,
                         "Aumenta o volume do áudio.");
                break;
            case RARCH_VOLUME_DOWN:
                snprintf(s, len,
                         "Diminui o volume do áudio.");
                break;
            case RARCH_OVERLAY_NEXT:
                snprintf(s, len,
                         "Muda para a próxima Transparência. Circula pelas opções.");
                break;
            case RARCH_DISK_EJECT_TOGGLE:
                snprintf(s, len,
                         "Alterna ejeção de disco. \n"
                                 " \n"
                                 "Usado para conteúdo em vários discos. ");
                break;
            case RARCH_DISK_NEXT:
            case RARCH_DISK_PREV:
                snprintf(s, len,
                         "Alterna pelas imagens de disco. Utilizado. \n"
                                 "após a ejeção. \n"
                                 " \n"
                                 "Concluído após alternar novamente a ejeção.");
                break;
            case RARCH_GRAB_MOUSE_TOGGLE:
                snprintf(s, len,
                         "Alterna captura de Mouse. \n"
                                 " \n"
                                 "Quando o Mouse é capturado, o RetroArch oculta \n"
                                 "o cursor do Mouse, e mantém o Mouse dentro \n"
                                 "da janela para permitir que a entrada de Mouse \n"
                                 "relativa funcione melhor.");
                break;
            case RARCH_GAME_FOCUS_TOGGLE:
                snprintf(s, len,
                         "Alterna o foco do jogo.\n"
                                 " \n"
                                 "Quando um jogo tem foco, o RetroArch irá desabilitar \n"
                                 "as teclas de atalho e manter o cursor do mouse dentro da janela.");
                break;
            case RARCH_MENU_TOGGLE:
                snprintf(s, len, "Alterna o menu.");
                break;
            case RARCH_LOAD_STATE_KEY:
                snprintf(s, len,
                         "Carrega Estado de Jogo.");
                break;
            case RARCH_FULLSCREEN_TOGGLE_KEY:
                snprintf(s, len,
                         "Alterna tela cheia.");
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
                         "Boxes de Estado de Jogo. \n"
                                 " \n"
                                 "Com o box definido em 0, o nome do Estado de Jogo \n"
                                 "é *.state (ou a definição da linha de comando). \n"
                                 " \n"
                                 "Quando o box não for 0, o caminho será <caminho><n>, \n"
                                 "onde <n> é o número do box.");
                break;
            case RARCH_SAVE_STATE_KEY:
                snprintf(s, len,
                         "Salva Estado de Jogo.");
                break;
            case RARCH_REWIND:
                snprintf(s, len,
                         "Manter o botão pressionado para Voltar Atrás. \n"
                                 " \n"
                                 "Voltar Atrás precisa estar ativado.");
                break;
            case RARCH_MOVIE_RECORD_TOGGLE:
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

    switch (msg) {
        case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            snprintf(s, len, "Detalhes de acesso para \n"
                    "sua conta Retro Achievements. \n"
                    " \n"
                    "Visite retroachievements.org e inscreva-se \n"
                    "em uma conta gratuita. \n"
                    " \n"
                    "Após o registro, você precisa \n"
                    "cadastrar o nome de usuário e a senha \n"
                    "no RetroArch.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len, "Nome de usuário para a sua conta Retro Achievements.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len, "Senha para a sua conta Retro Achievements.");
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
            snprintf(s, len, "Muda a fonte que é utilizada \n"
                    "para o texto da Exibição em Tela.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "Carregar opções de núcleo específicas do conteúdo automaticamente.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Carregar configurações de redefinição automaticamente.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Carregar arquivos de remapeamento de entrada automaticamente.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Colocar Estados de Jogo em pastas \n"
                    "com o nome do núcleo Libretro utilizado.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Colocar Jogos-Salvos em pastas \n"
                    "com o nome do núcleo Libretro utilizado.");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Sai do menu e retorna \n"
                    "ao conteúdo.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "Reinicia o conteúdo do começo.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "Fecha e descarrega o conteúdo \n"
                    "da memória.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Se um Estado de Jogo foi carregado, o conteúdo \n"
                    "irá retornar ao estado anterior ao carregamento.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Se um Estado de Jogo for sobrescrito, ele irá \n"
                    "retornar ao Estado de Jogo anterior.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Captura a tela. \n"
                    " \n"
                    "As imagens capturadas serão armazenadas dentro \n"
                    "do Diretório de Captura de Telas.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "Iniciar o conteúdo.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "Exibe informação adicional \n"
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
                     "Selecione para escanear o diretório atual \n"
                             "por conteúdo.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Selecione-o para definir esse como o diretório.");
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
                     "Arquivo de Transparência.");
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
                             "Selecione-o para abrir este arquivo \n"
                             "com o reprodutor de vídeo");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Música. \n"
                             " \n"
                             "Selecione-o para abrir este arquivo \n"
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
                             "Selecione-o para abrir este arquivo \n"
                             "com o visualizador de imagens.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Núcleo Libretro. \n"
                             " \n"
                             "Selecionar esta opção associa este núcleo \n"
                             "com o jogo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Núcleo Libretro. \n"
                             " \n"
                             "Selecione este arquivo para o RetroArch carregar este núcleo.");
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
                             "Define o diretório inicial para o navegador de arquivos do menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influencia como a chamada seletiva de entrada \n"
                             "é feita dentro do RetroArch. \n"
                             " \n"
                             "Mais cedo  - A chamada da entrada é realizada antes \n"
                             "do quadro ser processado. \n"
                             "Normal     - A chamada da entrada é realizada \n"
                             "quando a chamada é requisitada. \n"
                             "Mais tarde - A chamada seletiva é realizada no \n"
                             "primeiro estado de entrada requisitado pelo quadro.\n"
                             " \n"
                             "Definir como 'Cedo' ou 'Tarde' pode resultar \n"
                             "em menos latência, \n"
                             "dependendo da sua configuração.\n\n"
                             "Será ignorado no Netplay."
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
                             "Utilizado para calcular uma taxa de entrada de áudio adequada.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            snprintf(s, len,
                     "Desabilita de forma forçada o suporte sRGB FBO. Some Intel \n"
                             "Alguns drivers Intel OpenGL no Windows possuem problemas \n"
                             "de vídeo com o suporte sRGB FBO se estiver habilitado.");
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
                     "Permitir que os núcleos definam a rotação. Se for falso, \n"
                             "pedidos de rotação são honrados, porém ignorados .\n"
                             "Útil para configurações onde se rotaciona \n"
                             "manualmente a tela.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
            snprintf(s, len,
                     "Exibir os descritores de entrada definidos pelo \n"
                             "núcleo em vez dos padrões.");
            break;
        case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            snprintf(s, len,
                     "Número de registros para manter na \n"
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
                     "Automaticamente aumenta o índice de box a cada salvamento, \n"
                             "gerando vários arquivos de Estado de Jogo. \n"
                             "Quando o conteúdo for carregado, o box de Estado de Jogo será \n"
                             "o do valor existente mais alto (último Estado de Jogo).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Habilita a exibição de quadros por \n"
                             "segundo atual (FPS).");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            snprintf(s, len,
                     "Exibir e/ou ocultar mensagens na tela.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Deslocamento da onde as mensagens serão colocadas \n"
                             "na tela. Os valores são na faixa de [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Habilitar ou desabilitar a Transparência atual.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Ocultar a Transparência atual de aparecer \n"
                             "dentro do menu.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Caminho para Transparência de entrada.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Opacidade da Transparência.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Tempo limite para vínculo de entrada (em segundos). \n"
                             "Quantos segundos aguardar até prosseguir \n"
                             "para o próximo vínculo.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_SCALE:
            snprintf(s, len,
                     "Escala da Transparência.");
            break;
        case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            snprintf(s, len,
                     "Taxa de amostragem da saída de áudio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            snprintf(s, len,
                     "Defina como verdadeiro se os núcleos renderizados por \n"
                             "hardware devem ter seu próprio contexto privado. \n"
                             "Evita ter que assumir mudanças de \n"
                             "estado de hardware entre quadros."
            );
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Carregar Núcleo. \n"
                             " \n"
                             "Browse for a libretro core \n"
                             "implementation. Where the browser \n"
                             "starts depends on your Core Directory \n"
                             "path. If blank, it will start in root. \n"
                             " \n"
                             "If Core Directory is a directory, the menu \n"
                             "will use that as top folder. If Core \n"
                             "Directory is a full path, it will start \n"
                             "in the folder where the file is.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "Você pode utilizar os seguintes controles abaixo \n"
                             "tanto no seu Gamepad quanto no teclado a fim \n"
                             "de controlar o menu: \n"
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
                            "sincronização de áudio/video aonde ele precisa \n"
                            "ser calibrado pela taxa de atualização da sua \n"
                            "tela para um melhor resultado no desempenho.\n"
                            " \n"
                            "Se você experimentar qualquer estalido no áudio \n"
                            "ou rasgo de vídeo, normalmente isto significa que você \n"
                            "precisa calibrar as configurações. Algumas escolhas abaixo:\n"
                            " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) Vá para '%s' -> '%s', e habilite \n"
                             "'Video Paralelizado'. A taxa de atualização não \n"
                             "vai importar neste modo, a taxa de quadros será maior, \n"
                             "mas o vídeo será menos fluído. \n"
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
                     "Para escanear por conteúdo, vá para '%s' e \n"
                             "selecione '%s' ou '%s'. \n"
                             " \n"
                             "Os arquivos serão comparados com registros da base de dados.\n"
                             "Se houver uma correspondência, um registro será \n"
                             "adicionado a uma coleção. \n"
                             " \n"
                             "Você poderá então acessar facilmente este conteúdo \n"
                             "indo até '%s' ->\n"
                             "'%s'\n"
                             "em vez de ter que utilizar o \n"
                             "navegador de arquivos todas as vezes.\n"
                             " \n"
                             "OBS: Conteúdo para alguns núcleos pode ainda \n"
                             "não ser reconhecido.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST)
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
            if (settings)
                driver_hash = msg_hash_calculate(settings->arrays.input_driver);

            switch (driver_hash) {
                case MENU_LABEL_INPUT_DRIVER_UDEV:
                    snprintf(s, len,
                             "Driver de entrada udev. \n"
                                     " \n"
                                     "Utiliza a recente API evdev joypad \n"
                                     "para suporte a Joystick. Suporta \n"
                                     "hotplugging e force feedback. \n"
                                     " \n"
                                     "O driver lê eventos evdev para suporte \n"
                                     "a teclado. Tambêm suporta keyboard callback, \n"
                                     "Mouses e Touchpads. \n"
                                     " \n"
                                     "Por padrão na maioria das distros, nós /dev/input \n"
                                     "são somente root (mode 600). Você pode criar uma \n"
                                     "regra udev para torná-los acessíveis para não root."
                    );
                    break;
                case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
                    snprintf(s, len,
                             "Driver de entrada linuxraw. \n"
                                     " \n"
                                     "Este driver requer um TTY ativo. Eventos de \n"
                                     "teclado são lidos diretamente do TTY o que \n"
                                     "o torna simples, mas não tão flexível, quanto udev. \n"
                                     "Mouses, etc, não são suportados de nenhum modo. \n"
                                     " \n"
                                     "Este driver utiliza a antiga API de Joystick \n"
                                     "(/dev/input/js*).");
                    break;
                default:
                    snprintf(s, len,
                             "Driver de entrada.\n"
                                     " \n"
                                     "Dependendo do driver de vídeo, pode \n"
                                     "forçar um driver de entrada diferente.");
                    break;
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "Carregar Conteúdo. \n"
                             "Navegar por conteúdo. \n"
                             " \n"
                             "Para carregar conteúdo, você precisa de \n"
                             "um 'Núcleo' para utilizar, e um arquivo de conteúdo. \n"
                             " \n"
                             "Para controlar aonde o menu inicia \n"
                             "a procura pelo conteúdo, configure \n"
                             "'Diretório do Navegador de Arquivos'. \n"
                             "Se não estiver configurado, ele irá iniciar na raiz. \n"
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
                             "diretório do arquivo de configuração do RetroArch. \n"
                             "Se nenhum arquivo de configuração foi carregado  \n"
                             "na inicialização, o histórico não será salvo ou \n"
                             "carregado, e não irá existir no menu principal."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            snprintf(s, len,
                     "Driver de vídeo atual.");

            if (string_is_equal_fast(settings->arrays.video_driver, "gl", 2))
            {
                snprintf(s, len,
                         "Driver de vídeo OpenGL. \n"
                                 " \n"
                                 "Este driver permite utilizar núcleos Libretro GL, \n"
                                 "além de implementações de núcleo renderizados \n"
                                 "por software.\n"
                                 " \n"
                                 "O desempenho de implementações de núcleo \n"
                                 "Libretro GL ou renderizados por software \n"
                                 "é dependente do driver GL de sua placa de vídeo.");
            }
            else if (string_is_equal_fast(settings->arrays.video_driver, "sdl2", 4))
            {
                snprintf(s, len,
                         "Driver de vídeo SDL 2.\n"
                                 " \n"
                                 "Este é um driver de vídeo SDL 2 renderizado por \n"
                                 "software.\n"
                                 " \n"
                                 "O desempenho para implementações de núcleo \n"
                                 "Libretro renderizados por software é dependente \n"
                                 "da implementação SDL da sua plataforma.");
            }
            else if (string_is_equal_fast(settings->arrays.video_driver, "sdl1", 4))
            {
                snprintf(s, len,
                         "Driver de vídeo SDL. \n"
                                 " \n"
                                 "Este é um driver de vídeo SDL 1.2 renderizado por \n"
                                 "software.\n"
                                 " \n"
                                 "O desempenho é considerado medíocre. \n"
                                 "Cosidere utilizar apenas como último recurso.");
            }
            else if (string_is_equal_fast(settings->arrays.video_driver, "d3d", 3))
            {
                snprintf(s, len,
                         "Driver de vídeo Direct3D. \n"
                                 " \n"
                                 "O desempenho de núcleos renderizados por \n"
                                 "software depende do driver D3D de base da\n"
                                 "sua placa de vídeo).");
            }
            else if (string_is_equal_fast(settings->arrays.video_driver, "exynos", 6))
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
            else if (string_is_equal_fast(settings->arrays.video_driver, "drm", 3))
            {
                snprintf(s, len,
                         "Driver de vídeo Plain DRM. \n"
                                 " \n"
                                 "Este é um driver de vídeo de baixo-nível \n"
                                 "usando libdrm para escala por hardware utilizando \n"
                                 "Transparências de GPU.");
            }
            else if (string_is_equal_fast(settings->arrays.video_driver, "sunxi", 5))
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
            if (settings)
                driver_hash = msg_hash_calculate(settings->arrays.audio_resampler);

            switch (driver_hash) {
                case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
                    snprintf(s, len,
                             "Implementação SINC windowed.");
                    break;
                case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
                    snprintf(s, len,
                             "Implementação Convoluted Cosine.");
                    break;
                default:
                    if (string_is_empty(s))
                        strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
                    break;
            }
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
                     "Carregar Predefinição de Shader. \n"
                             " \n"
                             "Carregar uma predefinição de Shader diretamente. \n"
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
                             "O fator de escala acumula, e.x. 2x \n"
                             "para o primeiro estágio e 2x para o segundo estágio \n"
                             "resulta numa escala total de 4x. \n"
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
                             "O RetroArch permite a você misturar e combinar vários \n"
                             "Shaders com um número arbitrário de estágios de Shader, com \n"
                             "filtros de hardware personalizados e fatores de escala. \n"
                             " \n"
                             "Esta opção especifica o número de estágios de \n"
                             "Shader a ser utilizado. Se você definir isto em 0, e utilizar \n"
                             "Aplicar Alterações de Shader, você irá utilizar um Shader 'em branco'. \n"
                             " \n"
                             "A opção Filtro Padrão terá efeito no \n"
                             "filtro de alongamento.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
                     "Parâmetros de Shader. \n"
                             " \n"
                             "Modifica diretamente o Shader atual. Ele não \n"
                             "será salvo no arquivo de predefinição CGP/GLSLP.");
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
                             "Defina o Diretório de Shaders para estipular onde \n"
                             "o navegador de arquivos começa a busca pelos \n"
                             "Shaders."
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
                             "Útil para o menu já que as configurações \n"
                             "podem ser modificadas. Sobrescreve a configuração.\n"
                             " \n"
                             "#inclusões e comentários não são \n"
                             "preservados. \n"
                             " \n"
                             "Por esquema, o arquivo de configuração é \n"
                             "considerado imutável já que é \n"
                             "provavelmente mantido pelo usuário, \n"
                             "e não deve ser sobrescrito \n"
                             "por trás das costas do usuário."
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
                             "Se 'Não importa' estiver definido, 'Filtro \n"
                             "Padrão' será utilizado."
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
                             "Um valor de 0 desativa o salvamento automático.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Tipo de Dispositivo de Entrada. \n"
                             " \n"
                             "Escolhe qual tipo de dispositivo utilizar. Isto é \n"
                             "relevante para o núcleo Libretro em si."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Define o nível de registro de eventos dos núcleos Libretro \n"
                             "(GET_LOG_INTERFACE). \n"
                             " \n"
                             " Se o nível de registro de eventos emitido por um \n"
                             " núcleo Libretro for abaixo do nível libretro_log, \n"
                             " ele é ignorado.\n"
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
                             "Com o box definido em 0, o nome do Estado de Jogo \n"
                             "é *.state (ou a definição da linha de comando). \n"
                             "Quando o box não for 0, o caminho será (caminho)(n), \n"
                             "onde (n) é o número do box.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Aplicar Alterações de Shader. \n"
                             " \n"
                             "Após alterar as configurações de Shader, \n"
                             "utilize essa opção para aplicar as alterações. \n"
                             " \n"
                             "Alterar as configurações de Shader é uma operação \n"
                             "relativamente trabalhosa, então deve ser feita \n"
                             "de modo explícito. \n"
                             " \n"
                             "Ao aplicar os Shaders, as configurações de Shader \n"
                             "do menu são salvas em um dos arquivos temporários \n"
                             "(menu.cgp ou menu.glslp) e carregadas. O arquivo \n"
                             "permanece após o RetroArch encerrar. O arquivo é \n"
                             "salvo no Diretório de Shaders."
            );
            break;
        case MENU_ENUM_LABEL_MENU_TOGGLE:
            snprintf(s, len,
                     "Alterna o menu.");
            break;
        case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                     "Alterna captura de Mouse.\n"
                             " \n"
                             "Quando o Mouse é capturado, o RetroArch oculta \n"
                             "o cursor do Mouse, e mantém o Mouse dentro \n"
                             "da janela para permitir que a entrada de Mouse \n"
                             "relativa funcione melhor.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Alterna o foco do jogo.\n"
                             " \n"
                             "Quando um jogo tem foco, o RetroArch irá desabilitar \n"
                             "as teclas de atalho e manter o cursor do mouse dentro da janela.");
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
                     "Alterna pelas imagens de disco. Utilizado \n"
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
            "OSS quer um caminho (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK quer nomes de porta (e.g. system:playback1 \n"
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
                     "Alterna ejeção de disco.\n"
                             " \n"
                             "Usado para conteúdo em vários discos.");
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Habilitar outras teclas de atalho. \n"
                             " \n"
                             "Se essa tecla de atalho estiver vinculada ao teclado, \n"
                             "botão ou eixo de joypad, todas as outras teclas de atalho \n"
                             "serão desabilitadas a menos que essa tecla de atalho também \n"
                             "esteja pressionada ao mesmo tempo. \n"
                             " \n"
                             "Isso é útil para implementações com foco RETRO_KEYBOARD \n"
                             "que consultam uma grande parte do teclado, \n"
                             "quando não é desejável que as teclas de atalho \n"
                             "atrapalhem.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Habilitar Voltar Atrás. \n"
                             " \n"
                             "Causa impacto no desempenho, \n"
                             "por vem desligado por padrão.");
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
                     "Taxa de Atualização Automática. \n"
                             " \n"
                             "A taxa de atualização precisa da tela (Hz). \n"
                             "É utilizado para calcular a taxa de entrada \n"
                             "de áudio com a fórmula: \n"
                             " \n"
                             "taxa de entrada de áudio = taxa de entrada do jogo * taxa \n"
                             "de atualização da tela / taxa de atualização do jogo \n"
                             " \n"
                             "Se a implementação não reportar nenhum \n"
                             "valor, os padrões NTSC serão usados para \n"
                             "garantir compatibilidade.\n"
                             " \n"
                             "Este valor deve ficar próximo de 60Hz para evitar \n"
                             "mudanças bruscas de timbre de som. Se seu monitor não \n"
                             "roda a 60Hz, ou próximo disso, \n"
                             "desative o VSync, e deixe este valor no padrão.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Força uma rotação específica \n"
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
                             "Resolução como 0 usa a \n"
                             "resolução do ambiente.\n");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Taxa de Avanço Rápido."
                             " \n"
                             "A taxa máxima na qual o conteúdo será \n"
                             "executado quando utilizado o Avanço Rápido. \n"
                             " \n"
                             " (ex: 5.0x para conteúdos em 60fps = 300 fps \n"
                             "max). \n"
                             " \n"
                             "O RetroArch entra em 'sleep' para garantir \n"
                             "que a taxa máxima não seja excedida. \n"
                             "Não confie na precisão dessa limitação.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Dar preferência a qual monitor.\n"
                             " \n"
                             "0 (padrão) significa que nenhum é preferido \n"
                             "em particular, 1 e acima disso (1 sendo o primeiro \n"
                             "monitor), sugere ao RetroArch para usar aquele \n"
                             "monitor em especial.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Força o recorte de quadros em \n"
                             "overscan. \n"
                             " \n"
                             "O comportamento exato desta opção \n"
                             "é específico da implementação do núcleo.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Apenas escalona o vídeo em valores \n"
                             "inteiros.\n"
                             " \n"
                             "O tamanho de base depende da geometria \n"
                             "relatada pelo sistema e da proporção de tela.\n"
                             " \n"
                             "Se a função 'Forçar Aspecto' não estiver definida, \n"
                             "X / Y serão integralmente escalonados independentemente.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Volume de áudio, expresso em dB.\n"
                             " \n"
                             " 0dB é o volume normal, nenhum ganho é aplicado. \n"
                             "O ganho pode ser controlado na execução com \n"
                             "as teclas de controle de volume.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Controle dinâmico de taxa de áudio. \n"
                             " \n"
                             "Definido como 0 desativa o controle de taxa. \n"
                             "Qualquer outro valor controla o delta \n"
                             "do controle de taxa.\n"
                             " \n"
                             "Define o quanto a taxa de entrada pode ser \n"
                             "ajustada dinamicamente. \n"
                             " \n"
                             " A taxa de entrada é definida como: \n"
                             " taxa de entrada * (1.0 +/- (delta do controle de taxa))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Desvio máximo de tempo de áudio.\n"
                             " \n"
                             "Define a mudança máxima na taxa de saída de áudio. \n"
                             "Se aumentado habilita grandes mudanças \n"
                             "no sincronismo ao custo de imprecisão na \n"
                             "tonalidade de áudio \n"
                             "(ex: rodando núcleos PAL em modo NTSC). \n"
                             " \n"
                             " A taxa de entrada é definida como: \n"
                             " taxa de entrada * (1.0 +/- (desvio máximo de tempo))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Muda para a próxima transparência.\n"
                             " \n"
                             "Circula pela opções.");
            break;
        case MENU_ENUM_LABEL_LOG_VERBOSITY:
            snprintf(s, len,
                     "Habilita ou desabilita o nível \n"
                             "de verbosidade do frontend.");
            break;
        case MENU_ENUM_LABEL_VOLUME_UP:
            snprintf(s, len,
                     "Aumenta o volume de áudio.");
            break;
        case MENU_ENUM_LABEL_VOLUME_DOWN:
            snprintf(s, len,
                     "Diminui o volume de áudio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Desliga a composição a força. \n"
                             "Válido somente no Windows Vista/7 por enquanto.");
            break;
        case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            snprintf(s, len,
                     "Habilita ou desabilita os contadores \n"
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
                             "O RetroArch irá carregar automaticamente qualquer Estado de Jogo \n"
                             "com esse caminho na inicialização se a função \n"
                             "'Carregar Estado de Jogo Automaticamente' estiver habilitada.");
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
                     "Tenta a sincronia rígida entre \n"
                             "CPU e GPU. \n"
                             " \n"
                             "Reduz a latência ao custo de \n"
                             "desempenho.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Granularidade do Voltar Atrás. \n"
                             " \n"
                             "Ao Voltar Atrás um número de quadros \n"
                             "definido, você pode retroceder vários \n"
                             "quadros de cada vez, aumentando a \n"
                             "velocidade da função.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT:
            snprintf(s, len,
                     "Capturar tela.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Define quantos milisegundos atrasar\n"
                             "após o Vsync antes de rodar o núcleo.\n"
                             "\n"
                             "Reduz a latência ao custo de maior \n"
                             "risco de engasgamento de vídeo. \n"
                             " \n"
                             "O máximo é 15.");
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
                     "Insere um quadro opaco entre \n"
                             "quadros. \n"
                             " \n"
                             "Útil para usuários com telas de 120Hz que \n"
                             "desejam jogar conteúdos em 60Hz \n"
                             "para eliminar efeito de sombra. \n"
                             " \n"
                             "A taxa de atualização de vídeo ainda deve \n"
                             "ser configurada como se fosse uma tela de 60Hz \n"
                             "(dividir a taxa de atualização por 2).");
            break;
        case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            snprintf(s, len,
                     "Exibir a tela inicial no menu.\n"
                             "É automaticamente definido como falso após \n"
                             "o programa iniciar pela primeira vez. \n"
                             " \n"
                             "Somente é atualizado no arquivo de configuração \n"
                             "se 'Salvar Configuração ao Sair' estiver habilitado.");
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
                     "Intervalo de Alternância da Sincronia Vertical (Vsync). \n"
                             " \n"
                             "Usa um intervalo de troca personalizado para Vsync. Defina para \n"
                             "reduzir efetivamente a taxa de atualização do monitor pela metade.");
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
                             "Esta localização é consultada por padrão quando \n"
                             "a interface do menu tenta procurar por \n"
                             "recursos carregáveis, etc.");
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
                             "Quando está em Câmera Lenta, o conteúdo será diminuído \n"
                             "pelo fator especificado/definido.");
            break;
        case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
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
            snprintf(s, len, "Habilitar controle por Mouse dentro do menu.");
            break;
        case MENU_ENUM_LABEL_POINTER_ENABLE:
            snprintf(s, len, "Habilitar controles de toque no menu.");
            break;
        case MENU_ENUM_LABEL_MENU_WALLPAPER:
            snprintf(s, len, "Seleciona uma imagem para definir como plano de fundo.");
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
                     "Desativa o protetor de tela. É uma sugestão que \n"
                             "que não precisa necessariamente ser \n"
                             "honrada pelo driver de vídeo.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Modo cliente do Netplay para o usuário atual. \n"
                             "Será modo 'Servidor' se estiver desabilitado.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "A quantidade de atraso de quadros para utilizar no Netplay. \n"
                             " \n"
                             "Aumentar este valor irá aumentar o \n"
                             "desempenho, mas introduz mais latência.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Define se os jogos de Netplay são anunciados publicamente. \n"
                             " \n"
                             "Se definido como falso, os clientes deverão conectar \n"
                             "manualmente em vez de usar o lobby público.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Define se o Netplay deve iniciar em modo espectador. \n"
                             " \n"
                             "Se definido como verdadeiro, o Netplay estará em modo espectador \n"
                             "no começo. Sempre é possível alterar o modo \n"
                             "mais tarde.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Define se conexões em modo escravo são permitidas. \n"
                             " \n"
                             "Clientes em modo escravo requerem muito pouco \n"
                             "poder de processamento em ambos os lados, mas irão \n"
                             "sofrer significamente da latência de rede.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Define se conexões que não estão \n"
                            "em modo escravo são proibidas. \n"
                             " \n"
                             "Não recomendado, exceto para redes muito rápidas \n"
                             "com máquinas muito lentas. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Define se o Netplay deve executar em um modo que \n"
                             "não utilize Estados de Jogo. \n"
                             " \n"
                             "Se definido como verdadeiro, uma rede muito rápida é necessária, \n"
                             "mas Voltar Atrás não é permitido, então não \n"
                             "haverá oscilação no Netplay. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "A frequência em quadros na qual o Netplay \n"
                             "irá verificar se o hospedeiro e o cliente \n"
                             "estão sincronizados. \n"
                             " \n"
                             "Com a maioria dos núcleos, este valor não terá \n"
                             "efeito perceptível e pode ser ignorado. Com \n"
                             "núcleos não determinísticos, este valor define \n"
                             "quão frequente os pares do Netplay serão colocados \n"
                             "em sincronia. Com núcleos defeituosos, definir para \n"
                             "qualquer valor que não zero irá causar problemas \n"
                             "de desempenho severos. Defina como zero para desativar \n"
                             "verificações. Este valor é usado somente no \n"
                             "hospedeiro de Netplay. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "O número de quadros de latência de entrada \n"
                     "que pode ser usado pelo Netplay para mascarar \n"
                     "a latência de rede. \n"
                     " \n"
                     "Quando usando Netplay, esta opção atrasa a \n"
                     "entrada local, de modo que o quadro em execução \n"
                     "fique mais próximo do quadro recebido pela \n"
                     "rede. Isso reduz o jitter e torna o Netplay \n"
                     "menos intenso para a CPU, mas ao custo \n"
                     "de atraso perceptível na entrada. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "O intervalo de quadros de latência \n"
                     "de entrada que pode ser usado pelo Netplay \n"
                     "para mascarar a latência da rede. \n"
                     "\n"
                     "Se habilitado, o Netplay irá ajustar o número de \n"
                     "quadros de latência de entrada dinamicamente para \n"
                     "balancear tempo de CPU, latência de entrada e \n"
                     "latência de rede. Isso reduz o jitter e \n"
                     "torna o Netplay menos intensivo para a CPU, mas ao \n"
                     "custo de atraso imprevisível na entrada. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            snprintf(s, len,
                     "Quando hospedando, tentar escutar por \n"
                             "conexões da internet pública, usando UPnP \n"
                             "ou tecnologias similares para escapar de LANs. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "Quando hospedando, transmitir a conexão através \n"
                             "de um servidor 'homem no meio' \n"
                             "para contornar problemas de firewall ou NAT/UPnP.\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Quantidade máxima de imagens da cadeia de trocas. Isto \n"
                             "pode informar ao driver de vídeo para usar um \n"
                             "modo de buffer específico. \n"
                             " \n"
                             "Single buffering - 1\n"
                             "Double buffering - 2\n"
                             "Triple buffering - 3\n"
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
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Exibir o nível de bateria atual dentro do menu.");
            break;
        case MENU_ENUM_LABEL_CORE_ENABLE:
            snprintf(s, len,
                     "Exibir o nome do núcleo atual dentro do menu.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            snprintf(s, len,
                     "Habilita o Netplay no modo hospedeiro (servidor).");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            snprintf(s, len,
                     "Habilita o Netplay no modo cliente.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            snprintf(s, len,
                     "Desconecta de uma conexão de Netplay ativa.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Buscar por e conectar aos hospedeiros de Netplay na rede local.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Configurações relacionadas ao Netplay.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Carrega dinamicamente um novo plano de \n"
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
                     "Se habilitado, substitui os vínculos da entrada \n"
                             "com os vínculos remapeados definidos \n"
                             "para o núcleo atual.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            snprintf(s, len,
                     "Diretório de Transparências. \n"
                             " \n"
                             "Define um diretório onde as transparências \n"
                             "são mantidas para fácil acesso.");
            break;
        case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            snprintf(s, len,
                     "Número máximo de usuários \n"
                             "suportados pelo RetroArch.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            snprintf(s, len,
                     "Após o download, extrair automaticamente \n"
                             "os arquivos contidos nos arquivos comprimidos \n"
                             "baixados.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            snprintf(s, len,
                     "Filtrar arquivos em exibição \n"
                             "por extensões suportadas.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            snprintf(s, len,
                     "O nome de usuário da pessoa executando o RetroArch. \n"
                             "Será utilizado para jogos online.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
            snprintf(s, len,
                     "Ao ser o cliente de Netplay, use os \n"
                             "vínculos de teclas do jogador 1.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "A porta do endereço de IP do hospedeiro. \n"
                             "Pode ser ou uma porta TCP ou uma porta UDP.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Habilitar ou desabilitar o modo espectador \n"
                             "para o usuário durante o Netplay.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            snprintf(s, len,
                     "O endereço do hospedeiro a se conectar.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "A senha para conectar ao hospedeiro de Netplay \n"
                             "Utilizado apenas em modo hospedeiro.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "A senha para conectar ao hospedeiro de Netplay \n"
                             "apenas com privilégios de espectador. Utilizado \n"
                             "apenas em modo hospedeiro.");
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
                     "Combinação de botões do Gamepad para alternar o menu. \n"
                             " \n"
                             "0 - Nenhuma \n"
                             "1 - Pressionar L + R + Y + Direcional para baixo \n"
                             "simultaneamente. \n"
                             "2 - Pressionar L3 + R3 simultaneamente. \n"
                             "3 - Pressionar Start + Select simultaneamente.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len, "Permitir a qualquer usuário controlar o menu. \n"
                    " \n"
                    "Se desabilitado, apenas o Usuário 1 poderá controlar o menu.");
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
                             "Manter o turbo pressionado enquanto pressiona \n"
                             "outro botão faz com que o botão entre em \n"
                             "modo turbo, onde o estado do botão é \n"
                             "modulado com um sinal periódico. \n"
                             " \n"
                             "A modulação termina quando o botão em si \n"
                             "mesmo (não o botão de turbo) for liberado.");
            break;
        case MENU_ENUM_LABEL_OSK_ENABLE:
            snprintf(s, len,
                     "Habilitar/desabilitar teclado na tela.");
            break;
        case MENU_ENUM_LABEL_AUDIO_MUTE:
            snprintf(s, len,
                     "Áudio mudo/não-mudo.");
            break;
        case MENU_ENUM_LABEL_REWIND:
            snprintf(s, len,
                     "Manter o botão pressionado para Voltar Atrás. \n"
                             " \n"
                             "Voltar Atrás precisa estar habilitado.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "Tecla para fechar o RetroArch de modo limpo."
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
                     "Carrega Estado de Jogo.");
            break;
        case MENU_ENUM_LABEL_SAVE_STATE:
            snprintf(s, len,
                     "Salva Estado de Jogo.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_FLIP_PLAYERS:
            snprintf(s, len,
                     "Inverter usuários do Netplay.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                     "Alternar modo jogador/espectador do Netplay.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
            snprintf(s, len,
                     "Aumenta o índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
            snprintf(s, len,
                     "Diminui o índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_SHADER_PREV:
            snprintf(s, len,
                     "Aplica o Shader anterior no diretório.");
            break;
        case MENU_ENUM_LABEL_SHADER_NEXT:
            snprintf(s, len,
                     "Aplica o próximo Shader no diretório.");
            break;
        case MENU_ENUM_LABEL_RESET:
            snprintf(s, len,
                     "Reinicia o conteúdo. \n");
            break;
        case MENU_ENUM_LABEL_PAUSE_TOGGLE:
            snprintf(s, len,
                     "Alternar entre estado pausado e não-pausado.");
            break;
        case MENU_ENUM_LABEL_CHEAT_TOGGLE:
            snprintf(s, len,
                     "Alternar índice de Trapaça. \n");
            break;
        case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
            snprintf(s, len,
                     "Manter pressionado para Avanço Rápido \n"
                             "Soltar o botão desativa o Avanço Rápido.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION:
            snprintf(s, len,
                     "Manter pressionado para Câmera Lenta.");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "Avanço de quadro quando o conteúdo estiver pausado.");
            break;
        case MENU_ENUM_LABEL_MOVIE_RECORD_TOGGLE:
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
                             "Vinculado como de costume, contudo, se um eixo analógico real \n"
                             "estiver vinculado, ele pode ser lido como um analógico verdadeiro. \n"
                             " \n"
                             "Eixo X positivo é direita. \n"
                             "Eixo Y positivo é para baixo.");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "O RetroArch por si só não faz nada. \n"
                            " \n"
                            "Para fazê-lo fazer coisas, você precisa \n"
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
                     "Você pode alterar a transparência de \n"
                             "gamepad virtual indo em '%s' -> '%s'."
                             " \n"
                             "De lá você pode alterar a transparência, \n"
                             "mudar o tamanho e opacidade dos botões, etc. \n"
                             " \n"
                             "OBS: Por padrão, as transparências de gamepad virtual \n"
                             "ficam ocultas quando dentro do menu. \n"
                             "Se você quiser alterar este comportamento, \n"
                             "você pode definir '%s' como falso.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}

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
#include "msg_hash_us.h"
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

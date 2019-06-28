/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include "../../configuration.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_pt_pt_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   settings_t      *settings = config_get_ptr();

   switch (msg)
   {
      case MENU_ENUM_LABEL_CORE_LIST:
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
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         {
            const char *lbl = settings ? settings->arrays.input_driver : NULL;

            if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
               snprintf(s, len,
                     "Driver de entrada udev. \n"
                     " \n"
                     "Esse driver funciona sem o X. \n"
                     " \n"
                     "Ele usa a recente API de joypads \n"
                     "evdev para suporte a joystick. \n"
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
            else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
               snprintf(s, len,
                     "Driver de Entrada linuxraw. \n"
                     " \n"
                     "Esse driver requer um TTY ativo. Eventos de \n"
                     "teclado são lidos diretamente do TTY, tornando-o \n"
                     "simples, mas não tão flexível quanto o udev. \n" "Mouses, etc, não são suportados. \n"
                     " \n"
                     "Esse driver usa a antiga API de joysticks \n"
                     "(/dev/input/js*).");
            else
               snprintf(s, len,
                     "Driver de Entrada.\n"
                     " \n"
                     "Dependendo do driver de vídeo, pode ser necessário \n"
                     "forçar um driver de entrada diferente.");
         }
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
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
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "Driver de Vídeo em uso.");

         if (string_is_equal(settings->arrays.video_driver, "gl"))
         {
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
         }
         else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
         {
            snprintf(s, len,
                  "Driver de Vídeo SDL 2.\n"
                  " \n"
                  "Esse é um driver de vídeo SDL 2 de \n"
                  "renderização por software.\n"
                  " \n"
                  "O desempenho das implementações dos cores de \n"
                  "renderização por software depende da \n"
                  "implementação SDL de sua plataforma.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
         {
            snprintf(s, len,
                  "Driver de Vídeo SDL.\n"
                  " \n"
                  "Esse é um driver de vídeo SDL 1.2 de \n"
                  "renderização por software.\n"
                  " \n"
                  "O desemprenho é considerado subótimo. \n"
                  "Considere seu uso apenas em último caso.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "d3d"))
         {
            snprintf(s, len,
                  "Driver de Vídeo Direct3D. \n"
                  " \n"
                  "O desempenho das implementações dos cores de\n"
                  "renderização por software depende do driver \n"
                  "D3D instalado em sua placa de vídeo.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "exynos"))
         {
            snprintf(s, len,
                  "Driver de Vídeo Exynos-G2D. \n"
                  " \n"
                  "Esse é um driver de vídeo Exynos de baixo nível. \n"
                  "Usa o bloco G2D do SoC Samsung Exynos \n"
                  "para operações de blit. \n"
                  " \n"
                  "O desempenho para cores de renderização por \n"
                  "software deve ser ótimo.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
         {
            snprintf(s, len,
                  "Driver de Vídeo Sunxi-G2D. \n"
                  " \n"
                  "Esse é um driver de vídeo Sunxi de baixo nível. \n"
                  "Usa o bloco G2D dos SoCs Allwinner.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin de DSP de Áudio.\n"
               "Processa áudio antes de ser enviado ao \n"
               "driver."
               );
         break;
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         {
            const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

            if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
               strlcpy(s,
                     "Implementação Windowed SINC.", len);
            else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
               strlcpy(s,
                     "Implementação Convoluted Cosine.", len);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Parâmetros de Shaders. \n"
               " \n"
               "Modifica o shader em uso diretamente. Não será \n"
               "salvo no arquivo de predefinições CGP/GLSLP.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Parâmetros de Predefinições de Shader. \n"
               " \n"
               "Modifica as predefinições de shader em uso no menu."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
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
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Filtro de hardware para este passo. \n"
               " \n"
               "Se 'Tanto faz' estiver definido, o 'Filtro \n"
               "Padrão' será usado."
               );
         break;
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
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
      case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Tipo de Dispositivo de Entrada. \n"
               " \n"
               "Escolhe o dispositivo a usar. Isso é \n"
               "relevante para o core libretro."
               );
         break;
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
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
      case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
      case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
         snprintf(s, len,
               "Slot de Savestates.\n"
               " \n"
               " Com o slot definido em 0, o nome do Savestate \n"
               " será *.state (ou o que estiver definido em commandline).\n"
               "Se diferente de 0, o nome será (caminho)(d), \n"
               "em que (d) é o número do slot.");
         break;
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
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
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Alterna menu.");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Alterna uso de mouse.\n"
               " \n"
               "Quando o mouse é usado, RetroArch o esconde e \n"
               "mantém o seu ponteiro dentro da janela para \n"
               "permitir que a entrada relativa do mouse \n"
               "funcione melhor.");
         break;
      case MENU_ENUM_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Circula por imagens de discos. Usar \n"
               "após ejetar. \n"
               " \n"
               " Finaliza ao usar ejetar novamente.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FILTER:
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
      case MENU_ENUM_LABEL_AUDIO_DEVICE:
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
      case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Alterna ejeção para discos.\n"
               " \n"
               "Usado para conteúdos multidiscos.");
         break;
      case MENU_ENUM_LABEL_ENABLE_HOTKEY:
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
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Ativa retrocesso.\n"
               " \n"
               "Essa opção causa uma perda de desempenho, \n"
               "por isso está desativada por padrão.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Diretórios de Cores. \n"
               " \n"
               "Um diretório onde são buscadas as \n"
               "implementações de cores libretro.");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         {
            /* Work around C89 limitations */
            const char * u =
                  "Taxa de Atualização Automática.\n"
                  " \n"
                  "A taxa de atualização exata de nosso monitor (Hz).\n"
                  "É usada para calcular a taxa de entrada de áudio \n"
                  "com a fórmula: \n"
                  " \n"
                  "audio_input_rate = game input rate * display \n"
                  "refresh rate / game refresh rate\n"
                  " \n";
            const char * t =
                  "Se a implementação não informar valores, \n"
                  "valores NTSC serão assumidos por questão de \n"
                  "compatibilidade.\n"
                  " \n"
                  "Esse valor deve ficar próximo de 60Hz para \n"
                  "evitar grandes mudanças de pitch. Se o monitor \n"
                  "não rodar a 60Hz, ou algo próximo a isso, desative\n"
                  "o VSync, e deixe-o com valores padrão.";
            strlcpy(s, u, len);
            strlcat(s, t, len);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Força uma certa rotação da tela. \n"
               " \n"
               "A rotação é adicionada a outras definidas\n"
               "por conjuntos de cores (veja Permitir\n"
               "Rotação de Vídeo).");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Resolução de tela cheia.\n"
               " \n"
               "Resolução 0 usa a resolução \n"
               "do ambiente.\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
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
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Preferência de monitor.\n"
               " \n"
               "0 (padrão) significa nenhum monitor é \n"
               "preferido, 1 e demais (1 é o primeiro \n"
               "monitor), sugere ao RetroArch usar esse \n"
               "monitor em particular.");
         break;
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Força o descarte de quadros overscanned. \n"
               " \n"
               "O comportamento exato dessa opção é \n"
               "específico da implementação do core.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
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
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Volume de som, em dB.\n"
               " \n"
               " 0 dB representa o nível normal do volume. Nenhum ganho será aplicado.\n"
               "O ganho pode ser controlado em execução com \n"
               "Aumentar volume / Diminuir volume.");
         break;
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         snprintf(s, len,
               "Controlo de taxa de som.\n"
               " \n"
               "Se o valor 0 estiver definido, o controlo da taxa será desativado.\n"
               "Outros valores controlam a variação da taxa \n"
               "de som.\n"
               " \n"
               "Define o intervalo de variação da taxa de entrada, \n"
               "em termos de regulação.\n"
               " \n"
               " A taxa de entrada é definida como: \n"
               " taxa entrada * (1.0 +/- (Controlo da taxa de som))");
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         snprintf(s, len,
               "Distorção de som máxima.\n"
               " \n"
               "Define a máxima variação da taxa de entrada.\n"
               "Você pode querer aumentá-la para obter grandes\n"
               "variações no compasso, por exemplo, ao executar\n"
               "núcleos PAL em ecrãs NTSC, sofrendo uma penalização da tonalidade\n"
               "do som.\n"
               " \n"
               " A taxa de entrada é definida como: \n"
               " taxa de entrada * (1.0 +/- (distorção máxima do som))");
         break;
      case MENU_ENUM_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Muda para o próximo overlay.\n"
               " \n"
               "Navegação circular.");
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Ativa ou desativa o nível de verbosidade \n"
               "do frontend.");
         break;
      case MENU_ENUM_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Aumenta o volume de som.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Diminui o volume do som.");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Força a desativação de composição.\n"
               "Válido apenas para o Windows Vista/7.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Ativa ou desativa contadores de desempenho \n"
               "do frontend.");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "Pasta do sistema. \n"
               " \n"
               "Define a pasta 'system'.\n"
               "Os núcleos poderão consultar esta pasta\n"
               "para carregar BIOS, configurações\n"
               "específicas de sistema, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Guardar automaticamente o estado de gravação ao sair \n"
               "do RetroArch.\n"
               " \n"
               "O RetroArch carregará automaticamente qualquer\n"
               "estado de gravação com este caminho ao iniciar, \n"
               "caso a opção 'Carregar estado de gravação automaticamente' estiver ativa.");
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Utiliza um controlador de vídeo segmentado.\n"
               " \n"
               "Utilizando esta opção, poderá melhorar o desempenho, \n"
               "com um custo a nível de latência e mais engasgamentos \n"
               "de vídeo.");
         break;
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Sincronia vertical de vídeo.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Tenta sincronizar o processador com a GPU, \n"
               "com recurso ao hardware.\n"
               " \n"
               "Pode reduzir a latência, a troco de uma \n"
               "diminuição de desempenho.");
         break;
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Granularidade da função de retrocedimento \n"
               "do tempo.\n"
               "Ao retroceder um número definido de \n"
               "fotogramas, você poderá retroceder vários \n"
               "fotogramas de cada vez, aumentando assim a velocidade \n"
               "do retrocedimento.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Obtem uma captura de ecrã.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Define quantos milissegundos serão utilizados \n"
               "para atraso, após a sincronização vertical, \n"
               "antes de iniciar o núcleo.\n"
               "Pode reduzir a latência a troco de\n"
               "um aumento do risco de engasgamento de vídeo.\n"
               " \n"
               "O valor máximo é 15.");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
         snprintf(s, len,
               "Define quantos fotogramas o processador pode processar \n"
               "à frente da da GPU com a opção 'Sincronismo \n"
               "de GPU através de hardware' ativada.\n"
               " \n"
               "O valor máximo é 3.\n"
               " \n"
               " 0: Sincroniza com a GPU de imediato.\n"
               " 1: Sincroniza com o fotograma anterior.\n"
               " 2: Etc ...");
         break;
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(s, len,
               "Introduz um fotograma preto entre fotogramas. \n"
               " \n"
               "Útil para monitores de 120 Hz que executam \n"
               "conteúdo a 60 Hz, eliminando o efeito \n"
               "'ghosting'.\n"
               " \n"
               "A taxa de atualização de vídeo deve ainda \n"
               "ser configurada para 60 Hz \n"
               "(divida a taxa de atualização por 2).");
         break;
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Mostra o ecrã inicial no menu.\n"
               "Esta definição é desativada automaticamente quando\n"
               "vista pela primeira vez.\n"
               " \n"
               "Esta definição é definida na configuração apenas quando a\n"
               "opção 'Guardar configuração ao sair' estiver ativada.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Ativa/desativa ecrã completo.");
         break;
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Previne a sobrescrição da SRAM durante \n"
               "o carregamento de estados de gravação.\n"
               " \n"
               "Pode originar erros durante a execução de conteúdos.");
         break;
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Colocar em pausa o jogo quando o foco da janela \n"
               "for perdido.");
         break;
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Captura vídeo renderizado da saída da \n"
               "GPU, caso esteja disponível.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Pasta de capturas de ecrã. \n"
               " \n"
               "Pasta para armazenamento das capturas de ecrã."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "Intervalo de troca de sincronização vertical.\n"
               " \n"
               "Utiliza um intervalo de troca personalizado. \n"
               "Use-o para reduzir até metade a taxa de \n"
               "atualização do ecrã.");
         break;
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Pasta de jogos guardados. \n"
               " \n"
               "Guarda todos os ficheiros de gravação (*.srm) nesta \n"
               "pasta, tais como ficheiros relacionados: \n"
               ".bsv, .rt, .psrm, etc.\n"
               " \n"
               "Pode perder prevalência em relação a opções explícitas\n"
               "passadas como parâmetros na linha de comandos.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Pasta de gravação de estados. \n"
               " \n"
               "Todos os estados de gravação (*.state) serão \n"
               "guardados nesta pasta.\n"
               " \n"
               "Pode deixar de ter prevalência em relação a opções explícitas\n"
               "passadas como parâmetros na linha de comandos.");
         break;
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Pasta de recursos (assets). \n"
               " \n"
               "Esta localização é consultada por omissão \n"
               "pela interface do menu para procura de recursos (assets) \n"
               "recarregáveis.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Pasta de imagens de fundo dinâmicos. \n"
               " \n"
               " O lugar para armazenar imagens de fundo que \n"
               "serão carregadas dinamicamente pelo menu, \n"
               "dependendo do contexto.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Rácio da câmera lenta."
               " \n"
               "Quando ativo, o conteúdo será executado numa velocidade\n"
               "reduzida por esse fator.");
         break;
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len,
               "Período de turbo.\n"
               " \n"
               "Descreve a velocidade com que os botões\n"
               "com turbo ativo se alternam entre si."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Ativa auto-detecção de periféricos de entrada.\n"
               " \n"
               "Os comandos serão configurados, de forma automática, \n"
               "através do Plug and Play.");
         break;
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Autorizar ou desautorizar o acesso da câmera \n"
               "pelos núcleos.");
         break;
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Autorizar ou desautorizar o acesso de \n"
               "serviços de localização pelos núcleos.");
         break;
      case MENU_ENUM_LABEL_TURBO:
         snprintf(s, len,
               "Ativar turbo.\n"
               " \n"
               "Se manter o botão turbo pressionado e pressionar outro \n"
               "botão, este último entrará no modo \n"
               "turbo e o seu estado será modulado com \n"
               "um sinal periódico. \n"
               " \n"
               "A modulação pára quando o próprio botão \n"
               "(não o botão de turbo) já no se encontrar pressionado.");
         break;
      case MENU_ENUM_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Ativar/desativar teclado no ecrã.");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Silenciar ou remover silêncio do dispositivo.");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "Mantenha o botão pressionado para\n"
               "ativar o retrocedimento do tempo. \n"
               "O retrocedimento de tempo deve estar ativo.");
         break;
      case MENU_ENUM_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Tecla para encerrar o RetroArch."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nSe o fechar abruptamente, \n"
               "(SIGKILL, etc) sairá sem guardar a RAM, entre outros dados.\n"
               "Em sistemas baseados em Unix,\n"
               "o SIGINT/SIGTERM permite um\n"
               "encerramento seguro."
#endif
               );
         break;
      case MENU_ENUM_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Carrega o estado de gravação.");
         break;
      case MENU_ENUM_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Guarda o estado de gravação.");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Incrementa o índice de batota.\n");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrementa o índice de batota.\n");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Aplica o shader anterior na pasta.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Aplica o próximo shader na pasta.");
         break;
      case MENU_ENUM_LABEL_RESET:
         snprintf(s, len,
               "Reinicia o conteúdo.\n");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Pausa/resume o conteúdo.");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Alterna entre os índices de batota.\n");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Continue a pressionar para ativar a função de avanço rápido. Se soltar o botão \n"
               "irá desativar a função de avanço-rápido.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
         snprintf(s, len,
               "Continue a pressionar para manter o modo de câmera lenta.");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "O fotograma avança enquanto o conteúdo está em pausa.");
         break;
      case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
         snprintf(s, len,
               "Ativa ou desativa a gravação.");
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
               "Eixo para o analógico (esquema DualShock).\n"
               " \n"
               "A associação é feita normalmente, no entanto, se um analógico real \n"
               "for associado, pode ser lido como um analógico\n"
               "verdadeiro. \n"
               " \n"
               "O eixo positivo X é para direita. \n"
               "O eixo positivo Y é para baixo.");
         break;
      case MSG_UNKNOWN:
      default:
         if (s[0] == '\0')
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

const char *msg_hash_to_str_pt_pt(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "msg_hash_pt_pt.h"
      default:
         break;
   }

   return "null";
}

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

#include "../msg_hash.h"

const char *msg_hash_to_str_pt(uint32_t hash)
{
   switch (hash)
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
      default:
         break;
   }

   return "null";
}

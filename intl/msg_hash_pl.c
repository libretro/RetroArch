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

const char *msg_hash_to_str_pl(uint32_t hash)
{
   switch (hash)
   {
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Zatrzymano nagrywanie filmu.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Zakończono odtwarzanie filmu.";
      case MSG_AUTOSAVE_FAILED:
         return "Nie udało się zainicjalizować automatycznego zapisu.";
      case MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED:
         return "Odtwarzanie filmu w toku. Nie można rozpocząć gry sieciowej.";
      case MSG_NETPLAY_FAILED:
         return "Nie udało się zainicjalizować gry sieciowej.";
      case MSG_LIBRETRO_ABI_BREAK:
         return "został skompilowany dla innej wersji libretro, różnej od obecnie używanej.";
      case MSG_REWIND_INIT_FAILED_NO_SAVESTATES:
         return "Implementacja nie wspiera zapisywania stanu. Przewijanie nie jest możliwe.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementacja używa osobnego wątku do przetwarzania dźwięku. Przewijanie nie jest możliwe.";
      case MSG_REWIND_INIT_FAILED:
         return "Nie udało się zainicjalizować bufora przewijania. Przewijanie zostanie wyłączone.";
      case MSG_REWIND_INIT:
         return "Inicjalizowanie bufora przewijania o rozmiarze";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Wprowadzono niestandardowy timing";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Obliczanie rozmiaru viewportu nie powiodło się! Kontynuowanie z użyciem surowych danych. Prawdopodobnie nie będzie to działało poprawnie...";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Ten rdzeń libretro używa renderowania sprzętowego. Konieczne jest użycie nagrywania wraz z zaaplikowanymi shaderami.";
      case MSG_RECORDING_TO:
         return "Nagrywanie do";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Wykrywanie viewportu";
      case MSG_TAKING_SCREENSHOT:
         return "Zapisywanie zrzutu.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Nie udało się zapisać zrzutu.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Nie udało się rozpocząć nagrywania.";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Przerwano nagrywanie z powodu zmiany rozmiaru.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Atrapa rdzenia w użyciu. Pomijanie nagrywania.";
      case MSG_UNKNOWN:
         return "Nieznane";
      case MSG_LOADING_CONTENT_FILE:
         return "Wczytywanie pliku treści";
      case MSG_RECEIVED:
         return "otrzymano";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Nieznana komenda";
      case MSG_SENDING_COMMAND:
         return "Wysyłanie komendy";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Otrzymano nieprawidłowy indeks dysku.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Nie udało się usunąć dysku z tacki.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Usunięto dysk z tacki.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "wirtualna tacka."; /* this is funny */
      case MSG_FAILED_TO:
         return "Nie udało się";
      case MSG_TO:
         return "do";
      case MSG_SAVING_RAM_TYPE:
         return "Zapisywanie typu RAM";
      case MSG_SAVING_STATE:
         return "Zapisywanie stanu";
      case MSG_LOADING_STATE:
         return "Wczytywanie stanu";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Nie udało się wczytać pliku filmu";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Nie udało się wczytać treści";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Nie udało się odczytać pliku treści";
      case MSG_GRAB_MOUSE_STATE:
         return "Grab mouse state";
      case MSG_PAUSED:
         return "Wstrzymano.";
      case MSG_UNPAUSED:
         return "Wznowiono.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Nie udało się wczytać nakładki.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Nie udało się przywrócić dźwięku.";
      case MSG_AUDIO_MUTED:
         return "Wyciszono dźwięk.";
      case MSG_AUDIO_UNMUTED:
         return "Przywrócono dźwięk.";
      case MSG_RESET:
         return "Reset";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Nie udało się wczytać stanu z";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Nie udało się zapisać stanu do";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Nie udało się zapisać SRAM";
      case MSG_STATE_SIZE:
         return "Rozmiar stanu";
      case MSG_FOUND_SHADER:
         return "Znaleziono shader";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM nie zostanie zapisany.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blokowanie nadpisania SRAM";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Rdzeń nie wspiera zapisów stanu.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Zapisano stan w slocie";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Pomyślnie zapisano do";
      case MSG_BYTES:
         return "bajtów";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Nie ustawiono katalogu konfiguracji. Nie można zapisać nowej konfiguracji.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Pomijanie wczytywania SRAM.";
      case MSG_APPENDED_DISK:
         return "Dopisano do dysku";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Rozpoczynanie odtwarzania filmu.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Nie udało się usunąć pliku tymczasowego";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Usuwanie tymczasowego pliku treści";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Wczytano stan ze slotu";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Nie udało się przetworzyć pliku ZIP.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Zakończono skanowanie katalogu";
      case MSG_SCANNING:
         return "Skanowanie";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Przekierowywanie pliku cheatów do";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Przekierowywanie pliku zapisu do";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Przekierowywanie zapisu stanu do";
      case MSG_SHADER:
         return "Shader";
      case MSG_APPLYING_SHADER:
         return "Aplikowanie shadera";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Nie udało się zaaplikować shadera.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Rozpoczynanie zapisu filmu do";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Nie udało się rozpocząć nagrywania filmu.";
      case MSG_STATE_SLOT:
         return "Slot zapisu";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Ponowne rozpoczęcie nagrywania z powodu reinicjalizacji kontrolera.";
      case MSG_SLOW_MOTION:
         return "Spowolnione tempo.";
      case MSG_SLOW_MOTION_REWIND:
         return "Przewijanie w spowolnionym tempie.";
      case MSG_REWINDING:
         return "Przewijanie.";
      case MSG_REWIND_REACHED_END:
         return "W buforze przewijania nie ma więcej danych.";
      default:
         break;
   }

   return "null";
}

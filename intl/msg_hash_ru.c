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

const char *msg_hash_to_str_ru(uint32_t hash)
{
   switch (hash)
   {
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Запись остановлена.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Достигнут конец записи.";
      case MSG_AUTOSAVE_FAILED:
         return "Ошибка автосохранения.";
      case MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED:
         return "Воспроизведение записи. Невозможно начать сетевую игру.";
      case MSG_NETPLAY_FAILED:
         return "Ошибка запуска сетевой игры.";
      case MSG_LIBRETRO_ABI_BREAK:
         return "скомпилировано для другой версии libretro.";
      case MSG_REWIND_INIT_FAILED_NO_SAVESTATES:
         return "Ядро не поддерживает сохранения. Перемотка невозможна.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Ядро использует многопоточный звук. Перемотка невозможна.";
      case MSG_REWIND_INIT_FAILED:
         return "Ошибка создания буфера перемотки. Перемотка будет отключена.";
      case MSG_REWIND_INIT:
         return "Инициализация буфера перемотки с размером";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Задано ручное значение тайминга.";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Ошибка расчёта размеров окна проекции. Будут использованы необработанные данные. Возможны ошибки.";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Ядро использует аппаратный рендеринг. Включите запись с GPU.";
      case MSG_RECORDING_TO:
         return "Запись в";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Обнаружено окно проекции";
      case MSG_TAKING_SCREENSHOT:
         return "Скриншот сохранён.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Невозможно создать скриншот.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Невозможно начать запись.";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Размеры окна были изменены. Запись остановлена.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Используется фиктивное ядро. Запись не производится.";
      case MSG_UNKNOWN:
         return "Неизвестно.";
      case MSG_LOADING_CONTENT_FILE:
         return "Загружен файл контента";
      case MSG_RECEIVED:
         return "получено";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Неизвестная команда";
      case MSG_SENDING_COMMAND:
         return "Отправка команды";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Задан неверный номер диска.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Невозможно извлечь диск.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Диск извлечён.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "виртуальный лоток cd-привода";
      case MSG_FAILED_TO:
         return "Ошибка";
      case MSG_TO:
         return "в";
      case MSG_SAVING_RAM_TYPE:
         return "Запись RAM";
      case MSG_SAVING_STATE:
         return "Сохранено";
      case MSG_LOADING_STATE:
         return "Загружено сохранение";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Не удалось загрузить файл записи.";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Не удалось загрузить контент";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Не удалось прочитать файл контента";
      case MSG_GRAB_MOUSE_STATE:
         return "Режим перехвата мыши";
      case MSG_PAUSED:
         return "Пауза вкл.";
      case MSG_UNPAUSED:
         return "Пауза откл.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Ошибка загрузки оверлея.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Не удалось включить звук.";
      case MSG_AUDIO_MUTED:
         return "Звук откл.";
      case MSG_AUDIO_UNMUTED:
         return "Звук вкл.";
      case MSG_RESET:
         return "Сброс";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Ошибка загрузки сохранения из";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Ошибка записи сохранения в";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Ошибка сохранения SRAM";
      case MSG_STATE_SIZE:
         return "Размер сохранения";
      case MSG_FOUND_SHADER:
         return "Обнаружен шейдер";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "Невозможно сохранить SRAM.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Перезапись SRAM запрещена.";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Ядро не поддерживает быстрые сохранения.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Сохранено в слот";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Успешно сохранено в";
      case MSG_BYTES:
         return "байт";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Не задана папка хранения настроек. Невозможно сохранить конфигурацию.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Пропуск загрузки SRAM.";
      case MSG_APPENDED_DISK:
         return "Вставлен диск";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Воспроизведение записи.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Ошибка удаления временного файла.";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Удалён временный файл контента";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Загружено сохранение из слота";
      case MSG_DOWNLOADING:
         return "Прогресс загрузки";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Ошибка обработки ZIP-архива.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Сканирование папки завершено";
      case MSG_SCANNING:
         return "Сканирование";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Файл с чит-кодами перенаправлен в";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Файл карты памяти перенаправлен в";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Файл сохранения перенаправлен в";
      case MSG_SHADER:
         return "Шейдер";
      case MSG_APPLYING_SHADER:
         return "Применён шейдер";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Не удалось применить шейдер";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Запись видео в";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Невозможно начать запись видео.";
      case MSG_STATE_SLOT:
         return "Слот сохранения";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Реинициализация драйвера. Запись перезапущена.";
      case MSG_SLOW_MOTION:
         return "Замедление.";
      case MSG_SLOW_MOTION_REWIND:
         return "Замедленная перемотка.";
      case MSG_REWINDING:
         return "Перемотка.";
      case MSG_REWIND_REACHED_END:
         return "Достигнут предел буфера перемотки.";
      default:
         break;
   }

   return "null";
}

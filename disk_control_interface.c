/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (disk_control_interface.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string/stdstring.h>
#include <file/file_path.h>

#include "paths.h"
#include "retroarch.h"
#include "verbosity.h"
#include "msg_hash.h"

#include "disk_control_interface.h"

/*****************/
/* Configuration */
/*****************/

/* Sets all disk interface callback functions
 * to NULL */
static void disk_control_reset_callback(
      disk_control_interface_t *disk_control)
{
   if (!disk_control)
      return;

   memset(&disk_control->cb, 0,
         sizeof(struct retro_disk_control_ext_callback));
}

/* Set v0 disk interface callback functions */
void disk_control_set_callback(
      disk_control_interface_t *disk_control,
      const struct retro_disk_control_callback *cb)
{
   if (!disk_control)
      return;

   disk_control_reset_callback(disk_control);

   if (!cb)
      return;

   disk_control->cb.set_eject_state     = cb->set_eject_state;
   disk_control->cb.get_eject_state     = cb->get_eject_state;
   disk_control->cb.get_image_index     = cb->get_image_index;
   disk_control->cb.set_image_index     = cb->set_image_index;
   disk_control->cb.get_num_images      = cb->get_num_images;
   disk_control->cb.replace_image_index = cb->replace_image_index;
   disk_control->cb.add_image_index     = cb->add_image_index;
}

/* Set v1+ disk interface callback functions */
void disk_control_set_ext_callback(
      disk_control_interface_t *disk_control,
      const struct retro_disk_control_ext_callback *cb)
{
   if (!disk_control)
      return;

   disk_control_reset_callback(disk_control);

   if (!cb)
      return;

   disk_control->cb.set_eject_state     = cb->set_eject_state;
   disk_control->cb.get_eject_state     = cb->get_eject_state;
   disk_control->cb.get_image_index     = cb->get_image_index;
   disk_control->cb.set_image_index     = cb->set_image_index;
   disk_control->cb.get_num_images      = cb->get_num_images;
   disk_control->cb.replace_image_index = cb->replace_image_index;
   disk_control->cb.add_image_index     = cb->add_image_index;

   disk_control->cb.set_initial_image   = cb->set_initial_image;
   disk_control->cb.get_image_path      = cb->get_image_path;
   disk_control->cb.get_image_label     = cb->get_image_label;
}

/**********/
/* Status */
/**********/

/* Returns true if core supports basic disk
 * control functionality
 * - set_eject_state
 * - get_eject_state
 * - get_image_index
 * - set_image_index
 * - get_num_images */
bool disk_control_enabled(
      disk_control_interface_t *disk_control)
{
   if (!disk_control)
      return false;

   if (disk_control->cb.set_eject_state &&
       disk_control->cb.get_eject_state &&
       disk_control->cb.get_image_index &&
       disk_control->cb.set_image_index &&
       disk_control->cb.get_num_images)
      return true;

   return false;
}

/* Returns true if core supports disk append
 * functionality
 * - replace_image_index
 * - add_image_index */
bool disk_control_append_enabled(
      disk_control_interface_t *disk_control)
{
   if (!disk_control)
      return false;

   if (disk_control->cb.replace_image_index &&
       disk_control->cb.add_image_index)
      return true;

   return false;
}

/* Returns true if core supports image
 * labels
 * - get_image_label */
bool disk_control_image_label_enabled(
      disk_control_interface_t *disk_control)
{
   if (!disk_control || !disk_control->cb.get_image_label)
      return false;
   return true;
}

/* Returns true if core supports setting
 * initial disk index
 * - set_initial_image
 * - get_image_path */
bool disk_control_initial_image_enabled(
      disk_control_interface_t *disk_control)
{
   if (!disk_control)
      return false;

   if (disk_control->cb.set_initial_image &&
       disk_control->cb.get_image_path)
      return true;

   return false;
}

/***********/
/* Getters */
/***********/

/* Returns true if disk is currently ejected */
bool disk_control_get_eject_state(
      disk_control_interface_t *disk_control)
{
   if (!disk_control || !disk_control->cb.get_eject_state)
      return false;
   return disk_control->cb.get_eject_state();
}

/* Returns number of disk images registered
 * by the core */
unsigned disk_control_get_num_images(
      disk_control_interface_t *disk_control)
{
   if (!disk_control || !disk_control->cb.get_num_images)
      return 0;
   return disk_control->cb.get_num_images();
}

/* Returns currently selected disk image index */
unsigned disk_control_get_image_index(
      disk_control_interface_t *disk_control)
{
   if (!disk_control || !disk_control->cb.get_image_index)
      return 0;
   return disk_control->cb.get_image_index();
}

/* Fetches core-provided disk image label
 * (label is set to an empty string if core
 * does not support image labels) */
void disk_control_get_image_label(
      disk_control_interface_t *disk_control,
      unsigned index, char *label, size_t len)
{
   if (!label || len < 1)
      return;

   if (!disk_control)
      goto error;

   if (!disk_control->cb.get_image_label)
      goto error;

   if (!disk_control->cb.get_image_label(index, label, len))
      goto error;

   return;

error:
   label[0] = '\0';
}

/***********/
/* Setters */
/***********/

/* Generates an appropriate log/notification message
 * for a disk index change event */
static void disk_control_get_index_set_msg(
      disk_control_interface_t *disk_control,
      unsigned num_images, unsigned index, bool success,
      unsigned *msg_duration, char *msg, size_t len)
{
   bool has_label = false;
   char image_label[128];

   image_label[0] = '\0';

   if (!disk_control || !msg_duration || !msg || len < 1)
      return;

   /* Attempt to get image label */
   if (index < num_images)
   {
      disk_control_get_image_label(
            disk_control, index, image_label, sizeof(image_label));
      has_label = !string_is_empty(image_label);
   }

   /* Get message duration
    * > Default is 60
    * > If a label is shown, then increase duration by 50%
    * > For errors, duration is always 180 */
   *msg_duration = success ?
         (has_label ? 90 : 60) :
         180;

   /* Check whether image was inserted or removed */
   if (index < num_images)
   {
      if (has_label)
         snprintf(
               msg, len, "%s: %u/%u - %s",
               success ? msg_hash_to_str(MSG_SETTING_DISK_IN_TRAY) :
                     msg_hash_to_str(MSG_FAILED_TO_SET_DISK),
               index + 1, num_images, image_label);
      else
         snprintf(
               msg, len, "%s: %u/%u",
               success ? msg_hash_to_str(MSG_SETTING_DISK_IN_TRAY) :
                     msg_hash_to_str(MSG_FAILED_TO_SET_DISK),
               index + 1, num_images);
   }
   else
      strlcpy(
            msg,
            success ? msg_hash_to_str(MSG_REMOVED_DISK_FROM_TRAY) :
                  msg_hash_to_str(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY),
            len);
}

/* Sets the eject state of the virtual disk tray */
bool disk_control_set_eject_state(
      disk_control_interface_t *disk_control,
      bool eject, bool verbosity)
{
   bool error = false;
   char msg[128];

   msg[0] = '\0';

   if (!disk_control || !disk_control->cb.set_eject_state)
      return false;

   /* Set eject state */
   if (disk_control->cb.set_eject_state(eject))
      snprintf(
            msg, sizeof(msg), "%s",
            eject ? msg_hash_to_str(MSG_DISK_EJECTED) :
                  msg_hash_to_str(MSG_DISK_CLOSED));
   else
   {
      error = true;
      snprintf(
            msg, sizeof(msg), "%s",
            eject ? msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY_EJECT) :
                  msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY_CLOSE));
   }

   if (!string_is_empty(msg))
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Errors should always be displayed */
      if (verbosity || error)
         runloop_msg_queue_push(
               msg, 1, error ? 180 : 60,
               true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   return !error;
}

/* Sets currently selected disk index
 * NOTE: Will fail if disk is not currently ejected */
bool disk_control_set_index(
      disk_control_interface_t *disk_control,
      unsigned index, bool verbosity)
{
   bool error            = false;
   unsigned num_images   = 0;
   unsigned msg_duration = 0;
   char msg[PATH_MAX_LENGTH];

   msg[0] = '\0';

   if (!disk_control)
      return false;

   if (!disk_control->cb.get_eject_state ||
       !disk_control->cb.get_num_images ||
       !disk_control->cb.set_image_index)
      return false;

   /* Ensure that disk is currently ejected */
   if (!disk_control->cb.get_eject_state())
      return false;

   /* Get current number of disk images */
   num_images = disk_control->cb.get_num_images();

   /* Perform 'set index' action */
   error = !disk_control->cb.set_image_index(index);

   /* Get log/notification message */
   disk_control_get_index_set_msg(
         disk_control, num_images, index, !error,
         &msg_duration, msg, sizeof(msg));

   /* Output log/notification message */
   if (!string_is_empty(msg))
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Errors should always be displayed */
      if (verbosity || error)
         runloop_msg_queue_push(
               msg, 1, msg_duration,
               true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* If operation was successful, update disk
    * index record (if enabled) */
   if (!error && disk_control->record_enabled)
   {
      if (disk_control->cb.get_image_index &&
          disk_control->cb.get_image_path)
      {
         bool image_path_valid    = false;
         unsigned new_image_index = 0;
         char new_image_path[PATH_MAX_LENGTH];

         new_image_path[0] = '\0';

         /* Get current image index + path */
         new_image_index  = disk_control->cb.get_image_index();
         image_path_valid = disk_control->cb.get_image_path(
               new_image_index, new_image_path, sizeof(new_image_path));

         if (image_path_valid)
            disk_index_file_set(
                  &disk_control->index_record,
                  new_image_index, new_image_path);
         else
            disk_index_file_set(
                  &disk_control->index_record, 0, NULL);
      }
   }

   return !error;
}

/* Increments selected disk index */
bool disk_control_set_index_next(
      disk_control_interface_t *disk_control,
      bool verbosity)
{
   unsigned num_images   = 0;
   unsigned image_index  = 0;
   bool disk_next_enable = false;

   if (!disk_control)
      return false;

   if (!disk_control->cb.get_num_images ||
       !disk_control->cb.get_image_index)
      return false;

   num_images  = disk_control->cb.get_num_images();
   image_index = disk_control->cb.get_image_index();
   /* Would seem more sensible to check (num_images > 1)
    * here, but seems we need to be able to cycle the
    * same image for legacy reasons... */
   disk_next_enable = (num_images > 0) && (num_images != UINT_MAX);

   if (!disk_next_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return false;
   }

   if (image_index < (num_images - 1))
      image_index++;

   return disk_control_set_index(disk_control, image_index, verbosity);
}

/* Decrements selected disk index */
bool disk_control_set_index_prev(
      disk_control_interface_t *disk_control,
      bool verbosity)
{
   unsigned num_images   = 0;
   unsigned image_index  = 0;
   bool disk_prev_enable = false;

   if (!disk_control)
      return false;

   if (!disk_control->cb.get_num_images ||
       !disk_control->cb.get_image_index)
      return false;

   num_images  = disk_control->cb.get_num_images();
   image_index = disk_control->cb.get_image_index();
   /* Would seem more sensible to check (num_images > 1)
    * here, but seems we need to be able to cycle the
    * same image for legacy reasons... */
   disk_prev_enable = (num_images > 0);

   if (!disk_prev_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return false;
   }

   if (image_index > 0)
      image_index--;

   return disk_control_set_index(disk_control, image_index, verbosity);
}

/* Appends specified image file to disk image list */
bool disk_control_append_image(
      disk_control_interface_t *disk_control,
      const char *image_path)
{
   bool initial_disk_ejected   = false;
   unsigned initial_index      = 0;
   unsigned new_index          = 0;
   const char *image_filename  = NULL;
   struct retro_game_info info = {0};
   char msg[128];

   msg[0] = '\0';

   /* Sanity check. If any of these fail then a
    * frontend error has occurred - we will not
    * deal with that here */
   if (!disk_control)
      return false;

   if (!disk_control->cb.get_image_index ||
       !disk_control->cb.get_num_images ||
       !disk_control->cb.add_image_index ||
       !disk_control->cb.replace_image_index ||
       !disk_control->cb.get_eject_state)
      return false;

   if (string_is_empty(image_path))
      return false;

   image_filename = path_basename(image_path);

   if (string_is_empty(image_filename))
      return false;

   /* Get initial disk eject state */
   initial_disk_ejected = disk_control_get_eject_state(disk_control);

   /* Cache initial image index */
   initial_index = disk_control->cb.get_image_index();

   /* If tray is currently closed, eject disk */
   if (!initial_disk_ejected &&
       !disk_control_set_eject_state(disk_control, true, false))
      goto error;

   /* Append image */
   if (!disk_control->cb.add_image_index())
      goto error;

   new_index = disk_control->cb.get_num_images();
   if (new_index < 1)
      goto error;
   new_index--;

   info.path = image_path;
   if (!disk_control->cb.replace_image_index(new_index, &info))
      goto error;

   /* Set new index */
   if (!disk_control_set_index(disk_control, new_index, false))
      goto error;

   /* If tray was initially closed, insert disk
    * (i.e. leave system in the state we found it) */
   if (!initial_disk_ejected &&
       !disk_control_set_eject_state(disk_control, false, false))
      goto error;

   /* Display log */
   snprintf(
         msg, sizeof(msg), "%s: %s",
         msg_hash_to_str(MSG_APPENDED_DISK), image_filename);

   RARCH_LOG("%s\n", msg);
   /* This message should always be displayed, since
    * the menu itself does not provide sufficient
    * visual feedback */
   runloop_msg_queue_push(
         msg, 0, 120,
         true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return true;

error:
   /* If we reach this point then everything is
    * broken and the disk control interface is
    * in an undefined state. Try to restore some
    * sanity by reinserting the original disk...
    * NOTE: If this fails then it's game over -
    * just display the error notification and
    * hope for the best... */
   if (!disk_control->cb.get_eject_state())
      disk_control_set_eject_state(disk_control, true, false);
   disk_control_set_index(disk_control, initial_index, false);
   if (!initial_disk_ejected)
      disk_control_set_eject_state(disk_control, false, false);

   snprintf(
         msg, sizeof(msg), "%s: %s",
         msg_hash_to_str(MSG_FAILED_TO_APPEND_DISK), image_filename);

   runloop_msg_queue_push(
         msg, 0, 180,
         true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return false;
}

/*****************************/
/* 'Initial index' functions */
/*****************************/

/* Attempts to set current core's initial disk index.
 * > disk_control->record_enabled will be set to
 *   'false' if core does not support initial
 *   index functionality
 * > disk_control->index_record will be loaded
 *   from file (if an existing record is found)
 * NOTE: Must be called immediately before
 * loading content */
bool disk_control_set_initial_index(
      disk_control_interface_t *disk_control,
      const char *content_path,
      const char *dir_savefile)
{
   if (!disk_control)
      return false;

   if (string_is_empty(content_path))
      goto error;

   /* Check that 'initial index' functionality is enabled */
   if (!disk_control->cb.set_initial_image ||
       !disk_control->cb.get_num_images ||
       !disk_control->cb.get_image_index ||
       !disk_control->cb.get_image_path)
      goto error;

   /* Attempt to initialise disk index record (reading
    * from disk, if file exists) */
   disk_control->record_enabled = disk_index_file_init(
         &disk_control->index_record,
         content_path, dir_savefile);

   /* If record is enabled and initial index is *not*
    * zero, notify current core */
   if (disk_control->record_enabled &&
       (disk_control->index_record.image_index != 0))
   {
      if (!disk_control->cb.set_initial_image(
            disk_control->index_record.image_index,
            disk_control->index_record.image_path))
      {
         /* Note: We don't bother with an on-screen
          * notification at this stage, since an error
          * here may not matter (have to wait until
          * disk index is verified) */
         RARCH_ERR(
               "Failed to set initial disk index: [%u] %s\n",
               disk_control->index_record.image_index,
               disk_control->index_record.image_path);
         return false;
      }
   }

   return true;

error:
   disk_control->record_enabled = false;
   return false;
}

/* Checks that initial index has been set correctly
 * and provides user notification.
 * > Sets disk_control->initial_num_images if
 *   if functionality is supported by core
 * NOTE: Must be called immediately after
 * loading content */
bool disk_control_verify_initial_index(
      disk_control_interface_t *disk_control,
      bool verbosity)
{
   bool success         = false;
   unsigned image_index = 0;
   char image_path[PATH_MAX_LENGTH];

   image_path[0] = '\0';

   if (!disk_control)
      return false;

   /* If index record is disabled, can return immediately */
   if (!disk_control->record_enabled)
      return false;

   /* Check that 'initial index' functionality is enabled */
   if (!disk_control->cb.set_initial_image ||
       !disk_control->cb.get_num_images ||
       !disk_control->cb.get_image_index ||
       !disk_control->cb.get_image_path)
      return false;

   /* Cache initial number of images
    * (required for error checking when saving
    * disk index file) */
   disk_control->initial_num_images =
         disk_control->cb.get_num_images();

   /* Get current image index + path */
   image_index = disk_control->cb.get_image_index();

   if (disk_control->cb.get_image_path(
         image_index, image_path, sizeof(image_path)))
   {
      /* Check whether index + path match set
       * values
       * > Note that if set index was zero and
       *   set path was empty, we ignore the path
       *   read here (since this corresponds to a
       *   'first run', where no existing disk index
       *   file was present) */
      if ((image_index == disk_control->index_record.image_index) &&
          (string_is_equal(image_path, disk_control->index_record.image_path) ||
               ((disk_control->index_record.image_index == 0) &&
                string_is_empty(disk_control->index_record.image_path))))
         success = true;
   }

   /* If current disk is incorrect, notify user */
   if (!success)
   {
      RARCH_ERR(
               "Failed to set initial disk index:\n> Expected [%u] %s\n> Detected [%u] %s\n",
               disk_control->index_record.image_index + 1,
               disk_control->index_record.image_path,
               image_index + 1,
               image_path);

      /* Ignore 'verbosity' setting - errors should
       * always be displayed */
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_SET_INITIAL_DISK),
            0, 60,
            true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      /* Since a failure here typically means that the
       * original M3U content file has been altered,
       * any existing disk index record file will be
       * invalid. We therefore 'reset' and save the disk
       * index record to prevent a repeat of the error on
       * the next run */
      disk_index_file_set(&disk_control->index_record, 0, NULL);
      disk_index_file_save(&disk_control->index_record);
   }
   /* If current disk is correct and recorded image
    * path is empty (i.e. first run), need to register
    * current image path */
   else if (string_is_empty(disk_control->index_record.image_path))
      disk_index_file_set(
            &disk_control->index_record, image_index, image_path);

   /* Regardless of success/failure, notify user of
    * current disk index *if* more than one disk
    * is available */
   if (disk_control->initial_num_images > 1)
   {
      unsigned msg_duration = 0;
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

      disk_control_get_index_set_msg(
            disk_control, disk_control->initial_num_images, image_index, true,
            &msg_duration, msg, sizeof(msg));

      RARCH_LOG("%s\n", msg);

      /* Note: Do not flush message queue here, since
       * it is likely other notifications will be
       * generated before setting the disk index, and
       * we do not want to 'overwrite' them */
      if (verbosity)
         runloop_msg_queue_push(
               msg,
               0, msg_duration,
               false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   return success;
}

/* Saves current disk index to file, if supported
 * by current core */
bool disk_control_save_image_index(
      disk_control_interface_t *disk_control)
{
   if (!disk_control)
      return false;

   /* If index record is disabled, can return immediately */
   if (!disk_control->record_enabled)
      return false;

   /* If core started with less than two disks,
    * then a disk index record is unnecessary */
   if (disk_control->initial_num_images < 2)
      return false;

   /* If current index is greater than initial
    * number of disks then user has appended a
    * disk and it is currently active. This setup
    * *cannot* be restored, so cancel the file save */
   if (disk_control->index_record.image_index >=
         disk_control->initial_num_images)
      return false;

   /* Save record */
   return disk_index_file_save(&disk_control->index_record);
}

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

#ifdef RARCH_INTERNAL
#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int msg_hash_get_help_el_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
    {
       unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

       switch (idx)
       {
          case RARCH_FAST_FORWARD_KEY:
             snprintf(s, len,
                   "Αλλαγή ανάμεσα σε γρήγορη και \n"
                   "κανονική ταχύτητα."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "Κρατήστε πατημένο για γρήγορη ταχύτητα. \n"
                   " \n"
                   "Αφήνοντας το κουμπί απενεργοποιείται η γρήγορη ταχύτητα."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Αλλάζει την αργή ταχύτητα.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Κρατήστε για αρχή ταχύτητα.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "Αλλαγή ανάμεσα σε κατάσταση παύσης και μη παύσης.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "Προπόρευση καρέ κατά την παύση περιεχομένου.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "Εφαρμόζει την επόμενη σκίαση στο ευρετήριο.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "Εφαρμόζει την προηγούμενη σκίαση στο ευρετήριο.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "Κωδικοί.");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "Επαναφορά του περιεχομένου.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "Λήψη στιγμιοτύπου.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "Σίγαση/κατάργηση σίγασης ήχου.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "Ενεργοποιεί το πληκτρολόγιο οθόνης.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Εναλλαγή λειτουργίας παιχνιδιού/παρακολούθησης Netplay.");
             break;
          case RARCH_ENABLE_HOTKEY:
             {
                /* Work around C89 limitations */
                const char *t =
                   "Ενεργοποίηση άλλων πλήκτρων εντολών. \n"
                   " \n"
                   "Εάν αυτό το πλήκτρο είναι συνδεδεμένο είτε με\n"
                   "ένα πληκτρολόγιο ή κάποιο κουμπί χειριστιερίου, \n"
                   "όλα τα υπόλοιπα κουμπιά εντολών θα ενεργοποιηθούν μόνο \n";
                const char *u =
                   "εάν και αυτό είναι πατημένο την ίδια στιγμή. \n"
                   " \n"
                   "Διαφορετικά, όλα τα κουμπιά εντολών πληκτρολογίου \n"
                   "μπορούν να απενεργοποιηθούν από τον χρήστη.";
                strlcpy(s, t, len);
                strlcat(s, u, len);
             }
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                   "Αυξάνει την ένταση του ήχου.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                   "Μειώνει την ένταση του ήχου.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                   "Αλλάζει στο επόμενο επικάλλυμα. Επαναφέρεται.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                   "Ενεργοποιεί την αφαίρεση δίσκων. \n"
                   " \n"
                   "Χρησιμοποιείται για περιεχόμενο με πολλούς δίσκους. ");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                   "Αλλάζει ανάμεσα σε εικόνες δίσκων. Χρησιμοποιείστε μετά την εξαγωγή. \n"
                   " \n"
                   "Ολοκληρώστε πατώντας εξαγωγή ξανά.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                   "Ενεργοποίηση ελέγχου ποντικιού. \n"
                   " \n"
                   "Όταν το ποντίκι ελέγχεται, το RetroArch κρύβει το \n"
                   "ποντίκι, και κρατάει τον δρομέα μέσα στο \n"
                   "παράθυρο για να επιτρέψει την σχετική εισαγωγή \n"
                   "ώστε να λειτουργήσει καλύτερα.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Ενεργοποίηση εστίασης παιχνιδιού.\n"
                   " \n"
                   "Όταν ένα παιχνίδι έχει την εστίαση, το RetroArch θα απενεργοποιήσει \n"
                   "τα κουμπιά εντολών και θα κρατήσει τον δρομέα του ποντικιού μέσα στο παράθυρο.");
             break;
          case RARCH_MENU_TOGGLE:
             snprintf(s, len, "Ενεργοποιεί το μενού.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "Φορτώνει κατάσταση.");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "Ενεργοποιεί την πλήρη οθόνη.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
                   "Κουμπί καθαρής εξόδου από το RetroArch. \n"
                   " \n"
                   "Η απότομη απενεργοποίηση της λειτουργίας του θα \n"
                   "τερματίσει το RetroArch χωρίς να αποθηκεύσει την RAM κ.α."
#ifdef __unix__
                   "\nΣε Unix-likes, SIGINT/SIGTERM επιτρέπουν την \n"
                   "καθαρή απενεργοποίηση."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "Θυρίδες κατάστασης. \n"
                   " \n"
                   "Με την θυρίδα τοποθετημένη στο 0, το όνομα της αποθηκευμένης κατάστασης είναι \n"
                   "*.state (ή οτιδήποτε έχει καθοριστεί στην γραμμή εντολών). \n"
                   " \n"
                   "Όταν η θυρίδα δεν είναι 0, η διαδρομή θα είναι <διαδρομή><d>, \n"
                   "όπου <d> είναι ο αριθμός θυρίδας.");
             break;
          case RARCH_SAVE_STATE_KEY:
             snprintf(s, len,
                   "Αποθηκεύει την κατάσταση.");
             break;
          case RARCH_REWIND:
             snprintf(s, len,
                   "Κρατήστε το κουμπί για επιστροφή προς τα πίσω. \n"
                   " \n"
                   "Η επιστροφή προς τα πίσω πρέπει να είναι ενεργοποιημένη.");
             break;
          case RARCH_BSV_RECORD_TOGGLE:
             snprintf(s, len,
                   "Αλλαγή ανάμεσα σε εγγραφή ή όχι.");
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
            snprintf(s, len, "Στοιχεία σύνδεσης για τον \n"
                    "λογαριασμό Retro Achievements. \n"
                    " \n"
                    "Επισκεφθείτε το retroachievements.org και εγγραφείτε \n"
                    "για δωρεάν λογαριασμό. \n"
                    " \n"
                    "Μετά την ολοκλήρωση της εγγραφής, πρέπει να \n"
                    "εισάγετε το όνομα χρήστη και τον κωδικό στο \n"
                    "RetroArch.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len, "Όνομα χρήστη για τον λογαριασμό σας στο Retro Achievements.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len, "Κωδικός για τον λογαριασμό σας στο Retro Achievements.");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            {
               /* Work around C89 limitations */
               const char *t =
                  "Τοπικοποίηση του μενού και όλων των μηνυμάτων \n"
                  "ανάλογα με την γλώσσα που έχετε επιλέξει \n"
                  "εδώ. \n"
                  " \n"
                  "Χρειάζεται επανεκκίνηση για να ενεργοποιηθούν \n"
                  "οι αλλαγές. \n";
               const char *u =
                  " \n"
                  "Σημείωση: πιθανόν να μην έχουν εφαρμοστεί \n"
                  "όλες οι γλώσσες. \n"
                  " \n"
                  "Σε περίπτωση που μία γλώσσα δεν έχει εφαρμοστεί, \n"
                  "χρησιμοποιούμε τα Αγγλικά.";
               strlcpy(s, t, len);
               strlcat(s, u, len);
            }
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            snprintf(s, len, "Αλλαγή της γραμματοσειράς που χρησιμοποιείται \n"
                    "για το κείμενο της Οθόνης Απεικόνισης.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "Αυτόματη φόρτωση επιλογών πυρήνα βάση περιεχομένου.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Αυτόματη φόρτωση ρυθμίσεων παράκαμψης.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Αυτόματη φόρτωση αρχείων αναδιοργάνωσης πλήκτρων.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Οργάνωση καταστάσεων αποθήκευσης σε φακέλους \n"
                    "ονομασμένες με βάση τον πυρήνα libretro που χρησιμοποιούν.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Οργάνωση αρχείων αποθήκευσης σε φακέλους \n"
                    "ονομασμένα με βάση τον πυρήνα libretro που χρησιμοποιούν.");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Έξοδος από το μενού και επιστροφή \n"
                    "στο περιεχόμενο.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "Επανεκκινεί το περιεχόμενο από την αρχή.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "Κλείνει το περιεχόμενο και το αποφορτώνει από την \n"
                    "μνήμη.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Εάν μία κατάσταση φορτώθηκε, το περιεχόμενο \n"
                    "θα επανέλθει στην κατάσταση πριν την φόρτωση.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Εάν μία κατάσταση αντικαταστάθηκε, θα \n"
                    "επανέλθει στην προηγούμενη κατάσταση αποθήκευσης.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Δημιουργία στιγμιοτύπου. \n"
                    " \n"
                    "Το στιγμιότυπο θα αποθηκευθεί στην \n"
                    "Διαδρομή Στιγμιοτύπων.");
            break;
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Προσθήκη της καταχώρισης στα Αγαπημένα.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "Έναρξη περιεχομένου.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "Προβολή περισσότερων μεταδεδομένων πληροφοριών \n"
                    "σχετικά με το περιεχόμενο.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Αρχείο Διαμόρφωσης.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Συμπιεσμένο αρχείο.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Αρχείο διαμόρφωσης καταγραφών.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Αρχείο ερωτήματος βάσης δεδομένων.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Αρχείο Διαμόρφωσης.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Επέλεξε αυτό για να ανιχνεύσεις περιεχόμενο στην \n"
                             "τρέχουσα διαδρομή.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Επέλεξε αυτό για να ορίσεις αυτήν ως την διαδρομή.");
            break;
        case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            snprintf(s, len,
                     "Ευρετήριο Βάσης Δεδομένων Περιεχομένου. \n"
                             " \n"
                             "Διαδρομή για το ευρετήριο της βάσης δεδομένων \n"
                             "περιεχομένου.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Ευρετήριο Μικρογραφιών. \n"
                             " \n"
                             "Για την αποθήκευση αρχείων μικρογραφιών.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Ευρετήριο Πληροφοριών Πυρήνων. \n"
                             " \n"
                             "Ένα ευρετήριο για το που να ψάξεις \n"
                             "για πληφοροφίες των πυρήνων libretro.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Ευρετήριο Λιστών Αναπαραγωγής. \n"
                             " \n"
                             "Αποθηκέυστε όλα τα αρχεία λιστών αναπαραγωγής \n"
                             "σε αυτό το ευρετήριο.");
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
static const char *menu_hash_to_str_el_label_enum(enum msg_hash_enums msg)
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

const char *msg_hash_to_str_el(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_el_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_el.h"
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

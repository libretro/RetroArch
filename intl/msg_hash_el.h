#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

MSG_HASH(
    MSG_COMPILER,
    "Μεταγλωττιστής"
    )
MSG_HASH(
    MSG_UNKNOWN_COMPILER,
    "Άγνωστος Μεταγλωττιστής"
    )
MSG_HASH(
    MSG_NATIVE,
    "Ντόπιος"
	)
MSG_HASH(
    MSG_DEVICE_DISCONNECTED_FROM_PORT,
    "Η συσκευή αποσυνδέθηκε από την θύρα"
    )
MSG_HASH(
    MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
    "Λήφθηκε άγνωστη εντολή netplay"
    )
MSG_HASH(
    MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
    "Το αρχείο υπάρχει ήδη. Αποθήκευση σε εφεδρική ενδιάμεση μνήμη."
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM,
    "Λήφθηκε σύνδεση από: \"%s\""
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM_NAME,
    "Λήφθηκε σύνδεση από: \"%s (%s)\""
    )
MSG_HASH(
    MSG_PUBLIC_ADDRESS,
    "Δημόσια διεύθυνση"
    )
MSG_HASH(
    MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
    "Δεν παρασχέθηκε διαφωνία και δεν υπάρχει ενσωματωμένο μενού, εμφάνιση βοήθειας..."
    )
MSG_HASH(
    MSG_SETTING_DISK_IN_TRAY,
    "Τοποθέτηση δίσκου στην μονάδα δίσκου"
    )
MSG_HASH(
    MSG_WAITING_FOR_CLIENT,
    "Αναμονή για πελάτη ..."
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
    "Αποσυνδεθήκατε από το παιχνίδι"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
    "Έχετε συνδεθεί ως παίκτης %u"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
    "Έχετε συνδεθεί με συσκευές εισόδου %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_PLAYER_S_LEFT,
    "Ο παίκτης %.*s αποσυνδέθηκε από το παιχνίδι"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
    "%.*s συνδέθηκε ως παίκτης %u"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
    "%.*s συνδέθηκε με συσκευές εισόδου %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_NOT_RETROARCH,
    "Η προσπάθεια σύνδεσης netplay απέτυχε επειδή ο συμπέκτης δεν χρησιμοποιεί το RetroArch ή χρησιμοποιεί πιο παλιά έκδοση."
    )
MSG_HASH(
    MSG_NETPLAY_OUT_OF_DATE,
    "Ο συμπαίκτης χρησιμοποιεί πιο παλιά έκδοση RetroArch. Αδύνατη η σύνδεση."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_VERSIONS,
    "ΠΡΟΕΙΔΟΠΟΙΗΣΗ: Ο συμπαίκτης netplay χρησιμοποιεί διαφορετική έκδοση του RetroArch. Εάν προκύψουν προβλήματα χρησιμοποιήστε την ίδια έκδοση."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORES,
    "Ο συμπαίκτης netplay χρησιμοποιεί διαφορειτκό πυρήνα. Αδύνατη η σύνδεση."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
    "ΠΡΟΕΙΔΟΠΟΙΗΣΗ: Ο συμπαίκτης netplay χρησιμοποιεί διαφορετική έκδοση του πυρήνα. Εάν προκύψουν προβλήματα χρησιμοποιήστε την ίδια έκδοση."
    )
MSG_HASH(
    MSG_NETPLAY_ENDIAN_DEPENDENT,
    "Αυτός ο πυρήνας δεν υποστηρίζει σύνδεση διαφορετικών πλατφόρμων για netplay ανάμεσα σε αυτά τα συστήματα"
    )
MSG_HASH(
    MSG_NETPLAY_PLATFORM_DEPENDENT,
    "Αυτός ο πυρήνας δεν υποστηρίζει σύνδεση διαφορετικών πλατφόρμων για netplay"
    )
MSG_HASH(
    MSG_NETPLAY_ENTER_PASSWORD,
    "Εισάγετε κωδικό διακομιστή netplay:"
    )
MSG_HASH(
    MSG_NETPLAY_INCORRECT_PASSWORD,
    "Λάθος κωδικός"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_NAMED_HANGUP,
    "\"%s\" αποσυνδέθηκε"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_HANGUP,
    "Ένας πελάτης netplay έχει αποσυνδεθεί"
    )
MSG_HASH(
    MSG_NETPLAY_CLIENT_HANGUP,
    "Αποσύνδεση netplay"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
    "Δεν έχετε άδεια για να παίξετε"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
    "Δεν υπάρχουν κενές θέσεις παικτών"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
    "Οι συσκευές εισόδου που ζητήθηκαν δεν είναι διαθέσιμες"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY,
    "Δεν μπορεί να γίνει αλλαγή σε κατάσταση παιχνιδιού"
    )
MSG_HASH(
    MSG_NETPLAY_PEER_PAUSED,
    "Ο συμπαίκτης netplay \"%s\" έκανε παύση"
    )
MSG_HASH(
    MSG_NETPLAY_CHANGED_NICK,
    "Το ψευδώνυμο σας άλλαξε σε \"%s\""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SETTINGS,
    "Προσαρμόζει τις εμφανισιακές ρυθμίσεις της οθόνης του μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
    "Σκληρός συγχρονισμός επεξεργαστή και κάρτας γραφικών. Μειώνει την καθυστέρηση με τίμημα την επίδοση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_THREADED,
    "Βελτιώνει την επίδοση με τίμημα την καθυστέρηση και περισσότερα κολλήματα στο βίντεο. Χρησιμοποιείστε μόνο εάν δεν μπορείτε να αποκτήσετε πλήρη ταχύτητα με άλλον τρόπο."
    )
MSG_HASH(
    MSG_AUDIO_VOLUME,
    "Ένταση ήχου"
    )
MSG_HASH(
    MSG_AUTODETECT,
    "Αυτόματη ανίχνευση"
    )
MSG_HASH(
    MSG_AUTOLOADING_SAVESTATE_FROM,
    "Αυτόματη φόρτωση κατάστασης αποθήκευσης από"
    )
MSG_HASH(
    MSG_CAPABILITIES,
    "Ικανότητες"
    )
MSG_HASH(
    MSG_CONNECTING_TO_NETPLAY_HOST,
    "Σύνδεση με εξυπηρετητή netplay"
    )
MSG_HASH(
    MSG_CONNECTING_TO_PORT,
    "Σύνδεση στην θύρα"
    )
MSG_HASH(
    MSG_CONNECTION_SLOT,
    "Θέση σύνδεσης"
    )
MSG_HASH(
    MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
    "Συγγνώμη, μη εφαρμοσμένο: πυρήνες που δεν απαιτούν περιεχόμενο δεν μπορούν να συμμετέχουν στο netplay."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
    "Κωδικός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
    "Επιτεύγματα Λογαριασμού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
    "Όνομα Χρήστη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
    "Λογαριασμοί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
    "Επιτεύγματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
    "Παύση Σκληροπυρηνικής Λειτουργίας Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
    "Συνέχιση Σκληροπυρηνικής Λειτουργίας Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
    "Επιτεύγματα (Σκληροπυρηνικά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
    "Σάρωση Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
    "Διαμορφώσεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TAB,
    "Εισαγωγή περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
    "Ερώτηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
    "Εργαλεία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
    "Φραγή Καρέ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
    "Συσκευή Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
    "Οδηγός Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
    "Πρόσθετο Ήχου DSP"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
    "Ενεργοποίηση Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
    "Φίλτρα Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
    "Turbo/Νεκρή Ζώνη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
    "Καθυστέρηση Ήχου (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
    "Μέγιστη Χρονική Διαστρέβλωση Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
    "Σίγαση Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
    "Συχνότητα Εξόδου Ήχου (Hz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
    "Δυναμικός Έλεγχος Βαθμού Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
    "Οδηγός Επαναδειγματολήπτη Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
    "Ήχος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
    "Συγχρονισμός Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
    "Ένταση Ήχου (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "Αποκλειστική Λειτουργία WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
    "Ασταθής Μορφή WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "Μήκος Κοινόχρηστης Ενδιάμεσης Μνήμης WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
    "Διάστημα Αυτόματης Αποθήκευσης SaveRAM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
    "Φόρτωση Αρχείων Παράκαμψης Αυτόματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
    "Φόρτωση Αρχείων Αναδιοργάνωσης Πλήτρκων Αυτόματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
    "Φόρτωση Προεπιλογών Σκιάσεων Αυτόματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
    "Πίσω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
    "Επιβεβαίωση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
    "Έξοδος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
    "Μετακίνηση Προς Τα Κάτω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
    "Μετακίνηση Προς Τα Πάνω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
    "Εκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
    "Ενεργοποίηση/Απενεργοποίηση Πληκτρολογίου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
    "Ενεργοποίηση/Απενεργοποίηση Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
    "Βασικός χειρισμός μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
    "Επιβεβαίωση/ΟΚ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
    "Έξοδος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
    "Μετακίνηση Προς Τα Πάνω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
    "Προεπιλογές"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
    "Ενεργοποίηση/Απενεργοποίηση Πληκτρολογίου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
    "Ενεργοποίηση/Απενεργοποίηση Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
    "Απενεργοποίηση αντικατάστασης SaveRAM κατά την φάση φόρτωσης κατάστασης αποθήκευσης"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
    "Ενεργοποίηση Bluetooth"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
    "Σύνδεσμος Εργαλείων του Buildbot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
    "Κρυφή Μνήμη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
    "Επίτρεψη Κάμερας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
    "Οδηγός Κάμερας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT,
    "Απάτη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
    "Εφαρμογή Αλλαγών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
    "Έναρξη Αναζήτησης Για Νέους Κωδικούς Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
    "Συνέχιση Αναζήτησης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
    "Αρχεία Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
    "Αρχείο Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
    "Φόρτωση Αρχείου Απάτης (Αντικατάσταση)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
    "Φόρτωση Αρχείου Απάτης (Προσάρτηση)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
    "Αποθήκευση Αρχείου Απάτης Ως"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
    "Φορές Περάσματος Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
    "Περιγραφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Σκληροπυρηνική Λειτουργία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
    "Κατατάξεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
    "Εμβλήματα Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
    "Κλειδωμένο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
    "Δοκιμή Ανεπίσημων Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
    "Ξεκλείδωτο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
    "Σκληροπυρηνικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
    "Βερμπαλιστική Λειτουργία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
    "Αυτόματο Στιγμιότυπο Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
    "Κλείσιμο Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG,
    "Διαμόρφωση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
    "Φόρτωση Διαμορφώσεων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
    "Διαμόρφωση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
    "Απόθηκευση Διαμόρφωσης στην Έξοδο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
    "Βάσεις Δεδομένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
    "Περιεχόμενο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
    "Μέγεθος Λίστας Ιστορικού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
    "Επίτρεψη αφαίρεσης καταχωρήσεων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
    "Γρήγορο Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
    "Λήψεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
    "Λήψεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
    "Απάτες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
    "Μετρητές Πυρήνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
    "Εμφάνιση ονόματος πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
    "Πληροφορίες πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
    "Δημιουργοί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
    "Κατηγορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
    "Επιγραφή πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
    "Όνομα πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
    "Firmware(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
    "Άδεια(ες)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
    "Άδειες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
    "Υποστηριζόμενες επεκτάσεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
    "Κατασκευαστής συστήματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
    "Όνομα συστήματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
    "Χειρισμοί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_LIST,
    "Φόρτωση Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
    "Επιλογές"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
    "Πυρήνας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
    "Αυτόματη Έναρξη Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "Αυτόματη εξαγωγή ληφθέντος συμπιεσμένου αρχείου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
    "Σύνδεσμος Buildbot Πυρήνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
    "Ενημέρωση Πυρήνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
    "Ενημερωτής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
    "Αρχιτεκτονική Επεξεργαστή:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_CORES,
    "Πυρήνες Επεξεργαστή:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
    "Δρομείς"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
    "Διαχειριστής Δρομέα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
    "Προτιμώμενη Αναλογία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
    "Διαχειριστής Βάσης Δεδομένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
    "Επιλογή Βάσης Δεδομένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
    "Κατάργηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES,
    "Ευρετήριο έναρξης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
    "<Ευρετήριο περιεχομένων>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
    "<Προκαθορισμένο>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
    "<Κανένα>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
    "Το ευρετήριο δεν βρέθηκε."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
    "Ευρετήρια"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
    "Λήψεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
    "Λήψη Πυρήνα..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
    "Λήψη Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
    "Οδηγοί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
    "Φόρτωση Dummy στο Κλείσιμο Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
    "Έλεγχος για απών Firmware Πριν την Φόρτωση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
    "Δυναμικό Φόντο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Δυναμικά Φόντα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
    "Ενεργοποίηση Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FALSE,
    "Ψευδές"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
    "Μέγιστη Ταχύτητα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
    "Αγαπημένα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_SHOW,
    "Προβολή Ρυθμού Καρέ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
    "Περιορισμός Μέγιστης Ταχύτητας Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
    "Συγχρονισμός με τον Ακριβή Ρυθμό Καρέ του Περιεχομένου (G-Sync, FreeSync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
    "Περιορισμός Καρέ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
    "Φόρτωση Επιλογών Πυρήνα Βάση Συγκεκριμένου Περιεχομένου Αυτόματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
    "Δημιουργία αρχείου επιλογών παιχνιδιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
    "Αποθήκευση αρχείου επιλογών παιχνιδιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP,
    "Βοήθεια"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
    "Αντιμετώπιση Προβλημάτων Ήχου/Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
    "Αλλαγή Επικαλύμματος Εικονικού Χειριστηρίου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
    "Βασικός Χειρισμός Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LIST,
    "Βοήθεια"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
    "Φόρτωση Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
    "Σάρωση Για Περιεχόμενο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
    "Τι Είναι Ο Πυρήνας;"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
    "Ενεργοποίηση Λίστας Ιστορικού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
    "Ιστορικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
    "Οριζόντιο Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
    "Εικόνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
    "Τύπος Αναλογικού Σε Ψηφιακό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
    "Όλοι Οι Χρήστες Χειρίζονται Το Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
    "Αριστερό Αναλογικό X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
    "Αριστερό Αναλογικό X- (αριστερά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
    "Αριστερό Αναλογικό X+ (δεξιά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
    "Αριστερό Αναλογικό Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
    "Αριστερό Αναλογικό Y- (πάνω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
    "Αριστερό Αναλογικό Y+ (κάτω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
    "Δεξί Αναλογικό X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
    "Δεξί Αναλογικό X- (αριστερά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
    "Δεξί Αναλογικό X+ (δεξιά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
    "Δεξί Αναλογικό Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
    "Δεξί Αναλογικό Y- (πάνω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
    "Δεξί Αναλογικό Y+ (κάτω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
    "Σκανδάλη Όπλου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
    "Γέμισμα Όπλου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
    "Όπλο Aux A"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
    "Όπλο Aux B"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
    "Όπλο Aux C"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
    "Όπλο Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
    "Όπλο Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
    "Όπλο D-pad Πάνω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
    "Όπλο D-pad Κάτω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
    "Όπλο D-pad Αριστερά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
    "Όπλο D-pad Δεξιά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
    "Ενεργοποίηση Αυτόματης Διαμόρφωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
    "Εναλλαγή Κουμπιών Επιβεβαίωσης & Ακύρωσης Στο Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
    "Σύνδεση Όλων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
    "Επαναφορά Συνδέσεων Όλων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
    "Λήξη Χρόνου Σύνδεσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
    "Κράτημα Σύνδεσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
    "Hide Unbound Core Input Descriptors"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
    "Display Input Descriptor Labels"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
    "Κατάλογος Συσκευών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
    "Τύπος Συσκευής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
    "Κατάλογος Ποντικιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
    "Οδηγός Εισαγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
    "Σύνδεση Πλήκτρων Εντολών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
    "Κουμπί A (δεξιά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
    "Κουμπί B (κάτω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
    "D-pad κάτω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
    "Κουμπί L2 (σκανδάλι)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
    "Κουμπί L3 (αντίχειρας)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
    "Κουμπί L (πίσω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
    "D-pad αριστερό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
    "Κουμπί R2 (σκανδάλι)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
    "Κουμπί R3 (αντίχειρας)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
    "Κουμπί R (πίσω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
    "D-pad δεξί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
    "Κουμπί Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
    "Κουμπί Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
    "D-pad πάνω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
    "Κουμπί X (πάνω)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
    "Κουμπί Y (αριστερό)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEY,
    "(Κουμπί: %s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
    "Ποντίκι 1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
    "Ποντίκι 2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
    "Ποντίκι 3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
    "Ποντίκι 4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
    "Ποντίκι 5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
    "Ροδέλα Πάνω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
    "Ροδέλα Κάτω"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
    "Ροδέλα Αριστερά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
    "Ροδέλα Δεξιά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
    "Μέγιστοι Χρήστες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Συνδιασμός Πλήκτρων Χειριστηρίου για Άνοιγμα Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
    "Κατάλογος απάτης -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
    "Κατάλογος απάτης +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
    "Απάτες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
    "Εξαγωγή δίσκου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
    "Επόμενος δίσκος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
    "Προηγούμενος δίσκος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
    "Ενεργοποίηση πλήκτρων εντολών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
    "Παύση γρήγορης κίνησης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
    "Γρήγορη κίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
    "Πλήρης οθόνη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
    "Κλείδωμα ποντικιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
    "Εστίαση παιχνιδιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
    "Μενού επιφάνειας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
    "Φόρτωση κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
    "Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
    "Σίγαση Ήχου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
    "Εναλλαγή κατάστασης παιχνιδιού/θεατή Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
    "Πληκτρολόγιο οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
    "Επόμενο επικάλλυμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
    "Παύση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "Έξοδος από το RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
    "Επαναφορά παιχνιδιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
    "Επιστροφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
    "Λεπτομέρειες Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
    "Αποθήκευση κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
    "Λήψη Στιγμιότυπου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
    "Επόμενη σκίαση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
    "Προηγούμενη σκίαση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
    "Παύση αργής κίνησης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
    "Αργή κίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
    "Θέση κατάστασης αποθήκευσης -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
    "Θέση κατάστασης αποθήκευσης +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
    "Ένταση -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
    "Ένταση +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
    "Εμφάνιση Επικαλύμματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
    "Απόκρυψη Επικαλύμματος Στο Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Εμφάνιση Εισαγωγών Στο Επικάλλυμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Εμφάνιση Θύρας Εισαγωγών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
    "Τύπος Συμπεριφοράς Συγκέντρωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
    "Νωρίς"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
    "Αργά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
    "Φυσιολογικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
    "Αποθήκευση Αυτόματης Διαμόρφωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
    "Εισαγωγή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
    "Ενεργοποίηση Μικρού Πληκτρολογίου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
    "Ενεργοποίηση Αφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
    "Ενεργοποίηση Turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS, /* TODO/FIXME - Change User to Port */
    "Σύνδεση Πλήκτρων Εισόδου Χρήστη %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
    "Καθυστέρηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
    "Οδηγός Joypad"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
    "Υπηρεσίες"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
    "Κινέζικα (Απλοποιημένα)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
    "Κινέζικα (Παραδοσιακά)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
    "Ολλανδός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
    "Αγγλικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
    "Εσπεράντο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
    "Γαλλική γλώσσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
    "Γερμανός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
    "Ιταλικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
    "Ιαπωνικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
    "Κορεατικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_POLISH,
    "Πολωνία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
    "Πορτογαλικά (Βραζιλία)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
    "Πορτογαλικά (Πορτογαλία)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
    "Ρωσική"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
    "Ισπανικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
    "Βιετναμέζος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
    "Αραβικός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GREEK,
    "Ελληνικά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
    "Τούρκικος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
    "Αριστερό Αναλογικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
    "Πυρήνας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
    "Πληροφορίες Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LINEAR,
    "Γραμμικός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
    "Φόρτωση Αρχείου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
    "Φόρτωση Πρόσφατου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
    "Φόρτωση Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_STATE,
    "Φόρτωση Κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
    "Επίτρεψη Τοποθεσίας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
    "Οδηγός Τοποθεσίας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
    "Αρχείο Καταγραφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
    "Logging Verbosity"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MAIN_MENU,
    "Κεντρικό Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANAGEMENT,
    "Ρυθμίσεις Βάσης Δεδομένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
    "Χρώμα Θέματος Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
    "Μπλε"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
    "Μπλε Γκρι"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
    "Σκούρο Μπλε"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
    "Πράσινο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
    "Shield"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
    "Κόκκινο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
    "Κίτρινο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
    "Footer Opacity"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
    "Header Opacity"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
    "Οδηγός Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
    "Εμφάνιση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
    "Φόντο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MISSING,
    "Λείπει"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
    "Υποστήριξη Ποντικιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
    "Πολυμέσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
    "Μουσική"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Φιλτράρισμα άγνωστων επεκτάσεων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
    "Navigation Wrap-Around"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NEAREST,
    "Κοντινότερο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
    "Ενεργοποίηση Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
    "Σύνδεση σε οικοδεσπότη netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
    "Έναρξη netplay ως οικοδεσπότης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
    "Λήξη netplay ως οικοδεσπότης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
    "Διέυθυνση Διακομιστή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
    "Σάρωση τοπικού δικτύου Scan local network"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
    "Όνομα Χρήστη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
    "Κωδικός Διακομιστή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
    "Δημόσια Ανακοίνωση Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
    "Ρυθμίσεις Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
    "Μέγιστο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
    "Μέσος Όρος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
    "Κοινοποίηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
    "Ψήφος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
    "Κανείς"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
    "Καμία προτίμηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
    "Πληροφορίες Δικτύου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
    "Χειριστήριο Δικτύου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
    "Δίκτυο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO,
    "Όχι"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NONE,
    "Τίποτα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
    "Μ/Δ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
    "Δεν υπάρχουν επιτεύγματα προς προβολή."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE,
    "Κανένας Πυρήνας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
    "Δεν υπάρχουν διαθέσιμοι πυρήνες."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
    "Δεν υπάρχουν διαθέσιμες πληροφορίες πυρήνα."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
    "Δεν υπάρχουν διαθέσιμες επιλογές πυρήνα."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
    "Δεν υπάρχουν καταχωρήσεις προς εμφάνιση."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
    "Δεν υπάρχει διαθέσιμο ιστορικό."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
    "Δεν υπάρχουν διαθέσιμες πληροφορίες."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ITEMS,
    "Δεν υπάρχουν αντικείμενα."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
    "Δεν βρέθηκαν εξυπηρετητές netplay."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
    "Δεν βρέθηκαν δίκτυα."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
    "Δεν βρέθηκαν λίστες αναπαραγωγής."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
    "Δεν υπάρχουν διαθέσιμες καταχωρήσεις στην λίστα αναπαραγωγής."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
    "Δεν βρέθηκαν ρυθμίσεις."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
    "Δεν βρέθηκαν παράμετροι σκίασης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
    "Διαδικτυακός Ενημερωτής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
    "Οθόνη Απεικόνισης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
    "Επικάλλυμα Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
    "Προσαρμογή Προσόψεων και Χειρισμών Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Ειδοποιήσεις Οθόνης Απεικόνισης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Προσαρμόστε τις Ειδοποιήσεις Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
    "Περιήγηση Αρχείου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPTIONAL,
    "Προεραιτικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY,
    "Επικάλλυμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
    "Αυτόματη Φόρτωση Προτιμώμενου Επικαλύμματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
    "Επικάλλυμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
    "Διαφάνεια Επικαλύμματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
    "Προκαθορισμένο Επικάλλυμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
    "Κλίμακα Επικαλύμματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
    "Επικάλλυμα Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
    "Χρήση Λειτουργίας PAL60"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
    "Προηγούμενο ευρετήριο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
    "Παύση όταν ενεργοποιείται το μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
    "Μην εκτελείτε στο παρασκήνιο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
    "Λίστες Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
    "Λίστα Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
    "Λίστες Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
    "Υποστήριξη Αφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PORT,
    "Θύρα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
    "Ιδιωτικότητα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Έξοδος από RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
    "Όνομα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
    "Έναρξη Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Επανεκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
    "Εγγραφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
    "Οδηγός Εγγραφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
    "Οδηγός MIDI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
    "Ενεργοποίηση Εγγραφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_PATH,
    "Αποθήκευση Εγγραφής Ως..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
    "Αποθήκευση Εγγραφών στο Ευρετήριο Εξαγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REQUIRED,
    "Απαραίτητο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
    "Επανεκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
    "Επανεκκίνηση RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME,
    "Συνέχιση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
    "Συνέχιση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
    "RetroPad με Αναλογικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
    "Επιτεύγματα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
    "Ενεργοποίηση Επιστροφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
    "Εφαρμογή Μετά Την Ενεργοποίηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
    "Αυτόματη Εφαρμογή Απατών Κατά την Φόρτωση Παιχνιδιού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
    "Βαθμός Λεπτομέρειας Επιστροφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
    "Μέγεθος Ενδιάμεσης Μνήμης Επιστροφής (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
    "Βήμα Μεγέθους Ενδιάμεσης Μνήμης Επιστροφής (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
    "Επιστροφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
    "Ρυθμίσεις Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
    "Λεπτομέρειες Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
    "Έναρξη ή Συνέχιση Αναζήτησης Απάτης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
    "Περιηγητής Αρχείων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
    "Config"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
    "Εμφάνιση Αρχικής Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
    "Δεξί Αναλογικό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
    "Προσθήκη στα Αγαπημένα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
    "Προσθήκη στα Αγαπημένα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
    "Επαναφορά Συσχέτισης Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN,
    "Εκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
    "Εκκίνηση"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
    "Ενεργοποίηση SAMBA"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
    "Αρχείο Αποθήκευσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
    "Αποθήκευση Τρέχουσας Διαμόρφωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
    "Αποθήκευση Νέας Διαμόρφωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_STATE,
    "ποθήκευση Κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
    "Αποθήκευση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
    "Σάρωση Ευρετηρίου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_FILE,
    "Σάρωση αρχείου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
    "<Σάρωση Αυτού Του Ευρετηρίου>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
    "Στιγμιότυπο Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
    "Ανάλυση Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SEARCH,
    "Αναζήτηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SECONDS,
    "δευτερόλεπτα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS,
    "Ρυθμίσεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
    "Ρυθμίσεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER,
    "Σκίαση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
    "Εφαμοργή Αλλαγών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
    "Σκιάσεις"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
    "Κορδέλλα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
    "Κορδέλλα (απλοποιημένη)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
    "Απλό Χιόνι"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
    "Χιόνι"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
    "Εμφάνιση Ρυθμίσεων Για Προχωρημένους"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
    "Εμφάνιση Κρυφών Αρχείων και Φακέλων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHUTDOWN,
    "Τερματισμός"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
    "Ενεργοποίηση SSH"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_CORE,
    "Έναρξη Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
    "Έναρξη Απομακρυσμένου RetroPad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
    "Έναρξη Επεξεργαστή Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATE_SLOT,
    "Θέση Κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATUS,
    "Κατάσταση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
    "Εντολές stdin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
    "Προτεινόμενοι πυρήνες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
    "Αναστολή Προφύλαξης Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
    "System BGM Enable"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
    "Σύστημα/BIOS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
    "Πληροφορίες Συστήματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
    "Υποστήριξη 7zip"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
    "Υποστήριξη ALSA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
    "Ημερομηνία Κατασκευής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
    "Υποστήριξη Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
    "Υποστήριξη Cocoa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
    "Υποστήριξη Γραμμής Εντολών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
    "Υποστήριξη CoreText"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
    "Χαρακτηριστικά Επεξεργαστή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
    "DPI Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
    "Ύψος Οθόνης (mm)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
    "Πλάτος Οθόνης (mm)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
    "Υποστήριξη DirectSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
    "Υποστήριξη WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
    "Υποστήριξη δυναμικής βιβλιοθήκης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
    "Δυναμική φόρτωση κατά την εκτέλεση της βιβλιοθήκης libretro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
    "Υποστήριξη EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
    "Υποστήριξη OpenGL/Direct3D render-to-texture (multi-pass shaders)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
    "Υποστήριξη FFmpeg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
    "Υποστήριξη FreeType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
    "Υποστήριξη STB TrueType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
    "Αναγνωριστικό λειτουργικού συστήματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
    "Όνομα λειτουργικού συστήματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
    "Λειτουργικό Σύστημα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
    "Έκδοση Git"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
    "Υποστήριξη GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
    "Υποστήριξη HLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
    "Υποστήριξη JACK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
    "Υποστήριξη KMS/EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
    "Έκδοση Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
    "Υποστήριξη LibretroDB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
    "Υποστήριξη Libusb"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
    "Υποστήριξη Netplay (peer-to-peer)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
    "Υποστήριξη Γραμμής Εντολών Δικτύου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
    "Υποστήριξη Χειριστηρίου Δικτύου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
    "Υποστήριξη OpenAL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
    "Υποστήριξη OpenGL ES"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
    "Υποστήριξη OpenGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
    "Υποστήριξη OpenSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
    "Υποστήριξη OpenVG"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
    "Υποστήριξη OSS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
    "Υποστήριξη Επικαλλυμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
    "Πηγή ρεύματος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
    "Φορτισμένο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
    "Φορτίζει"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
    "Ξεφορτίζει"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
    "Καμία πηγή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
    "Υποστήριξη PulseAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
    "Υποστήριξη Python (υποστήριξη script στις σκιάσεις)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
    "Υποστήριξη BMP (RBMP)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
    "Επίπεδο RetroRating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
    "Υποστήριξη JPEG (RJPEG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
    "Υποστήριξη RoarAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
    "Υποστήριξη PNG (RPNG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
    "Υποστήριξη RSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
    "Υποστήριξη TGA (RTGA)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
    "Υποστήριξη SDL2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
    "Υποστήριξη Εικόνων SDL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
    "Υποστήριξη SDL1.2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
    "Υποστήριξη Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
    "Υποστήριξη Threading"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
    "Υποστήριξη Udev"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
    "Υποστήριξη Video4Linux2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
    "Οδηγός video context"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
    "Υποστήριξη Vulkan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
    "Υποστήριξη Metal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
    "Υποστήριξη Wayland"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
    "Υποστήριξη X11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
    "Υποστήριξη XAudio2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
    "Υποστήριξη XVideo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
    "Υποστήριξη Zlib"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
    "Λήψη Στιγμιότυπου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
    "Threaded tasks"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS,
    "Σκίτσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
    "Σκίτσα Αριστερά"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
    "Thumbnails Vertical Disposition"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
    "Σκίτσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
    "Ενημερωτής Σκίτσων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
    "Εξώφυλλα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
    "Στιγμιότυπα Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
    "Οθόνες Τίτλων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
    "Εμφάνιση ημερομηνίας / ώρας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
    "Στυλ ημερομηνίας / ώρας"
    )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Αλλάζει το στυλ της τρέχουσας ημερομηνίας ή και ώρας που φαίνεται μέσα στο μενού."
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS,
   "ΧΧΧΧ-ΜΜ-ΗΗ ΩΩ:ΛΛ:ΔΔ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM,
   "ΧΧΧΧ-ΜΜ-ΗΗ ΩΩ:ΛΛ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY,
   "ΜΜ-ΗΗ-ΧΧΧΧ ΩΩ:ΛΛ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS,
   "ΩΩ:ΛΛ:ΔΔ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM,
   "ΩΩ:ΛΛ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM,
   "ΗΗ/ΜΜ ΩΩ:ΛΛ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM,
   "ΜΜ/ΗΗ ΩΩ:ΛΛ"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS_AM_PM,
   "ΩΩ:ΛΛ:ΔΔ (ΠΜ/ΜΜ)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TRUE,
    "Αληθές"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
    "Εμφάνιση μενού επιφάνειας εργασίας κατά την εκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
    "Ενεργοποίηση μενού επιφάνειας εργασίας (επανεκκίνηση)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
    "Γραμμή Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
    "Αδυναμία ανάγνωσης συμπιεσμένου αρχείου."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
    "Αναίρεση Φόρτωσης Κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
    "Αναίρεση Αποθήκευσης Κατάστασης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNKNOWN,
    "Άγνωστο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
    "Ενημερωτής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
    "Ενημέρωση Βασικών Στοιχείων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
    "Ενημέρωση Προφίλ Joypad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
    "Ενημέρωση των Σκιάσεων Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
    "Ενημέρωση Απατών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
    "Ενημέρωση Αρχείων Πληροφοριών Πυρήνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
    "Ενημέρωση Βάσεων Δεδομένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
    "Ενημέρωση Σκιάσεων GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
    "Ενημέρωση Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
    "Ενημέρωση Επικαλλυμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
    "Ενημέρωση Σκιάσεων Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER,
    "Χρήστης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_KEYBOARD,
    "Kbd"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
    "Διεπαφή Χρήστη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
    "Γλώσσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
    "Χρήστης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
    "Χρήση Ενσωματωμένου Προβολέα Εικόνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
    "Χρήση Ενσωματωμένου Αναπαραγωγέα Πολυμέσων Use Builtin Media Player"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
    "<Χρήση αυτού του ευρετηρίου>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
    "Επίτρεψη περιστροφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
    "Διαμόρφωση Αναλογίας Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
    "Αυτόματη Αναλογία Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
    "Αναλογία Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
    "Εισαγωγή Μαύρων Καρέ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
    "Περικοπή Υπερσάρωσης (Επαναφόρτωση)"
    )
#if defined(_3DS)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
    "Κάτω οθόνη 3DS"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
    "Οδηγός Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
    "Φίλτρο Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
    "Φίλτρο Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
    "Ενεργοποίηση Ειδοποιήσεων Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
    "Γραμματοσειρά Ειδοποιήσεων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
    "Μέγεθος Γραμματοσειράς"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
    "Εξαναγκασμένη αναλογία απεικόνισης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
    "Εξαναγκασμένη απενεργοποίηση sRGB FBO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
    "Καθυστέρηση Καρέ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
    "Έναρξη σε Κατάσταση Πλήρης Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
    "Gamma Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
    "Χρήση Εγγραφής Κάρτας Γραφικών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
    "Ενεργοποίηση Στιγμιότυπου Οθόνης Κάρτας Γραφικών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
    "Σκληρός Συγχρονισμός Κάρτας Γραφικών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
    "Σκληρός Συγχρονισμός Καρέ Κάρτας Γραφικών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Μέγιστες εικόνες swapchain"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
    "Θέση Ειδοποιήσης X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
    "Θέση Ειδοποιήσης Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
    "Ένδειξη Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
    "Κάθετος Ρυθμός Ανανέωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
    "Εκτιμόμενος Ρυθμός Καρέ Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
    "Ορισμός Ρυθμού Ανανέωσης Βάση Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
    "Περιστροφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
    "Κλίμακα Παραθύρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
    "Ακέραια Κλίμακα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
    "Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
    "Σκίαση Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
    "Διγραμμικό Φιλτράρισμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
    "Ενεργοποίηση Απαλού Φίλτρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
    "Διάστημα Εναλλαγής Κάθετου Συγχρονισμόυ (Vsync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
    "Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Προτιμώμενο Ύψος Αναλογίας Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Προτιμώμενο Πλάτος Αναλογίας Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
    "Προτιμώμενη Θέση Άξωνα X Αναλογίας Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
    "Προτιμώμενη Θέση Άξωνα Y Αναλογίας Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
    "Ορισμός Πλάτους Οθόνης VI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
    "Παράθυρο Πλήρης Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
    "Πλάτος Παραθύρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
    "Ύψος Παραθύρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
    "Πλάτος Πλήρης Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
    "Ύψος Πλήρης Οθόνης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
    "Οδηγός Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
    "Γραμματοσειρά Μενού Κόκκινο Χρώμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
    "Γραμματοσειρά Μενού Πράσινο Χρώμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
    "Γραμματοσειρά Μενού Μπλε Χρώμα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_FONT,
    "Γραμματοσειρά Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
    "Μονόχρωμο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
    "Μονόχρωμο Ανεστραμμένο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
    "Χρώμα Θέματος Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
    "Πράσινο Μήλο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
    "Σκούρο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
    "Φωτεινό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
    "Πρωινό Μπλε"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
    "Σκούρο Μωβ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
    "Μπλε Ηλεκτρίκ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
    "Χρυσαφί"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
    "Legacy Κόκκινο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
    "Μεσωνύκτιο Μπλε"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
    "Απλό"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
    "Κάτω Από Την Θάλασσα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
    "Ηφαιστιακό Κόκκινο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
    "Ενεργοποίηση Σκιών Εικονιδίων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
    "Προβολή Καρτέλας Ιστορικού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
    "Προβολή Καρτέλας Εισαγωγής Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
    "Προβολή Καρτέλας Λίστας Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
    "Προβολή Καρτέλας Αγαπημένων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
    "Προβολή Καρτέλας Εικόνων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
    "Προβολή Καρτέλας Μουσικής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
    "Προβολή Καρτέλας Ρυθμίσεων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
    "Προβολή Καρτέλας Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
    "Προβολή Καρτέλας Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
    "Διάταξη Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_THEME,
    "Θέμα Εικόνων Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YES,
    "Ναι"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
    "Ενεργοποίηση ή απενεργοποίηση επιτευγμάτων. Για περισσότερες πληροφορίες επισκεφθείτε http://retroachievements.org"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
    "Αλλαγή οδηγών που χρησιμοποιούνται από το σύστημα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
    "Αλλαγή ρυθμίσεων επιτευγμάτων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_SETTINGS,
    "Αλλαγή ρυθμίσεων πυρήνα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
    "Αλλαγή ρυθμίσεων εγγραφής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
    "Αλλαγή επικάλλυψης οθόνης και επικάλλυψης πληκτρολογίου και ρυθμίσεις ειδοποιήσεων οθόνης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
    "Αλλαγή ρυθμίσεων επιστροφής, γρήγορης κίνησης και αργής κίνησης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
    "Αλλαγή ρυθμίσεων αποθήκευσης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
    "Αλλαγή ρυθμίσεων αρχείου καταγραφής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
    "Αλλαγή ρυθμίσεων περιβάλλοντος χρήστη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_SETTINGS,
    "Αλλαγή ρυθμίσεων λογαριασμού, ονόματος χρήστη και γλώσσας."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
    "Αλλαγή ρυθμίσεων ιδιοτηκότητας."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
    "Αλλαγή ρυθμίσεων MIDI."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
    "Αλλαγή προκαθορισμένων ευρετηρίων όπου βρίσκονται τα αρχεία."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
    "Αλλαγή ρυθμίσεων λιστών αναπαραγωγής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
    "Αλλαγή ρυθμίσεων εξυπηρετητή και δικτύου."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
    "Σάρωση περιεχομένου και προσθήκη στην βάση δεδομένων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
    "Αλλαγή ρυθμίσεων εξόδου ήχου."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
    "Ενεργοποίηση ή απενεργοποίηση bluetooth."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
    "Αποθήκευση αλλαγών στο αρχείο διαμόρφωσης κατά την έξοδο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
    "Αλλαγή προκαθορισμένων ρυθμίσεων των αρχείων διαμόρφωσης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
    "Διαχειρισμός και δημιουργία αρχείων διαμόρφωσης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CPU_CORES,
    "Αριθμός πυρήνων που έχει ο επεξεργαστής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_SHOW,
    "Εμφανίζει τον τρέχων ρυθμό καρέ ανά δευτερόλεπτο στην οθόνη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
    "Διαμόρφωση ρυθμίσεων πλήκτρων εντολών."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Συνδιασμός κουμπιών χειριστηρίου για την εμφάνιση του μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
    "Αλλαγή ρυθμίσεων χειριστηρίου, πληκτρολογίου και ποντικιού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_USER_BINDS, /* TODO/FIXME - change user to port */
    "Διαμόρφωση χειρισμών για αυτόν τον χρήστη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
    "Αλλαγή ρυθμίσεων συσχετιζόμενες με το βίντεο, τον ήχο και την καθυστέρηση εισαγωγής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
    "Ενεργοποίηση ή απενεργοποίηση αρχείων καταγραφής στο τερματικό."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY,
    "Συμμετοχή ή δημιουργία μίας συνεδρίας netplay."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
    "Αναζήτηση για και σύνδεση με οικοδεσπότη netplay στο τοπικό δίκτυο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
    "Εμφάνιση πληροφοριών συστήματος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
    "Κατεβάστε πρόσθετα, στοιχεία και περιεχόμενο για το RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
    "Αποτρέπει την προφύλαξη οθόνης του συστήματος από το να ενεργοποιηθεί."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
    "Ορισμός μεγέθους παραθύρου σε σχέση με το μέγεθος της οπτικής γωνίας του πυρήνα. Διαφορετικά, παρακάτω μπορείτε να ορίσετε το πλάτος και το ύψος του παραθύρου σε σταθερό μέγεθος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_LANGUAGE,
    "Ορίζει την γλώσσα του περιβάλλοντος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
    "Εισάγει ένα μαύρο καρέ ανάμεσα στα καρέ. Χρήσιμο για χρήστες με οθόνες 120Hz που θέλουν να παίξουν περιεχόμενο στα 60Hz χωρίς 'φαντάσματα' στην εικόνα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
    "Μειώνει την καθυστέρηση με μεγαλύτερο κίνδυνο για κολλήματα στο βίντεο. Προσθέτει μία επιβράδυνση μετά το V-Sync (σε ms)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
    "Ορίζει πόσα καρέ μπορεί ο επεξεργαστής να βρίσκεται μπροστά από την κάρτα γραφικών όταν χρησιμοποιείται τον 'Σκληρό Συγχρονισμό Κάρτα Γραφικών'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
    "Επιλέγει ποιά οθόνη θα χρησιμοποιηθεί."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
    "Ο ακριβής εκτιμόμενος ρυθμός ανανέωσης της οθόνης σε Hz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
    "Ο ρυθμός ανανέωσης όπως αναφέρεται από τον οδηγό οθόνης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
    "Αλλαγή ρυθμίσεων εξόδου βίντεο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
    "Σαρώνει για ασύρματα δίκτυα και δημιουργεί σύνδεση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_LIST,
    "Μάθετε περισσότερα για το πως λειτουργεί το πρόγραμμα."
    )
MSG_HASH(
    MSG_ADDED_TO_FAVORITES,
    "Προστέθηκε στα αγαπημένα"
    )
MSG_HASH(
    MSG_RESET_CORE_ASSOCIATION,
    "Ο σύνδεση με πυρήνα της λίστας αναπαραγωγής έχει επαναφερθεί."
    )
MSG_HASH(
    MSG_AUDIO_MUTED,
    "Ο ήχος απενεργοποιήθηκε."
    )
MSG_HASH(
    MSG_AUDIO_UNMUTED,
    "Ο ήχος ενεργοποιήθηκε."
    )
MSG_HASH(
    MSG_CONNECTED_TO,
    "Συνδέθηκε με"
    )
MSG_HASH(
    MSG_DOWNLOADING,
    "Γίνεται λήψη"
    )
MSG_HASH(
    MSG_INDEX_FILE,
    "ευρετηρίου"
    )
MSG_HASH(
    MSG_DOWNLOAD_FAILED,
    "Η λήψη απέτυχε"
    )
MSG_HASH(
    MSG_ERROR,
    "Πρόβλημα"
    )
MSG_HASH(
    MSG_EXTRACTING,
    "Γίνεται εξαγωγή"
    )
MSG_HASH(
    MSG_EXTRACTING_FILE,
    "Γίνεται εξαγωγή αρχείου"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_OVERLAY,
    "Αποτυχία φόρτωσης επικαλλύματος."
    )
MSG_HASH(
    MSG_FILE_NOT_FOUND,
    "Το αρχείο δεν βρέθηκε"
    )
MSG_HASH(
    MSG_FRAMES,
    "Καρέ"
    )
MSG_HASH(
    MSG_INPUT_CHEAT,
    "Εισαγωγή Απάτης"
    )
MSG_HASH(
    MSG_INTERFACE,
    "Αντάπτορας Δικτύου"
    )
MSG_HASH(
    MSG_INTERNAL_STORAGE,
    "Εσωτερική Μνήμη Αποθήκευσης"
    )
MSG_HASH(
    MSG_REMOVABLE_STORAGE,
    "Αφαιρούμενο Μέσο Αποθήκευσης"
    )
MSG_HASH(
    MSG_INVALID_NICKNAME_SIZE,
    "Μη έγκυρο μέγεθος ψευδώνυμου."
    )
MSG_HASH(
    MSG_IN_BYTES,
    "σε bytes"
    )
MSG_HASH(
    MSG_IN_GIGABYTES,
    "σε gigabytes"
    )
MSG_HASH(
    MSG_IN_MEGABYTES,
    "σε megabytes"
    )
MSG_HASH(
    MSG_LOADING,
    "Γίνεται φόρτωση"
    )
MSG_HASH(
    MSG_MEMORY,
    "Μνήμη"
    )
MSG_HASH(
    MSG_PAUSED,
    "Παύση."
    )
MSG_HASH(
    MSG_RECEIVED,
    "ελήφθη"
    )
MSG_HASH(
    MSG_RECORDING_TO,
    "Εγγραφή σε"
    )
MSG_HASH(
    MSG_SCANNING,
    "Σάρωση"
    )
MSG_HASH(
    MSG_SCANNING_OF_DIRECTORY_FINISHED,
    "Η σάρωση του ευρετηρίου ολοκληρώθηκε"
    )
MSG_HASH(
    MSG_SENDING_COMMAND,
    "Αποστολή εντολής"
    )
MSG_HASH(
    MSG_SHADER,
    "Σκίαση"
    )
MSG_HASH(
    MSG_SLOW_MOTION,
    "Αργή κίνηση."
    )
MSG_HASH(
    MSG_FAST_FORWARD,
    "Γρήγορη κίνηση."
    )
MSG_HASH(
    MSG_UNKNOWN,
    "Άγνωστο"
    )
MSG_HASH(
    MSG_VALUE_REBOOTING,
    "Επανεκκίνηση..."
    )
MSG_HASH(
    MSG_VALUE_SHUTTING_DOWN,
    "Τερματισμός λειτουργίας..."
    )
MSG_HASH(
    MSG_VERSION_OF_LIBRETRO_API,
    "Έκδοση του libretro API"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
    "Επιθυμητή καθυστέρηση ήχου σε milliseconds. Ίσως να μην τηρηθεί εάν ο οδηγός ήχου δεν μπορεί να παρέχει την επιλεγμένη καθυστέρηση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MUTE,
    "Σίγαση/κατάργηση σίγασης ήχου."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
    "Μέγιστος αριθμός χρηστών που υποστηρίζεται από το RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
    "Επιρροή του πως γίνεται η συγκέντρωση εισόδου μέσα στο RetroArch. Ο ορισμός σε 'Νωρίς' ή 'Αργά' μπορεί να έχει ως αποτέλεσμα μικρότερη καθυστέρηση με τις ρυθμίσεις σας."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
    "Επιτρέπει σε οποιονδήποτε χρήστη να χειριστεί το μενού. Εάν απενεργοποιηθεί, μόνο ο Χρήστης 1 μπορεί να χειριστει το μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
    "Ένταση ήχου (σε dB). Το 0 είναι η φυσιολογική ένταση και δεν εφαρμόζεται gain."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNC,
    "Συγχρονισμός ήχου. Προτείνεται."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
    "Χρόνος αναμονής σε δευτερόλεπτα μέχρι την συνέχιση στην επόμενη σύνδεση πλήκτρων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
    "Δευτερόλεπτα τα οποία χρειάζεται να κρατήσετε πατημένο κάποιο κουμπί μέχρι την σύνδεση του."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
    "Συγχρονίζει την έξοδο βίντεο της κάρτας γραφικών με τον ρυθμό ανανέωσης της οθόνης. Προτείνεται."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
    "Ο κάθετος ρυθμός ανανέωσης της οθόνης. Χρησιμοποιείται για τον υπολογισμό του κατάλληλου ρυθμού εισαγωγής ήχου.\n"
    "ΣΗΜΕΙΩΣΗ: Αυτή η επιλογή αγνοείται εάν έχετε ενεργοποιήσει το 'Threaded Video'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
    "Ενεργοποίηση εξόδου ήχου."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
    "Πληροφορίες Βάσης Δεδομένων"
    )
MSG_HASH(
    MSG_WIFI_SCAN_COMPLETE,
    "Η σάρωση του Wi-Fi ολοκληρώθηκε."
    )
MSG_HASH(
    MSG_SCANNING_WIRELESS_NETWORKS,
    "Σάρωση ασύρματων δικτύων..."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCAN_COMPLETE,
    "Οκληρώθηκε η σάρωση Netplay."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCANNING,
    "Σάρωση για οικοδεσπότες netplay..."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
    "Παύση παιχνιδιού όταν το RetroArch δεν είναι το ενεργό παράθυρο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
    "Ενεργοποίηση ή απενεργοποίηση λίστας πρόσφατων για παιχνίδια, εικόνες, μουσική και βίντεο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
    "Περιορισμός καταχωρήσεων στην λίστα πρόσφατων για παιχνίδια, εικόνες, μουσική και βίντεο."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
    "Ενοποιημένος Χειρισμός Μενού"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
    "Χρήση του ίδιου χειρισμού για το μενού και το παιχνίδι. Εφαρμόζεται στο πληκτρολόγιο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
    "Εμφάνιση μηνυμάτων οθόνης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
    "Εμφάνιση επιπέδου μπαταρίας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FILE,
    "Επιλογή Αρχείου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER,
    "Φίλτρα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCALE,
    "Κλίμακα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_START,
    "Έναρξη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
    "Χιονονιφάδα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
    "Ανανέωση Λίστας Δωματίων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
    "Ψευδώνυμο: %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
    "Ψευδώνυμο (lan): %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
    "Βρέθηκε συμβατό περιεχόμενο"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
    "Αφαιρεί μερικά pixel γύρω από την εικόνα όπου εθιμικά οι προγραμματιστές άφηναν κενά ή και που περιέχουν άχρηστα pixel."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
    "Προσθέτει μία μικρή θολούρα στην εικόνα ώστε να αφαιρέσει τις σκληρές γωνίες των pixel. Αυτή η επιλογή έχει πολύ μικρή επιρροή στην επίδοση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER,
    "Εφαρμόστε ένα φίλτρο βίντεο που λειτουργεί με τον επεξεργαστή.\n"
    "ΣΗΜΕΙΩΣΗ: Μπορεί να έχει μεγάλο κόστος στην επίδοση. Μερικά φίλτρα βίντεο μπορεί να δουλεύουν μόνο με πυρήνες που χρησιμοποιούν 32bit ή 16bit χρώμα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
    "Εισάγεται το όνομα χρήστη του λογαριασμού σας στο RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
    "Εισάγεται τον κωδικό πρόσβασης του λογαριασμού σας στο RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
    "Εισάγεται το όνομα χρήστη σας εδώ. Αυτό θα χρησιμοποιηθεί για συνεδρίες netplay ανάμεσα σε άλλα πράγματα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
    "Capture the image after filters (but not shaders) are applied. Your video will look as fancy as what you see on your screen."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_LIST,
    "Επιλέξτε ποιον πυρήνα θα χρησιμοποιήστε."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
    "Επιλέξτε ποιο περιεχόμενο θα ξεκινήσετε."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
    "Εμφάνιση ανταπτόρων δικτύου και τις συσχετιζόμενες διευθύνσεις IP."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
    "Εμφάνιση πληροφοριών για την συγκεκριμένη συσκευή."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Έξοδος από το πρόγραμμα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
    "Ορίστε το προτιμώμενο πλάτος του παραθύρου απεικόνισης. Αφήνοντας το στο 0 θα επιχειρηθεί η κλίμακα του παραθύρου να είναι όσο το δυνατόν μεγαλύτερη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
    "Ορίστε το προτιμώμενο ύψος του παραθύρου απεικόνισης. Αφήνοντας το στο 0 θα επιχειρηθεί η κλίμακα του παραθύρου να είναι όσο το δυνατόν μεγαλύτερη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
    "Απόκρυψη του επικαλλύματος μέσα στο μενού και εμφάνιση του ξανά με την έξοδο από το μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Εμφάνιση εισαγωγών πληκτρολογίου/χειριστηρίου στο επικάλλυμα οθόνης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Επιλογή της θύρας για όταν είναι ενεργοποιημένη η επιλογή 'Εμφάνιση Εισαγωγών Στην Οθόνη'"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
    "Το σαρωμένο περιεχόμενο θα εμφανίζεται εδώ."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
    "Αλλαγή κλίμακας βίντεο σε ακέραια βήματα. Το βασικό μέγεθος εξαρτάται από την γεωμετρία και την κλίμακα οθόνης του συστήματος. Εάν η 'Εξαναγκασμένη Κλίμακα' δεν έχει οριστεί, οι άξωνες X/Y θα αλλάζουν κλίμακα ξεχωριστά."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
    "Έναρξη σε πλήρη οθόνη. Μπορεί να αλλάξει κατά την εκτέλεση. Μπορεί να παρακαμπτεί από έναν διακόπτη γραμμής τερματικού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
    "Εάν χρησιμοποιηθεί πλήρης οθόνη προτιμήστε την κατάσταση παραθύρου πλήρης οθόνης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_ENABLE,
    "Ενεργοποίηση επιστροφής. Η επίδοση θα πέσει κατά το παιχνίδι."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
    "Αριστερά/Δεξιά για αλλαγή μεγέθους bit"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
    "Αριστερά/Δεξιά για αλλαγή τιμής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
    "Αριστερά/Δεξιά για αλλαγή τιμής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
    "Αριστερά/Δεξιά για αλλαγή τιμής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
    "Εναλλαγή πλήτρκων για Επιβεβαίωση/Ακύρωση. Απενεργοποιημένο είναι ο Ιαπωνικός προσανατολισμός, ενεργοποιημένος είναι ο δυτικός προσανατολισμός."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
    "Εάν απενεργοποιηθεί το περιεχόμενο θα συνεχίσει να τρέχει στο παρασκήνιο όταν το μενού του RetroArch είναι ανοικτό."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
    "Οδηγός βίντεο προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
    "Οδηγός ήχου προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DRIVER,
    "Οδηγός Εισόδου προς χρήση. Ανάλογα με τον οδηγό βίντεο, ίσως αλλάξει αναγκαστικά ο οδηγός εισαγωγής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
    "Οδηγός Joypad προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
    "Οδηγός Επαναδειγματολήπτη Ήχου προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
    "Οδηγός Κάμερας προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
    "Οδηγός Τοποθεσίας προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DRIVER,
    "Οδηγός Μενού προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORD_DRIVER,
    "Οδηγός Εγγραφής προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_DRIVER,
    "Οδηγός MIDI προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_DRIVER,
    "Οδηγός Wi-Fi προς χρήση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
    "Παράκαμψη της προκαθορισμένης συσκευής ήχου που χρησιμοποιεί ο οδηγός ήχου. Αυτή η επιλογή εξαρτάται από τον οδηγό."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
    "Πρόσθετο ήχου DSP που επεξεργάζεται τον ήχο πριν αποσταλεί στον οδηγό."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
    "Διαφάνεια όλων των στοιχείων του επικαλλύματος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
    "Κλίμακα όλων των στοιχείων του επικαλλύματος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
    "Ενεργοποίηση του επικαλλύματος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
    "Επιλογή ενός επικαλλύματος από τον περιηγητή αρχείων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
    "Εμφάνιση τρέχουσας ημερομηνίας ή και ώρας μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
    "Εμφάνιση τρέχουσας μπαταρίας μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
    "Ενεργοποιεί το netplay ως οικοδεσπότης (εξυπηρετητής)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
    "Ενεργοποιεί το netplay ως πελάτης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
    "Αποσυνδέει μία ενεργή σύνδεση Netplay."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
    "Σαρώνει ένα ευρετήριο για συμβατά αρχεία."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_FILE,
    "Σαρώνει ένα συμβατό αρχείο"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
    "Χρησιμοποιεί ένα προτιμώμενο διάστημα αλλαγής για το Vsync. Ορίστε αυτό ώστε να μειώσεται στο μισό τον ρυθμό ανανέωσης της οθόνης αποτελεσματικά."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
    "Σάρωση για νέα δωμάτια."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
    "Προσθήκη καταχώρησης στα αγαπημένα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
    "Προσθήκη καταχώρησης στα αγαπημένα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN,
    "Έναρξη περιεχομένου."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
    "Προσαρμογή ρυθμίσεων εξερευνητή αρχείου."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ENABLE,
    "Εμφανίζει το όνομα του τρέχων πυρήνα μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
    "Προβολή βάσεων δεδομένων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
    "Προβολή προηγούμενων αναζητήσεων."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
    "Καταγράφει μία εικόνα της οθόνης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
    "Κλείνει το τρέχον περιεχόμενο. Οποιεσδήποτε μη αποθηκευμένες αλλαγές μπορεί να χαθούν."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_STATE,
    "Φόρτωση μίας κατάστασης από την τρέχουσα θέση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_STATE,
    "Αποθήκευση μίας κατάστασης στην τρέχουσα θέση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME,
    "Συνέχιση εκτέλεσης του τρέχοντος περιεχομένου και έξοδος από το Γρήγορο Μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME_CONTENT,
    "Συνέχιση εκτέλεσης του τρέχοντος περιεχομένου και έξοδος από το Γρήγορο Μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATE_SLOT,
    "Αλλάζει την τρέχουσα επιλεγμένη θέση κατάστασης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
    "Εάν μία κατάσταση φορτώθηκε, το περιεχόμενο θα επιστρέψει στην κατάσταση πριν την φόρτωση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
    "Εάν μία κατάσταση αντικαταστάθηκε, το περιεχόμενο θα επιστρέψει στην προηγούμενη κατάσταση αποθήκευσης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "Υπηρεσία RetroAchievements. Για περισσότερες πληροφορίες επισκεφθείτε το http://retroachievements.org"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
    "Διαχειρίζεται τους τρέχοντες διαμορφωμένους λογαριασμούς."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
    "Διαχειρίζεται τις ρυθμίσεις επαναφοράς."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_CONTENT,
    "Επανεκκινεί το περιεχόμενο από την αρχή."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
    "Στήσιμο κωδικών απάτης."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
    "Στήσιμο σκιάσεων για την οπτική βελτίωση της εικόνας."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
    "Αλλαγή χειρισμών για το τρέχον εκτελούμενο περιεχόμενο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_OPTIONS,
    "Αλλαγή επιλογών για το τρέχον εκτελούμενο περιεχόμενο."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
    "Προβολή καρτέλας αγαπημένων μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
    "Προβολή καρτέλας εικόνων μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
    "Προβολή καρτέλας μουσικής μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
    "Προβολή καρτέλας βίντεο μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
    "Προβολή καρτέλας netplay μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
    "Προβολή καρτέλας ρυθμίσεων μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
    "Προβολή καρτέλας πρόσφατου ιστορικού μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
    "Προβολή καρτέλας εισαγωγής περιεχομένου μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
    "Προβολή καρτέλας λίστας αναπαραγωγής μέσα στο μενού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
    "Προβολή οθόνης εκκίνησης στο μενού. Τίθεται αυτόματα σε αρνητικό μετά την πρώτη εκκίνηση του προγράμματος."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
    "Ορίζει ένα ευρετήριο όπου τα επικαλλύματα αποθηκεύονται για εύκολη πρόσβαση."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
    "Τα αρχεία απάτης αποθηκεύονται εδώ."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
    "Γρήγορα πρόσβαση σε όλες τις σχετικές ρυθμίσεις παιχνιδιού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Προτιμώμενο ύψος οπτικής γωνίας το οποίο χρησιμοποιείται εάν η Αναλογία Οθόνης είναι ορισμένη ως 'Προτιμώμενη'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Προτιμώμενο πλάτος οπτικής γωνίας το οποίο χρησιμοποιείται εάν η Αναλογία Οθόνης είναι ορισμένη ως 'Προτιμώμενη'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
    "Προτιμώμενη απόκλειση οπτικής γωνίας για τον ορισμό της θέσης του άξωνα X της οπτικής γωνίας. Αυτό αγνοείται εάν έχεται ενεργοποιήσει την 'Ακέραια Κλίμακα'. Τότε θα κεντραριστεί αυτόματα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
    "Προτιμώμενη απόκλειση οπτικής γωνίας για τον ορισμό της θέσης του άξωνα Y της οπτικής γωνίας. Αυτό αγνοείται εάν έχεται ενεργοποιήσει την 'Ακέραια Κλίμακα'. Τότε θα κεντραριστεί αυτόματα."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
    "Χρήση Εξυπηρετητή Αναμετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
    "Forward netplay connections through a man-in-the-middle server. Useful if the host is behind a firewall or has NAT/UPnP problems."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
    "Τοποθεσία Εξυπηρετητή Αναμετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
    "Choose a specific relay server to use. Geographically closer locations tend to have lower latency."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
    "Προσθήκη στον μίκτη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
    "Προσθήκη στον μίκτη και αναπαραγωγή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
    "Προσθήκη στον μίκτη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
    "Προσθήκη στον μίκτη και αναπαραγωγή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
    "Φιλτράρισμα με βάση τον τρέχων πυρήνα"
    )
MSG_HASH(
    MSG_AUDIO_MIXER_VOLUME,
    "Γενική ένταση μίκτη ήχου"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
    "Γενική ένταση μίκτη ήχου (σε dB). Το 0 είναι η φυσιολογική ένταση και δεν εφαρμόζεται gain." /*Need a good translation for gain if there's any*/
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
    "Επίπεδο Έντασης Μίκτη Ήχου (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
    "Σίγαση Μίκτη Ήχου"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
    "Σίγαση/κατάργηση σίγασης μίκτη ήχου."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
    "Προβολή Διαδικτυακού Ενημερωτή"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
    "Εμφάνιση/απόκρυψη της επιλογής 'Διαδικτυακού Ενημερωτή'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
    "Προβολές"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
    "Προβολή ή απόκρυψη στοιχείων στην οθόνη του μενού."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
    "Προβολή Ενημερωτή Πυρήνων"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
    "Εμφάνιση/απόκρυψη της ικανότητας ενημέρωσης πυρήνων (και πληροφοριακών αρχείων πυρήνων)."
    )
MSG_HASH(
    MSG_PREPARING_FOR_CONTENT_SCAN,
    "Προετοιμασία για σάρωση περιεχομένου..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_DELETE,
    "Διαγραφή πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_DELETE,
    "Κατάργηση αυτού του πυρήνα από τον δίσκο."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
    "Framebuffer Opacity"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
    "Modify the opacity of the framebuffer."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
    "Αγαπημένα"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
    "Περιεχόμενο που έχετε προσθέσει στα 'Αγαπημένα' θα εμφανίζεται εδώ."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
    "Μουσική"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_MUSIC,
    "Μουσική που έχει προηγουμένως αναπαραχθεί θα εμφανίζονται εδώ."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
    "Εικόνα"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_IMAGES,
    "Εικόνες που έχουν προηγουμένως προβληθεί θα εμφανίζονται εδώ."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
    "Βίντεο"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_VIDEO,
    "Βίντεο που έχουν προηγουμένως αναπαραχθεί θα εμφανίζονται εδώ."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
    "Εικονίδια Μενού"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
    "Ενεργοποίηση/Απενεργοποίηση των εικονιδίων που εμφανίζονται στα αριστερά των καταχωρήσεων του μενού."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Ενεργοποίηση Καρτέλας Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Ορισμός Κωδικού Για Την Ενεργοποίηση Της Καρτέλας Ρυθμίσεων"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
    "Εισαγωγή Κωδικού"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
    "Σωστός κωδικός."
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
    "Λανθασμένος κωδικός."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Ενεργοποιεί την καρτέλα Ρυθμίσεις. Χρειάζεται επανεκκίνηση για να εμφανιστεί η καρτέλα."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Supplying a password when hiding the settings tab makes it possible to later restore it from the menu, by going to the Main Menu tab, selecting Enable Settings Tab and entering the password."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
    "Επίτρεψη μετονομασίας καταχωρήσεων"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RENAME_ENTRY,
    "Μετονομασία του τίτλου αυτής της καταχώρησης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
    "Μετονομασία"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
    "Προβολή Φόρτωσης Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
    "Εμφάνιση/απόκρυψη της επιλογής 'Φόρτωση Πυρήνα'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
    "Προβολή Φόρτωσης Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
    "Εμφάνιση/απόκρυψη της επιλογής 'Φόρτωση Περιεχομένου'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
    "Προβολή Πληροφοριών"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
    "Εμφάνιση/απόκρυψη της επιλογής 'Πληροφορίες'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
    "Προβολή Διαμορφώσεων"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
    "Εμφάνιση/απόκρυψη της επιλογής 'Διαμορφώσεις'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
    "Προβολή Βοήθειας"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
    "Εμφάνιση/απόκρυψη της επιλογής 'Βοήθεια'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Προβολή Εξόδου RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Εμφάνιση/απόκρυψη της επιλογής 'Έξοδος από RetroArch'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
    "Προβολή Επανεκκίνησης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
    "Εμφάνιση/απόκρυψη της επιλογής 'Επανεκκίνηση'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
    "Γρήγορο Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
    "Ενεργοποίηση Λειτουργίας Κιόσκι"
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD,
    "Εισαγωγή Κωδικού"
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
    "Σωστός κωδικός."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
    "Λανθασμένος κωδικός."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
    "Διαφάνεια Παραθύρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
    "Ποιότητα Επαναδειγματολήπτη Ήχου"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
    "Ελαττώστε αυτή την τιμή για καλύτερη επίδοση/χαμηλότερη καθυστέρηση αντί ποιότητας ήχου, αυξήστε εάν θέλετε καλύτερη ποιότητα με κόστος στην επίδοση/χαμηλότερη καθυστέρηση."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
    "Εμφάνιση Διακοσμητικών Παραθύρου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
    "Εμφάνιση Στατιστικών"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
    "Εμφάνιση τεχνικών στατιστικών στην οθόνη."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
    "Για οθόνες CRT μόνο. Προσπαθεί να χρησιμοποιήσει την ακριβή ανάλυση πυρήνα/παιχνιδιού και ρυθμού ανανέωσης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
    "Σούπερ Ανάλυση CRT"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
    "Προβολή Ρυθμίσεων Επιστροφής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
    "Εμφάνιση/απόκρυψη επιλογών Επιστροφής."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
    "Εμφάνιση/απόκρυψη επιλογών Καθυστέρησης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
    "Προβολή Ρυθμίσεων Καθυστέρησης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
    "Εμφάνιση/απόκρυψη επιλογών Επικαλλυμάτων."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
    "Προβολή Ρυθμίσεων Επικαλλυμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
    "Ενεργοποίηση ήχου μενού"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
    "Ενεργοποίηση ή απενεργοποίηση ήχου μενού."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
    "Ρυθμίσεις Μίκτη"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
    "Εμφάνιση και/ή επεξεργασία ρυθμίσεων μίκτη."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFO,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
    "Θέμα:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
    "Σκούρο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
    "Custom..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
    "Επιλογές"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
    "Σχετικά με το RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
    "Εγχειρίδιο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
    "Load Custom Core..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
    "Φόρτωση Πυρήνα΄"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
    "Φόρτωση Πυρήνα..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NAME,
    "Όνομα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
    "Έκδοση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
    "Λίστες Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
    "Εμφάνιση Μενού Επιφάνεις Εργασίας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_STOP,
    "Στοπ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
    "Αναπαραγωγή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
    "Αναπαραγωγή (Looped)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Αναπαραγωγή (Sequential)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
    "Στοπ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
    "Κατάργηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
    "Ένταση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
    "Τρέχων πυρήνας"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
    "Ενεργοποίηση Discord Rich Presence"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
    "Ενεργοποίηση ή απενεργοποίηση υποστήριξης Discord Rich Presence.\n"
    "ΣΗΜΕΙΩΣΗ: Δεν θα δουλέψει με την έκδοση του περιηγητή, μόνο με την τοπικά εγκατεστημένη."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
    "Είσοδος"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_INPUT,
    "Επιλογή συσκευής εισόδου."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
    "Έξοδος"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
    "Επιλογή συσκευής εξόδου."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
    "Ένταση"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_VOLUME,
    "Ορισμός έντασης εξόδου (%)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
    "Διαχείριση Ενέργειας"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
    "Αλλαγή ρυθμίσεων διαχείρισης ενέργειας."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
    "Κατάσταση Συνεχούς Επίδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
    "Υποστήριξη mpv"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
    "Πληροφορίες"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
    "Ενεργοποιημένο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
    "Κωδικός"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
    "Τύπος"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
    "Τιμή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
    "Εμφάνιση κρυφών αρχείων και φακέλων:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
    "Νέα Λίστα Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
    "Please enter the new playlist name:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
    "Διαγραφή Λίστας Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
    "Μετονομασία Λίστας Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
    "Λήψη Όλων των Σκίτσων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
    "Όλο το Σύστημα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
    "Αυτή η Λίστα Αναπαραγωγής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
    "Επιτυχής λήψη σκίτσων."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
    "Πέτυχαν: %1 Απέτυχαν: %2"
    )
MSG_HASH(
    MSG_DEVICE_CONFIGURED_IN_PORT,
    "Διαμορφώθηκε στην θύρα:"
    )
MSG_HASH(
    MSG_FAILED_TO_SET_DISK,
    "Αποτυχία ορισμού δίσκου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
    "Επιλογές Πυρήνα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Προσαρμοστικό Vsync"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "Το V-Sync είναι ενεργοποιημένο μέχρι η επίδοση να πέσει κάτω από τον στόχο ρυθμού ανανέωσης. Μπορεί να μειώσει τα κολλήματα όταν η επίδοση πέφτει χαμηλότερα από τον κανονικό χρόνο και μπορεί να είναι πιο αποδοτικό ενεργειακά."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Εξαγωγή ντόπιων, χαμηλής ανάλυσης σημάτων για χρήση με οθόνες CRT."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Εναλλάξτε μεταξύ αυτών των επιλογών εάν η εικόνα δεν είναι σωστά κεντραρισμένη στην οθόνη."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "Κεντράρισμα Άξωνα Χ"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Χρήση προσαρμοσμένου ρυθμού ανανέωσης προσδιορισμένου στο αρχείο διαμόρφωσης εάν χρειάζεται."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Χρήση Προσαρμοσμένου Ρυθμού Ανανέωσης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "Επιλέξτε την θύρα εξόδου που είναι συνδεδεμένη με την οθόνη CRT."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "ID Οθόνης Εξόδου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Έναρξη Εγγραφής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Ξεκινάει την εγγραφή."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Τέλος Εγγραφής"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Σταματάει την εγγραφή."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Έναρξη Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Ξεκινάει την απευθείας μετάδοση."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Τέλος Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Σταματάει την απευθείας μετάδοση."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
    "Εγγραφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
    "Απευθείας Μετάδοση"
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
    "A savestate was loaded, Achievements Hardcore Mode disabled for the current session. Restart to enable hardcore mode."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Ποιότητα Εγγραφής"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Ποιότητα Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "Σύνδεσμος Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "Θύρα UDP Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
    "Κλειδί Απευθείας Μετάδοσης Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
    "Κλειδί Απευθείας Μετάδοσης YouTube"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
    "Μέσο Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
    "Τίτλος Απευθείας Μετάδοσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Χωριστά Joy-Con"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Επαναφορά Προεπιλογών"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Επαναφορά της τρέχουσας διαμόρφωσης στις προεπιλεγμένες ρυθμίσεις."
    )

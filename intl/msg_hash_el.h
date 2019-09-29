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
    MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
    "Give hardware-rendered cores their own private context. Avoids having to assume hardware state changes inbetween frames."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
    "Enable horizontal animation for the menu. This will have a performance hit."
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
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
    "Accounts List Endpoint"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "RetroAchievements"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
    "Δωμάτια Netplay"
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
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
    "Ενεργοποίηση Bluetooth"
    )
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
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS,
    "Κλειδωμένα Επιτεύγματα:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
    "Κλειδωμένο"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
    "Δοκιμή Ανεπίσημων Επιτευγμάτων"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS,
    "Ξεκλειδωμένα Επιτεύγματα:"
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
    MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS,
    "Disk Cycle Tray Status"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
    "Disk Image Append"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_INDEX,
    "Disk Index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
    "Disk Control"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DONT_CARE,
    "Don't care"
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
    MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
    "DPI Override Enable"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
    "DPI Override"
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
    MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
    "Frontend Counters"
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
    MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
    "Duty Cycle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
    "Σύνδεση Πλήκτρων Εντολών"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
    "Keyboard Gamepad Mapping Enable"
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
    MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
    "Keyboard Gamepad Mapping Type"
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
    MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
    "Frameadvance"
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
    MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
    "Input replay movie record toggle"
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
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
    "Start or Continue Cheat Search"
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
    MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
    "Prefer Front Touch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
    "Input Remapping"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
    "Remap Binds Enable"
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
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
    "Turbo Period"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
    "Σύνδεση Πλήκτρων Εισόδου Χρήστη %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
    "Καθυστέρηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
    "Internal storage status"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
    "Input Autoconfig"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
    "Οδηγός Joypad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
    "Υπηρεσίες"
    )
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
    MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
    "Core Logging Level"
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
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_DISC,
      "Load Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMP_DISC,
      "Dump Disc")
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
    MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
    "Throttle Menu Framerate"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
    "File Browser"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
    "Menu Linear Filter"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
    "Horizontal Animation"
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
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
    "Background opacity"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MISSING,
    "Λείπει"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MORE,
    "..."
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
    MENU_ENUM_LABEL_VALUE_NETPLAY,
    "Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
    "Allow Slave-Mode Clients"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
    "Netplay Check Frames"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Input Latency Frames"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Input Latency Frames Range"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
    "Netplay Delay Frames"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
    "Disconnect from netplay host"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
    "Netplay Client Enable"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
    "Request Device %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
    "Disallow Non-Slave-Mode Clients"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
    "Ρυθμίσεις Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
    "Analog Input Sharing"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
    "Digital Input Sharing"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
    "Κοινοποίηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
    "Grapple"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
    "Netplay Spectator Mode"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
    "Netplay Stateless Mode"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
    "Server Spectate-Only Password"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
    "Netplay Spectator Enable"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
    "Netplay TCP Port"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
    "Netplay NAT Traversal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
    "Network Commands"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
    "Network Command Port"
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
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
    "Network Remote Base Port"
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
    MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
    "No performance counters."
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
    MENU_ENUM_LABEL_VALUE_OFF,
    "OFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ON,
    "ON"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE,
    "Online"
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
    MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
    "Performance Counters"
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
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Label Display Mode"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Change how the content labels are displayed in this playlist."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Show full labels"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "Remove () content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "Remove [] content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "Remove () and []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Keep region"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Keep disc index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Keep region and disc index"
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
    MENU_ENUM_LABEL_VALUE_PRESENT,
    "Present"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
    "Ιδιωτικότητα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
    "MIDI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Έξοδος από RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
    "Analog supported"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
    "BBFC Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
    "CERO Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
    "Co-op supported"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
    "CRC32"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
    "Description"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
    "Developer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Edge Magazine Issue"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
    "Edge Magazine Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
    "Edge Magazine Review"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
    "ELSPA Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
    "Enhancement Hardware"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
    "ESRB Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
    "Famitsu Magazine Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
    "Franchise"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
    "Genre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
    "MD5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
    "Όνομα"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
    "Origin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
    "PEGI Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
    "Publisher"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
    "Releasedate Month"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
    "Releasedate Year"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
    "Rumble supported"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
    "Serial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
    "SHA1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
    "Έναρξη Περιεχομένου"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
    "TGDB Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Επανεκκίνηση"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
    "Recording Config"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
    "Recording Output"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
    "Εγγραφή"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
    "Custom Record Config"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
    "Custom Stream Config"
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
    MENU_ENUM_LABEL_VALUE_REMAP_FILE,
    "Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
    "Load Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
    "Save Core Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
    "Save Content Directory Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
    "Save Game Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
    "Delete Core Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
    "Delete Game Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
    "Delete Game Content Directory Remap File"
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
    MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
    "RetroKeyboard"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD,
    "RetroPad"
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
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
    "Ενεργοποίηση SAMBA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
    "Αρχείο Αποθήκευσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
    "Save State Auto Index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
    "Auto Load State"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
    "Auto Save State"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
    "Savestate"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
    "Savestate Thumbnails"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
    "Αποθήκευση Τρέχουσας Διαμόρφωσης"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Save Core Overrides"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Save Content Directory Overrides"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Save Game Overrides"
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
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
    "Slow-Motion Ratio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
    "Run-Ahead to Reduce Latency"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
    "Number of Frames to Run Ahead"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
    "RunAhead Use Second Instance"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
    "RunAhead Hide Warnings"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
    "Sort Saves In Folders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
    "Sort Savestates In Folders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
    "Write Savestates to Content Dir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
    "Write Saves to Content Dir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
    "System Files are in Content Dir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
    "Write Screenshots to Content Dir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
    "Ενεργοποίηση SSH"
    )
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
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
    "UI Companion Enable"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
    "UI Companion Start On Boot"
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
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
    "Disable Desktop Composition"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
    "Flicker filter"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
    "Use Post Filter Recording"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
    "Shader Passes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
    "Shader Parameters"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
    "Load Shader Preset"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
    "Save Shader Preset As"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Save Core Preset"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Save Content Directory Preset"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Save Game Preset"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
    "Enable Hardware Shared Context"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
    "Threaded Video"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
    "Deflicker"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
    "Vertical Sync (Vsync)"
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
    MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
    "Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
    "Menu Alpha Factor"
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
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
    "Custom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
    "FlatUI"
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
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
    "Systematic"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
    "NeoActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
    "Pixel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
    "RetroActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
    "Retrosystem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
    "Dot-Art"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
    "Automatic"
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
    MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
    "Menu Shader Pipeline"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR,
    "Menu Scale Factor"
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
    MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
    "Shader Preset"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
    "Ενεργοποίηση ή απενεργοποίηση επιτευγμάτων. Για περισσότερες πληροφορίες επισκεφθείτε http://retroachievements.org"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
    "Enable or disable unofficial achievements and/or beta features for testing purposes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Enable or disable savestates, cheats, rewind, pause, and slow-motion for all games."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
    "Enable or disable in-game leaderboards. Has no effect if Hardcore Mode is disabled."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
    "Enable or disable badge display in the Achievement List."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
    "Enable or disable OSD verbosity for achievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
    "Automatically take a screenshot when an achievement is triggered."
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
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
    "Ενεργοποίηση ή απενεργοποίηση bluetooth."
    )
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
    MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
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
    MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
    "Enable or disable network sharing of your folders."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
    "Manage operating system level services."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
    "Show hidden files/directories inside the file browser."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SSH_ENABLE,
    "Enable or disable remote command line access."
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
    MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Tells the video driver to explicitly use a specified buffering mode."
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
    MSG_APPENDED_DISK,
    "Appended disk"
    )
MSG_HASH(
    MSG_APPLICATION_DIR,
    "Application Dir"
    )
MSG_HASH(
    MSG_APPLYING_CHEAT,
    "Applying cheat changes."
    )
MSG_HASH(
    MSG_APPLYING_SHADER,
    "Applying shader"
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
    MSG_AUTOCONFIG_FILE_ERROR_SAVING,
    "Error saving autoconf file."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
    "Autoconfig file saved successfully."
    )
MSG_HASH(
    MSG_AUTOSAVE_FAILED,
    "Could not initialize autosave."
    )
MSG_HASH(
    MSG_AUTO_SAVE_STATE_TO,
    "Auto save state to"
    )
MSG_HASH(
    MSG_BLOCKING_SRAM_OVERWRITE,
    "Blocking SRAM Overwrite"
    )
MSG_HASH(
    MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
    "Bringing up command interface on port"
    )
MSG_HASH(
    MSG_BYTES,
    "bytes"
    )
MSG_HASH(
    MSG_CANNOT_INFER_NEW_CONFIG_PATH,
    "Cannot infer new config path. Use current time."
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Achievements Hardcore Mode Enabled, savestate & rewind were disabled."
    )
MSG_HASH(
    MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
    "Comparing with known magic numbers..."
    )
MSG_HASH(
    MSG_COMPILED_AGAINST_API,
    "Compiled against API"
    )
MSG_HASH(
    MSG_CONFIG_DIRECTORY_NOT_SET,
    "Config directory not set. Cannot save new config."
    )
MSG_HASH(
    MSG_CONNECTED_TO,
    "Συνδέθηκε με"
    )
MSG_HASH(
    MSG_CONTENT_CRC32S_DIFFER,
    "Content CRC32s differ. Cannot use different games."
    )
MSG_HASH(
    MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
    "Content loading skipped. Implementation will load it on its own."
    )
MSG_HASH(
    MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "Core does not support save states."
    )
MSG_HASH(
    MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
    "Core options file created successfully."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
    "Could not find any next driver"
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
    "Could not find compatible system."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
    "Could not find valid data track"
    )
MSG_HASH(
    MSG_COULD_NOT_OPEN_DATA_TRACK,
    "could not open data track"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_CONTENT_FILE,
    "Could not read content file"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_MOVIE_HEADER,
    "Could not read movie header."
    )
MSG_HASH(
    MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
    "Could not read state from movie."
    )
MSG_HASH(
    MSG_CRC32_CHECKSUM_MISMATCH,
    "CRC32 checksum mismatch between content file and saved content checksum in replay file header. Replay highly likely to desync on playback."
    )
MSG_HASH(
    MSG_CUSTOM_TIMING_GIVEN,
    "Custom timing given"
    )
MSG_HASH(
    MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
    "Decompression already in progress."
    )
MSG_HASH(
    MSG_DECOMPRESSION_FAILED,
    "Decompression failed."
    )
MSG_HASH(
    MSG_DETECTED_VIEWPORT_OF,
    "Detected viewport of"
    )
MSG_HASH(
    MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
    "Did not find a valid content patch."
    )
MSG_HASH(
    MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
    "Disconnect device from a valid port."
    )
MSG_HASH(
    MSG_DISK_CLOSED,
    "Closed"
    )
MSG_HASH(
    MSG_DISK_EJECTED,
    "Ejected"
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
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
    "Libretro core requires content, but nothing was provided."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
    "Libretro core requires special content, but none were provided."
    )
MSG_HASH(
    MSG_ERROR_PARSING_ARGUMENTS,
    "Error parsing arguments."
    )
MSG_HASH(
    MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
    "Error saving core options file."
    )
MSG_HASH(
    MSG_ERROR_SAVING_REMAP_FILE,
    "Error saving remap file."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_REMAP_FILE,
    "Error removing remap file."
    )
MSG_HASH(
    MSG_ERROR_SAVING_SHADER_PRESET,
    "Error saving shader preset."
    )
MSG_HASH(
    MSG_EXTERNAL_APPLICATION_DIR,
    "External Application Dir"
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
    MSG_FAILED_SAVING_CONFIG_TO,
    "Failed saving config to"
    )
MSG_HASH(
    MSG_FAILED_TO,
    "Failed to"
    )
MSG_HASH(
    MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
    "Failed to accept incoming spectator."
    )
MSG_HASH(
    MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
    "Failed to allocate memory for patched content..."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER,
    "Failed to apply shader."
    )
MSG_HASH(
    MSG_FAILED_TO_BIND_SOCKET,
    "Failed to bind socket."
    )
MSG_HASH(
    MSG_FAILED_TO_CREATE_THE_DIRECTORY,
    "Failed to create the directory."
    )
MSG_HASH(
    MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
    "Failed to extract content from compressed file"
    )
MSG_HASH(
    MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
    "Failed to get nickname from client."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD,
    "Failed to load"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_CONTENT,
    "Failed to load content"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_MOVIE_FILE,
    "Failed to load movie file"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_OVERLAY,
    "Αποτυχία φόρτωσης επικαλλύματος."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_STATE,
    "Failed to load state from"
    )
MSG_HASH(
    MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
    "Failed to open libretro core"
    )
MSG_HASH(
    MSG_FAILED_TO_PATCH,
    "Failed to patch"
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
    "Failed to receive header from client."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME,
    "Failed to receive nickname."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
    "Failed to receive nickname from host."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
    "Failed to receive nickname size from host."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
    "Failed to receive SRAM data from host."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
    "Failed to remove disk from tray."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
    "Failed to remove temporary file"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_SRAM,
    "Failed to save SRAM"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_STATE_TO,
    "Failed to save state to"
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME,
    "Failed to send nickname."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_SIZE,
    "Failed to send nickname size."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
    "Failed to send nickname to client."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
    "Failed to send nickname to host."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
    "Failed to send SRAM data to client."
    )
MSG_HASH(
    MSG_FAILED_TO_START_AUDIO_DRIVER,
    "Failed to start audio driver. Will continue without audio."
    )
MSG_HASH(
    MSG_FAILED_TO_START_MOVIE_RECORD,
    "Failed to start movie record."
    )
MSG_HASH(
    MSG_FAILED_TO_START_RECORDING,
    "Failed to start recording."
    )
MSG_HASH(
    MSG_FAILED_TO_TAKE_SCREENSHOT,
    "Failed to take screenshot."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_LOAD_STATE,
    "Failed to undo load state."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_SAVE_STATE,
    "Failed to undo save state."
    )
MSG_HASH(
    MSG_FAILED_TO_UNMUTE_AUDIO,
    "Failed to unmute audio."
    )
MSG_HASH(
    MSG_FATAL_ERROR_RECEIVED_IN,
    "Fatal error received in"
    )
MSG_HASH(
    MSG_FILE_NOT_FOUND,
    "Το αρχείο δεν βρέθηκε"
    )
MSG_HASH(
    MSG_FOUND_AUTO_SAVESTATE_IN,
    "Found auto savestate in"
    )
MSG_HASH(
    MSG_FOUND_DISK_LABEL,
    "Found disk label"
    )
MSG_HASH(
    MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
    "Found first data track on file"
    )
MSG_HASH(
    MSG_FOUND_LAST_STATE_SLOT,
    "Found last state slot"
    )
MSG_HASH(
    MSG_FOUND_SHADER,
    "Found shader"
    )
MSG_HASH(
    MSG_FRAMES,
    "Καρέ"
    )
MSG_HASH(
    MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
    "Per-Game Options: game-specific core options found at"
    )
MSG_HASH(
    MSG_GOT_INVALID_DISK_INDEX,
    "Got invalid disk index."
    )
MSG_HASH(
    MSG_GRAB_MOUSE_STATE,
    "Grab mouse state"
    )
MSG_HASH(
    MSG_GAME_FOCUS_ON,
    "Game focus on"
    )
MSG_HASH(
    MSG_GAME_FOCUS_OFF,
    "Game focus off"
    )
MSG_HASH(
    MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
    "Libretro core is hardware rendered. Must use post-shaded recording as well."
    )
MSG_HASH(
    MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
    "Inflated checksum did not match CRC32."
    )
MSG_HASH(
    MSG_INPUT_CHEAT,
    "Εισαγωγή Απάτης"
    )
MSG_HASH(
    MSG_INPUT_CHEAT_FILENAME,
    "Input Cheat Filename"
    )
MSG_HASH(
    MSG_INPUT_PRESET_FILENAME,
    "Input Preset Filename"
    )
MSG_HASH(
    MSG_INPUT_RENAME_ENTRY,
    "Rename Title"
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
    MSG_LIBRETRO_ABI_BREAK,
    "is compiled against a different version of libretro than this libretro implementation."
    )
MSG_HASH(
    MSG_LIBRETRO_FRONTEND,
    "Frontend for libretro"
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT,
    "Loaded state from slot #%d."
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT_AUTO,
    "Loaded state from slot #-1 (auto)."
    )
MSG_HASH(
    MSG_LOADING,
    "Γίνεται φόρτωση"
    )
MSG_HASH(
    MSG_FIRMWARE,
    "One or more firmware files are missing"
    )
MSG_HASH(
    MSG_LOADING_CONTENT_FILE,
    "Loading content file"
    )
MSG_HASH(
    MSG_LOADING_HISTORY_FILE,
    "Loading history file"
    )
MSG_HASH(
    MSG_LOADING_STATE,
    "Loading state"
    )
MSG_HASH(
    MSG_MEMORY,
    "Μνήμη"
    )
MSG_HASH(
    MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
    "Input replay movie file is not a valid BSV1 file."
    )
MSG_HASH(
    MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
    "Input replay movie format seems to have a different serializer version. Will most likely fail."
    )
MSG_HASH(
    MSG_MOVIE_PLAYBACK_ENDED,
    "Input replay movie playback ended."
    )
MSG_HASH(
    MSG_MOVIE_RECORD_STOPPED,
    "Stopping movie record."
    )
MSG_HASH(
    MSG_NETPLAY_FAILED,
    "Failed to initialize netplay."
    )
MSG_HASH(
    MSG_NO_CONTENT_STARTING_DUMMY_CORE,
    "No content, starting dummy core."
    )
MSG_HASH(
    MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
    "No save state has been overwritten yet."
    )
MSG_HASH(
    MSG_NO_STATE_HAS_BEEN_LOADED_YET,
    "No state has been loaded yet."
    )
MSG_HASH(
    MSG_OVERRIDES_ERROR_SAVING,
    "Error saving overrides."
    )
MSG_HASH(
    MSG_OVERRIDES_SAVED_SUCCESSFULLY,
    "Overrides saved successfully."
    )
MSG_HASH(
    MSG_PAUSED,
    "Παύση."
    )
MSG_HASH(
    MSG_PROGRAM,
    "RetroArch"
    )
MSG_HASH(
    MSG_READING_FIRST_DATA_TRACK,
    "Reading first data track..."
    )
MSG_HASH(
    MSG_RECEIVED,
    "ελήφθη"
    )
MSG_HASH(
    MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
    "Recording terminated due to resize."
    )
MSG_HASH(
    MSG_RECORDING_TO,
    "Εγγραφή σε"
    )
MSG_HASH(
    MSG_REDIRECTING_CHEATFILE_TO,
    "Redirecting cheat file to"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVEFILE_TO,
    "Redirecting save file to"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVESTATE_TO,
    "Redirecting savestate to"
    )
MSG_HASH(
    MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
    "Remap file saved successfully."
    )
MSG_HASH(
    MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
    "Remap file removed successfully."
    )
MSG_HASH(
    MSG_REMOVED_DISK_FROM_TRAY,
    "Removed disk from tray."
    )
MSG_HASH(
    MSG_REMOVING_TEMPORARY_CONTENT_FILE,
    "Removing temporary content file"
    )
MSG_HASH(
    MSG_RESET,
    "Reset"
    )
MSG_HASH(
    MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
    "Restarting recording due to driver reinit."
    )
MSG_HASH(
    MSG_RESTORED_OLD_SAVE_STATE,
    "Restored old save state."
    )
MSG_HASH(
    MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
    "Shaders: restoring default shader preset to"
    )
MSG_HASH(
    MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
    "Reverting savefile directory to"
    )
MSG_HASH(
    MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
    "Reverting savestate directory to"
    )
MSG_HASH(
    MSG_REWINDING,
    "Rewinding."
    )
MSG_HASH(
    MSG_REWIND_INIT,
    "Initializing rewind buffer with size"
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED,
    "Failed to initialize rewind buffer. Rewinding will be disabled."
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
    "Implementation uses threaded audio. Cannot use rewind."
    )
MSG_HASH(
    MSG_REWIND_REACHED_END,
    "Reached end of rewind buffer."
    )
MSG_HASH(
    MSG_SAVED_NEW_CONFIG_TO,
    "Saved new config to"
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT,
    "Saved state to slot #%d."
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT_AUTO,
    "Saved state to slot #-1 (auto)."
    )
MSG_HASH(
    MSG_SAVED_SUCCESSFULLY_TO,
    "Saved successfully to"
    )
MSG_HASH(
    MSG_SAVING_RAM_TYPE,
    "Saving RAM type"
    )
MSG_HASH(
    MSG_SAVING_STATE,
    "Saving state"
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
    MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
    "Several patches are explicitly defined, ignoring all..."
    )
MSG_HASH(
    MSG_SHADER,
    "Σκίαση"
    )
MSG_HASH(
    MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
    "Shader preset saved successfully."
    )
MSG_HASH(
    MSG_SKIPPING_SRAM_LOAD,
    "Skipping SRAM load."
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
    MSG_SLOW_MOTION_REWIND,
    "Slow motion rewind."
    )
MSG_HASH(
    MSG_SRAM_WILL_NOT_BE_SAVED,
    "SRAM will not be saved."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_PLAYBACK,
    "Starting movie playback."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_RECORD_TO,
    "Starting movie record to"
    )
MSG_HASH(
    MSG_STATE_SIZE,
    "State size"
    )
MSG_HASH(
    MSG_STATE_SLOT,
    "State slot"
    )
MSG_HASH(
    MSG_TAKING_SCREENSHOT,
    "Taking screenshot."
    )
MSG_HASH(
    MSG_TO,
    "to"
    )
MSG_HASH(
    MSG_UNDID_LOAD_STATE,
    "Undid load state."
    )
MSG_HASH(
    MSG_UNDOING_SAVE_STATE,
    "Undoing save state"
    )
MSG_HASH(
    MSG_UNKNOWN,
    "Άγνωστο"
    )
MSG_HASH(
    MSG_UNPAUSED,
    "Unpaused."
    )
MSG_HASH(
    MSG_UNRECOGNIZED_COMMAND,
    "Unrecognized command"
    )
MSG_HASH(
    MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
    "Using core name for new config."
    )
MSG_HASH(
    MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
    "Using libretro dummy core. Skipping recording."
    )
MSG_HASH(
    MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
    "Connect device from a valid port."
    )
MSG_HASH(
    MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
    "Disconnecting device from port"
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
    MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
    "Viewport size calculation failed! Will continue using raw data. This will probably not work right ..."
    )
MSG_HASH(
    MSG_VIRTUAL_DISK_TRAY,
    "virtual disk tray."
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
    MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
    "Helps smooth out imperfections in timing when synchronizing audio and video. Be aware that if disabled, proper synchronization is nearly impossible to obtain."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
    "Allow or disallow camera access by cores."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
    "Allow or disallow location services access by cores."
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
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "Allow the WASAPI driver to take exclusive control of the audio device. If disabled, it will use shared mode instead."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
    "Use float format for the WASAPI driver, if supported by your audio device."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "The intermediate buffer length (in frames) when using the WASAPI driver in shared mode."
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
    MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
    "Describes the period when turbo-enabled buttons are toggled. Numbers are described in frames."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
    "Describes how long the period of a turbo-enabled button should be. Numbers are described in frames."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
    "Συγχρονίζει την έξοδο βίντεο της κάρτας γραφικών με τον ρυθμό ανανέωσης της οθόνης. Προτείνεται."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
    "Allow cores to set rotation. When disabled, rotation requests are ignored. Useful for setups where one manually rotates the screen."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
    "Some cores might have a shutdown feature. If enabled, it will prevent the core from shutting RetroArch down. Instead, it loads a dummy core."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
    "Check if all the required firmware is present before attempting to load content."
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
    MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
    "The maximum change in audio input rate. Increasing this enables very large changes in timing at the cost of an inaccurate audio pitch (e.g., running PAL cores on NTSC displays)."
    )
MSG_HASH(
    MSG_FAILED,
    "failed"
    )
MSG_HASH(
    MSG_SUCCEEDED,
    "succeeded"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED,
    "not configured"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
    "not configured, using fallback"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
    "Database Cursor List"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
    "Database - Filter : Developer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
    "Database - Filter : Publisher"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISABLED,
    "Disabled"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLED,
    "Enabled"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
    "Content History Path"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
    "Database - Filter : Origin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
    "Database - Filter : Franchise"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
    "Database - Filter : ESRB Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
    "Database - Filter : ELSPA Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
    "Database - Filter : PEGI Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
    "Database - Filter : CERO Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
    "Database - Filter : BBFC Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
    "Database - Filter : Max Users"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
    "Database - Filter : Releasedate By Month"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
    "Database - Filter : Releasedate By Year"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Database - Filter : Edge Magazine Issue"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
    "Database - Filter : Edge Magazine Rating"
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
    MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
    "Enable or disable composition."
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
    MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
    "User %d Remote Enable"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
    "Netplay will start when content is loaded."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
    "Couldn't find a suitable core or content file, load manually."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
    "Browse URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL,
    "URL Path"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_START,
    "Έναρξη"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
    "Bokeh"
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
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
    "Set the custom width size for the non-windowed fullscreen mode. Leaving it unset will use the desktop resolution."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
    "Set the custom height size for the non-windowed fullscreen mode. Leaving it unset will use the desktop resolution."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
    "Specify custom X axis position for onscreen text."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
    "Specify custom Y axis position for onscreen text."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
    "Specify the font size in points."
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
    MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
    "Screenshots output of GPU shaded material if available."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
    "Forces a certain rotation of the screen. The rotation is added to rotations which the core sets."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
    "Forcibly disable sRGB FBO support. Some Intel OpenGL drivers on Windows have video problems with sRGB FBO support if this is enabled. Enabling this can work around it."
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
    MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
    "Records output of GPU shaded material if available."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
    "When making a savestate, save state index is automatically increased before it is saved. When loading content, the index will be set to the highest existing index."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
    "Block Save RAM from being overwritten when loading save states. Might potentially lead to buggy games."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
    "The maximum rate at which content will be run when using fast forward (e.g., 5.0x for 60 fps content = 300 fps cap). If set to 0.0x, fastforward ratio is unlimited (no FPS cap)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
    "When in slow motion, content will slow down by the factor specified/set."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
    "Run core logic one or more frames ahead then load the state back to reduce perceived input lag."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
    "The number of frames to run ahead. Causes gameplay issues such as jitter if you exceed the number of lag frames internal to the game."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
    "Use a second instance of the RetroArch core to run ahead. Prevents audio problems due to loading state."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
    "Hides the warning message that appears when using RunAhead and the core does not support savestates."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_ENABLE,
    "Ενεργοποίηση επιστροφής. Η επίδοση θα πέσει κατά το παιχνίδι."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
    "Apply cheat immediately after toggling."
)
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
    "Auto-apply cheats when game loads."
)
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
    "The number of times the cheat will be applied.  Use with the other two Iteration options to affect large areas of memory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "After each 'Number of Iterations' the Memory Address will be increased by this number times the 'Memory Search Size'."
)
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
    "After each 'Number of Iterations' the Value will be increased by this amount."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
    "When rewinding a defined number of frames, you can rewind several frames at a time, increasing the rewind speed."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
    "The amount of memory (in MB) to reserve for the rewind buffer.  Increasing this will increase the amount of rewind history."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
    "Each time you increase or decrease the rewind buffer size value via this UI it will change by this amount"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_IDX,
    "Index position in list."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
    "Address bitmask when Memory Search Size < 8-bit."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
    "Select the match to view."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
    ""
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
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ,
    ""
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
    MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_VIEW_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CREATE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
    "Big endian  : 258 = 0x0102,\n"
    "Little endian : 258 = 0x0201"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
    "Sets log level for cores. If a log level issued by a core is below this value, it is ignored."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
    "Enable performance counters for RetroArch (and cores)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
    "Automatically makes a savestate at the end of RetroArch's runtime. RetroArch will automatically load this savestate if 'Auto Load State' is enabled."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
    "Automatically load the auto save state on startup."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
    "Show thumbnails of save states inside the menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
    "Autosaves the non-volatile Save RAM at a regular interval. This is disabled by default unless set otherwise. The interval is measured in seconds. A value of 0 disables autosave."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
    "If enabled, overrides the input binds with the remapped binds set for the current core."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
    "Enable input auto-detection. Will attempt to autoconfigure joypads, Plug-and-Play style."
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
    MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Filter files being shown in filebrowser by supported extensions."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
    "Select an image to set as menu wallpaper."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
    "Dynamically load a new wallpaper depending on context."
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
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
    "Audio output sample rate."
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
    MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
    "The address of the host to connect to."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
    "The port of the host IP address. Can be either a TCP or UDP port."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
    "The password for connecting to the netplay host. Used only in host mode."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
    "Whether to announce netplay games publicly. If unset, clients must manually connect rather than using the public lobby."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
    "The password for connecting to the netplay host with only spectator privileges. Used only in host mode."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
    "Whether to start netplay in spectator mode."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
    "Whether to allow connections in slave mode. Slave-mode clients require very little processing power on either side, but will suffer significantly from network latency."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
    "Whether to disallow connections not in slave mode. Not recommended except for very fast networks with very weak machines."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
    "Whether to run netplay in a mode not requiring save states. If set to true, a very fast network is required, but no rewinding is performed, so there will be no netplay jitter."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
    "The frequency in frames with which netplay will verify that the host and client are in sync."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
    "When hosting, attempt to listen for connections from the public Internet, using UPnP or similar technologies to escape LANs."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
    "Enable stdin command interface."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
    "Enable mouse controls inside the menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POINTER_ENABLE,
    "Enable touch controls inside the menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS,
    "Type of thumbnail to display."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
    "Type of thumbnail to display at the left."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
    "Display the left thumbnail under the right one, on the right side of the screen."
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
    MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
    "Wrap-around to beginning and/or end if boundary of list is reached horizontally or vertically."
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
    MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
    "Sort save files in folders named after the core used."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
    "Sort save states in folders named after the core used."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
    "Request to play with the given input device."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
    "URL to core updater directory on the Libretro buildbot."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
    "URL to assets updater directory on the Libretro buildbot."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "After downloading, automatically extract files contained in the downloaded archives."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
    "Σάρωση για νέα δωμάτια."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION,
    "View more information about the content."
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
    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
    "Enable customized controls by default at startup."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
    "Enable customized configuration by default at startup."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
    "Enable customized core options by default at startup."
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
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
    "Manages cheat details settings."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
    "Start or continue a cheat code search."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_CONTENT,
    "Επανεκκινεί το περιεχόμενο από την αρχή."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Saves an override configuration file which will apply for all content loaded with this core. Will take precedence over the main configuration."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Saves an override configuration file which will apply for all content loaded from the same directory as the current file. Will take precedence over the main configuration."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Saves an override configuration file which will apply for the current content only. Will take precedence over the main configuration."
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
    MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
    "Show advanced settings for power users (hidden by default)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
    "Perform tasks on a separate thread."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
    "Sets the System directory. Cores can query for this directory to load BIOSes, system-specific configs, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
    "Sets start directory for the filebrowser."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DIR,
    "Usually set by developers who bundle libretro/RetroArch apps to point to assets."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Directory to store wallpapers dynamically loaded by the menu depending on context."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
    "Supplementary thumbnails (boxarts/misc. images, etc.) are stored here."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
    "Sets start directory for menu configuration browser."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "The number of frames of input latency for netplay to use to hide network latency. Reduces jitter and makes netplay less CPU-intensive, at the expense of noticeable input lag."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "The range of frames of input latency that may be used to hide network latency. Reduces jitter and makes netplay less CPU-intensive, at the expense of unpredictable input lag."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_CYCLE_TRAY_STATUS,
    "Cycle the current disk. If the disk is inserted, it will eject the disk. If the disk has not been inserted, it will be inserted. "
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_INDEX,
    "Change the disk index."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_OPTIONS,
    "Disk image management."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
    "Select a disk image to insert."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
    "Makes sure the framerate is capped while inside the menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
    "No deviation from core requested timing. Use for Variable Refresh Rate screens, G-Sync, FreeSync."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_LAYOUT,
    "Select a different layout for the XMB interface."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_THEME,
    "Select a different theme for the icon. Changes will take effect after you restart the program."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
    "Enable drop shadows for all icons. This will have a minor performance hit."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
    "Select a different background color gradient theme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
    "Modify the opacity of the background wallpaper."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
    "Select a different background color gradient theme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
    "Select an animated background effect. Can be GPU-intensive depending on the effect. If performance is unsatisfactory, either turn this off or revert to a simpler effect."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_FONT,
    "Select a different main font to be used by the menu."
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
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
    "Modify the opacity of the header graphic."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
    "Modify the opacity of the footer graphic."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DPI_OVERRIDE_ENABLE,
    "The menu normally scales itself dynamically. If you want to set a specific scaling size instead, enable this."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DPI_OVERRIDE_VALUE,
    "Set the custom scaling size here.\n"
    "NOTE: You have to enable 'DPI Override' for this scaling size to take effect."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
    "Save all downloaded files to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
    "Save all remapped controls to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
    "Directory where the program searches for content/cores."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
    "Application/core information files are stored here."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
    "If a joypad is plugged in, that joypad will be autoconfigured if a config file corresponding to it is present inside this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
    "Save all playlists to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
    "If set to a directory, content which is temporarily extracted (e.g. from archives) will be extracted to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
    "Saved queries are stored to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
    "Databases are stored to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
    "This location is queried by default when menu interfaces try to look for loadable assets, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
    "Save all save files to this directory. If not set, will try to save inside the content file's working directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
    "Save all save states to this directory. If not set, will try to save inside the content file's working directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
    "Directory to dump screenshots to."
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
    MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
    "Directory where audio DSP filter files are kept."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
    "Directory where CPU-based video filter files are kept."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
    "Defines a directory where GPU-based video shader files are kept for easy access."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
    "Recordings will be dumped to this directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
    "Recording configurations will be kept here."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
    "Select a different font for onscreen notifications."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
    "Changes to the shader configuration will take effect immediately. Use this if you changed the amount of shader passes, filtering, FBO scale, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
    "Increase or decrease the amount of shader pipeline passes. You can bind a separate shader to each pipeline pass and configure its scale and filtering."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
    "Load a shader preset. The shader pipeline will be automatically set-up."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
    "Save the current shader settings as a new shader preset."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Save the current shader settings as the default settings for this application/core."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Save the current shader settings as the default settings for all files in the current content directory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Save the current shader settings as the default settings for the content."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
    "Modifies the current shader directly. Changes will not be saved to the preset file."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
    "Modifies the shader preset itself currently used in the menu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
    "Increase or decrease the amount of cheats."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
    "Cheat changes will take effect immediately."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
    "Start search for a new cheat.  Number of bits can be changed."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
    "Continue search for a new cheat."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
    "Load a cheat file and replace existing cheats."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
    "Load a cheat file and append to existing cheats."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
    "Save current cheats as a save file."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
    "Γρήγορα πρόσβαση σε όλες τις σχετικές ρυθμίσεις παιχνιδιού."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INFORMATION,
    "View information pertaining to the application/core."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
    "Floating point value for video aspect ratio (width / height), used if the Aspect Ratio is set to 'Config'."
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
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
    "Show Shutdown"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
    "Show/hide the 'Shutdown' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
    "Γρήγορο Μενού"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
    "Show or hide elements on the Quick Menu screen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Show Take Screenshot"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Show/hide the 'Take Screenshot' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Show Save/Load State"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Show/hide the options for saving/loading state."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Show Undo Save/Load State"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Show/hide the options for undoing save/load state."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Show Add to Favorites"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Show/hide the 'Add to Favorites' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
    "Show Start Recording"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
    "Show/hide the 'Start Recording' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
    "Show Start Streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
    "Show/hide the 'Start Streaming' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Show Reset Core Association"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Show/hide the 'Reset Core Association' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
    "Show Options"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
    "Show/hide the 'Options' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
    "Show Controls"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
    "Show/hide the 'Controls' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
    "Show Cheats"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
    "Show/hide the 'Cheats' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
    "Show Shaders"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
    "Show/hide the 'Shaders' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Show Save Core Overrides"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Show/hide the 'Save Core Overrides' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Show Save Game Overrides"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Show/hide the 'Save Game Overrides' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
    "Show Information"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
    "Show/hide the 'Information' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
    "Notification Background Enable"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
    "Notification Background Red Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
    "Notification Background Green Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
    "Notification Background Blue Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
    "Notification Background Opacity"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
    "Disable Kiosk Mode"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
    "Disables kiosk mode. A restart is required for the change to take full effect."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
    "Ενεργοποίηση Λειτουργίας Κιόσκι"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
    "Protects the setup by hiding all configuration related settings."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
    "Set Password For Disabling Kiosk Mode"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
    "Supplying a password when enabling kiosk mode makes it possible to later disable it from the menu, by going to the Main Menu, selecting Disable Kiosk Mode and entering the password."
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
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
    "Notification Red Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
    "Notification Green Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
    "Notification Blue Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
    "Display Frame Count"
    )
MSG_HASH(
    MSG_CONFIG_OVERRIDE_LOADED,
    "Configuration override loaded."
    )
MSG_HASH(
    MSG_GAME_REMAP_FILE_LOADED,
    "Game remap file loaded."
    )
MSG_HASH(
    MSG_CORE_REMAP_FILE_LOADED,
    "Core remap file loaded."
    )
MSG_HASH(
    MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "RunAhead has been disabled because this core does not support save states."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
    "Failed to save state.  RunAhead has been disabled."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
    "Failed to load state.  RunAhead has been disabled."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
    "Failed to create second instance.  RunAhead will now use only one instance."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Automatically add content to playlist"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Automatically scans loaded content so they appear inside playlists."
    )
MSG_HASH(
    MSG_SCANNING_OF_FILE_FINISHED,
    "Scanning of file finished"
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
    MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
    "Watch shader files for changes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
    "Auto-apply changes made to shader files on disk."
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
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Enable border filler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Enable border filler thickness"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Enable background filler thickness"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
    "Για οθόνες CRT μόνο. Προσπαθεί να χρησιμοποιήσει την ακριβή ανάλυση πυρήνα/παιχνιδιού και ρυθμού ανανέωσης."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
    "Switch among native and ultrawide super resolutions."
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
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
    "&File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
    "&Load Core..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
    "&Unload Core"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
    "E&xit"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
    "&Edit"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
    "&Search"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
    "&View"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
    "Closed Docks"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
    "Shader Parameters"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
    "&Options..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
    "Remember dock positions:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
    "Remember window geometry:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
    "Remember last content browser tab:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
    "Θέμα:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
    "<System Default>"
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
    MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
    "&Tools"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
    "&Help"
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
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
    "File Browser"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
    "Top"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
    "Up"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
    "Content Browser"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
    "Boxart"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
    "Screenshot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
    "Title Screen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
    "All Playlists"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE,
    "Core"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
    "Core Info"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
    "<Ask me>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
    "Information"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_WARNING,
    "Warning"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ERROR,
    "Error"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
    "Network Error"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
    "Please restart the program for the changes to take effect."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOG,
    "Log"
    )
#ifdef HAVE_QT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
    "Scan Finished.<br><br>\n"
    "In order for content to be correctly scanned, you must:\n"
    "<ul><li>have a compatible core already downloaded</li>\n"
    "<li>have \"Core Info Files\" updated via Online Updater</li>\n"
    "<li>have \"Databases\" updated via Online Updater</li>\n"
    "<li>restart RetroArch if any of the above was just done</li></ul>\n"
    "Finally, the content must match existing databases from <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">here</a>. If it is still not working, consider <a href=\"https://www.github.com/libretro/RetroArch/issues\">submitting a bug report</a>."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
    "Εμφάνιση Μενού Επιφάνεις Εργασίας"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_WIMP,
    "Opens the desktop menu if closed."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
    "Don't show this again"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_STOP,
    "Στοπ"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
    "Associate Core"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
    "Hidden Playlists"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDE,
    "Hide"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
    "Highlight color:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
    "&Choose..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
    "Select Color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
    "Select Theme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
    "Custom Theme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
    "File path is blank."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
    "File is empty."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
    "Could not open file for reading."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
    "Could not open file for writing."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
    "File does not exist."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
    "Suggest loaded core first:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ZOOM,
    "Zoom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW,
    "View"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
    "Icons"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
    "List"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
    "Overrides"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
    "Options for overriding the global configuration."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
    "Will start playback of the audio stream. Once finished, it will remove the current audio stream from memory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
    "Will start playback of the audio stream. Once finished, it will loop and play the track again from the beginning."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Will start playback of the audio stream. Once finished, it will jump to the next audio stream in sequential order and repeat this behavior. Useful as an album playback mode."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
    "This will stop playback of the audio stream, but not remove it from memory. You can start playing it again by selecting 'Play'."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
    "This will stop playback of the audio stream and remove it entirely from memory."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
    "Adjust the volume of the audio stream."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
    "Add this audio track to an available audio stream slot. If no slots are currently available, it will be ignored."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
    "Add this audio track to an available audio stream slot and play it. If no slots are currently available, it will be ignored."
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
    MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
    "Clear"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
    "Pause achievements for current session (This action will enable savestates, cheats, rewind, pause, and slow-motion)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
    "Resume achievements for current session (This action will disable savestates, cheats, rewind, pause, and slow-motion and reset the current game)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
    "In-Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
    "In-Game"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
    "In-Game (Paused)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
    "Playing"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
    "Paused"
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
    MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
    "Index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
    "View Match #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
    "Match Address: %08X Mask: %02X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
    "Create Code Match #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
    "Delete Match #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
    "Browse Address: %08X"
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
    MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
    "Handler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
    "Memory Search Size"
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
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
    "Memory Address"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
    "Memory Address Mask"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
    "Rumble When Memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
    "Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
    "Rumble Port"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
    "Rumble Primary Strength"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
    "Rumble Primary Duration (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
    "Rumble Secondary Strength"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
    "Rumble Secondary Duration (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
    "Number of Iterations"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
    "Value Increase Each Iteration"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Address Increase Each Iteration"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
    "Add New Cheat After This One"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
    "Add New Cheat Before This One"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
    "Copy This Cheat After"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
    "Copy This Cheat Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
    "Delete This Cheat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
    "Emulator"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_RETRO,
    "RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
    "<Disabled>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
    "Set To Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
    "Increase By Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
    "Decrease By Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
    "Run next cheat if value = memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
    "Run next cheat if value != memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
    "Run next cheat if value < memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
    "Run next cheat if value > memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
    "<Disabled>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
    "Changes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
    "Does Not Change"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
    "Increases"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
    "Decreases"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
    "= Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
    "!= Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
    "< Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
    "> Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
    "Increases by Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
    "Decreases by Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
    "1-bit, max value = 0x01"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
    "2-bit, max value = 0x03"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
    "4-bit, max value = 0x0F"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
    "8-bit, max value = 0xFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
    "16-bit, max value = 0xFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
    "32-bit, max value = 0xFFFFFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_0,
    "1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_1,
    "2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_2,
    "3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_3,
    "4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_4,
    "5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_5,
    "6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_6,
    "7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_7,
    "8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_8,
    "9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_9,
    "10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_10,
    "11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_11,
    "12"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_12,
    "13"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_13,
    "14"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_14,
    "15"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_15,
    "16"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_16,
    "All"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
    "Start or Continue Cheat Search"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
    "Start or Restart Cheat Search"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
    "Add the %u Matches to Your List"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
    "View the List of %u Matches"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
    "Create Code From This Match"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
    "Delete This Match"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
    "Add New Code to Top"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
    "Add New Code to Bottom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
    "Delete All Codes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
    "Reload Game-Specific Cheats"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
    "Equal to %u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
    "Less Than Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
    "Greater Than Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
    "Less Than or Equal To Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
    "Greater Than or Equal To Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
    "Equal to Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
    "Not Equal to Before"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
    "Equal to Before+%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
    "Equal to Before-%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
    "Start or Continue Cheat Search"
    )
MSG_HASH(
    MSG_CHEAT_INIT_SUCCESS,
    "Successfully started cheat search"
    )
MSG_HASH(
    MSG_CHEAT_INIT_FAIL,
    "Failed to start cheat search"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_NOT_INITIALIZED,
    "Searching has not been initialized/started"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_FOUND_MATCHES,
    "New match count = %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
    "Big Endian"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
    "Added %u matches"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
    "Failed to add matches"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
    "Created code from match"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
    "Failed to create code"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
    "Deleted match"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
    "Not enough room.  The total number of cheats you can have is 100."
    )
MSG_HASH(
    MSG_CHEAT_ADD_TOP_SUCCESS,
    "New cheat added to top of list."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BOTTOM_SUCCESS,
    "New cheat added to bottom of list."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
    "Press right five times to delete all cheats."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_SUCCESS,
    "All cheats deleted."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BEFORE_SUCCESS,
    "New cheat added before this one."
    )
MSG_HASH(
    MSG_CHEAT_ADD_AFTER_SUCCESS,
    "New cheat added after this one."
    )
MSG_HASH(
    MSG_CHEAT_COPY_BEFORE_SUCCESS,
    "Cheat copied before this one."
    )
MSG_HASH(
    MSG_CHEAT_COPY_AFTER_SUCCESS,
    "Cheat copied after this one."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_SUCCESS,
    "Cheat deleted."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
    "Progress:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
    "\"All Playlists\" max list entries:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
    "\"All Playlists\" max grid entries:"
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
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
    "Are you sure you want to delete the playlist \"%1\"?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_QUESTION,
    "Question"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
    "Could not delete file."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
    "Could not rename file."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
    "Gathering list of files..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
    "Adding files to playlist..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
    "Playlist Entry"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
    "Όνομα:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
    "Path:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
    "Πυρήνας:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
    "Database:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
    "Extensions:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
    "(space-separated; includes all by default)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
    "Filter inside archives"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
    "(used to find thumbnails)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
    "Are you sure you want to delete the item \"%1\"?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
    "Please choose a single playlist first."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE,
    "Delete"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
    "Add Entry..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
    "Add File(s)..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
    "Add Folder..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_EDIT,
    "Edit"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
    "Select Files"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
    "Select Folder"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
    "<multiple>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
    "Error updating playlist entry."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
    "Please fill out all required fields."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
    "Update RetroArch (nightly)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
    "RetroArch updated successfully. Please restart the application for the changes to take effect."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
    "Update failed."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
    "Contributors"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
    "Current shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
    "Move Down"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
    "Move Up"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD,
    "Load"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SAVE,
    "Save"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_REMOVE,
    "Remove"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_APPLY,
    "Apply"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
    "Add Pass"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
    "Clear All Passes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
    "No shader passes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
    "Reset Pass"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
    "Reset All Passes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
    "Reset Parameter"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
    "Download thumbnail"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
    "A download is already in progress."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
    "Start on playlist:"
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
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
    "Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
    "YouTube"
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

MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "OK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
    "Χρώμα Θέματος Μενού"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "Basic White"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "Basic Black"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Select a different color theme."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use preferred system color theme")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use your operating system's color theme (if any) - overrides theme settings.")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWEST,
      "Lowest")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWER,
      "Lower")
MSG_HASH(MSG_RESAMPLER_QUALITY_NORMAL,
      "Normal")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHER,
      "Higher")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHEST,
      "Highest")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "No music available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "No videos available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "No images available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "No favorites available."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
      "Remember Window Position and Size")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "CoreAudio support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "CoreAudio V3 support"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
      "Menu Widgets")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
      "Video Shaders")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
      "Scan without core match")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
      "When disabled, content is only added to playlists if you have a core installed that supports its extension. By enabling this, it will add to playlist regardless. This way, you can install the core you need later on after scanning.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "Animation Horizontal Icon Highlight")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "Animation Move Up/Down")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "Animation Main Menu Opens/Closes")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Disc Information"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "View information about inserted media discs."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Frontend Logging Level"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Sets log level for the frontend. If a log level issued by the frontend is below this value, it is ignored."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
      "Framerate Update Interval (in frames)")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
      "Framerate display will be updated at the set interval (in frames).")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show Restart Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show/hide the 'Restart Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show Close Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show/hide the 'Close Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show Resume Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show/hide the 'Resume Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Show Input"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Show or hide 'Input Settings' on the Settings screen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "AI Service"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Change settings for the AI Service (Translation/TTS/Misc)."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
      "AI Service Output")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
      "AI Service URL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
      "AI Service Enabled")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
      "Pauses gameplay during translation (Image mode), or continues to run (Speech mode)")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
      "A http:// url pointing to the translation service to use.")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
      "Enable AI Service to run when the AI Service hotkey is pressed.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
      "Target Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
      "The language the service will translate to. If set to 'Don't Care', it will default to English.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
      "Source Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
      "The language the service will translate from. If set to 'Don't Care', it will attempt to auto-detect the language. Setting it to a specific language will make the translation more accurate.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Czech"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Danish"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "Swedish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Croatian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Catalan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Bulgarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Basque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerbaijani"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Albanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Afrikaans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "Filipino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Finnish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galician"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Georgian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Gujarati"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Haitian Creole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebrew"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "Hindi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Hungarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "Icelandic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Indonesian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Kannada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "Latin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Latvian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Lithuanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Macedonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltese"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "Norwegian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "Persian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Serbian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Slovak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Slovenian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Swahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tamil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Telugu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Thai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ukrainian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "Urdu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Welsh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Yiddish"
   )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_DISC,
      "Load a physical media disc. You should first select the core (Load Core)  you intend to use with the disc.")
MSG_HASH(MENU_ENUM_SUBLABEL_DUMP_DISC,
      "Dump the physical media disc to internal storage. It will be saved as an image file.")
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Image Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Speech Mode"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
      "Remove")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
      "Remove shader presets of a specific type.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove Global Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove the Global Preset, used by all content and all cores.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove Core Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove the Core Preset, used by all content ran with the currently loaded core.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove Content Directory Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove the Content Directory Preset, used by all content inside the current working directory.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove Game Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove the Game Preset, used only for the specific game in question.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
      "Frame Time Counter")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
      "Adjust settings influencing the frame time counter (only active when threaded video is disabled).")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
      "Use modern decorated animations, notifications, indicators and controls instead of the old text only system.")

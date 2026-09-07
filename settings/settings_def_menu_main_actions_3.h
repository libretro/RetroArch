/* Single-source definitions: third main menu action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CDROM; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CDROM) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(LOAD_DISC,
      "load_disc",
      "Load Disc",
      "Load a physical media disc. First select the core (Load Core) to use with the disc.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CDROM; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CDROM) || defined(SETTINGS_DEF_STRINGS_PASS)
/* FIXME Is a specific image format used? Is it determined automatically? User choice? */
S_ACTION(DUMP_DISC,
      "dump_disc",
      "Dump Disc",
      "Dump the physical media disc to internal storage. It will be saved as an image file.")
#endif
#ifdef HAVE_CDROM
#ifdef HAVE_LAKKA
S_ACTION(EJECT_DISC,
      "eject_disc",
      "Eject Disc",
      "Ejects the disc from physical CD/DVD drive.")
#endif
#endif

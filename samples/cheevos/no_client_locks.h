/* Client state belongs to its owner; badge requests use the LC queue. */
#if defined(__GNUC__)
#pragma GCC poison rc_mutex_t rc_mutex_init rc_mutex_destroy
#pragma GCC poison rc_mutex_lock rc_mutex_unlock
#pragma GCC poison rcheevos_badge_lock_word
#pragma GCC poison RCHEEVOS_BADGE_LOCK RCHEEVOS_BADGE_UNLOCK
#endif

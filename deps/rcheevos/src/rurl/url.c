#include "rurl.h"

#include <stdio.h>

static int rc_url_encode(char* encoded, size_t len, const char* str) {
  for (;;) {
    switch (*str) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        if (len >= 2) {
          *encoded++ = *str++;
          len--;
        }
        else {
          return -1;
        }

        break;
      
      default:
        if (len >= 4) {
          snprintf(encoded, len, "%%%02x", (unsigned char)*str);
          encoded += 3;
          str++;
          len -= 3;
        }
        else {
          return -1;
        }

        break;
      
      case 0:
        *encoded = 0;
        return 0;
    }
  }
}

int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned cheevo_id, int hardcore) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
    urle_user_name,
    urle_login_token,
    cheevo_id,
    hardcore ? 1 : 0
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, int value, unsigned char hash[16]) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=submitlbentry&u=%s&t=%s&i=%u&s=%d&v=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    urle_user_name,
    urle_login_token,
    lboard_id,
    value,
    hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
    hash[ 8], hash[ 9], hash[10], hash[11],hash[12], hash[13], hash[14], hash[15]
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_gameid(char* buffer, size_t size, unsigned char hash[16]) {
  int written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=gameid&m=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
    hash[ 8], hash[ 9], hash[10], hash[11],hash[12], hash[13], hash[14], hash[15]
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=patch&u=%s&t=%s&g=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name) {
  int written = snprintf(
    buffer,
    size,
    "http://i.retroachievements.org/Badge/%s",
    badge_name
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password) {
  char urle_user_name[64];
  char urle_password[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_password, sizeof(urle_password), password) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
    urle_user_name,
    urle_password
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=login&u=%s&t=%s",
    urle_user_name,
    urle_login_token
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=%d",
    urle_user_name,
    urle_login_token,
    gameid,
    hardcore ? 1 : 0
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

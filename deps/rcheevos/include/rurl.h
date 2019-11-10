#ifndef RURL_H
#define RURL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned cheevo_id, int hardcore);

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, int value, unsigned char hash[16]);

int rc_url_get_gameid(char* buffer, size_t size, unsigned char hash[16]);

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name);

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password);

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token);

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore);

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);

#ifdef __cplusplus
}
#endif

#endif /* RURL_H */

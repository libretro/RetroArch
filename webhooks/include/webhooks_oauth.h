#ifndef __WEBHOOKS_OAUTH_H
#define __WEBHOOKS_OAUTH_H

RETRO_BEGIN_DECLS

void woauth_initiate_pairing();

bool woauth_is_pairing();

void woauth_abort_pairing();

const char* woauth_get_accesstoken();

RETRO_END_DECLS

#endif //__WEBHOOKS_OAUTH_H

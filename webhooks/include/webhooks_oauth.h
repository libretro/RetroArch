#ifndef __WEBHOOKS_OAUTH_H
#define __WEBHOOKS_OAUTH_H

RETRO_BEGIN_DECLS

void woauth_initiate_pairing
(
  void
);

bool woauth_is_pairing
(
  void
);

void woauth_abort_pairing
(
  void
);

const char* woauth_get_accesstoken
(
  void
);

RETRO_END_DECLS

#endif //__WEBHOOKS_OAUTH_H

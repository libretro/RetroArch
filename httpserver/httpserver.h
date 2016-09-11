#ifndef __RARCH_HTTPSERVR_H
#define __RARCH_HTTPSERVR_H

#ifdef __cplusplus
extern "C" {
#endif

  int  httpserver_init(unsigned port);
  void httpserver_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* __RARCH_HTTPSERVR_H */

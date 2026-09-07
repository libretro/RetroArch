/*  RetroArch - A frontend for libretro.
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

#include <compat/strl.h>
#include <encodings/base64.h>
#include <lists/string_list.h>
#include <lrc_hash.h>
#include <net/net_http.h>
#include <time/rtime.h>
#include <string/stdstring.h>

#include "../cloud_sync_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

typedef struct
{
   char path[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   cloud_sync_complete_handler_t cb;
   void *user_data;
   RFILE *rfile;
} webdav_cb_state_t;

typedef void (*webdav_mkdir_cb_t)(bool success, webdav_cb_state_t *state);

typedef struct
{
   char url[PATH_MAX_LENGTH];
   char *last_slash;
   char post_slash;
   webdav_mkdir_cb_t cb;
   webdav_cb_state_t *cb_st;
} webdav_mkdir_state_t;

/* TODO: all of this HTTP auth stuff should
 * probably live in libretro-common/net? */
typedef struct
{
   char url[PATH_MAX_LENGTH];

   /* Collections this sync has already created on the server. */
   struct string_list *dirs;

   bool basic;
   char *basic_auth_header;

   char *username;
   char *ha1hash;
   char *realm;
   char *nonce;
   char *algo;
   char *opaque;
   const char *cnonce;
   unsigned nc;
   bool qop_auth;
   bool dav_verified;
} webdav_state_t;

static webdav_state_t webdav_driver_st = {0};

webdav_state_t *webdav_state_get_ptr(void)
{
   return &webdav_driver_st;
}

static char *webdav_create_basic_auth(void)
{
   int         flen;
   char       *base64auth;
   char        userpass[512];
   settings_t *settings  = config_get_ptr();
   size_t      _len      = 0;
   if (*settings->arrays.webdav_username)
      _len += strlcpy(userpass + _len, settings->arrays.webdav_username, sizeof(userpass) - _len);
   userpass[_len++] = ':';
   if (*settings->arrays.webdav_password)
      _len += strlcpy(userpass + _len, settings->arrays.webdav_password, sizeof(userpass) - _len);
   userpass[_len]   = '\0';
   base64auth = base64(userpass, (int)_len, &flen);
   _len  = strlcpy_lit(userpass, "Authorization: Basic ", sizeof(userpass));
   _len += strlcpy(userpass + _len, base64auth, sizeof(userpass) - _len);
   free(base64auth);
   userpass[_len++] = '\r';
   userpass[_len++] = '\n';
   userpass[_len  ] = '\0';
   return strdup(userpass);
}

static void webdav_cleanup_digest(void)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();

   if (webdav_st->ha1hash)
      free(webdav_st->ha1hash);
   webdav_st->ha1hash = NULL;

   if (webdav_st->realm)
      free(webdav_st->realm);
   webdav_st->realm = NULL;

   if (webdav_st->nonce)
      free(webdav_st->nonce);
   webdav_st->nonce = NULL;

   if (webdav_st->algo)
      free(webdav_st->algo);
   webdav_st->algo = NULL;

   if (webdav_st->opaque)
      free(webdav_st->opaque);
   webdav_st->opaque = NULL;

   webdav_st->qop_auth = false;
   webdav_st->nc = 1;
}

static char *webdav_create_ha1_hash(char *user, char *realm, char *pass)
{
   MD5_CTX md5;
   unsigned char digest[16];
   char *hash = (char*)malloc(33);

   MD5_Init(&md5);
   MD5_Update(&md5, user, (unsigned long)strlen(user));
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, realm, (unsigned long)strlen(realm));
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, pass, (unsigned long)strlen(pass));
   MD5_Final(digest, &md5);

   snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
      );

   return hash;
}

static bool webdav_create_digest_auth(char *digest)
{
   size_t _len;
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   settings_t     *settings  = config_get_ptr();
   char           *ptr       = digest + (sizeof("WWW-Authenticate: Digest")-1);
   char           *end       = ptr + strlen(ptr);

   if (   !*settings->arrays.webdav_username
       && !*settings->arrays.webdav_password)
      return false;

   webdav_cleanup_digest();

   webdav_st->username = settings->arrays.webdav_username;

   while (ptr < end)
   {
      while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')
         ++ptr;

      if (!*ptr)
         break;

      if (string_starts_with(ptr, "realm=\""))
      {
         ptr += (sizeof("realm=\"")-1);
         _len = strchr(ptr, '"') + 1 - ptr;
         webdav_st->realm = (char*)malloc(_len);
         strlcpy(webdav_st->realm, ptr, _len);
         ptr += _len;

         webdav_st->ha1hash = webdav_create_ha1_hash(
               webdav_st->username, webdav_st->realm,
               settings->arrays.webdav_password);
      }
      else if (string_starts_with(ptr, "qop=\""))
      {
         char *tail;
         ptr += (sizeof("qop=\"")-1);
         tail = strchr(ptr, '"');
         while (ptr < tail)
         {
            if (    string_starts_with(ptr, "auth")
                && (ptr[4] == ',' || ptr[4] == '"'))
            {
               webdav_st->qop_auth = true;
               break;
            }
            while (*ptr != ',' && *ptr != '"' && *ptr != '\0')
               ptr++;
            ptr++;
         }
         /* not even going to try for auth-int, sorry */
         if (!webdav_st->qop_auth)
            return false;
         while (*ptr != ',' && *ptr != '"' && *ptr != '\0')
            ptr++;
         ptr++;
      }
      else if (string_starts_with(ptr, "nonce=\""))
      {
         ptr += (sizeof("nonce=\"")-1);
         _len = strchr(ptr, '"') + 1 - ptr;
         webdav_st->nonce = (char*)malloc(_len);
         strlcpy(webdav_st->nonce, ptr, _len);
         ptr += _len;
      }
      else if (string_starts_with(ptr, "algorithm="))
      {
         ptr += (sizeof("algorithm=")-1);
         if (strchr(ptr, ','))
         {
            _len = strchr(ptr, ',') + 1 - ptr;
            webdav_st->algo = (char*)malloc(_len);
            strlcpy(webdav_st->algo, ptr, _len);
            ptr += _len;
         }
         else
         {
            webdav_st->algo = strdup(ptr);
            ptr += strlen(ptr);
         }
      }
      else if (string_starts_with(ptr, "opaque=\""))
      {
         ptr += (sizeof("opaque=\"")-1);
         _len = strchr(ptr, '"') + 1 - ptr;
         webdav_st->opaque = (char*)malloc(_len);
         strlcpy(webdav_st->opaque, ptr, _len);
         ptr += _len;
      }
      else
      {
         while (*ptr != '=' && *ptr != '\0')
            ptr++;
         ptr++;
         if (*ptr == '"')
         {
            ptr++;
            while (*ptr != '"' && *ptr != '\0')
               ptr++;
            ptr++;
         }
         else
         {
            while (*ptr != ',' && *ptr != ',')
               ptr++;
         }
      }

      while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')
         ++ptr;
      if (*ptr == ',')
         ptr++;
   }

   if (!webdav_st->ha1hash || !webdav_st->nonce)
      return false;

   webdav_st->cnonce = "1a2b3c4f";
   webdav_st->basic = false;

   return true;
}

static char *webdav_create_ha1(void)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char           *hash;
   MD5_CTX         md5;
   unsigned char   digest[16];

   if (!string_is_equal(webdav_st->algo, "MD5-sess"))
      return strdup(webdav_st->ha1hash);

   hash = (char*)malloc(33);

   MD5_Init(&md5);
   MD5_Update(&md5, webdav_st->ha1hash, 32);
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, webdav_st->nonce, (unsigned long)strlen(webdav_st->nonce));
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, webdav_st->cnonce, (unsigned long)strlen(webdav_st->cnonce));
   MD5_Final(digest, &md5);

   snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
      );

   return hash;
}

static char *webdav_create_ha2(const char *method, const char *path)
{
   MD5_CTX         md5;
   unsigned char   digest[16];
   /* no attempt at supporting auth-int, everything else uses this */
   char           *hash      = (char*)malloc(33);

   MD5_Init(&md5);
   MD5_Update(&md5, method, (unsigned long)strlen(method));
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, path, (unsigned long)strlen(path));
   MD5_Final(digest, &md5);

   snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
      );

   return hash;
}

static char *webdav_create_digest_response(const char *method, const char *path)
{
   MD5_CTX         md5;
   unsigned char   digest[16];
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char           *ha1       = webdav_create_ha1();
   char           *ha2       = webdav_create_ha2(method, path);
   char           *hash      = (char*)malloc(33);

   MD5_Init(&md5);
   MD5_Update(&md5, ha1, 32);
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, webdav_st->nonce, (unsigned long)strlen(webdav_st->nonce));
   if (webdav_st->qop_auth)
   {
      char nonceCount[10];
      snprintf(nonceCount, sizeof(nonceCount), "%08x", webdav_st->nc);
      MD5_Update(&md5, ":", 1);
      MD5_Update(&md5, nonceCount, (unsigned long)strlen(nonceCount));
      MD5_Update(&md5, ":", 1);
      MD5_Update(&md5, webdav_st->cnonce, (unsigned long)strlen(webdav_st->cnonce));
      MD5_Update(&md5, ":", 1);
      MD5_Update(&md5, "auth", (sizeof("auth")-1));
   }
   MD5_Update(&md5, ":", 1);
   MD5_Update(&md5, ha2, 32);
   MD5_Final(digest, &md5);

   snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
      );

   free(ha1);
   free(ha2);

   return hash;
}

static char *webdav_create_digest_auth_header(const char *method, const char *url)
{
   size_t _len, __len;
   char nonceCount[10];
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char           *header    = NULL;
   char           *response  = NULL;
   const char     *path      = url;
   int             count     = 0;
   size_t          total     = 0;

   do
   {
      path++;
      path = strchr(path, '/');
      count++;
   } while (count < 3 && *path != '\0');

   response = webdav_create_digest_response(method, path);
   __len    = snprintf(nonceCount, sizeof(nonceCount),
         "%08x", webdav_st->nc++);

   _len  = STRLEN_CONST("Authorization: Digest ");
   _len += STRLEN_CONST("username=\"") + strlen(webdav_st->username) + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("realm=\"")    + strlen(webdav_st->realm) + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("nonce=\"")    + strlen(webdav_st->nonce) + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("uri=\"")      + strlen(path) + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("nc=\"")       + __len + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("cnonce=\"")   + strlen(webdav_st->cnonce) + STRLEN_CONST("\", ");
   if (webdav_st->qop_auth)
      _len += STRLEN_CONST("qop=\"auth\", ");
   if (webdav_st->opaque)
      _len += STRLEN_CONST("opaque=\"") + strlen(webdav_st->opaque) + STRLEN_CONST("\", ");
   _len += STRLEN_CONST("response=\"")  + strlen(response) + STRLEN_CONST("\"\r\n");
   _len += 1;

   total  = _len;
   _len   = 0;
   header = (char*)malloc(total);
   _len   = strlcpy_lit(header, "Authorization: Digest username=\"", total - _len);
   _len  += strlcpy(header + _len, webdav_st->username, total - _len);
   _len  += strlcpy_lit(header + _len, "\", realm=\"", total - _len);
   _len  += strlcpy(header + _len, webdav_st->realm, total - _len);
   _len  += strlcpy_lit(header + _len, "\", nonce=\"", total - _len);
   _len  += strlcpy(header + _len, webdav_st->nonce, total - _len);
   _len  += strlcpy_lit(header + _len, "\", uri=\"", total - _len);
   _len  += strlcpy(header + _len, path, total - _len);
   _len  += strlcpy_lit(header + _len, "\", nc=\"", total - _len);
   _len  += strlcpy(header + _len, nonceCount, total - _len);
   _len  += strlcpy_lit(header + _len, "\", cnonce=\"", total - _len);
   _len  += strlcpy(header + _len, webdav_st->cnonce, total - _len);
   if (webdav_st->qop_auth)
      _len += strlcpy_lit(header + _len, "\", qop=\"auth", total - _len);
   if (webdav_st->opaque)
   {
      _len += strlcpy_lit(header + _len, "\", opaque=\"", total - _len);
      _len += strlcpy(header + _len, webdav_st->opaque, total - _len);
   }
   _len += strlcpy_lit(header + _len, "\", response=\"", total - _len);
   _len += strlcpy(header + _len, response, total - _len);
   strlcpy_lit(header + _len, "\"\r\n", total - _len);
   free(response);
   return header;
}

static char *webdav_get_auth_header(const char *method, const char *url)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   settings_t     *settings  = config_get_ptr();

   if (   !*settings->arrays.webdav_username
       && !*settings->arrays.webdav_password)
      return NULL;

   if (webdav_st->basic)
   {
      if (!webdav_st->basic_auth_header)
         webdav_st->basic_auth_header = webdav_create_basic_auth();
      return strdup(webdav_st->basic_auth_header);
   }

   return webdav_create_digest_auth_header(method, url);
}

static void webdav_log_http_failure(const char *path,
      http_transfer_data_t *data, const char *err)
{
    size_t i;
    size_t _len = 0;
    /* Cloud sync runs several transfers at once, so one RARCH_WARN per
     * header interleaves with the other tasks' output and produces logs
     * with lines spliced into each other.  Build the whole report first
     * and emit it in a single write. */
    char report[2048];

    _len  = snprintf(report, sizeof(report), "[webdav] Failed: %s: HTTP %d",
          path, data->status);
    /* No status means the transport failed before the server answered;
     * the task's error names the stage (and the TLS code, if any),
     * which is the only clue a log on a console will carry. */
    if (data->status < 0 && err && *err && _len < sizeof(report) - 1)
       _len += snprintf(report + _len, sizeof(report) - _len, " (%s)", err);
    for (i = 0; data->headers && i < data->headers->size; i++)
    {
       if (_len >= sizeof(report) - 1)
          break;
       report[_len++] = '\n';
       _len += strlcpy(report + _len, data->headers->elems[i].data,
             sizeof(report) - _len);
    }
    RARCH_WARN("%s\n", report);
    /* The buffer returned by net_http_data() is sized exactly to
     * data->len -- net_http.c shrinks response->data with
     * realloc(p, response->len) on transition to P_DONE.  Writing a
     * NUL at data->data[data->len] is therefore a one-byte heap
     * overflow that corrupts the next chunk's metadata; glibc later
     * aborts in malloc_printerr from an unrelated free() (typically
     * the next task_http_transfer_cleanup at startup when cloud
     * sync issues many requests in quick succession).  Print with
     * an explicit length instead of relying on NUL-termination. */
    if (data->data)
        RARCH_WARN("%.*s\n", (int)data->len, (const char*)data->data);
}

static bool webdav_needs_reauth(http_transfer_data_t *data)
{
   size_t i;

   if (!data || data->status != 401 || !data->headers)
      return false;

   for (i = 0; i < data->headers->size; i++)
   {
      /* Header names are case-insensitive (RFC 9110 5.1), and the
       * emscripten backend gets them from the browser, which
       * lower-cases them.  The offset skipped by
       * webdav_create_digest_auth() below is a fixed length, so
       * matching case-insensitively here stays correct. */
      if (!string_starts_with_case_insensitive(data->headers->elems[i].data,
               "WWW-Authenticate: Digest "))
         continue;

      RARCH_DBG("[webdav] Found WWW-Authenticate: Digest header\n");
      if (webdav_create_digest_auth(data->headers->elems[i].data))
         return true;
      RARCH_WARN("[webdav] Failure creating WWW-Authenticate: Digest header\n");
   }

   return false;
}

/* RFC 9110 5.6.1.2: the Allow header is a comma-separated list of
 * method tokens describing what the *target resource* supports. Match
 * whole tokens so that a method merely containing another's name
 * cannot be mistaken for it. */
static bool webdav_allow_lists_method(const char *allow, const char *method)
{
   const char *p = allow;
   size_t _len   = strlen(method);

   while (*p)
   {
      const char *tok;

      while (*p == ' ' || *p == '\t' || *p == ',')
         p++;
      tok = p;
      while (*p && *p != ',' && *p != ' ' && *p != '\t')
         p++;
      if (     (size_t)(p - tok) == _len
            && string_starts_with_case_insensitive(tok, method))
         return true;
   }

   return false;
}

/* Inspects the response to the OPTIONS request cloud sync opens with.
 *
 * The URL cloud sync is given is a collection, and Allow describes the
 * collection itself: PUT is not defined on one (RFC 4918 9.7.1), so a
 * correct server may well leave it out and its absence says nothing
 * about the endpoint. What separates a WebDAV server from a plain HTTP
 * server that answers OPTIONS is the DAV header it announces itself
 * with (RFC 4918 10.1), and failing that, whether it offers any method
 * only WebDAV defines. */
static void webdav_check_options(http_transfer_data_t *data,
      bool *dav, bool *allow_seen, bool *allow_dav_method)
{
   size_t i;

   for (i = 0; data->headers && i < data->headers->size; i++)
   {
      const char *hdr = data->headers->elems[i].data;

      if (string_starts_with_case_insensitive(hdr, "DAV:"))
         *dav = true;
      else if (string_starts_with_case_insensitive(hdr, "Allow:"))
      {
         const char *allow = hdr + STRLEN_CONST("Allow:");

         *allow_seen = true;
         if (     webdav_allow_lists_method(allow, "PROPFIND")
               || webdav_allow_lists_method(allow, "MKCOL")
               || webdav_allow_lists_method(allow, "PROPPATCH"))
            *allow_dav_method = true;
      }
   }
}

static void webdav_stat_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   webdav_state_t       *webdav_st    = webdav_state_get_ptr();
   webdav_cb_state_t    *webdav_cb_st = (webdav_cb_state_t *)user_data;
   http_transfer_data_t *data         = (http_transfer_data_t*)task_data;
   bool                  success      = (data && data->status >= 200 && data->status < 300);

   if (!webdav_cb_st)
      return;

   if (!data)
      RARCH_WARN("[webdav] Did not get data for stat, is the server down?\n");

   if (webdav_needs_reauth(data))
   {
      char *auth_header = webdav_get_auth_header("OPTIONS", webdav_st->url);
      task_push_webdav_stat(webdav_st->url, true, auth_header, webdav_stat_cb, webdav_cb_st);
      free(auth_header);
      return;
   }

   if (!success && data)
       webdav_log_http_failure(webdav_st->url, data, err);

   if (success && data)
   {
      bool dav              = false;
      bool allow_seen       = false;
      bool allow_dav_method = false;

      webdav_check_options(data, &dav, &allow_seen, &allow_dav_method);

      /* A server that answers OPTIONS with neither the DAV header nor
       * a single WebDAV method in Allow is an ordinary HTTP server,
       * and it will refuse the first PUT far too late to tell the user
       * anything useful. Anything that shows either one is taken at
       * its word; PUT's absence from a collection's Allow means
       * nothing (RFC 4918 9.7.1) and is not consulted. */
      webdav_st->dav_verified = dav || allow_dav_method;

      if (webdav_st->dav_verified)
      {
         if (!dav)
            RARCH_WARN("[webdav] %s did not return a DAV header, but offers WebDAV methods. Continuing.\n",
                  webdav_st->url);
      }
      else if (allow_seen)
      {
         RARCH_ERR("[webdav] %s answers OPTIONS but offers no WebDAV method, so it is not a WebDAV endpoint. Check the URL: providers commonly serve WebDAV over https only, on a dedicated host.\n",
               webdav_st->url);
         success = false;
      }
      else
         RARCH_WARN("[webdav] %s returned neither a DAV header nor an Allow header, so it may not be a WebDAV endpoint. Continuing anyway.\n",
               webdav_st->url);
   }

   webdav_cb_st->cb(webdav_cb_st->user_data, NULL, success, NULL);
   free(webdav_cb_st);
}

static bool webdav_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   char *auth_header;
   size_t _len                  = 0;
   settings_t        *settings  = config_get_ptr();
   const char        *url       = settings->arrays.webdav_url;
   webdav_state_t    *webdav_st = webdav_state_get_ptr();

   if (!url || !*url)
      return false;

#ifndef HAVE_SSL
   if (strncmp(url, "https", 5) == 0)
   {
      RARCH_ERR("[webdav] This build has no TLS support, so the https URL %s cannot be used.\n", url);
      return false;
   }
#endif
   /* TODO/FIXME: LOCK? */
   if (!strstr(url, "://"))
       _len += strlcpy_lit(webdav_st->url, "http://", (sizeof("http://")-1));
   strlcpy(webdav_st->url + _len, url, sizeof(webdav_st->url) - _len);
   fill_pathname_slash(webdav_st->url, sizeof(webdav_st->url));

   /* A new sync knows nothing about the server's collections yet. */
   if (webdav_st->dirs)
      string_list_free(webdav_st->dirs);
   webdav_st->dirs         = NULL;
   webdav_st->dav_verified = false;

   /* URL/username/password may have changed, redo auth check */
   webdav_st->basic = true;
   auth_header      = webdav_get_auth_header(NULL, NULL);

   if (auth_header)
   {
      webdav_cb_state_t *webdav_cb_st = (webdav_cb_state_t*)calloc(1, sizeof(webdav_cb_state_t));
      webdav_cb_st->cb        = cb;
      webdav_cb_st->user_data = user_data;
      task_push_webdav_stat(webdav_st->url, true, auth_header, webdav_stat_cb, webdav_cb_st);
      free(auth_header);
   }
   else
   {
      RARCH_WARN("[webdav] No basic auth header, assuming no user, check username/password?\n");
      cb(user_data, NULL, true, NULL);
   }
   return true;
}

static bool webdav_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();

   /* TODO/FIXME: UNLOCK? */

   if (webdav_st->basic_auth_header)
      free(webdav_st->basic_auth_header);
   webdav_st->basic_auth_header = NULL;

   if (webdav_st->dirs)
      string_list_free(webdav_st->dirs);
   webdav_st->dirs = NULL;

   webdav_cleanup_digest();

   cb(user_data, NULL, true, NULL);
   return true;
}

static void webdav_read_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   webdav_cb_state_t    *webdav_cb_st = (webdav_cb_state_t *)user_data;
   http_transfer_data_t *data         = (http_transfer_data_t*)task_data;
   RFILE                *file         = NULL;
   bool success = (data
              && ((data->status >= 200 && data->status < 300) || data->status == 404));

   if (!success && data)
       webdav_log_http_failure(webdav_cb_st->path, data, err);

   if (webdav_needs_reauth(data))
   {
      webdav_state_t *webdav_st = webdav_state_get_ptr();
      char            url[PATH_MAX_LENGTH];
      char            url_encoded[PATH_MAX_LENGTH];
      char           *auth_header;

      fill_pathname_join_special(url, webdav_st->url, webdav_cb_st->path, sizeof(url));
      net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

      RARCH_DBG("[webdav] GET %s\n", url_encoded);
      auth_header = webdav_get_auth_header("GET", url_encoded);
      task_push_http_transfer_with_headers(url_encoded, true, NULL, auth_header, webdav_read_cb, webdav_cb_st);
      free(auth_header);
      return;
   }

   if (success && data->data && webdav_cb_st)
   {
      /* TODO/FIXME: it would be better if writing
       * to the file happened during the network reads */
      file = filestream_open(webdav_cb_st->file,
                             RETRO_VFS_FILE_ACCESS_READ_WRITE,
                             RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (file)
      {
         filestream_write(file, data->data, data->len);
         filestream_seek(file, 0, SEEK_SET);
      }
   }

   if (webdav_cb_st)
   {
      webdav_cb_st->cb(webdav_cb_st->user_data,
            webdav_cb_st->path, success, file);
      free(webdav_cb_st);
   }
}

static bool webdav_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   void              *t;
   char              *auth_header;
   char               url[PATH_MAX_LENGTH];
   char               url_encoded[PATH_MAX_LENGTH];
   webdav_state_t    *webdav_st    = webdav_state_get_ptr();
   webdav_cb_state_t *webdav_cb_st = (webdav_cb_state_t*)calloc(1, sizeof(webdav_cb_state_t));

   fill_pathname_join_special(url, webdav_st->url, path, sizeof(url));
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

   webdav_cb_st->cb        = cb;
   webdav_cb_st->user_data = user_data;
   strlcpy(webdav_cb_st->path, path, sizeof(webdav_cb_st->path));
   strlcpy(webdav_cb_st->file, file, sizeof(webdav_cb_st->file));

   RARCH_DBG("[webdav] GET %s\n", url_encoded);
   auth_header = webdav_get_auth_header("GET", url_encoded);
   t = task_push_http_transfer_with_headers(url_encoded, true, NULL,
         auth_header, webdav_read_cb, webdav_cb_st);
   free(auth_header);
   return (t != NULL);
}

/* WebDAV paths are case-sensitive, so this cannot use
 * string_list_find_elem(), which folds case. */
static bool webdav_dir_is_known(const char *url)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   size_t i;

   if (!webdav_st->dirs)
      return false;

   for (i = 0; i < webdav_st->dirs->size; i++)
      if (string_is_equal(webdav_st->dirs->elems[i].data, url))
         return true;

   return false;
}

static void webdav_dir_remember(const char *url)
{
   union string_list_elem_attr attr;
   webdav_state_t *webdav_st = webdav_state_get_ptr();

   if (!webdav_st->dirs)
      if (!(webdav_st->dirs = string_list_new()))
         return;

   if (webdav_dir_is_known(url))
      return;

   attr.i = 0;
   string_list_append(webdav_st->dirs, url, attr);
}

static void webdav_mkdir_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err);

/* Each upload walks the same directory prefixes and several uploads run
 * at once, so ask the server only for the collections this sync has not
 * created yet.  Task callbacks are retired on the main thread, so the
 * list needs no lock. */
static void webdav_mkdir_push(webdav_mkdir_state_t *webdav_mkdir_st)
{
   char *auth_header;

   if (webdav_dir_is_known(webdav_mkdir_st->url))
   {
      http_transfer_data_t data;
      memset(&data, 0, sizeof(data));
      data.status = 200;
      webdav_mkdir_cb(NULL, &data, webdav_mkdir_st, NULL);
      return;
   }

   RARCH_DBG("[webdav] MKCOL %s\n", webdav_mkdir_st->url);
   auth_header = webdav_get_auth_header("MKCOL", webdav_mkdir_st->url);
   task_push_webdav_mkdir(webdav_mkdir_st->url, true, auth_header,
         webdav_mkdir_cb, webdav_mkdir_st);
   free(auth_header);
}

static void webdav_mkdir_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char *auth_header;
   webdav_mkdir_state_t *webdav_mkdir_st = (webdav_mkdir_state_t *)user_data;
   http_transfer_data_t *data            = (http_transfer_data_t*)task_data;
   webdav_state_t       *webdav_st       = webdav_state_get_ptr();

   if (!webdav_mkdir_st)
      return;

   if (webdav_needs_reauth(data))
   {
      RARCH_DBG("[webdav] MKCOL %s\n", webdav_mkdir_st->url);
      auth_header = webdav_get_auth_header("MKCOL", webdav_mkdir_st->url);
      task_push_webdav_mkdir(webdav_mkdir_st->url, true, auth_header, webdav_mkdir_cb, webdav_mkdir_st);
      free(auth_header);
      return;
   }

   /* HTTP 405 on MKCOL means it's already there (RFC 4918 9.3.1) -- but
    * only on a server that implements MKCOL at all.  A plain HTTP server
    * answers 405 because it has never heard of the method, and treating
    * that as success walks the whole sync through to a pile of failed
    * PUTs. */
   if (   !data
       || data->status < 200
       || (   data->status >= 400
           && !(data->status == 405 && webdav_st->dav_verified)))
   {
      if (data)
         webdav_log_http_failure(webdav_mkdir_st->url, data, err);
      else
         RARCH_WARN("[webdav] Could not mkdir %s\n", webdav_mkdir_st ? webdav_mkdir_st->url : "<unknown>");
      webdav_mkdir_st->cb(false, webdav_mkdir_st->cb_st);
      free(webdav_mkdir_st);
      return;
   }

   /* url is truncated to the prefix that just came back, except on the
    * synthetic call webdav_ensure_dir() starts the walk with, where the
    * full path is still intact. */
   if (webdav_mkdir_st->last_slash && !webdav_mkdir_st->last_slash[1])
      webdav_dir_remember(webdav_mkdir_st->url);

   webdav_mkdir_st->last_slash[1] = webdav_mkdir_st->post_slash;
   webdav_mkdir_st->last_slash = strchr(webdav_mkdir_st->last_slash + 1, '/');
   if (webdav_mkdir_st->last_slash)
   {
      webdav_mkdir_st->post_slash    = webdav_mkdir_st->last_slash[1];
      webdav_mkdir_st->last_slash[1] = '\0';
      webdav_mkdir_push(webdav_mkdir_st);
   }
   else
   {
      RARCH_DBG("[webdav] MKCOL %s success\n", webdav_mkdir_st->url);
      webdav_mkdir_st->cb(true, webdav_mkdir_st->cb_st);
      free(webdav_mkdir_st);
   }
}

static void webdav_ensure_dir(const char *dir, webdav_mkdir_cb_t cb,
      webdav_cb_state_t *webdav_cb_st)
{
   char url[PATH_MAX_LENGTH];
   http_transfer_data_t  data;
   webdav_state_t       *webdav_st       = webdav_state_get_ptr();
   webdav_mkdir_state_t *webdav_mkdir_st = (webdav_mkdir_state_t *)malloc(sizeof(webdav_mkdir_state_t));

   fill_pathname_join_special(url, webdav_st->url, dir, sizeof(url));
   net_http_urlencode_full(webdav_mkdir_st->url, url, sizeof(webdav_mkdir_st->url));
   webdav_mkdir_st->last_slash = strchr(webdav_mkdir_st->url + strlen(webdav_st->url) - 1, '/');
   webdav_mkdir_st->post_slash = webdav_mkdir_st->last_slash[1];
   webdav_mkdir_st->cb         = cb;
   webdav_mkdir_st->cb_st      = webdav_cb_st;

   /* this is a recursive callback, set it up so it looks like it's still proceeding */
   data.status = 200;
   webdav_mkdir_cb(NULL, &data, webdav_mkdir_st, NULL);
}

static void webdav_do_update(bool success, webdav_cb_state_t *webdav_cb_st);

static void webdav_update_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   webdav_cb_state_t    *webdav_cb_st = (webdav_cb_state_t *)user_data;
   http_transfer_data_t *data         = (http_transfer_data_t*)task_data;
   bool                  success      = (data && data->status >= 200 && data->status < 300);

   if (!success && data)
       webdav_log_http_failure(webdav_cb_st->path, data, err);
   else if (!data)
      RARCH_WARN("[webdav] Could not upload %s\n", webdav_cb_st ? webdav_cb_st->path : "<unknown>");

   if (webdav_needs_reauth(data))
   {
      webdav_do_update(true, webdav_cb_st);
      return;
   }

   if (webdav_cb_st)
   {
      webdav_cb_st->cb(webdav_cb_st->user_data, webdav_cb_st->path, success, webdav_cb_st->rfile);
      free(webdav_cb_st);
   }
   else
      RARCH_WARN("[webdav] Missing cb data in update?\n");
}

static void webdav_do_update(bool success, webdav_cb_state_t *webdav_cb_st)
{
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char            url_encoded[PATH_MAX_LENGTH];
   char            url[PATH_MAX_LENGTH];
   void           *buf;
   int64_t         len;
   char           *auth_header;

   if (!webdav_cb_st)
      return;

   if (!success)
   {
      RARCH_DBG("[webdav] Cannot upload %s\n", webdav_cb_st->path);
      webdav_cb_st->cb(webdav_cb_st->user_data, webdav_cb_st->path, false, webdav_cb_st->rfile);
      free(webdav_cb_st);
      return;
   }

   /* TODO: would be better to read file as it's being written to wire, this is very inefficient */
   len = filestream_get_size(webdav_cb_st->rfile);
   buf = (char*)malloc((size_t)(len + 1));
   filestream_read(webdav_cb_st->rfile, buf, len);

   fill_pathname_join_special(url, webdav_st->url, webdav_cb_st->path, sizeof(url));
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

   RARCH_DBG("[webdav] PUT %s\n", url_encoded);
   auth_header = webdav_get_auth_header("PUT", url_encoded);
   task_push_webdav_put(url_encoded, buf, len, true, auth_header, webdav_update_cb, webdav_cb_st);
   free(auth_header);

   free(buf);
}

static bool webdav_update(const char *path, RFILE *rfile,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char               dir[DIR_MAX_LENGTH];
   webdav_cb_state_t *webdav_cb_st = (webdav_cb_state_t*)calloc(1, sizeof(webdav_cb_state_t));

   /* TODO/FIXME: if !settings->bools.cloud_sync_destructive, should move to deleted/ first */

   webdav_cb_st->cb = cb;
   webdav_cb_st->user_data = user_data;
   strlcpy(webdav_cb_st->path, path, sizeof(webdav_cb_st->path));
   webdav_cb_st->rfile = rfile;

   if (strchr(path, '/'))
   {
      fill_pathname_basedir(dir, path, sizeof(dir));
      webdav_ensure_dir(dir, webdav_do_update, webdav_cb_st);
   }
   else
      webdav_do_update(true, webdav_cb_st);

   return true;
}

static void webdav_delete_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   webdav_cb_state_t    *webdav_cb_st = (webdav_cb_state_t *)user_data;
   http_transfer_data_t *data         = (http_transfer_data_t*)task_data;
   bool                  success      = (data != NULL && data->status >= 200 && data->status < 300);

   if (!success && data)
      webdav_log_http_failure(webdav_cb_st->path, data, err);
   else if (!data)
      RARCH_WARN("[webdav] Could not delete %s\n", webdav_cb_st ? webdav_cb_st->path : "<unknown>");

   if (webdav_needs_reauth(data))
   {
      webdav_state_t *webdav_st = webdav_state_get_ptr();
      char            url[PATH_MAX_LENGTH];
      char            url_encoded[PATH_MAX_LENGTH];
      char           *auth_header;

      fill_pathname_join_special(url, webdav_st->url, webdav_cb_st->path, sizeof(url));
      net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

      RARCH_DBG("[webdav] DELETE %s\n", url_encoded);
      auth_header = webdav_get_auth_header("DELETE", url_encoded);
      task_push_webdav_delete(url_encoded, true, auth_header, webdav_delete_cb, webdav_cb_st);
      free(auth_header);
      return;
   }

   if (webdav_cb_st)
   {
      webdav_cb_st->cb(webdav_cb_st->user_data, webdav_cb_st->path, success, NULL);
      free(webdav_cb_st);
   }
   else
      RARCH_WARN("[webdav] Missing cb data in delete?\n");
}

static void webdav_do_backup(bool success, webdav_cb_state_t *webdav_cb_st);

static void webdav_backup_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   webdav_cb_state_t    *webdav_cb_st = (webdav_cb_state_t *)user_data;
   http_transfer_data_t *data         = (http_transfer_data_t*)task_data;
   bool                  success      = (data != NULL && data->status >= 200 && data->status < 300);

   if (!success && data)
       webdav_log_http_failure(webdav_cb_st->path, data, err);
   else if (!data)
      RARCH_WARN("[webdav] Could not backup %s\n", webdav_cb_st ? webdav_cb_st->path : "<unknown>");

   if (webdav_needs_reauth(data))
   {
      webdav_do_backup(true, webdav_cb_st);
      return;
   }

   if (webdav_cb_st)
   {
      webdav_cb_st->cb(webdav_cb_st->user_data, webdav_cb_st->path, success, NULL);
      free(webdav_cb_st);
   }
   else
      RARCH_WARN("[webdav] Missing cb data in backup?\n");
}

static void webdav_do_backup(bool success, webdav_cb_state_t *webdav_cb_st)
{
   char *auth_header;
   size_t          len;
   struct tm       tm_;
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char            dest_encoded[PATH_MAX_LENGTH];
   char            dest[PATH_MAX_LENGTH];
   char            url_encoded[PATH_MAX_LENGTH];
   char            url[PATH_MAX_LENGTH];
   time_t          cur_time = time(NULL);

   if (!webdav_cb_st)
      return;

   if (!success)
   {
      RARCH_DBG("[webdav] Cannot backup/delete %s\n", webdav_cb_st->path);
      webdav_cb_st->cb(webdav_cb_st->user_data, webdav_cb_st->path, false, NULL);
      free(webdav_cb_st);
      return;
   }

   fill_pathname_join_special(url, webdav_st->url, webdav_cb_st->path, sizeof(url));
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

   fill_pathname_join_special(url, webdav_st->url, "deleted/", sizeof(url));
   len = fill_pathname_join_special(dest, url, webdav_cb_st->path, sizeof(dest));
   rtime_localtime(&cur_time, &tm_);
   strftime(dest + len, sizeof(dest) - len, "-%y%m%d-%H%M%S", &tm_);
   net_http_urlencode_full(dest_encoded, dest, sizeof(dest_encoded));

   RARCH_DBG("[webdav] MOVE %s -> %s\n", url_encoded, dest_encoded);
   auth_header = webdav_get_auth_header("MOVE", url_encoded);
   task_push_webdav_move(url_encoded, dest_encoded, true, auth_header, webdav_backup_cb, webdav_cb_st);
   free(auth_header);
}

static bool webdav_delete(const char *path, cloud_sync_complete_handler_t cb, void *user_data)
{
   webdav_cb_state_t *webdav_cb_st = (webdav_cb_state_t*)calloc(1, sizeof(webdav_cb_state_t));
   settings_t        *settings     = config_get_ptr();

   webdav_cb_st->cb        = cb;
   webdav_cb_st->user_data = user_data;
   strlcpy(webdav_cb_st->path, path, sizeof(webdav_cb_st->path));

   /*
    * Should all cloud_sync_destructive handling be done in task_cloudsync? I
    * think not because it gives each driver a chance to do a move rather than a
    * delete/update. Or we could add a cloud_sync_move() API to the driver.
    */
   if (settings->bools.cloud_sync_destructive)
   {
      char *auth_header;
      char url[PATH_MAX_LENGTH];
      char url_encoded[PATH_MAX_LENGTH];
      webdav_state_t *webdav_st = webdav_state_get_ptr();

      fill_pathname_join_special(url, webdav_st->url, path, sizeof(url));
      net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));

      RARCH_DBG("[webdav] DELETE %s\n", url_encoded);
      auth_header = webdav_get_auth_header("DELETE", url_encoded);
      task_push_webdav_delete(url_encoded, true, auth_header, webdav_delete_cb, webdav_cb_st);
      free(auth_header);
   }
   else
   {
      char dir[DIR_MAX_LENGTH];
      size_t _len = strlcpy_lit(dir, "deleted/", sizeof(dir));
      fill_pathname_basedir(dir + _len, path, sizeof(dir) - _len);
      webdav_ensure_dir(dir, webdav_do_backup, webdav_cb_st);
   }

   return true;
}

cloud_sync_driver_t cloud_sync_webdav = {
   webdav_sync_begin,
   webdav_sync_end,
   webdav_read,
   webdav_update,
   webdav_delete,
   "webdav" /* ident */
};

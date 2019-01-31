/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_uri.h"

#define TEST_CHAR       '.'
#define TEST_STRING     "test"
#define TEST_NAME       "name"
#define TEST_VALUE      "value"

#define ARRAY_SIZE(X)   (sizeof(X)/sizeof(*(X)))

struct
{
   const char *before;
   const char *after;
} test_parse_uris[] = {
   {"", NULL},
   {"C:\\test\\filename", NULL},
   {"/usr/local/nix/filename", NULL},
   {"scheme-only:", NULL},
   {"scheme:/and/path", NULL},
   {"scheme:/and/path#with_fragment", NULL},
   {"scheme:/and/path?query=true", NULL},
   {"scheme:/and/path?multiple+queries=true&more=false", NULL},
   {"scheme:/and/path?multiple+queries=true&more=false#and+a+fragment,+too", NULL},
   {"scheme:C:\\Windows\\path", "scheme:C:%5CWindows%5Cpath"},
   {"scheme:C:\\Windows\\path#with_fragment", "scheme:C:%5CWindows%5Cpath#with_fragment"},
   {"scheme:C:\\Windows\\path?query=true", "scheme:C:%5CWindows%5Cpath?query=true"},
   {"scheme:C:\\Windows\\path?query#and+fragment", "scheme:C:%5CWindows%5Cpath?query#and+fragment"},
   {"scheme://just.host", NULL},
   {"scheme://host/and/path", NULL},
   {"scheme://host:port", NULL},
   {"scheme://host:port/and/path", NULL},
   {"scheme://127.0.0.1:port/and/path", NULL},
   {"scheme://userinfo@host:port/and/path?query#fragment", NULL},
   {"HTTP://www.EXAMPLE.com/", "http://www.example.com/"},
   {"%48%54%54%50://%54%45%53%54/", "http://test/"},
   {"scheme:esc%", "scheme:esc%25"},
   {"scheme:esc%%", "scheme:esc%25%25"},
   {"scheme:esc%%%", "scheme:esc"},
   {"scheme:esc%1%", "scheme:esc%10"},
   {"scheme:esc%%1", "scheme:esc%01"},
   {"s+-.1234567890:", NULL},
   {"scheme:///", NULL},
   {"scheme://:just_port", NULL},
   {"scheme://:port/and/path", NULL},
   {"scheme://just_userinfo@", NULL},
   {"scheme://userinfo@/and/path", NULL},
   {"scheme://userinfo@:port/and/path", NULL},
   {"%01%02%03%04%05%06%07%08%09%0a%0b%0c%0d%0e%0f%10%11%12%13%14%15%16%17%18%19%1a%1b%1c%1d%1e%1f:", "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F:"},
   {"%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F:", "%20%21%22%23%24%25%26%27%28%29%2A+%2C-.%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E_%60%7B%7C%7D~%7F:"},
   {"scheme://%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F@", "scheme://%20!%22%23$%25&'()*+,-.%2F:;%3C=%3E%3F%40%5B%5C%5D%5E_%60%7B%7C%7D~%7F@"},
   {"scheme://%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F", "scheme://%20!%22%23$%25&'()*+,-.%2F:;%3C=%3E%3F%40[%5C]%5E_%60%7B%7C%7D~%7F"},
   {"scheme://:%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F", "scheme://:%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E_%60%7B%7C%7D~%7F"},
   {"scheme:///%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F", "scheme:///%20!%22%23$%25&'()*+,-./:;%3C=%3E%3F@%5B%5C%5D%5E_%60%7B%7C%7D~%7F"},
   {"scheme://?%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F", "scheme://?%20!%22%23$%25&'()*+,-./:;%3C=%3E?@%5B%5C%5D%5E_%60%7B%7C%7D~%7F"},
   {"scheme://#%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E%7F", "scheme://#%20!%22%23$%25&'()*+,-./:;%3C=%3E?@%5B%5C%5D%5E_%60%7B%7C%7D~%7F"},
   {"scheme://[v6.1234:5678]/", NULL},
   {"scheme://[::1]/", NULL},
   {"scheme://[1234:5678:90ab:cdef:1234:5678:90ab:cdef]/", NULL},
   {"scheme://[::1]:1/", NULL},
   {"scheme://?", "scheme://"},
   {"scheme://?#", "scheme://#"},
   {"scheme://?q&&;&n=&=v&=&n=v", "scheme://?q&n=&n=v"},
   {"ldap://[2001:db8::7]/c=GB?objectClass?one", NULL},
   {"mailto:John.Doe@example.com", NULL},
   {"news:comp.infosystems.www.servers.unix", NULL},
   {"tel:+1-816-555-1212", NULL},
   {"urn:oasis:names:specification:docbook:dtd:xml:4.1.2", NULL},
};

typedef struct build_uri_tag
{
   const char *expected_uri;
   const char *scheme;
   const char *userinfo;
   const char *host;
   const char *port;
   const char *path;
   const char *fragment;
   const char **queries; /* Held as name then value. NULL values allowed. NULL name terminates list */
} BUILD_URI_T;

/* Test query lists */
const char *no_queries[] = { NULL };
const char *query_true[] = { "query", "true", NULL };
const char *multiple_queries[] = { "multiple+queries", "true", "more", "false", NULL };
const char *just_query[] = {"query", NULL, NULL};
const char *complex_query[] = { "q", NULL, "n", "", "n", "v", NULL };
const char *objectClass_one[] = { "objectClass?one", NULL, NULL };

BUILD_URI_T test_build_uris[] = {
   {"", NULL, NULL, NULL, NULL, NULL, NULL, no_queries },
   {"C:\\test\\filename", NULL, NULL, NULL, NULL, "C:\\test\\filename", NULL, no_queries},
   {"/usr/local/nix/filename", NULL, NULL, NULL, NULL, "/usr/local/nix/filename", NULL, no_queries},
   {"scheme-only:", "scheme-only", NULL, NULL, NULL, NULL, NULL, no_queries},
   {"scheme:/and/path", "scheme", NULL, NULL, NULL, "/and/path", NULL, no_queries},
   {"scheme:/and/path#with_fragment", "scheme", NULL, NULL, NULL, "/and/path", "with_fragment", no_queries},
   {"scheme:/and/path?query=true", "scheme", NULL, NULL, NULL, "/and/path", NULL, query_true},
   {"scheme:/and/path?multiple+queries=true&more=false", "scheme", NULL, NULL, NULL, "/and/path", NULL, multiple_queries},
   {"scheme:/and/path?multiple+queries=true&more=false#and+a+fragment,+too", "scheme", NULL, NULL, NULL, "/and/path", "and+a+fragment,+too", multiple_queries},
   {"scheme://just.host", "scheme", NULL, "just.host", NULL, NULL, NULL, no_queries},
   {"scheme://host/and/path", "scheme", NULL, "host", NULL, "/and/path", NULL, no_queries},
   {"scheme://host:port", "scheme", NULL, "host", "port", NULL, NULL, no_queries},
   {"scheme://host:port/and/path", "scheme", NULL, "host", "port", "/and/path", NULL, no_queries},
   {"scheme://127.0.0.1:port/and/path", "scheme", NULL, "127.0.0.1", "port", "/and/path", NULL, no_queries},
   {"scheme://userinfo@host:port/and/path?query#fragment", "scheme", "userinfo", "host", "port", "/and/path", "fragment", just_query},
   {"scheme:///", "scheme", NULL, "", NULL, "/", NULL, no_queries },
   {"scheme://:just_port", "scheme", NULL, "", "just_port", NULL, NULL, no_queries },
   {"scheme://:port/and/path", "scheme", NULL, "", "port", "/and/path", NULL, no_queries },
   {"scheme://just_userinfo@", "scheme", "just_userinfo", "", NULL, NULL, NULL, no_queries },
   {"scheme://userinfo@/and/path", "scheme", "userinfo", "", NULL, "/and/path", NULL, no_queries },
   {"scheme://userinfo@:port/and/path", "scheme", "userinfo", "", "port", "/and/path", NULL, no_queries },
   {"scheme://", "scheme", NULL, "", NULL, NULL, NULL, no_queries },
   {"scheme://#", "scheme", NULL, "", NULL, NULL, "", no_queries },
   {"scheme://?q&n=&n=v", "scheme", NULL, "", NULL, NULL, NULL, complex_query},
   {"ldap://[2001:db8::7]/c=GB?objectClass?one", "ldap", NULL, "[2001:db8::7]", NULL, "/c=GB", NULL, objectClass_one},
   {"mailto:John.Doe@example.com", "mailto", NULL, NULL, NULL, "John.Doe@example.com", NULL, no_queries },
   {"news:comp.infosystems.www.servers.unix", "news", NULL, NULL, NULL, "comp.infosystems.www.servers.unix", NULL, no_queries },
   {"tel:+1-816-555-1212", "tel", NULL, NULL, NULL, "+1-816-555-1212", NULL, no_queries },
   {"urn:oasis:names:specification:docbook:dtd:xml:4.1.2", "urn", NULL, NULL, NULL, "oasis:names:specification:docbook:dtd:xml:4.1.2", NULL, no_queries },
};

typedef struct merge_uri_tag
{
   const char *base;
   const char *relative;
   const char *merged;
} MERGE_URI_T;

MERGE_URI_T test_merge_uris[] = {
   /* Normal examples */
   { "http://a/b/c/d;p?q#f", "ftp:h",     "ftp:h" },
   { "http://a/b/c/d;p?q#f", "g",         "http://a/b/c/g" },
   { "http://a/b/c/d;p?q#f", "./g",       "http://a/b/c/g" },
   { "http://a/b/c/d;p?q#f", "g/",        "http://a/b/c/g/" },
   { "http://a/b/c/d;p?q#f", "/g",        "http://a/g" },
   { "http://a/b/c/d;p?q#f", "//g",       "http://g" },
   { "http://a/b/c/d;p?q#f", "?y",        "http://a/b/c/d;p?y" },
   { "http://a/b/c/d;p?q#f", "g?y",       "http://a/b/c/g?y" },
   { "http://a/b/c/d;p?q#f", "g?y/./x",   "http://a/b/c/g?y/./x" },
   { "http://a/b/c/d;p?q#f", "#s",        "http://a/b/c/d;p?q#s" },
   { "http://a/b/c/d;p?q#f", "g#s",       "http://a/b/c/g#s" },
   { "http://a/b/c/d;p?q#f", "g#s/./x",   "http://a/b/c/g#s/./x" },
   { "http://a/b/c/d;p?q#f", "g?y#s",     "http://a/b/c/g?y#s" },
   { "http://a/b/c/d;p?q#f", ";x",        "http://a/b/c/d;x" },
   { "http://a/b/c/d;p?q#f", "g;x",       "http://a/b/c/g;x" },
   { "http://a/b/c/d;p?q#f", "g;x?y#s",   "http://a/b/c/g;x?y#s" },
   { "http://a/b/c/d;p?q#f", ".",         "http://a/b/c/" },
   { "http://a/b/c/d;p?q#f", "./",        "http://a/b/c/" },
   { "http://a/b/c/d;p?q#f", "..",        "http://a/b/" },
   { "http://a/b/c/d;p?q#f", "../",       "http://a/b/" },
   { "http://a/b/c/d;p?q#f", "../g",      "http://a/b/g" },
   { "http://a/b/c/d;p?q#f", "../..",     "http://a/" },
   { "http://a/b/c/d;p?q#f", "../../",    "http://a/" },
   { "http://a/b/c/d;p?q#f", "../../g",   "http://a/g" },
   /* Normal examples, without base network info */
   { "http:/b/c/d;p?q#f",    "g",         "http:/b/c/g" },
   { "http:/b/c/d;p?q#f",    "./g",       "http:/b/c/g" },
   { "http:/b/c/d;p?q#f",    "g/",        "http:/b/c/g/" },
   { "http:/b/c/d;p?q#f",    "/g",        "http:/g" },
   { "http:/b/c/d;p?q#f",    "//g",       "http://g" },
   { "http:/b/c/d;p?q#f",    "?y",        "http:/b/c/d;p?y" },
   { "http:/b/c/d;p?q#f",    "g?y",       "http:/b/c/g?y" },
   { "http:/b/c/d;p?q#f",    "g?y/./x",   "http:/b/c/g?y/./x" },
   { "http:/b/c/d;p?q#f",    "#s",        "http:/b/c/d;p?q#s" },
   { "http:/b/c/d;p?q#f",    "g#s",       "http:/b/c/g#s" },
   { "http:/b/c/d;p?q#f",    "g#s/./x",   "http:/b/c/g#s/./x" },
   { "http:/b/c/d;p?q#f",    "g?y#s",     "http:/b/c/g?y#s" },
   { "http:/b/c/d;p?q#f",    ";x",        "http:/b/c/d;x" },
   { "http:/b/c/d;p?q#f",    "g;x",       "http:/b/c/g;x" },
   { "http:/b/c/d;p?q#f",    "g;x?y#s",   "http:/b/c/g;x?y#s" },
   { "http:/b/c/d;p?q#f",    ".",         "http:/b/c/" },
   { "http:/b/c/d;p?q#f",    "./",        "http:/b/c/" },
   { "http:/b/c/d;p?q#f",    "..",        "http:/b/" },
   { "http:/b/c/d;p?q#f",    "../",       "http:/b/" },
   { "http:/b/c/d;p?q#f",    "../g",      "http:/b/g" },
   { "http:/b/c/d;p?q#f",    "../..",     "http:/" },
   { "http:/b/c/d;p?q#f",    "../../",    "http:/" },
   { "http:/b/c/d;p?q#f",    "../../g",   "http:/g" },
   /* Normal examples, without base network info or path root */
   { "http:b/c/d;p?q#f",     "g",         "http:b/c/g" },
   { "http:b/c/d;p?q#f",     "./g",       "http:b/c/g" },
   { "http:b/c/d;p?q#f",     "g/",        "http:b/c/g/" },
   { "http:b/c/d;p?q#f",     "/g",        "http:/g" },
   { "http:b/c/d;p?q#f",     "//g",       "http://g" },
   { "http:b/c/d;p?q#f",     "?y",        "http:b/c/d;p?y" },
   { "http:b/c/d;p?q#f",     "g?y",       "http:b/c/g?y" },
   { "http:b/c/d;p?q#f",     "g?y/./x",   "http:b/c/g?y/./x" },
   { "http:b/c/d;p?q#f",     "#s",        "http:b/c/d;p?q#s" },
   { "http:b/c/d;p?q#f",     "g#s",       "http:b/c/g#s" },
   { "http:b/c/d;p?q#f",     "g#s/./x",   "http:b/c/g#s/./x" },
   { "http:b/c/d;p?q#f",     "g?y#s",     "http:b/c/g?y#s" },
   { "http:b/c/d;p?q#f",     ";x",        "http:b/c/d;x" },
   { "http:b/c/d;p?q#f",     "g;x",       "http:b/c/g;x" },
   { "http:b/c/d;p?q#f",     "g;x?y#s",   "http:b/c/g;x?y#s" },
   { "http:b/c/d;p?q#f",     ".",         "http:b/c/" },
   { "http:b/c/d;p?q#f",     "./",        "http:b/c/" },
   { "http:b/c/d;p?q#f",     "..",        "http:b/" },
   { "http:b/c/d;p?q#f",     "../",       "http:b/" },
   { "http:b/c/d;p?q#f",     "../g",      "http:b/g" },
   { "http:b/c/d;p?q#f",     "../..",     "http:" },
   { "http:b/c/d;p?q#f",     "../../",    "http:" },
   { "http:b/c/d;p?q#f",     "../../g",   "http:g" },
   /* Normal examples, without base path */
   { "http://a?q#f",         "g",         "http://a/g" },
   { "http://a?q#f",         "./g",       "http://a/g" },
   { "http://a?q#f",         "g/",        "http://a/g/" },
   { "http://a?q#f",         "/g",        "http://a/g" },
   { "http://a?q#f",         "//g",       "http://g" },
   { "http://a?q#f",         "?y",        "http://a?y" },
   { "http://a?q#f",         "g?y",       "http://a/g?y" },
   { "http://a?q#f",         "g?y/./x",   "http://a/g?y/./x" },
   { "http://a?q#f",         "#s",        "http://a?q#s" },
   { "http://a?q#f",         "g#s",       "http://a/g#s" },
   { "http://a?q#f",         "g#s/./x",   "http://a/g#s/./x" },
   { "http://a?q#f",         "g?y#s",     "http://a/g?y#s" },
   { "http://a?q#f",         ";x",        "http://a/;x" },
   { "http://a?q#f",         "g;x",       "http://a/g;x" },
   { "http://a?q#f",         "g;x?y#s",   "http://a/g;x?y#s" },
   { "http://a?q#f",         ".",         "http://a/" },
   { "http://a?q#f",         "./",        "http://a/" },
   /* Normal examples, without base network info or path */
   { "http:?q#f",            "g",         "http:g" },
   { "http:?q#f",            "./g",       "http:g" },
   { "http:?q#f",            "g/",        "http:g/" },
   { "http:?q#f",            "/g",        "http:/g" },
   { "http:?q#f",            "//g",       "http://g" },
   { "http:?q#f",            "?y",        "http:?y" },
   { "http:?q#f",            "g?y",       "http:g?y" },
   { "http:?q#f",            "g?y/./x",   "http:g?y/./x" },
   { "http:?q#f",            "#s",        "http:?q#s" },
   { "http:?q#f",            "g#s",       "http:g#s" },
   { "http:?q#f",            "g#s/./x",   "http:g#s/./x" },
   { "http:?q#f",            "g?y#s",     "http:g?y#s" },
   { "http:?q#f",            ";x",        "http:;x" },
   { "http:?q#f",            "g;x",       "http:g;x" },
   { "http:?q#f",            "g;x?y#s",   "http:g;x?y#s" },
   /* Abnormal (but valid) examples */
   { "http://a/b/c/d;p?q#f", "../../../g", "http://a/../g" },
   { "http://a/b/c/d;p?q#f", "../../../../g", "http://a/../../g" },
   { "http://a/b/c/d;p?q#f", "/./g",      "http://a/./g" },
   { "http://a/b/c/d;p?q#f", "/../g",     "http://a/../g" },
   { "http://a/b/c/d;p?q#f", "g.",        "http://a/b/c/g." },
   { "http://a/b/c/d;p?q#f", ".g",        "http://a/b/c/.g" },
   { "http://a/b/c/d;p?q#f", "g..",       "http://a/b/c/g.." },
   { "http://a/b/c/d;p?q#f", "..g",       "http://a/b/c/..g" },
   { "http://a/b/c/d;p?q#f", "./../g",    "http://a/b/g" },
   { "http://a/b/c/d;p?q#f", "./g/.",     "http://a/b/c/g/" },
   { "http://a/b/c/d;p?q#f", "g/./h",     "http://a/b/c/g/h" },
   { "http://a/b/c/d;p?q#f", "g/../h",    "http://a/b/c/h" },
   { "http://a/b/c/d;p?q#f", "./g:h",     "http://a/b/c/g:h" },
   { "http://a/b/c/d;p?q#f", "g/..",      "http://a/b/c/" },
   /* Abnormal examples without base path */
   { "http://a?q#f",         "../g",      "http://a/../g" },
   { "http://a?q#f",         "./../g",    "http://a/../g" },
   /* Abnormal examples without base network info or path */
   { "http:?q#f",            "../g",      "http:../g" },
   { "http:?q#f",            "./../g",    "http:../g" },
   { "http:?q#f",            ".",         "http:" },
   { "http:?q#f",            "./",        "http:" },
};


/** Dump a URI structure to the log.
 *
 * \param uri URI structure to be dumped. */
static void dump_uri(VC_URI_PARTS_T *uri)
{
   const char *str;
   uint32_t query_count, ii;

   str = vc_uri_scheme(uri);
   if (str)
      LOG_DEBUG(NULL, "Scheme: <%s>", str);

   str = vc_uri_userinfo(uri);
   if (str)
      LOG_DEBUG(NULL, "Userinfo: <%s>", str);

   str = vc_uri_host(uri);
   if (str)
      LOG_DEBUG(NULL, "Host: <%s>", str);

   str = vc_uri_port(uri);
   if (str)
      LOG_DEBUG(NULL, "Port: <%s>", str);

   str = vc_uri_path(uri);
   if (str)
      LOG_DEBUG(NULL, "Path: <%s>", str);

   query_count = vc_uri_num_queries(uri);
   for (ii = 0; ii < query_count; ii++)
   {
      const char *value;

      vc_uri_query(uri, ii, &str, &value);
      if (str)
      {
         if (value)
            LOG_DEBUG(NULL, "Query %d: <%s>=<%s>", ii, str, value);
         else
            LOG_DEBUG(NULL, "Query %d: <%s>", ii, str);
      }
   }

   str = vc_uri_fragment(uri);
   if (str)
      LOG_DEBUG(NULL, "Fragment: <%s>", str);
}

static int check_uri(VC_URI_PARTS_T *uri, const char *expected)
{
   uint32_t built_len;
   char *built;

   built_len = vc_uri_build(uri, NULL, 0) + 1;
   built = (char *)malloc(built_len);
   if (!built)
   {
      LOG_ERROR(NULL, "*** Unexpected memory allocation failure: %d bytes", built_len);
      return 1;
   }

   vc_uri_build(uri, built, built_len);

   if (strcmp(built, expected) != 0)
   {
      LOG_ERROR(NULL, "*** Built URI <%s>\nexpected  <%s>", built, expected);
      free(built);
      return 1;
   }

   free(built);

   return 0;
}

static int check_null_uri_pointer(void)
{
   int error_count = 0;
   char buffer[1];

   /* Check NULL URI can be passed without failure to all routines */
   vc_uri_release( NULL );
   vc_uri_clear( NULL );
   if (vc_uri_parse( NULL, NULL ))
      error_count++;
   if (vc_uri_parse( NULL, "" ))
      error_count++;
   if (vc_uri_build( NULL, NULL, 0 ) != 0)
      error_count++;
   buffer[0] = TEST_CHAR;
   if (vc_uri_build( NULL, buffer, sizeof(buffer) ) != 0)
      error_count++;
   if (buffer[0] != TEST_CHAR)
      error_count++;
   if (vc_uri_scheme( NULL ))
      error_count++;
   if (vc_uri_userinfo( NULL ))
      error_count++;
   if (vc_uri_host( NULL ))
      error_count++;
   if (vc_uri_port( NULL ))
      error_count++;
   if (vc_uri_path( NULL ))
      error_count++;
   if (vc_uri_fragment( NULL ))
      error_count++;
   if (vc_uri_num_queries( NULL ) != 0)
      error_count++;
   vc_uri_query( NULL, 0, NULL, NULL );
   if (vc_uri_set_scheme( NULL, NULL ))
      error_count++;
   if (vc_uri_set_userinfo( NULL, NULL ))
      error_count++;
   if (vc_uri_set_host( NULL, NULL ))
      error_count++;
   if (vc_uri_set_port( NULL, NULL ))
      error_count++;
   if (vc_uri_set_path( NULL, NULL ))
      error_count++;
   if (vc_uri_set_fragment( NULL, NULL ))
      error_count++;
   if (vc_uri_add_query( NULL, NULL, NULL ))
      error_count++;

   if (error_count)
      LOG_ERROR(NULL, "NULL URI parameter testing failed");

   return error_count;
}

static int check_parse_parameters(VC_URI_PARTS_T *uri)
{
   int error_count = 0;

   if (vc_uri_parse( uri, NULL ))
   {
      LOG_ERROR(NULL, "Parsing NULL URI failed");
      error_count++;
   }

   return error_count;
}

static int check_build_parameters(VC_URI_PARTS_T *uri)
{
   int error_count = 0;
   char buffer[1];

   vc_uri_set_path( uri, TEST_STRING );

   if (vc_uri_build( uri, NULL, 0 ) != strlen(TEST_STRING))
   {
      LOG_ERROR(NULL, "Retrieving URI length failed");
      error_count++;
   }

   buffer[0] = TEST_CHAR;
   if (vc_uri_build( uri, buffer, 1 ) != strlen(TEST_STRING))
   {
      LOG_ERROR(NULL, "Building URI to small buffer failed");
      error_count++;
   }
   if (buffer[0] != TEST_CHAR)
   {
      LOG_ERROR(NULL, "Building URI to small buffer modified buffer");
      error_count++;
   }

   vc_uri_set_path( uri, NULL );    /* Reset uri */

   return error_count;
}

static int check_get_defaults(VC_URI_PARTS_T *uri)
{
   int error_count = 0;
   const char *name = NULL, *value = NULL;
   char buffer[1];

   if (vc_uri_scheme( uri ))
      error_count++;
   if (vc_uri_userinfo( uri ))
      error_count++;
   if (vc_uri_host( uri ))
      error_count++;
   if (vc_uri_port( uri ))
      error_count++;
   if (vc_uri_path( uri ))
      error_count++;
   if (vc_uri_fragment( uri ))
      error_count++;
   if (vc_uri_num_queries( uri ) != 0)
      error_count++;

   vc_uri_query( uri, 0, &name, &value );
   if (name != NULL || value != NULL)
      error_count++;

   if (vc_uri_build(uri, NULL, 0) != 0)
      error_count++;
   buffer[0] = ~*TEST_STRING;    /* Initialize with something */
   vc_uri_build(uri, buffer, sizeof(buffer));
   if (buffer[0] != '\0')        /* Expect empty string */
      error_count++;

   if (error_count)
      LOG_ERROR(NULL, "Getting default values gave unexpected values");

   return error_count;
}

static int check_accessors(VC_URI_PARTS_T *uri)
{
   int error_count = 0;
   const char *str;

   if (vc_uri_set_scheme( uri, TEST_STRING ))
   {
      str = vc_uri_scheme(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_scheme( uri, NULL ))
         error_count++;
      if (vc_uri_scheme(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_set_userinfo( uri, TEST_STRING ))
   {
      str = vc_uri_userinfo(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_userinfo( uri, NULL ))
         error_count++;
      if (vc_uri_userinfo(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_set_host( uri, TEST_STRING ))
   {
      str = vc_uri_host(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_host( uri, NULL ))
         error_count++;
      if (vc_uri_host(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_set_port( uri, TEST_STRING ))
   {
      str = vc_uri_port(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_port( uri, NULL ))
         error_count++;
      if (vc_uri_port(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_set_path( uri, TEST_STRING ))
   {
      str = vc_uri_path(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_path( uri, NULL ))
         error_count++;
      if (vc_uri_path(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_set_fragment( uri, TEST_STRING ))
   {
      str = vc_uri_fragment(uri);
      if (!str || strcmp(TEST_STRING, str))
         error_count++;
      if (!vc_uri_set_fragment( uri, NULL ))
         error_count++;
      if (vc_uri_fragment(uri))
         error_count++;
   } else
      error_count++;

   if (vc_uri_add_query( uri, NULL, NULL ))
      error_count++;
   if (vc_uri_add_query( uri, NULL, TEST_VALUE ))
      error_count++;
   if (!vc_uri_add_query( uri, TEST_STRING, NULL ))
      error_count++;
   if (!vc_uri_add_query( uri, TEST_NAME, TEST_VALUE ))
      error_count++;

   if (vc_uri_num_queries(uri) == 2)
   {
      const char *name = NULL, *value = NULL;

      vc_uri_query(uri, 0, &name, &value);
      if (!name || strcmp(TEST_STRING, name))
         error_count++;
      if (value)
         error_count++;

      vc_uri_query(uri, 1, &name, &value);
      if (!name || strcmp(TEST_NAME, name))
         error_count++;
      if (!value || strcmp(TEST_VALUE, value))
         error_count++;
   } else
      error_count++;

   if (error_count)
      LOG_ERROR(NULL, "Accessors failed");

   return error_count;
}

/** Test parameter validation
 *
 * \param uri Pre-created URI structure.
 * \return 1 on error, 0 on success. */
static int test_parameter_validation(VC_URI_PARTS_T *uri)
{
   int error_count = 0;

   error_count += check_null_uri_pointer();
   error_count += check_get_defaults(uri);
   error_count += check_parse_parameters(uri);
   error_count += check_build_parameters(uri);
   error_count += check_accessors(uri);

   return error_count;
}

/** Test parsing and rebuilding a URI.
 *
 * \param uri Pre-created URI structure.
 * \param original The original URI string.
 * \param expected The expected, re-built string, or NULL if original is expected.
 * \return 1 on error, 0 on success. */
static int test_parsing_uri(VC_URI_PARTS_T *uri, const char *original, const char *expected)
{
   bool parsed;

   LOG_INFO(NULL, "URI: <%s>", original);

   parsed = vc_uri_parse( uri, original );
   if (!parsed)
   {
      LOG_ERROR(NULL, "*** Expected <%s> to parse, but it didn't", original);
      return 1;
   }

   dump_uri(uri);

   return check_uri(uri, expected ? expected : original);
}

/** Test building a URI from component parts.
 *
 * \param uri Pre-created URI structure.
 * \param uri_data The data for building the URI and the expected output.
 * \return 1 on error, 0 on success. */
static int test_building_uri(VC_URI_PARTS_T *uri, BUILD_URI_T *uri_data)
{
   const char **p_str;
   const char *name, *value;

   LOG_INFO(NULL, "Building URI <%s>", uri_data->expected_uri);

   vc_uri_clear(uri);

   if (!vc_uri_set_scheme(uri, uri_data->scheme))
   {
      LOG_ERROR(NULL, "*** Failed to set scheme");
      return 1;
   }

   if (!vc_uri_set_userinfo(uri, uri_data->userinfo))
   {
      LOG_ERROR(NULL, "*** Failed to set userinfo");
      return 1;
   }

   if (!vc_uri_set_host(uri, uri_data->host))
   {
      LOG_ERROR(NULL, "*** Failed to set host");
      return 1;
   }

   if (!vc_uri_set_port(uri, uri_data->port))
   {
      LOG_ERROR(NULL, "*** Failed to set port");
      return 1;
   }

   if (!vc_uri_set_path(uri, uri_data->path))
   {
      LOG_ERROR(NULL, "*** Failed to set path");
      return 1;
   }

   if (!vc_uri_set_fragment(uri, uri_data->fragment))
   {
      LOG_ERROR(NULL, "*** Failed to set fragment");
      return 1;
   }

   p_str = uri_data->queries;
   name = *p_str++;

   while (name)
   {
      value = *p_str++;
      if (!vc_uri_add_query(uri, name, value))
      {
         LOG_ERROR(NULL, "*** Failed to add query");
         return 1;
      }
      name = *p_str++;
   }

   dump_uri(uri);

   return check_uri(uri, uri_data->expected_uri);
}

/** Test merging a relative URI with a base URI.
 *
 * \param uri Pre-created URI structure.
 * \param uri_data The nase and relative URIs and the expected output.
 * \return 1 on error, 0 on success. */
static int test_merging_uri(VC_URI_PARTS_T *uri, MERGE_URI_T *uri_data)
{
   VC_URI_PARTS_T *base_uri;

   LOG_INFO(NULL, "Base <%s>, relative <%s>, expect <%s>", uri_data->base, uri_data->relative, uri_data->merged);

   vc_uri_clear(uri);
   base_uri = vc_uri_create();
   if (!base_uri)
   {
      LOG_ERROR(NULL, "*** Failed to allocate base URI structure");
      return 1;
   }

   if (!vc_uri_parse(base_uri, uri_data->base))
   {
      LOG_ERROR(NULL, "*** Failed to parse base URI structure");
      return 1;
   }
   if (!vc_uri_parse(uri, uri_data->relative))
   {
      LOG_ERROR(NULL, "*** Failed to parse relative URI structure");
      return 1;
   }

   if (!vc_uri_merge(base_uri, uri))
   {
      LOG_ERROR(NULL, "*** Failed to merge base and relative URIs");
      return 1;
   }

   vc_uri_release(base_uri);

   return check_uri(uri, uri_data->merged);
}

int main(int argc, char **argv)
{
   VC_URI_PARTS_T *uri;
   int error_count = 0;
   size_t ii;

   uri = vc_uri_create();
   if (!uri)
   {
      LOG_ERROR(NULL, "*** Failed to create URI structure.");
      return 1;
   }

   LOG_INFO(NULL, "Test parameter validation");
   error_count +=  test_parameter_validation(uri);

   LOG_INFO(NULL, "Test parsing URIs:");
   for (ii = 0; ii < ARRAY_SIZE(test_parse_uris); ii++)
   {
      error_count += test_parsing_uri(uri, test_parse_uris[ii].before, test_parse_uris[ii].after);
   }

   LOG_INFO(NULL, "Test building URIs:");
   for (ii = 0; ii < ARRAY_SIZE(test_build_uris); ii++)
   {
      error_count += test_building_uri(uri, &test_build_uris[ii]);
   }

   LOG_INFO(NULL, "Test merging URIs:");
   for (ii = 0; ii < ARRAY_SIZE(test_merge_uris); ii++)
   {
      error_count += test_merging_uri(uri, &test_merge_uris[ii]);
   }

   if (argc > 1)
   {
      LOG_INFO(NULL, "Test parsing URIs from command line:");

      while (argc-- > 1)
      {
         /* Test URIs passed on the command line are expected to parse and to
          * match themselves when rebuilt. */
         error_count += test_parsing_uri(uri, argv[argc], NULL);
      }
   }

   vc_uri_release(uri);

   if (error_count)
      LOG_ERROR(NULL, "*** %d errors reported", error_count);

#ifdef _MSC_VER
   LOG_INFO(NULL, "Press return to complete test.");
   getchar();
#endif

   return error_count;
}

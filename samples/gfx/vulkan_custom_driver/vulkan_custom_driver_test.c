/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include <lists/string_list.h>

#include "../../../gfx/common/vulkan_custom_driver.h"

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* meta.json of two real driver packages, byte for byte */
static const char t30_meta[] =
   "{\n"
   "  \"schemaVersion\": 1,\n"
   "  \"name\": \"Turnip Adreno Driver T30 (@Mr_Purple_666)\",\n"
   "  \"description\": \"Open-source Vulkan driver build, MrPurple still alive. Big thanks to DISKDVD\",\n"
   "  \"author\": \"Mr_Purple_666\",\n"
   "  \"packageVersion\": \"T30\",\n"
   "  \"vendor\": \"Mesa\",\n"
   "  \"driverVersion\": \"26.3.0-T30-1.4.359\",\n"
   "  \"minApi\": 30,\n"
   "  \"libraryName\": \"vulkan.purple.so\"\n"
   "}";
static const char k11_meta[] =
   "{\n"
   "  \"schemaVersion\": 1,\n"
   "  \"name\": \"Mesa Turnip driver v26.0.0 - R8\",\n"
   "  \"description\": \"Compiled from Source + unsupported gpu hacks\",\n"
   "  \"author\": \"KIMCHI\",\n"
   "  \"packageVersion\": \"1\",\n"
   "  \"vendor\": \"Mesa\",\n"
   "  \"driverVersion\": \"Vulkan 1.4.335\",\n"
   "  \"minApi\": 27,\n"
   "  \"libraryName\": \"vulkan.ad07xx.so\"\n"
   "}";

/* The first 20 bytes of vulkan.purple.so */
static const unsigned char t30_elf[VULKAN_CUSTOM_DRIVER_ELF_HEAD] = {
   0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0xb7, 0x00 };

static char json[VULKAN_CUSTOM_DRIVER_META_MAX * 2];

/* A meta.json with these raw JSON values; NULL leaves the key out. */
static const char *meta_of(const char *schema, const char *vendor,
      const char *library, const char *min_api)
{
   strcpy(json, "{\"name\": \"test\"");
   if (schema)
   {
      strcat(json, ", \"schemaVersion\": ");
      strcat(json, schema);
   }
   if (vendor)
   {
      strcat(json, ", \"vendor\": ");
      strcat(json, vendor);
   }
   if (library)
   {
      strcat(json, ", \"libraryName\": ");
      strcat(json, library);
   }
   if (min_api)
   {
      strcat(json, ", \"minApi\": ");
      strcat(json, min_api);
   }
   strcat(json, "}");
   return json;
}

static const char *parse(const char *buf, int api_level)
{
   struct vulkan_custom_driver_meta meta;
   return vulkan_custom_driver_parse_meta(buf, strlen(buf), api_level,
         &meta);
}

static void test_meta(void)
{
   struct vulkan_custom_driver_meta meta;
   char name[VULKAN_CUSTOM_DRIVER_LIBRARY_MAX + 8];
   size_t pad;

   CHECK(!vulkan_custom_driver_parse_meta(t30_meta, sizeof(t30_meta) - 1,
         33, &meta));
   CHECK(!strcmp(meta.library_name, "vulkan.purple.so"));
   CHECK(!strcmp(meta.vendor, "Mesa") && meta.mesa);
   CHECK(!strcmp(meta.name, "Turnip Adreno Driver T30 (@Mr_Purple_666)"));
   CHECK(!strcmp(meta.version, "26.3.0-T30-1.4.359"));
   CHECK(!vulkan_custom_driver_parse_meta(k11_meta, sizeof(k11_meta) - 1,
         28, &meta));
   CHECK(!strcmp(meta.library_name, "vulkan.ad07xx.so"));
   CHECK(!strcmp(meta.version, "Vulkan 1.4.335"));
   /* minApi is checked against the device */
   CHECK(!parse(t30_meta, 30));
   CHECK(parse(t30_meta, 29) != NULL);
   CHECK(!parse(meta_of("1", "\"Mesa\"", "\"v.so\"", NULL), 28));

   CHECK(parse(meta_of("1", "\"Mesa\"", NULL, NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"a/b.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"a\\\\b.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"a\\u0000.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"..so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\".so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"x.txt\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "7", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", NULL, "\"v.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("2", "\"Mesa\"", "\"v.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("\"1\"", "\"Mesa\"", "\"v.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of(NULL, "\"Mesa\"", "\"v.so\"", NULL), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"v.so\"", "34"), 33) != NULL);
   CHECK(parse(meta_of("1", "\"Mesa\"", "\"v.so\"", "\"30\""), 33) != NULL);
   CHECK(parse("[1]", 33) != NULL);
   CHECK(parse("", 33) != NULL);
   /* unknown keys of any type are ignored */
   CHECK(!parse("{\"schemaVersion\": 1, \"vendor\": \"Mesa\", \"x\": "
         "{\"libraryName\": 5}, \"y\": [1, null, true], "
         "\"libraryName\": \"v.so\"}", 33));

   /* 63 characters pass, 64 do not */
   memset(name, 'a', sizeof(name));
   name[0] = '"';
   strcpy(name + 61, ".so\"");
   CHECK(!parse(meta_of("1", "\"Mesa\"", name, NULL), 33));
   name[0] = 'a';
   memmove(name + 1, name, strlen(name) + 1);
   name[0] = '"';
   CHECK(parse(meta_of("1", "\"Mesa\"", name, NULL), 33) != NULL);

   /* truncated */
   CHECK(vulkan_custom_driver_parse_meta(t30_meta, sizeof(t30_meta) - 3,
         33, &meta) != NULL);

   /* 4 KiB pass, one byte more does not */
   pad = VULKAN_CUSTOM_DRIVER_META_MAX - (sizeof(k11_meta) - 1);
   json[0] = '{';
   memset(json + 1, ' ', pad);
   strcpy(json + 1 + pad, k11_meta + 1);
   CHECK(strlen(json) == VULKAN_CUSTOM_DRIVER_META_MAX);
   CHECK(!parse(json, 33));
   memmove(json + 1, json, strlen(json) + 1);
   json[1] = ' ';
   CHECK(parse(json, 33) != NULL);
}

/* {schemaVersion 1, vendor, libraryName "v.so", <rest>} */
static const char *meta_rest(const char *vendor, const char *rest,
      struct vulkan_custom_driver_meta *meta)
{
   strcpy(json, "{\"schemaVersion\": 1, \"vendor\": ");
   strcat(json, vendor);
   strcat(json, ", \"libraryName\": \"v.so\"");
   if (rest)
   {
      strcat(json, ", ");
      strcat(json, rest);
   }
   strcat(json, "}");
   return vulkan_custom_driver_parse_meta(json, strlen(json), 33, meta);
}

/* "<key>": "<n times c><tail>", or the string alone for a NULL key */
static const char *text_of(const char *key, size_t n, char c,
      const char *tail)
{
   static char text[VULKAN_CUSTOM_DRIVER_TEXT_MAX * 4];
   size_t len;
   text[0] = '\0';
   if (key)
   {
      strcpy(text, "\"");
      strcat(text, key);
      strcat(text, "\": ");
   }
   strcat(text, "\"");
   len = strlen(text);
   memset(text + len, c, n);
   strcpy(text + len + n, tail);
   strcat(text, "\"");
   return text;
}

/* Any vendor string passes; name and driverVersion never reject. */
static void test_meta_policy(void)
{
   struct vulkan_custom_driver_meta meta;
   char bom[sizeof(t30_meta) + 3];

   CHECK(!meta_rest("\"Qualcomm\"", NULL, &meta));
   CHECK(!strcmp(meta.vendor, "Qualcomm") && !meta.mesa);
   CHECK(!*meta.name && !*meta.version);
   /* Mesa in any case, and only Mesa */
   CHECK(!meta_rest("\"mesa\"", NULL, &meta) && meta.mesa);
   CHECK(!meta_rest("\"MESA\"", NULL, &meta) && meta.mesa);
   CHECK(!meta_rest("\"Mesa3D\"", NULL, &meta) && !meta.mesa);
   /* vendor: 127 characters pass, 128 do not */
   CHECK(!meta_rest(text_of(NULL, 127, 'v', ""), NULL, &meta));
   CHECK(strlen(meta.vendor) == 127);
   CHECK(meta_rest(text_of(NULL, 128, 'v', ""), NULL, &meta) != NULL);
   CHECK(meta_rest("\"\"", NULL, &meta) != NULL);
   CHECK(meta_rest("7", NULL, &meta) != NULL);
   CHECK(meta_rest("null", NULL, &meta) != NULL);
   CHECK(meta_rest("true", NULL, &meta) != NULL);
   CHECK(meta_rest("[\"Mesa\"]", NULL, &meta) != NULL);
   CHECK(meta_rest("{\"a\": \"Mesa\"}", NULL, &meta) != NULL);
   /* the other keys keep their types */
   CHECK(meta_rest("\"Mesa\"", "\"schemaVersion\": [1]", &meta) != NULL);
   CHECK(meta_rest("\"Mesa\"", "\"libraryName\": null", &meta) != NULL);
   CHECK(meta_rest("\"Mesa\"", "\"minApi\": null", &meta) != NULL);

   /* long text is cut to 127 bytes, before a split UTF-8 character */
   CHECK(!meta_rest("\"Mesa\"", text_of("name", 200, 'a', ""), &meta));
   CHECK(strlen(meta.name) == 127);
   CHECK(!meta_rest("\"Mesa\"", text_of("driverVersion", 300, 'b', ""),
         &meta));
   CHECK(strlen(meta.version) == 127);
   CHECK(!meta_rest("\"Mesa\"", text_of("name", 125, 'a', "\xC3\xA9"),
         &meta));
   CHECK(strlen(meta.name) == 127);
   CHECK(!meta_rest("\"Mesa\"", text_of("name", 126, 'a', "\xC3\xA9"),
         &meta));
   CHECK(strlen(meta.name) == 126);
   CHECK(!meta_rest("\"Mesa\"", text_of("name", 125, 'a', "\xE2\x82\xAC"),
         &meta));
   CHECK(strlen(meta.name) == 125);

   /* a wrong type counts as absent */
   CHECK(!meta_rest("\"Mesa\"", "\"name\": 1, \"driverVersion\": null",
         &meta));
   CHECK(!*meta.name && !*meta.version);
   CHECK(!meta_rest("\"Mesa\"", "\"name\": {\"libraryName\": 5}, "
         "\"driverVersion\": [1, true]", &meta));
   CHECK(!*meta.name && !*meta.version);
   CHECK(!strcmp(meta.library_name, "v.so"));
   CHECK(!meta_rest("\"Mesa\"", "\"name\": false, \"driverVersion\": 2",
         &meta));

   /* a leading UTF-8 BOM */
   memcpy(bom, "\xEF\xBB\xBF", 3);
   memcpy(bom + 3, t30_meta, sizeof(t30_meta));
   CHECK(!vulkan_custom_driver_parse_meta(bom, sizeof(bom) - 1, 33, &meta));
   CHECK(!strcmp(meta.library_name, "vulkan.purple.so"));
   CHECK(!strcmp(meta.version, "26.3.0-T30-1.4.359"));
}

static struct string_list *list_of(const char *a, const char *b,
      const char *c)
{
   union string_list_elem_attr attr;
   struct string_list *list = string_list_new();
   attr.i = 0;
   string_list_append(list, a, attr);
   if (b)
      string_list_append(list, b, attr);
   if (c)
      string_list_append(list, c, attr);
   return list;
}

static void check_members(const char *a, const char *b, const char *c,
      bool pass)
{
   struct string_list *list = list_of(a, b, c);
   const char *err = vulkan_custom_driver_check_members(list,
         "vulkan.purple.so");
   CHECK(pass ? !err : err != NULL);
   string_list_free(list);
}

static void test_members(void)
{
   check_members("meta.json", "vulkan.purple.so", NULL, true);
   check_members("vulkan.purple.so", "meta.json", "README.txt", true);
   check_members("meta.json", "vulkan.purple.so", "libextra.so", false);
   check_members("meta.json", "lib/vulkan.purple.so", NULL, false);
   check_members("meta.json", "vulkan.other.so", NULL, false);
   check_members("vulkan.purple.so", NULL, NULL, false);
   check_members("x/meta.json", "vulkan.purple.so", NULL, false);
}

static void test_elf(void)
{
   unsigned char head[VULKAN_CUSTOM_DRIVER_ELF_HEAD];
   CHECK(!vulkan_custom_driver_check_elf(t30_elf, sizeof(t30_elf),
         18869912));
   CHECK(!vulkan_custom_driver_check_elf(t30_elf, sizeof(t30_elf),
         VULKAN_CUSTOM_DRIVER_SO_MAX));
   CHECK(vulkan_custom_driver_check_elf(t30_elf, sizeof(t30_elf),
         VULKAN_CUSTOM_DRIVER_SO_MAX + 1) != NULL);
   CHECK(vulkan_custom_driver_check_elf(t30_elf, sizeof(t30_elf) - 1,
         19) != NULL);
   /* 32-bit ARM */
   memcpy(head, t30_elf, sizeof(head));
   head[4]  = 1;
   head[18] = 40;
   CHECK(vulkan_custom_driver_check_elf(head, sizeof(head), 1000) != NULL);
   /* x86_64 */
   memcpy(head, t30_elf, sizeof(head));
   head[18] = 62;
   CHECK(vulkan_custom_driver_check_elf(head, sizeof(head), 1000) != NULL);
   /* ET_EXEC */
   memcpy(head, t30_elf, sizeof(head));
   head[16] = 2;
   CHECK(vulkan_custom_driver_check_elf(head, sizeof(head), 1000) != NULL);
   /* big-endian */
   memcpy(head, t30_elf, sizeof(head));
   head[5] = 2;
   CHECK(vulkan_custom_driver_check_elf(head, sizeof(head), 1000) != NULL);
   /* not ELF */
   memcpy(head, "PK\003\004", 4);
   CHECK(vulkan_custom_driver_check_elf(head, sizeof(head), 1000) != NULL);
}

static void test_key(void)
{
   uint32_t key = vulkan_custom_driver_key("/sdcard/a.zip", 18870258, 1000);
   CHECK(key == vulkan_custom_driver_key("/sdcard/a.zip", 18870258, 1000));
   CHECK(key != vulkan_custom_driver_key("/sdcard/b.zip", 18870258, 1000));
   CHECK(key != vulkan_custom_driver_key("/sdcard/a.zip", 18870259, 1000));
   CHECK(key != vulkan_custom_driver_key("/sdcard/a.zip", 18870258, 1001));
   /* names dirs on disk: the same on every host */
   CHECK(key == 0xd78f6bd1UL);
}

static void test_crash_state(void)
{
   char state[64];
   size_t len = 0;

   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/A.zip", "Core");
   CHECK(len == 14 && !memcmp(state, "Core\t/d/A.zip\n", 14));
   CHECK(vulkan_custom_driver_crash_find(state, len, "/d/A.zip", "Core"));
   CHECK(!vulkan_custom_driver_crash_find(state, len, "/d/A.zip", "Other"));
   CHECK(!vulkan_custom_driver_crash_find(state, len, "/d/A.zip", NULL));
   CHECK(!vulkan_custom_driver_crash_find(state, len, "/d/B.zip", "Core"));
   CHECK(vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/A.zip", "Core") == len);

   /* the menu is its own key */
   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/A.zip", NULL);
   CHECK(len == 24);
   CHECK(vulkan_custom_driver_crash_find(state, len, "/d/A.zip", ""));
   CHECK(vulkan_custom_driver_crash_find(state, len, "/d/A.zip", NULL));
   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/B.zip", "Core");
   CHECK(len == 38);

   /* picking A again clears it for every core */
   len = vulkan_custom_driver_crash_clear(state, len, "/d/A.zip");
   CHECK(len == 14 && !memcmp(state, "Core\t/d/B.zip\n", 14));

   /* a torn last line does not count, and is dropped by the next add */
   memcpy(state + len, "Core\t/d/C.z", 11);
   CHECK(!vulkan_custom_driver_crash_find(state, len + 11, "/d/C.z", "Core"));
   len = vulkan_custom_driver_crash_add(state, len + 11, sizeof(state),
         "/d/C.zip", "X");
   CHECK(len == 25 && !memcmp(state + 14, "X\t/d/C.zip\n", 11));

   /* full: the oldest lines go */
   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/D.zip", "Core");
   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/E.zip", "Core");
   CHECK(len == 53);
   len = vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/F.zip", "Core");
   CHECK(len == 53 && !memcmp(state, "X\t/d/C.zip\n", 11));
   CHECK(!vulkan_custom_driver_crash_find(state, len, "/d/B.zip", "Core"));
   CHECK(vulkan_custom_driver_crash_find(state, len, "/d/F.zip", "Core"));

   /* a line that cannot be read back is not written */
   CHECK(vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/\n.zip", "Core") == len);
   CHECK(vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "/d/F.zip", "a\tb") == len);
   CHECK(vulkan_custom_driver_crash_add(state, len, sizeof(state),
         "", "Core") == len);
   /* a tab in the path is fine: the core has none */
   len = vulkan_custom_driver_crash_add(state, 0, sizeof(state),
         "/d/a\tb.zip", "Core");
   CHECK(vulkan_custom_driver_crash_find(state, len, "/d/a\tb.zip", "Core"));
}

static int handles[16];

struct fake_loader
{
   unsigned loads;
   bool fixed;        /* "bad" packages load */
};

static void *fake_load(void *data, const char *path, uint32_t key,
      struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg)
{
   struct fake_loader *f = (struct fake_loader*)data;
   (void)key;
   f->loads++;
   if (strstr(path, "bad") && !f->fixed)
   {
      *msg = VULKAN_CUSTOM_DRIVER_MSG_INVALID;
      return NULL;
   }
   if (strstr(path, "broken"))
   {
      *msg = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      return NULL;
   }
   strcpy(meta->library_name, path + strlen(path) - 5);
   return &handles[f->loads];
}

static vulkan_custom_driver_session_t session;
static struct fake_loader loader;
static char saved[VULKAN_CUSTOM_DRIVER_STATE_MAX + 1];

static void begin(const char *state, const char *marker)
{
   loader.loads = 0;
   loader.fixed = false;
   vulkan_custom_driver_session_init(&session, fake_load, &loader,
         state, state ? strlen(state) : 0,
         marker, marker ? strlen(marker) : 0);
}

static struct vulkan_custom_driver_request request(const char *path,
      const char *core)
{
   struct vulkan_custom_driver_request req;
   req.size      = 18870258;
   req.mtime     = 1000;
   req.path      = path;
   req.core      = core;
   req.supported = true;
   req.safe_mode = false;
   return req;
}

static void *open_req(const struct vulkan_custom_driver_request *req,
      enum vulkan_custom_driver_msg *msg)
{
   const struct vulkan_custom_driver_meta *meta;
   void *h = vulkan_custom_driver_session_open(&session, req, &meta, msg);
   CHECK(h ? meta != NULL : meta == NULL);
   return h;
}

static void *open_drv(const char *path, const char *core,
      enum vulkan_custom_driver_msg *msg)
{
   struct vulkan_custom_driver_request req = request(path, core);
   return open_req(&req, msg);
}

static void test_session(void)
{
   enum vulkan_custom_driver_msg msg;
   struct vulkan_custom_driver_request req;
   const struct vulkan_custom_driver_meta *meta;
   void *a;

   /* empty value: the system driver, nothing shown */
   begin(NULL, NULL);
   CHECK(!open_drv("", "Core", &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(!open_drv(NULL, "Core", &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 0);

   /* safe mode */
   req           = request("/d/A.zip", "Core");
   req.safe_mode = true;
   CHECK(!open_req(&req, &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 0);

   /* not Adreno: UNSUPPORTED once per path */
   req           = request("/d/A.zip", "Core");
   req.supported = false;
   CHECK(!open_req(&req, &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED);
   CHECK(!open_req(&req, &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   req.path = "/d/B.zip";
   CHECK(!open_req(&req, &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED);
   CHECK(loader.loads == 0);

   /* loaded once, then kept; the marker is outstanding until started */
   begin(NULL, NULL);
   req = request("/d/A.zip", "Core");
   a   = vulkan_custom_driver_session_open(&session, &req, &meta, &msg);
   CHECK(a && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && loader.loads == 1);
   CHECK(meta && !strcmp(meta->library_name, "A.zip"));
   CHECK(vulkan_custom_driver_session_started(&session));
   CHECK(!vulkan_custom_driver_session_started(&session));
   CHECK(open_drv("/d/A.zip", "Core", &msg) == a && loader.loads == 1);

   /* A, system, B, A: A's slot is used again */
   CHECK(vulkan_custom_driver_session_started(&session));
   CHECK(!open_drv("", "Core", &msg));
   CHECK(!vulkan_custom_driver_session_started(&session));
   CHECK(open_drv("/d/B.zip", "Core", &msg) != a && loader.loads == 2);
   CHECK(open_drv("/d/A.zip", "Other", &msg) == a && loader.loads == 2);

   /* a new version of A is a new driver */
   req       = request("/d/A.zip", "Core");
   req.mtime = 2000;
   CHECK(open_req(&req, &msg) != a && loader.loads == 3);

   /* a 5th distinct driver: RESTART once, and the system driver */
   CHECK(open_drv("/d/C.zip", "Core", &msg) && loader.loads == 4
         && !session.full);
   CHECK(!open_drv("/d/E.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_RESTART && session.full);
   CHECK(!open_drv("/d/E.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && session.full);
   CHECK(open_drv("/d/A.zip", "Core", &msg) == a && loader.loads == 4
         && !session.full);

   /* a bad package: INVALID once, not loaded again until picked again */
   begin(NULL, NULL);
   CHECK(!open_drv("/d/bad.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID);
   CHECK(!open_drv("/d/bad.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 1);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/bad.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(!open_drv("/d/bad.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID);
   CHECK(loader.loads == 2);
   /* fixed and picked again: it loads, no restart */
   loader.fixed = true;
   vulkan_custom_driver_session_picked(&session, "/d/bad.zip");
   CHECK(open_drv("/d/bad.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && loader.loads == 3);
   loader.fixed = false;
   /* a missing zip is not loaded at all, and is tried again once picked */
   req      = request("/d/gone.zip", "Core");
   req.size = -1;
   CHECK(!open_req(&req, &msg) && msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID);
   CHECK(loader.loads == 3);
   vulkan_custom_driver_session_picked(&session, "/d/gone.zip");
   CHECK(open_drv("/d/gone.zip", "Core", &msg) && loader.loads == 4);
   /* adrenotools refused it: picking it again needs a restart */
   CHECK(!open_drv("/d/broken.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   CHECK(!open_drv("/d/broken.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 5);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/broken.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(!open_drv("/d/broken.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && loader.loads == 5);
   /* a path this session never opened */
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/new.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);

   /* failed in init: FAILED once, the system driver all session */
   begin(NULL, NULL);
   CHECK(open_drv("/d/A.zip", "Core", &msg));
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   CHECK(!vulkan_custom_driver_session_started(&session));
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(!open_drv("/d/A.zip", "Other", &msg));
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(open_drv("/d/B.zip", "Core", &msg) && loader.loads == 2);
   /* picked again: it may still be mapped, so only a restart retries */
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/A.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/A.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && loader.loads == 2);
}

static bool opened(void)
{
   return vulkan_custom_driver_session_current(&session);
}

/* Fills the table past its first n entries with bad packages, which
 * take entries but no slots. */
static void fill_table(unsigned n)
{
   enum vulkan_custom_driver_msg msg;
   char path[32];
   unsigned i;
   for (i = n; i < VULKAN_CUSTOM_DRIVER_PATHS; i++)
   {
      sprintf(path, "/d/bad%u.zip", i);
      CHECK(!open_drv(path, "Core", &msg)
            && msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID);
   }
   CHECK(session.path_count == VULKAN_CUSTOM_DRIVER_PATHS);
}

static void test_session_table(void)
{
   enum vulkan_custom_driver_msg msg;
   char state[64];
   size_t len;
   void *a;

   /* a full table: a new path gets the system driver and RESTART once
    * a session */
   begin(NULL, NULL);
   a = open_drv("/d/A.zip", "Core", &msg);
   CHECK(a && vulkan_custom_driver_session_started(&session));
   CHECK(!open_drv("/d/broken.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   fill_table(2);
   CHECK(loader.loads == 8);
   CHECK(!session.full);
   CHECK(!open_drv("/d/C.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_RESTART && session.full);
   CHECK(!open_drv("/d/C.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && session.full);
   CHECK(!open_drv("/d/D.zip", "Other", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && session.full);
   CHECK(loader.loads == 8 && session.path_count == 8);
   /* the 8 entries keep their state */
   CHECK(open_drv("/d/A.zip", "Core", &msg) == a
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && !session.full);
   CHECK(!open_drv("/d/C.zip", "Core", &msg) && session.full);
   CHECK(!open_drv("/d/broken.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && !session.full);
   CHECK(!open_drv("/d/C.zip", "Core", &msg) && session.full);
   CHECK(!open_drv("", "Core", &msg) && !session.full);
   CHECK(!open_drv("/d/bad2.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 8);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/broken.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/bad2.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(!open_drv("/d/bad2.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID && loader.loads == 9);
   /* picking a path with no room needs a restart, every time */
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/C.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/E.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(session.path_count == 8);

   /* picked first: that RESTART is the session's, and its crash state
    * still goes */
   len = vulkan_custom_driver_crash_add(state, 0, sizeof(state),
         "/d/C.zip", "Core");
   state[len] = '\0';
   begin(state, NULL);
   fill_table(0);
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/C.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(session.state_len == 0);
   CHECK(!open_drv("/d/C.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 8);

   /* level 2 only while the driver this init opened has not started */
   begin(NULL, NULL);
   CHECK(!opened());
   CHECK(open_drv("/d/A.zip", "Core", &msg) && opened());
   CHECK(vulkan_custom_driver_session_started(&session));
   CHECK(!opened() && session.current);
   CHECK(open_drv("/d/A.zip", "Core", &msg) && opened());
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   CHECK(!opened());
   CHECK(!open_drv("/d/A.zip", "Core", &msg) && !opened());
   CHECK(open_drv("/d/B.zip", "Core", &msg) && opened());
   CHECK(!open_drv("", "Core", &msg) && !opened());
   /* an init that took the instance B's init kept, before B started */
   CHECK(open_drv("/d/B.zip", "Core", &msg) && opened());
   vulkan_custom_driver_session_kept(&session);
   CHECK(!opened());
   CHECK(vulkan_custom_driver_session_started(&session));
   CHECK(!opened());
}

static void test_crash_marker(void)
{
   enum vulkan_custom_driver_msg msg;
   char marker[64];
   size_t len;

   /* the marker the glue writes when open() returns a custom driver */
   begin(NULL, NULL);
   CHECK(open_drv("/d/A.zip", "Core", &msg));
   len = vulkan_custom_driver_crash_add(marker, 0, sizeof(marker),
         "/d/A.zip", "Core");
   marker[len] = '\0';

   /* the process died before started(): next start, A is off for Core,
    * CRASHED once */
   begin(NULL, marker);
   CHECK(session.state_len == len && !memcmp(session.state, marker, len));
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_CRASHED);
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 0);
   CHECK(open_drv("/d/A.zip", "Other", &msg) && loader.loads == 1);
   CHECK(open_drv("/d/A.zip", NULL, &msg) && loader.loads == 1);
   CHECK(open_drv("/d/B.zip", "Core", &msg) && loader.loads == 2);

   /* sticky: every later start skips it too, with CRASHED once */
   memcpy(saved, session.state, session.state_len);
   saved[session.state_len] = '\0';
   begin(saved, NULL);
   CHECK(open_drv("/d/A.zip", NULL, &msg) && loader.loads == 1);
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_CRASHED);
   CHECK(!open_drv("/d/A.zip", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(loader.loads == 1);

   /* picked again: on for every core */
   CHECK(vulkan_custom_driver_session_picked(&session, "/d/A.zip")
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(session.state_len == 0);
   CHECK(open_drv("/d/A.zip", "Core", &msg) && loader.loads == 1);

   /* a crash in the menu only turns it off for the menu */
   len = vulkan_custom_driver_crash_add(marker, 0, sizeof(marker),
         "/d/A.zip", "");
   marker[len] = '\0';
   begin(NULL, marker);
   CHECK(!open_drv("/d/A.zip", NULL, &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_CRASHED);
   CHECK(open_drv("/d/A.zip", "Core", &msg));

   /* no marker, or one that names nothing */
   CHECK(!vulkan_custom_driver_session_init(&session, fake_load, &loader,
         NULL, 0, NULL, 0));
   CHECK(!vulkan_custom_driver_session_init(&session, fake_load, &loader,
         NULL, 0, "Core/d/A.zip\n", 13));
   CHECK(!vulkan_custom_driver_session_init(&session, fake_load, &loader,
         NULL, 0, "Core\t/d/A.zip", 13));
   CHECK(!vulkan_custom_driver_session_init(&session, fake_load, &loader,
         NULL, 0, "Core\t\n", 6));
   CHECK(vulkan_custom_driver_session_init(&session, fake_load, &loader,
         NULL, 0, marker, strlen(marker)));
}

int main(void)
{
   test_meta();
   test_meta_policy();
   test_members();
   test_elf();
   test_key();
   test_crash_state();
   test_session();
   test_session_table();
   test_crash_marker();
   printf("vulkan_custom_driver_test: %u checks, %u failures\n",
         checks, failures);
   return failures ? 1 : 0;
}

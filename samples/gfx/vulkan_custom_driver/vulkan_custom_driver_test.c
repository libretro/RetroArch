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
   /* the loader refused it: picking it again needs a restart */
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

#define ELF_LEN 336

static void put_le(unsigned char *p, uint64_t v, unsigned n)
{
   while (n--)
   {
      *p++ = (unsigned char)(v & 0xff);
      v  >>= 8;
   }
}

/* A little-endian ELF64: section headers at 64 (null, .dynstr, .dynamic),
 * .dynstr at 256 holding "\0libvulkan.so\0", .dynamic at 288 holding
 * DT_STRSZ, DT_SONAME 1 and DT_NULL. */
static void make_elf(unsigned char *elf)
{
   unsigned char *sh;
   memset(elf, 0, ELF_LEN);
   memcpy(elf, "\177ELF", 4);
   elf[4] = 2;
   elf[5] = 1;
   elf[6] = 1;
   put_le(elf + 40, 64, 8);
   put_le(elf + 58, 64, 2);
   put_le(elf + 60, 3, 2);
   sh = elf + 128;
   put_le(sh + 4,  3,   4);
   put_le(sh + 24, 256, 8);
   put_le(sh + 32, 32,  8);
   sh = elf + 192;
   put_le(sh + 4,  6,   4);
   put_le(sh + 24, 288, 8);
   put_le(sh + 32, 48,  8);
   put_le(sh + 40, 1,   4);
   put_le(sh + 56, 16,  8);
   memcpy(elf + 257, "libvulkan.so", 12);
   put_le(elf + 288, 10, 8);
   put_le(elf + 296, 32, 8);
   put_le(elf + 304, 14, 8);
   put_le(elf + 312, 1,  8);
}

static void test_patch_soname(void)
{
   static unsigned char elf[ELF_LEN];
   static unsigned char want[ELF_LEN];

   make_elf(elf);
   make_elf(want);
   memcpy(want + 257, "007", 3);
   CHECK(vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   /* A longer patch stops at the soname's end */
   make_elf(elf);
   make_elf(want);
   memcpy(want + 257, "0123456789AB", 12);
   CHECK(vulkan_custom_driver_patch_soname(elf, ELF_LEN, "0123456789ABCDEF"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   /* ... and at the end of .dynstr when the soname has no NUL there */
   make_elf(elf);
   memcpy(elf + 276, "abcdefghijkl", 12);
   put_le(elf + 312, 20, 8);
   memcpy(want, elf, ELF_LEN);
   memcpy(want + 276, "0123456789AB", 12);
   CHECK(vulkan_custom_driver_patch_soname(elf, ELF_LEN, "0123456789ABCDEF"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   elf[0] = 0;
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   elf[4] = 1;
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   elf[5] = 2;
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, 63, "007"));
   CHECK(!vulkan_custom_driver_patch_soname(elf, 255, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 58, 32, 2);
   put_le(elf + 40, 128, 8);
   put_le(elf + 192 + 40, 0, 4);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 40, 4096, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 192 + 40, 3, 4);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 192 + 56, 0, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 192 + 24, 4096, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 192 + 32, 4096, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 128 + 24, 4096, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 128 + 32, 4096, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 312, 32, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 304, 1, 8);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));

   make_elf(elf);
   put_le(elf + 192 + 4, 1, 4);
   memcpy(want, elf, ELF_LEN);
   CHECK(!vulkan_custom_driver_patch_soname(elf, ELF_LEN, "007"));
   CHECK(!memcmp(elf, want, ELF_LEN));
}

/* An ICD manifest as Mesa installs it */
static const char radv_icd[] =
   "{\n"
   "    \"ICD\": {\n"
   "        \"api_version\": \"1.4.367\",\n"
   "        \"library_arch\": \"64\",\n"
   "        \"library_path\": \"/opt/mesa/lib/libvulkan_radeon.so\"\n"
   "    },\n"
   "    \"file_format_version\": \"1.0.1\"\n"
   "}\n";

static struct vulkan_custom_driver_icd icd;

static const char *icd_of(const char *text)
{
   return vulkan_custom_driver_parse_icd(text, strlen(text), &icd);
}

static void test_icd(void)
{
   CHECK(!icd_of(radv_icd));
   CHECK(!strcmp(icd.library_path, "/opt/mesa/lib/libvulkan_radeon.so"));
   CHECK(!strcmp(icd.api_version, "1.4.367"));
   /* JSON escapes: a Windows manifest's backslashes */
   CHECK(!icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":"
         "{\"library_path\":\".\\\\vulkan_lvp.dll\",\"api_version\":\"1.3.290\"}}"));
   CHECK(!strcmp(icd.library_path, ".\\vulkan_lvp.dll"));
   /* other keys, and the same names deeper down, are not read */
   CHECK(!icd_of("{\"x\":[1,{\"library_path\":2}],\"file_format_version\":\"1.0.0\","
         "\"ICD\":{\"is_portability_driver\":false,\"library_path\":\"./a.so\","
         "\"api_version\":\"1.3.0\",\"y\":{\"api_version\":3}}}"));
   CHECK(!strcmp(icd.library_path, "./a.so"));
   CHECK(!strcmp(icd.api_version, "1.3.0"));
   /* missing fields; library_path outside ICD is not ICD's */
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"library_path\":\"./a.so\","
         "\"ICD\":{\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"ICD\":{\"library_path\":\"./a.so\",\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"\",\"ICD\":"
         "{\"library_path\":\"./a.so\",\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":{\"library_path\":\"./a.so\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":"
         "{\"library_path\":\"\",\"api_version\":\"1.3.0\"}}") != NULL);
   /* wrong types */
   CHECK(icd_of("{\"file_format_version\":1,\"ICD\":"
         "{\"library_path\":\"./a.so\",\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":\"./a.so\"}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":[]}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":"
         "{\"library_path\":[\"./a.so\"],\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":"
         "{\"library_path\":null,\"api_version\":\"1.3.0\"}}") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":"
         "{\"library_path\":\"./a.so\",\"api_version\":{}}}") != NULL);
   /* not an object, not JSON, cut short */
   CHECK(icd_of("[]") != NULL);
   CHECK(icd_of("\"ICD\"") != NULL);
   CHECK(icd_of("") != NULL);
   CHECK(icd_of("not json") != NULL);
   CHECK(icd_of("{\"file_format_version\":\"1.0.0\",\"ICD\":{\"library_path\":\"./a.so\"") != NULL);
   /* over 4 KiB */
   memset(json, ' ', VULKAN_CUSTOM_DRIVER_META_MAX + 1);
   memcpy(json, radv_icd, strlen(radv_icd));
   CHECK(vulkan_custom_driver_parse_icd(json, VULKAN_CUSTOM_DRIVER_META_MAX + 1,
            &icd) != NULL);
   CHECK(!vulkan_custom_driver_parse_icd(json, VULKAN_CUSTOM_DRIVER_META_MAX,
            &icd));
}

static char lib[PATH_MAX_LENGTH];

static const char *lib_of(const char *json_path, const char *library_path)
{
   lib[0] = '\0';
   return vulkan_custom_driver_icd_library(json_path, library_path,
         lib, sizeof(lib));
}

static void test_icd_library(void)
{
   CHECK(!lib_of("/m/share/vulkan/icd.d/radeon_icd.x86_64.json",
            "/m/lib/libvulkan_radeon.so"));
   CHECK(!strcmp(lib, "/m/lib/libvulkan_radeon.so"));
   CHECK(!lib_of("/m/share/vulkan/icd.d/radeon_icd.x86_64.json",
            "../../../lib/libvulkan_radeon.so"));
   CHECK(!strcmp(lib, "/m/share/vulkan/icd.d/../../../lib/libvulkan_radeon.so"));
   CHECK(!lib_of("/d/icd.json", "./libvulkan_freedreno.so"));
   CHECK(!strcmp(lib, "/d/./libvulkan_freedreno.so"));
   /* a bare name is the system's library */
   CHECK(lib_of("/d/icd.json", "libvulkan_radeon.so") != NULL);
   CHECK(lib_of("C:\\Mesa\\lvp_icd.x86_64.json", "vulkan_lvp.dll") != NULL);
   CHECK(lib_of("C:\\Mesa\\lvp_icd.x86_64.json", "C:vulkan_lvp.dll") != NULL);
   /* Windows: backslashes, drive letters, forward slashes, shares */
   CHECK(!lib_of("C:\\Mesa\\lvp_icd.x86_64.json", ".\\vulkan_lvp.dll"));
   CHECK(!strcmp(lib, "C:\\Mesa\\.\\vulkan_lvp.dll"));
   CHECK(!lib_of("C:\\Mesa\\lvp_icd.x86_64.json", "d:\\lvp\\vulkan_lvp.dll"));
   CHECK(!strcmp(lib, "d:\\lvp\\vulkan_lvp.dll"));
   CHECK(!lib_of("C:/Mesa/lvp_icd.x86_64.json", "bin/vulkan_lvp.dll"));
   CHECK(!strcmp(lib, "C:/Mesa/bin/vulkan_lvp.dll"));
   CHECK(!lib_of("\\\\nas\\mesa\\icd.json", "\\\\nas\\mesa\\vulkan_lvp.dll"));
   CHECK(!strcmp(lib, "\\\\nas\\mesa\\vulkan_lvp.dll"));
   /* a manifest path with no directory leaves the name as it is */
   CHECK(!lib_of("icd.json", "./a.so"));
   CHECK(!strcmp(lib, "./a.so"));
   /* out holds PATH_MAX_LENGTH - 1 characters */
   memset(json, 'a', PATH_MAX_LENGTH + 1);
   json[0]                   = '/';
   json[PATH_MAX_LENGTH - 1] = '\0';
   CHECK(!lib_of("/d/icd.json", json));
   CHECK(strlen(lib) == PATH_MAX_LENGTH - 1);
   json[PATH_MAX_LENGTH - 1] = 'a';
   json[PATH_MAX_LENGTH]     = '\0';
   CHECK(lib_of("/d/icd.json", json) != NULL);
   json[0]                   = 'a';
   json[1]                   = '/';
   json[PATH_MAX_LENGTH - 4] = '\0';
   CHECK(!lib_of("/d/icd.json", json));
   CHECK(strlen(lib) == PATH_MAX_LENGTH - 1);
   json[PATH_MAX_LENGTH - 4] = 'a';
   json[PATH_MAX_LENGTH - 3] = '\0';
   CHECK(lib_of("/d/icd.json", json) != NULL);
}

static void test_session_device(void)
{
   enum vulkan_custom_driver_msg msg;
   void *a;
   void *b;

   begin(NULL, NULL);
   /* a custom init: its own library, or a layer in front, passes */
   a = open_drv("/d/a.json", "Core", &msg);
   CHECK(a && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_CURRENT, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN, &msg));
   CHECK(!vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(!vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_OTHER, &msg));
   /* it failed and was closed: its slot is free */
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   CHECK(vulkan_custom_driver_session_drop(&session, a));
   CHECK(!session.slots[0].handle);
   CHECK(!vulkan_custom_driver_session_drop(&session, a));
   CHECK(!vulkan_custom_driver_session_drop(&session, NULL));
   /* the system init after it: RESTART once for a custom library */
   CHECK(!open_drv("/d/a.json", "Core", &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_OTHER, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_RESTART);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_OTHER, &msg));
   CHECK(msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   /* the freed slot takes the next driver */
   b = open_drv("/d/b.json", "Core", &msg);
   CHECK(b && b != a && loader.loads == 2);
   CHECK(session.slots[0].handle == b);
   /* a kept instance is not judged, the next open() is */
   vulkan_custom_driver_session_kept(&session);
   CHECK(vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM, &msg));
   CHECK(open_drv("/d/b.json", "Core", &msg) == b && loader.loads == 2);
   CHECK(!vulkan_custom_driver_session_device(&session,
            VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM, &msg));
}

static void test_session_reason(void)
{
   enum vulkan_custom_driver_msg msg;

   begin(NULL, NULL);
   /* no custom driver starting: nothing to give a reason for */
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED);
   CHECK(session.reason == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   /* the first reason stands, in place of FAILED */
   CHECK(open_drv("/d/a.json", "Core", &msg));
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED);
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES);
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED);
   CHECK(session.reason == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   /* the system driver all session, the notice once */
   CHECK(!open_drv("/d/a.json", "Core", &msg)
         && msg == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(open_drv("/d/b.json", "Core", &msg));
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES);
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES);
   /* an init that never called failed(): the next open() drops its reason */
   CHECK(open_drv("/d/c.json", "Core", &msg));
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES);
   CHECK(open_drv("/d/c.json", "Core", &msg));
   CHECK(session.reason == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_FAILED);
   /* a started driver, or a kept instance, takes none */
   CHECK(open_drv("/d/d.json", "Core", &msg));
   CHECK(vulkan_custom_driver_session_started(&session));
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED);
   CHECK(session.reason == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   vulkan_custom_driver_session_kept(&session);
   vulkan_custom_driver_session_reason(&session,
         VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED);
   CHECK(session.reason == VULKAN_CUSTOM_DRIVER_MSG_NONE);
   CHECK(vulkan_custom_driver_session_failed(&session)
         == VULKAN_CUSTOM_DRIVER_MSG_NONE);
}

/* VkExtensionProperties' layout */
struct ext_rec
{
   char name[VULKAN_CUSTOM_DRIVER_EXT_NAME_MAX];
   uint32_t spec;
};

static struct ext_rec exts[8];

/* A driver's list of up to two names; the records after it are junk. */
static size_t exts_of(const char *a, const char *b)
{
   size_t n = 0;
   memset(exts, 0x5a, sizeof(exts));
   if (a)
   {
      strcpy(exts[n].name, a);
      exts[n++].spec = 1;
   }
   if (b)
   {
      strcpy(exts[n].name, b);
      exts[n++].spec = 1;
   }
   return n;
}

static unsigned exts_count(size_t n, const char *name)
{
   size_t i;
   unsigned found = 0;
   for (i = 0; i < n; i++)
      if (!strcmp(exts[i].name, name))
         found++;
   return found;
}

static size_t add_loader_exts(size_t n, size_t cap)
{
   return vulkan_custom_driver_loader_exts((char*)exts, sizeof(exts[0]),
         n, cap);
}

static void test_loader_exts(void)
{
   size_t n;

   /* a WSI-only driver: the loader's four follow, zero-filled */
   n = add_loader_exts(exts_of("VK_KHR_surface", "VK_KHR_xlib_surface"), 8);
   CHECK(n == 6);
   CHECK(!strcmp(exts[0].name, "VK_KHR_surface") && exts[0].spec == 1);
   CHECK(!strcmp(exts[1].name, "VK_KHR_xlib_surface") && exts[1].spec == 1);
   CHECK(!strcmp(exts[2].name, "VK_EXT_debug_report"));
   CHECK(!strcmp(exts[3].name, "VK_EXT_debug_utils"));
   CHECK(!strcmp(exts[4].name, "VK_KHR_portability_enumeration"));
   CHECK(!strcmp(exts[5].name, "VK_LUNARG_direct_driver_loading"));
   CHECK(exts[5].spec == 0
         && !exts[5].name[VULKAN_CUSTOM_DRIVER_EXT_NAME_MAX - 1]);
   CHECK(exts[6].spec == 0x5a5a5a5aUL);
   /* Mesa lists the debug extensions itself: no duplicates */
   n = add_loader_exts(exts_of("VK_EXT_debug_utils", "VK_EXT_debug_report"), 8);
   CHECK(n == 4);
   CHECK(exts_count(n, "VK_EXT_debug_utils") == 1);
   CHECK(exts_count(n, "VK_EXT_debug_report") == 1);
   CHECK(exts_count(n, "VK_LUNARG_direct_driver_loading") == 1);
   /* a longer name is another extension */
   n = add_loader_exts(exts_of("VK_EXT_debug_utils_x", NULL), 8);
   CHECK(n == 5 && exts_count(n, "VK_EXT_debug_utils") == 1);
   /* a driver with none */
   CHECK(add_loader_exts(exts_of(NULL, NULL), 8)
         == VULKAN_CUSTOM_DRIVER_LOADER_EXTS);
   /* cap records at most */
   n = add_loader_exts(exts_of("VK_KHR_surface", NULL), 2);
   CHECK(n == 2 && !strcmp(exts[1].name, "VK_EXT_debug_report"));
   CHECK(exts[2].spec == 0x5a5a5a5aUL);
   CHECK(add_loader_exts(2, 2) == 2);
   /* records too small for a Vulkan name are left alone */
   CHECK(vulkan_custom_driver_loader_exts((char*)exts, 16, 1, 8) == 1);
}

static void test_icd_folder(void)
{
   char f[PATH_MAX_LENGTH];
   char small[8];
   vulkan_custom_driver_icd_folder("/home/d/x/mesa-linux/install-radv/"
         "share/vulkan/icd.d/radeon_icd.x86_64.json", f, sizeof(f));
   CHECK(!strcmp(f, ".../mesa-linux/install-radv"));
   vulkan_custom_driver_icd_folder(
         "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json", f, sizeof(f));
   CHECK(!strcmp(f, "/usr"));
   vulkan_custom_driver_icd_folder(
         "C:\\mesa\\x64\\lvp_icd.x86_64.json", f, sizeof(f));
   CHECK(!strcmp(f, "...\\mesa\\x64"));
   vulkan_custom_driver_icd_folder("C:\\m\\inst\\share\\vulkan\\icd.d\\x.json", f, sizeof(f));
   CHECK(!strcmp(f, "...\\m\\inst"));
   vulkan_custom_driver_icd_folder("C:\\x.json", f, sizeof(f));
   CHECK(!strcmp(f, "C:\\"));
   vulkan_custom_driver_icd_folder("/opt/mesa/lib/x.json", f, sizeof(f));
   CHECK(!strcmp(f, ".../mesa/lib"));
   vulkan_custom_driver_icd_folder("/a/b/x.json", f, sizeof(f));
   CHECK(!strcmp(f, "/a/b"));
   vulkan_custom_driver_icd_folder("/radeon.json", f, sizeof(f));
   CHECK(!strcmp(f, "/"));
   vulkan_custom_driver_icd_folder("/share/vulkan/icd.d/x.json", f, sizeof(f));
   CHECK(!strcmp(f, "/"));
   vulkan_custom_driver_icd_folder("radeon.json", f, sizeof(f));
   CHECK(!strcmp(f, "."));
   vulkan_custom_driver_icd_folder("share/vulkan/icd.d/x.json", f, sizeof(f));
   CHECK(!strcmp(f, "."));
   vulkan_custom_driver_icd_folder("icd/radeon.json", f, sizeof(f));
   CHECK(!strcmp(f, "icd"));
   /* truncated, still terminated */
   CHECK(vulkan_custom_driver_icd_folder("/opt/mesa/lib/x.json",
            small, sizeof(small)) == sizeof(small) - 1);
   CHECK(!strcmp(small, ".../mes"));
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
   test_patch_soname();
   test_icd();
   test_icd_library();
   test_session_device();
   test_session_reason();
   test_loader_exts();
   test_icd_folder();
   printf("vulkan_custom_driver_test: %u checks, %u failures\n",
         checks, failures);
   return failures ? 1 : 0;
}

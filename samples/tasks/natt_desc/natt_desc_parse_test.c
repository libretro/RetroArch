/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (natt_desc_parse_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the UPnP IGD description parser
 * (network/natt_desc.c::natt_parse_desc_node).
 *
 * The description XML is served by a router on the LAN, so it is untrusted.
 * The parser previously:
 *   1. took the else-branch when a node had no children, then walked
 *      `child = child->next` with child already NULL, crashing on any leaf
 *      node, including an empty root;
 *   2. recursed only into nodes without children, so a <service> nested
 *      inside <serviceList>/<device>/<root> (the real UPnP shape) was never
 *      reached.
 * It now also NULL-checks serviceType->data / controlURL->data before strstr.
 *
 * The SSDP reply naming the device is untrusted as well: control and
 * description URLs must be http:// on the numeric IPv4 address the reply
 * came from (natt_url_matches_device), and the service type, which is
 * copied into the SOAPAction header, must be one of the two WAN service
 * URNs exactly (natt_service_type_valid).
 *
 * The walk lives in its own translation unit so this test can #include the
 * real parser source. The hostile-tree cases crash the old parser and the
 * nested-service case fails on it.
 *
 * Build standalone:
 *   cc -Wall -pedantic -std=gnu99 -g -O0 -I../../../libretro-common/include \
 *      -o natt_desc_parse_test natt_desc_parse_test.c
 *   ./natt_desc_parse_test
 */

#include <stdio.h>
#include <string.h>

/* The strl.h macro maps strlcpy/strlcat to strlcpy_retro__/strlcat_retro__ on
 * non-Apple hosts; pull in the real vendored implementation (dependency-free)
 * so the link resolves, then the real parser source. */
#include "../../../libretro-common/compat/compat_strl.c"
#include "../../../network/natt_desc.c"

static int failures = 0;

static void ok(int cond, const char *label)
{
   if (cond)
      printf("[SUCCESS] %s\n", label);
   else
   {
      printf("[FAILED]  %s\n", label);
      failures++;
   }
}

/* Wire one rxml node. name/data are read-only to the parser, so pointing them
 * at string literals is safe (the cast just drops const for the char* fields). */
static rxml_node_t *mk(rxml_node_t *n, const char *name, const char *data,
      rxml_node_t *children, rxml_node_t *next)
{
   n->name     = (char *)name;
   n->data     = (char *)data;
   n->attrib   = NULL;
   n->children = children;
   n->next     = next;
   return n;
}

/* A device whose SSDP reply came from 10.0.0.1. The address is written as
 * bytes (network order) so no socket library is needed to link. */
static void reset(struct natt_device *d)
{
   static const unsigned char ip[4] = { 10, 0, 0, 1 };
   memset(d, 0, sizeof(*d));
   d->addr.sin_family = AF_INET;
   memcpy(&d->addr.sin_addr, ip, sizeof(ip));
}

int main(void)
{
   struct natt_device dev;
   char long_str[400];
   size_t i;

   /* --- 1. NULL node: must not crash, returns false --- */
   reset(&dev);
   ok(!natt_parse_desc_node(NULL, &dev), "NULL node rejected (no crash)");

   /* --- 2. Empty root leaf: the hostile empty-root response that crashed
    *        the pre-fix parser (child==NULL -> child->next deref). --- */
   {
      rxml_node_t root;
      mk(&root, "root", NULL, NULL, NULL);
      reset(&dev);
      ok(!natt_parse_desc_node(&root, &dev), "empty root leaf rejected (no crash)");
   }

   /* --- 3. <service> leaf with no children: no crash, no match --- */
   {
      rxml_node_t svc;
      mk(&svc, "service", NULL, NULL, NULL);
      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "childless <service> rejected (no crash)");
   }

   /* --- 4. Deeply nested WANIPConnection, full http:// controlURL.
    *        Pre-fix this returned false (recursion never descended). --- */
   {
      rxml_node_t root, device, slist, svc, st, cu;
      mk(&cu,     "controlURL",  "http://10.0.0.1:5000/ctl", NULL, NULL);
      mk(&st,     "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc,    "service",     NULL, &st,     NULL);
      mk(&slist,  "serviceList", NULL, &svc,    NULL);
      mk(&device, "device",      NULL, &slist,  NULL);
      mk(&root,   "root",        NULL, &device, NULL);

      reset(&dev);
      ok(natt_parse_desc_node(&root, &dev), "nested WANIPConnection found");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/ctl"),
            "  -> control URL bound (full http)");
      ok(!strcmp(dev.service_type,
            "urn:schemas-upnp-org:service:WANIPConnection:1"),
            "  -> service_type recorded");
   }

   /* --- 5. Nested WANPPPConnection, RELATIVE controlURL resolved against
    *        the device desc URL (exercises the build else-branch). --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "/ppp", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANPPPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      strlcpy(dev.desc, "http://10.0.0.1:5000/desc.xml", sizeof(dev.desc));
      ok(natt_parse_desc_node(&svc, &dev), "WANPPPConnection found");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/ppp"),
            "  -> relative control URL resolved against desc");
   }

   /* --- 6. <service> whose serviceType child carries NO data: the NULL-guard
    *        case. Must not crash, no match. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "http://10.0.0.1/ctl", NULL, NULL);
      mk(&st,  "serviceType", NULL, NULL, &cu); /* data == NULL */
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "NULL serviceType->data rejected (no crash)");
   }

   /* --- 7. controlURL child with NO data: the other NULL-guard. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  NULL, NULL, NULL); /* data == NULL */
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "NULL controlURL->data rejected (no crash)");
   }

   /* --- 8. Unsupported service type: valid data, but not WANIP/WANPPP. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "http://10.0.0.1/ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "unsupported service type rejected");
   }

   /* --- 9. First service unsupported, sibling is WANIP: the parser must keep
    *        walking siblings and bind the second one. --- */
   {
      rxml_node_t slist, svcA, stA, cuA, svcB, stB, cuB;
      mk(&cuA,   "controlURL",  "http://10.0.0.1/a", NULL, NULL);
      mk(&stA,   "serviceType", "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1", NULL, &cuA);
      mk(&cuB,   "controlURL",  "http://10.0.0.1:5000/b", NULL, NULL);
      mk(&stB,   "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cuB);
      mk(&svcB,  "service",     NULL, &stB, NULL);
      mk(&svcA,  "service",     NULL, &stA, &svcB); /* svcA.next = svcB */
      mk(&slist, "serviceList", NULL, &svcA, NULL);

      reset(&dev);
      ok(natt_parse_desc_node(&slist, &dev), "sibling WANIP after unsupported found");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/b"),
            "  -> second service's control URL bound");
   }

   /* --- 10. URLs bound to the responder's address. --- */
   {
      static const char *const good[] = {
         "http://10.0.0.1",
         "http://10.0.0.1/",
         "http://10.0.0.1:5000/desc.xml",
         "http://10.0.0.1:1/x?y=z",
         "http://10.0.0.1?q",
         "HTTP://10.0.0.1:80/ctl",
         "http://10.0.0.1:00080/ctl",
         "http://10.0.0.1:5000//10.0.0.2/x",
      };
      static const char *const bad[] = {
         "",
         "10.0.0.1/ctl",
         "https://10.0.0.1/ctl",
         "http://10.0.0.2/ctl",          /* another host */
         "http://10.0.0.10/ctl",         /* responder is a prefix */
         "http://10.0.0.1.5/ctl",
         "http://010.0.0.1/ctl",
         "http://10.0.0/ctl",
         "http://10.0.0.256/ctl",
         "http://router.lan/ctl",
         "http://evil@10.0.0.1/ctl",     /* userinfo */
         "http://10.0.0.1@evil/ctl",
         "http://10.0.0.1:80@evil/ctl",
         "http://10.0.0.1:/ctl",
         "http://10.0.0.1:0/ctl",
         "http://10.0.0.1:65536/ctl",
         "http://10.0.0.1:123456/ctl",
         "http://10.0.0.1:+80/ctl",
         "http://10.0.0.1:80x/ctl",
         "http://10.0.0.1\\evil/ctl",
         "http://10.0.0.1/c tl",
         "http://10.0.0.1/ctl\r\nX-Injected: 1",
         "http://10.0.0.1/ctl\n",
         "http://10.0.0.1/ctl\t",
         "http://10.0.0.1/ctl\x7f",
         "http://10.0.0.1/ctl#frag",
      };

      reset(&dev);
      for (i = 0; i < sizeof(good) / sizeof(*good); i++)
         ok(natt_url_matches_device(good[i], &dev), good[i]);
      for (i = 0; i < sizeof(bad) / sizeof(*bad); i++)
      {
         char label[160];
         snprintf(label, sizeof(label), "rejected: %s", bad[i]);
         ok(!natt_url_matches_device(bad[i], &dev), label);
      }

      ok(!natt_url_matches_device(NULL, &dev), "NULL URL rejected");
      ok(!natt_url_matches_device("http://10.0.0.1/", NULL),
            "NULL device rejected");
      dev.addr.sin_family = 0;
      ok(!natt_url_matches_device("http://10.0.0.1/", &dev),
            "device without an IPv4 responder rejected");
   }

   /* --- 11. Service URNs that may reach the SOAPAction header. --- */
   {
      static const char *const good[] = {
         "urn:schemas-upnp-org:service:WANIPConnection:1",
         "urn:schemas-upnp-org:service:WANIPConnection:2",
         "urn:schemas-upnp-org:service:WANPPPConnection:1",
         "urn:schemas-upnp-org:service:WANIPConnection:10",
      };
      static const char *const bad[] = {
         "",
         "urn:schemas-upnp-org:service:WANIPConnection:",
         "urn:schemas-upnp-org:service:WANIPConnection:1 ",
         "urn:schemas-upnp-org:service:WANIPConnection:1\r\nX-Injected: 1",
         "urn:schemas-upnp-org:service:WANIPConnection:1\"",
         "urn:schemas-upnp-org:service:WANIPConnection:v1",
         "urn:evil:service:WANIPConnection:1",
         "x urn:schemas-upnp-org:service:WANIPConnection:1",
         "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1",
      };

      for (i = 0; i < sizeof(good) / sizeof(*good); i++)
         ok(natt_service_type_valid(good[i]), good[i]);
      for (i = 0; i < sizeof(bad) / sizeof(*bad); i++)
      {
         char label[160];
         snprintf(label, sizeof(label), "rejected: %s", bad[i]);
         ok(!natt_service_type_valid(bad[i]), label);
      }
      ok(!natt_service_type_valid(NULL), "NULL service type rejected");
   }

   /* --- 12. Absolute controlURL on another host: nothing bound. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "http://10.0.0.2:5000/ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "controlURL on another host rejected");
      ok(!*dev.control && !*dev.service_type, "  -> nothing left bound");
   }

   /* --- 13. Relative controlURL that would reach another host: it stays
    *         a path on the device's own host. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "@10.0.0.2/ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      strlcpy(dev.desc, "http://10.0.0.1:5000/desc.xml", sizeof(dev.desc));
      ok(natt_parse_desc_node(&svc, &dev), "relative '@host' controlURL bound");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/@10.0.0.2/ctl"),
            "  -> as a path on the device");
   }

   /* --- 14. Relative controlURL against a description URL with a query
    *         and no path. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      strlcpy(dev.desc, "http://10.0.0.1:5000?d=/x/y", sizeof(dev.desc));
      ok(natt_parse_desc_node(&svc, &dev), "relative controlURL after query-only desc");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/ctl"),
            "  -> resolved against the authority");
   }

   /* --- 15. Relative controlURL carrying a header break: rejected. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "/ctl HTTP/1.1\r\nX-Injected: 1", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      strlcpy(dev.desc, "http://10.0.0.1:5000/desc.xml", sizeof(dev.desc));
      ok(!natt_parse_desc_node(&svc, &dev), "controlURL with CRLF rejected");
      ok(!*dev.control && !*dev.service_type, "  -> nothing left bound");
   }

   /* --- 16. serviceType carrying a header break: rejected. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "http://10.0.0.1:5000/ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1\r\nX-Injected: 1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "serviceType with CRLF rejected");
      ok(!*dev.control && !*dev.service_type, "  -> nothing left bound");
   }

   /* --- 17. Valid serviceType too long for the buffer: rejected whole. --- */
   {
      rxml_node_t svc, st, cu;
      size_t _len = strlcpy(long_str,
            "urn:schemas-upnp-org:service:WANIPConnection:", sizeof(long_str));
      memset(long_str + _len, '1', sizeof(long_str) - _len - 1);
      long_str[sizeof(long_str) - 1] = '\0';
      mk(&cu,  "controlURL",  "http://10.0.0.1:5000/ctl", NULL, NULL);
      mk(&st,  "serviceType", long_str, NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "over-long serviceType rejected");
      ok(!*dev.control && !*dev.service_type, "  -> nothing left bound");
   }

   /* --- 18. Absolute controlURL too long for the buffer: rejected. --- */
   {
      rxml_node_t svc, st, cu;
      size_t _len = strlcpy(long_str, "http://10.0.0.1:5000/", sizeof(long_str));
      memset(long_str + _len, 'a', sizeof(long_str) - _len - 1);
      long_str[sizeof(long_str) - 1] = '\0';
      mk(&cu,  "controlURL",  long_str, NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      reset(&dev);
      ok(!natt_parse_desc_node(&svc, &dev), "over-long controlURL rejected");
      ok(!*dev.control && !*dev.service_type, "  -> nothing left bound");
   }

   /* --- 19. A rejected service does not stop a valid sibling binding. --- */
   {
      rxml_node_t slist, svcA, stA, cuA, svcB, stB, cuB;
      mk(&cuA,   "controlURL",  "http://10.0.0.9/a", NULL, NULL);
      mk(&stA,   "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cuA);
      mk(&cuB,   "controlURL",  "/b", NULL, NULL);
      mk(&stB,   "serviceType", "urn:schemas-upnp-org:service:WANPPPConnection:1", NULL, &cuB);
      mk(&svcB,  "service",     NULL, &stB, NULL);
      mk(&svcA,  "service",     NULL, &stA, &svcB);
      mk(&slist, "serviceList", NULL, &svcA, NULL);

      reset(&dev);
      strlcpy(dev.desc, "http://10.0.0.1:5000/desc.xml", sizeof(dev.desc));
      ok(natt_parse_desc_node(&slist, &dev), "valid sibling after a foreign-host service bound");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/b")
            && !strcmp(dev.service_type,
               "urn:schemas-upnp-org:service:WANPPPConnection:1"),
            "  -> second service's URL and type bound");
   }

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll natt_parse_desc_node regression tests passed.\n");
   return 0;
}

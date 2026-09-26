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

int main(void)
{
   struct natt_device dev;

   /* --- 1. NULL node: must not crash, returns false --- */
   memset(&dev, 0, sizeof(dev));
   ok(!natt_parse_desc_node(NULL, &dev), "NULL node rejected (no crash)");

   /* --- 2. Empty root leaf: the hostile empty-root response that crashed
    *        the pre-fix parser (child==NULL -> child->next deref). --- */
   {
      rxml_node_t root;
      mk(&root, "root", NULL, NULL, NULL);
      memset(&dev, 0, sizeof(dev));
      ok(!natt_parse_desc_node(&root, &dev), "empty root leaf rejected (no crash)");
   }

   /* --- 3. <service> leaf with no children: no crash, no match --- */
   {
      rxml_node_t svc;
      mk(&svc, "service", NULL, NULL, NULL);
      memset(&dev, 0, sizeof(dev));
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

      memset(&dev, 0, sizeof(dev));
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

      memset(&dev, 0, sizeof(dev));
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

      memset(&dev, 0, sizeof(dev));
      ok(!natt_parse_desc_node(&svc, &dev), "NULL serviceType->data rejected (no crash)");
   }

   /* --- 7. controlURL child with NO data: the other NULL-guard. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  NULL, NULL, NULL); /* data == NULL */
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      memset(&dev, 0, sizeof(dev));
      ok(!natt_parse_desc_node(&svc, &dev), "NULL controlURL->data rejected (no crash)");
   }

   /* --- 8. Unsupported service type: valid data, but not WANIP/WANPPP. --- */
   {
      rxml_node_t svc, st, cu;
      mk(&cu,  "controlURL",  "http://10.0.0.1/ctl", NULL, NULL);
      mk(&st,  "serviceType", "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1", NULL, &cu);
      mk(&svc, "service",     NULL, &st, NULL);

      memset(&dev, 0, sizeof(dev));
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

      memset(&dev, 0, sizeof(dev));
      ok(natt_parse_desc_node(&slist, &dev), "sibling WANIP after unsupported found");
      ok(!strcmp(dev.control, "http://10.0.0.1:5000/b"),
            "  -> second service's control URL bound");
   }

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll natt_parse_desc_node regression tests passed.\n");
   return 0;
}

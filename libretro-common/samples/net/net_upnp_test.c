/* net_upnp: the SSDP, description and SOAP response parsers, and the
 * SOAP operations against a fake gateway on the loopback interface. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket.h>
#include <net/net_upnp.h>
#include <rthreads/rthreads.h>

static int fails;

#define CHECK(cond, what) \
   do { if (!(cond)) { printf("FAIL: %s\n", what); fails++; } } while (0)

static const char desc_xml[] =
   "<?xml version=\"1.0\"?>"
   "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
   "<device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>"
   "<serviceList><service>"
   "<serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType>"
   "<controlURL>/l3f</controlURL></service></serviceList>"
   "<deviceList><device><deviceList><device><serviceList><service>"
   "<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
   "<controlURL>ctl/IPConn</controlURL>"
   "</service></serviceList></device></deviceList></device></deviceList>"
   "</device></root>";

static const char ext_xml[] =
   "<?xml version=\"1.0\"?>"
   "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
   "<u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
   "<NewExternalIPAddress>203.0.113.7</NewExternalIPAddress>"
   "</u:GetExternalIPAddressResponse></s:Body></s:Envelope>";

/* ---- the fake gateway: answers a fixed number of requests ---- */

struct gateway
{
   int  fd;
   int  requests;
   char last_action[64];
   char last_body[2048];
};

static void respond(int fd, int status, const char *body)
{
   char head[256];
   int  n = snprintf(head, sizeof(head),
         "HTTP/1.1 %d %s\r\nContent-Type: text/xml\r\n"
         "Content-Length: %u\r\nConnection: close\r\n\r\n",
         status, status == 200 ? "OK" : "Error", (unsigned)strlen(body));
   send(fd, head, n, 0);
   send(fd, body, (int)strlen(body), 0);
}

static void gateway_thread(void *data)
{
   struct gateway *g = (struct gateway*)data;
   int i;
   for (i = 0; i < g->requests; i++)
   {
      char        req[4096];
      size_t      got = 0;
      const char *body, *action, *cl;
      size_t      want = 0;
      int         c    = (int)accept(g->fd, NULL, NULL);
      if (c < 0)
         return;
      /* read the head, then as much body as Content-Length says */
      for (;;)
      {
         int n = (int)recv(c, req + got, (int)(sizeof(req) - 1 - got), 0);
         if (n <= 0)
            break;
         got += (size_t)n;
         req[got] = '\0';
         if ((body = strstr(req, "\r\n\r\n")))
         {
            body += 4;
            cl    = strstr(req, "Content-Length: ");
            want  = cl ? (size_t)atoi(cl + 16) : 0;
            if (got - (size_t)(body - req) >= want)
               break;
         }
      }
      req[got] = '\0';
      body     = strstr(req, "\r\n\r\n");
      body     = body ? body + 4 : "";
      action   = strstr(req, "#");
      g->last_action[0] = '\0';
      if (action && !strncmp(req, "POST", 4))
      {
         size_t n = strcspn(action + 1, "\"");
         if (n < sizeof(g->last_action))
         {
            memcpy(g->last_action, action + 1, n);
            g->last_action[n] = '\0';
         }
      }
      strncpy(g->last_body, body, sizeof(g->last_body) - 1);
      if (!strncmp(req, "GET /desc.xml", 13))
         respond(c, 200, desc_xml);
      else if (strncmp(req, "POST /ctl/IPConn ", 17))
         respond(c, 500, "<error/>");
      else if (!strcmp(g->last_action, "GetExternalIPAddress"))
         respond(c, 200, ext_xml);
      else if (!strcmp(g->last_action, "AddPortMapping")
            || !strcmp(g->last_action, "DeletePortMapping"))
         respond(c, 200, "<ok/>");
      else
         respond(c, 500, "<error/>");
      socket_close(c);
   }
}

int main(void)
{
   struct net_upnp_gateway gw;
   struct gateway          g;
   struct sockaddr_in      addr;
   socklen_t               alen = sizeof(addr);
   sthread_t              *t;
   char                    s[256];
   char                    url[128];
   unsigned                port;

   /* parsers */
   CHECK(net_upnp_ssdp_location("HTTP/1.1 200 OK\r\nST: x\r\n"
         "location:   http://10.0.0.1:5000/rootDesc.xml \r\n\r\n", 77,
         s, sizeof(s)) && !strcmp(s, "http://10.0.0.1:5000/rootDesc.xml"),
         "SSDP LOCATION, any case, trimmed");
   CHECK(!net_upnp_ssdp_location("HTTP/1.1 200 OK\r\n\r\n", 19, s, sizeof(s)),
         "SSDP without LOCATION");

   memset(&gw, 0, sizeof(gw));
   CHECK(net_upnp_parse_description(desc_xml, "http://10.0.0.1:5000/rootDesc.xml",
         &gw), "description: the nested WANIPConnection");
   CHECK(!strcmp(gw.control_url, "http://10.0.0.1:5000/ctl/IPConn"),
         "description: relative controlURL made absolute");
   CHECK(!strcmp(gw.service_type,
         "urn:schemas-upnp-org:service:WANIPConnection:1"),
         "description: service type");
   CHECK(!net_upnp_parse_description("<root><service><serviceType>"
         "urn:x:service:Other:1</serviceType><controlURL>/a</controlURL>"
         "</service></root>", "http://h/", &gw),
         "description without a WAN connection service");
   CHECK(net_upnp_parse_external_address(ext_xml, s, sizeof(s))
         && !strcmp(s, "203.0.113.7"), "external address");
   CHECK(!net_upnp_parse_external_address("<a><NewExternalIPAddress>bogus"
         "</NewExternalIPAddress></a>", s, sizeof(s)),
         "external address that is not one");

   /* operations against the fake gateway */
   if (!network_init())
   {
      printf("net_upnp_test: SKIP (no network)\n");
      return 0;
   }
   net_http_init();
   memset(&g, 0, sizeof(g));
   g.fd = (int)socket(AF_INET, SOCK_STREAM, 0);
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (g.fd < 0 || bind(g.fd, (struct sockaddr*)&addr, sizeof(addr))
         || getsockname(g.fd, (struct sockaddr*)&addr, &alen)
         || listen(g.fd, 4))
   {
      printf("net_upnp_test: FAIL (no loopback listener)\n");
      return 1;
   }
   port       = ntohs(addr.sin_port);
   g.requests = 6;
   t          = sthread_create(gateway_thread, &g);

   snprintf(url, sizeof(url), "http://127.0.0.1:%u/desc.xml", port);

   /* a gateway found before, by the description URL discovery kept */
   CHECK(net_upnp_gateway_from_url(&gw, url, 5000)
         && !strcmp(gw.desc_url, url)
         && strstr(gw.control_url, "/ctl/IPConn")
         && !strcmp(gw.lan_addr, "127.0.0.1"), "gateway from its URL");
   CHECK(!net_upnp_gateway_from_url(&gw, "ftp://127.0.0.1/desc.xml", 5000)
         && !gw.control_url[0], "gateway from a URL that is not http");

   memset(&gw, 0, sizeof(gw));
   {
      /* what discovery does once SSDP has named the description */
      CHECK(net_upnp_parse_description(desc_xml, url, &gw),
            "description of the fake gateway");
      strcpy(gw.lan_addr, "192.168.1.10");
   }
   CHECK(net_upnp_external_address(&gw, s, sizeof(s), 5000)
         && !strcmp(s, "203.0.113.7"), "GetExternalIPAddress");
   CHECK(net_upnp_add_port_mapping(&gw, 3658, true, "a<b", 5000),
         "AddPortMapping");
   CHECK(!strcmp(g.last_action, "AddPortMapping")
         && strstr(g.last_body, "<NewExternalPort>3658</NewExternalPort>")
         && strstr(g.last_body, "<NewProtocol>UDP</NewProtocol>")
         && strstr(g.last_body, "<NewInternalClient>192.168.1.10</NewInternalClient>")
         && strstr(g.last_body, "a&lt;b"), "AddPortMapping request");
   CHECK(net_upnp_add_port_mapping_ext(&gw, 3700, 3658, false, "x", 5000)
         && strstr(g.last_body, "<NewExternalPort>3700</NewExternalPort>")
         && strstr(g.last_body, "<NewInternalPort>3658</NewInternalPort>")
         && strstr(g.last_body, "<NewProtocol>TCP</NewProtocol>"),
         "AddPortMapping to another external port");
   CHECK(net_upnp_delete_port_mapping(&gw, 3658, true, 5000),
         "DeletePortMapping");
   CHECK(strstr(g.last_body, "<NewExternalPort>3658</NewExternalPort>") != NULL,
         "DeletePortMapping request");
   /* the fourth request: the gateway refuses it */
   snprintf(gw.control_url, sizeof(gw.control_url),
         "http://127.0.0.1:%u/nope", port);
   CHECK(!net_upnp_external_address(&gw, s, sizeof(s), 5000),
         "an error status is a failure");

   sthread_join(t);
   socket_close(g.fd);
   net_http_deinit();
   printf("net_upnp_test: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
   return fails ? 1 : 0;
}

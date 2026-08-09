#import <Foundation/Foundation.h>
#import <Foundation/NSNetServices.h>

#import "netplay_private.h"

#import "content.h"
#import "../../frontend/frontend_driver.h"
#import "../../verbosity.h"
#import "paths.h"
#import "version.h"

#define NETPLAY_MDNS_TYPE "_ra_netplay._tcp"

@interface NetplayBonjourMan : NSObject<NSNetServiceDelegate, NSNetServiceBrowserDelegate>

@property (strong, nonatomic) NSNetService *service;
@property (strong, nonatomic) NSNetServiceBrowser *browser;
@property (strong, atomic) NSMutableArray<NSNetService*> *services;

+ (NetplayBonjourMan*)shared;
- (void)publish:(netplay_t *)netplay;
- (void)unpublish;
- (void)browse;
- (void)finishBrowsing:(net_driver_state_t *)net_st;
- (void)suspend;

@end

static NetplayBonjourMan *nbm_instance;

@implementation NetplayBonjourMan

#pragma mark - (Semi-)Public API

+ (NetplayBonjourMan*)shared
{
    if (!nbm_instance)
        nbm_instance = [[NetplayBonjourMan alloc] init];
    return nbm_instance;
}

- (void)publish:(netplay_t *)netplay
{
    RARCH_LOG("[Bonjour] Publishing netplay service on port %d\n", netplay->tcp_port);
    self.service = [[NSNetService alloc] initWithDomain:@"" type:@NETPLAY_MDNS_TYPE name:@"" port:netplay->tcp_port];
    [self.service setTXTRecordData:[self TXTdataFromNetplay:netplay]];
    [self.service setDelegate:self];
    [self.service publish];
}

- (void)unpublish
{
    RARCH_LOG("[Bonjour] Unpublishing netplay service\n");
    [self.service stop];
    self.service = nil;
}

- (void)browse
{
    RARCH_LOG("[Bonjour] Starting netplay service discovery\n");
    /* The services array is only ever touched from the main queue: the
     * browser's delegate callbacks add to and remove from it there, and
     * -finishBrowsing: takes its snapshot there.  Creating it here, off
     * the main thread, would leave -didFindService: appending to an
     * array the worker thread is still publishing. */
    dispatch_async(dispatch_get_main_queue(), ^{
        self.services = [NSMutableArray arrayWithCapacity: 0];
        self.browser  = [[NSNetServiceBrowser alloc] init];
        [self.browser setDelegate:self];
        [self.browser searchForServicesOfType:@NETPLAY_MDNS_TYPE inDomain:@""];
    });
}

/* Copy one TXT record value into a fixed-size field.
 *
 * NSData contents are neither NUL-terminated nor bounded by the
 * destination, so strlcpy() cannot be handed the value directly: it
 * walks the source looking for a terminator (running off the end of
 * the NSData) and it takes the *destination* size, not the source
 * length.  Copy a bounded prefix and terminate it ourselves.
 *
 * A missing key yields a nil NSData, whose -bytes is NULL; that has to
 * degrade to an empty string rather than being dereferenced. */
static void txt_field_copy(char *dst, size_t dst_size, NSData *value)
{
    const char *src;
    size_t len;

    if (!dst_size)
        return;

    *dst = '\0';

    if (!value)
        return;
    if (!(src = (const char*)[value bytes]))
        return;

    len = (size_t)[value length];
    if (len >= dst_size)
        len = dst_size - 1;
    /* The value may itself contain an embedded NUL; stop there. */
    {
        const char *nul = (const char*)memchr(src, '\0', len);
        if (nul)
            len = (size_t)(nul - src);
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* Resolve one of a service's addresses to a numeric IPv4 string.
 *
 * -[NSNetService addresses] hands back NSData objects sized to the
 * concrete sockaddr (16 bytes for sockaddr_in, 28 for sockaddr_in6),
 * not to sockaddr_storage.  addr_6to4() rewrites its argument with a
 * sizeof(struct sockaddr_storage) memset, so it must be given a real
 * sockaddr_storage of our own - passing the NSData buffer both
 * overruns it by 100 bytes and writes through a pointer to immutable
 * NSData contents.  Likewise getnameinfo() wants the true address
 * length, not the storage size. */
static bool srv_address_to_string(NSNetService *srv, char *address,
                                  size_t address_size)
{
    for (NSData *addr_data in [srv addresses])
    {
        struct sockaddr_storage their_addr;
        socklen_t addr_len = (socklen_t)[addr_data length];

        if (!addr_len || addr_len > (socklen_t)sizeof(their_addr))
            continue;

        memset(&their_addr, 0, sizeof(their_addr));
        memcpy(&their_addr, [addr_data bytes], addr_len);

        if (!addr_6to4(&their_addr))
            continue;
        /* addr_6to4() may have rewritten a v4-mapped v6 address into a
         * shorter AF_INET one. */
        if (their_addr.ss_family == AF_INET)
            addr_len = (socklen_t)sizeof(struct sockaddr_in);

        if (!getnameinfo_retro((struct sockaddr*)&their_addr, addr_len,
                               address, (socklen_t)address_size, NULL, 0,
                               NI_NUMERICHOST))
            return address[0] != '\0';
    }

    return false;
}

- (void)finishBrowsing:(net_driver_state_t *)net_st
{
    /* -[NSNetServiceBrowser stop] and the services array both belong to
     * the main run loop, which is where the browser was scheduled and
     * where its delegate callbacks mutate self.services.  This method
     * runs on the task worker thread, so enumerating the live array
     * here races -didFindService: (mutation during fast enumeration
     * raises NSGenericException).  Stop the browser on its own queue and
     * enumerate a snapshot. */
    __block NSArray<NSNetService*> *services = nil;
    void (^stop_and_snapshot)(void) = ^{
        [self.browser stop];
        self.browser  = nil;
        services      = [self.services copy];
        self.services = nil;
    };

    /* With the non-threaded task queue the handler - and therefore this
     * function - already runs on the main thread, where dispatch_sync()
     * onto the main queue would deadlock. */
    if ([NSThread isMainThread])
        stop_and_snapshot();
    else
        dispatch_sync(dispatch_get_main_queue(), stop_and_snapshot);

    for (NSNetService *srv in services)
    {
        bool known = false;
        long port;
        size_t iter;
        char address[16];

        if (![srv.addresses count] || [srv port] <= 0 || ![srv TXTRecordData])
            continue;

        NSDictionary<NSString*,NSData*> *txt = [NSNetService dictionaryFromTXTRecordData:[srv TXTRecordData]];
        if (!txt)
            continue;

        if (!srv_address_to_string(srv, address, sizeof(address)))
            continue;

        /* Make sure we don't already know about it */
        port = [srv port];
        for (iter = 0; iter < net_st->discovered_hosts.size; iter++)
        {
            if (     port == net_st->discovered_hosts.hosts[iter].port
                  && string_is_equal(address,
                        net_st->discovered_hosts.hosts[iter].address))
            {
                known = true;
                break;
            }
        }
        if (known)
            continue;

        if (net_st->discovered_hosts.size >= NETPLAY_MAX_DISCOVERED_HOSTS)
            break;

        /* Allocate space for it */
        if (net_st->discovered_hosts.size >= net_st->discovered_hosts.allocated)
        {
            if (!net_st->discovered_hosts.size)
            {
                net_st->discovered_hosts.hosts = (struct netplay_host*)
                malloc(sizeof(*net_st->discovered_hosts.hosts));
                if (!net_st->discovered_hosts.hosts)
                    return;
                net_st->discovered_hosts.allocated = 1;
            }
            else
            {
                size_t new_allocated = net_st->discovered_hosts.allocated + 4;
                struct netplay_host *new_hosts = (struct netplay_host*)realloc(
                    net_st->discovered_hosts.hosts,
                    new_allocated * sizeof(*new_hosts));

                if (!new_hosts)
                {
                    free(net_st->discovered_hosts.hosts);
                    memset(&net_st->discovered_hosts, 0,
                           sizeof(net_st->discovered_hosts));

                    return;
                }

                net_st->discovered_hosts.allocated = new_allocated;
                net_st->discovered_hosts.hosts     = new_hosts;
            }
        }

        struct netplay_host *host = &net_st->discovered_hosts.hosts[net_st->discovered_hosts.size++];
        unsigned int content_crc  = 0;
        char crc_str[16];

        /* -scanHexInt: leaves its output untouched when the string does
         * not parse (and the string is nil when the key is absent), so
         * content_crc has to start from a defined value. */
        txt_field_copy(crc_str, sizeof(crc_str), txt[@"content_crc"]);
        if (*crc_str)
        {
            NSScanner *scanner = [NSScanner scannerWithString:
                [NSString stringWithUTF8String:crc_str]];
            if (![scanner scanHexInt:&content_crc])
                content_crc = 0;
        }
        host->content_crc = (int)ntohl(content_crc);
        host->port        = (int)port;

        strlcpy(host->address, address, sizeof(host->address));
        txt_field_copy(host->nick, sizeof(host->nick), txt[@"nick"]);
        txt_field_copy(host->frontend, sizeof(host->frontend),
            txt[@"frontend"]);
        txt_field_copy(host->core, sizeof(host->core), txt[@"core"]);
        txt_field_copy(host->core_version, sizeof(host->core_version),
            txt[@"core_version"]);
        txt_field_copy(host->retroarch_version,
            sizeof(host->retroarch_version), txt[@"retroarch_version"]);
        txt_field_copy(host->content, sizeof(host->content), txt[@"content"]);
        txt_field_copy(host->subsystem_name, sizeof(host->subsystem_name),
            txt[@"subsystem_name"]);

        {
            char flag[8];

            txt_field_copy(flag, sizeof(flag), txt[@"has_password"]);
            host->has_password = string_is_equal(flag, "true");
            txt_field_copy(flag, sizeof(flag), txt[@"has_spectate_password"]);
            host->has_spectate_password = string_is_equal(flag, "true");
        }
    }
}

#pragma mark - Browse helper functions

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
           didFindService:(NSNetService *)service
               moreComing:(BOOL)moreComing
{
    [service resolveWithTimeout:0.9f];
    [self.services addObject:service];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
         didRemoveService:(NSNetService *)service
               moreComing:(BOOL)moreComing
{
    [self.services removeObject:service];
}

#pragma mark - Publish helper functions

- (NSData *)TXTdataFromNetplay:(netplay_t *)netplay
{
    return [NSNetService dataFromTXTRecordDictionary:@{
        @"content_crc": [self content_crc],
        @"nick": [self nick:netplay],
        @"frontend": [self frontend],
        @"core": [self core],
        @"core_version": [self core_version],
        @"retroarch_version": [self retroarch_version],
        @"content": [self content],
        @"subsystem_name": [self subsystem_name],
        @"has_password": [self has_password],
        @"has_spectate_password": [self has_spectate_password]
    }];
}

- (NSData *)content_crc
{
    uint32_t crc = 0;
    struct string_list *subsystem = path_get_subsystem_list();
    if (!subsystem || subsystem->size <= 0)
        crc = netplay_content_crc();
    return [[NSString stringWithFormat:@"%08x", (uint32_t)htonl(crc)] dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSData *)nick:(netplay_t *)netplay
{
    return [[NSData alloc] initWithBytes:netplay->nick length:strlen(netplay->nick)];
}

- (NSData *)frontend
{
    char frontend_architecture_tmp[24];
    const frontend_ctx_driver_t *frontend_drv;

    frontend_drv = (const frontend_ctx_driver_t*)
        frontend_driver_get_cpu_architecture_str(frontend_architecture_tmp,
                                                 sizeof(frontend_architecture_tmp));
    NSString *frontend;
    if (frontend_drv)
        frontend = [NSString stringWithFormat:@"%s %s", frontend_drv->ident, frontend_architecture_tmp];
    else
        frontend = @"N/A";
    return [frontend dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSData *)core
{
    struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
    return [[NSData alloc] initWithBytes:system->library_name length:strlen(system->library_name)];
}

- (NSData *)core_version
{
    struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
    return [[NSData alloc] initWithBytes:system->library_version length:strlen(system->library_version)];
}

- (NSData *)retroarch_version
{
    return [[NSData alloc] initWithBytes:PACKAGE_VERSION length:strlen(PACKAGE_VERSION)];
}

- (NSData *)content
{
    struct string_list *subsystem = path_get_subsystem_list();
    if (subsystem && subsystem->size > 0)
    {
        unsigned i;
        NSMutableData *data = [[NSMutableData alloc] init];

        for (i = 0;;)
        {
            const char *pb = path_basename(subsystem->elems[i].data);
            [data appendBytes:pb length:strlen(pb)];
            if (++i >= subsystem->size)
                break;
            [data appendBytes:"|" length:strlen("|")];
        }
        return data;
    }
    else
    {
        const char *basename = path_basename(path_get(RARCH_PATH_BASENAME));
        if (!basename || !*basename)
            basename = "N/A";
        return [[NSData alloc] initWithBytes:basename length:strlen(basename)];
    }
}

- (NSData *)subsystem_name
{
    struct string_list *subsystem = path_get_subsystem_list();
    if (subsystem && subsystem->size > 0)
    {
        const char *path = path_get(RARCH_PATH_SUBSYSTEM);
        return [[NSData alloc] initWithBytes:path length:strlen(path)];
    }
    else
        return [[NSData alloc] initWithBytes:"N/A" length:3];
}

- (NSData *)has_password
{
    settings_t *settings = config_get_ptr();
    const char *has_password = !*settings->paths.netplay_password ? "false" : "true";
    return [[NSData alloc] initWithBytes:has_password length:strlen(has_password)];
}

- (NSData *)has_spectate_password
{
    settings_t *settings = config_get_ptr();
    const char *has_password = !*settings->paths.netplay_spectate_password ? "false" : "true";
    return [[NSData alloc] initWithBytes:has_password length:strlen(has_password)];
}

- (void)suspend
{
    /* Stop all Bonjour services to prevent XPC crashes when app is suspended.
     * Publishing service will need to be re-established by netplay code
     * if hosting is still active when app returns to foreground. */
    RARCH_LOG("[Bonjour] Suspending all netplay Bonjour services\n");
    if (self.service)
    {
        [self.service stop];
        self.service = nil;
    }
    if (self.browser)
    {
        [self.browser stop];
        self.browser = nil;
    }
}

@end

void netplay_mdns_publish(netplay_t *netplay)
{
    [[NetplayBonjourMan shared] publish:netplay];
}

void netplay_mdns_unpublish(void)
{
    [[NetplayBonjourMan shared] unpublish];
}

void netplay_mdns_start_discovery(void)
{
    [[NetplayBonjourMan shared] browse];
}

void netplay_mdns_finish_discovery(net_driver_state_t *net_st)
{
    [[NetplayBonjourMan shared] finishBrowsing:net_st];
}

void netplay_mdns_suspend(void)
{
    [[NetplayBonjourMan shared] suspend];
}

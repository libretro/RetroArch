#import <Foundation/Foundation.h>
#import <Foundation/NSNetServices.h>

#import "netplay_private.h"

#import "content.h"
#import "../../frontend/frontend_driver.h"
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
    self.service = [[NSNetService alloc] initWithDomain:@"" type:@NETPLAY_MDNS_TYPE name:@"" port:netplay->tcp_port];
    [self.service setTXTRecordData:[self TXTdataFromNetplay:netplay]];
    [self.service setDelegate:self];
    [self.service publish];
}

- (void)unpublish
{
    [self.service stop];
    self.service = nil;
}

- (void)browse
{
    self.services = [NSMutableArray arrayWithCapacity: 0];
    dispatch_async(dispatch_get_main_queue(), ^{
        self.browser = [[NSNetServiceBrowser alloc] init];
        [self.browser setDelegate:self];
        [self.browser searchForServicesOfType:@NETPLAY_MDNS_TYPE inDomain:@""];
    });
}

- (void)finishBrowsing:(net_driver_state_t *)net_st
{
    [self.browser stop];
    for (NSNetService *srv in self.services)
    {
        if (![srv.addresses count] || [srv port] <= 0 || ![srv TXTRecordData])
            continue;

        NSDictionary<NSString*,NSData*> *txt = [NSNetService dictionaryFromTXTRecordData:[srv TXTRecordData]];
        if (!txt)
            continue;

        char address[16] = {0};
        for (NSData *addr_data in [srv addresses])
        {
            struct sockaddr_storage *their_addr = (struct sockaddr_storage *)[addr_data bytes];
            if (!addr_6to4(their_addr))
                continue;
            if (!getnameinfo_retro((struct sockaddr*)their_addr, sizeof(struct sockaddr_storage),
                                   address, sizeof(address), NULL, 0, NI_NUMERICHOST))
                break;
        }
        if (!address[0])
            continue;

        /* Make sure we don't already know about it */
        long port = [srv port];
        size_t iter;
        for (iter = 0; iter < net_st->discovered_hosts.size; iter++)
            if (port != net_st->discovered_hosts.hosts[iter].port ||
                !string_is_equal(address, net_st->discovered_hosts.hosts[iter].address))
                continue;

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

        NSString *crc = [[NSString alloc] initWithData:txt[@"content_crc"] encoding:NSUTF8StringEncoding];
        NSScanner *scanner = [NSScanner scannerWithString:crc];
        unsigned int content_crc;
        [scanner scanHexInt:&content_crc];
        host->content_crc = (int)ntohl(content_crc);
        host->port        = (int)port;

        strlcpy(host->address, address, sizeof(host->address));
        strlcpy(host->nick, [txt[@"nick"] bytes], [txt[@"nick"] length] + 1);
        strlcpy(host->frontend, [txt[@"frontend"] bytes], [txt[@"frontend"] length] + 1);
        strlcpy(host->core, [txt[@"core"] bytes], [txt[@"core"] length] + 1);
        strlcpy(host->core_version, [txt[@"core_version"] bytes], [txt[@"core_version"] length] + 1);
        strlcpy(host->retroarch_version, [txt[@"retroarch_version"] bytes], [txt[@"retroarch_version"] length] + 1);
        strlcpy(host->content, [txt[@"content"] bytes], [txt[@"content"] length] + 1);
        strlcpy(host->subsystem_name, [txt[@"subsystem_name"] bytes], [txt[@"subsystem_name"] length] + 1);

        host->has_password = [[[NSString alloc] initWithData:txt[@"has_password"] encoding:NSUTF8StringEncoding] isEqualToString:@"true"];
        host->has_spectate_password = [[[NSString alloc] initWithData:txt[@"has_spectate_password"] encoding:NSUTF8StringEncoding] isEqualToString:@"true"];
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
        crc = content_get_crc();
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
        if (string_is_empty(basename))
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
    const char *has_password = string_is_empty(settings->paths.netplay_password) ? "false" : "true";
    return [[NSData alloc] initWithBytes:has_password length:strlen(has_password)];
}

- (NSData *)has_spectate_password
{
    settings_t *settings = config_get_ptr();
    const char *has_password = string_is_empty(settings->paths.netplay_spectate_password) ? "false" : "true";
    return [[NSData alloc] initWithBytes:has_password length:strlen(has_password)];
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

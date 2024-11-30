//
//  WebServer.m
//  MAME4iOS
//
//  Created by Yoshi Sugawara on 1/15/19.
//  Copyright © 2019 Seleuco. All rights reserved.
//

#import "WebServer.h"

@implementation WebServer

#pragma mark - singleton method

+(WebServer*)sharedInstance {
    static dispatch_once_t predicate = 0;
    static id sharedObject = nil;
    dispatch_once(&predicate, ^{
       sharedObject = [[self alloc] init];
    });
    return sharedObject;
}

#pragma mark Init

-(instancetype)init {
    if ( self = [super init] ) {
#if TARGET_OS_IOS
        NSString* docsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
#elif TARGET_OS_TV
        NSString* docsPath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
#endif
        docsPath = [docsPath stringByAppendingPathComponent:@"RetroArch"];
        _webUploader = [[GCDWebUploader alloc] initWithUploadDirectory:docsPath];
        _webUploader.allowHiddenItems = YES;
        _webDAVServer = [[GCDWebDAVServer alloc] initWithUploadDirectory:docsPath];
        _webDAVServer.allowHiddenItems = YES;
    }
    return self;
}

-(void)startServers {
    if ( _webDAVServer.isRunning ) {
        [_webDAVServer stop];
    }
    NSDictionary *webDAVSeverOptions = @{
        GCDWebServerOption_ServerName : @"RetroArch",
        GCDWebServerOption_BonjourName : @"RetroArch",
        GCDWebServerOption_BonjourType : @"_webdav._tcp",
        GCDWebServerOption_Port : @(8080)
    };
    [_webDAVServer startWithOptions:webDAVSeverOptions error:nil];

    if ( _webUploader.isRunning ) {
        [_webUploader stop];
    }
    NSDictionary *webSeverOptions = @{
        GCDWebServerOption_ServerName : @"RetroArch",
        GCDWebServerOption_BonjourName : @"RetroArch",
        GCDWebServerOption_BonjourType : @"_http._tcp",
        GCDWebServerOption_Port : @(80)
    };
    [_webUploader startWithOptions:webSeverOptions error:nil];
}

-(void)stopServers {
    [_webUploader stop];
}

@end

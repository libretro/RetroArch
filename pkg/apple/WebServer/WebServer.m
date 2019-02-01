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
        _webUploader = [[GCDWebUploader alloc] initWithUploadDirectory:docsPath];
        _webUploader.allowHiddenItems = YES;
    }
    return self;
}

-(void)startUploader {
    if ( _webUploader.isRunning ) {
        [_webUploader stop];
    }
    [_webUploader start];
}

-(void)stopUploader {
    [_webUploader stop];
}

@end

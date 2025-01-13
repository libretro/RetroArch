//
//  WebServer.h
//  MAME4iOS
//
//  Created by Yoshi Sugawara on 1/15/19.
//  Copyright © 2019 Seleuco. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "GCDWebDAVServer.h"
#import "GCDWebUploader.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebServer : NSObject

@property (nonatomic,readonly,strong) GCDWebDAVServer* webDAVServer;
@property (nonatomic,readonly,strong) GCDWebUploader* webUploader;

+(WebServer*)sharedInstance;

-(void)startServers;
-(void)stopServers;

@end

NS_ASSUME_NONNULL_END

//
//  WebServer.h
//  MAME4iOS
//
//  Created by Yoshi Sugawara on 1/15/19.
//  Copyright © 2019 Seleuco. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "GCDWebUploader.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebServer : NSObject

@property (nonatomic,readonly,strong) GCDWebUploader* webUploader;

+(WebServer*)sharedInstance;

-(void)startUploader;
-(void)stopUploader;

@end

NS_ASSUME_NONNULL_END

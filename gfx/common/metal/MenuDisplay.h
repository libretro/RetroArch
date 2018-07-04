//
// Created by Stuart Carnie on 6/24/18.
//

#import <Foundation/Foundation.h>
#import "ShaderTypes.h"

@class Context;
@class MetalDriver;

@interface MenuDisplay : NSObject

@property (nonatomic, readwrite) BOOL blend;
@property (nonatomic, readwrite) MTLClearColor clearColor;

- (instancetype)initWithDriver:(MetalDriver *)driver;
- (void)drawPipeline:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video;
- (void)draw:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video;

#pragma mark - static methods

+ (const float *)defaultVertices;
+ (const float *)defaultTexCoords;
+ (const float *)defaultColor;


@end

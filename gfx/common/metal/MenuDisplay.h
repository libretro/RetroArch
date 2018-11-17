//
// Created by Stuart Carnie on 6/24/18.
//

#import <Foundation/Foundation.h>

@class Context;

@interface MenuDisplay : NSObject

@property (nonatomic, readwrite) BOOL blend;
@property (nonatomic, readwrite) MTLClearColor clearColor;

- (instancetype)initWithContext:(Context *)context;
- (void)drawPipeline:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video;
- (void)draw:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video;
- (void)setScissorRect:(MTLScissorRect)rect;
- (void)clearScissorRect;

#pragma mark - static methods

+ (const float *)defaultVertices;
+ (const float *)defaultTexCoords;
+ (const float *)defaultColor;

@end

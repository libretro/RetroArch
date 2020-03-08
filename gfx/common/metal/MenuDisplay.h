/*
 * Created by Stuart Carnie on 6/24/18.
 */

#import <Foundation/Foundation.h>

@class Context;

@interface MenuDisplay : NSObject

@property (nonatomic, readwrite) BOOL blend;
@property (nonatomic, readwrite) MTLClearColor clearColor;

- (instancetype)initWithContext:(Context *)context;
- (void)drawPipeline:(gfx_display_ctx_draw_t *)draw;
- (void)draw:(gfx_display_ctx_draw_t *)draw;
- (void)setScissorRect:(MTLScissorRect)rect;
- (void)clearScissorRect;

#pragma mark - static methods

+ (const float *)defaultVertices;
+ (const float *)defaultTexCoords;
+ (const float *)defaultColor;

@end

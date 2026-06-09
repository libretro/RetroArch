/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Stuart Carnie
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* MetalView is the MTKView subclass owned by the Metal driver
 * (gfx/drivers/metal.m).  Its full @implementation lives there.
 *
 * This header exists because the @interface needs to be visible to:
 *   - gfx/drivers/metal.m            (the file that defines it)
 *   - ui/drivers/ui_cocoa.m          (instantiates it as the render view)
 *   - ui/drivers/ui_cocoatouch.m     (ditto, plus calls setDrawableSize:)
 *
 * On Apple platforms all three of those .m files end up in a single
 * translation unit via griffin/griffin_objc.m, so a sharable header
 * with include guards is the only way to declare MetalView once
 * without an Obj-C duplicate-class error.  Keep this file minimal --
 * the bulk of what used to live in gfx/common/metal_common.h is now
 * inlined into metal.m where it belongs. */

#ifndef METAL_VIEW_H__
#define METAL_VIEW_H__

#import <MetalKit/MetalKit.h>

@interface MetalView : MTKView
@end

#endif

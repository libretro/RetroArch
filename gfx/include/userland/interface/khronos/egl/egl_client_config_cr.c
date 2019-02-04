/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
   A FEATURES_T is a structure represented by a bit pattern

   Constructed with FEATURES_PACK(red, green, blue, alpha, depth, stencil, multi, mask, lockable)

   Attributes are returned with
      FEATURES_UNPACK_RED(c)
      FEATURES_UNPACK_GREEN(c)
      FEATURES_UNPACK_BLUE(c)
      FEATURES_UNPACK_ALPHA(c)
      FEATURES_UNPACK_DEPTH(c)
      FEATURES_UNPACK_STENCIL(c)
      FEATURES_UNPACK_MULTI(c)
      FEATURES_UNPACK_MASK(c)
      FEATURES_UNPACK_LOCKABLE(c)

   Additionally,
      FEATURES_UNPACK_COLOR(c) = red + green + blue + alpha

   Attributes can take the following values:
      0 <= red <= 15
      0 <= green <= 15
      0 <= blue <= 15
      0 <= alpha <= 15
      0 <= depth <= 255
      0 <= stencil <= 15
      multi in {0,1}
      mask in {0,8}
      lockable in {0,1}

*/

#define FEATURES_UNPACK_RED(c)        ((EGLint)((c) >> 28 & 0xf))
#define FEATURES_UNPACK_GREEN(c)      ((EGLint)((c) >> 24 & 0xf))
#define FEATURES_UNPACK_BLUE(c)       ((EGLint)((c) >> 20 & 0xf))
#define FEATURES_UNPACK_ALPHA(c)      ((EGLint)((c) >> 16 & 0xf))
#define FEATURES_UNPACK_DEPTH(c)      ((EGLint)((c) >> 8 & 0xff))
#define FEATURES_UNPACK_STENCIL(c)    ((EGLint)((c) >> 4 & 0xf))
#define FEATURES_UNPACK_MULTI(c)      ((EGLint)((c) >> 3 & 0x1))
#define FEATURES_UNPACK_MASK(c)       ((EGLint)(((c) >> 2 & 0x1) << 3))
#define FEATURES_UNPACK_LOCKABLE(c)   ((c) >> 1 & 0x1)

#define FEATURES_UNPACK_COLOR(c)      (FEATURES_UNPACK_RED(c)+FEATURES_UNPACK_GREEN(c)+FEATURES_UNPACK_BLUE(c)+FEATURES_UNPACK_ALPHA(c))

/*
   bool egl_config_check_attribs(const EGLint *attrib_list, bool *use_red, bool *use_green, bool *use_blue, bool *use_alpha)

   Checks whether all attributes and values are valid. Returns whether the list includes each of
   EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE

   We use the table in eglGetConfigAttrib to guide us as to which values are valid.

   Implementation notes:

   -

   Preconditions:

   use_red, use_green, use_blue, use_alpha are valid pointers
   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of attribute/value pairs

   Postconditions:

   If list is valid then we return true and:
      use_red is set to true   iff attrib_list includes EGL_RED_SIZE and the corresponding value is not 0 or EGL_DONT_CARE
      use_green, use_blue, use_alpha similarly
      all attributes in list are valid
   Else we return false.

   Invariants preserved:

   -

   Invariants used:

   -
*/

bool egl_config_check_attribs(const EGLint *attrib_list, bool *use_red, bool *use_green, bool *use_blue, bool *use_alpha)
{
   if (!attrib_list)
      return true;

   while (*attrib_list != EGL_NONE) {
      EGLint name = *attrib_list++;
      EGLint value = *attrib_list++;

      if (name == EGL_RED_SIZE && value != 0 && value != EGL_DONT_CARE)
         *use_red = true;

      if (name == EGL_GREEN_SIZE && value != 0 && value != EGL_DONT_CARE)
         *use_green = true;

      if (name == EGL_BLUE_SIZE && value != 0 && value != EGL_DONT_CARE)
         *use_blue = true;

      if (name == EGL_ALPHA_SIZE && value != 0 && value != EGL_DONT_CARE)
         *use_alpha = true;

      switch (name) {
      case EGL_BUFFER_SIZE:
      case EGL_RED_SIZE:
      case EGL_GREEN_SIZE:
      case EGL_BLUE_SIZE:
      case EGL_LUMINANCE_SIZE:
      case EGL_ALPHA_SIZE:
      case EGL_ALPHA_MASK_SIZE:
         if (value != EGL_DONT_CARE && value < 0) return false;
         break;
      case EGL_BIND_TO_TEXTURE_RGB:
      case EGL_BIND_TO_TEXTURE_RGBA:
         if (value != EGL_DONT_CARE && value != EGL_FALSE && value != EGL_TRUE)
            return false;
         break;
      case EGL_COLOR_BUFFER_TYPE:
         if (value != EGL_DONT_CARE && value != EGL_RGB_BUFFER && value != EGL_LUMINANCE_BUFFER)
            return false;
         break;
      case EGL_CONFIG_CAVEAT:
         if (value != EGL_DONT_CARE && value != EGL_NONE && value != EGL_SLOW_CONFIG && value != EGL_NON_CONFORMANT_CONFIG)
            return false;
         break;
      case EGL_CONFIG_ID:
         if (value != EGL_DONT_CARE && value < 1)
            return false;
         break;
      case EGL_CONFORMANT:
         if (value != EGL_DONT_CARE && (value & ~(EGL_OPENGL_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT)))
            return false;
         break;
      case EGL_DEPTH_SIZE:
         if (value != EGL_DONT_CARE && value < 0) return false;
         break;
      case EGL_LEVEL:
         break;
      case EGL_MATCH_NATIVE_PIXMAP:
         /* 1.4 Spec is poor here - says that value has to be a valid handle, but also says that any attribute
          * value (other than EGL_LEVEL) can be EGL_DONT_CARE. It also says that the default value is EGL_NONE,
          * but that doesn't really make sense - sensible to assume that the default is EGL_DONT_CARE, and don't
          * support EGL_NONE as an explicit parameter. (Could theoretically collide with a real handle...)
          */
         if (value != EGL_DONT_CARE) {
            KHRN_IMAGE_WRAP_T image;
            if (!platform_get_pixmap_info((EGLNativePixmapType)(intptr_t)value, &image))
               return false;
            khrn_platform_release_pixmap_info((EGLNativePixmapType)(intptr_t)value, &image);
         }
         break;
      case EGL_MAX_PBUFFER_WIDTH:
      case EGL_MAX_PBUFFER_HEIGHT:
      case EGL_MAX_PBUFFER_PIXELS:
         break;
      case EGL_MAX_SWAP_INTERVAL:
      case EGL_MIN_SWAP_INTERVAL:
         if (value != EGL_DONT_CARE && value < 0) return false;
         break;
      case EGL_NATIVE_RENDERABLE:
         if (value != EGL_DONT_CARE && value != EGL_FALSE && value != EGL_TRUE)
            return false;
         break;
      case EGL_NATIVE_VISUAL_ID:
      case EGL_NATIVE_VISUAL_TYPE:
         break;
      case EGL_RENDERABLE_TYPE:
         if (value != EGL_DONT_CARE && (value & ~(EGL_OPENGL_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT)))
            return false;
         break;
      case EGL_SAMPLE_BUFFERS:
      case EGL_SAMPLES:
      case EGL_STENCIL_SIZE:
         if (value != EGL_DONT_CARE && value < 0) return false;
         break;
      case EGL_SURFACE_TYPE:
      {
         int valid_bits = EGL_WINDOW_BIT|EGL_PIXMAP_BIT|EGL_PBUFFER_BIT|
            EGL_MULTISAMPLE_RESOLVE_BOX_BIT|EGL_SWAP_BEHAVIOR_PRESERVED_BIT|
            EGL_VG_COLORSPACE_LINEAR_BIT|EGL_VG_ALPHA_FORMAT_PRE_BIT;
#if EGL_KHR_lock_surface
         valid_bits |= EGL_LOCK_SURFACE_BIT_KHR|EGL_OPTIMAL_FORMAT_BIT_KHR;
#endif
         if (value != EGL_DONT_CARE && (value & ~valid_bits))
            return false;
         break;
      }
      case EGL_TRANSPARENT_TYPE:
         if (value != EGL_DONT_CARE && value != EGL_NONE && value != EGL_TRANSPARENT_RGB)
            return false;
         break;
      case EGL_TRANSPARENT_RED_VALUE:
      case EGL_TRANSPARENT_GREEN_VALUE:
      case EGL_TRANSPARENT_BLUE_VALUE:
         if (value != EGL_DONT_CARE && value < 0) return false;
         break;
#if EGL_KHR_lock_surface
      case EGL_MATCH_FORMAT_KHR:
         switch (value) {
         case EGL_DONT_CARE:
         case EGL_NONE:
         case EGL_FORMAT_RGB_565_EXACT_KHR:
         case EGL_FORMAT_RGB_565_KHR:
         case EGL_FORMAT_RGBA_8888_EXACT_KHR:
         case EGL_FORMAT_RGBA_8888_KHR:
            break;
         default:
            return false;
         }
         break;
#endif
#if EGL_ANDROID_recordable
      case EGL_RECORDABLE_ANDROID:
         switch (value) {
         case EGL_DONT_CARE:
         case EGL_TRUE:
         case EGL_FALSE:
            break;
         default:
            return false;
         }
         break;
#endif
      default:
         return false;
      }
   }

   return true;
}

/*
   bool less_than(int id0, int id1, bool use_red, bool use_green, bool use_blue, bool use_alpha)

   total ordering on configs; sort in

   - decreasing order of color buffer depth, ignoring components for which we have not expressed a preference
   - increasing order of color buffer depth
   - increasing order of multisample buffers
   - increasing order of depth buffer depth
   - increasing order of stencil buffer depth
   - increasing order of mask buffer depth

   Implementation notes:

   We ignore:
      EGL_CONFIG_CAVEAT (all EGL_NONE)
      EGL_COLOR_BUFFER_TYPE (all EGL_RGB_BUFFER)
      EGL_SAMPLES (if EGL_SAMPLES is 1 then all 4 else all 0)
      EGL_NATIVE_VISUAL_TYPE (all EGL_NONE)
      EGL_CONFIG_ID (already sorted in this order and sort algorithm is stable)

   Khrnonos documentation:

   Sorting of EGLConfigs
   If more than one matching EGLConfig is found, then a list of EGLConfigs
   is returned. The list is sorted by proceeding in ascending order of the ”Sort Priority”
   column of table 3.4. That is, configurations that are not ordered by a lower
   numbered rule are sorted by the next higher numbered rule.
   Sorting for each rule is either numerically Smaller or Larger as described in the
   ”Sort Order” column, or a Special sort order as described for each sort rule below:
   1. Special: by EGL CONFIG CAVEAT where the precedence is EGL NONE,
   EGL SLOW CONFIG, EGL NON CONFORMANT CONFIG.
   2. Special:
   by EGL COLOR BUFFER TYPE where the precedence is EGL RGB BUFFER,
   EGL LUMINANCE BUFFER.
   3. Special: by larger total number of color bits (for an RGB color buffer,
   this is the sum of EGL RED SIZE, EGL GREEN SIZE, EGL BLUE SIZE,
   and EGL ALPHA SIZE; for a luminance color buffer, the sum of
   EGL LUMINANCE SIZE and EGL ALPHA SIZE) [3]. If the requested number
   of bits in attrib list for a particular color component is 0 or EGL_DONT_CARE,
   then the number of bits for that component is not considered.
   4. Smaller EGL BUFFER SIZE.
   5. Smaller EGL SAMPLE BUFFERS.
   6. Smaller EGL SAMPLES.
   7. Smaller EGL DEPTH SIZE.
   8. Smaller EGL STENCIL SIZE.
   9. Smaller EGL ALPHA MASK SIZE.
   10. Special: by EGL NATIVE VISUAL TYPE (the actual sort order is
   implementation-defined, depending on the meaning of native visual types).
   11. Smaller EGL CONFIG ID (this is always the last sorting rule, and guarantees
   a unique ordering).
   EGLConfigs are not sorted with respect to the parameters
   EGL BIND TO TEXTURE RGB, EGL BIND TO TEXTURE RGBA, EGL CONFORMANT,
   EGL LEVEL, EGL NATIVE RENDERABLE, EGL MAX SWAP INTERVAL,
   EGL MIN SWAP INTERVAL, EGL RENDERABLE TYPE, EGL SURFACE TYPE,
   EGL TRANSPARENT TYPE, EGL TRANSPARENT RED VALUE,
   EGL TRANSPARENT GREEN VALUE, and EGL TRANSPARENT BLUE VALUE.

   3This rule places configs with deeper color buffers first in the list returned by eglChooseConfig.
Applications may find this counterintuitive, and need to perform additional processing on the list of
configs to find one best matching their requirements. For example, specifying RGBA depths of 5651
could return a list whose first config has a depth of 8888.

   Preconditions:

   0 <= id0 < EGL_MAX_CONFIGS
   0 <= id1 < EGL_MAX_CONFIGS

   Postconditions:

   -

   Invariants preserved:

   -

   Invariants used:

   -
*/

static bool less_than(int id0, int id1, bool use_red, bool use_green, bool use_blue, bool use_alpha)
{
   FEATURES_T features0 = formats[id0].features;
   FEATURES_T features1 = formats[id1].features;

   EGLint all0 = FEATURES_UNPACK_COLOR(features0);
   EGLint all1 = FEATURES_UNPACK_COLOR(features1);

   EGLint multi0 = FEATURES_UNPACK_MULTI(features0);
   EGLint multi1 = FEATURES_UNPACK_MULTI(features1);

   EGLint depth0 = FEATURES_UNPACK_DEPTH(features0);
   EGLint depth1 = FEATURES_UNPACK_DEPTH(features1);

   EGLint stencil0 = FEATURES_UNPACK_STENCIL(features0);
   EGLint stencil1 = FEATURES_UNPACK_STENCIL(features1);

   EGLint mask0 = FEATURES_UNPACK_MASK(features0);
   EGLint mask1 = FEATURES_UNPACK_MASK(features1);

   int used0 = 0;
   int used1 = 0;

   if (use_red) {
      used0 += FEATURES_UNPACK_RED(features0);
      used1 += FEATURES_UNPACK_RED(features1);
   }
   if (use_green) {
      used0 += FEATURES_UNPACK_GREEN(features0);
      used1 += FEATURES_UNPACK_GREEN(features1);
   }
   if (use_blue) {
      used0 += FEATURES_UNPACK_BLUE(features0);
      used1 += FEATURES_UNPACK_BLUE(features1);
   }
   if (use_alpha) {
      used0 += FEATURES_UNPACK_ALPHA(features0);
      used1 += FEATURES_UNPACK_ALPHA(features1);
   }

   return used0 > used1    ||    (used0 == used1 &&
      (all0 < all1         ||     (all0 == all1 &&
      (multi0 < multi1     ||   (multi0 == multi1 &&
      (depth0 < depth1     ||   (depth0 == depth1 &&
      (stencil0 < stencil1 || (stencil0 == stencil1 &&
      (mask0 < mask1))))))))));
}

/*
   void egl_config_sort(int *ids, EGLBoolean use_red, EGLBoolean use_green, EGLBoolean use_blue, EGLBoolean use_alpha)

   Sorts a list of EGL_CONFIG_IDs

   Implementation notes:

   Uses bubble sort

   Preconditions:

   ids is a pointer to EGL_MAX_CONFIGS elements
   0 <= ids[i] < EGL_MAX_CONFIG for 0 <= i < EGL_MAX_CONFIG

   Postconditions:

   ids is a permutation of the original

   Invariants preserved:

   -

   Invariants used:

   -
*/

void egl_config_sort(int *ids, bool use_red, bool use_green, bool use_blue, bool use_alpha)
{
   int i, j;

   for (i = 1; i < EGL_MAX_CONFIGS; i++)
      for (j = 0; j < EGL_MAX_CONFIGS - i; j++)
         if (less_than(ids[j + 1], ids[j], use_red, use_green, use_blue, use_alpha)) {
            int temp = ids[j];
            ids[j] = ids[j + 1];
            ids[j + 1] = temp;
         }
}

/*
   bool egl_config_get_attrib(int id, EGLint attrib, EGLint *value)

   Returns the value of the given attribute of the given config. Returns false
   if there is no such attribute.

   Implementation notes:

   We match EGL_MATCH_NATIVE_PIXMAP here
   too, because it *is* a valid attribute according to eglGetConfigAttrib
   (even though it doesn't return anything meaningful) and we need it for
   egl_config_filter.

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS
   value is a valid pointer

   Postconditions:

   If attrib is a valid attribute then true is returned and *value is set
   Else false is returned

   Invariants preserved:

   -

   Invariants used:

   -
*/

bool egl_config_get_attrib(int id, EGLint attrib, EGLint *value)
{
   FEATURES_T features = formats[id].features;

   switch (attrib) {
   case EGL_BUFFER_SIZE:
      *value = FEATURES_UNPACK_COLOR(features);
      return true;
   case EGL_RED_SIZE:
      *value = FEATURES_UNPACK_RED(features);
      return true;
   case EGL_GREEN_SIZE:
      *value = FEATURES_UNPACK_GREEN(features);
      return true;
   case EGL_BLUE_SIZE:
      *value = FEATURES_UNPACK_BLUE(features);
      return true;
   case EGL_LUMINANCE_SIZE:
      *value = 0;
      return true;
   case EGL_ALPHA_SIZE:
      *value = FEATURES_UNPACK_ALPHA(features);
      return true;
   case EGL_ALPHA_MASK_SIZE:
      *value = FEATURES_UNPACK_MASK(features);
      return true;
   case EGL_BIND_TO_TEXTURE_RGB:
      *value = bindable_rgb(features);
      return true;
   case EGL_BIND_TO_TEXTURE_RGBA:
      *value = bindable_rgba(features);
      return true;
   case EGL_COLOR_BUFFER_TYPE:
      *value = EGL_RGB_BUFFER;
      return true;
   case EGL_CONFIG_CAVEAT:
      *value = EGL_NONE;
      return true;
   case EGL_CONFIG_ID:
      *value = (EGLint)(uintptr_t)egl_config_from_id(id);
      return true;
   case EGL_CONFORMANT:
      *value = egl_config_get_api_conformance(id);
      return true;
   case EGL_DEPTH_SIZE:
      *value = FEATURES_UNPACK_DEPTH(features);
      return true;
   case EGL_LEVEL:
      *value = 0;
      return true;
   case EGL_MATCH_NATIVE_PIXMAP:
      *value = 0;
      return true;
   case EGL_MAX_PBUFFER_WIDTH:
      *value = EGL_CONFIG_MAX_WIDTH;
      return true;
   case EGL_MAX_PBUFFER_HEIGHT:
      *value = EGL_CONFIG_MAX_HEIGHT;
      return true;
   case EGL_MAX_PBUFFER_PIXELS:
      *value = EGL_CONFIG_MAX_WIDTH * EGL_CONFIG_MAX_HEIGHT;
      return true;
   case EGL_MAX_SWAP_INTERVAL:
      *value = EGL_CONFIG_MAX_SWAP_INTERVAL;
      return true;
   case EGL_MIN_SWAP_INTERVAL:
      *value = EGL_CONFIG_MIN_SWAP_INTERVAL;
      return true;
   case EGL_NATIVE_RENDERABLE:
      *value = EGL_TRUE;
      return true;
   case EGL_NATIVE_VISUAL_ID:
      *value = platform_get_color_format(egl_config_get_color_format(id));
      return true;
   case EGL_NATIVE_VISUAL_TYPE:
      *value = EGL_NONE;
      return true;
   case EGL_RENDERABLE_TYPE:
      *value = egl_config_get_api_support(id);
      return true;
   case EGL_SAMPLE_BUFFERS:
      *value = FEATURES_UNPACK_MULTI(features);
      return true;
   case EGL_SAMPLES:
      *value = FEATURES_UNPACK_MULTI(features) * 4;
      return true;
   case EGL_STENCIL_SIZE:
      *value = FEATURES_UNPACK_STENCIL(features);
      return true;
   case EGL_SURFACE_TYPE:
      *value = (EGLint)(EGL_PBUFFER_BIT | EGL_PIXMAP_BIT | EGL_WINDOW_BIT | EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT | EGL_MULTISAMPLE_RESOLVE_BOX_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT);
#if EGL_KHR_lock_surface
      if (egl_config_is_lockable(id))
      {
         *value |= EGL_LOCK_SURFACE_BIT_KHR;
         if (egl_config_get_mapped_format(id) == egl_config_get_color_format(id))
            *value |= EGL_OPTIMAL_FORMAT_BIT_KHR;      /* Considered optimal if no format conversion needs doing. Currently all lockable surfaces are optimal */
      }
#endif
      return true;
   case EGL_TRANSPARENT_TYPE:
      *value = EGL_NONE;
      return true;
   case EGL_TRANSPARENT_RED_VALUE:
   case EGL_TRANSPARENT_GREEN_VALUE:
   case EGL_TRANSPARENT_BLUE_VALUE:
      *value = 0;
      return true;
#if EGL_KHR_lock_surface
   case EGL_MATCH_FORMAT_KHR:
      if (!egl_config_is_lockable(id))
         *value = EGL_NONE;
      else {
         switch (egl_config_get_mapped_format(id))
         {
         case RGB_565_RSO:
            *value = EGL_FORMAT_RGB_565_EXACT_KHR;
            break;
         case ARGB_8888_RSO:
            *value = EGL_FORMAT_RGBA_8888_EXACT_KHR;
            break;
         default:
            UNREACHABLE();
         }
      }
      return true;
#endif
#if EGL_ANDROID_recordable
   case EGL_RECORDABLE_ANDROID:
      *value = EGL_TRUE;
      return true;
#endif
   default:
      return false;
   }
}

/*
   bool egl_config_filter(int id, const EGLint *attrib_list)

   Returns whether the given EGL config satisfies the supplied attrib_list.

   Implementation notes:

   The following attributes:
   EGL_COLOR_BUFFER_TYPE
   EGL_LEVEL
   EGL_RENDERABLE_TYPE
   EGL_SURFACE_TYPE
   EGL_TRANSPARENT_TYPE
   (possibly EGL_MATCH_NATIVE_PIXMAP - see comment)

   have default values not equivalent to EGL_DONT_CARE. But all of our configs
   match the default value in all of these cases so we can treat the default as
   EGL_DONT_CARE for all attributes.

   Preconditions:

   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of valid attribute/value pairs
   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   -

   Invariants preserved:

   -

   Invariants used:

   -
*/

bool egl_config_filter(int id, const EGLint *attrib_list)
{
   if (!attrib_list)
      return true;

   while (*attrib_list != EGL_NONE) {
      EGLint name = *attrib_list++;
      EGLint value = *attrib_list++;
      EGLint actual_value;

      if (!egl_config_get_attrib(id, name, &actual_value) )
      {
         UNREACHABLE();
         return false;
      }

      switch (name) {
         /* Selection Criteria: AtLeast */
      case EGL_BUFFER_SIZE:
      case EGL_RED_SIZE:
      case EGL_GREEN_SIZE:
      case EGL_BLUE_SIZE:
      case EGL_LUMINANCE_SIZE:
      case EGL_ALPHA_SIZE:
      case EGL_ALPHA_MASK_SIZE:
      case EGL_DEPTH_SIZE:
      case EGL_SAMPLE_BUFFERS:
      case EGL_SAMPLES:
      case EGL_STENCIL_SIZE:
         if (value != EGL_DONT_CARE && value > actual_value)
            return false;
         break;

         /* Selection Criteria: Exact */
         /*
            Excluding EGL_TRANSPARENT_x_VALUE and EGL_MATCH_FORMAT_KHR which are listed in
            the table as Exact, but seem to have special rules attached to them.

            Excluding EGL_NATIVE_VISUAL_TYPE which is in the ignore list
            Excluding EGL_LEVEL because EGL_DONT_CARE is not allowed
         */
      case EGL_BIND_TO_TEXTURE_RGB:
      case EGL_BIND_TO_TEXTURE_RGBA:
      case EGL_COLOR_BUFFER_TYPE:
      case EGL_CONFIG_CAVEAT:
      case EGL_CONFIG_ID:
      case EGL_MAX_SWAP_INTERVAL:
      case EGL_MIN_SWAP_INTERVAL:
      case EGL_NATIVE_RENDERABLE:
      case EGL_TRANSPARENT_TYPE:
#if EGL_ANDROID_recordable
      case EGL_RECORDABLE_ANDROID:
#endif
         if (value != EGL_DONT_CARE && value != actual_value)
            return false;
         break;

      case EGL_LEVEL:
         if (value != actual_value)
            return false;
         break;

         /* Selection Criteria: Mask */
      case EGL_CONFORMANT:
      case EGL_RENDERABLE_TYPE:
      case EGL_SURFACE_TYPE:
         if (value != EGL_DONT_CARE && (value & ~actual_value))
            return false;
         break;

         /* Selection Criteria: Special */
      case EGL_MATCH_NATIVE_PIXMAP:
         if (value != EGL_DONT_CARE) { /* see comments in egl_config_check_attribs */
            EGLNativePixmapType pixmap = (EGLNativePixmapType)(intptr_t)value;
            KHRN_IMAGE_WRAP_T image;
            if (!platform_get_pixmap_info(pixmap, &image)) {
               /*
                  Not actually unreachable in theory!
                  We should have detected this in egl_config_check_attribs
                  It's possible that the validity of pixmap has changed since then however...
               */
               UNREACHABLE();
               return false;
            }
            if (!egl_config_match_pixmap_info(id, &image) ||
               !platform_match_pixmap_api_support(pixmap, egl_config_get_api_support(id)))
            {
               khrn_platform_release_pixmap_info(pixmap, &image);
               return false;
            }

            khrn_platform_release_pixmap_info(pixmap, &image);
         }
         break;
#if EGL_KHR_lock_surface
      case EGL_MATCH_FORMAT_KHR:
         if (!(value == EGL_DONT_CARE || value == actual_value
            || (value == EGL_FORMAT_RGB_565_KHR && actual_value == EGL_FORMAT_RGB_565_EXACT_KHR)
            || (value == EGL_FORMAT_RGBA_8888_KHR && actual_value == EGL_FORMAT_RGBA_8888_EXACT_KHR)))
         {
            return false;
         }
         break;
#endif

         /* Attributes we can completely ignore */
      case EGL_MAX_PBUFFER_WIDTH:
      case EGL_MAX_PBUFFER_HEIGHT:
      case EGL_MAX_PBUFFER_PIXELS:
      case EGL_NATIVE_VISUAL_ID:
         /*
         "If EGL_MAX_PBUFFER_WIDTH, EGL_MAX_PBUFFER_HEIGHT,
         EGL_MAX_PBUFFER_PIXELS, or EGL_NATIVE_VISUAL_ID are specified in
         attrib_list, then they are ignored"
         */

      case EGL_NATIVE_VISUAL_TYPE:
         /*
         "if there are no native visual types, then the EGL NATIVE VISUAL TYPE attribute is
         ignored."
         */

      case EGL_TRANSPARENT_BLUE_VALUE:
      case EGL_TRANSPARENT_GREEN_VALUE:
      case EGL_TRANSPARENT_RED_VALUE:
         /*
          "If EGL_TRANSPARENT_TYPE is set to EGL_NONE in attrib_list, then
         the EGL_TRANSPARENT_RED_VALUE, EGL_TRANSPARENT_GREEN_VALUE, and
         EGL_TRANSPARENT_BLUE_VALUE attributes are ignored."

         Possible spec deviation if EGL_TRANSPARENT_TYPE is specified as EGL_DONT_CARE
         and EGL_TRANSPARENT_*_VALUE is also specified?
         */

         break;

      default:
         UNREACHABLE();
         break;
      }
   }

   return true;
}

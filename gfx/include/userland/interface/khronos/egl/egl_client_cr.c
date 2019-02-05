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
   EGLAPI EGLint EGLAPIENTRY eglGetError(void)

   Khronos documentation:

   3.1 Errors
   Where possible, when an EGL function fails it has no side effects.
   EGL functions usually return an indicator of success or failure; either an
   EGLBoolean EGL TRUE or EGL FALSE value, or in the form of an out-of-band
   return value indicating failure, such as returning EGL NO CONTEXT instead of a requested
   context handle. Additional information about the success or failure of the
   most recent EGL function called in a specific thread, in the form of an error code,
   can be obtained by calling
   EGLint eglGetError();
   The error codes that may be returned from eglGetError, and their meanings,
   are:
   EGL SUCCESS
   Function succeeded.
   EGL NOT INITIALIZED
   EGL is not initialized, or could not be initialized, for the specified display.
   EGL BAD ACCESS
   EGL cannot access a requested resource (for example, a context is bound in
   another thread).
   EGL BAD ALLOC
   EGL failed to allocate resources for the requested operation.
   9
   10 CHAPTER 3. EGL FUNCTIONS AND ERRORS
   EGL BAD ATTRIBUTE
   An unrecognized attribute or attribute value was passed in an attribute list.
   EGL BAD CONTEXT
   An EGLContext argument does not name a valid EGLContext.
   EGL BAD CONFIG
   An EGLConfig argument does not name a valid EGLConfig.
   EGL BAD CURRENT SURFACE
   The current surface of the calling thread is a window, pbuffer, or pixmap that
   is no longer valid.
   EGL BAD DISPLAY
   An EGLDisplay argument does not name a valid EGLDisplay; or, EGL
   is not initialized on the specified EGLDisplay.
   EGL BAD SURFACE
   An EGLSurface argument does not name a valid surface (window, pbuffer,
   or pixmap) configured for rendering.
   EGL BAD MATCH
   Arguments are inconsistent; for example, an otherwise valid context requires
   buffers (e.g. depth or stencil) not allocated by an otherwise valid surface.
   EGL BAD PARAMETER
   One or more argument values are invalid.
   EGL BAD NATIVE PIXMAP
   An EGLNativePixmapType argument does not refer to a valid native
   pixmap.
   EGL BAD NATIVE WINDOW
   An EGLNativeWindowType argument does not refer to a valid native
   window.
   EGL CONTEXT LOST
   A power management event has occurred. The application must destroy all
   contexts and reinitialise client API state and objects to continue rendering,
   as described in section 2.6.
   When there is no status to return (in other words, when eglGetError is called
   as the first EGL call in a thread, or immediately after calling eglReleaseThread),
   EGL SUCCESS will be returned.

   Implementation notes:

   What should we do if eglGetError is called twice? Currently we reset the error to EGL_SUCCESS.

   Preconditions:

   -

   Postconditions:

   Result is in the list (CLIENT_THREAD_STATE_ERROR)

   Invariants preserved:

   -

   Invariants used:

   (CLIENT_THREAD_STATE_ERROR)
*/

EGLAPI EGLint EGLAPIENTRY eglGetError(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_CHECK_THREAD_STATE();

   if (thread)
   {
      EGLint result;

      vcos_assert( thread != NULL );

      result = thread->error;
      thread->error = EGL_SUCCESS;

      return result;
   }
   else
      return EGL_SUCCESS;
}

/*
   EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id)

   Khronos documentation:

   3.2 Initialization
Initialization must be performed once for each display prior to calling most other
EGL or client API functions. A display can be obtained by calling
EGLDisplay eglGetDisplay(EGLNativeDisplayType
display id);
The type and format of display id are implementation-specific, and it describes a
specific display provided by the system EGL is running on. For example, an EGL
implementation under X windows would require display id to be an X Display,
while an implementation under Microsoft Windows would require display id to be
a Windows Device Context. If display id is EGL DEFAULT DISPLAY, a default
display is returned.
If no display matching display id is available, EGL NO DISPLAY is returned;
no error condition is raised in this case.

   Implementation notes:

   We only support one display. This is assumed to have a native display_id
   of 0 (==EGL_DEFAULT_DISPLAY) and an EGLDisplay id of 1

   Preconditions:

   -

   Postconditions:

   -

   Invariants preserved:

   -

   Invariants used:

   -
*/

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_CHECK_THREAD_STATE();
   if (thread)
      thread->error = EGL_SUCCESS;

   return khrn_platform_set_display_id(display_id);
}

//eglInitialize
//eglTerminate
//eglQueryString

/*
   EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)

   Khronos documentation:

3.4.1 Querying Configurations
Use
EGLBoolean eglGetConfigs(EGLDisplay dpy,
EGLConfig *configs, EGLint config size,
EGLint *num config);
to get the list of all EGLConfigs that are available on the specified display. configs
is a pointer to a buffer containing config size elements. On success, EGL TRUE is
returned. The number of configurations is returned in num config, and elements 0
through num config - 1 of configs are filled in with the valid EGLConfigs. No
more than config size EGLConfigs will be returned even if more are available on
the specified display. However, if eglGetConfigs is called with configs = NULL,
then no configurations are returned, but the total number of configurations available
will be returned in num config.
On failure, EGL FALSE is returned. An EGL NOT INITIALIZED error is generated
if EGL is not initialized on dpy. An EGL BAD PARAMETER error is generated
if num config is NULL.

   Implementation notes:

   -

   Preconditions:

   configs is NULL or a valid pointer to config_size elements
   num_config is NULL or a valid pointer

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.
      EGL_BAD_PARAMETER             num_config is null
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Invariants preserved:

   -

   Invariants used:

   -
*/

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if (!num_config) {
         thread->error = EGL_BAD_PARAMETER;
         result = EGL_FALSE;
      } else if (!configs) {
         thread->error = EGL_SUCCESS;
         *num_config = EGL_MAX_CONFIGS;
         result = EGL_TRUE;
      } else {
         int i;
         for (i = 0; i < EGL_MAX_CONFIGS && i < config_size; i++)
            configs[i] = egl_config_from_id(i);

         thread->error = EGL_SUCCESS;
         *num_config = i;
         result = EGL_TRUE;
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

/*
   EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)

   Khronos documentation:

   Use
EGLBoolean eglChooseConfig(EGLDisplay dpy, const
EGLint *attrib list, EGLConfig *configs,
EGLint config size, EGLint *num config);
to get EGLConfigs that match a list of attributes. The return value and the meaning
of configs, config size, and num config are the same as for eglGetConfigs.
However, only configurations matching attrib list, as discussed below, will be returned.
On failure, EGL FALSE is returned. An EGL BAD ATTRIBUTE error is generated
if attrib list contains an undefined EGL attribute or an attribute value that is
unrecognized or out of range.
All attribute names in attrib list are immediately followed by the corresponding
desired value. The list is terminated with EGL NONE. If an attribute is not specified
in attrib list, then the default value (listed in Table 3.4) is used (it is said to be
specified implicitly). If EGL_DONT_CARE is specified as an attribute value, then the
attribute will not be checked. EGL_DONT_CARE may be specified for all attributes
except EGL LEVEL. If attrib list is NULL or empty (first attribute is EGL NONE),
then selection and sorting of EGLConfigs is done according to the default criteria
in Tables 3.4 and 3.1, as described below under Selection and Sorting.
Selection of EGLConfigs
Attributes are matched in an attribute-specific manner, as shown in the ”Selection
Critera” column of table 3.4. The criteria listed in the table have the following
meanings:
AtLeast Only EGLConfigs with an attribute value that meets or exceeds the
specified value are selected.
Exact Only EGLConfigs whose attribute value equals the specified value are
matched.
Mask Only EGLConfigs for which the bits set in the attribute value include all
the bits that are set in the specified value are selected (additional bits might
be set in the attribute value).
Special As described for the specific attribute.
Some of the attributes must match the specified value exactly; others, such as
EGL RED SIZE, must meet or exceed the specified minimum values.
To retrieve an EGLConfig given its unique integer ID, use the
EGL CONFIG ID attribute. When EGL CONFIG ID is specified, all other attributes
are ignored, and only the EGLConfig with the given ID is returned.
If EGL MAX PBUFFER WIDTH, EGL MAX PBUFFER HEIGHT,
EGL MAX PBUFFER PIXELS, or EGL NATIVE VISUAL ID are specified in
attrib list, then they are ignored (however, if present, these attributes must still be
Version 1.3 - December 4, 2006
3.4. CONFIGURATION MANAGEMENT 21
followed by an attribute value in attrib list). If EGL SURFACE TYPE is specified
in attrib list and the mask that follows does not have EGL WINDOW BIT set, or if
there are no native visual types, then the EGL NATIVE VISUAL TYPE attribute is
ignored.
If EGL TRANSPARENT TYPE is set to EGL NONE in attrib list, then
the EGL TRANSPARENT RED VALUE, EGL TRANSPARENT GREEN VALUE, and
EGL TRANSPARENT BLUE VALUE attributes are ignored.
If EGL MATCH NATIVE PIXMAP is specified in attrib list, it must be followed
by an attribute value which is the handle of a valid native pixmap. Only
EGLConfigs which support rendering to that pixmap will match this attribute2.
If no EGLConfig matching the attribute list exists, then the call succeeds, but
num config is set to 0.

Attribute                  Default       Selection Sort    Sort
                                         Criteria  Order   Priority
EGL_BUFFER_SIZE            0              AtLeast  Smaller 4
EGL_RED_SIZE               0              AtLeast  Special 3
EGL_GREEN_SIZE             0              AtLeast  Special 3
EGL_BLUE_SIZE              0              AtLeast  Special 3
EGL_LUMINANCE_SIZE         0              AtLeast  Special 3
EGL_ALPHA_SIZE             0              AtLeast  Special 3
EGL_ALPHA_MASK_SIZE        0              AtLeast  Smaller 9
EGL_BIND_TO_TEXTURE_RGB    EGL_DONT_CARE  Exact    None
EGL_BIND_TO_TEXTURE_RGBA   EGL_DONT_CARE  Exact    None
EGL_COLOR_BUFFER_TYPE      EGL_RGB BUFFER Exact    None    2
EGL_CONFIG_CAVEAT          EGL_DONT_CARE  Exact    Special 1
EGL_CONFIG_ID              EGL_DONT_CARE  Exact    Smaller 11 (last)
EGL_CONFORMANT             0              Mask     None
EGL_DEPTH_SIZE             0              AtLeast  Smaller 7
EGL_LEVEL                  0              Exact    None
EGL_MATCH_NATIVE_PIXMAP    EGL_NONE       Special  None
EGL_MAX_SWAP_INTERVAL      EGL_DONT_CARE  Exact    None
EGL_MIN_SWAP_INTERVAL      EGL_DONT_CARE  Exact    None
EGL_NATIVE_RENDERABLE      EGL_DONT_CARE  Exact    None
EGL_NATIVE_VISUAL_TYPE     EGL_DONT_CARE  Exact    Special 10
EGL_RENDERABLE_TYPE        EGL_OPENGL_ES_BIT Mask  None
EGL_SAMPLE_BUFFERS         0              AtLeast  Smaller 5
EGL_SAMPLES                0              AtLeast  Smaller 6
EGL_STENCIL_SIZE           0              AtLeast  Smaller 8
EGL_SURFACE_TYPE           EGL_WINDOW_BIT Mask     None
EGL_TRANSPARENT_TYPE       EGL_NONE       Exact    None
EGL_TRANSPARENT_RED_VALUE  EGL_DONT_CARE  Exact    None
EGL_TRANSPARENT_GREEN_VALUE EGL_DONT_CARE Exact    None
EGL_TRANSPARENT_BLUE_VALUE EGL_DONT_CARE  Exact    None
Table 3.4: Default values and match criteria for EGLConfig attributes.

2 The special match criteria for EGL MATCH NATIVE PIXMAP was introduced due to the
difficulty of determining an EGLConfig equivalent to a native pixmap using only color component
depths.
3This rule places configs with deeper color buffers first in the list returned by eglChooseConfig.
Applications may find this counterintuitive, and need to perform additional processing on the list of
configs to find one best matching their requirements. For example, specifying RGBA depths of 5651
could return a list whose first config has a depth of 8888.

   Implementation notes:

   Configurations are not always returned in the same order; the sort order depends on
   whether we care about EGL_RED_SIZE, EGL_GREEN_SIZE, etc. So we need to extract the information
   about which of these we care about, then pass this to a sorting function.

   Preconditions:

   configs is NULL or a valid pointer to config_size elements
   num_config is NULL or a valid pointer
   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of attribute/value pairs

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for dpy
      EGL_BAD_PARAMETER             num_config is null
      EGL_BAD_ATTRIBUTE             attrib_list contains an undefined EGL attribute
      EGL_BAD_ATTRIBUTE             attrib_list contains an attribute value that is unrecognized or out of range.
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Invariants preserved:

   -

   Invariants used:

   -
*/

static EGLBoolean choose_config(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config, bool sane)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if (!num_config) {
         thread->error = EGL_BAD_PARAMETER;
         result = EGL_FALSE;
      } else {
         /*
            check for invalid attributes, and find color components for which
            we have expressed a preference
         */

         bool use_red = false;
         bool use_green = false;
         bool use_blue = false;
         bool use_alpha = false;

         if (!egl_config_check_attribs(attrib_list, &use_red, &use_green, &use_blue, &use_alpha)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_FALSE;
         } else {

            /*
               sort configs
            */

            int ids[EGL_MAX_CONFIGS];
            int i, j;

            for (i = 0; i < EGL_MAX_CONFIGS; i++)
               ids[i] = i;

            egl_config_sort(ids,
               !sane && use_red, !sane && use_green,
               !sane && use_blue, !sane && use_alpha);

            /*
               return configs
            */

            j = 0;
            for (i = 0; i < EGL_MAX_CONFIGS; i++) {
               if (egl_config_filter(ids[i], attrib_list)) {
                  if (configs && j < config_size) {
                     configs[j] = egl_config_from_id(ids[i]);
                     j++;
                  } else if (!configs) {
                     // If configs==NULL then we count all configs
                     // Otherwise we only count the configs we return
                     j++;
                  }
               }
            }

            thread->error = EGL_SUCCESS;
            *num_config = j;
            result = EGL_TRUE;
         }
      }

      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
   return choose_config(dpy, attrib_list, configs, config_size, num_config, false);
}

#if EGL_BRCM_sane_choose_config
EGLAPI EGLBoolean EGLAPIENTRY eglSaneChooseConfigBRCM(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
   return choose_config(dpy, attrib_list, configs, config_size, num_config, true);
}
#endif

/*
   EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)

   Khronos documentation:
   3.4.3 Querying Configuration Attributes
   To get the value of an EGLConfig attribute, use
   EGLBoolean eglGetConfigAttrib(EGLDisplay dpy,
   EGLConfig config, EGLint attribute, EGLint
   *value);
   If eglGetConfigAttrib succeeds then it returns EGL_TRUE and the value for the
   specified attribute is returned in value. Otherwise it returns EGL_FALSE. If attribute
   is not a valid attribute then EGL_BAD_ATTRIBUTE is generated.
   Refer to Table 3.1 and Table 3.4 for a list of valid EGL attributes.

EGL_BUFFER_SIZE               integer    depth of the color buffer
EGL_RED_SIZE                  integer    bits of Red in the color buffer
EGL_GREEN_SIZE                integer    bits of Green in the color buffer
EGL_BLUE_SIZE                 integer    bits of Blue in the color buffer
EGL_LUMINANCE_SIZE            integer    bits of Luminance in the color buffer
EGL_ALPHA_SIZE                integer    bits of Alpha in the color buffer
EGL_ALPHA_MASK_SIZE           integer    bits of Alpha Mask in the mask buffer
EGL_BIND_TO_TEXTURE_RGB       boolean    True if bindable to RGB textures.
EGL_BIND_TO_TEXTURE_RGBA      boolean    True if bindable to RGBA textures.
EGL_COLOR_BUFFER_TYPE         enum       color buffer type
EGL_CONFIG_CAVEAT             enum       any caveats for the configuration
EGL_CONFIG_ID                 integer    unique EGLConfig identifier
EGL_CONFORMANT                bitmask    whether contexts created with this config are conformant
EGL_DEPTH_SIZE                integer    bits of Z in the depth buffer
EGL_LEVEL                     integer    frame buffer level
EGL_MAX_PBUFFER_WIDTH         integer    maximum width of pbuffer
EGL_MAX_PBUFFER_HEIGHT        integer    maximum height of pbuffer
EGL_MAX_PBUFFER_PIXELS        integer    maximum size of pbuffer
EGL_MAX_SWAP_INTERVAL         integer    maximum swap interval
EGL_MIN_SWAP_INTERVAL         integer    minimum swap interval
EGL_NATIVE_RENDERABLE         boolean    EGL_TRUE if native rendering APIs can render to surface
EGL_NATIVE_VISUAL_ID          integer    handle of corresponding native visual
EGL_NATIVE_VISUAL_TYPE        integer    native visual type of the associated visual
EGL_RENDERABLE_TYPE           bitmask    which client APIs are supported
EGL_SAMPLE_BUFFERS            integer    number of multisample buffers
EGL_SAMPLES                   integer    number of samples per pixel
EGL_STENCIL_SIZE              integer    bits of Stencil in the stencil buffer
EGL_SURFACE_TYPE              bitmask    which types of EGL surfaces are supported.
EGL_TRANSPARENT_TYPE          enum       type of transparency supported
EGL_TRANSPARENT_RED_VALUE     integer    transparent red value
EGL_TRANSPARENT_GREEN_VALUE   integer    transparent green value
EGL_TRANSPARENT_BLUE_VALUE    integer    transparent blue value

   Preconditions:

   value is null or a valid pointer

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.
      EGL_BAD_PARAMETER             value is null
      EGL_BAD_CONFIG                config does not name a valid EGLConfig
      EGL_BAD_ATTRIBUTE             attribute is not a valid attribute
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

*/

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if (!value) {
         thread->error = EGL_BAD_PARAMETER;
         result = EGL_FALSE;
      } else if (egl_config_to_id(config) < 0 || egl_config_to_id(config) >= EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_FALSE;
      } else if (!egl_config_get_attrib(egl_config_to_id(config), attribute, value)) {
         thread->error = EGL_BAD_ATTRIBUTE;
         result = EGL_FALSE;
      } else {
         thread->error = EGL_SUCCESS;
         result = EGL_TRUE;
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

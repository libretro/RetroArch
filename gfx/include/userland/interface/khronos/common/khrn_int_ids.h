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

#ifndef KHRN_INT_IDS_H
#define KHRN_INT_IDS_H

/*
   dispatch class ids
*/

#define GLBASE_ID_11                   0x1000
#define GLBASE_ID_20                   0x2000
#define VGBASE_ID                      0x3000
#define EGLBASE_ID                     0x4000

#define KHRNMISC_ID                    0x6000
#define GLBASE_ID                      0x7000

#define GET_BASE_ID(x)                 ((x) & 0xf000)

/*
   common OpenGL ES 1.1 and 2.0 dispatch ids
*/

#define GLACTIVETEXTURE_ID                       0x7001
#define GLBINDBUFFER_ID                          0x7002
#define GLBINDTEXTURE_ID                         0x7003
#define GLBUFFERDATA_ID                          0x7004
#define GLBUFFERSUBDATA_ID                       0x7005
#define GLCLEAR_ID                               0x7006
#define GLCLEARCOLOR_ID                          0x7007
#define GLCLEARDEPTHF_ID                         0x7008
#define GLCLEARSTENCIL_ID                        0x700a
#define GLCOLORMASK_ID                           0x700b
#define GLCOMPRESSEDTEXIMAGE2D_ID                0x700c
#define GLCOMPRESSEDTEXSUBIMAGE2D_ID             0x700d
#define GLCOPYTEXIMAGE2D_ID                      0x700e
#define GLCOPYTEXSUBIMAGE2D_ID                   0x700f
#define GLCULLFACE_ID                            0x7010
#define GLDELETEBUFFERS_ID                       0x7011
#define GLDELETETEXTURES_ID                      0x7012
#define GLDEPTHFUNC_ID                           0x7013
#define GLDEPTHMASK_ID                           0x7014
#define GLDEPTHRANGEF_ID                         0x7015
#define GLDISABLE_ID                             0x7016
#define GLINTDRAWELEMENTS_ID                     0x7018
#define GLENABLE_ID                              0x701a
#define GLFINISH_ID                              0x701b
#define GLFLUSH_ID                               0x701c
#define GLFRONTFACE_ID                           0x701d
#define GLGENBUFFERS_ID                          0x701e
#define GLGENTEXTURES_ID                         0x701f
#define GLGETBOOLEANV_ID                         0x7020
#define GLGETBUFFERPARAMETERIV_ID                0x7021
#define GLGETERROR_ID                            0x7022
#define GLGETFLOATV_ID                           0x7023
#define GLGETINTEGERV_ID                         0x7024
#define GLGETTEXPARAMETERFV_ID                   0x7025
#define GLGETTEXPARAMETERIV_ID                   0x7026
#define GLHINT_ID                                0x7027
#define GLISBUFFER_ID                            0x7028
#define GLISENABLED_ID                           0x702a
#define GLISTEXTURE_ID                           0x702b
#define GLLINEWIDTH_ID                           0x702c
#define GLPOLYGONOFFSET_ID                       0x702d
#define GLREADPIXELS_ID                          0x702e
#define GLSAMPLECOVERAGE_ID                      0x702f
#define GLSCISSOR_ID                             0x7030
#define GLTEXIMAGE2D_ID                          0x7031
#define GLTEXPARAMETERF_ID                       0x7032
#define GLTEXPARAMETERI_ID                       0x7033
#define GLTEXSUBIMAGE2D_ID                       0x7034
#define GLVIEWPORT_ID                            0x7035
#define GLINTFINDMAX_ID                          0x7036
#define GLINTCACHECREATE_ID                      0x7037
#define GLINTCACHEDELETE_ID                      0x7038
#define GLINTCACHEDATA_ID                        0x703a
#define GLINTCACHEGROW_ID                        0x703b
#define GLINTCACHEUSE_ID                         0x708c
#define GLBLENDFUNCSEPARATE_ID                   0x708d
#define GLSTENCILFUNCSEPARATE_ID                 0x708e
#define GLSTENCILMASKSEPARATE_ID                 0x708f
#define GLSTENCILOPSEPARATE_ID                   0x7090
#define GLEGLIMAGETARGETTEXTURE2DOES_ID          0x7091 /* GL_OES_EGL_image */
#define GLGLOBALIMAGETEXTURE2DOES_ID             0x7092 /* GL_OES_EGL_image/EGL_BRCM_global_image */
#define GLDISCARDFRAMEBUFFEREXT_ID               0x7100
/* GL_OES_framebuffer_object */
#define GLISRENDERBUFFER_ID                      0x7101
#define GLBINDRENDERBUFFER_ID                    0x7102
#define GLDELETERENDERBUFFERS_ID                 0x7103
#define GLGENRENDERBUFFERS_ID                    0x7104
#define GLRENDERBUFFERSTORAGE_ID                 0x7105
#define GLGETRENDERBUFFERPARAMETERIV_ID          0x7106
#define GLISFRAMEBUFFER_ID                       0x7107
#define GLBINDFRAMEBUFFER_ID                     0x7108
#define GLDELETEFRAMEBUFFERS_ID                  0x7109
#define GLGENFRAMEBUFFERS_ID                     0x710a
#define GLCHECKFRAMEBUFFERSTATUS_ID              0x710b
#define GLFRAMEBUFFERTEXTURE2D_ID                0x710c
#define GLFRAMEBUFFERRENDERBUFFER_ID             0x710d
#define GLGETFRAMEBUFFERATTACHMENTPARAMETERIV_ID 0x710e
#define GLGENERATEMIPMAP_ID                      0x710f
#define GLTEXPARAMETERFV_ID                      0x7110
#define GLTEXPARAMETERIV_ID                      0x7111
#define GLINSERTEVENTMARKEREXT_ID                0x7112
#define GLPUSHGROUPMARKEREXT_ID                  0x7113
#define GLPOPGROUPMARKEREXT_ID                   0x7114
#define TEXSUBIMAGE2DASYNC_ID                    0x7115
#define GLPIXELSTOREI_ID                         0x7116
#define GLINTATTRIBPOINTER_ID                    0x7117
#define GLINTATTRIB_ID                           0x7118
#define GLINTATTRIBENABLE_ID                     0x7119


/*
   OpenGL ES 1.1 specific dispatch ids
*/
#define GLALPHAFUNC_ID_11                 0x1001
#define GLALPHAFUNCX_ID_11                0x1002
#define GLCLEARCOLORX_ID_11               0x1004
#define GLCLEARDEPTHX_ID_11               0x1005
#define GLCLIPPLANEF_ID_11                0x1006
#define GLCLIPPLANEX_ID_11                0x1007
//#define GLCOLORPOINTER_ID_11              0x1008
#define GLCLIENTACTIVETEXTURE_ID_11       0x1009
#define GLDEPTHRANGEX_ID_11               0x100a
#define GLFOGF_ID_11                      0x100b
#define GLFOGX_ID_11                      0x100c
#define GLFOGFV_ID_11                     0x100d
#define GLFOGXV_ID_11                     0x100e
#define GLFRUSTUMF_ID_11                  0x100f
#define GLFRUSTUMX_ID_11                  0x1020
#define GLGETCLIPPLANEF_ID_11             0x1021
#define GLGETCLIPPLANEX_ID_11             0x1022
#define GLGETFIXEDV_ID_11                 0x1023
#define GLGETLIGHTFV_ID_11                0x1024
#define GLGETLIGHTXV_ID_11                0x1025
#define GLGETMATERIALFV_ID_11             0x1026
#define GLGETMATERIALXV_ID_11             0x1027
#define GLGETTEXENVFV_ID_11               0x1028
#define GLGETTEXENVIV_ID_11               0x102a
#define GLGETTEXENVXV_ID_11               0x102b
#define GLGETTEXPARAMETERXV_ID_11         0x102c
#define GLLIGHTF_ID_11                    0x102d
#define GLLIGHTX_ID_11                    0x102e
#define GLLIGHTFV_ID_11                   0x102f
#define GLLIGHTXV_ID_11                   0x1030
#define GLLIGHTMODELF_ID_11               0x1031
#define GLLIGHTMODELX_ID_11               0x1032
#define GLLIGHTMODELFV_ID_11              0x1033
#define GLLIGHTMODELXV_ID_11              0x1034
#define GLLINEWIDTHX_ID_11                0x1035
#define GLLOADIDENTITY_ID_11              0x1036
#define GLLOADMATRIXF_ID_11               0x1037
#define GLLOADMATRIXX_ID_11               0x1038
#define GLLOGICOP_ID_11                   0x103a
#define GLMATERIALF_ID_11                 0x103b
#define GLMATERIALX_ID_11                 0x103c
#define GLMATERIALFV_ID_11                0x103d
#define GLMATERIALXV_ID_11                0x103e
#define GLMATRIXMODE_ID_11                0x103f
#define GLMULTMATRIXF_ID_11               0x1040
#define GLMULTMATRIXX_ID_11               0x1041
//#define GLNORMALPOINTER_ID_11             0x1042
#define GLORTHOF_ID_11                    0x1043
#define GLORTHOX_ID_11                    0x1044
//#define GLPIXELSTOREI_ID_11               0x1045
#define GLPOINTPARAMETERF_ID_11           0x1046
#define GLPOINTPARAMETERX_ID_11           0x1047
#define GLPOINTPARAMETERFV_ID_11          0x1048
#define GLPOINTPARAMETERXV_ID_11          0x104a
#define GLPOLYGONOFFSETX_ID_11            0x104b
#define GLPOPMATRIX_ID_11                 0x104c
#define GLPUSHMATRIX_ID_11                0x104d
#define GLROTATEF_ID_11                   0x104e
#define GLROTATEX_ID_11                   0x104f
#define GLSAMPLECOVERAGEX_ID_11           0x1050
#define GLSCALEF_ID_11                    0x1051
#define GLSCALEX_ID_11                    0x1052
#define GLSHADEMODEL_ID_11                0x1053
#define GLTEXENVF_ID_11                   0x1057
#define GLTEXENVI_ID_11                   0x1058
#define GLTEXENVX_ID_11                   0x105a
#define GLTEXENVFV_ID_11                  0x105b
#define GLTEXENVIV_ID_11                  0x105c
#define GLTEXENVXV_ID_11                  0x105d
#define GLTEXPARAMETERX_ID_11             0x105e
#define GLTRANSLATEF_ID_11                0x105f
#define GLTRANSLATEX_ID_11                0x1060
//#define GLTEXCOORDPOINTER_ID_11           0x1061
//#define GLVERTEXPOINTER_ID_11             0x1062

//#define GLPOINTSIZEPOINTEROES_ID_11       0x1063
#define GLINTCOLOR_ID_11                  0x1064
#define GLQUERYMATRIXXOES_ID_11           0x1065
#define GLTEXPARAMETERXV_ID_11            0x1067
#define GLDRAWTEXFOES_ID_11               0x1068

#define GLCURRENTPALETTEMATRIXOES_ID_11   0x1069               /* GL_OES_matrix_palette */
#define GLLOADPALETTEFROMMODELVIEWMATRIXOES_ID_11     0x1070   /* GL_OES_matrix_palette */
//#define GLMATRIXINDEXPOINTEROES_ID_11     0x1071               /* GL_OES_matrix_palette */
//#define GLWEIGHTPOINTEROES_ID_11          0x1072               /* GL_OES_matrix_palette */

/*
   OpenGL ES 2.0 dispatch ids
*/
#define GLATTACHSHADER_ID_20                             0x2001
#define GLBINDATTRIBLOCATION_ID_20                       0x2002
#define GLBLENDCOLOR_ID_20                               0x2005
#define GLBLENDEQUATIONSEPARATE_ID_20                    0x2006
#define GLCOMPILESHADER_ID_20                            0x200a
#define GLCREATEPROGRAM_ID_20                            0x200b
#define GLCREATESHADER_ID_20                             0x200c
#define GLDELETEPROGRAM_ID_20                            0x200e
#define GLDELETESHADER_ID_20                             0x2010
#define GLDETACHSHADER_ID_20                             0x2011
#define GLGETATTRIBLOCATION_ID_20                        0x2017
#define GLGETACTIVEATTRIB_ID_20                          0x2018
#define GLGETACTIVEUNIFORM_ID_20                         0x201a
#define GLGETATTACHEDSHADERS_ID_20                       0x201b
#define GLGETPROGRAMIV_ID_20                             0x201d
#define GLGETPROGRAMINFOLOG_ID_20                        0x201e
#define GLGETSHADERIV_ID_20                              0x2020
#define GLGETSHADERINFOLOG_ID_20                         0x2021
#define GLGETSHADERSOURCE_ID_20                          0x2022
#define GLGETSHADERPRECISIONFORMAT_ID_20                 0x2023
#define GLGETUNIFORMFV_ID_20                             0x2024
#define GLGETUNIFORMIV_ID_20                             0x2025
#define GLGETUNIFORMLOCATION_ID_20                       0x2026
#define GLISPROGRAM_ID_20                                0x2028
#define GLISSHADER_ID_20                                 0x202b
#define GLLINKPROGRAM_ID_20                              0x202c
//#define GLPIXELSTOREI_ID_20                              0x202d
#define GLPOINTSIZE_ID_20                                0x202e
#define GLSHADERSOURCE_ID_20                             0x2030
#define GLTEXPARAMETERIV_ID_20                           0x2034
#define GLUNIFORM1F_ID_20                                0x2035
#define GLUNIFORM2F_ID_20                                0x2036
#define GLUNIFORM3F_ID_20                                0x2037
#define GLUNIFORM4F_ID_20                                0x2038
#define GLUNIFORM1I_ID_20                                0x203a
#define GLUNIFORM2I_ID_20                                0x203b
#define GLUNIFORM3I_ID_20                                0x203c
#define GLUNIFORM4I_ID_20                                0x203d
#define GLUNIFORM1FV_ID_20                               0x203e
#define GLUNIFORM2FV_ID_20                               0x203f
#define GLUNIFORM3FV_ID_20                               0x2040
#define GLUNIFORM4FV_ID_20                               0x2041
#define GLUNIFORM1IV_ID_20                               0x2042
#define GLUNIFORM2IV_ID_20                               0x2043
#define GLUNIFORM3IV_ID_20                               0x2044
#define GLUNIFORM4IV_ID_20                               0x2045
#define GLUNIFORMMATRIX2FV_ID_20                         0x2046
#define GLUNIFORMMATRIX3FV_ID_20                         0x2047
#define GLUNIFORMMATRIX4FV_ID_20                         0x2048
#define GLUSEPROGRAM_ID_20                               0x204a
#define GLVALIDATEPROGRAM_ID_20                          0x204b
//#define GLVERTEXATTRIBPOINTER_ID_20                      0x204c
#define GLEGLIMAGETARGETRENDERBUFFERSTORAGEOES_ID_20     0x204d /* GL_OES_EGL_image */
#define GLGLOBALIMAGERENDERBUFFERSTORAGEOES_ID_20        0x204e /* GL_OES_EGL_image/EGL_BRCM_global_image */

/*
   OpenVG dispatch ids
*/

#define VGCLEARERROR_ID                      0x3000
#define VGSETERROR_ID                        0x3001
#define VGGETERROR_ID                        0x3002
#define VGFLUSH_ID                           0x3003
#define VGFINISH_ID                          0x3004
#define VGCREATESTEMS_ID                     0x3005
#define VGDESTROYSTEM_ID                     0x3006
#define VGSETIV_ID                           0x3007
#define VGSETFV_ID                           0x3008
#define VGGETFV_ID                           0x3009
#define VGSETPARAMETERIV_ID                  0x300a
#define VGSETPARAMETERFV_ID                  0x300b
#define VGGETPARAMETERIV_ID                  0x300c
#define VGLOADMATRIX_ID                      0x300d
#define VGMASK_ID                            0x300e
#define VGRENDERTOMASK_ID                    0x300f /* vg 1.1 */
#define VGCREATEMASKLAYER_ID                 0x3010 /* vg 1.1 */
#define VGDESTROYMASKLAYER_ID                0x3011 /* vg 1.1 */
#define VGFILLMASKLAYER_ID                   0x3012 /* vg 1.1 */
#define VGCOPYMASK_ID                        0x3013 /* vg 1.1 */
#define VGCLEAR_ID                           0x3014
#define VGCREATEPATH_ID                      0x3015
#define VGCLEARPATH_ID                       0x3016
#define VGDESTROYPATH_ID                     0x3017
#define VGREMOVEPATHCAPABILITIES_ID          0x3018
#define VGAPPENDPATH_ID                      0x3019
#define VGAPPENDPATHDATA_ID                  0x301a
#define VGMODIFYPATHCOORDS_ID                0x301b
#define VGTRANSFORMPATH_ID                   0x301c
#define VGINTERPOLATEPATH_ID                 0x301d
#define VGPATHLENGTH_ID                      0x301e
#define VGPOINTALONGPATH_ID                  0x301f
#define VGPATHBOUNDS_ID                      0x3020
#define VGPATHTRANSFORMEDBOUNDS_ID           0x3021
#define VGDRAWPATH_ID                        0x3022
#define VGCREATEPAINT_ID                     0x3023
#define VGDESTROYPAINT_ID                    0x3024
#define VGSETPAINT_ID                        0x3025
#define VGPAINTPATTERN_ID                    0x3026
#define VGCREATEIMAGE_ID                     0x3027
#define VGDESTROYIMAGE_ID                    0x3028
#define VGCLEARIMAGE_ID                      0x3029
#define VGIMAGESUBDATA_ID                    0x302a
#define VGGETIMAGESUBDATA_ID                 0x302b
#define VGCHILDIMAGE_ID                      0x302c
#define VGGETPARENT_ID                       0x302d
#define VGCOPYIMAGE_ID                       0x302e
#define VGDRAWIMAGE_ID                       0x302f
#define VGSETPIXELS_ID                       0x3030
#define VGWRITEPIXELS_ID                     0x3031
#define VGGETPIXELS_ID                       0x3032
#define VGREADPIXELS_ID                      0x3033
#define VGCOPYPIXELS_ID                      0x3034
#define VGCREATEFONT_ID                      0x3035 /* vg 1.1 */
#define VGDESTROYFONT_ID                     0x3036 /* vg 1.1 */
#define VGSETGLYPHTOPATH_ID                  0x3037 /* vg 1.1 */
#define VGSETGLYPHTOIMAGE_ID                 0x3038 /* vg 1.1 */
#define VGCLEARGLYPH_ID                      0x3039 /* vg 1.1 */
#define VGDRAWGLYPH_ID                       0x303a /* vg 1.1 */
#define VGDRAWGLYPHS_ID                      0x303b /* vg 1.1 */
#define VGCOLORMATRIX_ID                     0x303c
#define VGCONVOLVE_ID                        0x303d
#define VGSEPARABLECONVOLVE_ID               0x303e
#define VGGAUSSIANBLUR_ID                    0x303f
#define VGLOOKUP_ID                          0x3040
#define VGLOOKUPSINGLE_ID                    0x3041
#define VGULINE_ID                           0x3042 /* vgu */
#define VGUPOLYGON_ID                        0x3043 /* vgu */
#define VGURECT_ID                           0x3044 /* vgu */
#define VGUROUNDRECT_ID                      0x3045 /* vgu */
#define VGUELLIPSE_ID                        0x3046 /* vgu */
#define VGUARC_ID                            0x3047 /* vgu */
#define VGCREATEEGLIMAGETARGETKHR_ID         0x3048 /* VG_KHR_EGL_image */
#define VGCREATEIMAGEFROMGLOBALIMAGE_ID      0x3049 /* VG_KHR_EGL_image/EGL_BRCM_global_image */

/*
   EGL dispatch ids
*/

#define EGLINTCREATESURFACE_ID            0x4000
#define EGLINTCREATEGLES11_ID             0x4001
#define EGLINTCREATEGLES20_ID             0x4002
#define EGLINTCREATEVG_ID                 0x4003
#define EGLINTDESTROYSURFACE_ID           0x4004
#define EGLINTDESTROYGL_ID                0x4005
#define EGLINTDESTROYVG_ID                0x4006
/*#define EGLINTRESIZESURFACE_ID            0x4007*/
#define EGLINTMAKECURRENT_ID              0x4008
#define EGLINTFLUSHANDWAIT_ID             0x4009
#define EGLINTSWAPBUFFERS_ID              0x400a
#define EGLINTSELECTMIPMAP_ID             0x400b
#define EGLINTFLUSH_ID                    0x400c
#define EGLINTGETCOLORDATA_ID             0x400d
#define EGLINTSETCOLORDATA_ID             0x400e
#define EGLINTBINDTEXIMAGE_ID             0x400f
#define EGLINTRELEASETEXIMAGE_ID          0x4010
#define EGLINTCREATEPBUFFERFROMVGIMAGE_ID 0x4011
#define EGLINTCREATEWRAPPEDSURFACE_ID     0x4012
#define EGLCREATEIMAGEKHR_ID              0x4013 /* EGL_KHR_image */
#define EGLDESTROYIMAGEKHR_ID             0x4014 /* EGL_KHR_image */
#define EGLINTOPENMAXILDONEMARKER_ID      0x4015 /* EGL-OpenMAX interworking (Broadcom-specific) */
#define EGLINTSWAPINTERVAL_ID             0x4016
#define EGLINTGETPROCESSMEMUSAGE_ID       0x4017 /* EGL_BRCM_mem_usage */
#define EGLINTGETGLOBALMEMUSAGE_ID        0x4018
#define EGLCREATEGLOBALIMAGEBRCM_ID       0x4019 /* EGL_BRCM_global_image */
#define EGLFILLGLOBALIMAGEBRCM_ID         0x401a /* EGL_BRCM_global_image */
#define EGLCREATECOPYGLOBALIMAGEBRCM_ID   0x401b /* EGL_BRCM_global_image */
#define EGLDESTROYGLOBALIMAGEBRCM_ID      0x401c /* EGL_BRCM_global_image */
#define EGLQUERYGLOBALIMAGEBRCM_ID        0x401d /* EGL_BRCM_global_image */
#define EGLINTCREATESYNC_ID               0x401e /* EGL_KHR_fence_sync */
#define EGLINTDESTROYSYNC_ID              0x401f /* EGL_KHR_fence_sync */
#define EGLINITPERFMONITORBRCM_ID         0x4020 /* EGL_BRCM_perf_monitor */
#define EGLTERMPERFMONITORBRCM_ID         0x4021 /* EGL_BRCM_perf_monitor */
#define EGLINTDESTROYBYPID_ID             0x4022
#define EGLINTIMAGESETCOLORDATA_ID        0x4023 /* EGL_KHR_image (client-side pixmaps etc.) */
#define EGLPERFSTATSRESETBRCM_ID          0x4024 /* EGL_BRCM_perf_stats */
#define EGLPERFSTATSGETBRCM_ID            0x4025 /* EGL_BRCM_perf_stats */
#define EGLINTCREATEENDPOINTIMAGE_ID      0x4026 /* EGL_NOK_image_endpoint */
#define EGLINTDESTROYENDPOINTIMAGE_ID     0x4027 /* EGL_NOK_image_endpoint */
#define EGLINTACQUIREENDPOINTIMAGE_ID     0x4028 /* EGL_NOK_image_endpoint */
#define EGLINITDRIVERMONITORBRCM_ID       0x4029 /* EGL_BRCM_driver_monitor */
#define EGLTERMDRIVERMONITORBRCM_ID       0x402a /* EGL_BRCM_driver_monitor */
#define EGLGETDRIVERMONITORXMLBRCM_ID     0x402b /* EGL_BRCM_driver_monitor */
#define EGLDIRECTRENDERINGPOINTER_ID      0x402c /* DIRECT_RENDERING */
#define EGLPUSHRENDERINGIMAGE_ID          0x402d /* Android GL App supportN */
#define EGLINTUPDATETEXTURE_ID            0x402e /* Android GL App supportN */
#define EGLINTCREATESYNCFENCE_ID          0x402f /* EGL_KHR_fence_sync */

/*
   Miscellaneous driver control functions (not related to any particular API)
*/

#define KHRNMISCTRYUNLOAD_ID           0x6000
#define KHRNMISCBULKRXREQUIRED_ID      0x6001 /* bulk transfer client->server advance notifier */

/*
   signalling length used to indicate a NULL argument
*/

#define LENGTH_SIGNAL_NULL             0xffffffff

/*
   async (KHAN) channel commands
*/

#define ASYNC_COMMAND_WAIT    0
#define ASYNC_COMMAND_POST    1
#define ASYNC_COMMAND_DESTROY 2
#define ASYNC_RENDER_COMPLETE 3
#define ASYNC_ERROR_NOTIFY    4
#endif

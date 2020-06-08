/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "display.h"

#include "queue.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <gbm.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include <rga/RgaApi.h>

#define EGL_EGLEXT_PROTOTYPES
//#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
// #include <GLES2/gl2.h>
// #include <GLES2/gl2ext.h>

#include <png.h>


typedef struct go2_display
{
    int fd;
    uint32_t connector_id;
    drmModeModeInfo mode;
    uint32_t width;
    uint32_t height;
    uint32_t crtc_id;
} go2_display_t;

typedef struct go2_surface
{
    go2_display_t* display;
    uint32_t gem_handle;
    uint64_t size;
    int width;
    int height;
    int stride;
    uint32_t format;
    int prime_fd;
    uint8_t* map;
} go2_surface_t;

typedef struct go2_frame_buffer
{
    go2_surface_t* surface;
    uint32_t fb_id;
} go2_frame_buffer_t;

typedef struct go2_presenter
{
    go2_display_t* display;
    uint32_t format;
    uint32_t background_color;
    go2_queue_t* freeFrameBuffers;
    go2_queue_t* usedFrameBuffers;
    pthread_mutex_t queueMutex;
    pthread_t renderThread;
    sem_t freeSem;
    sem_t usedSem;
    volatile bool terminating;
    bool managed;
} go2_presenter_t;


go2_display_t* go2_display_create()
{
    int i;

    go2_display_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        goto out;
    }

    memset(result, 0, sizeof(*result));


    // Open device
    result->fd = open("/dev/dri/card0", O_RDWR);
    if (result->fd < 0)
    {
        printf("open /dev/dri/card0 failed.\n");
        goto err_00;
    }


    drmModeRes* resources = drmModeGetResources(result->fd);
    if (!resources)
    {
        printf("drmModeGetResources failed: %s\n", strerror(errno));
        goto err_01;
    }


    // Find connector
    drmModeConnector* connector;
    for (i = 0; i < resources->count_connectors; i++)
    {
        connector = drmModeGetConnector(result->fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            break;
        }

        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (!connector)
    {
        printf("DRM_MODE_CONNECTED not found.\n");
        goto err_02;
    }

    result->connector_id = connector->connector_id;


    // Find prefered mode
    drmModeModeInfo* mode;
    for (i = 0; i < connector->count_modes; i++)
    {
        drmModeModeInfo *current_mode = &connector->modes[i];
        if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
        {
            mode = current_mode;
            break;
        }

        mode = NULL;
    }

    if (!mode)
    {
        printf("DRM_MODE_TYPE_PREFERRED not found.\n");
        goto err_03;
    }

    result->mode = *mode;
    result->width = mode->hdisplay;
    result->height = mode->vdisplay;


    // Find encoder
    drmModeEncoder* encoder;
    for (i = 0; i < resources->count_encoders; i++)
    {
        encoder = drmModeGetEncoder(result->fd, resources->encoders[i]);
        if (encoder->encoder_id == connector->encoder_id)
        {
            break;
        }

        drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder)
    {

        printf("could not find encoder!\n");
        goto err_03;
    }

    result->crtc_id = encoder->crtc_id;

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    return result;


err_03:
    drmModeFreeConnector(connector);

err_02:
    drmModeFreeResources(resources);

err_01:
    close(result->fd);

err_00:
    free(result);

out:
    return NULL;
}


void go2_display_destroy(go2_display_t* display)
{
    close(display->fd);
    free(display);
}

int go2_display_width_get(go2_display_t* display)
{
    return display->width;
}

int go2_display_height_get(go2_display_t* display)
{
    return display->height;
}

void go2_display_present(go2_display_t* display, go2_frame_buffer_t* frame_buffer)
{
    int ret = drmModeSetCrtc(display->fd, display->crtc_id, frame_buffer->fb_id, 0, 0, &display->connector_id, 1, &display->mode);
    if (ret)
    {
        printf("drmModeSetCrtc failed.\n");
    }
}

const char* BACKLIGHT_BRIGHTNESS_NAME = "/sys/class/backlight/backlight/brightness";
const char* BACKLIGHT_BRIGHTNESS_MAX_NAME = "/sys/class/backlight/backlight/max_brightness";
#define BACKLIGHT_BUFFER_SIZE (127)

uint32_t go2_display_backlight_get(go2_display_t* display)
{
    int fd;
    int max = 255;
    int value = 0;
    char buffer[BACKLIGHT_BUFFER_SIZE + 1];

    fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            max = atoi(buffer);
        }

        close(fd);

        if (max == 0) return 0;
    }

    fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            value = atoi(buffer);
        }

        close(fd);
    }

    float percent = value / (float)max * 100.0f;
    return (uint32_t)percent;
}

void go2_display_backlight_set(go2_display_t* display, uint32_t value)
{
    int fd;
    int max = 255;
    char buffer[BACKLIGHT_BUFFER_SIZE + 1];


    if (value > 100) value = 100;

    fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            max = atoi(buffer);
        }

        close(fd);

        if (max == 0) return;
    }

    fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_WRONLY);
    if (fd > 0)
    {
        float percent = value / 100.0f * (float)max;
        sprintf(buffer, "%d\n", (uint32_t)percent);

        //printf("backlight=%d, max=%d\n", (uint32_t)percent, max);

        ssize_t count = write(fd, buffer, strlen(buffer));
        if (count < 0)
        {
            printf("go2_display_backlight_set write failed.\n");
        }

        close(fd);
    }
    else
    {
        printf("go2_display_backlight_set open failed.\n");
    }

}


int go2_drm_format_get_bpp(uint32_t format)
{
    int result;

    switch(format)
    {
        case DRM_FORMAT_XRGB4444:
        case DRM_FORMAT_XBGR4444:
        case DRM_FORMAT_RGBX4444:
        case DRM_FORMAT_BGRX4444:

        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGRA4444:

        case DRM_FORMAT_XRGB1555:
        case DRM_FORMAT_XBGR1555:
        case DRM_FORMAT_RGBX5551:
        case DRM_FORMAT_BGRX5551:

        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_BGRA5551:

        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
            result = 16;
            break;


        case DRM_FORMAT_RGB888:
        case DRM_FORMAT_BGR888:
            result = 24;
            break;


        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_BGRX8888:

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_BGRA8888:

        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_RGBX1010102:
        case DRM_FORMAT_BGRX1010102:

        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_BGRA1010102:
            result = 32;
            break;


        default:
            printf("unhandled DRM FORMAT.\n");
            result = 0;
    }

    return result;
}

go2_surface_t* go2_surface_create(go2_display_t* display, int width, int height, uint32_t format)
{
    go2_surface_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        goto out;
    }

    memset(result, 0, sizeof(*result));


    struct drm_mode_create_dumb args = {0};
    args.width = width;
    args.height = height;
    args.bpp = go2_drm_format_get_bpp(format);
    args.flags = 0;

    int io = drmIoctl(display->fd, DRM_IOCTL_MODE_CREATE_DUMB, &args);
    if (io < 0)
    {
        printf("DRM_IOCTL_MODE_CREATE_DUMB failed.\n");
        goto out;
    }


    result->display = display;
    result->gem_handle = args.handle;
    result->size = args.size;
    result->width = width;
    result->height = height;
    result->stride = args.pitch;
    result->format = format;
    result->map = NULL;

    return result;

out:
    free(result);
    return NULL;
}

void go2_surface_destroy(go2_surface_t* surface)
{
    struct drm_mode_destroy_dumb args = { 0 };
    args.handle = surface->gem_handle;

    int io = drmIoctl(surface->display->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &args);
    if (io < 0)
    {
        printf("DRM_IOCTL_MODE_DESTROY_DUMB failed.\n");
    }

    free(surface);
}

int go2_surface_width_get(go2_surface_t* surface)
{
    return surface->width;
}

int go2_surface_height_get(go2_surface_t* surface)
{
    return surface->height;
}

uint32_t go2_surface_format_get(go2_surface_t* surface)
{
    return surface->format;
}

int go2_surface_stride_get(go2_surface_t* surface)
{
    return surface->stride;
}

go2_display_t* go2_surface_display_get(go2_surface_t* surface)
{
    return surface->display;
}

int go2_surface_prime_fd(go2_surface_t* surface)
{
    if (surface->prime_fd <= 0)
    {
        int io = drmPrimeHandleToFD(surface->display->fd, surface->gem_handle, DRM_RDWR | DRM_CLOEXEC, &surface->prime_fd);
        if (io < 0)
        {
            printf("drmPrimeHandleToFD failed.\n");
            goto out;
        }
    }

    return surface->prime_fd;

out:
    surface->prime_fd = 0;
    return 0;
}

void* go2_surface_map(go2_surface_t* surface)
{
    if (surface->map)
        return surface->map;

    int prime_fd = go2_surface_prime_fd(surface);
    surface->map = mmap(NULL, surface->size, PROT_READ | PROT_WRITE, MAP_SHARED, prime_fd, 0);
    if (surface->map == MAP_FAILED)
    {
        printf("mmap failed.\n");
        return NULL;
    }

    return surface->map;
}

void go2_surface_unmap(go2_surface_t* surface)
{
    if (surface->map)
    {
        munmap(surface->map, surface->size);
        surface->map = NULL;
    }
}


static uint32_t go2_rkformat_get(uint32_t drm_fourcc)
{
    switch (drm_fourcc)
    {
        case DRM_FORMAT_RGBA8888:
            return RK_FORMAT_RGBA_8888;

        case DRM_FORMAT_RGBX8888:
            return RK_FORMAT_RGBX_8888;

        case DRM_FORMAT_RGB888:
            return RK_FORMAT_RGB_888;

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XRGB8888:
            return RK_FORMAT_BGRA_8888;

        case DRM_FORMAT_RGB565:
            return RK_FORMAT_RGB_565;

        case DRM_FORMAT_RGBA5551:
            return RK_FORMAT_RGBA_5551;

        case DRM_FORMAT_RGBA4444:
            return RK_FORMAT_RGBA_4444;

        case DRM_FORMAT_BGR888:
            return RK_FORMAT_BGR_888;

        default:
            printf("RKFORMAT not supported. ");
            printf("drm_fourcc=%c%c%c%c\n", drm_fourcc & 0xff, drm_fourcc >> 8 & 0xff, drm_fourcc >> 16 & 0xff, drm_fourcc >> 24);
            return 0;
    }
}

void go2_surface_blit(go2_surface_t* srcSurface, int srcX, int srcY, int srcWidth, int srcHeight,
                      go2_surface_t* dstSurface, int dstX, int dstY, int dstWidth, int dstHeight,
                      go2_rotation_t rotation, int scale_mode)
{
    rga_info_t dst = { 0 };
    dst.fd = go2_surface_prime_fd(dstSurface);
    dst.mmuFlag = 1;
    dst.rect.xoffset = dstX;
    dst.rect.yoffset = dstY;
    dst.rect.width = dstWidth;
    dst.rect.height = dstHeight;
    dst.rect.wstride = dstSurface->stride / (go2_drm_format_get_bpp(dstSurface->format) / 8);
    dst.rect.hstride = dstSurface->height;
    dst.rect.format = go2_rkformat_get(dstSurface->format);

    rga_info_t src = { 0 };
    src.fd = go2_surface_prime_fd(srcSurface);
    src.mmuFlag = 1;

    switch (rotation)
    {
        case GO2_ROTATION_DEGREES_0:
            src.rotation = 0;
            break;

        case GO2_ROTATION_DEGREES_90:
            src.rotation = HAL_TRANSFORM_ROT_90;
            break;

        case GO2_ROTATION_DEGREES_180:
            src.rotation = HAL_TRANSFORM_ROT_180;
            break;

        case GO2_ROTATION_DEGREES_270:
            src.rotation = HAL_TRANSFORM_ROT_270;
            break;

        default:
            printf("rotation not supported.\n");
            return;
    }

    src.rect.xoffset = srcX;
    src.rect.yoffset = srcY;
    src.rect.width = srcWidth;
    src.rect.height = srcHeight;
    src.rect.wstride = srcSurface->stride / (go2_drm_format_get_bpp(srcSurface->format) / 8);
    src.rect.hstride = srcSurface->height;
    src.rect.format = go2_rkformat_get(srcSurface->format);

#if 0
    enum
    {
        CATROM    = 0x0,
        MITCHELL  = 0x1,
        HERMITE   = 0x2,
        B_SPLINE  = 0x3,
    };  /*bicubic coefficient*/
#endif
    src.scale_mode = scale_mode;

    int ret = c_RkRgaBlit(&src, &dst, NULL);
    if (ret)
    {
        printf("c_RkRgaBlit failed.\n");
    }
}

int go2_surface_save_as_png(go2_surface_t* surface, const char* filename)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;


    png_byte color_type = 0;
    png_byte bit_depth = 0;
    switch (surface->format)
    {
        case DRM_FORMAT_RGBA8888:
            color_type = PNG_COLOR_TYPE_RGBA;
            bit_depth = 8;
            break;

        case DRM_FORMAT_RGB888:
            color_type = PNG_COLOR_TYPE_RGB;
            bit_depth = 8;
            break;

        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGR888:

        default:
            printf("The image format is not supported.\n");
            return -2;
    }


    // based on http://zarb.org/~gc/html/libpng.html

    /* create file */
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("fopen failed. filename='%s'\n", filename);
        return -1;
    }


    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
    {
        printf("png_create_write_struct failed.\n");
        fclose(fp);
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        printf("png_create_info_struct failed.\n");
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("init_io failed.\n");
        goto out;
    }

    png_init_io(png_ptr, fp);


    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
     {
        printf("write header failed.\n");
        goto out;
    }

    png_set_IHDR(png_ptr, info_ptr, surface->width, surface->height,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);


    /* write bytes */
    png_bytep src = (png_bytep)go2_surface_map(surface);
    row_pointers = malloc(sizeof(png_bytep) * surface->height);
    for (int y = 0; y < surface->height; ++y)
    {
        row_pointers[y] = src + (surface->stride * y);
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("writing bytes failed.\n");
        goto out;
    }

    png_write_image(png_ptr, row_pointers);


    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("end of write failed.\n");
        goto out;
    }

    png_write_end(png_ptr, NULL);

    /* cleanup heap allocation */
    free(row_pointers);

    fclose(fp);
    return 0;

out:
    if (info_ptr)
        png_destroy_info_struct(png_ptr, &info_ptr);

    if (png_ptr)
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

    if (row_pointers)
        free(row_pointers);

    if (fp)
        fclose(fp);

    return -1;
}


go2_frame_buffer_t* go2_frame_buffer_create(go2_surface_t* surface)
{
    go2_frame_buffer_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));


    result->surface = surface;

    const uint32_t handles[4] = {surface->gem_handle, 0, 0, 0};
    const uint32_t pitches[4] = {surface->stride, 0, 0, 0};
    const uint32_t offsets[4] = {0, 0, 0, 0};

    int ret = drmModeAddFB2(surface->display->fd,
        surface->width,
        surface->height,
        surface->format,
        handles,
        pitches,
        offsets,
        &result->fb_id,
        0);
    if (ret)
    {
        printf("drmModeAddFB2 failed.\n");
        free(result);
        return NULL;
    }

    return result;
}

void go2_frame_buffer_destroy(go2_frame_buffer_t* frame_buffer)
{
    int ret = drmModeRmFB(frame_buffer->surface->display->fd, frame_buffer->fb_id);
    if (ret)
    {
        printf("drmModeRmFB failed.\n");
    }

    free(frame_buffer);
}

go2_surface_t* go2_frame_buffer_surface_get(go2_frame_buffer_t* frame_buffer)
{
    return frame_buffer->surface;
}





#if 0
typedef struct go2_presenter
{
    go2_display_t* display;
    uint32_t format;
    uint32_t background_color;
    go2_queue_t* freeFrameBuffers;
    go2_queue_t* usedFrameBuffers;
    pthread_mutex_t queueMutex;
    pthread_t renderThread;
    sem_t freeSem;
    sem_t usedSem;
    volatile bool terminating;
} go2_presenter_t;
#endif

#define BUFFER_COUNT (3)

static void* go2_presenter_renderloop(void* arg)
{
    go2_presenter_t* presenter = (go2_presenter_t*)arg;
    go2_frame_buffer_t* prevFrameBuffer = NULL;

    presenter->terminating = false;
    while(!presenter->terminating)
    {
        sem_wait(&presenter->usedSem);
        if(presenter->terminating) break;


        pthread_mutex_lock(&presenter->queueMutex);

        if (go2_queue_count_get(presenter->usedFrameBuffers) < 1)
        {
            printf("no framebuffer available.\n");
            abort();
        }

        go2_frame_buffer_t* dstFrameBuffer = (go2_frame_buffer_t*)go2_queue_pop(presenter->usedFrameBuffers);

        pthread_mutex_unlock(&presenter->queueMutex);


        go2_display_present(presenter->display, dstFrameBuffer);

        if (prevFrameBuffer)
        {
            pthread_mutex_lock(&presenter->queueMutex);
            go2_queue_push(presenter->freeFrameBuffers, prevFrameBuffer);
            pthread_mutex_unlock(&presenter->queueMutex);

            sem_post(&presenter->freeSem);
        }

        prevFrameBuffer = dstFrameBuffer;
    }


    return NULL;
}

go2_display_t* go2_presenter_display_get(go2_presenter_t* presenter)
{
    return presenter->display;
}

go2_presenter_t* go2_presenter_create(go2_display_t* display, uint32_t format, uint32_t background_color, bool managed)
{
    go2_presenter_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));


    result->display = display;
    result->format = format;
    result->background_color = background_color;
    result->managed = managed;
    if (managed) {
        result->freeFrameBuffers = go2_queue_create(BUFFER_COUNT);
        result->usedFrameBuffers = go2_queue_create(BUFFER_COUNT);

        int width = go2_display_width_get(display);
        int height = go2_display_height_get(display);

        for (int i = 0; i < BUFFER_COUNT; ++i)
        {
            go2_surface_t* surface = go2_surface_create(display, width, height, format);
            go2_frame_buffer_t* frameBuffer = go2_frame_buffer_create(surface);

            go2_queue_push(result->freeFrameBuffers, frameBuffer);
        }

        sem_init(&result->usedSem, 0, 0);
        sem_init(&result->freeSem, 0, BUFFER_COUNT);

        pthread_mutex_init(&result->queueMutex, NULL);

        pthread_create(&result->renderThread, NULL, go2_presenter_renderloop, result);
    }
    else {
        result->freeFrameBuffers = go2_queue_create(0);
        result->usedFrameBuffers = go2_queue_create(0);
    }

    return result;
}

void go2_presenter_destroy(go2_presenter_t* presenter)
{
    if (presenter->managed) {
        presenter->terminating = true;
        sem_post(&presenter->usedSem);

        pthread_join(presenter->renderThread, NULL);
        pthread_mutex_destroy(&presenter->queueMutex);

        sem_destroy(&presenter->freeSem);
        sem_destroy(&presenter->usedSem);

        while(go2_queue_count_get(presenter->usedFrameBuffers) > 0)
        {
            go2_frame_buffer_t* frameBuffer = go2_queue_pop(presenter->usedFrameBuffers);

            go2_surface_t* surface = frameBuffer->surface;

            go2_frame_buffer_destroy(frameBuffer);
            go2_surface_destroy(surface);
        }

        while(go2_queue_count_get(presenter->freeFrameBuffers) > 0)
        {
            go2_frame_buffer_t* frameBuffer = go2_queue_pop(presenter->freeFrameBuffers);

            go2_surface_t* surface = frameBuffer->surface;

            go2_frame_buffer_destroy(frameBuffer);
            go2_surface_destroy(surface);
        }
    }

    free(presenter);
}

void go2_presenter_post(go2_presenter_t* presenter, go2_surface_t* surface, int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, go2_rotation_t rotation, int scale_mode)
{
    sem_wait(&presenter->freeSem);


    pthread_mutex_lock(&presenter->queueMutex);

    if (go2_queue_count_get(presenter->freeFrameBuffers) < 1)
    {
        printf("no framebuffer available.\n");
        abort();
    }

    go2_frame_buffer_t* dstFrameBuffer = go2_queue_pop(presenter->freeFrameBuffers);

    pthread_mutex_unlock(&presenter->queueMutex);


    go2_surface_t* dstSurface = go2_frame_buffer_surface_get(dstFrameBuffer);

    rga_info_t dst = { 0 };
    dst.fd = go2_surface_prime_fd(dstSurface);
    dst.mmuFlag = 1;
    dst.rect.xoffset = 0;
    dst.rect.yoffset = 0;
    dst.rect.width = go2_surface_width_get(dstSurface);
    dst.rect.height = go2_surface_height_get(dstSurface);
    dst.rect.wstride = go2_surface_stride_get(dstSurface) / (go2_drm_format_get_bpp(go2_surface_format_get(dstSurface)) / 8);
    dst.rect.hstride = go2_surface_height_get(dstSurface);
    dst.rect.format = go2_rkformat_get(go2_surface_format_get(dstSurface));
    dst.color = presenter->background_color;

    int ret = c_RkRgaColorFill(&dst);
    if (ret)
    {
        printf("c_RkRgaColorFill failed.\n");
    }


    go2_surface_blit(surface, srcX, srcY, srcWidth, srcHeight, dstSurface, dstX, dstY, dstWidth, dstHeight, rotation, scale_mode);


    pthread_mutex_lock(&presenter->queueMutex);
    go2_queue_push(presenter->usedFrameBuffers, dstFrameBuffer);
    pthread_mutex_unlock(&presenter->queueMutex);

    sem_post(&presenter->usedSem);
}

#define BUFFER_MAX (3)

typedef struct buffer_surface_pair
{
    struct gbm_bo* gbmBuffer;
    go2_surface_t* surface;
} buffer_surface_pair_t;

typedef struct go2_context
{
    go2_display_t* display;
    int width;
    int height;
    go2_context_attributes_t attributes;
    struct gbm_device* gbmDevice;
    EGLDisplay eglDisplay;
    struct gbm_surface* gbmSurface;
    EGLSurface eglSurface;
    EGLContext eglContext;
    uint32_t drmFourCC;
    buffer_surface_pair_t bufferMap[BUFFER_MAX];
    int bufferCount;
} go2_context_t;



static EGLConfig FindConfig(EGLDisplay eglDisplay, int redBits, int greenBits, int blueBits, int alphaBits, int depthBits, int stencilBits)
{
    EGLint configAttributes[] =
    {
        EGL_RED_SIZE,            redBits,
        EGL_GREEN_SIZE,          greenBits,
        EGL_BLUE_SIZE,           blueBits,
        EGL_ALPHA_SIZE,          alphaBits,

        EGL_DEPTH_SIZE,          depthBits,
        EGL_STENCIL_SIZE,        stencilBits,

        EGL_SURFACE_TYPE,        EGL_WINDOW_BIT,

        EGL_NONE
    };


    int num_configs;
    EGLBoolean success = eglChooseConfig(eglDisplay, configAttributes, NULL, 0, &num_configs);
    if (success != EGL_TRUE)
    {
        printf("eglChooseConfig failed.\n");
        abort();
    }


    //EGLConfig* configs = new EGLConfig[num_configs];
    EGLConfig configs[num_configs];
    success = eglChooseConfig(eglDisplay, configAttributes, configs, num_configs, &num_configs);
    if (success != EGL_TRUE)
    {
        printf("eglChooseConfig failed.\n");
        abort();
    }


    EGLConfig match = 0;
    for (int i = 0; i < num_configs; ++i)
    {
        EGLint configRedSize;
        EGLint configGreenSize;
        EGLint configBlueSize;
        EGLint configAlphaSize;
        EGLint configDepthSize;
        EGLint configStencilSize;

        eglGetConfigAttrib(eglDisplay, configs[i], EGL_RED_SIZE, &configRedSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_GREEN_SIZE, &configGreenSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_BLUE_SIZE, &configBlueSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_ALPHA_SIZE, &configAlphaSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_DEPTH_SIZE, &configDepthSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_STENCIL_SIZE, &configStencilSize);

        //printf("Egl::FindConfig: index=%d, red=%d, green=%d, blue=%d, alpha=%d\n",
        //	i, configRedSize, configGreenSize, configBlueSize, configAlphaSize);

        if (configRedSize == redBits &&
            configBlueSize == blueBits &&
            configGreenSize == greenBits &&
            configAlphaSize == alphaBits &&
            configDepthSize == depthBits &&
            configStencilSize == stencilBits)
        {
            match = configs[i];
            break;
        }
    }

    return match;
}

go2_context_t* go2_context_create(go2_display_t* display, int width, int height, const go2_context_attributes_t* attributes)
{
    EGLBoolean success;


    go2_context_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));


    result->display = display;
    result->width = width;
    result->height = height;
    result->attributes = *attributes;


    result->gbmDevice = gbm_create_device(display->fd);
    if (!result->gbmDevice)
    {
        printf("gbm_create_device failed.\n");
        goto err_00;
    }


    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
    get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    if(get_platform_display == NULL)
    {
        printf("eglGetProcAddress failed.\n");
        goto err_01;
    }

    result->eglDisplay = get_platform_display(EGL_PLATFORM_GBM_KHR, result->gbmDevice, NULL);
    if (result->eglDisplay == EGL_NO_DISPLAY)
    {
        printf("eglGetPlatformDisplayEXT failed.\n");
        goto err_01;
    }


    // Initialize EGL
    EGLint major;
    EGLint minor;
    success = eglInitialize(result->eglDisplay, &major, &minor);
    if (success != EGL_TRUE)
    {
        printf("eglInitialize failed.\n");
        goto err_01;
    }

    printf("EGL: major=%d, minor=%d\n", major, minor);
    printf("EGL: Vendor=%s\n", eglQueryString(result->eglDisplay, EGL_VENDOR));
    printf("EGL: Version=%s\n", eglQueryString(result->eglDisplay, EGL_VERSION));
    printf("EGL: ClientAPIs=%s\n", eglQueryString(result->eglDisplay, EGL_CLIENT_APIS));
    printf("EGL: Extensions=%s\n", eglQueryString(result->eglDisplay, EGL_EXTENSIONS));
    printf("EGL: ClientExtensions=%s\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    printf("\n");


    EGLConfig eglConfig = FindConfig(result->eglDisplay, attributes->red_bits, attributes->green_bits,
        attributes->blue_bits, attributes->alpha_bits, attributes->depth_bits, attributes->stencil_bits);


    // Get the native visual id associated with the config
    //int visual_id;
    eglGetConfigAttrib(result->eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, (EGLint*)&result->drmFourCC);

    result->gbmSurface = gbm_surface_create(result->gbmDevice,
        width,
        height,
        result->drmFourCC,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!result->gbmSurface)
    {
        printf("gbm_surface_create failed.\n");
        abort();
    }


    result->eglSurface = eglCreateWindowSurface(result->eglDisplay, eglConfig, (EGLNativeWindowType)result->gbmSurface, NULL);	
    if (result->eglSurface == EGL_NO_SURFACE)
    {
        printf("eglCreateWindowSurface failed\n");
        abort();
    }


    // Create a context
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, attributes->major,
        EGL_NONE };

    result->eglContext = eglCreateContext(result->eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (result->eglContext == EGL_NO_CONTEXT)
    {
        printf("eglCreateContext failed\n");
        abort();
    }

    success = eglMakeCurrent(result->eglDisplay, result->eglSurface, result->eglSurface, result->eglContext);
    if (success != EGL_TRUE)
    {
        printf("eglMakeCurrent failed\n");
        abort();
    }


    return result;


err_01:
    gbm_device_destroy(result->gbmDevice);

err_00:
    free(result);
    return NULL;
}

void go2_context_destroy(go2_context_t* context)
{
    eglDestroyContext(context->eglDisplay, context->eglContext);
    eglDestroySurface(context->eglDisplay, context->eglSurface);
    gbm_surface_destroy(context->gbmSurface);
    eglTerminate(context->eglDisplay);
    gbm_device_destroy(context->gbmDevice);

    for(int i = 0; i < context->bufferCount; ++i)
    {
        free(context->bufferMap[i].surface);
    }

    free(context);
}

void* go2_context_egldisplay_get(go2_context_t* context)
{
    return context->eglDisplay;
}

void go2_context_make_current(go2_context_t* context)
{
    EGLBoolean success = eglMakeCurrent(context->eglDisplay, context->eglSurface, context->eglSurface, context->eglContext);
    if (success != EGL_TRUE)
    {
        printf("eglMakeCurrent failed\n");
        abort();
    }
}

void go2_context_swap_buffers(go2_context_t* context)
{
    EGLBoolean ret = eglSwapBuffers(context->eglDisplay, context->eglSurface);
    if (ret == EGL_FALSE)
    {
        printf("eglSwapBuffers failed\n");
        //abort();
    }
}

go2_surface_t* go2_context_surface_lock(go2_context_t* context)
{
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(context->gbmSurface);
    if (!bo)
    {
        printf("gbm_surface_lock_front_buffer failed.\n");
        abort();
    }

    go2_surface_t* surface = NULL;
    for (int i = 0; i < context->bufferCount; ++i)
    {
        buffer_surface_pair_t* pair = &context->bufferMap[i];
        if (pair->gbmBuffer == bo)
        {
            surface = pair->surface;
            break;
        }
    }

    if (!surface)
    {
        if (context->bufferCount >= BUFFER_MAX)
        {
            printf("swap buffers count exceeded.\n");
            abort();
        }

        surface = malloc(sizeof(*surface));
        if (!surface)
        {
            printf("malloc failed.\n");
            abort();
        }

        memset(surface, 0, sizeof(*surface));


        surface->display = context->display;
        surface->gem_handle = gbm_bo_get_handle(bo).u32;
        surface->size = gbm_bo_get_stride(bo);
        surface->width = gbm_bo_get_width(bo);
        surface->height = gbm_bo_get_height(bo);
        surface->stride = gbm_bo_get_stride(bo);
        surface->format = context->drmFourCC;


        buffer_surface_pair_t* pair = &context->bufferMap[context->bufferCount++];
        pair->gbmBuffer = bo;
        pair->surface = surface;

        //printf("added buffer - bo=%p, count=%d\n", bo, context->bufferCount);
    }

    return surface;
}

void go2_context_surface_unlock(go2_context_t* context, go2_surface_t* surface)
{
    struct gbm_bo* bo = NULL;
    for (int i = 0; i < context->bufferCount; ++i)
    {
        buffer_surface_pair_t* pair = &context->bufferMap[i];
        if (pair->surface == surface)
        {
            bo = pair->gbmBuffer;
            break;
        }
    }

    if (!bo)
    {
        abort();
    }

    gbm_surface_release_buffer(context->gbmSurface, bo);
}

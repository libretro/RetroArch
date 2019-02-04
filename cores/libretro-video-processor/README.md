# libretro-video-processor
Libretro core for V4L2 capture devices

The basic idea is this -- plug your legacy console into a capture device and use RetroArch to upscale it and apply shaders to taste.

## Raspberry Pi specific config

Add to /boot/config.txt:
```
gpu_mem_256=112
gpu_mem_512=368
cma_lwm=16
cma_hwm=32
cma_offline_start=16
```

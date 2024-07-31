# 赛题提交报告

## 背景情况说明

RetroArch 前端其实不需要任何源代码的移植，在 Debian Sid 上直接安装，或在其他 Linux 开发版上编译安装，即可运行 RetroArch 图形前端；配置方法详见下方的方案。  
PPSSPP 作为后端核心，上游也已经完成了对 RISC-V 的移植并提供了各种优化，如 [Initial RISC-V jit based on IR](https://github.com/hrydgard/ppsspp/pull/17751).
然而 flycast 作为 Sega Dreamcast 的模拟器，因为年代久远，目前暂不支持 RISC-V 架构；个人认为 flycast 的后端移植和优化较为困难，因为涉及到了指令集的转译，是需要源代码的架构移植和优化。

本人在此提出两种方案使 SG2042 平台上运行 RetroArch 和 PPSSPP 核心。

## 第一种方案

安装 Debian Sid，推荐通过 `debianbootstrap` 来快速安装，参考 [Debootstrap - Debian Wiki](https://wiki.debian.org/Debootstrap)；之后替换 Debian 上游内核为算能的 SG2042 内核，同时需注意 bootloader 的配置，且推荐采用 UEFI 的启动模式，以免无法启动运行 Debian；之后通过 `apt install retroarch` 来安装 RetroArch；参考 [Raspberry Pi - Libretro Docs](https://docs.libretro.com/guides/rpi/) 可通过以下命令来编译 PPSSPP 核心并安装：

```
git clone https://github.com/libretro/libretro-super
cd libretro-super ./libretro-fetch.sh ppsspp
./libretro-build.sh ppsspp
cp dist/unix/*.so ~/.config/retroarch/cores
```


## 第二种方案

考虑到 SG2042 上 Linux 发行版众多，桌面环境和 GPU 图形驱动，环境配置不一，也推荐从源码编译 RetroArch, 参考官方文档 [Overview for Linux/BSD - Libretro Docs](https://docs.libretro.com/development/retroarch/compilation/linux-and-bsd/).

考虑到兼容性，服务器和桌面环境，个人的编译配置上暂时取消掉了对 Wayland, mbedtls, bearssl 的支持，通过 `./configure --disable-wayland --disable-mbedtls --diable-bearssl` 来实现。  
在 Pioneer Box 服务器 rvbox12 上本人编译的二进制位于 `/home/y449xu/myretro/RetroArch/retroarch`，在 X11 桌面环境下，GPU 连接的显示屏上应可以直接运行。  
libretro-ppsspp 则安装在 `/home/y449xu/.config/retroarch/cores`，复制 core 到当前的非 root 且有图形权限的用户，后运行 RetroArch 即可。


### 后日谈

在 rvbox12 上也尝试过用 X11 Forwarding 来实现远程验证 RetroArch 的运行，然而可能因为桌面环境配置的缺失，如 `graphical.target`，或者是 GPU 离屏渲染 (GPU off-screen rendering) 功能尚未在 RetroArch 上实现，导致远程运行时报段错误；利用远程桌面，如 VNC，也很有可能无法运行 RetroArch, 个人验证只好作罢。

## 总结

我在此赛题上付出了精力，收获了知识和快乐；不管结果如何，也一直想在 RISC-V 软件生态建设上出一份力，让 RISC-V 能够在更多的软件上闪亮出彩。 

队伍名称：Crossa  
参赛选手：许彦骐
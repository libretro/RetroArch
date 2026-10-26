# Temporal upscaling: DLSS, FSR and XeSS

A temporal upscaler lets a core render at a lower resolution and turns
that into a sharp, anti-aliased picture at your screen's resolution. It
uses the core's previous frames, depth and motion to do it. RetroArch
can use three of them:

- **NVIDIA DLSS Super Resolution**, on NVIDIA RTX graphics cards.
- **AMD FidelityFX Super Resolution (FSR)**: FSR 4 on the AMD cards that
  have it, FSR 3.1 on any other card.
- **Intel XeSS Super Resolution**, on any recent card.

RetroArch does not ship any of these libraries. You download them from
NVIDIA, AMD or Intel and put them in a folder, as described below.

## What you need

- **Windows (64-bit) or Linux (x86-64 or ARM64).**
- **The `vulkan` or `d3d12` video driver.** Set it in **Settings >
  Drivers > Video** and restart RetroArch. Other video drivers do not
  offer the upscaler.
- **A core that supports it.** The core has to give RetroArch its depth
  and motion information. Currently that is **parallel-dc**
  (Dreamcast/NAOMI), with one of its Vulkan or Direct3D 12 renderers.
  Every other core runs exactly as before, and RetroArch does not load
  any upscaler library for it.
- **The upscaler's library**, in its folder (see below).

## Where the files go

Each upscaler has its own folder inside your RetroArch folder:

| Upscaler | Folder | Files |
| --- | --- | --- |
| DLSS | `upscalers/dlss` | Windows: `nvngx_dlss.dll`. Linux: `libnvidia-ngx-dlss.so.<version>` |
| FSR | `upscalers/fsr` | Direct3D 12: `amd_fidelityfx_loader_dx12.dll` with `amd_fidelityfx_upscaler_dx12.dll` (FidelityFX SDK 2), or `amd_fidelityfx_dx12.dll` (SDK 1.1). Vulkan: `amd_fidelityfx_vk.dll` (SDK 1.1). |
| XeSS | `upscalers/xess` | Windows: `libxess.dll`. Linux: `libxess.so` |

For example, on Windows with RetroArch in `C:\RetroArch`, DLSS goes in
`C:\RetroArch\upscalers\dlss\nvngx_dlss.dll`. Create the folders if
they do not exist.

To use other folders, set them in **Settings > Video > Scaling** with
**DLSS Directory**, **FSR Directory** and **XeSS Directory**.

## Getting the libraries

### DLSS

1. Download the DLSS SDK from NVIDIA's GitHub page, **NVIDIA/DLSS**:
   use *Code > Download ZIP*, or a release.
2. Copy the library into `upscalers/dlss`:
   - Windows: `lib/Windows_x86_64/rel/nvngx_dlss.dll`.
   - Linux: `lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.<version>`.
     Keep the full file name; RetroArch reads the version from it.
3. Make sure your NVIDIA driver is up to date. DLSS also needs the NGX
   library the driver installs, and RetroArch finds that one by itself.

A newer `nvngx_dlss.dll` gives newer models (the presets below). You
can swap in a newer copy at any time. The version RetroArch finds is
shown in System Information.

### FSR

1. Download the **AMD FidelityFX SDK** from AMD's GitHub page,
   **GPUOpen-LibrariesAndSDKs/FidelityFX-SDK**, under Releases.
2. Copy the prebuilt, signed libraries into `upscalers/fsr`:
   - For **d3d12**: from SDK 2, `amd_fidelityfx_loader_dx12.dll` and
     `amd_fidelityfx_upscaler_dx12.dll`. From SDK 1.1,
     `amd_fidelityfx_dx12.dll`.
   - For **vulkan**: `amd_fidelityfx_vk.dll`, from SDK 1.1. SDK 2 has no
     Vulkan library.

FSR 4 is only in the Direct3D 12 libraries, and only on graphics cards
that support it. Everywhere else FSR runs as FSR 3.1.

### XeSS

1. Download the **XeSS SDK** from Intel's GitHub page, **intel/xess**,
   under Releases.
2. Copy `libxess.dll` from its `bin` folder into `upscalers/xess`. On
   Linux, copy `libxess.so`, where Intel's SDK provides one.

## Turning it on

1. Load a core that supports the upscaler, with the `vulkan` or `d3d12`
   driver (see above).
2. Open **Settings > Video > Scaling**.
3. Set **Temporal Upscaler** to DLSS, FSR or XeSS.
4. Close and reload the content. The core has to ask for the upscaler
   when it starts, so the change applies from the next content load.

In parallel-dc, use a Vulkan or Direct3D 12 renderer.

### Settings

All of these are in **Settings > Video > Scaling**:

- **Temporal Upscaler**: Off, DLSS, FSR or XeSS.
- **Temporal Upscaler Quality**: how much smaller than the screen the
  core renders.
  - Quality: about 2/3 of the output size.
  - Balanced: about 58%.
  - Performance: 1/2.
  - Ultra Performance: 1/3.
  - Native: full size, anti-aliasing only.
  - Auto: Quality, or Balanced above 1440 lines.

  A lower setting is faster but softer.
- **Temporal Upscaler Preset** (DLSS only): which DLSS model runs.
  - Default lets the DLSS runtime choose.
  - **K** is the DLSS 4 transformer model. It gives the best picture
    and costs a little more GPU time.
  - **J** is similar, with less ghosting but some flicker.
  - **A** to **G** are the older models from DLSS 2 and 3.
  - Letters your `nvngx_dlss.dll` doesn't have are marked "(not in this
    DLSS runtime)", and fall back to Default.

  Changes apply on the next frame.
- **Temporal Upscaler Sharpness**: extra sharpening, in percent; 0 is
  off. FSR has its own sharpening pass. DLSS 2.5.1 and newer have no
  sharpening and ignore this setting, unless OptiScaler is in use (see
  below).

## Checking that it works

Open **Information > System Information** and look for the **Temporal
Upscaler** line, for example:

```
Temporal Upscaler: DLSS (NGX: found, DLSS runtime 310.9.1)
```

- **NGX: found** means the NVIDIA driver's NGX library is there.
- **DLSS runtime 310.9.1** is the version of the `nvngx_dlss.dll` in
  your DLSS folder.
- **"not in DLSS Directory"**, or **"FSR library: not in FSR
  Directory"**, means RetroArch did not find the file. Check the folder
  and the file name.

The log (**Settings > Logging**, with logging to file on) says what was
loaded from where when content starts, and why if something failed.

## OptiScaler

[OptiScaler](https://github.com/optiscaler/OptiScaler) is a third-party
tool. It stands in for NVIDIA's NGX library and can run FSR or XeSS
wherever a program asks for DLSS, including on AMD and Intel cards.

To use it with RetroArch:

1. Put OptiScaler's DLL in the RetroArch folder, next to
   `retroarch.exe`, renamed to `winmm.dll`. Put OptiScaler's
   `OptiScaler.ini` next to it.
2. Keep a `nvngx_dlss.dll` in `upscalers/dlss`. RetroArch needs one
   there before it uses DLSS.
3. Set **Temporal Upscaler** to **DLSS**.

Don't name it `nvngx.dll`. Next to `retroarch.exe` under that name,
OptiScaler takes itself for NVIDIA's NGX library and passes every call
back to itself, and RetroArch freezes or crashes when content starts.
As `winmm.dll` it loads when RetroArch starts and passes the calls on
to NVIDIA's own NGX library.

System Information then shows **via OptiScaler**, and the log names
OptiScaler as the NGX library. OptiScaler's own menu opens with the
Insert key. The **Temporal Upscaler Sharpness** setting drives
OptiScaler's sharpening pass for every upscaler it runs.

Loaded as `winmm.dll`, OptiScaler hooks into RetroArch for every core,
not just the ones that use the upscaler. If a core misbehaves with it,
rename or remove `winmm.dll`; RetroArch then uses NVIDIA's NGX library
directly.

## Troubleshooting

- **The Temporal Upscaler setting is missing**: the video driver is not
  `vulkan` or `d3d12`.
- **It's on, but the picture looks the same**: the core does not
  support the upscaler, or you haven't reloaded content since turning
  it on. Also check the core's renderer.
- **DLSS: "NGX: not found"**: update the NVIDIA driver. DLSS needs an
  RTX card.
- **The DLSS version looks wrong**: RetroArch shows the version stored
  in the file itself, the same one Windows shows under the file's
  *Properties > Details*.

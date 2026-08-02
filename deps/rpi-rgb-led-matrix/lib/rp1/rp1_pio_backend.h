// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#ifndef RP1_PIO_BACKEND_H
#define RP1_PIO_BACKEND_H

struct HardwareMapping;

namespace rgb_matrix {
namespace internal {
class Framebuffer;

// Internal Pi 5-family RP1 PIO renderer.
//
// Expected call flow:
// - detect/select with PlatformDetected()/ConfigSupported()/ShouldActivate()
// - push runtime timing with SetGpioSlowdown()
// - initialize once with InitOrDie()
// - optionally run panel-specific startup via InitializePanels()
// - stream each refresh via DumpFramebuffer()
// - release claimed state-machine resources with Deinit()
bool Rp1PioPlatformDetected();
bool Rp1PioConfigSupported(const char *hardware_mapping, int row_address_type,
                           int parallel);
bool Rp1PioShouldActivate(const char *hardware_mapping, int row_address_type,
                          int parallel);
void Rp1PioSetGpioSlowdown(int slowdown);
bool Rp1PioIsActive();
void Rp1PioInitOrDie(const HardwareMapping &mapping, int double_rows, int parallel,
                     int pwm_lsb_nanoseconds, int dither_bits,
                     int row_address_type);
void Rp1PioInitializePanels(const HardwareMapping &mapping,
                            const char *panel_type, int columns);
void Rp1PioDumpFramebuffer(Framebuffer *framebuffer, int pwm_low_bit);
void Rp1PioDeinit();

}  // namespace internal
}  // namespace rgb_matrix

#endif  // RP1_PIO_BACKEND_H

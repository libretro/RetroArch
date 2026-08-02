// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#ifndef RPI_RP1_RIO_BACKEND_H
#define RPI_RP1_RIO_BACKEND_H

struct HardwareMapping;

namespace rgb_matrix {
namespace internal {
class Framebuffer;

// Internal Pi 5-family RP1 RIO renderer.
//
// This backend drives the RP1 GPIO fabric directly through MMIO. The lifecycle
// is the same as the PIO backend, with SetEnabled()/BackendRequested() carrying
// the user-facing runtime choice before activation.
bool Rp1RioPlatformDetected();
void Rp1RioSetEnabled(bool enabled);
bool Rp1RioBackendRequested();
bool Rp1RioShouldActivate(const char *hardware_mapping, int row_address_type,
                          int parallel);
void Rp1RioSetGpioSlowdown(int slowdown);
bool Rp1RioIsActive();
void Rp1RioInitOrDie(const HardwareMapping &mapping, int double_rows,
                     int parallel, int pwm_lsb_nanoseconds, int dither_bits,
                     int row_address_type);
void Rp1RioInitializePanels(const HardwareMapping &mapping,
                            const char *panel_type, int columns);
void Rp1RioDumpFramebuffer(Framebuffer *framebuffer, int pwm_low_bit);
void Rp1RioDeinit();

}  // namespace internal
}  // namespace rgb_matrix

#endif  // RPI_RP1_RIO_BACKEND_H

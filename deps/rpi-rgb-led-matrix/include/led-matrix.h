// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

// Controlling 16x32 or 32x32 RGB matrixes via GPIO. It allows daisy chaining
// of a string of these, and also connecting a parallel string on newer
// Raspberry Pis with more GPIO pins available.

#ifndef RPI_RGBMATRIX_H
#define RPI_RGBMATRIX_H

#include <stdint.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "thread.h"
#include "pixel-mapper.h"
#include "graphics.h"

namespace rgb_matrix {
class RGBMatrix;
class FrameCanvas;   // Canvas for Double- and Multibuffering
struct RuntimeOptions;

// The RGB matrix provides the framebuffer and the facilities to constantly
// update the LED matrix.
//
// This implement the Canvas interface that represents the display with
// (led_cols * chained_displays)x(rows * parallel_displays) pixels.
//
// If can do multi-buffering using the CreateFrameCanvas() and SwapOnVSync()
// methods. This is useful for animations and to prevent tearing.
//
// If you arrange the panels in a different way in the physical space, write
// a CanvasTransformer that does coordinate remapping and which should be added
// to the transformers, like with UArrangementTransformer in demo-main.cc.
class RGBMatrix : public Canvas {
public:
  // Options to initialize the RGBMatrix. Also see the main README.md for
  // detailed descriptions of the command line flags.
  struct Options {
    Options();   // Creates a default option set.

    // Validate the options and possibly output a message to string. If
    // "err" is NULL, outputs validation problems to stderr.
    // Returns 'true' if all options look good.
    bool Validate(std::string *err) const;

    // Name of the hardware mapping. Something like "regular" or "adafruit-hat"
    const char *hardware_mapping;

    // The "rows" are the number
    // of rows supported by the display, so 32 or 16. Default: 32.
    // Flag: --led-rows
    int rows;

    // The "cols" are the number of columns per panel. Typically something
    // like 32, but also 64 is possible. Sometimes even 40.
    // cols * chain_length is the total length of the display, so you can
    // represent a 64 wide display as cols=32, chain=2 or cols=64, chain=1;
    // same thing, but more convenient to think of.
    // Flag: --led-cols
    int cols;

    // The chain_length is the number of displays daisy-chained together
    // (output of one connected to input of next). Default: 1
    // Flag: --led-chain
    int chain_length;

    // The number of parallel chains connected to the Pi; in old Pis with 26
    // GPIO pins, that is 1, in newer Pis with 40 interfaces pins, that can
    // also be 2 or 3. The effective number of pixels in vertical direction is
    // then thus rows * parallel. Default: 1
    // Flag: --led-parallel
    int parallel;

    // Set PWM bits used for output. Default is 11, but if you only deal with
    // limited comic-colors, 1 might be sufficient. Lower require less CPU and
    // increases refresh-rate.
    // Flag: --led-pwm-bits
    int pwm_bits;

    // Change the base time-unit for the on-time in the lowest
    // significant bit in nanoseconds.
    // Higher numbers provide better quality (more accurate color, less
    // ghosting), but have a negative impact on the frame rate.
    // Flag: --led-pwm-lsb-nanoseconds
    int pwm_lsb_nanoseconds;

    // The lower bits can be time-dithered for higher refresh rate.
    // Flag: --led-pwm-dither-bits
    int pwm_dither_bits;

    // The initial brightness of the panel in percent. Valid range is 1..100
    // Default: 100
    // Flag: --led-brightness
    int brightness;

    // Scan mode: 0=progressive, 1=interlaced.
    // Flag: --led-scan-mode
    int scan_mode;

    // Default row address type is 0, corresponding to direct setting of the
    // row, while row address type 1 is used for panels that only have A/B,
    // typically some 64x64 panels
    int row_address_type;  // Flag --led-row-addr-type

    // Type of multiplexing. 0 = direct, 1 = stripe, 2 = checker,...
    // Flag: --led-multiplexing
    int multiplexing;

    // Disable the PWM hardware subsystem to create pulses.
    // Typically, you don't want to disable hardware pulsing, this is mostly
    // for debugging and figuring out if there is interference with the
    // sound system.
    // This won't do anything if output enable is not connected to GPIO 18 in
    // non-standard wirings.
    bool disable_hardware_pulsing;     // Flag: --led-hardware-pulse

    // Show refresh rate on the terminal for debugging and tweaking purposes.
    bool show_refresh_rate;            // Flag: --led-show-refresh

    // Some panels have inversed colors.
    bool inverse_colors;                // Flag: --led-inverse

    // In case the internal sequence of mapping is not "RGB", this contains the
    // real mapping. Some panels mix up these colors. String of length three
    // which has to contain all characters R, G and B.
    const char *led_rgb_sequence;  // Flag: --led-rgb-sequence

    // A string describing a sequence of pixel mappers that should be applied
    // to this matrix. A semicolon-separated list of pixel-mappers with optional
    // parameter.
    const char *pixel_mapper_config;   // Flag: --led-pixel-mapper

    // Panel type. Typically an empty string or NULL, but some panels need
    // a particular initialization sequence, so this is used for that.
    // This can be e.g. "FM6126A" for that particular panel type.
    const char *panel_type;  // Flag: --led-panel-type

    // Limit refresh rate of LED panel. This will help on a loaded system
    // to keep a constant refresh rate. <= 0 for no limit.
    int limit_refresh_rate_hz;   // Flag: --led-limit-refresh

    // Sleep instead of busy wait to free CPU cycles but get slightly less
    // accurate frame timing.
    bool disable_busy_waiting;   // Flag: --led-busy-waiting
  };

  // Factory to create a matrix. Additional functionality includes dropping
  // privileges and becoming a daemon.
  // Returns NULL, if there was a problem (a message then is written to stderr).
  static RGBMatrix *CreateFromOptions(const Options &options,
                                      const RuntimeOptions &runtime_options);

  // A factory that parses your main() commandline flags to read options
  // meant to configure the the matrix and returns a freshly allocated matrix.
  //
  // Optionally,  you can pass in option structs with a couple of defaults
  // which are used unless overwritten on the command line.
  // A matrix is created and returned; also the options structs are
  // updated to reflect the values that were used and set on the command line.
  //
  // If you allow the user to start a daemon with --led-daemon, make sure to
  // call this function before you have started any threads, so early on in
  // main() (see RuntimeOptions documentation).
  //
  // Note, the permissions are dropped by default from 'root' to 'daemon', so
  // if you are required to stay root after this, disable this option in
  // the default RuntimeOptions (set drop_privileges = -1).
  // Returns NULL, if there was a problem (a message then is written to stderr).
  static RGBMatrix *CreateFromFlags(int *argc, char ***argv,
                                    RGBMatrix::Options *default_options = NULL,
                                    RuntimeOptions *default_runtime_opts = NULL,
                                    bool remove_consumed_flags = true);

  // Stop matrix, delete all resources.
  virtual ~RGBMatrix();

  // -- Canvas interface. These write to the active FrameCanvas
  // (see documentation in canvas.h)
  //
  // Since this is updating the canvas that is currently displayed, this
  // might result in tearing.
  // Prefer using a FrameCanvas and do double-buffering, see section below.
  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue);
  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);

  // -- Double- and Multibuffering.

  // Create a new buffer to be used for multi-buffering. The returned new
  // Buffer implements a Canvas with the same size as this RGBMatrix.
  // You can use it to draw off-screen on it, then swap it with the active
  // buffer using SwapOnVSync(). That would be classic double-buffering.
  //
  // You can also create as many FrameCanvas as you like and for instance use
  // them to pre-fill scenes of an animation for fast playback later.
  //
  // The ownership of the created Canvases remains with the RGBMatrix, so you
  // don't have to worry about deleting them (but you also don't want to create
  // more than needed as this will fill up your memory as they are only deleted
  // when the RGBMatrix is deleted).
  FrameCanvas *CreateFrameCanvas();

  // This method waits to the next VSync and swaps the active buffer with the
  // supplied buffer. The formerly active buffer is returned.
  //
  // If you pass in NULL, the active buffer is returned, but it won't be
  // replaced with NULL. You can use the NULL-behavior to just wait on
  // VSync or to retrieve the initial buffer when preparing a multi-buffer
  // animation.
  //
  // The optional "framerate_fraction" parameter allows to choose which
  // multiple of the global frame-count to use. So it slows down your animation
  // to an exact integer fraction of the refresh rate.
  // Default is 1, so immediately next available frame.
  // (Say you have 140Hz refresh rate, then a value of 5 would give you an
  // 28Hz animation, nicely locked to the refresh-rate).
  // If you combine this with Options::limit_refresh_rate_hz you can create
  // time-correct animations.
  FrameCanvas *SwapOnVSync(FrameCanvas *other, unsigned framerate_fraction = 1);

  // -- Setting shape and behavior of matrix.

  // Apply a pixel mapper. This is used to re-map pixels according to some
  // scheme implemented by the PixelMapper. Does _not_ take ownership of the
  // mapper. Mapper can be NULL, in which case nothing happens.
  // Returns a boolean indicating if this was successful.
  bool ApplyPixelMapper(const PixelMapper *mapper);

  // Note, there used to be ApplyStaticTransformer(), which has been deprecated
  // since 2018 and changed to a compile-time option, then finally removed
  // in 2020. Use PixelMapper instead, which is simpler and more intuitive.

  // Set PWM bits used for output. Default is 11, but if you only deal with
  // limited comic-colors, 1 might be sufficient. Lower require less CPU and
  // increases refresh-rate.
  //
  // Returns boolean to signify if value was within range.
  //
  // This sets the PWM bits for the current active FrameCanvas and future
  // ones that are created with CreateFrameCanvas().
  bool SetPWMBits(uint8_t value);
  uint8_t pwmbits();   // return the pwm-bits of the currently active buffer.

  // Map brightness of output linearly to input with CIE1931 profile.
  void set_luminance_correct(bool on);
  bool luminance_correct() const;

  // Set brightness in percent for all created FrameCanvas. 1%..100%.
  // This will only affect newly set pixels.
  void SetBrightness(uint8_t brightness);
  uint8_t brightness();

  //-- GPIO interaction.
  // This library uses the GPIO pins to drive the matrix; this is a safe way
  // to request the 'remaining' bits to be used for user purposes.

  // Request user readable GPIO bits.
  // This function allows you to request pins you'd like to read with
  // AwaitInputChange().
  // Only bits that are not already in use for reading or wrtiting
  // by the matrix are allowed.
  // Input is a bitmap of all the GPIO bits you're interested in; returns all
  // the bits that are actually available.
  uint64_t RequestInputs(uint64_t all_interested_bits);

  // This function will return whenever the GPIO input pins
  // change (pins that are not already in use for output, that is) or the
  // timeout is reached. You need to have reserved the inputs with
  // matrix->RequestInputs(...) first (e.g.
  //   matrix->RequestInputs((1<<25)|(1<<24));
  //
  // A positive timeout waits the given amount of milliseconds for a change
  // (e.g. a button-press) to occur; if there is no change, it will just
  // return the last value.
  // If you just want to know how the pins are right now, call with zero
  // timeout.
  // A negative number waits forever and will only return if there is a change.
  //
  // This function only samples between display refreshes so polling some
  // input does not generate flicker and provide a convenient change interface.
  //
  // Returns the bitmap of all GPIO input pins.
  uint64_t AwaitInputChange(int timeout_ms);

  // Request user writable GPIO bits.
  // This allows to request a bitmap of GPIO-bits to be used by the user for
  // writing.
  // Only bits that are not already in use for reading or wrtiting
  // by the matrix are allowed.
  // Returns the subset bits that are _actually_ available,
  uint64_t RequestOutputs(uint64_t output_bits);

  // Set the user-settable bits according to output bits.
  void OutputGPIO(uint64_t output_bits);

  // Legacy way to set gpio pins. We're not doing this anymore but need to
  // be source-compatible with old calls of the form
  // matrix->gpio()->RequestInputs(...)
  //
  // Don't use, use AwaitInputChange() directly.
  RGBMatrix *gpio() __attribute__((deprecated)) { return this; }

  //--  Rarely needed
  // Start the refresh thread.
  // This is only needed if you chose RuntimeOptions::daemon = -1 (see below),
  // otherwise the refresh thread is already started.
  bool StartRefresh();

private:
  class Impl;

  RGBMatrix(Impl *impl) : impl_(impl) {}
  Impl *const impl_;
};

namespace internal {
class Framebuffer;
}

class FrameCanvas : public Canvas {
public:
  // Set PWM bits used for this Frame.
  // Simple comic-colors, 1 might be sufficient (111 RGB, i.e. 8 colors).
  // Lower require less CPU.
  // Returns boolean to signify if value was within range.
  bool SetPWMBits(uint8_t value);
  uint8_t pwmbits();

  // Map brightness of output linearly to input with CIE1931 profile.
  void set_luminance_correct(bool on);
  bool luminance_correct() const;

  void SetBrightness(uint8_t brightness);
  uint8_t brightness();

  //-- Serialize()/Deserialize() are fast ways to store and re-create a canvas.

  // Provides a pointer to a buffer of the internal representation to
  // be copied out for later Deserialize().
  //
  // Returns a "data" pointer and the data "len" in the given out-paramters;
  // the content can be copied from there by the caller.
  //
  // Note, the content is not simply RGB, it is the opaque and platform
  // specific representation which allows to make deserialization very fast.
  // It is also bigger than just RGB; if you want to store it somewhere,
  // using compression is a good idea.
  void Serialize(const char **data, size_t *len) const;

  // Load data previously stored with Serialize(). Needs to be restored into
  // a FrameCanvas with exactly the same settings (rows, chain, transformer,...)
  // as serialized.
  // Returns 'false' if size is unexpected.
  // This method should only be called if FrameCanvas is off-screen.
  bool Deserialize(const char *data, size_t len);

  // Copy content from other FrameCanvas owned by the same RGBMatrix.
  void CopyFrom(const FrameCanvas &other);

  // -- Canvas interface.
  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue);
  virtual void SetPixels(int x, int y, int width, int height,
                         Color *colors);
  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);
  virtual void SubFill(int x, int y, int width, int height, uint8_t red, uint8_t green, uint8_t blue);
  
private:
  friend class RGBMatrix;

  FrameCanvas(internal::Framebuffer *frame) : frame_(frame){}
  virtual ~FrameCanvas();   // Any FrameCanvas is owned by RGBMatrix.
  internal::Framebuffer *framebuffer() { return frame_; }

  internal::Framebuffer *const frame_;
};

// Runtime options to simplify doing common things for many programs such as
// dropping privileges and becoming a daemon.
struct RuntimeOptions {
  RuntimeOptions();

  int gpio_slowdown;    // 0 = no slowdown.    Flag: --led-slowdown-gpio
  int rp1_pio;          // 0 = default RP1 RIO. 1 = RP1 PIO. Flag: --led-rp1-pio

  // ----------
  // If the following options are set to disabled with -1, they are not
  // even offered via the command line flags.
  // ----------

  //   -1 : don't leave choice of becoming daemon to the command line
  //        parsing. If set to -1, the --led-daemon option is not offered.
  //    0 : do not become a daemon, run in foreground (default value)
  //    0 : do not becoma a daemon, run in forgreound (default value)
  //    1 : become a daemon, run in background.
  //
  // If daemon is disabled (= -1), the user has to call
  // RGBMatrix::StartRefresh() manually once the matrix is created, to leave
  // the decision to become a daemon
  // after the call (which requires that no threads have been started yet).
  // In the other cases (off or on), the choice is already made, so the
  // thread is conveniently already started for you.
  int daemon;           // -1 disabled. 0=off, 1=on. Flag: --led-daemon

  // Drop privileges from 'root' to drop_priv_user/group once the hardware is
  // initialized.
  // This is usually a good idea unless you need to stay on elevated privs.
  // -1, 0, 1 similar meaning to 'daemon' above.
  int drop_privileges;  // -1 disabled. 0=off, 1=on. flag: --led-drop-privs

  // By default, the gpio is initialized for you, but if you run on a platform
  // not the Raspberry Pi, this will fail. If you don't need to access GPIO
  // e.g. you want to just create a stream output (see content-streamer.h),
  // set this to false.
  bool do_gpio_init;

  // If drop privileges is enabled, this is the user/group we drop privileges
  // to. Unless chosen otherwise, the default is "daemon" for user and group.
  const char *drop_priv_user;
  const char *drop_priv_group;
};

// Convenience utility functions to read standard rgb-matrix flags and create
// a RGBMatrix. Commandline flags are something like --led-rows, --led-chain,
// --led-parallel. See output of PrintMatrixFlags() for all available options
// and detailed description in
// https://github.com/hzeller/rpi-rgb-led-matrix#changing-parameters-via-command-line-flags
//
// Example use:
/*
using rgb_matrix::RGBMatrix;
int main(int argc, char **argv) {
  RGBMatrix::Options led_options;
  rgb_matrix::RuntimeOptions runtime;

  // Set defaults
  led_options.chain_length = 3;
  led_options.show_refresh_rate = true;
  runtime.drop_privileges = 1;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &led_options, &runtime)) {
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
  }

  // Do your own command line handling with the remaining flags.
  while (getopt()) {...}

  // Looks like we're ready to start
  RGBMatrix *matrix = RGBMatrix::CreateFromOptions(led_options, runtime);
  if (matrix == NULL) {
    return 1;
  }

  //  .. now use matrix

  delete matrix;   // Make sure to delete it in the end to switch off LEDs.
  return 0;
}
*/
// This parses the flags from argv and updates the structs with the parsed-out
// values. Structs can be NULL if you are not interested in it.
// The recognized flags are removed from argv if "remove_consumed_flags" is
// The recognized flags are removed from argv if "remove_consumed_flags" is
// true; this simplifies your command line processing for the remaining options.
//
// Returns 'true' on success, 'false' if there was flag parsing problem.
bool ParseOptionsFromFlags(int *argc, char ***argv,
                           RGBMatrix::Options *default_options,
                           RuntimeOptions *rt_options,
                           bool remove_consumed_flags = true);

// Show all the available options in a style that can be used in a --help
// output on the command line.
void PrintMatrixFlags(FILE *out,
                      const RGBMatrix::Options &defaults = RGBMatrix::Options(),
                      const RuntimeOptions &rt_opt = RuntimeOptions());

// Legacy version of RGBMatrix::CreateFromOptions()
inline RGBMatrix *CreateMatrixFromOptions(
  const RGBMatrix::Options &options,
  const RuntimeOptions &runtime_options) {
  return RGBMatrix::CreateFromOptions(options, runtime_options);
}

// Legacy version of RGBMatrix::CreateFromFlags()
inline RGBMatrix *CreateMatrixFromFlags(
  int *argc, char ***argv,
  RGBMatrix::Options *default_options = NULL,
  RuntimeOptions *default_runtime_opts = NULL,
  bool remove_consumed_flags = true) {
  return RGBMatrix::CreateFromFlags(argc, argv,
                                    default_options, default_runtime_opts,
                                    remove_consumed_flags);
}

}  // end namespace rgb_matrix
#endif  // RPI_RGBMATRIX_H

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

#include "led-matrix.h"

#include <assert.h>
#include <grp.h>
#include <pwd.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "gpio.h"
#include "rp1/rp1_pio_backend.h"
#include "rp1/rp1_rio_backend.h"
#include "thread.h"
#include "framebuffer-internal.h"
#include "multiplex-mappers-internal.h"

// C wrapper to reset global GPIO bookkeeping from external callers.
extern "C" void ledmatrix_reset_global_gpio();

// Global GPIO instance used by the library. Previously this was a function-
// local static inside CreateFromOptions; move it to file-scope so the C
// wrapper can access and reset it.
static rgb_matrix::GPIO s_global_io;  // file-scope global

// Implement the C wrapper here so it has file scope and can access
// the file-scope GPIO instance.
extern "C" void ledmatrix_reset_global_gpio() { s_global_io.ResetState(); }

// Leave this in here for a while. Setting things from old defines.
#if defined(ADAFRUIT_RGBMATRIX_HAT)
#  error "ADAFRUIT_RGBMATRIX_HAT has long been deprecated. Please use the Options struct or --led-gpio-mapping=adafruit-hat commandline flag"
#endif

#if defined(ADAFRUIT_RGBMATRIX_HAT_PWM)
#  error "ADAFRUIT_RGBMATRIX_HAT_PWM has long been deprecated. Please use the Options struct or --led-gpio-mapping=adafruit-hat-pwm commandline flag"
#endif

namespace rgb_matrix {
// Implementation details of RGBmatrix.
class RGBMatrix::Impl {
  class UpdateThread;
  friend class UpdateThread;

public:
  // Create an RGBMatrix.
  //
  // Needs an initialized GPIO object and configuration options from the
  // RGBMatrix::Options struct.
  //
  // If you pass an GPIO object (which has to be Init()ialized), it will start  // the internal thread to start the screen immediately.
  //
  // If you need finer control over when the refresh thread starts (which you
  // might when you become a daemon), pass NULL here and see SetGPIO() method.
  //
  // The resulting canvas is (options.rows * options.parallel) high and
  // (32 * options.chain_length) wide.
  Impl(GPIO *io, const Options &options);

  ~Impl();

  // Used to be there to help user delay initialization of thread starting,
  // these days only used internally.
  void SetGPIO(GPIO *io, bool start_thread = true);

  bool StartRefresh();

  FrameCanvas *CreateFrameCanvas();
  FrameCanvas *SwapOnVSync(FrameCanvas *other, unsigned framerate_fraction);
  bool ApplyPixelMapper(const PixelMapper *mapper);

  bool SetPWMBits(uint8_t value);
  uint8_t pwmbits();   // return the pwm-bits of the currently active buffer.

  void set_luminance_correct(bool on);
  bool luminance_correct() const;

  // Set brightness in percent for all created FrameCanvas. 1%..100%.
  // This will only affect newly set pixels.
  void SetBrightness(uint8_t brightness);
  uint8_t brightness();

  uint64_t RequestInputs(uint64_t);
  uint64_t AwaitInputChange(int timeout_ms);

  uint64_t RequestOutputs(uint64_t output_bits);
  void OutputGPIO(uint64_t output_bits);

private:
  friend class RGBMatrix;

  // Apply pixel mappers that have been passed down via a configuration
  // string.
  void ApplyNamedPixelMappers(const char *pixel_mapper_config,
                              int chain, int parallel);

  Options params_;
  bool do_luminance_correct_;

  FrameCanvas *active_;

  GPIO *io_;
  Mutex active_frame_sync_;
  UpdateThread *updater_;
  std::vector<FrameCanvas*> created_frames_;
  internal::PixelDesignatorMap *shared_pixel_mapper_;
  uint64_t user_output_bits_;
};

using namespace internal;

// Pump pixels to screen. Needs to be high priority real-time because jitter
class RGBMatrix::Impl::UpdateThread : public Thread {
public:
  UpdateThread(GPIO *io, FrameCanvas *initial_frame,
               int pwm_dither_bits, bool show_refresh,
               int limit_refresh_hz, bool allow_busy_waiting)
    : io_(io), show_refresh_(show_refresh),
      target_frame_usec_(limit_refresh_hz < 1 ? 0 : 1e6/limit_refresh_hz),
      allow_busy_waiting_(allow_busy_waiting),
      running_(true),
      current_frame_(initial_frame), next_frame_(NULL),
      requested_frame_multiple_(1) {
    pthread_cond_init(&frame_done_, NULL);
    pthread_cond_init(&input_change_, NULL);
    switch (pwm_dither_bits) {
    case 0:
      start_bit_[0] = 0; start_bit_[1] = 0;
      start_bit_[2] = 0; start_bit_[3] = 0;
      break;
    case 1:
      start_bit_[0] = 0; start_bit_[1] = 1;
      start_bit_[2] = 0; start_bit_[3] = 1;
      break;
    case 2:
      start_bit_[0] = 0; start_bit_[1] = 1;
      start_bit_[2] = 2; start_bit_[3] = 2;
      break;
    }
  }

  void Stop() {
    MutexLock l(&running_mutex_);
    running_ = false;
  }

  virtual void Run() {
    unsigned frame_count = 0;
    unsigned low_bit_sequence = 0;
    uint32_t largest_time = 0;
    gpio_bits_t last_gpio_bits = 0;

    // Let's start measure max time only after a we were running for a few
    // seconds to not pick up start-up glitches.
    static const int kHoldffTimeUs = 2000 * 1000;
    uint32_t initial_holdoff_start = GetMicrosecondCounter();
    bool max_measure_enabled = false;

    while (running()) {
      const uint32_t start_time_us = GetMicrosecondCounter();

      current_frame_->framebuffer()
        ->DumpToMatrix(io_, start_bit_[low_bit_sequence % 4]);

      // SwapOnVSync() exchange.
      {
        MutexLock l(&frame_sync_);
        // Do fast equality test first (likely due to frame_count reset).
        if (frame_count == requested_frame_multiple_
            || frame_count % requested_frame_multiple_ == 0) {
          // We reset to avoid frame hick-up every couple of weeks
          // run-time iff requested_frame_multiple_ is not a factor of 2^32.
          frame_count = 0;
          if (next_frame_ != NULL) {
            current_frame_ = next_frame_;
            next_frame_ = NULL;
          }
          pthread_cond_signal(&frame_done_);
        }
      }

      // Read input bits.
      if (!Rp1PioIsActive() && !Rp1RioIsActive()) {
        const gpio_bits_t inputs = io_->Read();
        if (inputs != last_gpio_bits) {
          last_gpio_bits = inputs;
          MutexLock l(&input_sync_);
          gpio_inputs_ = inputs;
          pthread_cond_signal(&input_change_);
        }
      }

      ++frame_count;
      ++low_bit_sequence;

      if (target_frame_usec_) {
        if (allow_busy_waiting_) {
          while ((GetMicrosecondCounter() - start_time_us) < target_frame_usec_) {
            // busy wait. We have our dedicated core, so ok to burn cycles.
          }
        } else {
          long spent_us = GetMicrosecondCounter() - start_time_us;
          SleepMicroseconds(target_frame_usec_ - spent_us);
        }
      }

      const uint32_t end_time_us = GetMicrosecondCounter();
      if (show_refresh_) {
        uint32_t usec = end_time_us - start_time_us;
        printf("\b\b\b\b\b\b\b\b%6.1fHz", 1e6 / usec);
        if (usec > largest_time && max_measure_enabled) {
          largest_time = usec;
          const float lowest_hz = 1e6 / largest_time;
          printf(" (lowest: %.1fHz)"
                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b", lowest_hz);
        } else {
          // Don't measure at startup, as times will be janky.
          max_measure_enabled = (end_time_us - initial_holdoff_start) > kHoldffTimeUs;
        }
      }
    }
  }

  FrameCanvas *SwapOnVSync(FrameCanvas *other, unsigned frame_fraction) {
    MutexLock l(&frame_sync_);
    FrameCanvas *previous = current_frame_;
    next_frame_ = other;
    requested_frame_multiple_ = frame_fraction;
    frame_sync_.WaitOn(&frame_done_);
    return previous;
  }

  gpio_bits_t AwaitInputChange(int timeout_ms) {
    MutexLock l(&input_sync_);
    input_sync_.WaitOn(&input_change_, timeout_ms);
    return gpio_inputs_;
  }

private:
  inline bool running() {
    MutexLock l(&running_mutex_);
    return running_;
  }

  GPIO *const io_;
  const bool show_refresh_;
  const uint32_t target_frame_usec_;
  const bool allow_busy_waiting_;
  uint32_t start_bit_[4];

  Mutex running_mutex_;
  bool running_;

  Mutex input_sync_;
  pthread_cond_t input_change_;
  gpio_bits_t gpio_inputs_;

  Mutex frame_sync_;
  pthread_cond_t frame_done_;
  FrameCanvas *current_frame_;
  FrameCanvas *next_frame_;
  unsigned requested_frame_multiple_;
};

// Some defaults. See options-initialize.cc for the command line parsing.
RGBMatrix::Options::Options() :
  // Historically, we provided these options only as #defines. Make sure that
  // things still behave as before if someone has set these.
  // At some point: remove them from the Makefile. Later: remove them here.
#ifdef DEFAULT_HARDWARE
  hardware_mapping(DEFAULT_HARDWARE),
#else
  hardware_mapping("regular"),
#endif

  rows(32), cols(32), chain_length(1), parallel(1),
  pwm_bits(internal::Framebuffer::kDefaultBitPlanes),

#ifdef LSB_PWM_NANOSECONDS
    pwm_lsb_nanoseconds(LSB_PWM_NANOSECONDS),
#else
    pwm_lsb_nanoseconds(130),
#endif

  pwm_dither_bits(0),
  brightness(100),

#ifdef RGB_SCAN_INTERLACED
    scan_mode(1),
#else
    scan_mode(0),
#endif

  row_address_type(0),
  multiplexing(0),

#ifdef DISABLE_HARDWARE_PULSES
    disable_hardware_pulsing(true),
#else
    disable_hardware_pulsing(false),
#endif

#ifdef SHOW_REFRESH_RATE
    show_refresh_rate(true),
#else
    show_refresh_rate(false),
#endif

#ifdef INVERSE_RGB_DISPLAY_COLORS
    inverse_colors(true),
#else
    inverse_colors(false),
#endif
  led_rgb_sequence("RGB"),
  pixel_mapper_config(NULL),
  panel_type(NULL),
#ifdef FIXED_FRAME_MICROSECONDS
  limit_refresh_rate_hz(1e6 / FIXED_FRAME_MICROSECONDS),
#else
  limit_refresh_rate_hz(0),
#endif
#ifdef DISABLE_BUSY_WAITING
    disable_busy_waiting(true)
#else
    disable_busy_waiting(false)
#endif
{
  // Nothing to see here.
}

#define DEBUG_MATRIX_OPTIONS 0

#if DEBUG_MATRIX_OPTIONS
static void PrintOptions(const RGBMatrix::Options &o) {
#define P_INT(val) fprintf(stderr, "%s : %d\n", #val, o.val)
#define P_STR(val) fprintf(stderr, "%s : %s\n", #val, o.val)
#define P_BOOL(val) fprintf(stderr, "%s : %s\n", #val, o.val ? "true":"false")
  P_STR(hardware_mapping);
  P_INT(rows);
  P_INT(cols);
  P_INT(chain_length);
  P_INT(parallel);
  P_INT(pwm_bits);
  P_INT(pwm_lsb_nanoseconds);
  P_INT(pwm_dither_bits);
  P_INT(brightness);
  P_INT(scan_mode);
  P_INT(row_address_type);
  P_INT(multiplexing);
  P_BOOL(disable_hardware_pulsing);
  P_BOOL(show_refresh_rate);
  P_BOOL(inverse_colors);
  P_STR(led_rgb_sequence);
  P_STR(pixel_mapper_config);
  P_STR(panel_type);
  P_INT(limit_refresh_rate_hz);
  P_BOOL(disable_busy_waiting);
#undef P_INT
#undef P_STR
#undef P_BOOL
}
#endif  // DEBUG_MATRIX_OPTIONS

RGBMatrix::Impl::Impl(GPIO *io, const Options &options)
  : params_(options), io_(NULL), updater_(NULL), shared_pixel_mapper_(NULL),
    user_output_bits_(0) {
  assert(params_.Validate(NULL));
#if DEBUG_MATRIX_OPTIONS
  PrintOptions(params_);
#endif
  const MultiplexMapper *multiplex_mapper = NULL;
  if (params_.multiplexing > 0) {
    const MuxMapperList &multiplexers = GetRegisteredMultiplexMappers();
    if (params_.multiplexing <= (int) multiplexers.size()) {
      // TODO: we could also do a find-by-name here, but not sure if worthwhile
      multiplex_mapper = multiplexers[params_.multiplexing - 1];
    }
  }

  if (multiplex_mapper) {
    // The multiplexers might choose to have a different physical layout.
    // We need to configure that first before setting up the hardware.
    multiplex_mapper->EditColsRows(&params_.cols, &params_.rows);
  }

  Framebuffer::InitHardwareMapping(params_.hardware_mapping);

  active_ = CreateFrameCanvas();
  active_->Clear();
  SetGPIO(io, true);

  // We need to apply the mapping for the panels first.
  ApplyPixelMapper(multiplex_mapper);

  // .. followed by higher level mappers that might arrange panels.
  ApplyNamedPixelMappers(options.pixel_mapper_config,
                         params_.chain_length, params_.parallel);
}

RGBMatrix::Impl::~Impl() {
  if (updater_) {
    updater_->Stop();
    updater_->WaitStopped();
  }
  delete updater_;

  // Make sure LEDs are off.
  active_->Clear();
  if (io_) active_->framebuffer()->DumpToMatrix(io_, 0);
  internal::Rp1RioDeinit();
  internal::Rp1PioDeinit();

  for (size_t i = 0; i < created_frames_.size(); ++i) {
    delete created_frames_[i];
  }
  delete shared_pixel_mapper_;
}

RGBMatrix::~RGBMatrix() {
  delete impl_;
}

uint64_t RGBMatrix::Impl::RequestInputs(uint64_t bits) {
  if (Rp1PioIsActive() || Rp1RioIsActive()) return 0;
  return io_->RequestInputs(static_cast<gpio_bits_t>(bits));
}

uint64_t RGBMatrix::Impl::RequestOutputs(uint64_t output_bits) {
  if (Rp1PioIsActive() || Rp1RioIsActive()) return 0;
  uint64_t success_bits = io_->InitOutputs(static_cast<gpio_bits_t>(output_bits));
  user_output_bits_ |= success_bits;
  return success_bits;
}

void RGBMatrix::Impl::OutputGPIO(uint64_t output_bits) {
  if (Rp1PioIsActive() || Rp1RioIsActive()) return;
  io_->WriteMaskedBits(static_cast<gpio_bits_t>(output_bits), static_cast<gpio_bits_t>(user_output_bits_));
}

void RGBMatrix::Impl::ApplyNamedPixelMappers(const char *pixel_mapper_config,
                                             int chain, int parallel) {
  if (pixel_mapper_config == NULL || strlen(pixel_mapper_config) == 0)
    return;
  char *const writeable_copy = strdup(pixel_mapper_config);
  const char *const end = writeable_copy + strlen(writeable_copy);
  char *s = writeable_copy;
  while (s < end) {
    char *const semicolon = strchrnul(s, ';');
    *semicolon = '\0';
    char *optional_param_start = strchr(s, ':');
    if (optional_param_start) {
      *optional_param_start++ = '\0';
    }
    if (*s == '\0' && optional_param_start && *optional_param_start != '\0') {
      fprintf(stderr, "Stray parameter ':%s' without mapper name ?\n", optional_param_start);
    }
    if (*s) {
      ApplyPixelMapper(FindPixelMapper(s, chain, parallel, optional_param_start));
    }
    s = semicolon + 1;
  }
  free(writeable_copy);
}

void RGBMatrix::Impl::SetGPIO(GPIO *io, bool start_thread) {
  if (io != NULL && io_ == NULL) {
    io_ = io;
    Framebuffer::InitGPIO(io_, params_.rows, params_.parallel,
                          !params_.disable_hardware_pulsing,
                          params_.pwm_lsb_nanoseconds, params_.pwm_dither_bits,
                          params_.row_address_type);
    Framebuffer::InitializePanels(io_, params_.panel_type,
                                  params_.cols * params_.chain_length);
  }
  if (start_thread) {
    StartRefresh();
  }
}

bool RGBMatrix::Impl::StartRefresh() {
  if (updater_ == NULL && io_ != NULL) {
    updater_ = new UpdateThread(io_, active_, params_.pwm_dither_bits,
                                params_.show_refresh_rate,
                                params_.limit_refresh_rate_hz,
                                !params_.disable_busy_waiting);
    // If we have multiple processors, the kernel
    // jumps around between these, creating some global flicker.
    // So let's tie it to the last CPU available.
    // The Raspberry Pi2 has 4 cores, our attempt to bind it to
    //   core #3 will succeed.
    // The Raspberry Pi1 only has one core, so this affinity
    //   call will simply fail and we keep using the only core.
    updater_->Start(99, (1<<3));  // Prio: high. Also: put on last CPU.
  }
  return updater_ != NULL;
}

FrameCanvas *RGBMatrix::Impl::CreateFrameCanvas() {
  FrameCanvas *result =
    new FrameCanvas(new Framebuffer(params_.rows,
                                    params_.cols * params_.chain_length,
                                    params_.parallel,
                                    params_.scan_mode,
                                    params_.led_rgb_sequence,
                                    params_.inverse_colors,
                                    &shared_pixel_mapper_));
  if (created_frames_.empty()) {
    // First time. Get defaults from initial Framebuffer.
    do_luminance_correct_ = result->framebuffer()->luminance_correct();
  }

  result->framebuffer()->SetPWMBits(params_.pwm_bits);
  result->framebuffer()->set_luminance_correct(do_luminance_correct_);
  result->framebuffer()->SetBrightness(params_.brightness);

  created_frames_.push_back(result);

  if (created_frames_.size() % 500 == 0) {
    if (created_frames_.size() == 500) {
      fprintf(stderr, "CreateFrameCanvas() called %d times; Usually you only want to call it once (or at most a few times) for double-buffering. These frames will not be freed until the end of the program.\n"
              "Typical reasons: \n"
              "  * Accidentally called CreateFrameCanvas() inside your inner loop (move outside the loop. Create offscreen-canvas once, then re-use. See SwapOnVSync() examples).\n"
              "  * Used to pre-compute many frames (use led_matrix::StreamWriter instead for such use-case. See e.g. led-image-viewer)\n",
              (int)created_frames_.size());
    } else {
      fprintf(stderr, "FYI: CreateFrameCanvas() now called %d times.\n",
              (int)created_frames_.size());
    }
  }

  return result;
}

FrameCanvas *RGBMatrix::Impl::SwapOnVSync(FrameCanvas *other,
                                          unsigned frame_fraction) {
  if (frame_fraction == 0) frame_fraction = 1; // correct user error.
  if (!updater_) return NULL;
  FrameCanvas *const previous = updater_->SwapOnVSync(other, frame_fraction);
  if (other) active_ = other;
  return previous;
}

uint64_t RGBMatrix::Impl::AwaitInputChange(int timeout_ms) {
  if (!updater_) return 0;
  if (Rp1PioIsActive() || Rp1RioIsActive()) return 0;
  return updater_->AwaitInputChange(timeout_ms);
}

bool RGBMatrix::Impl::SetPWMBits(uint8_t value) {
  const bool success = active_->framebuffer()->SetPWMBits(value);
  if (success) {
    params_.pwm_bits = value;
  }
  return success;
}
uint8_t RGBMatrix::Impl::pwmbits() { return params_.pwm_bits; }

// Map brightness of output linearly to input with CIE1931 profile.
void RGBMatrix::Impl::set_luminance_correct(bool on) {
  active_->framebuffer()->set_luminance_correct(on);
  do_luminance_correct_ = on;
}
bool RGBMatrix::Impl::luminance_correct() const {
  return do_luminance_correct_;
}

void RGBMatrix::Impl::SetBrightness(uint8_t brightness) {
  for (size_t i = 0; i < created_frames_.size(); ++i) {
    created_frames_[i]->framebuffer()->SetBrightness(brightness);
  }
  params_.brightness = brightness;
}

uint8_t RGBMatrix::Impl::brightness() {
  return params_.brightness;
}

bool RGBMatrix::Impl::ApplyPixelMapper(const PixelMapper *mapper) {
  if (mapper == NULL) return true;
  using internal::PixelDesignatorMap;
  const int old_width = shared_pixel_mapper_->width();
  const int old_height = shared_pixel_mapper_->height();
  int new_width, new_height;
  if (!mapper->GetSizeMapping(old_width, old_height, &new_width, &new_height)) {
    return false;
  }
  PixelDesignatorMap *new_mapper = new PixelDesignatorMap(
    new_width, new_height, shared_pixel_mapper_->GetFillColorBits());
  switch (mapper->GetMappingType()) {
    case PixelMapper::VisibleToMatrix:
      for (int y = 0; y < new_height; ++y) {
        for (int x = 0; x < new_width; ++x) {
          int orig_x = -1, orig_y = -1;
          mapper->MapVisibleToMatrix(old_width, old_height,
                                     x, y, &orig_x, &orig_y);
          if (orig_x < 0 || orig_y < 0 ||
              orig_x >= old_width || orig_y >= old_height) {
            fprintf(stderr, "Error in PixelMapper: (%d, %d) -> (%d, %d) [range: "
                    "%dx%d]\n", x, y, orig_x, orig_y, old_width, old_height);
            continue;
          }
          const internal::PixelDesignator *orig_designator;
          orig_designator = shared_pixel_mapper_->get(orig_x, orig_y);
          *new_mapper->get(x, y) = *orig_designator;
        }
      }
      break;
    case PixelMapper::MatrixToVisible: {
      bool collision_reported = false;
      for (int y = 0; y < old_height; ++y) {
        for (int x = 0; x < old_width; ++x) {
          int new_x = -1, new_y = -1;
          if (mapper->MapMatrixToVisible(old_width, old_height,
                                         x, y, &new_x, &new_y)) {
            if (new_x < 0 || new_y < 0 ||
                new_x >= new_width || new_y >= new_height) {
              fprintf(stderr, "Error in PixelMapper MapMatrixToVisible: (%d, %d) "
                      "-> (%d, %d) [range: %dx%d]\n",
                      x, y, new_x, new_y, new_width, new_height);
              continue;
            }
            const internal::PixelDesignator *orig_designator;
            orig_designator = shared_pixel_mapper_->get(x, y);
            internal::PixelDesignator *new_designator = new_mapper->get(new_x, new_y);
            if (new_designator->gpio_word >= 0 && !collision_reported) {
              fprintf(stderr, "Warning: MapMatrixToVisible: %s mapped twice to the same pixel (%d, %d) -> (%d, %d)\n", mapper->GetName(), x, y, new_x, new_y);
              collision_reported = true;
            }
            *new_designator = *orig_designator;
          }
        }
      }
      break;
    }
  }
  delete shared_pixel_mapper_;
  shared_pixel_mapper_ = new_mapper;
  return true;
}

// -- Public interface of RGBMatrix. Delegate everything to impl_

static bool drop_privs(const char *priv_user, const char *priv_group) {
  uid_t ruid, euid, suid;
  if (getresuid(&ruid, &euid, &suid) >= 0) {
    if (euid != 0)   // not root anyway. No priv dropping.
      return true;
  }

  if (priv_user == nullptr || priv_user[0] == 0) priv_user = "daemon";
  if (priv_group == nullptr || priv_group[0] == 0) priv_group = "daemon";

  gid_t gid = atoi(priv_group);  // Attempt to parse as GID first
  if (gid == 0) {
    struct group *g = getgrnam(priv_group);
    if (g == NULL) {
      perror("group lookup.");
      return false;
    }
    gid = g->gr_gid;
  }
  if (setresgid(gid, gid, gid) != 0) {
    perror("setresgid()");
    return false;
  }

  uid_t uid = atoi(priv_user);  // Attempt to parse as UID first.
  if (uid == 0) {
    struct passwd *p = getpwnam(priv_user);
    if (p == NULL) {
      perror("user lookup.");
      return false;
    }
    uid = p->pw_uid;
  }
  if (setresuid(uid, uid, uid) != 0) {
    perror("setresuid()");
    return false;
  }
  return true;
}

RGBMatrix *RGBMatrix::CreateFromOptions(const RGBMatrix::Options &options,
                                        const RuntimeOptions &runtime_options) {
  std::string error;
  if (!options.Validate(&error)) {
    fprintf(stderr, "%s\n", error.c_str());
    return NULL;
  }

  // For the Pi4, we might need 2, maybe up to 4. Let's open up to 5.
  // on supported architectures, -1 will emit memory barier (DSB ST) after GPIO write
  if (runtime_options.gpio_slowdown < (LED_MATRIX_ALLOW_BARRIER_DELAY ? -1 : 0)
      || runtime_options.gpio_slowdown > 60) {
    fprintf(stderr, "--led-slowdown-gpio=%d is outside usable range\n",
            runtime_options.gpio_slowdown);
    return NULL;
  }
  if (runtime_options.rp1_pio != 0 && runtime_options.rp1_pio != 1) {
    fprintf(stderr, "--led-rp1-pio=%d is outside usable range 0..1\n",
            runtime_options.rp1_pio);
    return NULL;
  }

  // Use file-scope GPIO instance.
  GPIO &io = s_global_io;
  Rp1RioSetEnabled(runtime_options.rp1_pio == 0);
  const bool use_rp1_rio = runtime_options.do_gpio_init
      && Rp1RioShouldActivate(options.hardware_mapping,
                              options.row_address_type,
                              options.parallel);
  const bool use_rp1_pio = !use_rp1_rio && runtime_options.do_gpio_init
      && Rp1PioShouldActivate(options.hardware_mapping,
                              options.row_address_type,
                              options.parallel);
  if (use_rp1_rio) {
    Rp1RioSetGpioSlowdown(runtime_options.gpio_slowdown);
  }
  if (use_rp1_pio) {
    Rp1PioSetGpioSlowdown(runtime_options.gpio_slowdown);
  }

  const bool pi5_backend_available =
      Rp1PioPlatformDetected() || Rp1RioPlatformDetected();
  if (runtime_options.do_gpio_init && pi5_backend_available
      && !use_rp1_pio && !use_rp1_rio) {
    if (Rp1RioBackendRequested()) {
      fprintf(stderr,
              "Pi 5-family RP1 RIO backend is selected, but this "
              "configuration is not supported yet.\n"
              "RIO supports mappings "
              "regular/adafruit-hat/adafruit-hat-pwm/classic and "
              "--led-row-addr-type=0, 1, 2, 3, 4, or 5.\n"
              "For configurations supported by PIO, use --led-rp1-pio=1.\n");
    } else {
      fprintf(stderr,
              "Pi 5-family RP1 PIO backend is selected, but this "
              "configuration is not supported yet.\n"
              "Supported in PIO mode for now: mappings "
              "regular/regular-pi1/adafruit-hat/adafruit-hat-pwm/classic and "
              "--led-row-addr-type=0, 1, 2, 3, 4, or 5.\n");
    }
    return NULL;
  }

  // C wrapper implementation is at file scope; use local reference `io`.
  if (runtime_options.do_gpio_init && !use_rp1_pio && !use_rp1_rio
      && !io.Init(runtime_options.gpio_slowdown)) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command\n");
    return NULL;
  }

  if (runtime_options.daemon > 0 && daemon(1, 0) != 0) {
    perror("Failed to become daemon");
  }

  RGBMatrix::Impl *result = new RGBMatrix::Impl(NULL, options);
  // Allowing daemon also means we are allowed to start the thread now.
  const bool allow_daemon = !(runtime_options.daemon < 0);
  if (runtime_options.do_gpio_init)
    result->SetGPIO(&io, allow_daemon);

  // TODO(hzeller): if we disallow daemon, then we might also disallow
  // drop privileges: we can't drop privileges until we have created the
  // realtime thread that usually requires root to be established.
  // Double check and document.
  if (runtime_options.drop_privileges > 0) {
    drop_privs(runtime_options.drop_priv_user,
               runtime_options.drop_priv_group);
  }

  return new RGBMatrix(result);
}

// Public interface.
RGBMatrix *RGBMatrix::CreateFromFlags(int *argc, char ***argv,
                                      RGBMatrix::Options *m_opt_in,
                                      RuntimeOptions *rt_opt_in,
                                      bool remove_consumed_options) {
  RGBMatrix::Options scratch_matrix;
  RGBMatrix::Options *mopt = (m_opt_in != NULL) ? m_opt_in : &scratch_matrix;

  RuntimeOptions scratch_rt;
  RuntimeOptions *ropt = (rt_opt_in != NULL) ? rt_opt_in : &scratch_rt;

  if (!ParseOptionsFromFlags(argc, argv, mopt, ropt, remove_consumed_options))
    return NULL;
  return CreateFromOptions(*mopt, *ropt);
}

FrameCanvas *RGBMatrix::CreateFrameCanvas() {
  return impl_->CreateFrameCanvas();
}
FrameCanvas *RGBMatrix::SwapOnVSync(FrameCanvas *other,
                                    unsigned framerate_fraction) {
  return impl_->SwapOnVSync(other, framerate_fraction);
}
bool RGBMatrix::ApplyPixelMapper(const PixelMapper *mapper) {
  return impl_->ApplyPixelMapper(mapper);
}
bool RGBMatrix::SetPWMBits(uint8_t value) { return impl_->SetPWMBits(value); }
uint8_t RGBMatrix::pwmbits() { return impl_->pwmbits(); }

void RGBMatrix::set_luminance_correct(bool on) {
  return impl_->set_luminance_correct(on);
}
bool RGBMatrix::luminance_correct() const { return impl_->luminance_correct(); }

void RGBMatrix::SetBrightness(uint8_t brightness) {
  impl_->SetBrightness(brightness);
}
uint8_t RGBMatrix::brightness() { return impl_->brightness(); }

uint64_t RGBMatrix::RequestInputs(uint64_t all_interested_bits) {
  return impl_->RequestInputs(all_interested_bits);
}
uint64_t RGBMatrix::AwaitInputChange(int timeout_ms) {
  return impl_->AwaitInputChange(timeout_ms);
}

uint64_t RGBMatrix::RequestOutputs(uint64_t all_interested_bits) {
  return impl_->RequestOutputs(all_interested_bits);
}
void RGBMatrix::OutputGPIO(uint64_t output_bits) {
  impl_->OutputGPIO(output_bits);
}

bool RGBMatrix::StartRefresh() { return impl_->StartRefresh(); }

// -- Implementation of RGBMatrix Canvas: delegation to ContentBuffer
int RGBMatrix::width() const {
  return impl_->active_->width();
}

int RGBMatrix::height() const {
  return impl_->active_->height();
}

void RGBMatrix::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
  impl_->active_->SetPixel(x, y, red, green, blue);
}

void RGBMatrix::Clear() {
  impl_->active_->Clear();
}

void RGBMatrix::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  impl_->active_->Fill(red, green, blue);
}

// FrameCanvas implementation of Canvas
FrameCanvas::~FrameCanvas() { delete frame_; }
int FrameCanvas::width() const { return frame_->width(); }
int FrameCanvas::height() const { return frame_->height(); }
void FrameCanvas::SetPixel(int x, int y,
                         uint8_t red, uint8_t green, uint8_t blue) {
  frame_->SetPixel(x, y, red, green, blue);
}
void FrameCanvas::SetPixels(int x, int y, int width, int height,
                         Color *colors) {
  frame_->SetPixels(x, y, width, height, colors);
}
void FrameCanvas::Clear() { return frame_->Clear(); }
void FrameCanvas::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  frame_->Fill(red, green, blue);
}
void FrameCanvas::SubFill(int x, int y, int width, int height, uint8_t red, uint8_t green, uint8_t blue) {
  frame_->SubFill(x, y, width, height, red, green, blue);
}
bool FrameCanvas::SetPWMBits(uint8_t value) { return frame_->SetPWMBits(value); }
uint8_t FrameCanvas::pwmbits() { return frame_->pwmbits(); }

// Map brightness of output linearly to input with CIE1931 profile.
void FrameCanvas::set_luminance_correct(bool on) { frame_->set_luminance_correct(on); }
bool FrameCanvas::luminance_correct() const { return frame_->luminance_correct(); }

void FrameCanvas::SetBrightness(uint8_t brightness) { frame_->SetBrightness(brightness); }
uint8_t FrameCanvas::brightness() { return frame_->brightness(); }

void FrameCanvas::Serialize(const char **data, size_t *len) const {
  frame_->Serialize(data, len);
}
bool FrameCanvas::Deserialize(const char *data, size_t len) {
  return frame_->Deserialize(data, len);
}
void FrameCanvas::CopyFrom(const FrameCanvas &other) {
  frame_->CopyFrom(other.frame_);
}
}  // end namespace rgb_matrix

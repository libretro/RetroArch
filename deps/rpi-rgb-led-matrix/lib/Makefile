# Creating RGB matrix library
# When you link this library with your binary, you need to add -lrt -lm -lpthread
# So
#   -lrgbmatrix
##
include ../config.mk

OBJECTS=gpio.o led-matrix.o options-initialize.o framebuffer.o \
	thread.o bdf-font.o graphics.o led-matrix-c.o hardware-mapping.o \
	pixel-mapper.o multiplex-mappers.o \
	content-streamer.o content-streamer-c.o \
	rp1/rp1_pio_backend.o rp1/rp1_pio_support.o rp1/rp1_rio_backend.o

TARGET=librgbmatrix

###
# After you change any of the following DEFINES, make sure to 'make' again.
#
# ###########      NOTE     ###########
# all of these options can now can be set programmatically and
# via command line flags as well. No real need to change them in the Makefile.
# (So be prepared for these to be removed at some point)
###

# There are several different pinouts for various breakout boards that uses
# this library. If you are using the described pinout in the toplevel README.md
# or the standard active-3 breakout board, then 'regular' is the one you'd like
# to use.
#
# Adafruit also made a breakout board, if you want to use that, choose
# 'adafruit-hat'
#
# These are the choices
#   regular            # Following this project wiring and using these PCBs
#   adafruit-hat       # If you have a RGB matrix HAT from Adafruit
#   adafruit-hat-pwm   # If you have an Adafruit HAT with PWM hardware mod.
#   regular-pi1        # If you have an old Pi1 and regular didn't work.
#   classic            # (deprecated) Classic Pi1/2/. Not used anymore.
#   classic-pi1        # (deprecated) Classic pinout on Rasperry Pi 1
HARDWARE_DESC?=regular

# If you see that your display is inverse, you might have a matrix variant
# has uses inverse logic for the RGB bits. In that case: uncomment this.
# Flag: --led-inverse
#DEFINES+=-DINVERSE_RGB_DISPLAY_COLORS

# For curiosity reasons and while tweaking values for LSB_PWM_NANOSECONDS,
# uncomment to see refresh rate in terminal.
# Flag: --led-show-refresh
#DEFINES+=-DSHOW_REFRESH_RATE

# For low refresh rates below 100Hz (e.g. a lot of panels), the eye will notice
# some flicker. With this option enabled, the refreshed lines are interleaved,
# so it is less noticeable. But looks less pleasant with fast eye movements.
# Flag: --led-scan-mode=1
#DEFINES+=-DRGB_SCAN_INTERLACED=1

# The signal can be too fast for some LED panels, in particular with newer
# (faster) Raspberry Pi 2s - in that case, the LED matrix only shows garbage.
# This allows to slow down the GPIO for these cases.
#
# Set to 1 for RPi2 or RPi3 (default below), because they are typically
# faster than the panels can digest.
#
# Set to 0 (or comment out) for RPi1, that are slow enough.
#
# Sometimes, you even have to give RGB_SLOWDOWN_GPIO=2 or even 3 for
# particularly slow panels or bad signal cable situations. If that happens, you
# typically should double check cables and add TTL level converter if you
# haven't.
# Flag: --led-slowdown-gpio
#DEFINES+=-DRGB_SLOWDOWN_GPIO=1

# This allows to change the base time-unit for the on-time in the lowest
# significant bit in nanoseconds.
# Higher numbers provide better quality (more accurate color, less ghosting),
# but have a negative impact on the frame rate.
#
# For the same frame-rate, displays with higher multiplexing (e.g. 1:16 or 1:32)
# require lower values.
#
# Good values for full-color display (PWM=11) are somewhere between 100 and 300.
#
# If you you use reduced bit color (e.g. PWM=1 for 8 colors like for text),
# then higher values might be good to minimize ghosting (and you can afford
# that, because lower PWM values result in higher frame-rates).
#
# How to decide ? Just leave the default if things are fine. If you see
# ghosting in high-contrast applications (e.g. text), increase the value.
# If you want to tweak, watch the framerate (-DSHOW_FRAME_RATE) while playing
# with this number and the PWM values.
# Flag: --led-pwm-lsb-nanoseconds
#DEFINES+=-DLSB_PWM_NANOSECONDS=130

# This is to debug problems with the hardware pulse generation. The PWM hardware
# module is also used by Raspberry Pi sound system, so there might be
# interference. Note, you typically don't want the hardware pulses disabled, as
# the image will have visible brightness glitches; but for debugging, this is
# a good choice.
# Flag: --led-no-hardware-pulses
#DEFINES+=-DDISABLE_HARDWARE_PULSES

# This allows to fix the refresh rate to a particular refresh time in
# microseconds.
#
# This can be used to mitigate some situations in which you have a rare
# faint flicker, which can happen due to hardware events (network access)
# or other situations such as other IO or heavy memory access by other
# processes (all of which seem to break the isolation we request from the
# kernel. You did set isolcpus=3 right ?)
# You trade a slightly slower refresh rate and display brightness for less
# visible flicker situations.
#
# For this to calibrate, run your program for a while with --led-show-refresh
# and watch the line that shows the refresh time and the maximum microseconds
# for a frame observed. The maximum number is updated whenever the frame
# refresh take a little bit longer. So wait a while until that value doesn't
# change anymore (at least a minute, so that you catch tasks that happen once
# a minute). Some value might read e.g.
#   204.6Hz max: 5133usec
# Now take this maximum value you see there (here: 5133) and put in
# this define (don't forget to remove the # in front).
#
# The refresh rate will now be adapted to always have this amount of time
# between frames, so faster refreshes will be slowed down, but the occasional
# delayed frame will fit into the time-window as well, thus reducing visible
# brightness fluctuations.
#
# You can play with value a little and reduce until you find a good balance
# between refresh rate (which is reduce the higher this value is) and
# flicker suppression (which is better with higher values).
# Flag: --led-limit-refresh
#DEFINES+=-DFIXED_FRAME_MICROSECONDS=5000

# When limiting refrash rate, a CPU core is busy waiting to get accurate
# timing. On single board systems, this results in an unresponsive system.
# By disabling busy waiting, CPU cycles are freed up, leading to a more
# responsive system at the cost of slightly less accurate frame timing.
# Flag: --led-no-busy-waiting
#DEFINES+=-DDISABLE_BUSY_WAITING

# Enable wide 64 bit GPIO offered with the compute module.
# This will use more memory to internally represent the frame buffer, so
# caches can't be utilized as much.
# So only switch this on if you really use the compute module and use more
# than 3 parallel chains.
# (this is untested right now, waiting for hardware to arrive for testing)
#DEFINES+=-DENABLE_WIDE_GPIO_COMPUTE_MODULE

# ---- Pinout options for hardware variants; usually no change needed here ----

# Uncomment if you want to use the Adafruit HAT with stable PWM timings.
# The newer version of this library allows for much more stable (less flicker)
# output, but it does not work with the Adafruit HAT unless you do a
# simple hardware hack on them:
# connect GPIO 4 (old OE) with 18 (the new OE); there are
# convenient solder holes labeled 4 and 18 on the Adafruit HAT, pretty
# close together.
# Then you can set the flag --led-gpio-mapping=adafruit-hat-pwm
# .. or uncomment the following line.
#HARDWARE_DESC=adafruit-hat-pwm

# Typically, a Hub75 panel is split in two half displays, so that a 1:16
# multiplexing actually multiplexes over two half displays and gives 32 lines.
# There are some other displays out there that you might experiment with
# that are internally wired to only have one sub-panel. In that case you might
# want to try this define to get a more reasonable canvas mapping.
# This option is typically _not_ needed, only use when you attempt to connect
# some oddball old (typically one-colored) display, such as Hub12.
#DEFINES+=-DONLY_SINGLE_SUB_PANEL

# If someone gives additional values on the make commandline e.g.
# make USER_DEFINES="-DSHOW_REFRESH_RATE"
DEFINES+=$(USER_DEFINES)

DEFINES+=-DDEFAULT_HARDWARE='"$(HARDWARE_DESC)"'
INCDIR=../include
RP1_PIO_VENDOR_DIR=./rp1/rp1_pio_vendor
RP1_PIO_INCLUDES=-I$(RP1_PIO_VENDOR_DIR)/piolib/include -I$(RP1_PIO_VENDOR_DIR)/include

CFLAGS=-W -Wall -Wextra -Wno-unused-parameter -O3 $(CPU_ARCH_FLAGS) $(LTO_FLAGS) -g -fPIC $(DEFINES) $(RP1_PIO_INCLUDES)
CXXFLAGS=$(CFLAGS) -fno-exceptions -std=c++11

all : $(TARGET).a $(TARGET).so.1

$(TARGET).a : $(OBJECTS)
	$(AR) rcs $@ $^

$(TARGET).so.1 : $(OBJECTS)
	$(CXX) -shared $(LTO_FLAGS) -Wl,-soname,$@ -o $@ $^ -lpthread  -lrt -lm -lpthread

led-matrix.o: led-matrix.cc $(INCDIR)/led-matrix.h
thread.o : thread.cc $(INCDIR)/thread.h
framebuffer.o: framebuffer.cc framebuffer-internal.h
graphics.o: graphics.cc utf8-internal.h

%.o : %.cc compiler-flags
	$(CXX) -I$(INCDIR) $(CXXFLAGS) -c -o $@ $<

%.o : %.c compiler-flags
	$(CC)  -I$(INCDIR) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET).a $(TARGET).so.1

compiler-flags: FORCE
	@echo '$(CXX) $(CXXFLAGS)' | cmp -s - $@ || echo '$(CXX) $(CXXFLAGS)' > $@

.PHONY: FORCE

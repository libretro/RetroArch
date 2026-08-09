# ra_stress — RetroArch lifecycle stress harness

Shakes out teardown and rebuild bugs by driving **the real RetroArch
binary** through its own command interface, cycling core loads, content
loads, unloads and driver reinits. Nothing is reimplemented: every
transition goes through
`task_push_load_content_*` / `CMD_EVENT_REINIT` exactly as the menu would.

## Contents

| File | What it is |
|---|---|
| `tools/ra_stress.py` | The driver. Python 3.3+, standard library only. |
| `tools/ra_stress.md` | This document. |

The driver depends on the `LOAD_CONTENT`, `START_CORE`, `UNLOAD_CORE`,
`VIDEO_REINIT`, `AUDIO_REINIT` and `DRIVERS_REINIT` commands in `command.c`
/ `command.h`. A build without them cannot drive a content transition or a
driver reinit from outside the process.

## What the command interface provides, and why it needed extending

The command interface already has `CLOSE_CONTENT`, `LOAD_CORE`,
`MENU_TOGGLE`, `RESET`, `SAVE_STATE` and the rest, and `HAVE_COMMAND` /
`HAVE_NETWORK_CMD` are set in `pkg/apple/BaseConfig.xcconfig` — so the UDP
listener is already in the iOS app. What it did not have was a set of
lifecycle commands that match the menu entries they correspond to:

| Command | Mirrors |
|---|---|
| `LOAD_CORE <core path>` | core list / file browser core selection (`ACTION_OK_LOAD_CORE`) |
| `START_CORE` | Main Menu -> *Start Core* (`action_ok_start_core`) |
| `LOAD_CONTENT <core path>\|<content path>` | *(new)* content loading - see note below |
| `CLOSE_CONTENT` | Quick Menu -> *Close Content* (`action_ok_close_content`) |
| `UNLOAD_CORE` | Main Menu -> *Unload Core* (`action_ok_unload_core`) |
| `VIDEO_REINIT` | *(no menu equivalent)* `CMD_EVENT_REINIT` + `DRIVER_VIDEO_MASK` (normalised to video+input) |
| `AUDIO_REINIT` | *(no menu equivalent)* `CMD_EVENT_AUDIO_REINIT` |
| `DRIVERS_REINIT` | *(no menu equivalent)* `CMD_EVENT_REINIT` + `DRIVERS_CMD_ALL` |

None of these call into menu code. Every one issues a frontend event or a
`task_push_*` entry point declared outside `HAVE_MENU`, so the command
interface keeps working on menuless builds. One consequence is worth
knowing about:

- **`CLOSE_CONTENT` on a menuless build.** `CMD_EVENT_CLOSE_CONTENT` is
  `#define`d to `CMD_EVENT_QUIT` when `HAVE_MENU` is off — sensible for a
  hotkey on a frontend with nowhere to return to, fatal for a harness. The
  handler uses `CMD_EVENT_UNLOAD_CORE` there instead, which unloads the core
  and starts the dummy core, leaving the process alive.

Two of these already existed but did not match the menu:

- **`CLOSE_CONTENT`** was a `map[]` entry driving `RARCH_CLOSE_CONTENT_KEY`,
  so it went through the hotkey handler in `runloop_iterate()` - which
  applies the `confirm_close` double-press timer. Nothing external can
  produce the second press inside the window, so with that setting on the
  command did nothing at all, silently. It now issues
  `CMD_EVENT_CLOSE_CONTENT` directly.
- **`LOAD_CORE`** made the right `task_push_load_new_core()` call but threw
  the result away and always reported success, so a bad core path was a
  silent no-op. It now propagates, and rejects an empty argument.

`LOAD_CONTENT` is the one command with no single menu entry to mirror - the
menu reaches content loading through several paths (playlist, file browser,
contentless). It goes through
`task_push_load_content_with_new_core_from_companion_ui()`, on the grounds
that a command-interface request is a frontend-external requester in the
same sense a companion UI is.

The three reinit commands are split by scope, which matters:
`CMD_EVENT_REINIT` falls back to `DRIVERS_CMD_ALL` when its data pointer is
NULL, so a naive "video reinit" would in fact tear down audio, input, MIDI,
Bluetooth, LED and the rest too. `VIDEO_REINIT` passes `DRIVER_VIDEO_MASK`
explicitly — the same mask the CRT switch path uses — and `DRIVERS_REINIT`
keeps the all-drivers behaviour under an honest name.

Note that `DRIVER_FLAGS_NORMALIZE()` widens that request to
`DRIVER_VIDEO_AND_INPUT_MASK` inside both `drivers_init()` and
`driver_uninit()`, so `VIDEO_REINIT` cycles video **and input**. That is not
a leak in the scoping — the input driver is not separately initialisable, it
is brought up inside `video_driver_init_internal()` and several video
drivers hand back the input driver and its data through their `init()`
out-params. Video-without-input is not a state the frontend can be in, so
video+input is the narrowest honest scope.

`VIDEO_REINIT` is still the most valuable discriminator: it cycles the video
stack *without* core churn and without dragging audio, MIDI, camera,
Bluetooth, LED and the rest along. If it alone kills the target, the fault
is in video/input teardown and rebuild. If only `DRIVERS_REINIT` does,
something outside that pair is implicated — and `AUDIO_REINIT` narrows that
further. If neither does and only content cycling kills it, core
load/unload is involved.

Closing content and unloading the core are different operations - the first
leaves the core loaded, the second releases it so the next load goes through
a fresh `dlopen`. Your crash log shows the full sequence (`Unloading
game... Unloading core... Unloading core symbols...`), so they are separate
driver ops and the fuzzer can tell you which one matters.


### Python requirement

Python 2.7, or 3.2 and newer. Standard library only — no `pip install`
step. The shebang selects `python3`, which is what this is developed
against; 2.7 is supported for boxes whose system interpreter is still that.

Nothing is given up to reach back that far. The conveniences that would
have raised the floor each have a small local equivalent, and none of them
is on a hot path — the driver spends its time in sleeps and socket waits.

| Replaced | Would have needed |
|---|---|
| `random.choices()` → `weighted_choice()` | 3.6 |
| `subprocess.run()` → `subprocess.call()` | 3.5 |
| `signal.Signals` enum → `signal_name()` | 3.5 |
| `time.monotonic()` → `_make_monotonic()` | 3.3 |
| `shlex.quote()` → local `quote()` | 3.3 |
| `print(..., flush=True)` → `say()` | 3.3 |
| `proc.wait(timeout=)` → poll loop | 3.3 |
| `os.makedirs(exist_ok=)` → `makedirs()` | 3.2 |
| `datetime.timezone` → `utcnow()` | 3.2 |

`quote()` is identical to `shlex.quote()` across 50,000 random strings and
on non-ASCII input, and sidesteps `pipes.quote` being removed in 3.13.
`weighted_choice()` matches `random.choices()`'s distribution to within
sampling error; a seed still gives a stable sequence, though not the one
`random.choices()` produced — saved reproducers carry the full op list
rather than a seed, so they are unaffected.

**The clock is the part worth explaining.** Every timeout, every probe and
the entire stall detector are elapsed-time calculations, and `time.time()`
steps backwards when NTP corrects the clock — which would produce negative
round-trip times and bogus stall reports. Falling back to it would have
been the one genuine sacrifice, so the script does not. It uses
`time.monotonic()` where available and otherwise asks the platform
directly: `GetTickCount64` on Windows, `mach_absolute_time` on macOS,
`clock_gettime(CLOCK_MONOTONIC)` elsewhere, via `ctypes`. Measured against
`time.monotonic()` the `clock_gettime` path drifts under 10 µs over two
seconds and is strictly non-decreasing across 200,000 samples.

If every one of those is somehow unavailable the script warns on stderr and
uses `time.time()`, so an exotic platform degrades loudly rather than
silently reporting nonsense.

Two smaller 2.7 traps also handled: `socket.error` is `IOError` there and so
escapes `except OSError`, and calling `.encode()` on an already-bytes `str`
forces an implicit ASCII decode that fails on non-ASCII content paths.

## Setup

Whatever the target, two things have to be true before the driver can talk
to it.

**1. Enable the command interface.** In RetroArch: *Settings → Network →
Network Commands*. Or in `retroarch.cfg` on the target:

```
network_cmd_enable  = "true"
network_cmd_port    = "55355"
```

**2. Build a RetroArch new enough to have the commands.** `LOAD_CONTENT`,
`START_CORE`, `UNLOAD_CORE` and the three `*_REINIT` commands are the ones
that matter; `GET_STATUS VERSION` will not tell you whether they are
present, so check `command.h` if in doubt.

Optionally, to make the soak *play* rather than idle in attract mode, also
enable the Remote RetroPad — a separate interface from the command socket:

```
network_remote_enable         = "true"
network_remote_enable_user_p0 = "true"
network_remote_base_port      = "55400"
```

then pass `--input`. `HAVE_NETWORKGAMEPAD` is already set in
`pkg/apple/BaseConfig.xcconfig` and in
`pkg/android/phoenix-common/jni/Android.mk`, so on those targets this part
needs no rebuild.

### Finding the paths to pass

`--core` and `--content` are paths *as they exist on the target*, not on
your workstation. The reliable way to get them is to load the content once
by hand and read them out of the log:

```
[Core] Loading dynamic libretro core from: "/.../fbneo.libretro.framework"
[Core] Using content: "/.../roms/FBNeo - Arcade Games/galaxian.zip"
```

Copy those two strings verbatim. On iOS both live under randomly-named
container UUIDs that change when the app is reinstalled, so re-read them
after any reinstall.

## Why input matters here

The command interface carries only `RARCH_*` meta binds — hotkeys. It
cannot press A, Start or a D-pad direction, and it never could. Pad input
goes over the Remote RetroPad interface instead: a raw
`struct remote_message { int port, device, index, id; uint16_t state; }`
(20 bytes, native LE) sent by UDP to `network_remote_base_port + user`.

Worth knowing before you rely on it:

- **Button state is latched.** Every press needs a matching release packet,
  or the button stays held forever.
- **One packet is consumed per user per frame.** `recvfrom` is called once
  per user per poll, so transitions must be spaced at least a frame apart —
  bursts get swallowed.
- **`msg.port` is ignored for routing.** The *socket* port selects the user
  (`base_port + user`); the field in the struct is not what dispatches it.
- **A wrong-sized packet clears all buttons.** The `ret != sizeof(msg)`
  branch resets button and analog state, so a malformed send silently
  wipes input rather than erroring.

This matters because content sitting in attract mode puts far less load on
the driver stack than actual gameplay does. If the bug you are chasing needs
real work in flight to show up, idling will not find it. `--input` coins up,
hits start, and then drives movement and fire throughout each playing
phase; see [Scripting input](#scripting-input) for how to change or replace
that.

## Scripting input

`--input` on its own plays a generic pattern: it coins up, starts, then
cycles movement and fire for the rest of each playing phase. Two flags
change what it sends, and a set of ops let you script input exactly.

### Changing the pattern

Both flags take `name[:hold_ms]`, comma separated. Hold defaults to 80 ms.

```sh
  --input-open 'select,select,start:200'      # two coins, longer start press
  --input-loop 'left:120,b,right:120,b'
```

Button names are the RetroPad ones: `a b x y l r l2 r2 l3 r3`, `up down
left right`, `start select`. An empty string sends nothing, so
`--input-loop ''` coins up and then leaves the game alone.

### Scripting an exact sequence

For anything reproducible, put input ops in a reproducer JSON alongside the
lifecycle ops. Five verbs:

| Op | Meaning |
|---|---|
| `["press", "start"]` | tap, 80 ms |
| `["press", "start", 250]` | tap, held 250 ms |
| `["down", "left"]` | press and leave held |
| `["up", "left"]` | release |
| `["releaseall"]` | release all sixteen buttons |
| `["wait", 0.5]` | idle, still probing for stalls |

`down` / `up` matter because they let a button stay held *across* a
lifecycle transition — which is exactly the kind of thing that finds input
state bugs:

```json
{
  "ops": [
    ["load", "/path/to/core", "/path/to/content"],
    ["press", "select"],
    ["press", "start", 250],
    ["down", "left"],
    ["wait", 0.5],
    ["press", "b"],
    ["up", "left"],
    ["releaseall"],
    ["close"]
  ]
}
```

Run it with `--mode replay --replay yourscript.json --input`. `--core` and
`--content` are not needed in replay or minimize — each `load` op carries
its own core path.

### Rotating several cores

Because the core path is per-op, one script can cycle cores as well as
content. This is the shape worth reaching for first, since swapping cores
exercises a full core unload and `dlopen` between each content load rather
than reusing the one already resident:

```json
{
  "ops": [
    ["load",  "/cores/fbneo_libretro.so",  "/roms/galaxian.zip"],
    ["press", "select"],
    ["press", "start", 250],
    ["wait",  4.0],
    ["close"],

    ["load",  "/cores/snes9x_libretro.so", "/roms/smw.sfc"],
    ["press", "start", 250],
    ["down",  "right"],
    ["wait",  4.0],
    ["up",    "right"],
    ["close"],

    ["load",  "/cores/genesis_plus_gx_libretro.so", "/roms/sonic.md"],
    ["press", "start"],
    ["wait",  4.0],
    ["releaseall"],
    ["close"]
  ]
}
```

Swap some `close` ops for `unload` to release the core as well as the
content, and the two paths can be compared directly in one run.

For a longer soak, generate the JSON rather than typing it — the file is
plain data, so a few lines of Python will repeat the block above fifty
times with whatever cores and content you have.

**Turn the automatic pattern off when scripting.** A `load` op runs its
playing phase with `--input-open` and `--input-loop` still active, so a
scripted sequence would be layered on top of generic play. Pass
`--input-open '' --input-loop ''` for a run that sends only what you wrote.

**Two constraints inherited from the protocol.** A hold shorter than one
frame may never be observed, so keep `hold_ms` above ~20 ms and well above
it if the target is running at 30 fps. And because only one packet is
consumed per user per frame, the driver spaces every transition by 20 ms —
a long scripted sequence takes real time to deliver rather than arriving at
once.

## Use case 1: a Mac

The easiest target, and where you should start. The driver spawns the binary
itself, so it can distinguish a **crash from a hang** by exit status —
something no remote target can do — and it captures the log automatically.

```sh
python3 tools/ra_stress.py --host 127.0.0.1 \
  --binary ./retroarch \
  --core    ./cores/fbneo_libretro.dylib \
  --content ~/roms/galaxian.zip \
  --content ~/roms/centiped.zip \
  --mode soak --cycles 40
```

`--host 127.0.0.1` because the spawned process listens locally. Add
`--binary-args '--appendconfig ./stress.cfg'` to point it at a throwaway
config so the soak cannot disturb your real one.

`--env` is passed through to the spawned process, so this is where any
validation layer, sanitiser or debug knob goes:

```sh
  --env SOME_VALIDATION_KNOB=1
```

Validation is worth turning on. It tends to fault at the point the mistake
happens rather than letting it surface later somewhere unrelated, which can
be the difference between a one-line diagnosis and a week of guessing.

Once something reproduces here, `--mode minimize` works unattended, because
restarting the target is just spawning it again.

## Use case 2: a Windows PC

Mechanically the same as the Mac — the driver spawns `retroarch.exe` and
owns it — but Windows is the most *useful* target for a lifecycle bug,
because it is the only platform where you can run the identical soak across
five different video backends and see which ones survive.

```bat
python tools\ra_stress.py --host 127.0.0.1 ^
  --binary  C:\RetroArch\retroarch.exe ^
  --core    C:\RetroArch\cores\fbneo_libretro.dll ^
  --content "C:\roms\FBNeo - Arcade Games\galaxian.zip" ^
  --content "C:\roms\FBNeo - Arcade Games\centiped.zip" ^
  --mode soak --cycles 40
```

Paths with backslashes are handled correctly in `--binary-args` and
`--relaunch-cmd`; the driver switches `shlex` out of POSIX mode on Windows
so separators are not eaten as escapes.

**Allow the firewall prompt.** Windows Defender will ask about
`retroarch.exe` on first bind. Deny it and the driver simply never gets a
reply, which looks identical to the app never starting.

**Config lives in one of two places.** If a `retroarch.cfg` sits next to the
`.exe`, that is the portable config being used; otherwise it is under
`%APPDATA%\RetroArch`. Check which before you go editing the wrong one.

### Comparing video backends

This is the thing Windows is for. Write one throwaway config per backend:

```bat
echo video_driver = "vulkan" > vk.cfg
echo video_driver = "d3d11"  > d3d11.cfg
echo video_driver = "d3d12"  > d3d12.cfg
echo video_driver = "glcore" > glcore.cfg
echo video_driver = "gl"     > gl.cfg
```

then run the same sequence against each:

```bat
for %%D in (vk d3d11 d3d12 glcore gl) do ^
  python tools\ra_stress.py --host 127.0.0.1 --binary C:\RetroArch\retroarch.exe ^
    --binary-args "--appendconfig C:\RetroArch\%%D.cfg" ^
    --core C:\RetroArch\cores\fbneo_libretro.dll ^
    --content "C:\roms\galaxian.zip" ^
    --mode soak --cycles 20 --outdir out-%%D
```

The result is diagnostic on its own. If one backend dies and the rest do
not, the fault is in that backend's teardown. If all five die at the same
point, it is in the shared frontend path above them — `driver_uninit` /
`drivers_init` and everything they call — and the graphics API is a red
herring.

### Reading a Windows death

Windows reports a fatal exception as an NTSTATUS value in the exit code
rather than as a signal, so a raw "exited with status 3221225477" is really
an access violation. The driver decodes the ones worth recognising:

| Code | Meaning |
|---|---|
| `0xC0000005` | Access violation — bad pointer |
| `0xC0000374` | Heap corruption |
| `0xC00000FD` | Stack overflow |
| `0xC0000409` | Stack buffer overrun / `__fastfail` |
| `0x80000003` | Breakpoint — an assert fired |
| `0xC000001D` | Illegal instruction |

To get a dump alongside it, turn on WER local dumps once:

```
HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps
    DumpFolder   (REG_EXPAND_SZ)  C:\dumps
    DumpType     (REG_DWORD)      2      ; full dump
```

Every crash then leaves a `.dmp` you can open in WinDbg or Visual Studio,
which pairs well with the reproducer JSON — you get the exact op sequence
*and* the stack.

For heap corruption specifically (`0xC0000374`), enable PageHeap under
Application Verifier or `gflags` before the run. Like a sanitiser, it makes
the process fault at the moment of the bad write rather than much later at
some unrelated free.

## Use case 3: an iPhone

You cannot spawn the app, so the driver runs on your Mac and talks UDP to
the phone over Wi-Fi. Both must be on the same network.

**Before you start:**

- Get the phone's IP: *Settings → Wi-Fi → (i)* next to the network.
- *Settings → Display & Brightness → Auto-Lock → Never.* If the screen locks
  the app is suspended, it stops answering, and the driver will report that
  as a death.
- Keep RetroArch in the foreground for the whole run. Same reason.
- Plug in power. A soak with `--input` runs the GPU hard for hours.
- Allow the local-network permission prompt the first time.

```sh
python3 tools/ra_stress.py \
  --host 192.168.1.42 \
  --core    '/var/containers/Bundle/Application/<UUID>/RetroArch.app/Frameworks/fbneo.libretro.framework' \
  --content '/var/mobile/Containers/Data/Application/<UUID>/Documents/RetroArch/roms/FBNeo - Arcade Games/galaxian.zip' \
  --content '/var/mobile/Containers/Data/Application/<UUID>/Documents/RetroArch/roms/FBNeo - Arcade Games/centiped.zip' \
  --input \
  --mode soak --cycles 40
```

To let the driver restart the app after a death — needed for `fuzz`, and
required for `minimize`:

```sh
xcrun devicectl list devices          # get the UDID
```

```sh
  --relaunch-cmd 'xcrun devicectl device process launch --device <UDID> org.warmenhoven.RetroArch'
```

Without `--relaunch-cmd` the run simply stops at the first death, which is
fine for `soak` — you relaunch by hand and read the reproducer JSON.

**Collecting the wreckage.** RetroArch runs a WebDAV server on 8080 and an
HTTP server on 80, so after a death you can pull the log and any crash
report straight off the device rather than fishing them out over USB.

## Use case 4: an iPad

Identical to the iPhone in every mechanical respect — same command line,
same `--relaunch-cmd`, same foreground and auto-lock caveats. What makes it
worth running separately is that it reaches surface configurations an iPhone
never will:

- **Stage Manager and split view** resize the window while content is
  running, which drives the resize path continuously rather than only at
  load. If you suspect resize handling, this is the target that exercises
  it.
- **External displays** add a second surface and a display-mode change on
  connect and disconnect.
- **Rotation** without the aspect constraints a phone imposes.

None of that is scriptable from the driver, so the productive shape is: run
a long `soak` and manually rotate, enter and leave Stage Manager, and plug
and unplug a display while it runs. The stall detector keeps measuring
throughout, so anything that wedges the run loop is timestamped even if it
never crashes.

## Use case 5: an Android phone

`HAVE_COMMAND`, `HAVE_NETWORK_CMD` and `HAVE_NETWORKGAMEPAD` are all set in
`pkg/android/phoenix-common/jni/Android.mk`, so both interfaces are already
in the app.

Android is the best remote target of the three, because `adb` closes most of
the gaps that make iOS awkward: you can relaunch the app, keep the screen
on, and read a full native backtrace without touching the device.

**Before you start.** Find the package — the flavour determines it:

```sh
adb shell pm list packages | grep retroarch
```

`com.retroarch`, `com.retroarch.aarch64` or `com.retroarch.ra32`, per the
`applicationIdSuffix` in `pkg/android/phoenix/build.gradle`. Then:

```sh
adb shell ip route                     # the device's IP on your network
adb shell svc power stayon true        # no screen-off while charging
```

Also turn off battery optimisation for RetroArch, or Doze will suspend the
process partway through a long soak and the driver will read that as a
death.

```sh
python3 tools/ra_stress.py \
  --host 192.168.1.57 \
  --core    /data/data/com.retroarch/cores/fbneo_libretro_android.so \
  --content "/storage/emulated/0/RetroArch/roms/FBNeo - Arcade Games/galaxian.zip" \
  --input \
  --relaunch-cmd 'adb shell am start -n com.retroarch/com.retroarch.browser.mainmenu.MainMenuActivity' \
  --mode fuzz --fuzz-runs 50
```

Because the relaunch is one `adb` call away, `fuzz` and `minimize` both run
unattended here — the only remote target where that is true without extra
setup.

**You still need Wi-Fi.** `adb forward` handles TCP, `localabstract` and a
few others, but not UDP, and both interfaces here are UDP. There is no way
to tunnel them over the cable; device and workstation must share a network.

### Reading an Android death

This is where Android pulls ahead. `debuggerd` writes a full native crash
report to the crash log buffer:

```sh
adb logcat -b crash
```

You get the signal, the fault address, the aborting thread and a symbolised
backtrace — roughly what you would otherwise have to extract from a
tombstone or a dump. Leave it running in a second terminal alongside the
driver and you will have the stack and the reproducer JSON for the same
event.

`adb logcat -s RetroArch` gives RetroArch's own log output for the same run.

### Comparing backends on Android

`HAVE_VULKAN` and `HAVE_OPENGLES` are both set, so Android is the second
platform after Windows where the same sequence can be run against different
graphics backends — `video_driver = "vulkan"` against `"glcore"` or
`"gl"`. Fewer backends than Windows, but on a completely different driver
stack, which is the point: a bug that reproduces on both a desktop GPU
driver and a mobile one is almost certainly above the driver.

## Modes

- **`soak`** — repeat the plan built from `--content`. Deterministic, best
  first attempt.
- **`fuzz`** — randomised lifecycle op sequences (`load`/`close`/`unload`/
  `reinit`/`driversreinit`/`audioreinit`/`menu`/`pause`/`reset`/`savestate`/
  `loadstate`/`shader`) from a seed. Input ops are not fuzzed; they are for
  hand-written scripts. Every run is reproducible from its printed seed.
- **`replay`** — re-run a saved reproducer JSON to confirm it is reliable.
- **`minimize`** — delta-debug a reproducer down to the shortest sequence
  that still kills it. Needs `--binary` or `--relaunch-cmd`.

Typical flow: `fuzz` until something dies → `replay` a few times to check
it is not a one-off → `minimize` → you have a two-line bug report.

## Stall detection

The command interface is serviced from `runloop_iterate()`, so **command
round-trip time is a direct measurement of main-thread progress**. Anything
that blocks the run loop — a driver sitting on a timeout, a lock held too
long, a slow teardown — shows up as an elevated RTT.

Set `--stall-threshold` below the shortest timeout you expect a blocked run
loop to wait on; it defaults to 0.75 s, which catches the common
one-second-timeout case.

This matters because stalls are far more frequent than crashes. You will
usually see stalls long before you see a death, and a sequence that reliably
produces stalls is a much better starting point for bisection than waiting
twenty minutes for a segfault. Stalls are recorded in the reproducer JSON
even when the run completes cleanly.

## What happens when it crashes

The driver treats "stopped answering the command interface" as death. That
is detected either by a probe going unanswered for `--probe-timeout`, or by
a state transition not completing within `--load-timeout-total`.

When it fires:

1. **The sequence stops at that op.** Remaining ops are not run — there is
   no point sending commands to a process that is gone, and continuing
   would muddy which op was responsible.
2. **It waits two seconds, then classifies the death.** On a local target
   that means a POSIX signal name, a Windows NTSTATUS code, or the verdict
   `process alive but unresponsive -- HANG, not a crash`. On a remote target
   it can only report that the target stopped responding.
3. **A local target is deliberately not cleaned up.** The process is left
   as it is so you can attach a debugger, read its log, or confirm it really
   died. Only clean runs get torn down.
4. **A reproducer JSON is written to `--outdir`.**
5. **The driver exits non-zero**, so it drops straight into a shell loop or
   a CI job without extra plumbing. A target that never answered at startup
   also exits non-zero — an unreachable target is not a passing run.

In `soak` the run ends there. In `fuzz` the first crash ends the run too,
and the exact `--mode minimize` command line to shrink it is printed for
you to paste.

### The reproducer JSON

```json
{
  "label": "fuzz seed=250934579",
  "started": "2026-07-28T01:29:31Z",
  "ops": [ ... every op in the sequence ... ],
  "crashed": true,
  "completed": 2,
  "died_index": 2,
  "died_at": "reinit",
  "reason": "no reply to VERSION after 4.0s",
  "detail": "process died: 0xC0000005 STATUS_ACCESS_VIOLATION (bad pointer)",
  "stalls": [ {"t": "...", "rtt_ms": 1103, "at": "playing galaxian.zip"} ],
  "max_rtt_ms": 1103,
  "logs": ["./ra-stress-out/ra-012931_004821.log"],
  "seed": 250934579,
  "host": "127.0.0.1"
}
```

`died_index` is an index into `ops`, so the failing op and everything that
led to it are both right there. Feed the file straight back in with
`--mode replay` to check the crash is reliable, then `--mode minimize` to
cut it down.

**A file is also written when nothing crashes**, provided any stall was
recorded. Those are worth keeping: a sequence that reliably stalls is
usually a better lead than one that crashes once in twenty minutes.

### Reading a death on a remote target

The driver can only see that the target stopped answering. That covers
several things, and it is worth knowing which you have:

| What you see | What it usually means |
|---|---|
| App is gone from the screen | Real process death; look for a crash report |
| App is visible but frozen | Hang, not a crash — the run loop is wedged |
| App is visible and fine | Wi-Fi dropped, or it got backgrounded |

On Android, `adb logcat -b crash` resolves this almost as well as a local
target: if `debuggerd` logged a report, it was a real crash.

### Crashes are not the only failure

`detail` distinguishing a hang from a crash matters more than it sounds.
A wedged run loop and a dead process look identical from the command socket,
but they are completely different bugs — and on a local target the driver
can tell them apart for you. This is the strongest argument for reproducing
on a desktop before chasing something on a device.

## Suggested order of attack

1. **Desktop, `--mode soak --cycles 40`, two or three contents.** Is
   repeated content cycling sufficient on its own? Start here even if the
   bug was first seen on a device — a local target tells you crash vs. hang,
   and iterating is seconds rather than minutes.
2. **Windows, the same soak across every video backend.** Cheap, and it
   tells you immediately whether you are chasing a backend bug or a
   frontend one.
3. **Desktop, reinit-only.** Write a reproducer JSON whose ops are one `load`
   followed by 200 × `reinit`, and `--mode replay` it. If that dies, the
   fault is in the driver teardown/rebuild cycle alone and core loading is
   irrelevant — a much smaller place to look. Swap `reinit` for
   `driversreinit` to widen the scope, `audioreinit` to narrow it.
4. **Desktop, `--mode fuzz --fuzz-runs 50`.** Then `replay` any hit a few times
   to confirm it is not a one-off, then `minimize`.
5. **Device, replaying the minimized sequence.** Confirms it is the same bug
   and not a host-only artifact.
6. **Device, overnight `fuzz` with `--relaunch-cmd`** if nothing reproduced
   on desktop. Some bugs only exist on the device's driver stack. Android is
   the easiest device to leave running unattended, since `adb` can relaunch
   the app and `adb logcat -b crash` records the backtrace for every death.

A reproducer JSON is just `{"ops": [["load", "<core>", "<content>"],
["reinit"], ...]}` — hand-writing one for step 3 takes a moment.

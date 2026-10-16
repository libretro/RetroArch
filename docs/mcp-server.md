# MCP server: RetroArch as a tool for AI assistants

RetroArch can act as a [Model Context Protocol](https://modelcontextprotocol.io)
server, so an AI assistant that speaks MCP can drive it: pause and resume,
load and save states, take screenshots, read core memory, load content.
Every tool is one of RetroArch's network commands; the list, and what each
one does, is the same one `HELP` prints on the network command interface.

The server is off by default.

## Turning it on

In **Settings > Network**, turn on **MCP Server**. Two more entries appear:

| Key | Default | Meaning |
| --- | --- | --- |
| `mcp_server_enable` | `false` | Runs the server. Takes effect when RetroArch next starts. |
| `mcp_server_port` | `55357` | TCP port. |
| `mcp_server_token` | | The token every client must send. Made on the first start when empty. |
| `mcp_server_bind_address` | `127.0.0.1` | Address to listen on; `retroarch.cfg` only. |

Restart RetroArch. The log reports the address:

    [MCP] Listening on http://127.0.0.1:55357/mcp.

On the first start a random token is made and shown under **MCP Server
Token**. It is saved with the other passwords in the keychain, never in
`retroarch.cfg`. A build without the crypto library cannot make one:
enter your own there, or set `mcp_server_token`, before the server will
start.

## Connecting a client

Clients that support MCP over HTTP ("Streamable HTTP") need three things:

- the address: `http://127.0.0.1:55357/mcp`
- the transport: HTTP
- a header: `Authorization: Bearer <token>`

Where these go depends on the client. Many take a JSON entry of this
shape in their MCP server configuration:

```json
{
  "retroarch": {
    "type": "http",
    "url": "http://127.0.0.1:55357/mcp",
    "headers": { "Authorization": "Bearer <token>" }
  }
}
```

The server speaks protocol revision 2026-07-28, and 2025-11-25, 2025-06-18
and 2025-03-26 for clients that open with `initialize`.

## Over stdin and stdout

A client that starts RetroArch itself can talk to it on stdin and
stdout instead of a port, with no token: only the process that started
RetroArch holds the other ends.

```json
{
  "retroarch": {
    "command": "/path/to/retroarch",
    "args": ["--mcp-stdio"]
  }
}
```

Each message is one line of JSON. stdout is kept for them; whatever
else would write there, RetroArch's own log and any core's included,
goes to stderr. The MCP Server setting does not need to be on for this.
Over stdin and stdout RetroArch also offers logging: after
`logging/setLevel`, what it logs at that level and above arrives as
`notifications/message`. When the client closes stdin, RetroArch quits.

## What the tools do

Each command is one tool, named as the command (`GET_STATUS`,
`STATE`, `SCREENSHOT`). Its parameters are named, typed and
marked needed or not, so a client is told what to send rather than
having to write the command's own line by hand:

```json
{ "name": "LOAD_CONTENT",
  "arguments": { "core": "/path/to/core.so", "content": "/games/a.rom" } }
```

`STATE` takes an `action` and a `slot` number, `GET_PLAYLIST` takes
`playlist` and an optional `first`, `SET_OPTION` takes `option` and a
`state` of `on` or `off` - which the schema gives as the only two values
it accepts. `tools/list` describes every one of them.

A parameter that is needed and left out is answered before the command
runs at all, as JSON-RPC invalid params rather than as a tool failure.

The single `argument` string this server took before the parameters
still works, so a client holding a cached tool list keeps going until it
reads the new one.

The tool's result is what the command answers; commands that answer
nothing report `Done.`.

## Results that are rows

The replies that are lines of tab-separated fields - `GET_OPTION`,
`SET_OPTION`, `LIST_CORES`, `LIST_PLAYLISTS`, `GET_PLAYLIST`,
`GET_CORE_OPTION`, `GET_MEMORY_MAP`, `GET_LOG`, `SET_INPUT` and
`RELEASE_INPUT` - come
with `structuredContent` beside the text, so a client is handed the
rows rather than splitting them itself. Each of those tools declares an
`outputSchema` naming its columns:

```json
{ "content": [ { "type": "text",
    "text": "0\tSuper Mario World\t/games/smw.sfc\t/cores/snes9x.so\n" } ],
  "structuredContent": { "rows": [ { "index": "0",
    "label": "Super Mario World", "content": "/games/smw.sfc",
    "core": "/cores/snes9x.so" } ] } }
```

The columns are `option` and `value` for the options, `name` and `path`
for the cores, `playlist` for the playlists, `index`, `label`, `content`
and `core` for a playlist's entries, `key`, `value`, `description` and
`values` for a core's options, `start`, `length`, `select`,
`disconnect`, `kind`, `access`, `order` and `space` for the memory map,
and `player`, `buttons` and `axes` for what a player holds. Every field
is a string: it is the text's own field, which the server does not read
as anything else.

**A tool answers with `structuredContent` exactly when it declared an
`outputSchema`**, so a client reads the schema it was handed at
`tools/list` and does not have to probe each reply. A tool with no
`outputSchema` answers with text alone, because its reply is prose or a
confirmation rather than rows.

A reply may end short of its columns, and the schema requires only the
ones every row carries: `GET_CORE_OPTION` gives an option's `values`
only where it was asked for that option by key.

A line the command opens with its own name, such as `LIST_CORES none`,
is not a row - though `RUN_FRAMES 3` and `PRESS 1 right+a 4` still give
the frames run as `frames`, the number ending the line - and a failure is its reason and no rows at all. Neither
are the lines that carry a number about the reply rather than a row of
it, which become fields of their own:

| Line | Field | Where |
| --- | --- | --- |
| `MORE` | `more: true` | a list cut short with nowhere to resume |
| `MORE 200` | `more: true`, `next: 200` | `GET_PLAYLIST`'s next page |
| `NEXT 48` | `next: 48` | where `GET_LOG` reached |
| `SKIPPED 12` | `skipped: 12` | log lines gone before the first kept |
| `RUN_FRAMES 3` | `frames: 3` | the frames `RUN_FRAMES` or `PRESS` ran |

`GET_LOG` always says where to go on from, so it carries `next` without
carrying `more`: a client asks again from `next` and gets whatever has
been written since.

`GET_LOG` carries no rows: the log is its lines, and a page of them runs
to 60 kB, so answering with a row apiece would send the whole page
twice. Its `structuredContent` is the trailers alone - `{"next": 482}`
- and its `outputSchema` declares only those.

The text is unchanged: a client that reads it and nothing else sees
exactly what it always did. A game whose name holds a tab is the one
thing it will read wrong, and the rows get right - the fields before and
after the name are read from the ends of the line, so the stray tab
stays inside the name.

Both come from revision 2025-06-18, which added them; a client of
2025-03-26 is sent neither. Over HTTP the revision is the one the
client names in `MCP-Protocol-Version` on each request; over stdin and
stdout it is the one agreed at `initialize`, and it holds for the
session.

Not every command is a tool. `HELP` and `VERSION` are not, because
`tools/list` already lists the commands and every answer names the
server's version, and neither are the toggle hotkeys or
`GET_CONFIG_PARAM`, whose work `SET_OPTION` and `GET_OPTION` do below.

Nor is a hotkey that steps a selection. A person presses
`STATE_SLOT_PLUS` while watching the slot on screen; for a program the
command that takes the value outright is the tool, and stepping towards
it is only a way to get the value wrong: `STATE save` over
`STATE_SLOT_PLUS` and `SAVE_STATE`, `REPLAY seek` over
`NEXT_REPLAY_CHECKPOINT`, `REPLAY play` over `PLAY_REPLAY`. Where
nothing reports the selection either - which cheat is picked, which
shader preset is loaded - there is nothing worth offering yet, and a
tool that writes where it cannot read is worse than none.

`READ_CORE_RAM` and `WRITE_CORE_RAM` are not tools either: they take an
achievement address, which looks like `READ_CORE_MEMORY`'s system
address and is not one, and `GET_MEMORY_MAP` answers the addresses
`READ_CORE_MEMORY` takes.

The disc and the menu are the exceptions: `DISK` and `MENU` step blind,
and until something answers where they landed, stepping is all there is
- and a disc swap is part of playing a game. Each is one tool taking a
choice - `MENU down`, `DISK next` - rather than a tool per hotkey.
`REINIT`, `STATE` and `REPLAY` stand for their families likewise:
`STATE load 3` is `LOAD_STATE_SLOT 3`, and `REPLAY seek 600` is
`SEEK_REPLAY 600`. Each answers under its own name - `STATE load 3`,
`DISK next pressed`, `REPLAY ERROR no replay is playing or recording`,
and `REINIT audio` for a restart that says nothing itself - while the
commands sent by name answer as they always have. `DISK` is refused on
a core with no disk control, where the hotkeys would do nothing.

`SAVE_FILES` and `LOAD_FILES` are not tools: the core writes its save
files itself, and playing a game does not need them.

All of them are still commands on the network and stdin interfaces,
where a person is typing and can see what happened.

## Options

A hotkey toggle is of little use to an assistant: `FAST_FORWARD` turns
fast-forward on or off depending on what it already was, which cannot be
known without asking. Two tools replace the toggles with something that
can be asked and told:

- `GET_OPTION [option]` reports every option and its value, name and
  value tab separated, one per line; with a name, that option's line
  alone.
- `SET_OPTION <option> on|off` puts an option in a state. Asking for the
  state it is already in changes nothing and succeeds, so the same
  request twice is one change.

Each option is read where its state lives rather than from the setting
behind it, so `GET_OPTION` reports what is true now and not what was
configured. Which options a build has depends on what it was built with,
so `GET_OPTION` with no argument is the list: it covers what is on or off
(pausing, fast-forward, mute, the menu, the shader, recording), what the
frontend is doing (the frame count, the state and replay slots, whether
content is loading), and the configured directories. Some only report,
and `SET_OPTION` says so for them.

Options take effect at once and report the resulting state. Speed changes
respect the runloop's pause, netplay, core override and hardcore restrictions.
An option that cannot change right now is an error. Recording and streaming
share one recorder; stop the active mode before starting the other.

A hotkey tool that is still a tool answers `<NAME> pressed` for the same
reason: the press goes out on the next poll, and what it
changed is read back with `GET_OPTION`.

`GET_LOG [since]` gives the lines RetroArch has logged since line
`since` (0 when left out), as far as the logging verbosity lets them be
logged, then `NEXT` and the line number to ask from next time; when
lines were overwritten before they were read it says `SKIPPED` and how
many first. The last 256 lines are kept, from when the MCP server
starts or the first `GET_LOG`.

`PRESS <player> <control>[+<control>...] <frames>` is how an assistant
taps a button: it holds the buttons, runs that many frames and lets
them go, answered once the last frame has run. A tap is 2 to 4 frames;
`PRESS 1 right+a 6` runs and jumps.

This is one request where four were needed - `SET_INPUT`, `RUN_FRAMES`,
`SET_INPUT` to release, `RUN_FRAMES` again - and each of those is its
own HTTP connection, so it is latency saved as much as tokens. The
buttons are let go of however the press ends, including the error that
ends it early. Axes are `SET_INPUT`'s: they are held at a value rather
than pressed, so `PRESS` refuses one.

**A single request runs at most nine seconds of frames** - 540 at
60fps, 450 at 50fps, and never more than 600 - because a reply is
waited for ten. An MCP client
is told the command did not answer after ten seconds and the frames run
on regardless, so both commands refuse more than that outright rather
than failing later with hundreds of frames already run. Ask again to
run more.

`PRESS` holds the limit on every interface, because it holds input
through every frame it runs and a press nobody is waiting for would go
on holding buttons down. `RUN_FRAMES` keeps the full ten minutes on the
network and stdin interfaces, which have no deadline, and leaves
nothing held if its reply is dropped - so that pair is the way to hold
a control for longer than ten seconds.

`WATCH <label> <address>` watches the byte at a system address, and
`RUN_FRAMES`, `PRESS` and `RUN_UNTIL` then carry it, as a line of
label, address and value after the command's own answer:

```
PRESS 1 right+a 4
x	7e0086	40
mode	7e0770	8
```

A step and the look that follows it become one request, rather than
the step and then a read for each thing the assistant wants to know.

One request sets several - `WATCH x 7e0086 mode 7e0770` - and takes
all of them or, where one is refused, none. `WATCH` with nothing
answers what is watched and what each byte holds now, `off` in place of
an address stops watching that label, and a label already watched keeps
its place in the list when its address changes. Values are decimal,
0 to 255, as `RUN_UNTIL` takes them; addresses are hexadecimal. Eight at once. Each is
checked when it is set - an address nothing maps is refused there -
and read afresh on every reply, so a core that moves its memory is
followed; one that stops mapping a watched address answers `none` for
it. All of them are dropped when content closes, since the next
content's addresses mean something else.

`RUN_UNTIL <address> <condition> <value> [max frames]` runs the content
until the byte at a system address meets the condition - `==`, `!=`,
`<`, `>`, `<=` or `>=` against a value from 0 to 255 - and answers with
how many frames ran, what `WATCH` watches, and what the byte holds:

```
MET 12
x	7e0086	52
VALUE 5
```

Where a client would otherwise run a few frames, read the byte and go
round again, this is one request. The address is the kind
`GET_MEMORY_MAP` gives and `READ_CORE_MEMORY` takes, and one nothing
maps is refused before any frame runs rather than after a wait that
could not have ended. Already meeting the condition answers `MET 0`.
The byte is looked up afresh every frame, as the watched ones are, so a
core that moves its memory is followed; one that stops mapping the
address while it is waited on ends the wait with an error.

Left to itself it waits about nine seconds of the content's own frames
- 540 at 60fps, 450 at 50fps - so the answer comes inside the
ten-second deadline at any frame rate, and `[max frames]` asks for up
to that many. When they run out first it answers `WAITING` with how far it got,
so the next request carries on from there instead of the client being
told it failed.

`RUN_FRAMES <count>` pauses the content, if it is running, and runs it
that many frames, one per frame advance, answering once the last has
run; it is left paused. Use it to let the game run on its own - a
cutscene, a fall - where `PRESS` is for holding something while it
does. Both are refused while the menu is open, without content, and in
achievements hardcore mode, and both are the same one job: a second
`RUN_FRAMES` or `PRESS` replaces one still running, which is answered
with an error, as it is when the content closes.

`SET_INPUT` with `RUN_FRAMES` is still the way to hold something across
several steps - a direction held while reading memory between frames -
and to put a stick anywhere but centre.

`SET_INPUT <player> <control> <value>` holds a RetroPad button (`a`,
`b`, `start`, `up` and the rest; 1 holds it, 0 lets it go) or a stick
axis (`left_x`, `left_y`, `right_x`, `right_y`; -32768 to 32767, 0 lets
it go) for the core, over the player's own controller, until it is set
again or `RELEASE_INPUT [player]` lets go of it. Both answer with the
player, the buttons held and the axes held, tab separated, and `none`
for either where nothing is held. `RELEASE_INPUT` with no player lets
go for every player and answers for the first of them. Achievements
hardcore mode refuses `SET_INPUT`. `GET_INPUT [player]` answers in the same
shape **without changing anything**, which neither of the others can
do: one has to set something to answer and the other lets go before it
does.

Several controls join with `+` to be held together:
`SET_INPUT 1 right+a 1` runs and jumps in one request, where two
requests would take two frames. Every control is checked before any is
held, so a combination with a name or a value wrong in it holds
nothing. `PRESS` takes the same combinations.

The running core's own options are `GET_CORE_OPTION` and
`SET_CORE_OPTION`. With no key, `GET_CORE_OPTION` lists every option as
key, value and description, tab separated; with a key it gives that
option's line with the values it takes after a further tab, separated
by `|`. `SET_CORE_OPTION <key> <value>` takes one of those values and
answers with the option's line; asking for the value it already has
changes nothing. Whether a change applies at once or on the next start
of the content is the core's to say.

`GET_MEMORY_MAP` lists the regions of the core's memory map, one per
line: the start address `READ_CORE_MEMORY` and `WRITE_CORE_MEMORY` take,
the length, the select and disconnect masks (hexadecimal), what the
region holds (`system`, `save`, `video` or `other`), `rw`, `ro`, or
`none` when nothing backs it, the byte order, and the address space
name. A region with a select mask answers every address that agrees
with its start in the selected bits, not only start to start plus
length.

To find something to play, an assistant lists the playlists with
`LIST_PLAYLISTS`, reads one with `GET_PLAYLIST` (index, label, content
path and core path per entry, 200 at a time; `MORE <next>` gives the
argument for the next page, as in `Nintendo - SNES.lpl 200`), and loads
an entry with `LOAD_CONTENT <core path>|<content path>`. `LIST_CORES`
gives the installed cores and their paths.

An entry with no core of its own gives `DETECT` as its core path, and
`LOAD_CONTENT` takes an entry as it reads: `DETECT`, or no `core` at
all, loads the content with an installed core that supports it. Both
paths are checked before anything is unloaded, so content that cannot
be found and a core that is not installed are answered with what is
wrong while the running game keeps playing.

Where more than one installed core supports the content, `LOAD_CONTENT`
says so and leaves the choice: `LIST_CORES` with that same content path
answers the candidates as rows, so a client reads the list from the tool
whose job it is rather than out of an error message. Where none does,
`LIST_CORES` answers no rows, as it does when no cores are installed at
all.

Over MCP, `playlist` and `first` are separate parameters, so a name is
never mistaken for an entry number. On the network and stdin interfaces
the two share one line, where a trailing number alone is the entry to
start at: `Atari - 2600` reads there as the playlist `Atari -` from
entry 2600, while `Atari - 2600.lpl` cannot be read as anything but the
playlist.

A tool whose work runs over later frames answers once that work is
through, with what came of it, or with the command's name, `ERROR` and
the reason:

- `LOAD_CONTENT`, `START_CORE`: the content's path once it runs; a core
  that fails to start is an error. `CLOSE_CONTENT`, `UNLOAD_CORE`: once
  the content is closed. Requests sent while content loads wait for it.
- `STATE`: once the state is applied or written.
- `REPLAY`: `play`, `seek`, `record` and `checkpoint` once the replay
  has started or the seek or checkpoint has run; `halt` is a hotkey
  press, answered at once.
- `SET_SHADER`: the preset's path once it has compiled.
- `AI_SERVICE`: one translation of the screen, answered with the
  service's text.

A tool that fails says why, as `<NAME> ERROR <reason>`, and the result
is marked `isError`. Whether a reply is a failure is the command's own
word, not something read out of its text: a screenshot path that holds
`ERROR` - a game named so - is no error, and a failure whose text says
nothing about one still is.

Work that takes longer than ten seconds, such as a slow load, is
answered with an error saying so; the work goes on, and `GET_STATUS`
tells when it is through. A request still waiting for its answer when
the server goes is answered with an error rather than left without one.

Hotkey tools (`MENU`, `DISK`, `QUIT`, `RESET`) press the hotkey for one
frame.
`SCREENSHOT` and `REPLAY` are the exceptions, answered as above; `SCREENSHOT`
takes the screenshot at once and answers when it has been written - with the
picture itself as image content (PNG, up to 4 MiB) and the file's path as
text, so a client can see the screen without reaching the file - or with
`SCREENSHOT ERROR` and the reason if it could not be.

Every tool carries hints for the client:

- **read-only**: only reports (`VERSION`, `GET_STATUS`, `READ_CORE_MEMORY`,
  `GET_MEMORY_MAP`, `GET_CORE_OPTION`);
- **destructive**: can lose unsaved progress or data, or loads code
  (`QUIT`, `RESET`, `STATE`, `WRITE_CORE_MEMORY`, `LOAD_CORE`,
  `LOAD_CONTENT`).

`SET_OPTION` is neither: it writes, but only to the options `GET_OPTION`
lists, and none of them loses progress.

Most clients ask before running a destructive tool. Keep that on.

A core path that is not a core is found out before anything is torn
down for it. `LOAD_CORE` and `LOAD_CONTENT` both probe the library
first - the same probe core scanning uses - and answer `no core at
<path>` or `<path> is not a libretro core`. The core already loaded is
not probed. A stale library or something renamed would otherwise
unload the running core and then fail, costing the game that was
running.

## Security

An MCP client with the token can do what the tools allow, and `LOAD_CORE`
loads a library from any path: treat the token like a password.

- The server listens on `127.0.0.1` and is reachable from this machine
  only. Changing `mcp_server_bind_address` exposes it to the network;
  anyone who can reach the port and has the token then controls
  RetroArch.
- Every request must carry the token. Requests without it get HTTP 401.
- A request from a web page is refused (HTTP 403) unless the page itself
  is served from this machine, so a site cannot reach the server by
  pointing a host name at `127.0.0.1`.
- To revoke a client's access, clear **MCP Server Token**: a new one is
  made on the next start.

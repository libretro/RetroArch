# MCP server: RetroArch as a tool for AI assistants

RetroArch can act as a [Model Context Protocol](https://modelcontextprotocol.io)
server, so an AI assistant that speaks MCP can drive it: pause and resume,
load and save states, take screenshots, read core memory, load content.
Every tool is one of RetroArch's network commands, described as `HELP`
describes it on the network command interface. Not every command is a
tool: `tools/list` leaves out the ones an assistant has no use for, or
that another tool covers.

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

If it cannot listen there - another RetroArch still running holds the
port, say - it says so on screen as well as in the log, since a client
would otherwise be talking to that other RetroArch without knowing.

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
and 2025-03-26 for clients that open with `initialize`. Its list results
and `server/discover` carry `ttlMs: 0` and `cacheScope: "private"`, as
2026-07-28 requires
them: the tools are compiled in and `listChanged` is false, so nothing
would tell a client holding a list that an upgrade changed it. A client
that keeps one anyway sees a rebuilt RetroArch's new parameters once it
reconnects.

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

## The handshake

`initialize` answers with `instructions`: how the tools fit together to
play a game - find it with `LIST_PLAYLISTS` and `GET_PLAYLIST`, load it
paused, `WATCH` what each step needs, step with `STEPS` - queued, so
the game plays on while the next stretch is planned - `PRESS ... until`
or `RUN_UNTIL`, and `STATE checkpoint` before a risky move. A client
shows this to the model once rather than in every tool.

Every tool also has a `title` for people, made from its name: `Run
frames` for `RUN_FRAMES`.

## What the tools do

A tool is named as its command (`GET_STATUS`, `STATE`,
`SCREENSHOT`). Its parameters are named, typed and
marked needed or not, so a client is told what to send rather than
having to write the command's own line by hand:

```json
{ "name": "LOAD_CONTENT",
  "arguments": { "core": "/path/to/core.so", "content": "/games/a.rom" } }
```

`STATE` takes an `action`, and a `slot` number for `save` and `load`,
`GET_PLAYLIST` takes
`playlist` and an optional `first`, `OPTION` takes an `action`, an
`option` and a `state` of `on` or `off` - which the schema gives as the
only two values it accepts. `tools/list` describes every one of them.

A parameter that is needed and left out, one that should be an integer
and is not, one the tool does not take, a value outside the set a
parameter offers, one sent as `true`, `false`, `null`, an object or an
array - no tool takes those - and a tool that does not exist are
answered before
anything runs, as JSON-RPC invalid params (`-32602`) rather than as a
tool failure. The schemas leave other names unforbidden to keep the
tool list short; the server refuses them instead, so a misspelt
parameter is never dropped and the command run without it.

The single `argument` string this server took before the parameters
still works, so a client holding a cached tool list keeps going until it
reads the new one.

The tool's result is what the command answers; commands that answer
nothing report `Done.`.

`GET_STATUS` and `READ_CORE_MEMORY` answer one line, and give it as
fields of its own: `{"state": "PLAYING", "system": "nes", "content":
"Super Mario Bros.nes"}` and `{"address": "1000", "bytes": "AA 0F
10"}`. The last field takes the rest of the line, so a comma in a
game's name stays in it. A read that runs past the end of one region
goes on into the next; where nothing maps the rest it stops there and
says so, `TRUNCATED 2` after the bytes, given as `truncated: true,
read: 2`.

## Results that are rows

The replies that are lines of tab-separated fields - `OPTION`,
`LIST_CORES`, `LIST_PLAYLISTS`, `GET_PLAYLIST`, `CORE_OPTION`,
`GET_MEMORY_MAP`, `GET_LOG`, `INPUT`, `WATCH`, `RUN_FRAMES`, `PRESS`,
`RUN_UNTIL`, `STEPS` and `MENU` - come
with `structuredContent` beside the text, so a client is handed the
rows rather than splitting them itself. Each of those tools declares an
`outputSchema` naming its columns, described by the first tool in the
list to answer with them - `RUN_FRAMES` for the watched bytes, which
`PRESS`, `RUN_UNTIL`, `STEPS` and `WATCH` then name without repeating:

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
`player`, `buttons` and `axes` for what a player holds, and `label`,
`address` and `value` for the bytes `WATCH` watches, and `index`,
`label` and `value` for the menu's entries. Every field
is a string: it is the text's own field, which the server does not read
as anything else.

**A tool answers with `structuredContent` exactly when it declared an
`outputSchema` and succeeded**, so a client reads the schema it was
handed at `tools/list` and does not have to probe each reply. The
end-to-end test calls every tool that declares one and holds each reply
to it: the required fields, the declared types, and no field it leaves
out. A reply
marked `isError` is its reason as text, with no `structuredContent`. A tool with no
`outputSchema` answers with text alone, because its reply is prose or a
confirmation rather than rows.

A reply may end short of its columns, and the schema requires only the
ones every row carries: `CORE_OPTION show` gives an option's `values`
only where it was asked for that option by key.

A line the command opens with its own name, such as `LIST_CORES none`,
is not a row. Neither are the lines that carry a number about the reply
rather than a row of it, which become fields of their own - the number
ending the line, so `PRESS 1 right+a 4` gives `frames: 4`:

| Line | Field | Where |
| --- | --- | --- |
| `MORE` | `more: true` | a list cut short with nowhere to resume |
| `MORE 200` | `more: true`, `next: 200` | `GET_PLAYLIST`'s next page |
| `NEXT 48` | `next: 48` | where `GET_LOG` reached |
| `SKIPPED 12` | `skipped: 12` | log lines gone before the first kept |
| `RUN_FRAMES 3` | `frames: 3` | the frames `RUN_FRAMES` ran |
| `PRESS 1 a 4` | `frames: 4` | the frames `PRESS` ran |
| `MET 12` | `met: 12` | the frames `RUN_UNTIL` ran until the byte met it |
| `WAITING 540` | `waiting: 540` | the frames it ran without that |
| `VALUE 5` | `value: 5` | what `RUN_UNTIL`, or a `PRESS` with `until`, waited on holds |
| `QUEUED 4` | `queued: 4` | the steps a queued `STEPS` has left to run |
| `DISCARDED 3` | `discarded: 3` | the steps a queued `STEPS` dropped, as the queue had ended |
| `PREVIOUS STOPPED 43 STOP 1 STEP 1` | `previous: {"stopped": 43, "stop": 1, "step": 1}` | how a queued `STEPS` ended, on the next reply; `PREVIOUS ERROR <why>` gives `error` |
| `IDLE 1830` | `idle: 1830` | milliseconds the content has sat paused since the queue ran dry |
| `DONE 30` | `done: 30` | the frames `STEPS` ran when every step had |
| `STOPPED 12` | `stopped: 12` | the frames it ran when its `stop` held |
| `STOP 2` | `stop: 2` | which `stop` held, from 1, when `STEPS` stopped |
| `STEP 3` | `step: 3` | the step it ended on, from 1 |
| `TRUNCATED 2` | `truncated: true`, `read: 2` | a `READ_CORE_MEMORY` that met unmapped memory |
| `CLOSED` | `closed: true` | `MENU` with the menu closed |
| `SELECTED 3` | `selected: 3` | the menu entry selected |
| `COUNT 40` | `count: 40` | the entries in the menu's list |
| `MESSAGE Press again to quit` | `message: "Press again to quit"` | a message over the menu, on any line |
| `KEYBOARD Username` | `keyboard: "Username"` | the menu asking for text, on any line |
| `PATH Main Menu > Settings` | `path: "Main Menu > Settings"` | the lists above the menu's own, on any line |
| `MORE 50` | `more: true`, `next: 50` | the menu's next page, as `from` takes it |

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
`GET_CONFIG_PARAM`, whose work `OPTION` does below.

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

The disc is the exception: `DISK` steps blind, and a disc swap is part
of playing a game. It is one tool taking a choice - `DISK next` -
rather than a tool per hotkey; `MENU`, below, answers with where it
landed.
`REINIT`, `STATE`, `REPLAY`, `CORE`, `INPUT`, `OPTION` and
`CORE_OPTION` stand for their families likewise: `STATE load 3` is
`LOAD_STATE_SLOT 3`, `REPLAY seek 600` is `SEEK_REPLAY 600`, `CORE load
<path>` is `LOAD_CORE <path>`, `INPUT set 1 a 1` is `SET_INPUT 1 a 1`,
and `OPTION show pause` is `GET_OPTION pause`. A reply that is rows -
`INPUT`'s, `OPTION`'s - is the tool's answer as it is. Each
answers under its own name - `STATE load 3`,
`DISK next pressed`, `REPLAY ERROR no replay is playing or recording`,
and `REINIT audio` for a restart that says nothing itself - while the
commands sent by name answer as they always have. `DISK` is refused on
a core with no disk control, where the hotkeys would do nothing. A word
the tool does not take is invalid params naming the ones it does; on
the line interfaces it is `STATE ERROR one of
save|load|checkpoint|rollback`, and so is
anything after a word that takes nothing. `REPLAY`'s slot or frame is its `number` parameter.

`SAVE_FILES` and `LOAD_FILES` are not tools: the core writes its save
files itself, and playing a game does not need them.

All of them are still commands on the network and stdin interfaces,
where a person is typing and can see what happened.

## The menu

`MENU <action> [index] [from <entry>]` reads and drives RetroArch's own
menu - settings, playlists, cores, the Quick Menu - and answers with
what it shows once the action has run: its title, a page of 50 entries
holding the selection, which entry is selected and how many there are.

```
MENU down
MENU Quick Menu
0	Resume	
1	Reset	
2	Close Content	
3	Save States	...
SELECTED 1
COUNT 15
```

The menu opens where it was left, so below the title `PATH` names the
lists above it - `PATH Main Menu > Settings > Drivers` - and `b` goes
back up them. Where the list runs past the page, `MORE 50` says where
the next one starts, for `from`.

XMB and Ozone keep their top level in tabs - Main Menu, Settings,
History, the playlists - rather than in one list. On XMB, `left` and
`right` at the top switch tabs. On Ozone, `left` moves to the tabs,
`up` and `down` pick one, and `right` returns to its list; `a` there
only returns too. A playlist's tab is titled with the playlist's name.

Each entry is its index, its label and its value: a setting's value,
`...` for an entry that opens a list, or `checked` for the current
choice in a list of them - the driver in use, say. `title`, `rows`, `selected`
and `count` are its structured fields.

- `show` answers what the menu shows; `from` lists from that entry.
- `open` and `close` open and close it. With it closed, `show` and
  `close` answer `CLOSED`, and the rest are refused. With no content
  running the menu cannot close, and `close` says so.
- `select <index>` moves straight to an entry, rather than pressing
  `down` 137 times.
- `up`, `down`, `left`, `right`, `a` and `b` press the menu's buttons:
  `left` and `right` change a value, `a` confirms, `b` goes back. `left`,
  `right` and `a` take an index too, acting on that entry: `MENU a 3`
  opens Save States.
- `type <text>` answers a keyboard: one line, entered whole and
  confirmed, as a platform's own keyboard hands it over.

What waits on the user comes as a line of its own after the title. A
message - `MESSAGE Press again to close content...`, the confirmation
Close Content, Reset and Quit ask for - is confirmed with `a` and
dismissed with `b`. An entry that asks for text - a username, a search
- answers `KEYBOARD Username`, and `type` or `b` ends it.

The action runs on the menu's next frame as its own input would, and
the answer comes a frame after that, so a list or a message an action
opens is there to read. `MENU open` and `close` are the menu's switch:
`menu` is not an `OPTION`. A second `MENU` while one waits replaces it, answered
with an error; one whose list another hand changes first does nothing
to the entry that took its place, and says so; and one whose menu closes first - a hotkey, another
client - is answered `CLOSED`. `MENU` is marked destructive: its
entries include Quit, Close Content and Delete. It is RetroArch's menu, not a game's: a game's menus are
its screen and memory, played with `PRESS`, `STEPS` and `WATCH`.

## Options

A hotkey toggle is of little use to an assistant: `FAST_FORWARD` turns
fast-forward on or off depending on what it already was, which cannot be
known without asking. `OPTION` replaces the toggles with something that
can be asked and told:

- `OPTION show [option]` reports every option and its value, name and
  value tab separated, one per line; with a name, that option's line
  alone.
- `OPTION set <option> on|off` puts an option in a state. Asking for the
  state it is already in changes nothing and succeeds, so the same
  request twice is one change.

Each option is read where its state lives rather than from the setting
behind it, so `OPTION show` reports what is true now and not what was
configured. Which options a build has depends on what it was built with,
so `OPTION show` with no argument is the list: it covers what is on or off
(pausing, fast-forward, mute, the shader, recording), what the
frontend is doing (the frame count, the state and replay slots, whether
content is loading), and the configured directories. Some only report,
and `OPTION set` says so for them.

Options take effect at once and report the resulting state. Speed changes
respect the runloop's pause, netplay, core override and hardcore restrictions.
An option that cannot change right now is an error. Recording and streaming
share one recorder; stop the active mode before starting the other.

A hotkey tool that is still a tool answers `<NAME> pressed` for the same
reason: the press goes out on the next poll, and what it
changed is read back with `OPTION show`.

`GET_LOG [since]` gives the lines RetroArch has logged since line
`since` (0 when left out), as far as the logging verbosity lets them be
logged, then `NEXT` and the line number to ask from next time; when
lines were overwritten before they were read it says `SKIPPED` and how
many first. The last 256 lines are kept, from when the MCP server
starts or the first `GET_LOG`.

`PRESS <player> <control>[+<control>...] <frames>` is how an assistant
taps a button: it holds the buttons, runs that many frames and lets
them go, answered once the last frame has run. A tap is 2 to 4 frames;
`PRESS 1 right+a 6` runs and jumps. What `INPUT set` holds stays held,
under the press and after it: `INPUT set 1 right+b 1` then `PRESS 1 a
20` is a running jump.

This is one request where four were needed - `INPUT set`, `RUN_FRAMES`,
`INPUT set` to release, `RUN_FRAMES` again - and each of those is its
own HTTP connection, so it is latency saved as much as tokens. The
buttons are let go of however the press ends, including the error that
ends it early. Axes are `INPUT set`'s: they are held at a value rather
than pressed, so `PRESS` refuses one.

`PRESS ... until <condition>` lets go and runs on until the condition
holds, as `RUN_UNTIL` would, and answers as `RUN_UNTIL` does:
`PRESS 1 a 20 until 1d == 0` is a jump and its landing in one request.

**A single request runs at most nine seconds of frames** - 540 at
60fps, 450 at 50fps, and never more than 600 - because a reply is
waited for ten. An MCP client is told the command did not answer after
ten seconds and the frames run on regardless, so `RUN_FRAMES`, `PRESS`,
`STEPS` and `RUN_UNTIL`'s `[max frames]` all refuse more than that outright
rather than failing later with hundreds of frames already run:
`RUN_FRAMES` answers "a frame count from 1 to 540; ask again to run
more" for a count of -5 and of 600 alike.

`PRESS` has a limit on every interface - nine seconds of frames over
MCP, 600 frames on the network and stdin interfaces - because it holds
input through every frame it runs and a press nobody is waiting for
would go on holding buttons down. `RUN_FRAMES` keeps the full ten minutes on the
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
its place in the list when its address changes; with nothing watched
it answers `WATCH none`. Values are decimal, 0 to 255 - or to 65535
for a `high:low` watch - as `RUN_UNTIL` takes them; addresses are hexadecimal. A label is up to 15 characters,
with no tab or newline in it. `x 6d:86` watches two bytes as one value,
high:low, as conditions read them. Sixteen at once. Each is checked when it is
set - an address nothing maps is refused there -
and read afresh on every reply, so a core that moves its memory is
followed; one that stops mapping a watched address answers `none` for
it. All of them are dropped when content closes, since the next
content's addresses mean something else.

`RUN_UNTIL <address> <condition> <value> [max frames]` runs the content
until the byte at a system address meets the condition - `==`, `!=`,
`<`, `>`, `<=` or `>=` against a value from 0 to 255, decimal or `0x`
hexadecimal (`ce & 0xF0 == 0x70`, as `STEPS` and `PRESS ... until`
take them too; over MCP `RUN_UNTIL`'s `value` is a JSON number) - and
answers with
how many frames ran, what `WATCH` watches, and what the byte holds.
Two addresses as `high:low` read two bytes as one value, 0 to 65535:
`6d:86 >= 1800` waits for Super Mario Bros.'s Mario to pass x 1800,
which the game keeps as a page in one byte and a place on it in
another. `& <mask>` after the address compares only those bits, as a
game keeps its flags: `1d & 0x40 != 0`; the value it answers is the
masked one.

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

`STEPS <step>[; <step>...]` runs a stretch of play as one request. Each
step is `run <frames>`, `press <player> <controls> <frames>`, `until
<condition>` or `checkpoint`, in order; up to four `stop <condition>`
entries anywhere in the list end it after any frame where one holds - a
death, the level's end - so neither plays out the rest. `checkpoint`
keeps the state `STATE rollback` returns to, where the list has got to,
so a stretch can mark its own retry point:

```
STEPS until 6d:86 >= 1500; press 1 a 20; until 1d == 0; stop e == 11
DONE 74
x	86	212
STEP 3
```

Two presses of the same button in a row get a frame with it let go
between them, so the game reads two taps rather than one long press -
also where one request ends on the press and the next starts with it.
That frame is the second press's own, not taken from it, and is not
needed where `INPUT set` held the button throughout or a frame has run
since.

It answers `DONE` when every step has run, `STOPPED` when `stop` held,
and `WAITING` when its nine seconds ran out first, each with the frames
run, what `WATCH` watches, and the `STEP` it ended on - so after
`WAITING` the client asks again with the steps from that one. 32
steps at most; the frames `run` and `press` ask for must fit in the
nine seconds, and an address nothing maps is refused before any frame
runs.

**Queued, the content plays on between requests.** `STEPS <steps>|on`
(`queue: on`) answers at once with `QUEUED` and the steps left to run,
and its steps go behind those still running, so the game does not pause
while the next stretch is planned; it pauses once they have all run.
Each list runs with its own stops - none where it has none - and counts
its own frames. A stop that held, a wait that ran out or a failure
ends the whole queue, lists behind it too.

**The next reply, whatever the request, says how the queue ended**, on
a `PREVIOUS` line after its own first one: `PREVIOUS STOPPED 43 STOP 1
STEP 1`, `PREVIOUS WAITING 540 STEP 1` or `PREVIOUS ERROR <why>`, given
as `previous` - its `STEP` counts within the list that step came in,
stops aside. A queued `STEPS` then runs none of its own steps, which
would play on from a death: it answers `DISCARDED` with how many it
dropped, or `QUEUED 0` where it brought none. Any other request runs as
asked. A request that waits - `RUN_FRAMES`, `PRESS`, `RUN_UNTIL`,
`STEPS` without `on` - while the queue still runs replaces it, and its
reply says so: `PREVIOUS REPLACED <frames run> STEP <step>`. Each
ending is told once.

A rollback, a state load, `RESET` and closing the content each end the
queue and forget what it came to, however they come - a hotkey, the
menu, a command; asked for as `STATE` or `RESET`, the reply says so:
`STATE rollback, queue ended`. With no steps it only asks. A queue that ran dry
says so: `IDLE` is how long, in milliseconds, the content has sat paused
since, waiting for more; a frame run meanwhile - the user unpausing -
ends it. A client that sees it knows it has fallen behind: two
stretches sent at once get it ahead again. Nine seconds bound each
step, not the queue: an `until` that has not held by then ends it
`WAITING`. It holds 64 steps and stops at most.

`RUN_FRAMES <count>` pauses the content, if it is running, and runs it
that many frames, one per frame advance, answering once the last has
run; it is left paused. Use it to let the game run on its own - a
cutscene, a fall - where `PRESS` is for holding something while it
does. Both are refused while the menu is open, without content, and in
achievements hardcore mode, and they, `RUN_UNTIL` and `STEPS` are the
same one job: a second of any of them replaces one still running, which is answered with an error, as it is when the content
closes.

`INPUT set` with `RUN_FRAMES` is still the way to hold something across
several steps - a direction held while reading memory between frames -
and to put a stick anywhere but centre.

`INPUT set <player> <control> <value>` holds a RetroPad button (`a`,
`b`, `start`, `up` and the rest; 1 holds it, 0 lets it go) or a stick
axis (`left_x`, `left_y`, `right_x`, `right_y`; -32768 to 32767, 0 lets
it go) for the core, over the player's own controller, until it is set
again, `INPUT release [player]` lets go of it, or the content closes -
the next content does not start with a button held for this one. Both answer with the
player, the buttons held and the axes held, tab separated, and `none`
for either where nothing is held. `INPUT release` with no player lets
go for every player and answers for the first of them. Achievements
hardcore mode refuses `INPUT set`. The core is given exactly what is
held: opposing directions are not cleaned and the D-Pad is not turned
with the picture, and under netplay the player is this machine's, so a
hold is sent like a press. `INPUT show [player]` answers in the same
shape **without changing anything**, which neither of the others can
do: one has to set something to answer and the other lets go before it
does.

Several controls join with `+` to be held together:
`INPUT set 1 right+a 1` runs and jumps in one request, where two
requests would take two frames. Every control is checked before any is
held, so a combination with a name or a value wrong in it holds
nothing. `PRESS` takes the same combinations.

The running core's own options are `CORE_OPTION`. With no key, `CORE_OPTION show` lists every option as
key, value and description, tab separated; with a key it gives that
option's line with the values it takes after a further tab, separated
by `|`. `CORE_OPTION set <key> <value>` takes one of those values and
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

A core that publishes no memory map - snes9x among them - still hands
its RAM over through `retro_get_memory_data`, and every memory tool reads
that instead: system RAM from address 0 and save RAM straight after it,
the layout RetroArch's achievements use when they know no console.
`GET_MEMORY_MAP` lists the two as `system` and `save`, in the address
space `core`. These are offsets into the core's RAM, not the console's
addresses: the SNES's work RAM at `$7E0000` is `0` here, which is also the
address RetroAchievements' notes give for it. Other consoles' notes may
place RAM elsewhere - the NES's save RAM at `0x6000`, say, is straight
after system RAM here. Reads run across from one
to the other, `WATCH` and conditions take them, and `WRITE_CORE_MEMORY`
writes them.

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

- `LOAD_CONTENT`, `CORE start`: the content's path once it runs; a core
  that fails to start is an error. `CLOSE_CONTENT`, `CORE unload`: once
  the content is closed. Requests sent while content loads wait for it.
  `LOAD_CONTENT` with `paused` on pauses the content once it has
  loaded, so nothing runs on between the load and the first step: a
  title screen's demo starting, say. The core has run a frame by the
  answer - two where no content was running before, as a core counts
  as running after its first - and a game may take a few more to set
  up its RAM, so run some before reading it. On the network
  and stdin interfaces `<core>|<content>|on` does the same, and is
  answered there too.
- `STATE`: once the state is applied or written. `STATE checkpoint`
  keeps one state in memory, not in a slot's file, and `STATE rollback`
  restores it: the retry after a mistake, in one request where
  replaying the level took a dozen. What `INPUT set` holds stays held
  through it; a queued `STEPS` ends, as it does on any load. The
  checkpoint lives as long as the content does; a rollback is refused
  while a replay records, as a load from a slot is.
- `RESET`: once the content is reset; with `paused` on, paused after
  it before any frame runs, so the game has not set up its RAM yet. A
  queued `STEPS` ends.
- `REPLAY`: `play`, `seek`, `record` and `checkpoint` once the replay
  has started or the seek or checkpoint has run; `halt` is a hotkey
  press, answered at once.
- `SET_SHADER`: the preset's path once it has compiled.

A tool that fails says why, as `<NAME> ERROR <reason>`, and the result
is marked `isError`. It is a JSON-RPC `result`, not a JSON-RPC
`error`: those are kept for a request the server could not run at all -
an unknown tool, invalid params, broken JSON. Whether a reply is a failure is the command's own
word, not something read out of its text: a screenshot path that holds
`ERROR` - a game named so - is no error, and a failure whose text says
nothing about one still is.

A content load is answered when it finishes, however long it takes:
the server is not polled while one runs, and requests sent meanwhile
wait for it. Other work that takes longer than ten seconds - a shader
that is slow to compile, say - is answered with an error saying so;
the work goes on, and `GET_STATUS` tells when it is through. A request still waiting for its answer when
the server goes is answered with an error rather than left without one.

A request over HTTP is at most 64 KiB, headers and body together, and a
larger one is answered `413`; only `POST /mcp` and `POST /` are served.
Over stdio a line past 64 KiB is answered with a parse error and skipped
to its newline, and a `\r` before the newline is taken as part of it.
Batch requests are refused as invalid.

Hotkey tools (`QUIT`), and the hotkeys behind `DISK` and `REPLAY
halt`, press for one frame. `SCREENSHOT` is the exception: it
takes the screenshot at once and answers when it has been written - with the
picture itself as image content (PNG, up to 4 MiB) and the file's path as
text, so a client can see the screen without reaching the file - or with
`SCREENSHOT ERROR` and the reason if it could not be.

`GET_FRAME [size]` is the look without the file: the content's frame
on screen now, scaled down to fit a square of `size` pixels (64 to
640, 320 when left out; never scaled up), answered as image content
with `GET_FRAME 320x240` as its text. Nothing is written - no file, no
history entry, no notification - so it costs a client only the image.
It takes the frame the way `SCREENSHOT` does, from the GPU's read-back
for a hardware-rendered core, and is refused while another screenshot
is being taken. It is for what memory cannot say: a game whose RAM no
one has charted, or checking what the RAM says. It is not for the step
loop, where `WATCH`'s numbers cost far less than a picture. The line
interfaces carry no image, so there it is refused, pointing to
`SCREENSHOT`.

Every tool carries hints for the client:

- **read-only**: only reports (`GET_STATUS`, `GET_FRAME`, `GET_LOG`, `LIST_CORES`,
  `LIST_PLAYLISTS`, `GET_PLAYLIST`, `READ_CORE_MEMORY`,
  `GET_MEMORY_MAP`);
- **destructive**: can lose unsaved progress or data, or loads code
  (`QUIT`, `RESET`, `STATE`, `REPLAY`, `WRITE_CORE_MEMORY`, `CORE`,
  `LOAD_CONTENT`, `CLOSE_CONTENT`, `MENU`).

A tool that is not read-only states `destructiveHint`, true or false,
since its default is true; a read-only one leaves it out, as it means
nothing there. `openWorldHint` is left at its default.

`OPTION`, `INPUT` and `CORE_OPTION` are neither: they write, but only to
the options and controls they list, and none of them loses progress.

Most clients ask before running a destructive tool. Keep that on.

A core path that is not a core is found out before anything is torn
down for it. `CORE load` and `LOAD_CONTENT` both probe the library
first - the same probe core scanning uses - and answer `no core at
<path>` or `<path> is not a libretro core`. The core already loaded is
not probed. A stale library or something renamed would otherwise
unload the running core and then fail, costing the game that was
running.

## Achievements hardcore mode

While RetroAchievements' hardcore mode is on, the server runs no tool:
every `tools/call` is answered as the tool's own failure, `<NAME> ERROR
achievements hardcore mode is on; the player can turn it off`. Hardcore
achievements are the player's own, as the mode's ban on rewind, slow
motion, cheats and states already says. The handshake and `tools/list`
still answer, so a client can connect and learn why. When hardcore mode
takes effect - switched on, or a game loaded with it on - what a client
left playing stops with it, as slow motion does: a running or queued
step job ends, and what `INPUT` holds is let go. Turning hardcore mode
off is the player's, in RetroArch.

## Security

An MCP client with the token can do what the tools allow, and `CORE load`
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

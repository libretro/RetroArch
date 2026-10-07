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

## What the tools do

Each command is one tool, named as the command (`GET_STATUS`,
`SAVE_STATE_SLOT`, `SCREENSHOT`). A command that takes an argument has a
single string parameter, `argument`, written as the command expects it:
`SAVE_STATE_SLOT` takes a slot number, `LOAD_CONTENT` takes
`<core path>|<content path>`. The tool's result is what the command
answers; commands that answer nothing report `Done.`.

Not every command is a tool. `HELP` and `VERSION` are not, because
`tools/list` already lists the commands and every answer names the
server's version, and neither are the toggle hotkeys or
`GET_CONFIG_PARAM`, whose work `SET_OPTION` and `GET_OPTION` do below.
All of them are still commands on the network and stdin interfaces.

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

`fast_forward` and `slow_motion` have no setter in the frontend - their
whole path is the runloop's hotkey handling - so setting them presses
that hotkey, the change lands on the frame after, and `SET_OPTION`
answers that the press went out rather than what came of it. Every other
option takes effect at once and is read back before answering, so an
option that cannot change right now - `pause` with no content running -
is an error and not a quiet success.

A hotkey tool that is still a tool answers `<NAME> pressed` for the same
reason as the two above: the press goes out on the next poll, and what it
changed is read back with `GET_OPTION`.

To find something to play, an assistant lists the playlists with
`LIST_PLAYLISTS`, reads one with `GET_PLAYLIST` (index, label, content
path and core path per entry, 200 at a time; `MORE <next>` gives the
argument for the next page, as in `Nintendo - SNES.lpl 200`), and loads
an entry with `LOAD_CONTENT <core path>|<content path>`. `LIST_CORES`
gives the installed cores and their paths.

Name the playlist as `LIST_PLAYLISTS` gives it, `.lpl` and all. A
trailing number alone is the entry to start at, so `Atari - 2600` reads
as the playlist `Atari -` from entry 2600, while `Atari - 2600.lpl`
cannot be read as anything but the playlist.

A tool whose work runs over later frames answers once that work is
through, with what came of it, or with the command's name, `ERROR` and
the reason:

- `LOAD_CONTENT`, `START_CORE`: the content's path once it runs; a core
  that fails to start is an error. `CLOSE_CONTENT`, `UNLOAD_CORE`: once
  the content is closed. Requests sent while content loads wait for it.
- `LOAD_STATE_SLOT`, `SAVE_STATE_SLOT`: once the state is applied or
  written.
- `PLAY_REPLAY_SLOT`, `RECORD_REPLAY`, `SEEK_REPLAY` and the replay
  checkpoint tools: once the replay has started, or the seek or
  checkpoint has run.
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

Hotkey tools (`PAUSE_TOGGLE`, `FAST_FORWARD`, `MENU_UP`) press the hotkey
for one frame. Tools marked as holds (`FAST_FORWARD_HOLD`, `REWIND`) last
that one frame too. `SCREENSHOT`, `AI_SERVICE` and the replay recording
and checkpoint tools are the exceptions, answered as above; `SCREENSHOT`
takes the screenshot at once and answers when it has been written - with the
picture itself as image content (PNG, up to 4 MiB) and the file's path as
text, so a client can see the screen without reaching the file - or with
`SCREENSHOT ERROR` and the reason if it could not be.

Every tool carries hints for the client:

- **read-only**: only reports (`VERSION`, `GET_STATUS`, `READ_CORE_MEMORY`);
- **destructive**: can lose unsaved progress or data, or loads code
  (`QUIT`, `RESET`, `LOAD_STATE_SLOT`, `WRITE_CORE_MEMORY`, `LOAD_CORE`,
  `LOAD_CONTENT`).

`SET_OPTION` is neither: it writes, but only to the options `GET_OPTION`
lists, and none of them loses progress.

Most clients ask before running a destructive tool. Keep that on.

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

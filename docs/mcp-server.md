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

To find something to play, an assistant lists the playlists with
`LIST_PLAYLISTS`, reads one with `GET_PLAYLIST` (index, label, content
path and core path per entry, 200 at a time; `MORE <next>` gives the
argument for the next page, as in `Nintendo - SNES 200`), and loads an
entry with `LOAD_CONTENT <core path>|<content path>`. `LIST_CORES` gives
the installed cores and their paths.

`LOAD_CONTENT` answers once the load has started, not once it is through:
the load runs over the next frames, and while it does the server restarts
with the rest of RetroArch's command interfaces, so a request sent then
may find no server. `GET_STATUS` reports `PLAYING` with the system and
the content's name once the game runs, and `CONTENTLESS` if the load
failed. A request still waiting for its answer when the server restarts
is answered with an error rather than left without one.

Hotkey tools (`PAUSE_TOGGLE`, `FAST_FORWARD`, `MENU_UP`) press the hotkey
for one frame. Tools marked as holds (`FAST_FORWARD_HOLD`, `REWIND`) last
that one frame too. `SCREENSHOT` is the exception: it takes the
screenshot at once and answers when it has been written - with the
picture itself as image content (PNG, up to 4 MiB) and the file's path as
text, so a client can see the screen without reaching the file - or with
`SCREENSHOT ERROR` and the reason if it could not be.

Every tool carries hints for the client:

- **read-only**: only reports (`VERSION`, `GET_STATUS`, `READ_CORE_MEMORY`);
- **destructive**: can lose unsaved progress or data, or loads code
  (`QUIT`, `RESET`, `LOAD_STATE_SLOT`, `WRITE_CORE_MEMORY`, `LOAD_CORE`,
  `LOAD_CONTENT`).

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

# Built-in network stack: TLS, SMB, NFS and the keychain

RetroArch carries its own implementation of the network protocols it uses:
a TLS client for secure web connections, SMB2/3 and NFS clients for
loading content from network shares, and an encrypted keychain for saved
passwords. All of it lives in `libretro-common` (`crypto/`, `net/`,
`vfs/`, `file/keychain.c`), is written in C89, and needs no third-party
library, so every platform builds it the same way.

## Secure connections (TLS)

RetroAchievements, Cloud Sync, the Online Updater and other HTTPS traffic
go through the built-in TLS client. It speaks TLS 1.3 (AES-128-GCM,
AES-256-GCM, ChaCha20-Poly1305) and TLS 1.2 (ECDHE with AES-GCM or
ChaCha20-Poly1305, over RSA or ECDSA certificates). Server certificates
are checked against the certificate authority bundle compiled into
RetroArch; the operating system's certificate store is not used.

The **TLS Certificate Verification** setting (`tls_verify_mode` in
`retroarch.cfg`) chooses what happens when a certificate does not verify:

| Value | Setting | Behaviour |
| --- | --- | --- |
| 0 | Required (Recommended) | A certificate that does not verify refuses the connection. The default. |
| 1 | Optional (Warn Only) | A failed check is logged and the connection goes ahead. |
| 2 | Disabled (Insecure) | Certificates are not checked; every connection logs a warning. |

Anything other than Required lets someone on the network read and change
the traffic, passwords included. Lower it only to diagnose a connection
problem, and set it back afterwards.

## Loading content from SMB shares

The SMB client connects to Windows shares, Samba and NAS devices using
SMB 2.0.2 through 3.1.1. Messages are signed (a guest session has no key
to sign with): with AES-GMAC on SMB 3.1.1 when the server offers it
(Samba 4.15 and later, Windows 11 and Server 2022), otherwise AES-CMAC,
or HMAC-SHA256 on SMB 2. GMAC runs several times faster than CMAC at
both ends, which matters where every read is signed, as Windows 11 24H2
requires by default. When a server or share requires encryption, traffic
is sealed with AES-CCM (SMB 3.0/3.0.2) or AES-GCM (SMB 3.1.1).
Authentication is NTLMv2 with a user name and password, or Kerberos in
an Active Directory domain.

Enable it under **Settings > Network > SMB Network Settings**:

| Key | Default | Meaning |
| --- | --- | --- |
| `smb_client_enable` | `false` | Turns the client on. |
| `smb_client_server_address` | | Server IP address or host name. |
| `smb_client_share` | | Share name. Empty lists every share the server exports, to pick one while browsing. |
| `smb_client_subdir` | | Directory on the share. Optional. |
| `smb_client_username`, `smb_client_password` | | Credentials; kept in the keychain (see below). |
| `smb_client_workgroup` | | Workgroup or domain. Optional in many setups. |
| `smb_client_auth_mode` | `1` | 1 NTLMSSP, 2 Kerberos, 0 Kerberos if available and NTLM if not. |
| `smb_client_realm` | | Kerberos realm, the Active Directory domain in capitals (`EXAMPLE.COM`). Empty for password authentication. |
| `smb_client_kdc` | | Kerberos key distribution center host. Empty when the SMB server is the domain controller. |
| `smb_client_num_contexts` | `4` | Connections kept open to the server, 1 to 20. |
| `smb_client_timeout` | `5` | Seconds to wait for the server, 1 to 60. |
| `smb_client_readahead` | `0` | Read-ahead in KiB, 0 (off) to 16384 (see below). |

For Kerberos, give the server by host name, not by IP address: the ticket
is issued for that name.

Content on a share is addressed as `smb://server/share/path/to/game.chd`,
and the share appears in the file browser once the client is enabled.
Cores that read files through the VFS stream straight from the share;
other cores have the file copied to local storage first. Changes to these
settings apply the next time a share is browsed; content already running
keeps its connection. A wired connection is more reliable than Wi-Fi.

Besides the desktop and mobile builds, the 3DS, Vita and Switch carry the
SMB client, on their hardware random number generators. The Wii U does
not: it has no kernel source of randomness for the keys SMB derives.

## Loading content from NFS exports

The NFS client supports NFSv3, which finds the NFS and MOUNT services
through the server's portmapper, and NFSv4, which connects straight to
the NFS port and addresses the export as a path in the server's
pseudo-filesystem. With version 4 chosen, the client speaks the newest
of 4.2, 4.1 and 4.0 the server offers, so servers that have dropped 4.0
work too. Over 4.2 it reads with READ_PLUS, which sends the holes of a
sparse file as their extent instead of as zeros: an image the
filesystem stores sparse costs its data on the network, not its size.
A server whose READ_PLUS answers are impossible (nfs-ganesha 4.3's) is
read with plain READ instead. It authenticates with AUTH_UNIX, as the user
RetroArch runs as; platforms without user IDs, such as Windows and the
consoles, send user and group 1000.

**Settings > Network > NFS Network Settings**:

| Key | Default | Meaning |
| --- | --- | --- |
| `nfs_server` | | Server IP address or host name. |
| `nfs_export` | | Exported path, for example `/export/roms`. For NFSv4 the same path works: when the server roots its v4 namespace lower down (Linux `fsid=0`), the leading part is dropped until the directory is found. Empty to give the export in the address instead. |
| `nfs_subdir` | | Directory under the export. Optional. |
| `nfs_version` | `3` | 3, or 4 for the newest of 4.2, 4.1 and 4.0 the server offers. |
| `nfs_port` | `0` | NFS service port; 0 asks the portmapper. |
| `nfs_mount_port` | `0` | MOUNT service port (NFSv3); 0 asks the portmapper. |
| `nfs_num_contexts` | `4` | Connections kept open, 1 to 16. |
| `nfs_timeout` | `5` | Seconds to wait for the server, 1 to 60. |
| `nfs_readahead` | `0` | Read-ahead in KiB, 0 (off) to 16384 (see below). |

The same settings are used by the `nfs` Cloud Sync driver. For cloud
sync, `nfs_export` must be set; synchronized files are stored in the
`cloud_sync` directory below `nfs_subdir`.

Content is addressed as `nfs://server/export/path/to/game.chd`. Besides
the desktop and mobile builds, the 3DS, Vita, Switch and Wii U carry the
NFS client; it needs no crypto library.

## Read-ahead

Read-ahead is off by default: each read a core makes is one request to
the server, which suits most content.

Some cores read a disc image in many small pieces, and over a slow or
high-latency link each piece costs a round trip, which shows as
stutter. With `smb_client_readahead` or `nfs_readahead` set, the client
fetches a window of that many KiB around each read, with several
requests in flight, and on a build with threads a background thread
keeps the next window coming for files opened read-only. Each open file
then holds that much memory, a second connection and a thread, so it is
a poor fit for content that opens many small files at once. Try 1024
for a large disc image that stutters; raise it for a slow link.

## Saved passwords: the keychain

Passwords, tokens and stream keys are not written to `retroarch.cfg`.
They go to `retroarch-keychain.cfg` beside it, each value sealed with
ChaCha20-Poly1305: RetroAchievements, WebDAV and S3 cloud sync, Google
Drive, the streaming services, SMB, netplay, and the kiosk and settings
passwords. Plaintext secrets found in an older `retroarch.cfg` move into
the keychain the next time the configuration is saved.

The key comes from two things: `retroarch-keychain.key`, a random key
file created beside the configuration, and the machine's identity (the
Windows MachineGuid, `/etc/machine-id` on Linux, the host UUID on
macOS). On platforms without a machine identity, the key file alone is
the key. So `retroarch-keychain.cfg` copied without its key file, or to
another machine, does not open.

### Moving a configuration to another machine

A keychain opens on the machine that made it. To take it along, set a
passphrase first, under **Settings > User > Keychain Passphrase**. The
passphrase is kept nowhere; it wraps the keychain's key inside
`retroarch-keychain.key`, next to the wrap for the current machine.
Nothing needs entering on this machine afterwards.

Copy `retroarch.cfg`, `retroarch-keychain.cfg` and
`retroarch-keychain.key` together. On the new machine the keychain comes
up locked:

- the log says so, and the saved passwords are not available;
- saving the configuration keeps them sealed as they are, and never
  writes a password in the clear. A password set while the keychain is
  locked is not saved; unlock first, then set it again.

Open **Settings > User > Keychain Passphrase** and enter the passphrase.
The keychain unlocks, the saved passwords apply at once without a
restart, and the keychain is wrapped for the new machine, so it opens by
itself from then on. The passphrase stays in the key file for the next
move.

To change the passphrase, enter a new one. To remove it, enter nothing.
A key file without a passphrase is the plain key file older versions of
RetroArch wrote and still read. Deriving the key from a passphrase takes
a moment, longer on a handheld; it runs in the background and a
notification reports the result.

If the key file is lost, the sealed values cannot be recovered: enter the
passwords again. RetroArch never deletes sealed values it cannot open,
so restoring the right key file later brings them back.

## Build options

The stack is on by default. `./configure` switches:

| Option | Effect |
| --- | --- |
| `--disable-crypto` | Leaves out the crypto library and everything built on it: the keychain, the TLS client and the SMB client. |
| `--disable-keychain` | Stores credentials in the clear in `retroarch-keychain.cfg`. |
| `--disable-retrossl` | Leaves out the built-in TLS client. |
| `--disable-retrosmb`, `--disable-retronfs` | Leave out the SMB or NFS client. |
| `--enable-mbedtls`, `--enable-bearssl` | Use a system mbedTLS or BearSSL for TLS in place of the built-in client. |
| `--enable-libsmb` | Use a system libsmb2 for SMB in place of the built-in client. |

The library options need the library installed and fail at configure time
if it is not found. The consoles with 24 or 32 MiB of RAM (GameCube, Wii,
PS2) build without the crypto library.

The Visual Studio projects build the whole stack, from Visual Studio 2005
up; Visual Studio 6 and .NET 2003 build without it. The CA bundle
(`libretro-common/net/cacert.h`) is kept in parts under 64 KiB, the
longest string literal MSVC before 2019 accepts; `tools/cacert_split.py`
regenerates it from a new `cacert.pem`.

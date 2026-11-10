"""Local TLS peer forwarding plaintext to the independent RTMP receiver."""
import asyncio
from pathlib import Path
import ssl
import subprocess
import threading


class TlsRelay:
    def __init__(self, root, target, version, capture_connect=False):
        self.root = Path(root)
        self.target = target
        self.version = version
        self.ready = threading.Event()
        self.error = None
        self.port = None
        self.ca = self.root / 'ca.pem'
        self.loop = None
        self.done = None
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.connect_bytes = bytearray() if capture_connect else None
        self.context = None
        if version is None:
            return
        key, cert = self.root / 'server.key', self.root / 'server.pem'
        ca_key, csr = self.root / 'ca.key', self.root / 'server.csr'
        extension = self.root / 'server.ext'
        extension.write_text('subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n')
        commands = [
            ['req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', ca_key,
             '-out', self.ca, '-days', '2', '-subj', '/CN=RTMPS test CA'],
            ['req', '-newkey', 'rsa:2048', '-nodes', '-keyout', key, '-out', csr,
             '-subj', '/CN=localhost'],
            ['x509', '-req', '-in', csr, '-CA', self.ca, '-CAkey', ca_key,
             '-CAcreateserial', '-out', cert, '-days', '2', '-extfile', extension],
        ]
        for command in commands:
            subprocess.run(['openssl'] + list(map(str, command)), check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=15)
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        protocol = {'1.2': ssl.TLSVersion.TLSv1_2, '1.3': ssl.TLSVersion.TLSv1_3}[version]
        self.context.minimum_version = self.context.maximum_version = protocol
        self.context.load_cert_chain(cert, key)

    async def forward(self, reader, writer):
        peer = None
        try:
            deadline = self.loop.time() + 5
            while True:
                try:
                    remote, peer = await asyncio.open_connection('127.0.0.1', self.target)
                    break
                except ConnectionRefusedError:
                    if self.loop.time() >= deadline:
                        raise
                    await asyncio.sleep(0.01)

            async def copy(source, destination, upload):
                forwarding = True
                while True:
                    data = await source.read(16384)
                    if not data:
                        if destination.can_write_eof():
                            destination.write_eof()
                        return
                    if upload and self.connect_bytes is not None:
                        self.connect_bytes.extend(data[:max(0, 16384 - len(self.connect_bytes))])
                    if not forwarding:
                        continue
                    try:
                        destination.write(data)
                        await destination.drain()
                    except ConnectionError:
                        if upload:
                            raise
                        # Drain final replies until the receiver closes.
                        forwarding = False

            await asyncio.wait_for(asyncio.gather(copy(reader, peer, True),
                                                  copy(remote, writer, False)), 20)
        except Exception as error:
            self.error = error
        finally:
            if peer is not None:
                peer.close()
                try:
                    await peer.wait_closed()
                except ConnectionError:
                    pass
            writer.close()
            try:
                await writer.wait_closed()
            except (ConnectionError, ssl.SSLError):
                pass
            self.done.set()

    async def serve(self):
        self.loop = asyncio.get_running_loop()
        self.done = asyncio.Event()
        options = dict(ssl=self.context, ssl_handshake_timeout=5,
                       ssl_shutdown_timeout=1) if self.context else {}
        async with await asyncio.start_server(self.forward, '127.0.0.1', 0, **options) as server:
            self.port = server.sockets[0].getsockname()[1]
            self.ready.set()
            await self.done.wait()

    def run(self):
        try:
            asyncio.run(self.serve())
        except Exception as error:
            self.error = error
            self.ready.set()

    def __enter__(self):
        self.thread.start()
        if not self.ready.wait(5) or self.error or not self.port:
            if self.loop and self.done and not self.loop.is_closed():
                self.loop.call_soon_threadsafe(self.done.set)
                self.thread.join(2)
            raise RuntimeError('TLS relay did not start') from self.error
        return self

    def __exit__(self, kind, value, traceback):
        self.thread.join(2)
        if self.thread.is_alive():
            self.loop.call_soon_threadsafe(self.done.set)
            self.thread.join(2)
            if kind is None:
                raise TimeoutError('TLS relay did not drain')
        if kind is None and self.error:
            raise RuntimeError('TLS relay failed') from self.error

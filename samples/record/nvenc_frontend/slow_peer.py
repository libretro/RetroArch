"""Bounded test relay and memory snapshots for this test's owned frontend."""
import ctypes
from ctypes import wintypes
import select
import socket
import threading


class SlowPeer:
    def __init__(self, target, listen_port=0, allow_reset=False):
        self.listener = socket.socket()
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        self.listener.bind(('127.0.0.1', listen_port))
        self.listener.listen(1)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.target = target
        self.pause = threading.Event()
        self.paused = threading.Event()
        self.stop = threading.Event()
        self.forwarded = 0
        self.error = None
        self.allow_reset = allow_reset
        self.thread = threading.Thread(target=self.run, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def run(self):
        try:
            front = None
            while not self.stop.is_set() and front is None:
                try:
                    front, _ = self.listener.accept()
                except socket.timeout:
                    pass
            if front is None:
                return
            with front, socket.create_connection(('127.0.0.1', self.target), timeout=5) as back:
                front.setblocking(False)
                back.setblocking(False)
                buffers = {front: bytearray(), back: bytearray()}
                other = {front: back, back: front}
                eof = set()
                while not self.stop.is_set():
                    paused = self.pause.is_set()
                    if paused:
                        self.paused.set()
                    else:
                        self.paused.clear()
                    reads = [s for s in (front, back) if s not in eof
                             and len(buffers[other[s]]) < 65536 and not (s is front and paused)]
                    writes = [s for s in (front, back) if buffers[s]]
                    ready, writable, _ = select.select(reads, writes, [], .02)
                    for source in ready:
                        data = source.recv(min(16384, 65536 - len(buffers[other[source]])))
                        if data:
                            buffers[other[source]].extend(data)
                        else:
                            eof.add(source)
                    for destination in writable:
                        count = destination.send(buffers[destination])
                        del buffers[destination][:count]
                        if destination is back:
                            self.forwarded += count
                    for source in tuple(eof):
                        if not buffers[other[source]]:
                            try:
                                other[source].shutdown(socket.SHUT_WR)
                            except OSError:
                                pass
                    if len(eof) == 2 and not any(buffers.values()):
                        return
        except Exception as error:
            if not self.stop.is_set() and not (
                    self.allow_reset and isinstance(error, ConnectionResetError)):
                self.error = error

    def close(self):
        self.stop.set()
        self.thread.join(6)
        self.listener.close()
        assert not self.thread.is_alive(), 'slow-peer relay did not retire'

    def __exit__(self, *unused):
        self.close()
        if not unused[0]:
            assert self.error is None, self.error


def frontend_handle(runner_pid, executable):
    class Entry(ctypes.Structure):
        _fields_ = [('size', wintypes.DWORD), ('usage', wintypes.DWORD),
                    ('pid', wintypes.DWORD), ('heap', ctypes.c_size_t),
                    ('module', wintypes.DWORD), ('threads', wintypes.DWORD),
                    ('parent', wintypes.DWORD), ('priority', wintypes.LONG),
                    ('flags', wintypes.DWORD), ('exe', wintypes.WCHAR * 260)]
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    api.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    api.Process32FirstW.argtypes = api.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(Entry)]
    api.CloseHandle.argtypes = [wintypes.HANDLE]
    api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    api.OpenProcess.restype = wintypes.HANDLE
    snapshot = api.CreateToolhelp32Snapshot(2, 0)
    assert snapshot and snapshot != ctypes.c_void_p(-1).value
    entry = Entry()
    entry.size = ctypes.sizeof(entry)
    found = []
    try:
        okay = api.Process32FirstW(snapshot, ctypes.byref(entry))
        while okay:
            if entry.parent == runner_pid and entry.exe.casefold() == executable.casefold():
                found.append(entry.pid)
            okay = api.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        api.CloseHandle(snapshot)
    assert len(found) == 1, ('owned frontend child', found)
    handle = api.OpenProcess(0x410, False, found[0])
    assert handle, ('OpenProcess', ctypes.get_last_error())
    return handle, lambda: api.CloseHandle(handle)


def memory(handle):
    class Counters(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('faults', wintypes.DWORD)] + [
            (name, ctypes.c_size_t) for name in ('peak_ws', 'working_set', 'peak_paged',
             'paged', 'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile', 'private')]
    api = ctypes.WinDLL('psapi', use_last_error=True).GetProcessMemoryInfo
    api.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    data = Counters()
    data.cb = ctypes.sizeof(data)
    assert api(handle, ctypes.byref(data), data.cb), ctypes.get_last_error()
    return dict(private=data.private, working_set=data.working_set)

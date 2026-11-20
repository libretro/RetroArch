#!/usr/bin/env python3
"""Optional real multicast interoperability test against python-zeroconf."""
import argparse
import socket
import subprocess
import threading
from pathlib import Path

from zeroconf import IPVersion, ServiceBrowser, ServiceInfo, Zeroconf

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--demo', type=Path, required=True)
parser.add_argument('--interface', default='127.0.0.1')
args = parser.parse_args()
service_type = '_pyrowave._tcp.local.'
zc = Zeroconf(interfaces=[args.interface], ip_version=IPVersion.V4Only)
added, removed = threading.Event(), threading.Event()


class Listener:
    def add_service(self, zeroconf, type_, name):
        if name != 'LibretroInterop.' + service_type:
            return
        info = zeroconf.get_service_info(type_, name)
        if (info and info.port == 32566 and socket.inet_aton(args.interface) in info.addresses
                and info.properties.get(b'version') == b'1'):
            added.set()

    def remove_service(self, zeroconf, type_, name):
        if name == 'LibretroInterop.' + service_type:
            removed.set()

    def update_service(self, zeroconf, type_, name):
        self.add_service(zeroconf, type_, name)


browser = ServiceBrowser(zc, service_type, Listener())
process = None
registered = None
try:
    process = subprocess.Popen([str(args.demo.resolve()), 'publish', 'LibretroInterop',
        args.interface, '32566', '5'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert added.wait(8), 'python-zeroconf did not resolve the C advertisement'
    output, _ = process.communicate(timeout=8)
    assert process.returncode == 0, output
    assert removed.wait(5), 'python-zeroconf did not observe the C goodbye'
    print('PASS: independent Python browser resolves C publisher and goodbye', flush=True)

    info = ServiceInfo(service_type, 'IndependentPython.' + service_type,
        addresses=[socket.inet_aton(args.interface)], port=32567,
        properties={b'version': b'1'}, server='mdns-independent-python.local.')
    zc.register_service(info)
    registered = info
    process = subprocess.Popen([str(args.demo.resolve()), 'browse', args.interface, '15'],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    c_added, c_removed = threading.Event(), threading.Event()
    lines = []

    def read_output():
        for line in process.stdout:
            lines.append(line)
            if line.startswith('ADD IndependentPython ') and ':32567 ' in line:
                c_added.set()
            if line.startswith('REMOVE IndependentPython '):
                c_removed.set()

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    assert c_added.wait(8), ''.join(lines) or 'C browser did not resolve the Python advertisement'
    zc.unregister_service(info)
    registered = None
    assert c_removed.wait(5), ''.join(lines) or 'C browser did not observe the Python goodbye'
    assert process.wait(timeout=16) == 0, ''.join(lines)
    reader.join(timeout=1)
    print('PASS: C browser resolves independent Python publisher and goodbye', flush=True)
finally:
    if process and process.poll() is None:
        process.kill()
        process.wait()
    if registered:
        zc.unregister_service(registered)
    browser.cancel()
    zc.close()

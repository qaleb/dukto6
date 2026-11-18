#!/usr/bin/env python3
"""Receive a real cross-app Android share; choose the displayed IP recipient.

Requires the disposable emulator, the Dukto debug APK, and tests/android-storage.
The helper instrumentation grants 1..1000 content URIs from another UID. In Dukto
choose IP connection, enter 10.0.2.2:PORT and press Send shared content. This test
verifies every file byte and observes the app's descriptor count while sending.
"""
import argparse
import json
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('--serial', required=True)
parser.add_argument('--adb', default='adb')
parser.add_argument('--count', type=int, default=500)
parser.add_argument('--port', type=int, default=14644)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if not args.serial.startswith('emulator-'):
    raise SystemExit('Use only a disposable emulator.')
package = 'org.qtproject.example.dukto6'
def adb(*command):
    return subprocess.check_output([args.adb, '-s', args.serial, *command], text=True, timeout=30).strip()

def descriptor_count(pid):
    return len(adb('shell', 'run-as', package, 'ls', '/proc/' + pid + '/fd').splitlines())

with socket.socket() as listener:
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(('127.0.0.1', args.port)); listener.listen(1); listener.settimeout(180)
    sender = subprocess.Popen([args.adb, '-s', args.serial, 'shell', 'am', 'instrument', '-w',
                               '-e', 'count', str(args.count), '-e', 'hold', 'true',
                               'dev.dukto.storagecheck/.ShareChecks'], stdout=subprocess.PIPE, text=True)
    while True:
        line = sender.stdout.readline()
        if not line: raise AssertionError('Share sender did not stage the files')
        if 'failure=' in line: raise AssertionError(line)
        if 'INSTRUMENTATION_STATUS_CODE: 1' in line: break
    pid = adb('shell', 'pidof', package)
    baseline = descriptor_count(pid)
    samples = [baseline]
    stopped = threading.Event()
    def monitor():
        while not stopped.is_set():
            try:
                samples.append(descriptor_count(pid))
            except subprocess.SubprocessError:
                break
            stopped.wait(0.08)
    thread = threading.Thread(target=monitor, daemon=True); thread.start()
    print('Share staged. Choose IP 10.0.2.2:%d and Send shared content.' % args.port, flush=True)
    expected = b'Dukto shared provider bytes\n'
    try:
        connection, _ = listener.accept()
        with connection:
            connection.settimeout(30)
            stream = connection.makefile('rb')
            def read(count):
                value = stream.read(count)
                if len(value) != count:
                    raise AssertionError('Truncated transfer')
                return value
            count, total = struct.unpack('<qq', read(16))
            assert count == args.count and total == count * len(expected), (count, total)
            names = set()
            for _ in range(count):
                name = bytearray()
                while True:
                    ch = read(1)
                    if ch == b'\0': break
                    name += ch
                size, = struct.unpack('<q', read(8))
                assert size == len(expected)
                assert read(size) == expected
                names.add(name.decode('utf-8'))
            assert names == {'shared-%d.txt' % i for i in range(args.count)}
            assert stream.read(1) == b''
    finally:
        stopped.set(); thread.join(timeout=5)
        adb('shell', 'run-as', 'dev.dukto.storagecheck', 'touch', 'cache/share-check-finish')
        sender.wait(timeout=10)
    assert adb('shell', 'pidof', package) == pid
    peak = max(samples)
    assert peak - baseline < 128, (baseline, peak)
    report = {'files': args.count, 'bytes_verified': True, 'cross_app_uri_grants': True,
              'baseline_descriptors': baseline, 'peak_descriptors': peak, 'samples': len(samples)}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))

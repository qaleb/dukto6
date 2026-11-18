#!/usr/bin/env python3
"""Regression check for the short-transfer foreground-service crash.

Use only a disposable, foregrounded emulator with confirmation disabled.
Forward --port to device TCP 4644 before running. This deliberately waits past
Android's foreground-service deadline after each burst, rather than trusting
that a successful socket write means the app remained alive.
"""
import argparse
import json
from pathlib import Path
import socket
import struct
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('--serial', required=True)
parser.add_argument('--adb', default='adb')
parser.add_argument('--port', type=int, default=15644)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if not args.serial.startswith('emulator-'):
    raise SystemExit('Use a disposable emulator, not a personal device.')
package = 'org.qtproject.example.dukto6'

def adb(*command):
    return subprocess.check_output([args.adb, '-s', args.serial, *command], text=True, timeout=30).strip()

pid = adb('shell', 'pidof', package)
if not pid:
    raise SystemExit('Foreground Dukto and disable receive confirmation first.')
adb('logcat', '-c')
messages = ['Short transfer %d' % i for i in range(20)] + ['', 'Unicode: café 世界 👋', 'Large text\n' * 10000]
for index, text in enumerate(messages):
    content = text.encode('utf-8')
    packet = struct.pack('<qq', 1, len(content)) + b'___DUKTO___TEXT___\0' + struct.pack('<q', len(content)) + content
    with socket.create_connection(('127.0.0.1', args.port), timeout=10) as connection:
        connection.sendall(packet)
    time.sleep(0.3)
    if adb('shell', 'pidof', package) != pid:
        raise AssertionError('Android app restarted during message %d' % index)
for _ in range(15):
    time.sleep(1)
    if adb('shell', 'pidof', package) != pid:
        raise AssertionError('Android app exited after a completed text transfer')
crashes = adb('logcat', '-d', '-b', 'crash')
if 'FATAL EXCEPTION' in crashes or 'Fatal signal' in crashes:
    raise AssertionError(crashes)
services = adb('shell', 'dumpsys', 'activity', 'services', package)
if 'ServiceRecord{' in services and 'DuktoTransferService' in services:
    raise AssertionError('Transfer service remained active after all text transfers finished')
result = {'messages': len(messages), 'same_process': True, 'wait_after_burst_seconds': 15,
          'crash_log': crashes, 'service_stopped': True}
args.output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))

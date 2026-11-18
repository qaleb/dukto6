#!/usr/bin/env python3
"""Run only against an explicitly selected disposable Android emulator.

Install the debug APK and launch Dukto first. Requires an adb forward from --port
to device TCP 4644. This sends legacy Dukto packets and verifies shared files.
"""
import argparse
import hashlib
import json
import shlex
import socket
import struct
import subprocess
import time
import uuid
import zlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--adb", required=True)
parser.add_argument("--serial", required=True)
parser.add_argument("--port", type=int, default=15644)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not args.serial.startswith("emulator-"):
    raise SystemExit("This test is restricted to disposable emulators.")

def adb(*command):
    return subprocess.check_output([args.adb, "-s", args.serial, *command], text=True, timeout=30).strip()

def shell(command):
    return adb("shell", command)

def active_locks():
    return shell("dumpsys power").partition("Wake Locks:")[2].partition("Suspend Blockers:")[0]

def wait(check, description, timeout=20):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = check()
        if value:
            return value
        time.sleep(0.2)
    raise AssertionError(description)

def connection():
    return socket.create_connection(("127.0.0.1", args.port), timeout=10)

def send(entries, directory=None):
    total = sum(len(data) for _, data in entries if data is not None)
    with connection() as stream:
        stream.sendall(struct.pack("=qq", len(entries), total))
        for name, data in entries:
            stream.sendall(name.encode() + b"\0" + struct.pack("=q", -1 if data is None else len(data)))
            if data:
                stream.sendall(data)
        # adb forwarding closes the device-side socket on a host half-close;
        # let it deliver the payload before sending EOF (it is not a wire ACK).
        last = next(name for name, data in reversed(entries) if data is not None)
        if directory is None and "/" in last:
            directory = "Download/Dukto/" + last.rsplit("/", 1)[0] + "/"
        wait(lambda: "Row:" in row("file", last.rsplit("/", 1)[-1], directory=directory),
             "Last received file was not published")
        stream.shutdown(socket.SHUT_WR)
    wait(lambda: "isForeground=true" not in shell("dumpsys activity services org.qtproject.example.dukto6"),
         "Previous receive session did not stop")

def row(collection, name, pending=0, directory=None):
    where = "_display_name='" + name + "' AND is_pending=" + str(pending)
    if directory:
        where += " AND relative_path='" + directory + "'"
    uri = "content://media/external_primary/" + collection + ("?includePending=1" if pending else "")
    return shell("content query --uri " + shlex.quote(uri)
                 + " --projection _display_name:relative_path:is_pending:_size --where " + shlex.quote(where))

def verify(path, content):
    expected = hashlib.sha256(content).hexdigest()
    actual = wait(lambda: (text if (text := shell("sha256sum " + shlex.quote(path))).split()[0] == expected else None),
                  "Received file is missing or its checksum differs")
    assert actual.split()[0] == expected, actual

def png():
    def chunk(kind, data):
        return struct.pack("!I", len(data)) + kind + data + struct.pack("!I", zlib.crc32(kind + data))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack("!IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(b"\0\x20\xa0\x40")) + chunk(b"IEND", b"")

prefix = "dukto-native-" + uuid.uuid4().hex[:12]
# am start -W reports the Android frame before Qt has fully resumed its event loop.
time.sleep(2)
results = {}
image = png()
photo = prefix + ".png"
send([(photo, image)])
results["image"] = wait(lambda: (text if "Row:" in (text := row("images/media", photo)) else None), "Photo was not published in Images")
assert "relative_path=Pictures/Dukto/" in results["image"]
verify("/sdcard/Pictures/Dukto/" + photo, image)

document = prefix + ".pdf"
pdf = b"%PDF-1.4\n% Dukto transfer fixture\n%%EOF\n"
send([(document, pdf)])
results["document"] = wait(lambda: (text if "Row:" in (text := row("file", document)) else None), "Document was not published")
assert "relative_path=Documents/Dukto/" in results["document"]
verify("/sdcard/Documents/Dukto/" + document, pdf)

# Establish an active transfer and foreground service before turning the screen off.
binary = prefix + ".bin"
block = bytes(range(256)) * 256
payload = block * 128
with connection() as stream:
    stream.sendall(struct.pack("=qq", 1, len(payload)) + binary.encode() + b"\0" + struct.pack("=q", len(payload)))
    stream.sendall(block)
    service = wait(lambda: (text if "isForeground=true" in (text := shell("dumpsys activity services org.qtproject.example.dukto6")) else None), "Transfer service did not start")
    results["service"] = [line.strip() for line in service.splitlines() if "isForeground" in line or "foregroundServiceType" in line]
    locks = active_locks()
    assert "Dukto:Transfer" in locks, "Transfer has no CPU wake lock"
    shell("input keyevent 223")
    results["screen_off"] = wait(lambda: (text if "mWakefulness=Asleep" in (text := shell("dumpsys power")) else None), "Screen did not sleep")
    results["screen_off"] = [line.strip() for line in results["screen_off"].splitlines() if "mWakefulness=" in line]
    for offset in range(len(block), len(payload), len(block)):
        stream.sendall(payload[offset:offset + len(block)])
        time.sleep(0.01)
    wait(lambda: "Row:" in row("downloads", binary), "Screen-off transfer was not published")
    stream.shutdown(socket.SHUT_WR)
results["sleep_transfer"] = wait(lambda: (text if "Row:" in (text := row("downloads", binary)) else None), "Screen-off transfer did not complete")
verify("/sdcard/Download/Dukto/" + binary, payload)
wait(lambda: "isForeground=true" not in shell("dumpsys activity services org.qtproject.example.dukto6"), "Service did not stop after the transfer")
wait(lambda: "Dukto:Transfer" not in active_locks(), "Wake lock leaked after the transfer")

shell("input keyevent 224")
shell("wm dismiss-keyguard")
time.sleep(1)
partial = prefix + "-partial.png"
with connection() as stream:
    stream.sendall(struct.pack("=qq", 1, 1000) + partial.encode() + b"\0" + struct.pack("=q", 1000) + b"partial")
    wait(lambda: "Row:" in row("images/media", partial, pending=1), "Partial photo was not staged")
    stream.shutdown(socket.SHUT_WR)
time.sleep(1)
assert "Row:" not in row("images/media", partial), "An interrupted photo appeared in Images"
assert shell("test ! -f " + shlex.quote("/sdcard/Pictures/Dukto/" + partial) + "; echo $?") == "0"
results["partial_cleanup"] = True

folder = prefix + "-folder"
html = b"<img src='assets/photo.png'>"
send([(folder, None), (folder + "/empty", None), (folder + "/assets", None),
      (folder + "/index.html", html), (folder + "/assets/photo.png", image)])
wait(lambda: shell("test -f " + shlex.quote("/sdcard/Download/Dukto/" + folder + "/index.html") + "; echo $?") == "0", "Folder transfer failed")
verify("/sdcard/Download/Dukto/" + folder + "/index.html", html)
verify("/sdcard/Download/Dukto/" + folder + "/assets/photo.png", image)
assert shell("test -d " + shlex.quote("/sdcard/Download/Dukto/" + folder + "/empty") + "; echo $?") == "0"
results["folder_structure"] = True
second_html = b"<title>Second copy</title><img src='assets/photo.png'>"
send([(folder, None), (folder + "/assets", None),
      (folder + "/index.html", second_html), (folder + "/assets/photo.png", image)],
     directory="Download/Dukto/" + folder + " (2)/assets/")
verify("/sdcard/Download/Dukto/" + folder + " (2)/index.html", second_html)
verify("/sdcard/Download/Dukto/" + folder + " (2)/assets/photo.png", image)
verify("/sdcard/Download/Dukto/" + folder + "/index.html", html)
results["duplicate_folder_preserved"] = True
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(results, indent=2))
print("Native Android receive, publication, screen-off transfer, cleanup and folder checks passed")

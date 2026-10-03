"""Transfer this installed test app and collect logs using Vita Companion/FTP."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP, error_perm
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import time

PROJECT = Path(__file__).resolve().parents[1]
TITLE_ID = "SCRB00001"


def timestamp():
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def connection(host):
    ftp = FTP()
    ftp.connect(host, 1337, timeout=10)
    ftp.login()
    return ftp


def command(host, message):
    # Installed Companion accepts these older plain-text commands.
    with socket.create_connection((host, 1338), timeout=5) as sock:
        sock.sendall((message + "\n").encode("ascii"))
        sock.settimeout(2)
        try:
            reply = sock.recv(8192).decode("utf-8", errors="replace").strip()
        except socket.timeout:
            reply = "No response; command delivery does not confirm execution."
    print(f"Companion {message}: {reply}", flush=True)
    if "unknown command" in reply.lower():
        raise RuntimeError(reply)


def fetch(ftp, remote, local):
    with local.open("wb") as stream:
        ftp.retrbinary("RETR " + remote, stream.write)


def remote_hash(ftp, remote):
    digest = hashlib.sha256()
    ftp.retrbinary("RETR " + remote, digest.update)
    return digest.hexdigest()


def logs(host):
    output = PROJECT / "analysis/hardware" / timestamp()
    output.mkdir(parents=True)
    with connection(host) as ftp:
        for remote, name in (("ux0:/data/scribblenauts/loader.log", "loader.log"),
                             ("ux0:/data/vitaGL.log", "vitaGL.log")):
            local = output / name
            try:
                fetch(ftp, remote, local)
            except error_perm as exc:
                if not str(exc).startswith("550"):
                    raise
                local.unlink(missing_ok=True)
                print(f"Log unavailable: {remote}", flush=True)
                continue
            print(f"Saved {local}\n{local.read_text(errors='replace')[-6000:]}", flush=True)


def deploy(host):
    executable = PROJECT / "build/eboot.bin"
    payload = executable.read_bytes()
    if payload[:4] != b"SCE\0":
        raise ValueError("Build eboot.bin is not a Vita SELF")
    expected = hashlib.sha256(payload).hexdigest()
    output = PROJECT / "analysis/hardware" / (timestamp() + "-deploy")
    output.mkdir(parents=True)
    shutil.copyfile(PROJECT / "build/scribblenauts_vita", output / "loader.elf")
    remote = f"ux0:/app/{TITLE_ID}/eboot.bin"
    temporary = remote + ".upload"
    # Preserve the installed executable before changing application state.
    with connection(host) as ftp:
        fetch(ftp, remote, output / "eboot-before.bin")
    command(host, "destroy")
    with connection(host) as ftp:
        # Companion can acknowledge destroy before the app mount is writable.
        # Retry only the temporary upload; leave the installed file intact.
        for attempt in range(3):
            try:
                with executable.open("rb") as stream:
                    ftp.storbinary("STOR " + temporary, stream)
                break
            except error_perm as exc:
                if not str(exc).startswith("550") or attempt == 2:
                    raise
                time.sleep(2)
        if remote_hash(ftp, temporary) != expected:
            raise RuntimeError("Uploaded executable hash does not match; installed file retained")
        backup = remote + ".previous-" + timestamp()
        ftp.rename(remote, backup)
        try:
            ftp.rename(temporary, remote)
        except Exception:
            ftp.rename(backup, remote)
            raise
        if remote_hash(ftp, remote) != expected:
            raise RuntimeError("Installed executable readback does not match")
    record = {"host": host, "title_id": TITLE_ID, "sha256": expected,
              "remote_backup": backup, "installed": remote}
    (output / "deployment.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"Installed and verified {remote}: {expected}", flush=True)
    command(host, "launch " + TITLE_ID)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("logs", "deploy", "launch", "stop"))
    config = PROJECT / "analysis/vita-connection.json"
    default_host = json.loads(config.read_text())["host"] if config.is_file() else os.environ.get("PSVITAIP")
    parser.add_argument("--host", default=default_host, required=not default_host)
    args = parser.parse_args()
    if args.action == "logs":
        logs(args.host)
    elif args.action == "deploy":
        deploy(args.host)
    else:
        command(args.host, "launch " + TITLE_ID if args.action == "launch" else "destroy")

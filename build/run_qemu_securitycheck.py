from __future__ import annotations

import shutil
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IMAGE = ROOT / "lunaos.img"
SECURITY_IMAGE = ROOT / "lunaos_securitycheck.img"
LOG_PATH = ROOT / "qemu_securitycheck.log"
ERR_PATH = ROOT / "qemu_securitycheck.err.log"


def find_qemu() -> str:
    found = shutil.which("qemu-system-x86_64")
    if found:
        return found
    for candidate in (
        r"C:\Program Files\qemu\qemu-system-x86_64.exe",
        r"C:\Program Files\QEMU\qemu-system-x86_64.exe",
    ):
        if Path(candidate).exists():
            return candidate
    raise FileNotFoundError("qemu-system-x86_64 not found. Install QEMU or add it to PATH.")


def main() -> int:
    qemu = find_qemu()
    if not IMAGE.exists():
        raise FileNotFoundError(f"missing built image: {IMAGE}")
    shutil.copyfile(IMAGE, SECURITY_IMAGE)

    for path in (LOG_PATH, ERR_PATH):
        if path.exists():
            path.unlink()

    required = [
        "audit crypto.secret denied reason=scope",
        "[SECURITY] crypto.scope negative ok",
        "audit cap.issue denied reason=user-auth-caller",
        "[SECURITY] crypto.authcap negative ok",
        "audit crypto.secret denied reason=cap",
        "[SECURITY] crypto.target negative ok",
        "audit cap.revoke denied reason=holder",
        "[SECURITY] cap.revoke holder negative ok",
        "audit cap.issue denied reason=high-risk-caller",
        "[SECURITY] cap.highrisk negative ok",
        "audit govern denied reason=security-owned-secret",
        "[SECURITY] secret.protect negative ok",
        "audit network.policy denied reason=holder",
        "[SECURITY] network.caller negative ok",
        "audit network.policy denied reason=revoked",
        "[SECURITY] network.revoked negative ok",
        "audit network.policy denied reason=offline",
        "[SECURITY] network.offline negative ok",
        "audit network.policy denied reason=unauthorized-injection",
        "[SECURITY] network.inject negative ok",
        "audit device.policy denied reason=holder",
        "[SECURITY] device.caller negative ok",
        "audit device.policy denied reason=revoked",
        "[SECURITY] device.revoked negative ok",
        "audit device.policy denied reason=recovery",
        "[SECURITY] device.recovery negative ok",
        "audit device.policy denied reason=actor",
        "[SECURITY] device.actor negative ok",
        "[SECURITY] ready",
        "[USER] shell ready",
    ]
    forbidden = [
        "[SECURITY] crypto.scope negative fail",
        "[SECURITY] crypto.authcap negative fail",
        "[SECURITY] crypto.target negative fail",
        "[SECURITY] cap.revoke holder negative fail",
        "[SECURITY] cap.highrisk negative fail",
        "[SECURITY] secret.protect negative fail",
        "[SECURITY] network.caller negative fail",
        "[SECURITY] network.revoked negative fail",
        "[SECURITY] network.offline negative fail",
        "[SECURITY] network.inject negative fail",
        "[SECURITY] device.caller negative fail",
        "[SECURITY] device.revoked negative fail",
        "[SECURITY] device.recovery negative fail",
        "[SECURITY] device.actor negative fail",
    ]

    with LOG_PATH.open("w", encoding="utf-8", errors="replace") as stdout_file, ERR_PATH.open(
        "w", encoding="utf-8", errors="replace"
    ) as stderr_file:
        proc = subprocess.Popen(
            [
                qemu,
                "-drive",
                f"format=raw,file={SECURITY_IMAGE}",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
            ],
            stdout=stdout_file,
            stderr=stderr_file,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

        try:
            deadline = time.time() + 35.0
            while time.time() < deadline:
                if LOG_PATH.exists():
                    text = LOG_PATH.read_text(encoding="utf-8", errors="replace")
                    if all(needle in text for needle in required) or any(
                        needle in text for needle in forbidden
                    ):
                        break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=5)
        finally:
            if proc.poll() is None:
                proc.kill()

    stdout = LOG_PATH.read_text(encoding="utf-8", errors="replace")
    stderr = ERR_PATH.read_text(encoding="utf-8", errors="replace")
    missing = [needle for needle in required if needle not in stdout]
    failures = [needle for needle in forbidden if needle in stdout]
    if missing or failures:
        if stdout:
            print(stdout)
        if stderr:
            print(stderr)
        if missing:
            print(f"securitycheck missing required markers: {missing}")
        if failures:
            print(f"securitycheck saw forbidden markers: {failures}")
        return 1

    print("securitycheck passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

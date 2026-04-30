from __future__ import annotations

import sys
from pathlib import Path

from run_qemu_updateapplycheck import (
    IMAGE,
    ROOT,
    pack_devloop_bundle,
    patch_session_script,
    require_absent,
    require_all,
    run_vm,
)


CHECK_IMAGE = ROOT / "lunaos_updaterollbackcheck.img"
LOG1_PATH = ROOT / "qemu_updaterollbackcheck_boot1.log"
ERR1_PATH = ROOT / "qemu_updaterollbackcheck_boot1.err.log"
LOG2_PATH = ROOT / "qemu_updaterollbackcheck_boot2.log"
ERR2_PATH = ROOT / "qemu_updaterollbackcheck_boot2.err.log"
LOG3_PATH = ROOT / "qemu_updaterollbackcheck_boot3.log"
ERR3_PATH = ROOT / "qemu_updaterollbackcheck_boot3.err.log"
LOG4_PATH = ROOT / "qemu_updaterollbackcheck_boot4.log"
ERR4_PATH = ROOT / "qemu_updaterollbackcheck_boot4.err.log"
LOG5_PATH = ROOT / "qemu_updaterollbackcheck_boot5.log"
ERR5_PATH = ROOT / "qemu_updaterollbackcheck_boot5.err.log"
LOG6_PATH = ROOT / "qemu_updaterollbackcheck_boot6.log"
ERR6_PATH = ROOT / "qemu_updaterollbackcheck_boot6.err.log"
LOG7_PATH = ROOT / "qemu_updaterollbackcheck_boot7.log"
ERR7_PATH = ROOT / "qemu_updaterollbackcheck_boot7.err.log"


def main() -> int:
    bundle = pack_devloop_bundle()
    CHECK_IMAGE.write_bytes(IMAGE.read_bytes())

    patch_session_script(
        CHECK_IMAGE,
        b"setup.init luna dev secret\r\n"
        b"package.install sample\r\n"
        b"update.status\r\n",
        bundle,
    )
    boot1 = run_vm(
        CHECK_IMAGE,
        LOG1_PATH,
        ERR1_PATH,
        ["package.install ok", "update action=apply-ready"],
        30.0,
    )

    patch_session_script(
        CHECK_IMAGE,
        b"login dev secret\r\n"
        b"update.status\r\n"
        b"update.apply\r\n",
    )
    boot2 = run_vm(CHECK_IMAGE, LOG2_PATH, ERR2_PATH, ["update.apply ok"], 50.0)

    patch_session_script(CHECK_IMAGE, b"")
    boot3 = run_vm(
        CHECK_IMAGE,
        LOG3_PATH,
        ERR3_PATH,
        ["audit update.apply activation=LSYS", "audit update.apply persisted=DATA authority=UPDATE"],
        25.0,
    )

    patch_session_script(
        CHECK_IMAGE,
        b"login dev secret\r\n"
        b"update.status\r\n"
        b"list-apps\r\n"
        b"run sample\r\n",
    )
    boot4 = run_vm(
        CHECK_IMAGE,
        LOG4_PATH,
        ERR4_PATH,
        ["update action=applied", "launch request: sample.luna"],
        40.0,
    )

    patch_session_script(
        CHECK_IMAGE,
        b"login dev secret\r\n"
        b"update.rollback\r\n"
        b"update.status\r\n"
        b"list-apps\r\n"
        b"run sample\r\n",
    )
    boot5 = run_vm(
        CHECK_IMAGE,
        LOG5_PATH,
        ERR5_PATH,
        ["update.rollback ok", "update state=rolled_back", "launch request: sample.luna"],
        45.0,
    )

    patch_session_script(
        CHECK_IMAGE,
        b"login dev secret\r\n"
        b"update.status\r\n"
        b"list-apps\r\n"
        b"run sample\r\n",
    )
    boot6 = run_vm(
        CHECK_IMAGE,
        LOG6_PATH,
        ERR6_PATH,
        ["update state=rolled_back", "launch request: sample.luna"],
        40.0,
    )

    patch_session_script(
        CHECK_IMAGE,
        b"login dev secret\r\n"
        b"update.rollback\r\n"
        b"update.status\r\n"
        b"list-apps\r\n"
        b"run sample\r\n",
    )
    boot7 = run_vm(
        CHECK_IMAGE,
        LOG7_PATH,
        ERR7_PATH,
        ["audit recovery.denied reason=rollback", "update.rollback fail", "launch request: sample.luna"],
        45.0,
    )

    require_all(
        boot1,
        "updaterollback boot1",
        (
            "package.install ok",
            "update mode=run action=apply-ready",
            "update state=staged current=1 target=2",
        ),
    )
    require_all(
        boot2,
        "updaterollback boot2",
        (
            "audit update.apply activation=COMMITTED",
            "update.apply ok",
            "update.result state=committed current=1 target=2",
        ),
    )
    require_all(
        boot3,
        "updaterollback boot3",
        (
            "audit update.apply activation=LSYS",
            "audit update.apply persisted=DATA authority=UPDATE",
        ),
    )
    require_absent(
        boot3,
        "updaterollback boot3",
        (
            "[USER] shell ready",
            "login ok: session active",
        ),
    )
    require_all(
        boot4,
        "updaterollback boot4",
        (
            "update state=active current=2 target=2",
            "update action=applied",
            "sample.luna",
            "launch request: sample.luna",
        ),
    )
    require_all(
        boot5,
        "updaterollback boot5",
        (
            "audit recovery.persisted=DATA authority=UPDATE",
            "update.rollback ok",
            "update.result state=rolled_back current=1 target=2",
            "update state=rolled_back current=1 target=2",
            "sample.luna",
            "launch request: sample.luna",
        ),
    )
    require_absent(
        boot5,
        "updaterollback boot5",
        (
            "audit recovery.denied reason=rollback",
            "update.rollback fail",
            "update.apply ok",
            "update.result state=committed",
            "update state=active current=2 target=2",
            "launch failed",
        ),
    )
    require_all(
        boot6,
        "updaterollback boot6",
        (
            "update state=rolled_back current=1 target=2",
            "sample.luna",
            "launch request: sample.luna",
        ),
    )
    require_absent(
        boot6,
        "updaterollback boot6",
        (
            "update state=active current=2 target=2",
            "launch failed",
        ),
    )
    require_all(
        boot7,
        "updaterollback boot7",
        (
            "audit recovery.denied reason=rollback",
            "update.rollback fail",
            "update.result=unavailable",
            "update state=rolled_back current=1 target=2",
            "sample.luna",
            "launch request: sample.luna",
        ),
    )
    require_absent(
        boot7,
        "updaterollback boot7",
        (
            "audit recovery.persisted=DATA authority=UPDATE",
            "update.rollback ok",
            "update.result state=rolled_back",
            "update.apply ok",
            "update.result state=committed",
            "update state=active current=2 target=2",
            "launch failed",
        ),
    )

    sys.stdout.write(boot1)
    sys.stdout.write("\n--- APPLY COMMIT ---\n")
    sys.stdout.write(boot2)
    sys.stdout.write("\n--- ACTIVATION REBOOT ---\n")
    sys.stdout.write(boot3)
    sys.stdout.write("\n--- ACTIVE CONFIRM ---\n")
    sys.stdout.write(boot4)
    sys.stdout.write("\n--- ROLLBACK ---\n")
    sys.stdout.write(boot5)
    sys.stdout.write("\n--- ROLLBACK CONFIRM ---\n")
    sys.stdout.write(boot6)
    sys.stdout.write("\n--- ROLLBACK DENIED ---\n")
    sys.stdout.write(boot7)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""
NoobOS 자동 스모크 테스트 (`make test`).

QEMU 를 화면 없이 띄우고 COM1(-serial stdio)으로 셸 명령을 보낸 뒤, 프롬프트가 다시 나올 때까지
출력을 모아 기대 문자열이 있는지 확인한다. 커널 내 selftest(lib/user/kmalloc)와 주요 유저 프로그램,
ATA 디스크 왕복을 한 번에 돌린다. DEBUG 빌드(selftest 명령 포함)가 필요하다.

usage: smoke_test.py <kernel.elf> <initrd.tar>
"""
import os
import subprocess
import sys
import tempfile
import threading
import time

PROMPT = "noob> "


class Qemu:
    def __init__(self, kernel, initrd, disk):
        self.proc = subprocess.Popen(
            ["qemu-system-i386", "-kernel", kernel, "-initrd", initrd, "-no-reboot",
             "-display", "none", "-monitor", "none", "-serial", "stdio",
             "-drive", f"file={disk},format=raw,if=ide,index=0"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.buf = ""
        self.lock = threading.Lock()
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        for chunk in iter(lambda: self.proc.stdout.read(1), b""):
            with self.lock:
                self.buf += chunk.decode(errors="replace").replace("\r", "")

    def wait_for(self, text, start, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                idx = self.buf.find(text, start)
                if idx >= 0:
                    return self.buf[start:idx]
            time.sleep(0.05)
        with self.lock:
            raise TimeoutError(f"timed out waiting for {text!r}; got:\n{self.buf[start:]}")

    def run(self, command, timeout=10):
        with self.lock:
            start = len(self.buf)
        for ch in command:
            self.proc.stdin.write(ch.encode())
            self.proc.stdin.flush()
            time.sleep(0.002)
        self.proc.stdin.write(b"\r")
        self.proc.stdin.flush()
        # 명령 줄의 에코 다음부터 다음 프롬프트까지가 출력
        echoed = self.wait_for("\n", start, timeout) + "\n"
        out = self.wait_for(PROMPT, start + len(echoed), timeout)
        return out

    def close(self):
        self.proc.kill()
        self.proc.wait()


# (명령, 출력에 있어야 할 문자열들, 없어야 할 문자열들, 타임아웃)
CASES = [
    ("selftest lib", ["lib selftest: ALL PASS"], ["FAIL"], 10),
    ("selftest user", ["=== result: ALL PASS ==="], ["FAIL"], 20),
    ("selftest kmalloc", ["=== result: ALL PASS ==="], ["FAIL"], 60),
    ("hello a b", ['argv[2] = "b"'], [], 10),
    ("cat /etc/motd", ["Welcome to NoobOS."], [], 10),
    ("/bin/cat /etc/motd /missing", ["Welcome to NoobOS.", "/missing: no such file", "exit code 1"], [], 10),
    ("ls /bin", ["/bin/counter", "/bin/spawner"], [], 10),
    ("spawner", ["exited 3", "exited 2"], [], 15),
    ("fault null", ["Page Fault at 0x0", "terminated by a fault"], [], 10),
    ("fault cli", ["General Protection Fault", "terminated by a fault"], [], 10),
    ("fault div", ["Divide by Zero", "terminated by a fault"], [], 10),
    ("fault stack 600", ["survived"], ["terminated"], 15),
    ("fault stack 2000", ["terminated by a fault"], ["survived"], 15),
    ("nosuchprogram", ["command not found"], [], 10),
    ("disk write 3 smoke-test-marker", ["wrote sector 3"], [], 10),
    ("disk read 3", ["smoke-test-marker"[:16]], ["failed"], 10),
    ("ps", ["kernel_main", "idle", "shell"], [], 10),
    ("date", ["UTC"], [], 10),
]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    kernel, initrd = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as disk:
        disk.truncate(1024 * 1024)
        disk_path = disk.name

    qemu = Qemu(kernel, initrd, disk_path)
    failures = 0
    try:
        qemu.wait_for(PROMPT, 0, 15)
        for command, expect, reject, timeout in CASES:
            try:
                out = qemu.run(command, timeout)
                missing = [e for e in expect if e not in out]
                found = [r for r in reject if r in out]
                ok = not missing and not found
            except TimeoutError as e:
                ok, out, missing, found = False, str(e), [], []
            print(f"[{'PASS' if ok else 'FAIL'}] {command}")
            if not ok:
                failures += 1
                if missing:
                    print(f"       missing: {missing}")
                if found:
                    print(f"       unexpected: {found}")
                print("       " + out.strip().replace("\n", "\n       "))
        with qemu.lock:
            if "KERNEL PANIC" in qemu.buf:
                print("[FAIL] kernel panicked during the run")
                failures += 1
    finally:
        qemu.close()
        os.unlink(disk_path)

    print(f"\n{len(CASES) - failures}/{len(CASES)} passed" if failures == 0 else f"\n{failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

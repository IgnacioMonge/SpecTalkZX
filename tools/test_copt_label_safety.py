#!/usr/bin/env python3
"""Compile a control-flow probe that unsafe multipass copt rules used to break."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROBE = r"""
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
extern uint8_t esx_handle;
extern uint16_t esx_count;
extern uint16_t esx_result;
extern void esx_freplace(const char *) __z88dk_fastcall;
extern void esx_fwrite(void);
extern uint8_t esx_fclose(void);
extern void esx_commit(const char *) __z88dk_fastcall;
extern void esx_funlink(const char *) __z88dk_fastcall;

uint8_t copt_label_probe(const char *path) __z88dk_fastcall
{
    uint8_t created;
    uint8_t ok = 0;
    esx_freplace(path);
    if (esx_handle) {
        created = (esx_result == 0);
        esx_fwrite();
        ok = (esx_result == esx_count);
        if (esx_fclose() != 0) ok = 0;
        if (ok && created) {
            esx_commit(path);
            ok = (esx_result != 0);
        }
        if (!ok && created) esx_funlink(path);
    }
    return ok;
}
"""


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="spectalk-copt-") as directory:
        source = Path(directory) / "probe.c"
        output = Path(directory) / "probe.o"
        source.write_text(PROBE, encoding="ascii")
        command = [
            "zcc", "+zx", "-vn", "-SO3", "-compiler=sdcc", "-clib=sdcc_iy",
            "--opt-code-size", "--fomit-frame-pointer",
            "-custom-copt-rules=src/spectalk_copt.rul",
            "-c", str(source), "-o", str(output),
        ]
        result = subprocess.run(command, cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise SystemExit(result.stdout)
    print("copt control-flow label safety check OK")


if __name__ == "__main__":
    main()

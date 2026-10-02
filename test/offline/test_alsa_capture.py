"""Compile the actual capture functions against mocks; never open an audio device.

Run with Python 3 and a C compiler: python test/offline/test_alsa_capture.py
Set CC to another compiler executable if required. No ALSA/cmocka installation
is needed. Function extraction deliberately fails if the source layout changes.
"""

import os
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / "src/linux/ALSASound.c").read_text()
    functions = []
    for signature in [
        "int SoundCardRead(short * input, unsigned int nSamples)",
        "void PollReceivedSamples()",
    ]:
        matches = re.findall(
            "^" + re.escape(signature) + r"\n\{\n.*?^\}", source,
            flags=re.MULTILINE | re.DOTALL,
        )
        if len(matches) != 1:
            raise RuntimeError(f"Expected exactly one definition of {signature}")
        functions.append(matches[0])
    harness = (root / "test/offline/alsa_capture_harness.c").read_text()
    assert harness.count("/* CAPTURE_FUNCTIONS */") == 1
    harness = harness.replace("/* CAPTURE_FUNCTIONS */", "\n\n".join(functions))
    with tempfile.TemporaryDirectory(prefix="ardop-capture-") as directory:
        directory = Path(directory)
        c_file = directory / "capture.c"
        executable = directory / ("capture.exe" if os.name == "nt" else "capture")
        c_file.write_text(harness)
        subprocess.run([
            os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-Wextra",
            "-Werror", str(c_file), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True, timeout=10)


if __name__ == "__main__":
    main()

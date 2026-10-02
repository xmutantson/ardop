"""Offline regression checks for actual ALSA initialization and lifecycle functions."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def extract(source, signature):
    matches = re.findall("^" + re.escape(signature) + r"\n\{\n.*?^\}", source, re.M | re.S)
    if len(matches) != 1:
        raise RuntimeError(f"Expected one definition of {signature}")
    return matches[0]


def main():
    here = Path(__file__).resolve().parent
    source = (here.parents[1] / "src/linux/ALSASound.c").read_text()
    initializer = (here / "alsa_open_harness.c").read_text().replace(
        "/* OPEN_FUNCTION */", extract(source, "int OpenSoundCapture(char * CaptureDevice, int m_sampleRate, char * ErrorMsg)"))
    capture = (here / "alsa_capture_harness.c").read_text().replace("int main(void)", "int original_test_main(void)")
    capture = capture.replace("/* CAPTURE_FUNCTIONS */", "\n".join(extract(source, sig) for sig in (
        "int SoundCardRead(short * input, unsigned int nSamples)", "void PollReceivedSamples()")))
    lifecycle = (here / "alsa_lifecycle_harness.c").read_text().replace("/* LIFECYCLE_FUNCTIONS */", "\n".join(extract(source, sig) for sig in (
        "int CloseSoundCard()", "int SoundCardWrite(short * input, unsigned int nSamples)", "void SoundFlush()")))
    with tempfile.TemporaryDirectory(prefix="ardop-lifecycle-") as folder:
        for name, unit in (("open", initializer), ("lifecycle", capture + lifecycle)):
            path = Path(folder) / (name + ".c")
            path.write_text(unit)
            exe = path.with_suffix(".exe" if os.name == "nt" else "")
            subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-Wno-sign-compare",
                            str(path), "-o", str(exe)], check=True, timeout=60)
            subprocess.run([str(exe)], check=True, timeout=10)


if __name__ == "__main__":
    main()

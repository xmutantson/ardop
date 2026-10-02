# Offline capture regression patch, 2026-10-02

The local branch preserves the reviewed left/right packing fix
`374772da4e6c11e1518f4b784358e20b7a1819ff`, based on `a7c9228`.
These were re-fetched from the fork before editing. The new changes address
inherited short-read handling separately from that existing fix.

`SoundCardRead` now copies exactly the returned mono samples once. It retains
the existing contiguous extraction of the selected stereo channel. Failed
capture reopens no longer pass a null handle back to ALSA, and negative
availability is checked before the unsigned frame-count comparison.

`PollReceivedSamples` now passes the actual returned sample count to noise
processing, peak measurement, WAV recording, and the decoder. It no longer
processes the unread tail of a short capture. `ProcessNewSamples` already
accumulates small blocks in `src/common/SoundInput.c`.

## Offline checks

```text
python test/offline/test_alsa_capture.py
```

Result: **20 cases passed**, compiled with GCC 13.2.0 and
`-std=c99 -Wall -Wextra -Werror`. The runner extracts the actual production
definitions of `SoundCardRead` and `PollReceivedSamples`, then compiles them
against mocked ALSA, recording, level-reporting, and decoder functions. It does
not copy the production algorithm into a test implementation and links no
device libraries. Tests exercise mono, left and right channels, full/partial/
zero reads, unavailable samples, read errors, failed and successful reopen,
disabled capture, loopback, NOSOUND, and recording expiry. Tail sentinels check
that stale samples are not copied, measured, recorded, or decoded.

The same harness fails against `374772d` at the short-read downstream-count
assertion, confirming the regression detects the inherited bug.

`mingw32-make -j2` also built the complete Windows executable successfully. It
was not run. `mingw32-make buildtest -j2` could not build the existing unit suite
because `cmocka.h` is absent. A full native Linux build against ALSA headers,
cmocka tests, device recovery under real ALSA, and radio performance remain
unvalidated; the standalone mock compilation is not a substitute for those.

## Upstream disposition

Upstream master at `e27af046c04f973ed8958a219844f655ead6c419` states that
development is discontinued and additional pull requests will not be accepted.
Upstream develop at `c1e11ef514ff0776fea74fb7bf7283909910af38` already extracts
alternate stereo samples contiguously in `src/linux/ALSA.c`, but includes a broad
unreleased audio refactor. Neither branch was merged. The closed PR was not
reopened; no remote writes, device operations, or live host connections occurred.

## Capture lifecycle follow-up

`OpenSoundCapture` now frees hardware parameters and closes/nulls the capture
handle on every failed initialization step, including prepare/start and explicit
stereo configuration failures. A successful initializer frees its temporary
parameters and retains only the ready handle.

Unexpected availability/read failures share one recovery path. It respects the
initializer return value, retries a failed reopen on later polls, and makes at
most one reopen attempt per poll and per 1,000 ms. Unsigned elapsed time handles
clock rollover. Playback, shutdown, and device enumeration cancel pending retries;
a failed post-playback reopen schedules receive recovery. Buffer capacity is
rechecked after channel renegotiation. Existing short-read sample propagation is
preserved.

Additional offline checks:

```text
python test/offline/test_alsa_capture.py
python test/offline/test_alsa_lifecycle.py
```

The capture suite now has 26 cases. The lifecycle runner compiles the actual
initializer and the actual playback/flush/shutdown definitions against mocks:
110 failed initialization attempts span 11 failure stages in mono/fallback and
explicit stereo modes, followed by successful initialization, replacement
cleanup, and stereo fallback. Playback tests verify retries remain cancelled
through transmit, restart after a failed flush reopen, and stay cancelled after
shutdown. No ALSA device, transmitter, socket, or radio is used. Reopen frequency
is bounded; actual backend call duration and native Linux integration remain
unmeasured in this Windows host environment.

/* All I/O and decoder entry points below are mocks. No ALSA/device libraries
 * are linked. The runner inserts the two production function definitions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FALSE 0
#define ZF_LOG_DEBUG 2
#define ZF_LOGD(...) ((void)0)
#define ZF_LOGE(...) ((void)0)
#define ZF_LOGI(...) ((void)0)

void *rechandle = (void *)1;
int captureRetryPending = 0, captureRetryScheduled = 0;
unsigned int captureLastRetry = 0;
char SavedCaptureDevice[] = "mock";
int SavedCaptureRate = 12000, m_recchannels = 1, UseLeftRX = 1;
char PlaybackDevice[16] = "mock";
short inbuffer[1][16];
int ReceiveSize = 4, min = 0, max = 0, lastlevelGUI = 0, lastlevelreport = 0;
unsigned char CurrentLevel;
int Now = 100, WaterfallActive = 0, SpectrumActive = 0;
int Capturing = 1, Loopback = 0, InputNoiseStdDev = 0;
void *rxwf = (void *)2;
int rxwf_EndNow = 1000;

short source_samples[] = {100, -200, 300, -400, 500, -600, 700, -800};
int available = 4, read_result = 4, read_calls, open_calls, close_calls;
int reopen_ok = 1, after_reopen_available = 4;
int failed_open_keeps_handle = 0, reopen_channels = 0;
int noise_count, wav_count, decode_count, wav_closed, level_calls;
short decoded[16];

int snd_pcm_avail_update(void *handle) { assert(handle); return available; }
int snd_pcm_readi(void *handle, short *samples, unsigned int requested)
{
    assert(handle);
    assert(requested == (unsigned int)ReceiveSize);
    read_calls++;
    if (read_result > 0)
        memcpy(samples, source_samples, read_result * m_recchannels * sizeof(short));
    return read_result;
}
int snd_pcm_close(void *handle) { assert(handle); close_calls++; return 0; }
int OpenSoundCapture(char *device, int rate, void *unused)
{
    (void)device; (void)rate; (void)unused;
    open_calls++;
    rechandle = (reopen_ok || failed_open_keeps_handle) ? (void *)1 : NULL;
    available = after_reopen_available;
    if (reopen_channels) m_recchannels = reopen_channels;
    return reopen_ok;
}
int add_noise(short *samples, int count, int deviation)
{
    (void)samples; (void)deviation; noise_count = count; return 0;
}
void wg_send_currentlevel(int channel, unsigned char level)
{ (void)channel; (void)level; level_calls++; }
void SendtoGUI(char command, unsigned char *data, int count)
{ (void)command; (void)data; (void)count; }
int ardop_log_get_level_console(void) { return 3; }
void SendCommandToHostQuiet(char *command) { (void)command; }
void CloseWav(void *file) { assert(file); wav_closed++; }
void WriteWav(short *samples, int count, void *file)
{ (void)samples; assert(file); wav_count = count; }
void ProcessNewSamples(short *samples, int count)
{
    decode_count = count;
    memcpy(decoded, samples, count * sizeof(short));
}

/* CAPTURE_FUNCTIONS */

void reset(void)
{
    rechandle = (void *)1;
    captureRetryPending = captureRetryScheduled = 0;
    captureLastRetry = 0;
    failed_open_keeps_handle = reopen_channels = 0;
    available = after_reopen_available = read_result = ReceiveSize = 4;
    m_recchannels = UseLeftRX = reopen_ok = Capturing = 1;
    Loopback = 0;
    read_calls = open_calls = close_calls = 0;
    noise_count = wav_count = decode_count = wav_closed = level_calls = 0;
    min = max = lastlevelGUI = lastlevelreport = 0;
    Now = 100;
    rxwf_EndNow = 1000;
    rxwf = (void *)2;
    strcpy(PlaybackDevice, "mock");
    for (int i = 0; i < 16; i++) inbuffer[0][i] = 30000;
}

void check_channel(int channels, int left, int count)
{
    reset();
    m_recchannels = channels;
    UseLeftRX = left;
    read_result = count;
    PollReceivedSamples();
    assert(read_calls == 1);
    assert(noise_count == count && wav_count == count && decode_count == count);
    int expected_min = 0, expected_max = 0;
    for (int i = 0; i < count; i++) {
        short expected = source_samples[channels * i + (channels == 2 && !left)];
        assert(decoded[i] == expected);
        if (expected < expected_min) expected_min = expected;
        if (expected > expected_max) expected_max = expected;
    }
    assert(min == expected_min && max == expected_max);
    // An unread tail must not be copied, scanned for level, recorded or decoded.
    for (int i = count; i < 16; i++) assert(inbuffer[0][i] == 30000);
    assert(level_calls == (count > 0));
}

int main(void)
{
    int tests = 0;
    for (int mode = 0; mode < 3; mode++) {
        int channels = mode == 0 ? 1 : 2;
        int left = mode != 2;
        check_channel(channels, left, 4); tests++;
        check_channel(channels, left, 2); tests++;
        check_channel(channels, left, 0); tests++;
    }
    reset(); available = 2; PollReceivedSamples();
    assert(read_calls == 0 && decode_count == 0); tests++;

    reset(); read_result = -32; PollReceivedSamples();
    assert(read_calls == 1 && open_calls == 1 && close_calls == 1);
    assert(noise_count == 0 && wav_count == 0 && decode_count == 0); tests++;

    reset(); available = -32; reopen_ok = 0; PollReceivedSamples();
    assert(rechandle == NULL && read_calls == 0 && decode_count == 0); tests++;

    reset(); read_result = -32; reopen_ok = 0; PollReceivedSamples();
    assert(rechandle == NULL && decode_count == 0); tests++;

    reset(); available = after_reopen_available = -32; PollReceivedSamples();
    assert(read_calls == 0 && decode_count == 0); tests++;

    reset(); available = -32; PollReceivedSamples();
    assert(open_calls == 1 && decode_count == 4); tests++;

    reset(); Capturing = 0; read_result = 2; PollReceivedSamples();
    assert(wav_count == 2 && decode_count == 0); tests++;

    reset(); Loopback = 1; read_result = 2; PollReceivedSamples();
    assert(wav_count == 2 && decode_count == 0); tests++;

    reset(); strcpy(PlaybackDevice, "NOSOUND"); PollReceivedSamples();
    assert(read_calls == 0 && decode_count == 0); tests++;

    reset(); rxwf_EndNow = 0; read_result = 2; PollReceivedSamples();
    assert(wav_closed == 1 && rxwf == NULL && wav_count == 0 && decode_count == 2); tests++;

    reset(); rechandle = NULL; PollReceivedSamples();
    assert(read_calls == 0 && decode_count == 0); tests++;

    reset(); available = -32; reopen_ok = 0; PollReceivedSamples();
    assert(open_calls == 1 && captureRetryPending);
    for (int i = 0; i < 999; ++i) { Now++; PollReceivedSamples(); }
    assert(open_calls == 1 && decode_count == 0);
    Now++; reopen_ok = 1; PollReceivedSamples();
    assert(open_calls == 2 && decode_count == 4 && !captureRetryPending); tests++;

    reset(); available = -32; reopen_ok = 0; failed_open_keeps_handle = 1;
    PollReceivedSamples();
    assert(rechandle == NULL && open_calls == 1 && close_calls == 2);
    assert(read_calls == 0 && decode_count == 0); tests++;

    reset(); available = after_reopen_available = -32;
    for (int i = 0; i < 5000; ++i) { PollReceivedSamples(); Now++; }
    assert(open_calls == 5 && rechandle == NULL && decode_count == 0); tests++;

    reset(); available = -32; reopen_ok = 0; Now = -500;
    PollReceivedSamples(); Now = 499; PollReceivedSamples();
    assert(open_calls == 1);
    Now = 500; reopen_ok = 1; PollReceivedSamples();
    assert(open_calls == 2 && decode_count == 4); tests++;

    reset(); rechandle = NULL;
    for (int i = 0; i < 10; ++i) { Now += 1000; PollReceivedSamples(); }
    assert(open_calls == 0 && read_calls == 0); tests++;

    reset(); available = -32; reopen_channels = 2; ReceiveSize = 32769;
    PollReceivedSamples();
    assert(open_calls == 1 && read_calls == 0 && decode_count == 0); tests++;

    printf("PASS: %d offline ALSA capture cases\n", tests);
    return 0;
}

/* Extra mocks for the actual playback/flush/shutdown definitions. */
#define SHARECAPTURE
void *playhandle = (void *)3;
typedef int snd_pcm_status_t;
typedef int snd_pcm_sframes_t;
short buffer[1][16];
int Index, Number = 4, SampleNo = 0, pttOnTime = 0;
int SoundIsPlaying = 1, blnEnbARQRpt, blnDISCRepeating;
int dttNextPlay, intFrameRepeatInterval, extraDelay, WriteRxWav;
void *txwff = NULL;
void txSleep(int ms) { Now += ms; }
int snd_pcm_recover(void *h, int e, int silent) { (void)h; (void)e; (void)silent; return 0; }
int PackSamplesAndSend(short *input, int count) { (void)input; return count; }
void AddTrailer(void) {}
void SendtoCard(short *input, int count) { (void)input; (void)count; }
void usleep(int us) { Now += us / 1000; }
void KeyPTT(int state) { (void)state; }
void StartCapture(void) { Capturing = 1; }
void StartRxWav(void) {}

/* LIFECYCLE_FUNCTIONS */

int main(void) {
    reset();
    available = -32; reopen_ok = 0; PollReceivedSamples();
    assert(captureRetryPending && !rechandle);
    available = 4;
    assert(SoundCardWrite(source_samples, 4) == 4);
    for (int i = 0; i < 5; i++) { Now += 1000; PollReceivedSamples(); }
    assert(open_calls == 1 && !captureRetryPending && !rechandle);
    SoundFlush();  /* post-playback reopen also fails */
    assert(open_calls == 2 && captureRetryPending && !rechandle && !playhandle);
    Now += 999; PollReceivedSamples(); assert(open_calls == 2);
    Now++; reopen_ok = 1; PollReceivedSamples();
    assert(open_calls == 3 && rechandle && decode_count == 4);

    reset(); playhandle = (void *)3;
    assert(SoundCardWrite(source_samples, 4) == 4);
    Now += 5000; PollReceivedSamples();
    assert(!rechandle && open_calls == 0);
    SoundFlush(); PollReceivedSamples();
    assert(rechandle && open_calls == 1 && decode_count == 4);

    reset(); playhandle = (void *)3;
    available = -32; reopen_ok = 0; PollReceivedSamples();
    CloseSoundCard();
    for (int i = 0; i < 5; i++) { Now += 1000; PollReceivedSamples(); }
    assert(!rechandle && !playhandle && open_calls == 1 && !captureRetryPending);
    puts("PASS: playback cancels pending retry, failed/successful flush reopen, shutdown cancels retry");
    return 0;
}

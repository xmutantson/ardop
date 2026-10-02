/* Production initializer, with every ALSA allocation and initialization step mocked. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ZF_LOGE(...) ((void)0)
#define ZF_LOGI(...) ((void)0)
#define TRUE 1
#define FALSE 0
#define true 1
#define false 0
#define SND_PCM_STREAM_CAPTURE 1
#define SND_PCM_ACCESS_RW_INTERLEAVED 1
#define SND_PCM_FORMAT_S16_LE 1
typedef int snd_pcm_t;
typedef int snd_pcm_hw_params_t;
snd_pcm_t token, *rechandle;
snd_pcm_hw_params_t params;
int fpp, dir, m_recchannels, UseLeftRX = 1, UseRightRX = 1;
int fail_stage, opened, closed, allocations, frees, starts, channel_calls;
const char *snd_strerror(int n) { (void)n; return "mock initialization failure"; }
int stage(int n) { return fail_stage == n ? -5 : 0; }
int snd_pcm_open(snd_pcm_t **h, const char *d, int s, int flags) {
    (void)d; (void)s; (void)flags;
    if (stage(1)) return -5;
    *h = &token; opened++; return 0;
}
int snd_pcm_close(snd_pcm_t *h) { assert(h == &token); closed++; return 0; }
int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **p) {
    if (stage(2)) return -5;
    *p = &params; allocations++; return 0;
}
void snd_pcm_hw_params_free(snd_pcm_hw_params_t *p) { assert(p == &params); frees++; }
int snd_pcm_hw_params_any(snd_pcm_t *h, snd_pcm_hw_params_t *p) { (void)h; (void)p; return stage(3); }
int snd_pcm_hw_params_set_period_size_near(snd_pcm_t *h, snd_pcm_hw_params_t *p, int *f, int *d) { (void)h; (void)p; (void)f; (void)d; return stage(4); }
int snd_pcm_hw_params_set_access(snd_pcm_t *h, snd_pcm_hw_params_t *p, int a) { (void)h; (void)p; (void)a; return stage(5); }
int snd_pcm_hw_params_set_format(snd_pcm_t *h, snd_pcm_hw_params_t *p, int f) { (void)h; (void)p; (void)f; return stage(6); }
int snd_pcm_hw_params_set_rate(snd_pcm_t *h, snd_pcm_hw_params_t *p, int r, int d) { (void)h; (void)p; (void)r; (void)d; return stage(7); }
int snd_pcm_hw_params_set_channels(snd_pcm_t *h, snd_pcm_hw_params_t *p, int n) {
    (void)h; (void)p; channel_calls++;
    if (fail_stage == 12 && n == 1) return -5; /* supported stereo fallback */
    return stage(8);
}
int snd_pcm_hw_params(snd_pcm_t *h, snd_pcm_hw_params_t *p) { (void)h; (void)p; return stage(9); }
int snd_pcm_prepare(snd_pcm_t *h) { (void)h; return stage(10); }
int snd_pcm_start(snd_pcm_t *h) { (void)h; starts++; return stage(11); }

/* OPEN_FUNCTION */

int main(void) {
    char error[256];
    int attempts = 0;
    for (int side = 0; side < 2; side++) {
        UseLeftRX = side;  /* explicit stereo as well as mono -> stereo fallback */
        for (fail_stage = 1; fail_stage <= 11; fail_stage++) {
            for (int repeat = 0; repeat < 5; repeat++) {
                assert(OpenSoundCapture("mock", 12000, repeat % 2 ? error : NULL) == 0);
                assert(rechandle == NULL && opened == closed && allocations == frees);
                attempts++;
            }
        }
    }
    fail_stage = 0;
    assert(OpenSoundCapture("mock", 12000, NULL) == 1);
    assert(rechandle && opened == closed + 1 && allocations == frees);
    fail_stage = 3;
    assert(OpenSoundCapture("mock", 12000, NULL) == 0);
    assert(!rechandle && opened == closed && allocations == frees);
    fail_stage = 12; UseLeftRX = UseRightRX = 1;
    channel_calls = 0;
    assert(OpenSoundCapture("mock", 12000, error) == 1);
    assert(rechandle && m_recchannels == 2 && channel_calls == 2 && allocations == frees);
    snd_pcm_close(rechandle); rechandle = NULL;
    assert(opened == closed);
    printf("PASS: %d initialization failure attempts, replacement cleanup, success and stereo fallback\n", attempts);
    return 0;
}

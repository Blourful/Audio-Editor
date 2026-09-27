#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sound_seg;
struct sound_seg *tr_init(void);
void tr_destroy(struct sound_seg *track);
size_t tr_length(struct sound_seg *track);
void tr_read(struct sound_seg *track, int16_t *dest, size_t pos, size_t len);
void tr_write(struct sound_seg *track, const int16_t *src, size_t pos, size_t len);
bool tr_delete_range(struct sound_seg *track, size_t pos, size_t len);
char *tr_identify(const struct sound_seg *target, const struct sound_seg *ad);
void tr_insert(struct sound_seg *src, struct sound_seg *dest,
               size_t destpos, size_t srcpos, size_t len);
void wav_load(const char *fname, int16_t *dest);
void wav_save(const char *fname, const int16_t *src, size_t len);

static void expect_track(struct sound_seg *t, const int16_t *want, size_t n) {
    assert(tr_length(t) == n);
    int16_t *got = malloc((n ? n : 1) * sizeof(*got));
    assert(got != NULL);
    if (n) tr_read(t, got, 0, n);
    assert(n == 0 || memcmp(got, want, n * sizeof(*want)) == 0);
    free(got);
}

static void test_basic_write_read(void) {
    struct sound_seg *t = tr_init();
    int16_t a[] = {10, 20, 30, 40};
    tr_write(t, a, 0, 4);
    expect_track(t, a, 4);

    int16_t x[] = {99, 88};
    tr_write(t, x, 1, 2);
    int16_t want[] = {10, 99, 88, 40};
    expect_track(t, want, 4);
    tr_destroy(t);
    puts("PASS basic read/write");
}

static void test_write_extension(void) {
    struct sound_seg *t = tr_init();
    int16_t x[] = {7, 8};
    tr_write(t, x, 3, 2);
    int16_t want[] = {0, 0, 0, 7, 8};
    expect_track(t, want, 5);
    tr_destroy(t);
    puts("PASS write extension");
}

static void test_delete(void) {
    struct sound_seg *t = tr_init();
    int16_t a[] = {1, 2, 3, 4, 5, 6};
    tr_write(t, a, 0, 6);
    assert(tr_delete_range(t, 2, 2));
    int16_t want[] = {1, 2, 5, 6};
    expect_track(t, want, 4);
    tr_destroy(t);
    puts("PASS delete range");
}

static void test_insert_and_shared_write(void) {
    struct sound_seg *parent = tr_init();
    struct sound_seg *child = tr_init();
    int16_t p[] = {10, 20, 30, 40, 50};
    int16_t c[] = {1, 2};
    tr_write(parent, p, 0, 5);
    tr_write(child, c, 0, 2);

    tr_insert(parent, child, 1, 1, 3); /* child = 1,20,30,40,2 */
    int16_t cw[] = {1, 20, 30, 40, 2};
    expect_track(child, cw, 5);

    /* parent -> child propagation */
    int16_t v = 777;
    tr_write(parent, &v, 2, 1);
    int16_t got = 0;
    tr_read(child, &got, 2, 1);
    assert(got == 777);

    /* child -> parent propagation */
    v = -123;
    tr_write(child, &v, 1, 1);
    tr_read(parent, &got, 1, 1);
    assert(got == -123);

    /* REQ3.2: parent portion with children cannot be deleted. */
    assert(!tr_delete_range(parent, 1, 3));

    tr_destroy(child);
    assert(tr_delete_range(parent, 1, 3));
    tr_destroy(parent);
    puts("PASS insert sharing + protected parent deletion");
}

static void test_multigeneration(void) {
    struct sound_seg *a = tr_init();
    struct sound_seg *b = tr_init();
    struct sound_seg *c = tr_init();
    int16_t src[] = {5, 6, 7, 8};
    tr_write(a, src, 0, 4);
    tr_insert(a, b, 0, 1, 2); /* b = 6,7 */
    tr_insert(b, c, 0, 0, 2); /* c = 6,7 */

    int16_t v = 999;
    tr_write(c, &v, 1, 1);
    int16_t got = 0;
    tr_read(a, &got, 2, 1);
    assert(got == 999);
    tr_read(b, &got, 1, 1);
    assert(got == 999);

    /* Destroying an ancestor must not invalidate shared sample data. */
    tr_destroy(a);
    v = 321;
    tr_write(c, &v, 0, 1);
    tr_read(b, &got, 0, 1);
    assert(got == 321);

    tr_destroy(c);
    tr_destroy(b);
    puts("PASS multi-generation sharing + ancestor destroy");
}

static void test_self_insert(void) {
    struct sound_seg *t = tr_init();
    int16_t a[] = {1, 2, 3, 4};
    tr_write(t, a, 0, 4);
    tr_insert(t, t, 2, 0, 3);
    int16_t want[] = {1, 2, 1, 2, 3, 3, 4};
    expect_track(t, want, 7);

    /* inserted references must still share with the original source samples */
    int16_t v = 55;
    tr_write(t, &v, 0, 1);
    int16_t got = 0;
    tr_read(t, &got, 2, 1);
    assert(got == 55);
    tr_destroy(t);
    puts("PASS self insertion");
}

static void test_identify(void) {
    struct sound_seg *target = tr_init();
    struct sound_seg *ad = tr_init();
    int16_t av[] = {100, -200, 300};
    int16_t tv[] = {1, 2, 100, -200, 300, 9, 8, 100, -200, 300, 4};
    tr_write(ad, av, 0, 3);
    tr_write(target, tv, 0, sizeof(tv) / sizeof(tv[0]));
    char *s = tr_identify(target, ad);
    assert(s != NULL);
    assert(strcmp(s, "2,4\n7,9") == 0);
    free(s);
    tr_destroy(ad);
    tr_destroy(target);
    puts("PASS identify multiple ads");
}

static void test_wav_roundtrip(void) {
    const char *name = "test_roundtrip.wav";
    int16_t src[] = {0, 123, -456, 32767, -32768};
    int16_t dst[5] = {0};
    wav_save(name, src, 5);
    wav_load(name, dst);
    assert(memcmp(src, dst, sizeof(src)) == 0);
    remove(name);
    puts("PASS WAV save/load round trip");
}

int main(void) {
    test_basic_write_read();
    test_write_extension();
    test_delete();
    test_insert_and_shared_write();
    test_multigeneration();
    test_self_insert();
    test_identify();
    test_wav_roundtrip();
    puts("\nALL TESTS PASSED");
    return 0;
}

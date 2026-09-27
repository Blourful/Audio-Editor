#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

struct sound_seg;

struct sound_seg *tr_init(void);
void tr_destroy(struct sound_seg *track);
size_t tr_length(struct sound_seg *track);
void tr_write(struct sound_seg *track,
              const int16_t *src,
              size_t pos,
              size_t len);
void tr_read(struct sound_seg *track,
             int16_t *dest,
             size_t pos,
             size_t len);

int main(void)
{
    struct sound_seg *track = tr_init();

    int16_t data[] = {10, 20, 30, 40, 50};

    tr_write(track, data, 0, 5);

    printf("Track length: %zu\n", tr_length(track));

    int16_t output[5];
    tr_read(track, output, 0, 5);

    printf("Samples: ");

    for (size_t i = 0; i < 5; i++)
    {
        printf("%d ", output[i]);
    }

    printf("\n");

    tr_destroy(track);

    return 0;
}
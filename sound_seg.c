#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "wavheader.h"

/* A sample_store is the shared backing store for one logical sample.
 * Every node produced by tr_insert points at the same sample_store as its
 * source node, so writes are automatically visible through every reference. */
struct sample_store {
    int16_t value;
    size_t refs;
};

struct sound_node {
    struct sample_store *store;
    struct sound_node *prev;
    struct sound_node *next;

    /* Structural parent/child links are separate from the shared data.
     * They exist only to enforce REQ3.2 (a parent cannot be deleted while it
     * has children).  tr_destroy is allowed to break these links. */
    struct sound_node *parent;
    struct sound_node *first_child;
    struct sound_node *prev_sibling;
    struct sound_node *next_sibling;
};

struct sound_seg {
    struct sound_node *head;
    struct sound_node *tail;
    size_t length;
};

static void die_alloc(void) { exit(1); }

static struct sample_store *store_new(int16_t value) {
    struct sample_store *s = malloc(sizeof(*s));
    if (!s) die_alloc();
    s->value = value;
    s->refs = 1;
    return s;
}

static struct sound_node *node_new_with_store(struct sample_store *store) {
    struct sound_node *n = calloc(1, sizeof(*n));
    if (!n) die_alloc();
    n->store = store;
    return n;
}

static struct sound_node *node_new_value(int16_t value) {
    return node_new_with_store(store_new(value));
}

static struct sound_node *node_at(const struct sound_seg *track, size_t pos) {
    if (!track || pos >= track->length) return NULL;
    if (pos < track->length / 2) {
        struct sound_node *n = track->head;
        for (size_t i = 0; i < pos; ++i) n = n->next;
        return n;
    }
    struct sound_node *n = track->tail;
    for (size_t i = track->length - 1; i > pos; --i) n = n->prev;
    return n;
}

static void add_child(struct sound_node *parent, struct sound_node *child) {
    child->parent = parent;
    child->prev_sibling = NULL;
    child->next_sibling = parent->first_child;
    if (parent->first_child) parent->first_child->prev_sibling = child;
    parent->first_child = child;
}

static void unlink_from_parent(struct sound_node *node) {
    if (!node->parent) return;
    if (node->prev_sibling) node->prev_sibling->next_sibling = node->next_sibling;
    else node->parent->first_child = node->next_sibling;
    if (node->next_sibling) node->next_sibling->prev_sibling = node->prev_sibling;
    node->parent = NULL;
    node->prev_sibling = NULL;
    node->next_sibling = NULL;
}

static void orphan_children(struct sound_node *node) {
    struct sound_node *c = node->first_child;
    while (c) {
        struct sound_node *next = c->next_sibling;
        c->parent = NULL;
        c->prev_sibling = NULL;
        c->next_sibling = NULL;
        c = next;
    }
    node->first_child = NULL;
}

static void release_node(struct sound_node *node, bool destroying) {
    if (!node) return;
    unlink_from_parent(node);
    if (destroying) orphan_children(node);
    if (node->store && --node->store->refs == 0) free(node->store);
    free(node);
}

void wav_load(const char *fname, int16_t *dest) {
    FILE *f = fopen(fname, "rb");
    if (!f) exit(1);
    /* Under ASM0.1 the supplied WAVs use the ordinary 44-byte PCM header. */
    if (fseek(f, 44, SEEK_SET) != 0) { fclose(f); exit(1); }
    while (fread(dest, sizeof(*dest), 1, f) == 1) ++dest;
    fclose(f);
}

void wav_save(const char *fname, const int16_t *src, size_t len) {
    wav_header h;
    memcpy(h.riff_header, "RIFF", 4);
    h.wav_size = (int)(36 + len * sizeof(int16_t));
    memcpy(h.wave_header, "WAVE", 4);
    memcpy(h.fmt_header, "fmt ", 4);
    h.fmt_chunk_size = 16;
    h.audio_format = 1;
    h.num_channels = 1;
    h.sample_rate = 8000;
    h.byte_rate = 8000 * 1 * 16 / 8;
    h.sample_alignment = 1 * 16 / 8;
    h.bit_depth = 16;
    memcpy(h.data_header, "data", 4);
    h.data_bytes = (int)(len * sizeof(int16_t));

    FILE *f = fopen(fname, "wb");
    if (!f) exit(1);
    fwrite(&h, sizeof(h), 1, f);
    fwrite(src, sizeof(*src), len, f);
    fclose(f);
}

struct sound_seg *tr_init(void) {
    struct sound_seg *track = calloc(1, sizeof(*track));
    if (!track) die_alloc();
    return track;
}

void tr_destroy(struct sound_seg *track) {
    if (!track) return;
    struct sound_node *n = track->head;
    while (n) {
        struct sound_node *next = n->next;
        release_node(n, true);
        n = next;
    }
    free(track);
}

size_t tr_length(struct sound_seg *track) {
    return track ? track->length : 0;
}

void tr_read(struct sound_seg *track, int16_t *dest, size_t pos, size_t len) {
    if (!track || !dest || len == 0) return;
    if (pos > track->length || len > track->length - pos) return;
    struct sound_node *n = (pos == track->length) ? NULL : node_at(track, pos);
    for (size_t i = 0; i < len; ++i, n = n->next) dest[i] = n->store->value;
}

static void append_zero(struct sound_seg *track) {
    struct sound_node *n = node_new_value(0);
    n->prev = track->tail;
    if (track->tail) track->tail->next = n;
    else track->head = n;
    track->tail = n;
    ++track->length;
}

void tr_write(struct sound_seg *track, const int16_t *src, size_t pos, size_t len) {
    if (!track || !src || len == 0) return;
    if (pos > SIZE_MAX - len) return;
    size_t need = pos + len;
    while (track->length < need) append_zero(track);

    struct sound_node *n = node_at(track, pos);
    for (size_t i = 0; i < len; ++i, n = n->next) {
        /* Do not detach a shared store: propagation is the required behaviour. */
        n->store->value = src[i];
    }
}

bool tr_delete_range(struct sound_seg *track, size_t pos, size_t len) {
    if (!track || pos > track->length || len > track->length - pos) return false;
    if (len == 0) return true;

    struct sound_node *first = node_at(track, pos);
    struct sound_node *n = first;
    for (size_t i = 0; i < len; ++i, n = n->next) {
        if (n->first_child) return false;
    }

    struct sound_node *after = n;
    struct sound_node *before = first->prev;
    if (before) before->next = after;
    else track->head = after;
    if (after) after->prev = before;
    else track->tail = before;

    n = first;
    for (size_t i = 0; i < len; ++i) {
        struct sound_node *next = n->next;
        release_node(n, false);
        n = next;
    }
    track->length -= len;
    return true;
}

static long double dot_window(const int16_t *target, const int16_t *ad,
                              size_t start, size_t ad_len) {
    long double sum = 0.0L;
    for (size_t i = 0; i < ad_len; ++i)
        sum += (long double)target[start + i] * (long double)ad[i];
    return sum;
}

static int16_t *extract_rawdata(const struct sound_seg *track) {
    if (!track || track->length == 0) return NULL;
    int16_t *data = malloc(track->length * sizeof(*data));
    if (!data) die_alloc();
    struct sound_node *n = track->head;
    for (size_t i = 0; i < track->length; ++i, n = n->next) data[i] = n->store->value;
    return data;
}

char *tr_identify(const struct sound_seg *target, const struct sound_seg *ad) {
    if (!target || !ad || ad->length == 0 || target->length < ad->length) {
        char *empty = malloc(1);
        if (!empty) die_alloc();
        empty[0] = '\0';
        return empty;
    }

    int16_t *t = extract_rawdata(target);
    int16_t *a = extract_rawdata(ad);
    long double reference = dot_window(a, a, 0, ad->length);
    if (reference <= 0.0L) {
        free(t); free(a);
        char *empty = malloc(1);
        if (!empty) die_alloc();
        empty[0] = '\0';
        return empty;
    }
    long double threshold = reference * 0.95L;

    size_t cap = 64, used = 0;
    char *out = malloc(cap);
    if (!out) die_alloc();
    out[0] = '\0';

    for (size_t start = 0; start + ad->length <= target->length; ) {
        if (dot_window(t, a, start, ad->length) >= threshold) {
            char line[64];
            int count = snprintf(line, sizeof(line), "%s%zu,%zu",
                                 used ? "\n" : "", start, start + ad->length - 1);
            if (count < 0) { free(out); free(t); free(a); exit(1); }
            size_t add = (size_t)count;
            if (used + add + 1 > cap) {
                while (used + add + 1 > cap) cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); free(t); free(a); die_alloc(); }
                out = tmp;
            }
            memcpy(out + used, line, add + 1);
            used += add;
            start += ad->length; /* occurrences are guaranteed non-overlapping */
        } else {
            ++start;
        }
    }

    free(t);
    free(a);
    return out;
}

void tr_insert(struct sound_seg *src_track, struct sound_seg *dest_track,
               size_t destpos, size_t srcpos, size_t len) {
    if (!src_track || !dest_track || len == 0) return;
    if (srcpos > src_track->length || len > src_track->length - srcpos) return;
    if (destpos > dest_track->length) return;

    /* Snapshot the exact source nodes before modifying dest.  This is what
     * makes self-insertion well-defined even when destpos lies in the source. */
    struct sound_node **src_nodes = malloc(len * sizeof(*src_nodes));
    if (!src_nodes) die_alloc();
    struct sound_node *s = node_at(src_track, srcpos);
    for (size_t i = 0; i < len; ++i, s = s->next) src_nodes[i] = s;

    struct sound_node *insert_head = NULL, *insert_tail = NULL;
    for (size_t i = 0; i < len; ++i) {
        struct sample_store *store = src_nodes[i]->store;
        ++store->refs;
        struct sound_node *n = node_new_with_store(store);
        add_child(src_nodes[i], n);
        n->prev = insert_tail;
        if (insert_tail) insert_tail->next = n;
        else insert_head = n;
        insert_tail = n;
    }

    struct sound_node *after = (destpos == dest_track->length) ? NULL : node_at(dest_track, destpos);
    struct sound_node *before = after ? after->prev : dest_track->tail;

    insert_head->prev = before;
    insert_tail->next = after;
    if (before) before->next = insert_head;
    else dest_track->head = insert_head;
    if (after) after->prev = insert_tail;
    else dest_track->tail = insert_tail;
    dest_track->length += len;

    free(src_nodes);
}

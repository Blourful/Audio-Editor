# Audio Editor Backend in C

A lightweight audio editing backend written in C for manipulating PCM audio data.

The project explores low-level memory management and data structures for audio editing, with a focus on efficiently sharing audio data between multiple tracks.

## Features

- Read and write 16-bit PCM WAV audio
- Create and manage audio tracks
- Read and overwrite audio samples
- Dynamically extend tracks
- Delete sections of audio
- Insert sections between tracks
- Shared backing storage for inserted audio
- Changes to shared samples are reflected across tracks
- Self-insertion support
- Audio pattern detection using cross-correlation

## Audio Format

The current implementation works with:

- PCM audio
- Mono channel
- 16-bit samples
- 8000 Hz sample rate

## Shared Audio Storage

Instead of copying audio data whenever a section is inserted into another track, inserted samples can reference shared underlying storage.

Conceptually:

```text id="4ofh4c"
Track A ─────┐
             │
             ├── Shared Audio Data
             │
Track B ─────┘
```

If a shared sample is modified through one track, the change is visible from other tracks referencing the same sample.

This reduces unnecessary duplication while allowing tracks to maintain independent logical structures.

## Building

The project uses GCC and Make.

```bash id="zt82za"
make
```

To remove generated files:

```bash id="f00zoz"
make clean
```

## Testing

Run the test suite with:

```bash id="yio5bv"
make test
```

Memory safety can also be checked using AddressSanitizer and UndefinedBehaviorSanitizer:

```bash id="ly4co2"
make asan
```

## Technologies

- C
- GCC
- GNU Make
- AddressSanitizer
- UndefinedBehaviorSanitizer

## Purpose

This project is an exploration of systems programming concepts including:

- manual memory management
- linked data structures
- shared backing storage
- reference management
- file I/O
- digital audio representation
- cross-correlation

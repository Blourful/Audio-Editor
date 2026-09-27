CC = gcc

CFLAGS = -fPIC -Wall -Wextra -Werror -std=c11
ASAN_FLAGS = -fsanitize=address,undefined -g

TARGET = sound_seg.o
TEST = test_sound_seg


all: $(TARGET)

$(TARGET): sound_seg.c
	$(CC) $(CFLAGS) -c sound_seg.c -o $(TARGET)


test: sound_seg.c test_sound_seg.c
	$(CC) $(CFLAGS) sound_seg.c test_sound_seg.c -o $(TEST)
	./$(TEST)

asan: sound_seg.c test_sound_seg.c
	$(CC) $(CFLAGS) $(ASAN_FLAGS) sound_seg.c test_sound_seg.c -o $(TEST)_asan
	./$(TEST)_asan

clean:
	rm -f $(TARGET)
	rm -f $(TEST)
	rm -f $(TEST)_asan
	rm -f test_roundtrip.wav

.PHONY: all test asan clean
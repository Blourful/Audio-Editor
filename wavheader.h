// WAV header spec information:
//https://web.archive.org/web/20140327141505/https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
//http://www.topherlee.com/software/pcm-tut-wavformat.html
//https://stackoverflow.com/questions/13039846/what-do-the-bytes-in-a-wav-file-represent
/*Positions  Typical Value Description

1 - 4      "RIFF"        Marks the file as a RIFF multimedia file.
                         Characters are each 1 byte long.

5 - 8      (integer)     The overall file size in bytes (32-bit integer)
                         minus 8 bytes. Typically, you'd fill this in after
                         file creation is complete.

9 - 12     "WAVE"        RIFF file format header. For our purposes, it
                         always equals "WAVE".

13-16      "fmt "        Format sub-chunk marker. Includes trailing null.

17-20      16            Length of the rest of the format sub-chunk below.

21-22      1             Audio format code, a 2 byte (16 bit) integer. 
                         1 = PCM (pulse code modulation).

23-24      2             Number of channels as a 2 byte (16 bit) integer.
                         1 = mono, 2 = stereo, etc.

25-28      44100         Sample rate as a 4 byte (32 bit) integer. Common
                         values are 44100 (CD), 48000 (DAT). Sample rate =
                         number of samples per second, or Hertz.

29-32      176400        (SampleRate * BitsPerSample * Channels) / 8
                         This is the Byte rate.

33-34      4             (BitsPerSample * Channels) / 8
                         1 = 8 bit mono, 2 = 8 bit stereo or 16 bit mono, 4
                         = 16 bit stereo.

35-36      16            Bits per sample. 

37-40      "data"        Data sub-chunk header. Marks the beginning of the
                         raw data section.

41-44      (integer)     The number of bytes of the data section below this
                         point. Also equal to (#ofSamples * #ofChannels *
                         BitsPerSample) / 8

45+                      The raw audio data.  */

// WAV header struct
typedef struct wav_header {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    int wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"
    
    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    int fmt_chunk_size; // Should be 16 for PCM
    short audio_format; // Should be 1 for PCM. 3 for IEEE Float
    short num_channels;
    int sample_rate;
    int byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    short sample_alignment; // num_channels * Bytes Per Sample
    short bit_depth; // Number of bits per sample
    
    // Data
    char data_header[4]; // Contains "data"
    int data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
} wav_header;
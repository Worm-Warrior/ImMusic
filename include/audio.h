#ifndef IMMUSIC_AUDIO_H
#define IMMUSIC_AUDIO_H

/*
 * This is for all the audio streaming stuff to be used in a different thread in the background.
 * We will use ffmpeg for the bulk of the work, and feed it to the SDL audio engine.
 */

#include <mutex>
#include <SDL_audio.h>
#include <string>
#include <thread>

extern "C" {
#include <ffmpeg/libavformat/avformat.h>
#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libswresample/swresample.h>
#include <ffmpeg/libavutil/opt.h>
}

struct audio_context_t {
    AVFormatContext *format_context = nullptr;
    AVCodecContext *codec_context = nullptr;
    SwrContext *swr_context = nullptr;
    SDL_AudioStream *audio_stream = nullptr;

    int audio_stream_index = -1;
    bool is_paused = false;
    bool should_stop = false;

    Sint64 frames = 0;

    std::atomic<bool> seek_req{false};
    std::atomic<int> seek_seconds{0};
    std::mutex mutex;

    Sint32 bytes_per_frame;
};
// Core
bool init_audio(audio_context_t &ctx, SDL_AudioSpec &spec);
bool load_file(audio_context_t &ctx, const std::string &filepath);
void start_decoding_thread(audio_context_t &ctx, std::thread &thread);
void stop_decoding(audio_context_t &ctx, std::thread &thread);

// Controls for playback
void toggle_play_pause(audio_context_t &ctx);
void seek(audio_context_t &ctx, int seconds);
void cleanup_audio(audio_context_t &ctx);

#endif //IMMUSIC_AUDIO_H
/*
 * Implementation for audio.h to handle audio streaming on background thread
 */
#include "audio.h"

#include <SDL3/SDL_oldnames.h>
#include <cstdint>
#include <ffmpeg/libavutil/avutil.h>

#include "app_state.h"
#include "debug_log.h"


// TODO: add checks for if ALL things are initialized, currently missing some and assuming they will work.
void decode_thread(audio_context_t &ctx, app_state_t &app_state) {
    while (!ctx.should_stop) {
        if (ctx.is_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (ctx.seek_req.load(std::memory_order_acquire)) {
            std::lock_guard lock(ctx.mutex);

            const int64_t seconds = ctx.seek_seconds.load(std::memory_order_relaxed);
            const AVStream *st = ctx.format_context->streams[ctx.audio_stream_index];

            // All timestamps passed to avformat_seek_file() are in the time base of the
            // selected stream, not in AV_TIME_BASE units.
            const int64_t min_ts = st->start_time != AV_NOPTS_VALUE ? st->start_time : 0;

            int64_t max_ts;
            if (st->duration != AV_NOPTS_VALUE && st->duration > 0) {
                // If the stream duration is valid, then we make the max_ts = start time + duration.
                max_ts = av_sat_add64(min_ts, st->duration);
            } else if (ctx.format_context->duration != AV_NOPTS_VALUE) {
                // If the stream duration is not valid then we need to use the format_context duration
                // This is by default in AV_TIME_BASE_Q units, not the stream untis,
                // so we need to rescale to get the right unit of time.
                max_ts = av_sat_add64(min_ts, av_rescale_q(ctx.format_context->duration, AV_TIME_BASE_Q,
                                                           st->time_base));
            } else {
                // If all else fails, make it INT_MAX
                max_ts = INT64_MAX;
            }

            // The target is the start time + scaled units
            // Seconds * AV_TIME_BASE => Get time in base unit => Rescale to get stream units.
            int64_t target_ts = av_sat_add64(min_ts, av_rescale_q(seconds * AV_TIME_BASE, AV_TIME_BASE_Q,
                                                                  st->time_base));

            // The duration used by the UI (taglib) does not always match FFmpeg's, so
            // clamp the target into the range the demuxer considers seekable.
            target_ts = av_clip64(target_ts, min_ts, max_ts);

            int ret = avformat_seek_file(ctx.format_context, ctx.audio_stream_index, min_ts, target_ts,
                                         max_ts, 0);

            // Demuxers that seek with a binary search (FLAC being the common case) can fail
            // close to the end of a file: the final frame has no following frame header to
            // delimit it. Back off by increasingly large steps until the demuxer accepts a
            // timestamp instead of failing the whole seek.
            int64_t seek_ts = target_ts;
            int64_t back_off_ts = av_rescale_q(AV_TIME_BASE, AV_TIME_BASE_Q, st->time_base);
            while (ret < 0 && seek_ts > min_ts) {
                seek_ts = seek_ts > av_sat_add64(min_ts, back_off_ts) ? seek_ts - back_off_ts : min_ts;
                ret = avformat_seek_file(ctx.format_context, ctx.audio_stream_index, min_ts, seek_ts,
                                         max_ts, 0);
                if (ret >= 0) {
                    target_ts = seek_ts;
                }
                back_off_ts = av_sat_add64(back_off_ts, back_off_ts);
            }

            if (ret >= 0) {
                SDL_ClearAudioStream(ctx.audio_stream);
                avcodec_flush_buffers(ctx.codec_context);
                //SDL_FlushAudioStream(ctx.audio_stream); // this is used to signal end of input.
                swr_init(ctx.swr_context);

                // Store the position we actually landed on. It can be slightly earlier than
                // requested when the seek had to back off from the end of the file.
                const double actual_seconds = (target_ts - min_ts) * av_q2d(st->time_base);
                ctx.played_samples.store(
                    llround(actual_seconds * ctx.codec_context->sample_rate),
                    std::memory_order_release
                );
                app_state.just_seeked.store(true, std::memory_order_release);
            } else {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                fprintf(stderr, "Seek failed: %s (code: %d)\n", err_buf, ret);
            }

            ctx.seek_req.store(false, std::memory_order_release);
        }

        if (!ctx.format_context || !ctx.codec_context) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        {
            std::lock_guard lock(ctx.mutex);

            AVPacket *packet = av_packet_alloc();
            AVFrame *frame = av_frame_alloc();

            // Read ONE packet and see if it is an error.
            if (const int err = av_read_frame(ctx.format_context, packet); err == AVERROR_EOF) {
                // fprintf(stderr, "EOF error encountered from av_read_frame\n");
                ctx.decode_EOF.exchange(true);
                av_packet_free(&packet);
                av_frame_free(&frame);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Process that ONE packet if it's audio
            if (packet->stream_index == ctx.audio_stream_index) {
                if (avcodec_send_packet(ctx.codec_context, packet) < 0) {
                    fprintf(stderr, "Error sending packet\n");
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                    av_frame_free(&frame);
                    continue;
                }

                // Decode all frames from this packet
                // From what I can see, this will decode *everything* from the song at once.
                while (avcodec_receive_frame(ctx.codec_context, frame) == 0) {
                    uint8_t *out_buffer[1];
                    int out_samples = av_rescale_rnd(
                        swr_get_delay(ctx.swr_context, ctx.codec_context->sample_rate) + frame->nb_samples,
                        ctx.codec_context->sample_rate, ctx.codec_context->sample_rate, AV_ROUND_UP);

                    out_buffer[0] = (uint8_t *) av_malloc(
                        out_samples * ctx.codec_context->ch_layout.nb_channels * sizeof(float));

                    int num_samples = swr_convert(ctx.swr_context, out_buffer, out_samples,
                                                  (const uint8_t **) frame->data, frame->nb_samples);

                    if (num_samples > 0) {
                        int audio_size = num_samples * ctx.codec_context->ch_layout.nb_channels * sizeof(float);
                        SDL_PutAudioStreamData(ctx.audio_stream, out_buffer[0], audio_size);
                        ctx.played_samples += frame->nb_samples;
                    }
                    double seconds = frame->best_effort_timestamp * av_q2d(ctx.codec_context->time_base);
                    //printf("decoding while loop: %lf\n", seconds);
                    //printf("%lu\n", ctx.played_samples.load());
                    av_free(out_buffer[0]);
                }
            }

            av_packet_unref(packet);
            av_packet_free(&packet);
            av_frame_free(&frame);
        }
    }
}

bool init_audio(audio_context_t &ctx, SDL_AudioSpec &spec) {
    ctx.audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!ctx.audio_stream) {
        fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(ctx.audio_stream));
    return true;
}

bool load_file(audio_context_t &ctx, const std::string &filepath) {
    std::lock_guard lock(ctx.mutex);
    // We will need this later on, so we do it here at the top now.
    const char *file = filepath.c_str();

    if (ctx.format_context) {
        avformat_close_input(&ctx.format_context);
    }

    if (ctx.codec_context) {
        avcodec_free_context(&ctx.codec_context);
    }

    if (ctx.swr_context) {
        swr_free(&ctx.swr_context);
    }

    // * We are now free to load the file
    if (avformat_open_input(&ctx.format_context, file, nullptr, nullptr) != 0) {
        fprintf(stderr, "Could not open file: %s\n", file);
        exit(1);
    }

    if (avformat_find_stream_info(ctx.format_context, nullptr) < 0) {
        fprintf(stderr, "Could not find stream info for: %s\n", file);
        exit(1);
    }

    int stream_index = -1;
    for (unsigned int i = 0; i < ctx.format_context->nb_streams; ++i) {
        if (ctx.format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
            break;
        }
    }

    if (stream_index == -1) {
        fprintf(stderr, "No audio stream found for: %s\n", file);
        exit(1);
    }

    AVStream *av_stream = ctx.format_context->streams[stream_index];
    AVCodecParameters *codec_parameters = av_stream->codecpar;

    const AVCodec *decoder = avcodec_find_decoder(codec_parameters->codec_id);

    ctx.codec_context = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(ctx.codec_context, codec_parameters);
    avcodec_open2(ctx.codec_context, decoder, nullptr);


    ctx.audio_stream_index = stream_index;

    ctx.swr_context = swr_alloc();

    av_opt_set_chlayout(ctx.swr_context, "in_chlayout", &ctx.codec_context->ch_layout, 0);
    av_opt_set_chlayout(ctx.swr_context, "out_chlayout", &ctx.codec_context->ch_layout, 0);

    av_opt_set_int(ctx.swr_context, "in_sample_rate", ctx.codec_context->sample_rate, 0);
    av_opt_set_int(ctx.swr_context, "out_sample_rate", ctx.codec_context->sample_rate, 0);

    av_opt_set_sample_fmt(ctx.swr_context, "in_sample_fmt", ctx.codec_context->sample_fmt, 0);
    av_opt_set_sample_fmt(ctx.swr_context, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

    //printf("%ld\n", ctx.format_context->duration / AV_TIME_BASE);

    swr_init(ctx.swr_context);

    ctx.should_stop = false;
    ctx.decode_EOF = false;
    ctx.played_samples = 0;
    return true;
}

void toggle_play_pause(audio_context_t &ctx) {
    ctx.is_paused = !ctx.is_paused;
    if (ctx.is_paused) {
        SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(ctx.audio_stream));
    } else {
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(ctx.audio_stream));
    }
}

void start_decoding_thread(audio_context_t &ctx, std::thread &thread, app_state_t &app_state) {
    thread = std::thread(decode_thread, std::ref(ctx), std::ref(app_state));
}

void stop_decoding(audio_context_t &ctx, std::thread &thread) {
    ctx.should_stop = true;
    if (thread.joinable()) {
        thread.join();
    }
}

void cleanup_audio(audio_context_t &ctx) {
    SDL_DestroyAudioStream(ctx.audio_stream);
    avcodec_free_context(&ctx.codec_context);
    avformat_close_input(&ctx.format_context);
    swr_free(&ctx.swr_context);
}

/*
 * Implementation for audio.h to handle audio streaming on background thread
 */
#include "audio.h"


// TODO: add checks for if ALL things are initialized, currently missing some and assuming they will work.
void decode_thread(audio_context_t &ctx) {
    while (!ctx.should_stop) {
        if (ctx.is_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        {
            std::lock_guard lock(ctx.mutex);

            AVPacket *packet = av_packet_alloc();
            AVFrame *frame = av_frame_alloc();

            // Read ONE packet
            if (av_read_frame(ctx.format_context, packet) < 0) {
                ctx.should_stop = true;
                av_packet_free(&packet);
                av_frame_free(&frame);
                break;
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
                    }
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

    swr_init(ctx.swr_context);

    ctx.should_stop = false;
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

void seek(audio_context_t &ctx, int seconds) {
    std::lock_guard lock(ctx.mutex);
    int64_t timestamp = seconds * AV_TIME_BASE;
    av_seek_frame(ctx.format_context, ctx.audio_stream_index, timestamp, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(ctx.codec_context);
    swr_init(ctx.swr_context);
}

void start_decoding_thread(audio_context_t &ctx, std::thread &thread) {
    thread = std::thread(decode_thread, std::ref(ctx));
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

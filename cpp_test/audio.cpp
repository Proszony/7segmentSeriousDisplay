#include "audio.h"
#include <iostream>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <SDL2/SDL.h>
}

using namespace std;

void run_audio(const string &filename, atomic<bool> &stop_flag) {
    avformat_network_init();

    // --------------------------
    // Open media file
    // --------------------------
    AVFormatContext *fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename.c_str(), nullptr, nullptr) != 0) {
        cerr << "Error: Cannot open file: " << filename << endl;
        return;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        cerr << "Error: Cannot find stream info" << endl;
        return;
    }

    int audio_stream_index = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        cerr << "Error: No audio stream found" << endl;
        return;
    }

    AVCodecParameters *codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        cerr << "Error: Unsupported codec" << endl;
        return;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        cerr << "Error: Could not open codec" << endl;
        return;
    }

    // --------------------------
    // SDL2 audio initialization
    // --------------------------
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        cerr << "Error: SDL_Init failed: " << SDL_GetError() << endl;
        return;
    }

    SDL_AudioSpec want_spec, have_spec;
    SDL_zero(want_spec);
    int channels = codec_ctx->ch_layout.nb_channels;
    want_spec.freq = codec_ctx->sample_rate;
    want_spec.format = AUDIO_S16SYS;
    want_spec.channels = channels;
    want_spec.samples = 1024;
    want_spec.callback = nullptr;

    // Use SDL_OpenAudioDevice to get a valid device ID
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want_spec, &have_spec, 0);
    if (dev == 0) {
        cerr << "Error: SDL_OpenAudioDevice failed: " << SDL_GetError() << endl;
        return;
    }

    // --------------------------
    // Setup resampler
    // --------------------------
    SwrContext *swr = nullptr;
    AVChannelLayout in_ch_layout, out_ch_layout;
    av_channel_layout_copy(&in_ch_layout, &codec_ctx->ch_layout);
    av_channel_layout_default(&out_ch_layout, channels);

    swr_alloc_set_opts2(&swr,
                        &out_ch_layout,
                        AV_SAMPLE_FMT_S16,
                        codec_ctx->sample_rate,
                        &in_ch_layout,
                        codec_ctx->sample_fmt,
                        codec_ctx->sample_rate,
                        0, nullptr);
    swr_init(swr);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    // Start audio playback
    SDL_PauseAudioDevice(dev, 0);

    // --------------------------
    // Main decoding + playback loop
    // --------------------------
    while (av_read_frame(fmt_ctx, packet) >= 0 && !stop_flag.load()) {
        if (packet->stream_index == audio_stream_index) {
            if (avcodec_send_packet(codec_ctx, packet) >= 0) {
                while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                    uint8_t *out[2] = {nullptr};
                    av_samples_alloc(out, nullptr, channels,
                                     frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

                    swr_convert(swr, out, frame->nb_samples,
                                (const uint8_t **)frame->data, frame->nb_samples);

                    int buffer_size = av_samples_get_buffer_size(nullptr, channels,
                                                                 frame->nb_samples, AV_SAMPLE_FMT_S16, 1);

                    SDL_QueueAudio(dev, out[0], buffer_size);
                    av_freep(&out[0]);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Wait for last audio to finish
    while (SDL_GetQueuedAudioSize(dev) > 0 && !stop_flag.load())
        SDL_Delay(10);

    // --------------------------
    // Cleanup
    // --------------------------
    SDL_CloseAudioDevice(dev);
    swr_free(&swr);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
}


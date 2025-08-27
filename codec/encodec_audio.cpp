extern "C" {
    #include <libavutil/log.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
}
#include <string>
using namespace std;

static int select_best_samplerate(const AVCodec *codec) {
    const int *p = codec->supported_samplerates;
    int best_samplerate = 0;
    if (!p) {
        return 44100;
    }
    while (*p) {
        // 找到最接近44100的采样率
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate)) {
            best_samplerate = *p;
        }
        p++;
    }
    return best_samplerate;
}

static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt) {
            return 1;
        }
        p++;
    }
    av_log(nullptr, AV_LOG_ERROR, "sample format %s not support\n", av_get_sample_fmt_name(sample_fmt));
    return 0;
}

static int encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *out) {
    int ret = -1;
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send frame to encoder error\n");
        goto _END;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            return -1;
        }
        fwrite(pkt->data, pkt->size, 1, out);
        av_packet_unref(pkt);
    }

_END:
    return 0;
}
/**
 * 参数1：输出文件名
 * 参数2：编码器名称，比如 libfdk-aac 暂时去掉
 */
int main(int argc, char* argv[]) {
    av_log_set_level(AV_LOG_DEBUG);
    string dst;
    // string codec_name;
    int ret = -1;
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    FILE *fp = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    uint16_t *samples = nullptr;
    float t = 0;
    float tincr = 0;
    AVChannelLayout stereo_layout;

    // 1. 输入参数
    if (argc < 3) {
        av_log(nullptr, AV_LOG_ERROR, "arguments must be more than 2\n");
        goto _ERROR;
    }

    dst = argv[1];
    // codec_name = argv[2];

    // 2. 查找编码器
    // codec = avcodec_find_encoder_by_name(codec_name.c_str());
    codec = avcodec_find_encoder_by_name("libfdk_aac");  // MAC默认不带这个，需要自己编译， 支持
    // codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        av_log(nullptr, AV_LOG_ERROR, "could not find encoder aac\n");
        goto _ERROR;
    }
    
    // 3. 创建编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate codec context\n");
        goto _ERROR;
    }

    // 4. 设置编码器参数
    codec_ctx->bit_rate = 64000;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, codec_ctx->sample_fmt)) {
        av_log(nullptr, AV_LOG_ERROR, "encoder does not support sample format %s\n", av_get_sample_fmt_name(codec_ctx->sample_fmt));
        goto _ERROR;
    }

    codec_ctx->sample_rate = select_best_samplerate(codec);
    av_channel_layout_copy(&codec_ctx->ch_layout, &stereo_layout);

    // 设置264编码器的特殊参数
    if (codec->id == AV_CODEC_ID_H264) {
        // priv_data是编码器私有数据
        // 通过它可以设置编码器私有参数
        // 这些参数在不同编码器中是不一样的
        av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);
    }

    // 5. 编码器与编码器上下文绑定到一起
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not open codec aac\n");
        goto _ERROR;
    }
    // 6. 创建输出文件
    fp = fopen(dst.c_str(), "wb+");
    if (!fp) {
        av_log(nullptr, AV_LOG_ERROR, "could not open file %s\n", dst.c_str());
        goto _ERROR;
    }

    // 7. 创建AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate frame\n");
        goto _ERROR;
    }
    frame->nb_samples = codec_ctx->frame_size; // 每个通道的采样点数
    frame->format = codec_ctx->sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, &codec_ctx->ch_layout);

    // 真正的数据分配，并与AVFrame绑定，后面设置为0，会自动根据cpu对齐
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate frame data\n");
        goto _ERROR;
    }

    // 8. 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate packet\n");
        goto _ERROR;
    }

    tincr = 2 * M_PI * 440.0 / codec_ctx->sample_rate; // 440hz
    // 9. 生成音频内容
    for (int i = 0; i < 200; i++) { // 200 * 1024 / 44100 = 4.6s
        // 设置数据
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "frame not writable\n");
            goto _ERROR;
        }

        samples = (uint16_t*)frame->data[0];
        for (int j = 0; j < codec_ctx->frame_size; j++) {
            samples[2*j] = (int)sin(t) * 10000;
            for (int k = 1; k < codec_ctx->ch_layout.nb_channels; k++) {
                samples[2*j + k] = samples[2*j];
            }
            t += tincr;
        }
        
        // 编码
        ret = encode(codec_ctx, frame, pkt, fp);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "encode error\n");
            goto _ERROR;
        }
    }
    // 10. 编码 输入空帧目的是输出队列中剩余的数据
    encode(codec_ctx, nullptr, pkt, fp);

_ERROR:
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
        codec_ctx = nullptr;
    }
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (pkt) { 
        av_packet_free(&pkt);
        pkt = nullptr;
    }

    return 0;
}
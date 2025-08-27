extern "C" {
    #include <libavutil/log.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
}
#include <string>
using namespace std;

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

int main(int argc, char* argv[]) {
    av_log_set_level(AV_LOG_DEBUG);
    string dst;
    string codec_name;
    int ret = -1;
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    FILE *fp = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    // 1. 输入参数
    if (argc < 3) {
        av_log(nullptr, AV_LOG_ERROR, "arguments must be more than 2\n");
        goto _ERROR;
    }

    dst = argv[1];
    codec_name = argv[2];

    // 2. 查找编码器
    codec = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!codec) {
        av_log(nullptr, AV_LOG_ERROR, "could not find encoder %s\n", codec_name.c_str());
        goto _ERROR;
    }
    
    // 3. 创建编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate codec context\n");
        goto _ERROR;
    }

    // 4. 设置编码器参数
    codec_ctx->width = 640;
    codec_ctx->height = 480;
    codec_ctx->bit_rate = 500000;

    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    codec_ctx->gop_size = 10; // 每10帧一个关键帧
    codec_ctx->max_b_frames = 1; // 一个gop允许1个B
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

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
        av_log(nullptr, AV_LOG_ERROR, "could not open codec %s\n", codec_name.c_str());
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
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    frame->format = codec_ctx->pix_fmt;

    // 真正的像素数据分配，并与AVFrame绑定，后面设置为0，会自动根据cpu对齐
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
    // 9. 生成视频内容
    for (int i = 0; i < 25; ++i) {
        // 确保frame是否可写
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            break;
        }

        // Y分量
        for (int y = 0; y < codec_ctx->height; y++) {
            for (int x = 0; x < codec_ctx->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }
        // UV分量
        for (int y = 0; y < codec_ctx->height / 2; y++) {
            for (int x = 0; x < codec_ctx->width / 2; x++) {
                // U分量：128是黑色
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                // V分量：64是黑色
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;

            }
        }

        frame->pts = i;
        // 10. 编码
        ret = encode(codec_ctx, frame, pkt, fp);

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
#include <iostream>
using namespace std;
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/log.h>
    #include <libavutil/avutil.h>
}

/** 
 * argv[0]: 可执行程序的路径
 * argv[1]: 源文件路径
 * argv[2]: 目的文件路径
 * */ 
int main(int argc, char* argv[]) {
    // 1. 处理参数
    string src;
    string dst;
    int ret = -1;
    int idx = -1;
    AVFormatContext *pFmtCtx = nullptr;
    AVFormatContext *oFmtCtx = nullptr;
    AVStream *outStream = nullptr;
    AVStream *inStream = nullptr;
    AVPacket pkt;

    av_log_set_level(AV_LOG_INFO);
    if (argc < 3) {
        av_log(nullptr, AV_LOG_ERROR, "arguments must be more than 2\n");
        exit(-1);
    }
    src = argv[1];
    dst = argv[2];

    // 2. 打开多媒体文件
    ret = avformat_open_input(&pFmtCtx, src.c_str(), nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }

    // 3. 从多媒体文件中找到视频流
    idx = av_find_best_stream(pFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (idx < 0) {
        av_log(pFmtCtx, AV_LOG_ERROR, "could not find audio stream in %s\n", src.c_str());
        goto _ERROR;
    }
    // 4. 打开目的文件的上下文
    oFmtCtx = avformat_alloc_context();
    if (!oFmtCtx) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate output format context\n");
        goto _ERROR;
    }
    oFmtCtx->oformat = av_guess_format(nullptr, dst.c_str(), nullptr);

    // 5. 为目的文件创建视频流
    outStream = avformat_new_stream(oFmtCtx, nullptr);
    // 6. 设置输出视频参数
    inStream = pFmtCtx->streams[idx];
    avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    outStream->codecpar->codec_tag = 0; // 设置成0是为了让ffmpeg自动识别编码器并重新设置codec_tag
    // 绑定
    ret = avio_open2(&oFmtCtx->pb, dst.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not open output file %s\n", dst.c_str());
        goto _ERROR;
    }
    // 7. 写多媒体文件头到目的文件
    ret = avformat_write_header(oFmtCtx, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not write header to output file %s\n", dst.c_str());
        goto _ERROR;
    }
    // 8. 读取视频帧，进行编码，写入目的文件
    while (av_read_frame(pFmtCtx, &pkt) >= 0) {
        if (pkt.stream_index == idx) {
            pkt.stream_index = outStream->index;
            pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, inStream->time_base, outStream->time_base);
            pkt.pos = -1;
            ret = av_interleaved_write_frame(oFmtCtx, &pkt);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "could not write frame to output file %s\n", dst.c_str());
                av_packet_unref(&pkt);
                goto _ERROR;
            }
            av_packet_unref(&pkt);
        } else {
            av_packet_unref(&pkt);
        }
    }
    // 9. 写多媒体文件尾到目的文件
    av_write_trailer(oFmtCtx);
    
    // 10. 释放资源，关闭文件
_ERROR:
    if (pFmtCtx) {
        avformat_close_input(&pFmtCtx);
        pFmtCtx = nullptr;
    }

    if (oFmtCtx) {
        if (oFmtCtx->pb) {
            avio_closep(&oFmtCtx->pb);
        }
        avformat_free_context(oFmtCtx);
        oFmtCtx = nullptr;
    }

    return 0;
}
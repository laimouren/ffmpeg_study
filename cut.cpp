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
 * argv[3]: 开始时间，单位秒
 * argv[4]: 结束时间，单位秒
 * */ 
int main(int argc, char* argv[]) {
    // 1. 处理参数
    string src;
    string dst;
    double start_time = 0;
    double end_time = 0;
    int ret = -1;
    int idx = 0;
    AVFormatContext *pFmtCtx = nullptr;
    AVFormatContext *oFmtCtx = nullptr;
    AVPacket pkt;
    int *stream_map = nullptr;
    int64_t *dts_start_time = nullptr;
    int64_t *pts_start_time = nullptr;


    av_log_set_level(AV_LOG_INFO);
    if (argc < 5) {
        av_log(nullptr, AV_LOG_ERROR, "arguments must be more than 4\n");
        exit(-1);
    }
    src = argv[1];
    dst = argv[2];
    start_time = atof(argv[3]);
    end_time = atof(argv[4]);
    if (start_time < 0 || end_time <= start_time) {
        av_log(nullptr, AV_LOG_ERROR, "arguments is invalid\n");
        exit(-1);
    }

    // 2. 打开多媒体文件
    ret = avformat_open_input(&pFmtCtx, src.c_str(), nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }

    // 4. 打开目的文件的上下文
    avformat_alloc_output_context2(&oFmtCtx, nullptr, nullptr, dst.c_str());
    if (!oFmtCtx) {
        av_log(nullptr, AV_LOG_ERROR, "could not create output context for %s\n", dst.c_str());
        goto _ERROR;
    }

    stream_map = static_cast<int *>(av_calloc(pFmtCtx->nb_streams, sizeof(int)));
    if (!stream_map) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate stream map\n");
        goto _ERROR;
    }   

    for (int i = 0; i < pFmtCtx->nb_streams; ++i) {
        AVStream *stream = pFmtCtx->streams[i];
        AVStream *outStream = nullptr;
        // 只处理音频、视频、字幕流，其他类型的流不处理
        if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                stream_map[i] = -1;
                continue;
        }
        stream_map[i] = idx++;

        // 5. 为目的文件创建视频流
        outStream = avformat_new_stream(oFmtCtx, nullptr);
        if (!outStream) {
            av_log(nullptr, AV_LOG_ERROR, "failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto _ERROR;
        }
        // 6. 设置输出视频参数
        avcodec_parameters_copy(outStream->codecpar, stream->codecpar);
        outStream->codecpar->codec_tag = 0; // 设置成0是为了让ffmpeg自动识别编码器并重新设置codec_tag
    
    }
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

    ret = av_seek_frame(pFmtCtx, -1, static_cast<int64_t>(start_time * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not seek to position %f\n", start_time);
        goto _ERROR;
    }

    dts_start_time = static_cast<int64_t *>(av_calloc(pFmtCtx->nb_streams, sizeof(int64_t)));
    pts_start_time = static_cast<int64_t *>(av_calloc(pFmtCtx->nb_streams, sizeof(int64_t)));

    for (int i = 0; i < pFmtCtx->nb_streams; ++i) {
        dts_start_time[i] = -1;
        pts_start_time[i] = -1;
    }

    // 8. 读取视频字幕音频包，进行编码，写入目的文件
    while (av_read_frame(pFmtCtx, &pkt) >= 0) {
        AVStream *inStream = pFmtCtx->streams[pkt.stream_index];
        AVStream *outStream = nullptr;

        if (dts_start_time[pkt.stream_index] == -1) {
            dts_start_time[pkt.stream_index] = pkt.dts;
        }
        if (pts_start_time[pkt.stream_index] == -1) {
            pts_start_time[pkt.stream_index] = pkt.pts;
        }
        if (av_q2d(inStream->time_base) * pkt.dts > end_time) {
            av_log(nullptr, AV_LOG_INFO, "demuxing succeeded. read total %lld packets\n", pkt.pts);
            av_packet_unref(&pkt);
            break;
        }

        if (pkt.stream_index < 0 || pkt.stream_index >= pFmtCtx->nb_streams ||
            stream_map[pkt.stream_index] < 0) {
                av_packet_unref(&pkt);
                continue;
        }
        pkt.pts = pkt.pts - pts_start_time[pkt.stream_index];
        pkt.dts = pkt.dts - dts_start_time[pkt.stream_index];
        if (pkt.pts < pkt.dts) {
            pkt.pts = pkt.dts;
        }

        pkt.stream_index = stream_map[pkt.stream_index];
        av_packet_rescale_ts(&pkt, inStream->time_base, oFmtCtx->streams[pkt.stream_index]->time_base);
        
        pkt.pos = -1;
        ret = av_interleaved_write_frame(oFmtCtx, &pkt);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "could not write frame to output file %s\n", dst.c_str());
            av_packet_unref(&pkt);
            goto _ERROR;
        }
        av_packet_unref(&pkt);

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

    if (stream_map) {
        av_freep(&stream_map);
        stream_map = nullptr;
    }
    if (dts_start_time) {
        av_freep(&dts_start_time);
        dts_start_time = nullptr;
    }
    if (pts_start_time) {
        av_freep(&pts_start_time);
        pts_start_time = nullptr;
    }

    return 0;
}
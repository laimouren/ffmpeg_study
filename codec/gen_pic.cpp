#include <iostream>
using namespace std;
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/log.h>
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}
#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t

#pragma pack(push, 1) // 强制1字节对齐
typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

static int savePic(unsigned char *buf, int linesize, int width, int height, string fileName) {
    FILE *fp = fopen(fileName.c_str(), "wb+");
    
    if (!fp) {
        av_log(nullptr, AV_LOG_ERROR, "could not open %s\n", fileName.c_str());
        return -1;
    }
    // 写入头部信息，固定值，这个是PGM格式的magic头
    fprintf(fp, "P5\n%d %d\n255\n", width, height);
    for (int i = 0; i < height; i++) {
        fwrite(buf + i * linesize, 1, width, fp);
    }
    fclose(fp);
    return 0;
}
static void saveBMP(SwsContext *sws_ctx, AVFrame *frame, int w, int h, string fileName) {
    int dataSize = w * h * 3;
    FILE *fp = nullptr;
    // 1. 先进行转换，将YUV frame转成BGR24 frame
    AVFrame *frameBGR = av_frame_alloc();
    frameBGR->width = w;
    frameBGR->height = h;
    frameBGR->format = AV_PIX_FMT_BGR24;
    av_frame_get_buffer(frameBGR, 0);
    
    sws_scale(sws_ctx, frame->data, frame->linesize, 0, 
        frame->height, frameBGR->data, frameBGR->linesize);

    // BMP规范：每行字节数必须 4 字节对齐
    int rowSize = (w * 3 + 3) & ~3;
    int imageSize = rowSize * h;

    // 2. 构造BITMAPINFOHEADER
    BITMAPINFOHEADER infoHeader;
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = w;
    infoHeader.biHeight = h * (-1);
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = 0;
    infoHeader.biClrImportant = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biPlanes = 1;

    // 3. 构造BITMAPFILEHEADER
    BITMAPFILEHEADER fileHeader = {0,};
    fileHeader.bfType = 0x4d42; // "BM"
    
    fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    // 4. 将数据写入
    fp = fopen(fileName.c_str(), "wb");
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, fp);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, fp);

    // 每行写入，注意 frameBGR->linesize[0] 可能大于 w*3
    uint8_t *srcData = frameBGR->data[0];
    for (int y = 0; y < h; y++) {
        fwrite(srcData + y * frameBGR->linesize[0], 1, w * 3, fp);

        // 如果 w*3 不是 4 的倍数，要补 padding
        uint8_t padding[3] = {0, 0, 0};
        fwrite(padding, 1, rowSize - w * 3, fp);
    }
    
    // fwrite(frameBGR->data[0], 1, dataSize, fp); // 不能直接这样写，因为BMP是每行都是4位对齐的，只能逐行写入，不够四位需要补零
    
    // 5. 释放资源
    fclose(fp);
    av_freep(&frameBGR->data[0]);
    av_free(frameBGR);
}

static int decode(AVCodecContext *ctx, SwsContext *sws_ctx, AVFrame *frame, AVPacket *pkt, string fileName) {
    int ret = -1;
    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send frame to encoder error\n");
        goto _END;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            return -1;
        }
        string buf = "";
        buf += fileName;
        buf += "-";
        buf += to_string(ctx->frame_num);
        buf += ".bmp";

        // saveBMP(sws_ctx, frame, frame->width, frame->height, buf);
        savePic(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
        if (pkt) {
            av_packet_unref(pkt);
        }
    }

_END:
    return 0;
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
    AVStream *inStream = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    SwsContext *sws_ctx = nullptr;

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

    inStream = pFmtCtx->streams[idx];
    // 4. 查找解码器
    codec = avcodec_find_decoder(inStream->codecpar->codec_id);
    if (!codec) {
        av_log(nullptr, AV_LOG_ERROR, "could not find encoder x264\n");
        goto _ERROR;
    }
    
    // 5. 创建解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate codec context\n");
        goto _ERROR;
    }

    // 6. 根据流参数，设置解码器上下文
    avcodec_parameters_to_context(codec_ctx, inStream->codecpar);

    // 7. 解码器与解码器上下文绑定到一起
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "could not open codec x264\n");
        goto _ERROR;
    }

    // 7.1 获得SWSContext
    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P, 
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, nullptr, nullptr, nullptr);

    // 8. 创建AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate frame\n");
        goto _ERROR;
    }

    // 9. 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(nullptr, AV_LOG_ERROR, "could not allocate packet\n");
        goto _ERROR;
    }
   
    
    // 10. 读取视频帧，进行编码，写入目的文件
    while (av_read_frame(pFmtCtx, pkt) >= 0) {
        if (pkt->stream_index == idx) {
            decode(codec_ctx, sws_ctx, frame, pkt, dst);
        }
        av_packet_unref(pkt);
    }
    decode(codec_ctx, sws_ctx, frame, nullptr, dst); // 这里传nullptr，表示刷新编码器
    
    // 11. 释放资源，关闭文件
_ERROR:
    if (pFmtCtx) {
        avformat_close_input(&pFmtCtx);
        pFmtCtx = nullptr;
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

#pragma pack(pop) // 恢复对齐设置
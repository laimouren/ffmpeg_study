extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/log.h>
}

int main() {
    int ret;
    AVFormatContext *fmt_ctx = nullptr;
    av_log_set_level(AV_LOG_INFO);
    
    // av_register_all(); ffmpeg 4.0 and later do not require this
    ret = avformat_open_input(&fmt_ctx, "./input.mp4", nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Could not open input file: %s\n", av_err2str(ret));
        return ret;
    }

    av_dump_format(fmt_ctx, 0, "./input.mp4", 0);

    avformat_close_input(&fmt_ctx);

    return 0;
}
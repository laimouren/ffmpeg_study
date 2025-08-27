#include <iostream>
#include <inttypes.h>
extern "C" {
    #include <libavutil/log.h>
    #include <libavformat/avformat.h>
}

int main(void) {

    int ret;
    AVIODirContext *ctx = nullptr;
    AVIODirEntry *entry = nullptr;
    av_log_set_level(AV_LOG_INFO);

    ret = avio_open_dir(&ctx, "./", nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Could not open directory: %s\n", av_err2str(ret));
        return ret;
    }

    while (1) {
        ret = avio_read_dir(ctx, &entry);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not read directory: %s\n", av_err2str(ret));
            
            break;
        }
        if (!entry) {
            break; // No more entries
        }
        av_log(nullptr, AV_LOG_INFO, "Entry type: %d\n", entry->type);

        av_log(nullptr, AV_LOG_INFO, "%12"PRId64" %s\n", entry->size, entry->name);
        avio_free_directory_entry(&entry);
    }

    avio_close_dir(&ctx);
    return 0;
}
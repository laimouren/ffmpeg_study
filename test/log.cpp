#include <iostream>
extern "C" {
#include <libavutil/log.h>
}

int main() {
    // Set the log level to AV_LOG_DEBUG
    av_log_set_level(AV_LOG_DEBUG);

    // Log a message at the INFO level
    av_log(nullptr, AV_LOG_INFO, "This is an informational message.\n");

    // Log a message at the WARNING level
    av_log(nullptr, AV_LOG_WARNING, "This is a warning message.\n");

    // Log a message at the ERROR level
    av_log(nullptr, AV_LOG_ERROR, "This is an error message.\n");

    return 0;
}
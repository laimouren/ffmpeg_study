#include <SDL.h>
#include <fstream>
using namespace std;
#define BLOCK_SIZE 4096000

static Uint8 *audioBuf = NULL;
static Uint8 *audioPos = NULL;
static size_t bufferLen = 0;

// 音频回调函数
void audio_callback(void *userdata, Uint8 *stream, int len) {
    if (bufferLen == 0) {
        return;
    }

    SDL_memset(stream, 0, len); // 先将stream清0

    len = (len > bufferLen ? bufferLen : len);
    // 混音，将解码后的音频数据混合到stream
    SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);

    audioPos += len;
    bufferLen -= len;
}

int main(int argc, char* argv[]) {
    int res = -1;
    fstream fs("output.pcm", ios::in | ios::binary);
    if (!fs.is_open()) {
        printf("open file failed\n");
        return -1;
    }

    SDL_AudioSpec spec;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // 申请音频缓冲区
    audioBuf = new Uint8[BLOCK_SIZE];
    if (!audioBuf) {
        printf("new audioBuf failed\n");
        goto __FAIL;
    }

    // 设置音频参数
    spec.freq = 44100;               // 采样率
    spec.format = AUDIO_S16SYS;      // 采样格式
    spec.channels = 2;                // 声道数
    spec.silence = 0;                 // 静音数据
    spec.samples = 1024;              // 采样点数
    spec.callback = audio_callback;   // 音频回调函数
    spec.userdata = nullptr;           // 传递给回调函数的参数

    // 打开音频设备
    if (SDL_OpenAudio(&spec, nullptr) < 0) {
        printf("SDL_OpenAudio failed: %s\n", SDL_GetError());
        goto __FAIL;
    }

    // 开始播放
    SDL_PauseAudio(0);

    do {
        // 从文件读取数据到缓冲区
        fs.read((char*)audioBuf, BLOCK_SIZE);
        bufferLen = fs.gcount();
        if (bufferLen <= 0) {
            printf("file read end, exit\n");
            break;
        }

        audioPos = audioBuf;

        // 等待数据播放完
        while (audioPos < (audioBuf + BLOCK_SIZE)) {
            SDL_Delay(1);
        }
    } while (bufferLen != 0);
    SDL_CloseAudio();
    res = 0;

__FAIL:
    if (audioBuf) {
        delete[] audioBuf;
        audioBuf = nullptr;
    }
    SDL_Quit();
    return res;
}
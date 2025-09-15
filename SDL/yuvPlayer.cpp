#include <SDL.h>
#include <fstream>
using namespace std;

#define BLOCK_SIZE 4096000 // 4MB
//event message
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
#define QUIT_EVENT  (SDL_USEREVENT + 2)

int thread_exit = 0;
int refreshVideoTimer(void *updata) {
    thread_exit = 0;

    while (!thread_exit) {
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(40); // 25fps
    }

    thread_exit = 0;

    // 推送退出消息
    SDL_Event event;
    event.type = QUIT_EVENT;
    SDL_PushEvent(&event);

    return 0;
}
int main(int argc, char* argv[]) {
    fstream fs("output.yuv", ios::in | ios::binary);
    if (!fs.is_open()) {
        printf("open file failed\n");
        return -1;
    }   

    SDL_Event event;
    SDL_Rect rect;
    Uint32 pixformat = 0;

    SDL_Window *window = nullptr;
    SDL_Renderer *render = nullptr;
    SDL_Texture *texture = nullptr;
    SDL_Thread *timerThread = nullptr;

    int wWidth = 640;
    int wHeight = 480;

    const int videoWidth = 608;
    const int videoHeight = 368;
    
    Uint8 *videoPos = nullptr;
    Uint8 *videoEnd = nullptr;

    unsigned int remainLen = 0;
    unsigned int videoBuffLen = 0;
    unsigned int blankSpaceLen = 0;
    Uint8 videoBuf[BLOCK_SIZE] = {0};

    // 只有yuv420p才这样计算
    // y:w*h, u:w*h/4, v:w*h/4
    // yuv420p: 1.5 bytes per pixel
    const unsigned int yuvFrameLen = videoWidth * videoHeight * 12 / 8;
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow("yuv player", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        wWidth, wHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto __FAIL;
    }

    render = SDL_CreateRenderer(window, -1, 0);

    //IYUV: Y + U + V  (3 planes)
    //YV12: Y + V + U  (3 planes)
    pixformat = SDL_PIXELFORMAT_IYUV; // YUV420P

    // 创建纹理 SDL_TEXTUREACCESS_STREAMING去了解一下learn_OPENGL里有提及
    texture = SDL_CreateTexture(render, pixformat, 
        SDL_TEXTUREACCESS_STREAMING, videoWidth, videoHeight);

    
    // 从文件读取数据到缓冲区
    fs.read((char*)videoBuf, BLOCK_SIZE);
    videoBuffLen = fs.gcount();

    // 设置视频指针位置
    videoPos = videoBuf;
    videoEnd = videoBuf + videoBuffLen;

    blankSpaceLen = BLOCK_SIZE - videoBuffLen;

    timerThread = SDL_CreateThread(refreshVideoTimer, nullptr, nullptr);
    if (!timerThread) {
        printf("SDL_CreateThread failed: %s\n", SDL_GetError());
        goto __FAIL;
    }

    do {
        // wait event
        SDL_WaitEvent(&event);
        if (event.type == REFRESH_EVENT) {
            // 不够数据展示
            if (videoPos + yuvFrameLen > videoEnd) {
                // 将剩余数据移到缓冲区头部
                remainLen = videoEnd - videoPos;
                if (remainLen > 0 && !blankSpaceLen) {
                    memcpy(videoBuf, videoPos, remainLen);
                    blankSpaceLen = BLOCK_SIZE - remainLen;
                    videoPos = videoBuf;
                    videoEnd = videoBuf + remainLen;
                }

                // 表示数据已经读完
                if (videoEnd == videoBuf + BLOCK_SIZE) {
                    videoPos = videoBuf;
                    videoEnd = videoBuf;
                    blankSpaceLen = BLOCK_SIZE;
                }

                // 从文件读取数据到缓冲区
                fs.read((char*)(videoEnd), blankSpaceLen);
                videoBuffLen = fs.gcount();
                if (videoBuffLen <= 0) {
                    printf("file read end, exit thread\n");
                    thread_exit = 1;
                    continue; // 等待waitEvent退出
                }

                videoEnd += videoBuffLen;
                blankSpaceLen -= videoBuffLen;

                printf("not enought data: pos:%p, video_end:%p, blank_space_len:%d\n", videoPos, videoEnd, blankSpaceLen);
            }

            SDL_UpdateTexture(texture, nullptr, videoPos, videoWidth);
            // 如果窗口resize
            rect.x = 0;
            rect.y = 0;
            rect.w = wWidth;
            rect.h = wHeight;

            SDL_RenderClear(render);
            SDL_RenderCopy(render, texture, nullptr, &rect);
            SDL_RenderPresent(render);

            printf("not enought data: pos:%p, video_end:%p, blank_space_len:%d\n", videoPos, videoEnd, blankSpaceLen);
            videoPos += yuvFrameLen;
        } else if (event.type == SDL_WINDOWEVENT) {
            // 窗口大小改变
            SDL_GetWindowSize(window, &wWidth, &wHeight);
        } else if (event.type == SDL_QUIT) {
            thread_exit = 1;
        } else if (event.type == QUIT_EVENT) {
            printf("receive quit event, exit\n");
            break;
        }
    } while (1);

__FAIL:
    if (timerThread) {
        SDL_WaitThread(timerThread, nullptr);
        timerThread = nullptr;
    }
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (render) {
        SDL_DestroyRenderer(render);
        render = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
    fs.close();

    return 0;
}
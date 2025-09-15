#include <SDL.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("hello sdl", 200, 200, 640, 480, SDL_WINDOW_SHOWN);
    SDL_Renderer* render;
    SDL_Event event;
    SDL_Texture* texture;
    SDL_Rect rect;
    rect.w = 30;
    rect.h = 30;
    int quit = 1;
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        goto __EXIT;
    }

    render = SDL_CreateRenderer(window, -1, 0);
    if (!render) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        goto __EXIT;
    }
      
    // SDL_PollEvent(&event); // 没有事件处理就不会显示窗口

    SDL_SetRenderDrawColor(render, 255, 10, 0, 255); // 必须在RenderClear之前设置颜色

    SDL_RenderClear(render);

    SDL_RenderPresent(render);  // 显示

    texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 640, 480);
    if (!texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        goto __EXIT;
    }
    do {
        SDL_PollEvent(&event);  // 将waitEvent改成PollEvent就不会阻塞, 因为PollEvent会轮询事件
        switch (event.type)
        {
        case SDL_QUIT:
            quit = 0;
            break;
        
        default:
            SDL_Log("event type: %d", event.type);
            break;
        }

    
        rect.x = (rand() % (640 - rect.w));
        rect.y = (rand() % (480 - rect.h));

        SDL_SetRenderTarget(render, texture);   // 设置渲染目标为纹理
        SDL_SetRenderDrawColor(render, 0, 0, 0, 0);
        SDL_RenderClear(render);

        SDL_RenderDrawRect(render, &rect);      // 绘制矩形边框
        SDL_SetRenderDrawColor(render, 255, 0, 0, 0);
        SDL_RenderFillRect(render, &rect);      // 填充矩形

        SDL_SetRenderTarget(render, NULL);      // 恢复渲染目标为默认
        SDL_RenderCopy(render, texture, NULL, NULL); // 将纹理复制到渲染器

        SDL_RenderPresent(render);  // 显示
    } while(quit);

    // SDL_Delay(300000);


__EXIT:
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(texture);
    SDL_Quit();

    return 0;
}
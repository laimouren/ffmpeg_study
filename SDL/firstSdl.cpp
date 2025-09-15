#include <SDL.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("hello sdl", 200, 200, 640, 470, SDL_WINDOW_SHOWN);
    SDL_Renderer* render;
    SDL_Event event;
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        goto __EXIT;
    }

    render = SDL_CreateRenderer(window, -1, 0);
    if (!render) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        goto __EXIT;
    }
      
    SDL_PollEvent(&event); // 没有事件处理就不会显示窗口

    SDL_SetRenderDrawColor(render, 255, 10, 0, 255); // 必须在RenderClear之前设置颜色

    SDL_RenderClear(render);

    SDL_RenderPresent(render);  // 显示

    SDL_Delay(300000);


__EXIT:
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
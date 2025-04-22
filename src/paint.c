#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

typedef enum {
    NONE,
    PROTECTED,
    WEAK,
} Pixel_state;

int img_width, img_height;

void paint_circle(int center_x, int center_y, int radius, Pixel_state state, Pixel_state* states) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                int x = center_x + dx;
                int y = center_y + dy;
                if (x >= 0 && x < img_width && y >= 0 && y < img_height) {
                    states[y * img_width + x] = state;
                }
            }
        }
    }
}

void painter(const char* image_path, Pixel_state* states) {

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init Error: %s", IMG_GetError());
        SDL_Quit();
        return;
    }

    SDL_Surface* img_surface = IMG_Load(image_path);
    if (!img_surface) {
        SDL_Log("Unable to create fallback surface: %s", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return;
    }

    img_width = img_surface->w;
    img_height = img_surface->h;

    // Create a window with an initial resolution of 800x600 that is resizable.
    SDL_Window* window = SDL_CreateWindow(
        "Image Painter",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        free(states);
        SDL_FreeSurface(img_surface);
        IMG_Quit();
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        free(states);
        SDL_FreeSurface(img_surface);
        IMG_Quit();
        SDL_Quit();
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer, img_width, img_height);

    SDL_Texture* img_texture = SDL_CreateTextureFromSurface(renderer, img_surface);
    SDL_FreeSurface(img_surface);
    if (!img_texture) {
        SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        free(states);
        IMG_Quit();
        SDL_Quit();
        return;
    }

    bool running = true;
    bool eraser_mode = false;
    int radius = 10;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                eraser_mode = true;
            } else if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE) {
                eraser_mode = false;
            }

            SDL_Rect viewport;
            SDL_RenderGetViewport(renderer, &viewport);

            float scale_x = (float)img_width / viewport.w;
            float scale_y = (float)img_height / viewport.h;

            // When a mouse event occurs, first convert its position relative to the viewport.
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION) {
                int win_x, win_y;

                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    win_x = event.button.x;
                    win_y = event.button.y;
                } else { // SDL_MOUSEMOTION
                    win_x = event.motion.x;
                    win_y = event.motion.y;
                }

                if (win_x < 0 || win_y < 0 || win_x >= viewport.w || win_y >= viewport.h)
                    continue;

                int image_x = win_x * scale_x;
                int image_y = win_y * scale_y;

                if (eraser_mode) {
                    paint_circle(image_x, image_y, radius, NONE, states);
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    paint_circle(image_x, image_y, radius, PROTECTED, states);
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                    paint_circle(image_x, image_y, radius, WEAK, states);
                } else if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    paint_circle(image_x, image_y, radius, PROTECTED, states);
                } else if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                    paint_circle(image_x, image_y, radius, WEAK, states);
                }
            }

            if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0) {
                    radius += 2;
                } else if (event.wheel.y < 0) {
                    radius = radius > 2 ? radius - 2 : 2;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_KP_PLUS) {
                    radius += 2;
                } else if (event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS) {
                    radius = radius > 2 ? radius - 2 : 2;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, img_texture, NULL, NULL);

        for (int y = 0; y < img_height; ++y) {
            for (int x = 0; x < img_width; ++x) {
                int index = y * img_width + x;
                if (states[index] == PROTECTED) {
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 128);
                    SDL_RenderDrawPoint(renderer, x, y);
                } else if (states[index] == WEAK) {
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 128);
                    SDL_RenderDrawPoint(renderer, x, y);
                }
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(img_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

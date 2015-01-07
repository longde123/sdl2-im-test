// vim: set ts=4 sw=4 sts=4 et:

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct TextField {
    SDL_Rect rect;
    char *text;
    size_t text_length;
    int text_width;
    char *composition;
    size_t composition_length;
};

struct Screen {
    SDL_Renderer *renderer;
    TTF_Font *font;
    struct TextField field;
};

static void
dump_textediting(SDL_TextEditingEvent *e) {
    printf("{timestamp=%d, \"%s\", start=%d, length=%d}\n",
           e->timestamp, e->text, e->start, e->length);
}

static void
exit_handler() {
    TTF_Quit();
    SDL_Quit();
}

static void
draw_text(struct SDL_Renderer *renderer, SDL_Point *position,
          TTF_Font *font, const char *text, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        fputs("FAIL: TTF_RenderUTF8_Blended failed.", stderr);
        fprintf(stderr, "    \"%s\"\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int texw, texh;
    SDL_QueryTexture(texture, NULL, NULL, &texw, &texh);
    SDL_Rect dest = {position->x, position->y, texw, texh};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void
redraw(const struct Screen *screen) {
    // ** background **

    SDL_SetRenderDrawColor(screen->renderer, 0, 64, 232, 255);
    SDL_RenderClear(screen->renderer);

    // ** text field **

    // border
    SDL_SetRenderDrawColor(screen->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(screen->renderer, &screen->field.rect);

    // actual text
    SDL_Color color_white = {255, 255, 255};
    if (screen->field.text[0]) {
        draw_text(screen->renderer, (SDL_Point*)&screen->field.rect,
                  screen->font, screen->field.text, color_white);
    }

    int caretx = screen->field.rect.x + screen->field.text_width;

    // composition text
    if (screen->field.composition[0]) {
        int width;
        TTF_SizeUTF8(screen->font, screen->field.composition, &width, NULL);

        // text
        SDL_Point pos = {screen->field.rect.x + screen->field.text_width,
                         screen->field.rect.y};
        draw_text(screen->renderer, &pos, screen->font,
                  screen->field.composition, color_white);

        // underline
        int x = screen->field.rect.x + screen->field.text_width;
        int h = screen->field.rect.y + TTF_FontHeight(screen->font);
        SDL_RenderDrawLine(screen->renderer, x, h, x + width, h);

        caretx += width;
    }

    // caret
    SDL_RenderDrawLine(screen->renderer,
                       caretx, screen->field.rect.y + 1,
                       caretx, screen->field.rect.y + screen->field.rect.h - 1);

    SDL_RenderPresent(screen->renderer);
}

const char *TTF_PATH = "ume-tgo4.ttf";

int
main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    atexit(exit_handler);

    SDL_Window *window = SDL_CreateWindow("SDL2 CJK Input", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 640, 240,
                                          SDL_WINDOW_OPENGL);
    if (!window) {
        fputs("FAIL: Failed to create the window.", stderr);
        exit(EXIT_FAILURE);
    }

    struct Screen screen;
    screen.renderer = SDL_CreateRenderer(window, -1, 0);
    screen.font = TTF_OpenFont(TTF_PATH, 20);
    if (!screen.font) {
        fprintf(stderr, "FAIL: Couldn't open %s.", TTF_PATH);
        exit(EXIT_FAILURE);
    }
    screen.field.rect = ((struct SDL_Rect){ 20, 20, 600, 24 });
    screen.field.text = calloc(128, sizeof(char));
    //strcpy(screen.field.text, "あーあ。");
    screen.field.text_length = strlen(screen.field.text);
    TTF_SizeUTF8(screen.font, screen.field.text,
                 &screen.field.text_width, NULL);
    screen.field.composition = calloc(128, sizeof(char));;
    screen.field.composition_length = 0;

    redraw(&screen);

    SDL_StartTextInput();
    SDL_SetTextInputRect(&screen.field.rect);
    for (;;) {
        SDL_Event e;
        int ok = SDL_WaitEvent(&e);
        if (!ok) {
            fputs("FAIL: SDL_WaitEvent failed.", stderr);
            exit(EXIT_FAILURE);
        }
        switch (e.type) {
        case SDL_QUIT:
            goto out;
        case SDL_TEXTEDITING: {
            size_t len = strlen(e.edit.text);
            if (!e.edit.start) {
                assert(len < 127);
                strcpy(screen.field.composition, e.edit.text);
                screen.field.composition_length = len;
            } else {
                assert(screen.field.composition_length + len < 127);
                strcpy(screen.field.composition + screen.field.composition_length,
                       e.edit.text);
                screen.field.composition_length += len;
            }
            dump_textediting(&e.edit);
            redraw(&screen);
            break;
        }
        case SDL_TEXTINPUT: {
            if (screen.field.composition[0]) {
                screen.field.composition[0] = '\0';
                screen.field.composition_length = 0;
            }
            strcat(screen.field.text, e.text.text);
            screen.field.text_length += strlen(screen.field.text);
            TTF_SizeUTF8(screen.font, screen.field.text,
                         &screen.field.text_width, NULL);

            SDL_Rect rect;
            rect.x = screen.field.rect.x + screen.field.text_width;
            rect.y = screen.field.rect.y;
            rect.w = screen.field.rect.w - screen.field.text_width;
            rect.h = screen.field.rect.h;
            SDL_SetTextInputRect(&rect);

            redraw(&screen);
            break;
        }
        };
    }
out:
    SDL_StopTextInput();

    TTF_CloseFont(screen.font);
    SDL_DestroyRenderer(screen.renderer);
    SDL_DestroyWindow(window);
    exit(EXIT_SUCCESS);
}

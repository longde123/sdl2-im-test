// vim: set ts=4 sw=4 sts=4 et:

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unicode/utf8.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct TextField {
    SDL_Rect rect;
    int padding;
    TTF_Font* font;

    char *text;
    size_t text_bufsize;
    size_t text_len;
    int text_width;

    char *composition;
    size_t composition_bufsize;
    size_t composition_length;
};

static void
text_field_update_text_info(struct TextField* field) {
    TTF_SizeUTF8(field->font, field->text, &field->text_width, NULL);

    SDL_Rect rect;
    rect.x = field->rect.x + field->text_width;
    rect.y = field->rect.y;
    rect.w = 1;
    rect.h = field->rect.h;
    SDL_SetTextInputRect(&rect);
}

static void
text_field_create(int x, int y, int width, int height,
                  TTF_Font *font, struct TextField *field) {
    field->rect = ((SDL_Rect){ x, y, width, height });
    field->padding = 2;
    if (!height) {
        field->rect.h = TTF_FontHeight(font) + field->padding * 2;
    }
    field->font = font;
    field->text_bufsize = 128;
    field->text = calloc(field->text_bufsize, sizeof(char));
    field->text_len = strlen(field->text);
    field->text_width = 0;
    field->composition_bufsize = 128;
    field->composition = calloc(field->composition_bufsize, sizeof(char));
    field->composition_length = 0;
    text_field_update_text_info(field);
}

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

    SDL_Point inner_pos = {screen->field.rect.x + screen->field.padding,
                           screen->field.rect.y + screen->field.padding};

    // actual text
    SDL_Color color_white = {255, 255, 255};
    if (screen->field.text[0]) {
        draw_text(screen->renderer, &inner_pos,
                  screen->font, screen->field.text, color_white);
    }

    int caretx = inner_pos.x + screen->field.text_width;

    // composition text
    if (screen->field.composition[0]) {
        int width;
        TTF_SizeUTF8(screen->font, screen->field.composition, &width, NULL);

        // text
        SDL_Point pos = {inner_pos.x + screen->field.text_width, inner_pos.y};
        draw_text(screen->renderer, &pos, screen->font,
                  screen->field.composition, color_white);

        // underline
        int x = inner_pos.x + screen->field.text_width;
        int h = inner_pos.y + TTF_FontHeight(screen->font);
        SDL_RenderDrawLine(screen->renderer, x, h, x + width, h);

        caretx += width;
    }

    // caret
    int bottom = screen->field.rect.y + screen->field.rect.h;
    SDL_RenderDrawLine(screen->renderer,
                       caretx, inner_pos.y,
                       caretx, bottom - screen->field.padding);

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

    text_field_create(20, 20, 600, 0, screen.font, &screen.field);
    redraw(&screen);

    SDL_StartTextInput();
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
            screen.field.text_len = strlen(screen.field.text);
            text_field_update_text_info(&screen.field);
            redraw(&screen);
            break;
        }
        case SDL_KEYUP:
            switch (e.key.keysym.sym) {
            case SDLK_BACKSPACE:
                U8_BACK_1(screen.field.text, 0, screen.field.text_len);
                screen.field.text[screen.field.text_len] = '\0';
                text_field_update_text_info(&screen.field);
                redraw(&screen);
                break;
            }
            break;
        };
    }
out:
    SDL_StopTextInput();

    TTF_CloseFont(screen.font);
    SDL_DestroyRenderer(screen.renderer);
    SDL_DestroyWindow(window);
    exit(EXIT_SUCCESS);
}

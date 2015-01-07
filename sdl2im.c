// vim: set ts=4 sw=4 sts=4 et:

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unicode/utf8.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef void (*UIElementOp_Render)(void *this, SDL_Renderer *renderer);

struct UIElementOperations {
    UIElementOp_Render render;
};

struct UIElementHeader {
    struct UIElementOperations *op;
    struct UIElementHeader *kids;
    struct UIElementHeader *siblings;
};

struct Screen {
    struct UIElementHeader header;

    SDL_Renderer *renderer;
};

struct TextField {
    struct UIElementHeader header;

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
text_field_op_render(struct TextField *this, SDL_Renderer *renderer) {
    // border
    SDL_SetRenderDrawColor(renderer, 96, 96, 96, 255);
    SDL_RenderDrawRect(renderer, &this->rect);

    SDL_Color text_color = {0, 0, 0};
    SDL_Point inner_pos = {this->rect.x + this->padding,
                           this->rect.y + this->padding};

    // actual text
    if (this->text[0]) {
        draw_text(renderer, &inner_pos, this->font, this->text, text_color);
    }

    // composition text
    int composition_width = 0;
    if (this->composition[0]) {
        SDL_Point pos = {inner_pos.x + this->text_width, inner_pos.y};
        draw_text(renderer, &pos, this->font,
                  this->composition, text_color);

        TTF_SizeUTF8(this->font, this->composition, &composition_width, NULL);

        int x = inner_pos.x + this->text_width;
        int h = inner_pos.y + TTF_FontHeight(this->font);
        SDL_RenderDrawLine(renderer, x, h, x + composition_width, h);
    }

    // caret
    int x = inner_pos.x + this->text_width + composition_width;
    int bottom = this->rect.y + this->rect.h;
    SDL_RenderDrawLine(renderer, x, inner_pos.y, x, bottom - this->padding);
}

static void
text_field_update_text_info(struct TextField *this) {
    TTF_SizeUTF8(this->font, this->text, &this->text_width, NULL);

    SDL_Rect rect;
    rect.x = this->rect.x + this->text_width;
    rect.y = this->rect.y;
    rect.w = 1;
    rect.h = this->rect.h;
    SDL_SetTextInputRect(&rect);
}

static struct TextField *
text_field_create(struct UIElementHeader *parent, int x, int y,
                  int width, int height, TTF_Font *font) {
    static struct UIElementOperations op = {
        .render = (UIElementOp_Render)text_field_op_render
    };

    struct TextField *this = malloc(sizeof(struct TextField));
    if (!this) {
        return NULL;
    }
    this->header = ((struct UIElementHeader){&op, NULL, parent->kids});
    parent->kids = this;

    this->rect = ((SDL_Rect){ x, y, width, height });
    this->padding = 2;
    if (!height) {
        this->rect.h = TTF_FontHeight(font) + this->padding * 2;
    }
    this->font = font;
    this->text_bufsize = 128;
    this->text = calloc(this->text_bufsize, sizeof(char));
    this->text_len = strlen(this->text);
    this->text_width = 0;
    this->composition_bufsize = 128;
    this->composition = calloc(this->composition_bufsize, sizeof(char));
    this->composition_length = 0;
    text_field_update_text_info(this);
    return this;
}

static void
text_field_destroy(struct TextField *this) {
    free(this->text);
    free(this->composition);
    free(this);
}

static void
screen_op_render(struct Screen *this, SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);
}

static struct Screen *
screen_create(SDL_Renderer *renderer) {
    static struct UIElementOperations op = {
        .render = (UIElementOp_Render)screen_op_render
    };

    struct Screen *this = malloc(sizeof(struct Screen));
    if (!this) {
        return NULL;
    }
    this->header = ((struct UIElementHeader){&op, NULL, NULL});
    this->renderer = renderer;
    return this;
}

static void
screen_destroy(struct Screen *this) {
    free(this);
}

static void
render(struct UIElementHeader *element, SDL_Renderer *renderer) {
    struct UIElementHeader *sibling;

    element->op->render(element, renderer);
    if (element->kids) {
        render(element->kids, renderer);
    }
    while ((sibling = element->siblings)) {
        render(sibling, renderer);
    }
}

static void
screen_render(struct Screen *screen) {
    render((struct UIElementHeader *)screen, screen->renderer);
    SDL_RenderPresent(screen->renderer);
}

static void
exit_handler() {
    TTF_Quit();
    SDL_Quit();
}

static const char *TTF_PATH = "ume-tgo4.ttf";

int
main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    atexit(exit_handler);

    SDL_Window *window =
        SDL_CreateWindow("SDL2 Japanese Input",
                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         640, 240, SDL_WINDOW_OPENGL);
    if (!window) {
        fputs("FAIL: Failed to create the window.", stderr);
        exit(EXIT_FAILURE);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    TTF_Font *font = TTF_OpenFont(TTF_PATH, 20);
    if (!font) {
        fprintf(stderr, "FAIL: Couldn't open %s.", TTF_PATH);
        exit(EXIT_FAILURE);
    }
    struct Screen *screen = screen_create(renderer);
    struct TextField *field =
        text_field_create((struct UIElementHeader *)screen,
                          20, 20, 600, 0, font);
    screen_render(screen);

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
                strcpy(field->composition, e.edit.text);
                field->composition_length = len;
            } else {
                assert(field->composition_length + len < 127);
                strcpy(field->composition + field->composition_length,
                       e.edit.text);
                field->composition_length += len;
            }
            printf("{timestamp=%d, \"%s\", start=%d, length=%d}\n",
                   e.edit.timestamp, e.edit.text, e.edit.start, e.edit.length);
            screen_render(screen);
            break;
        }
        case SDL_TEXTINPUT: {
            if (field->composition[0]) {
                field->composition[0] = '\0';
                field->composition_length = 0;
            }
            strcat(field->text, e.text.text);
            field->text_len = strlen(field->text);
            text_field_update_text_info(field);
            screen_render(screen);
            break;
        }
        case SDL_KEYUP:
            switch (e.key.keysym.sym) {
            case SDLK_BACKSPACE:
                U8_BACK_1(field->text, 0, field->text_len);
                field->text[field->text_len] = '\0';
                text_field_update_text_info(field);
                screen_render(screen);
                break;
            }
            break;
        };
    }
out:
    SDL_StopTextInput();

    text_field_destroy(field);
    screen_destroy(screen);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    exit(EXIT_SUCCESS);
}

// Simple block game <https://github.com/ccoffing/blokus>
// Copyright (c) 2017 Chuck Coffing <clc@alum.mit.edu>
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>

#include <SDL/SDL.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SCREENX 1024
#define SCREENY 768
#define SQX 30
#define SQY 30
#define BOARDX 20
#define BOARDY 20
#define numDefaultPieces (sizeof(defaultPieces) / sizeof(struct Piece))
#define numPlayers 4
#define MASK(x, y, width) (1 << ((y) * (width) + (x)))

struct Player;

enum PieceStatus {
    StatusUnplayed,
    StatusPlaying,
    StatusPlayable,
    StatusNotPlayable,
    StatusDead, // Alter-ego is already played
    StatusPlayed,
};

struct Piece {
    int x;
    int y;
    int bits;
    uint32_t touching;
    uint32_t diag;
    struct Player* player;
    int num;
    enum PieceStatus inPlay;
    int anchorX;
    int anchorY;
};

struct Board {
    int sw; // square width
    int sh;
    int nx; // number x
    int ny;
    int pad;
    int x;
    int y;
    struct Piece** pieces;
    struct Player** player;
    uint32_t color;
    bool dirty; // layout needed?
};

struct Piece defaultPieces[] = {
    { 1, 1, 0x001, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 0x003, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 3, 0x007, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 2, 0x00d, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 4, 0x00f, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 3, 0x03a, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 3, 0x01d, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 2, 0x00f, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 2, 0x033, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 5, 0x01f, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 0x0ea, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 0x07a, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 3, 0x03e, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 3, 0x03b, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 0x05d, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x1d2, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x1c9, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x133, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x139, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x0b9, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 0x0ba, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0x000, 0, 0, 0, 0, 0, 0, 0 },
};

struct Player {
    int num;
    uint32_t color;
    struct Piece* pieces[numDefaultPieces]; // Still to be played
    int moves;
    int homeX;
    int homeY;
};

static struct Player* players[numPlayers];
static struct Board* bg[4];
static SDL_Surface* screen = NULL;

void InitPieces()
{
    struct Piece* p;

    for (int i = 0; (p = &defaultPieces[i])->x; ++i) {
        p->num = i;
        p->inPlay = StatusUnplayed;
        for (int y = 0; y < p->y; ++y) {
            for (int x = 0; x < p->x; ++x) {
                if (p->bits & MASK(x, y, p->x)) {
                    p->touching |= MASK(x + 1, y + 1, p->x + 2);
                    p->touching |= MASK(x, y + 1, p->x + 2);
                    p->touching |= MASK(x + 2, y + 1, p->x + 2);
                    p->touching |= MASK(x + 1, y, p->x + 2);
                    p->touching |= MASK(x + 1, y + 2, p->x + 2);
                }
            }
        }
        p->diag = p->touching;
        for (int y = 0; y < p->y; ++y) {
            for (int x = 0; x < p->x; ++x) {
                if (p->bits & MASK(x, y, p->x)) {
                    p->diag |= MASK(x, y, p->x + 2);
                    p->diag |= MASK(x + 2, y, p->x + 2);
                    p->diag |= MASK(x, y + 2, p->x + 2);
                    p->diag |= MASK(x + 2, y + 2, p->x + 2);
                }
            }
        }
        p->diag ^= p->touching;
    }
}

void Board_Clear(struct Board* b)
{
    for (int i = 0; i < b->nx * b->ny; ++i) {
        b->pieces[i] = (struct Piece*)0;
        b->player[i] = 0;
    }
}

struct Board* Board_New(int nx, int ny, int sw, int sh)
{
    struct Board* b = (struct Board*)malloc(sizeof(struct Board));

    b->sw = sw;
    b->sh = sh;
    b->nx = nx;
    b->ny = ny;
    b->pad = 2;
    b->x = 0;
    b->y = 0;
    b->pieces = (struct Piece**)malloc(sizeof(struct Piece*) * nx * ny);
    b->player = (struct Player**)malloc(sizeof(struct Player*) * nx * ny);
    Board_Clear(b);
    b->color = SDL_MapRGB(screen->format, 0x70, 0x70, 0x70);
    b->dirty = true;
    return b;
}

void Board_PlayPiece(struct Board* b, struct Piece* p, int x, int y)
{
    int mask = 1;
    bool anchored = false;

    assert(x >= 0 && y >= 0 && x + p->x <= b->nx && y + p->y <= b->ny);

    for (int j = 0; j < p->y; ++j) {
        for (int i = 0; i < p->x; ++i) {
            if (p->bits & mask) {
                if (!anchored) {
                    b->pieces[(y + j) * b->nx + (x + i)] = p;
                    p->anchorX = i;
                    p->anchorY = j;
                    anchored = true;
                }
                b->player[(y + j) * b->nx + (x + i)] = p->player;
            }
            mask <<= 1;
        }
    }
}

struct Player* Player_New(int num, uint32_t color)
{
    struct Player* player = (struct Player*)malloc(sizeof(struct Player));
    int i = 0;

    while (1) {
        player->pieces[i] = (struct Piece*)malloc(sizeof(struct Piece));
        *player->pieces[i] = defaultPieces[i];
        if (!defaultPieces[i].x)
            break;
        player->pieces[i]->player = player;
        ++i;
    }
    player->num = num;
    player->color = color;
    player->moves = 0;
    return player;
}

void Player_Delete(struct Player* player)
{
    int i = 0;
    bool done;
    do {
        done = player->pieces[i]->x == 0;
        free(player->pieces[i++]);
    } while (!done);
    free(player);
}

uint32_t PlayerColor(int player)
{
    static int init;
    static uint32_t colors[4];
    if (!init) {
        init = 1;
        colors[0] = SDL_MapRGBA(screen->format, 0x10, 0x10, 0xf0, 0xff);
        colors[1] = SDL_MapRGBA(screen->format, 0xf0, 0xf0, 0x10, 0xff);
        colors[2] = SDL_MapRGBA(screen->format, 0xf0, 0x10, 0x10, 0xff);
        colors[3] = SDL_MapRGBA(screen->format, 0x10, 0xf0, 0x10, 0xff);
    }

    return colors[player];
}

void InitPlayers()
{
    for (int i = 0; i < numPlayers; ++i) {
        players[i] = Player_New(i, PlayerColor(i));
        switch (i) {
        case 0:
            players[i]->homeX = 0;
            players[i]->homeY = 0;
            break;
        case 1:
            players[i]->homeX = BOARDX - 1;
            players[i]->homeY = 0;
            break;
        case 2:
            players[i]->homeX = BOARDX - 1;
            players[i]->homeY = BOARDY - 1;
            break;
        case 3:
            players[i]->homeX = 0;
            players[i]->homeY = BOARDY - 1;
            break;
        }
    }
}

void DeinitPlayers()
{
    for (int i = 0; i < numPlayers; ++i) {
        Player_Delete(players[i]);
    }
}

void Piece_Flip(struct Piece* p)
{
    int mask = 1;
    struct Piece p2 = *p;

    p2.bits = 0;
    for (int y = 0; y < p->y; ++y) {
        for (int x = 0; x < p->x; ++x) {
            if (p->bits & mask) {
                p2.bits |= (1 << (p->x - x - 1 + (y * p->x)));
            }
            mask <<= 1;
        }
    }
    *p = p2;
}

void Piece_Rotate90(struct Piece* p)
{
    int mask = 1;
    struct Piece p2 = *p;

    p2.x = p->y;
    p2.y = p->x;
    p2.bits = p2.diag = p2.touching = 0;
    for (int y = 0; y < p->y; ++y) {
        for (int x = 0; x < p->x; ++x) {
            int mask2 = (1 << (x * p->y + (p->y - y - 1)));
            if (p->bits & mask)
                p2.bits |= mask2;
            // if (p->diag & mask)
            //    p2.diag |= mask2;
            // if (p->touching & mask)
            //    p2.touching |= mask2;
            mask <<= 1;
        }
    }
    *p = p2;
}

bool Piece_CheckCovers(int x, int y, struct Piece* p, int coverX, int coverY)
{
    if (x > coverX || y > coverY || x + p->x <= coverX || y + p->y <= coverY)
        return false;
    return p->bits & MASK(coverX - x, coverY - y, p->x);
}

bool CheckPieceFits(struct Board* b, int x, int y, struct Piece* p, struct Player* cantTouch,
    struct Player* cantDiag, struct Player* mustDiag)
{
    if (x < 0 || y < 0 || x > b->nx - p->x || y > b->ny - p->y)
        return 0;
    int mask = 1;
    for (int j = 0; j < p->y; ++j) {
        for (int i = 0; i < p->x; ++i) {
            if (p->bits & mask) {
                if (b->player[(x + i) + (y + j) * b->nx])
                    return 0;
            }
            mask <<= 1;
        }
    }
    bool foundMustDiag = false;
    for (int j = -1; j <= p->y; ++j) {
        for (int i = -1; i <= p->x; ++i) {
            if (x + i < 0 || y + j < 0 || x + i >= b->nx || y + j >= b->ny)
                continue;
            struct Player* player = b->player[(x + i) + (y + j) * b->nx];
            if (mustDiag && player == mustDiag && (p->diag & MASK(i + 1, j + 1, p->x + 2)))
                foundMustDiag = true;
            if (cantDiag && player == cantDiag && (p->diag & MASK(i + 1, j + 1, p->x + 2)))
                return 0;
            if (cantTouch && player == cantTouch && (p->touching & MASK(i + 1, j + 1, p->x + 2)))
                return 0;
        }
    }
    return !(mustDiag && !foundMustDiag);
}

bool CheckPiecePlayable(struct Board* b, int x, int y, struct Piece* p)
{
    if (x >= 0 && y >= 0 && x + p->x <= b->nx && y + p->y <= b->ny) {
        if ((p->player->moves == 0 && CheckPieceFits(b, x, y, p, p->player, 0, 0)
                && Piece_CheckCovers(x, y, p, p->player->homeX, p->player->homeY))
            || (p->player->moves > 0 && CheckPieceFits(b, x, y, p, p->player, 0, p->player))) {
            return true;
        }
    }
    return false;
}

void DrawBetween(struct Board* b, int x, int y, int dir, uint32_t color)
{
    SDL_Rect r;

    switch (dir) {
    case 0: // right
        r.x = b->x + ((x + 1) * b->sw) - b->pad;
        r.y = b->y + (y * b->sh) + b->pad * 2;
        r.w = b->pad;
        r.h = b->sh - b->pad * 4;
        break;
    case 1: // below
        r.x = b->x + (x * b->sw) + b->pad * 2;
        r.y = b->y + ((y + 1) * b->sh) - b->pad;
        r.w = b->sw - b->pad * 4;
        r.h = b->pad;
        break;
    case 2: // left
        r.x = b->x + (x * b->sw);
        r.y = b->y + (y * b->sh) + b->pad * 2;
        r.w = b->pad;
        r.h = b->sh - b->pad * 4;
        break;
    case 3: // above
        r.x = b->x + (x * b->sw) + b->pad * 2;
        r.y = b->y + (y * b->sh);
        r.w = b->sw - b->pad * 4;
        r.h = b->pad;
        break;
    }
    SDL_FillRect(screen, &r, color);
}

void DrawSquare(struct Board* b, int x, int y, uint32_t color)
{
    SDL_Rect r = {.x = b->x + (x * b->sw) + b->pad,
        .y = b->y + (y * b->sh) + b->pad,
        .w = b->sw - b->pad * 2,
        .h = b->sh - b->pad * 2 };
    SDL_FillRect(screen, &r, color);
}

void DrawPiece(struct Board* board, struct Piece* p, int x, int y)
{
    int mask = 1;
    uint32_t color = p->player->color;
    uint32_t outline = color;

    Uint8 r, g, b;
    SDL_GetRGB(color, screen->format, &r, &g, &b);
    if (p->inPlay == StatusDead) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
        outline = color = SDL_MapRGB(screen->format, r, g, b);
    } else if (p->inPlay == StatusPlayed) {
        r *= 0.8;
        g *= 0.8;
        b *= 0.8;
        outline = SDL_MapRGB(screen->format, r, g, b);
    } else if (p->inPlay == StatusPlayable) {
        outline = color;
    } else if (p->inPlay == StatusNotPlayable) {
        r *= 0.8;
        g *= 0.8;
        b *= 0.8;
        color = SDL_MapRGB(screen->format, r, g, b);
        r *= 0.8;
        g *= 0.8;
        b *= 0.8;
        outline = SDL_MapRGB(screen->format, r, g, b);
    }

    for (int j = 0; j < p->y; ++j) {
        for (int i = 0; i < p->x; ++i) {
            if (p->bits & mask) {
                DrawSquare(board, x + i, y + j, color);

                // Draw in-between connecting bits:
                // right
                // if (i + 1 < p->x && (p->bits & (mask << 1)))
                DrawBetween(board, x + i, y + j, 0, outline);
                // below
                // if (j + 1 < p->y && (p->bits & (mask << p->x)))
                DrawBetween(board, x + i, y + j, 1, outline);
                // left
                // if (i && p->bits & (mask >> 1))
                DrawBetween(board, x + i, y + j, 2, outline);
                // above
                // if (j && p->bits & (mask >> p->x))
                DrawBetween(board, x + i, y + j, 3, outline);
            }
            mask <<= 1;
        }
    }
}

void Board_Draw(struct Board* b)
{
    for (int y = 0; y < b->ny; ++y) {
        for (int x = 0; x < b->nx; ++x) {
            if (!b->player[y * b->nx + x])
                DrawSquare(b, x, y, b->color);
            struct Piece* p = b->pieces[y * b->nx + x];
            if (p)
                DrawPiece(b, p, x - p->anchorX, y - p->anchorY);
        }
    }
}

struct Piece* GetNextPlayablePiece(struct Player* player, struct Piece* p)
{
    int num = p ? p->num : -1;

    while (1) {
        num = (num + 1) % (numDefaultPieces - 1);
        p = player->pieces[num];
        if (p->inPlay == StatusUnplayed)
            break;
        // TODO:  no more playable == win!
    }

    p->inPlay = StatusPlaying;

    struct Piece* piece = (struct Piece*)malloc(sizeof(struct Piece));
    *piece = *p;

    return piece;
}

void ReturnPiece(struct Piece* p)
{
    struct Piece* origPiece = p->player->pieces[p->num];
    origPiece->inPlay = StatusUnplayed;
    free(p);
}

void PlayPiece(struct Board* b, struct Piece* p, int x, int y)
{
    p->inPlay = StatusPlayed;
    Board_PlayPiece(b, p, x, y);
}

bool PlacePiece(struct Board* board, struct Piece* dragging, int x, int y)
{
    if (CheckPiecePlayable(board, x, y, dragging)) {
        PlayPiece(board, dragging, x, y);
        return true;
    }
    return false;
}

void RedrawScreen(struct Board* b, struct Board** bg)
{
    SDL_Rect r = {.x = 0, .y = 0, .w = SCREENX, .h = SCREENY };

    SDL_FillRect(screen, &r, 0);
    Board_Draw(b);

    int n;
    for (n = 0; n < numPlayers; ++n) {
        if (bg[n]->dirty) {
            bg[n]->dirty = false;
            Board_Clear(bg[n]);

            int x = 0;
            int y = 0;
            int i = 0;
            struct Player* player = players[n];
            while (y < b->ny) {
                struct Piece* p = player->pieces[i];
                if (!p) {
                    ++i;
                    continue;
                }
                if (!p->x)
                    break;
                for (int rotates = 4;;) {
                    if (CheckPieceFits(bg[n], x, y, p, player, player, 0))
                        break;
                    if (rotates == 0) {
                        goto nextSquare;
                    } else {
                        if (rotates & 1)
                            Piece_Rotate90(p);
                        else
                            Piece_Flip(p);
                        rotates--;
                    }
                }
                Board_PlayPiece(bg[n], p, x, y);
                i++;
nextSquare:
                if (++x >= bg[n]->nx) {
                    x = 0;
                    y++;
                }
            }
        }
        Board_Draw(bg[n]);
    }
}

int MainLoop()
{
    int vx = 0;
    int vy = 0;
    int px = 0;
    int py = 0;
    SDL_Event event;
    bool dirty = true;
    bool dirtyPiece = false;
    int curPlayer = 0;
    struct Player* player = players[curPlayer];
    int left = (SCREENX - (BOARDX * SQX)) / 2;
    int top = (SCREENY - (BOARDY * SQY)) / 2;
    int bottom = SCREENY - top - 1;
    int right = SCREENX - left - 1;
    struct Board* board = Board_New(BOARDX, BOARDY, SQX, SQY);
    struct Piece* dragging = 0;

    board->x = left;
    board->y = top;

    bg[0] = Board_New(left / (SQX / 2), bottom / (SQY / 2), SQX / 2, SQY / 2);
    bg[0]->x = 0;
    bg[0]->y = 0;
    bg[0]->color = 0;
    bg[1] = Board_New(left / (SQX / 2), bottom / (SQY / 2), SQX / 2, SQY / 2);
    bg[1]->x = right + 1;
    bg[1]->y = 0;
    bg[1]->color = 0;
    bg[2] = Board_New(left / (SQX / 2), bottom / (SQY / 2), SQX / 2, SQY / 2);
    bg[2]->x = right + 1;
    bg[2]->y = SCREENY / 2;
    bg[2]->color = 0;
    bg[3] = Board_New(left / (SQX / 2), bottom / (SQY / 2), SQX / 2, SQY / 2);
    bg[3]->x = 0;
    bg[3]->y = SCREENY / 2;
    bg[3]->color = 0;

    do {
        if (dirty || dirtyPiece) {
            RedrawScreen(board, bg);
        }
        if (dragging && dirtyPiece) {
            bool playable = CheckPiecePlayable(board, px, py, dragging);
            dragging->inPlay = playable ? StatusPlayable : StatusNotPlayable;
            DrawPiece(board, dragging, px, py);
        }
        if (dirty || dirtyPiece) {
            dirty = dirtyPiece = false;
            SDL_UpdateRect(screen, 0, 0, 0, 0);
        }

        if (SDL_PollEvent(&event) == 0) {
            SDL_Delay(10);
        } else {
            switch (event.type) {
            case SDL_KEYUP: {
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    if (vx < 0)
                        vx = 0;
                    break;
                case SDLK_RIGHT:
                    if (vx > 0)
                        vx = 0;
                    break;
                case SDLK_UP:
                    if (vy < 0)
                        vy = 0;
                    break;
                case SDLK_DOWN:
                    if (vy > 0)
                        vy = 0;
                    break;
                default:
                    break;
                }
                break;
            }

            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_TAB: {
                    struct Piece* nextPiece = GetNextPlayablePiece(player, dragging);
                    if (dragging)
                        ReturnPiece(dragging);
                    dragging = nextPiece;
                    bg[curPlayer]->dirty = true;
                    dirty = dirtyPiece = true;
                    break;
                }
                case SDLK_RETURN:
                    if (dragging) {
                        bool played = PlacePiece(board, dragging, px, py);
                        if (played) {
                            player->moves++;
                            curPlayer = (curPlayer + 1) % numPlayers;
                            player = players[curPlayer];

                            dragging = 0;
                            dirty = true;
                            bg[curPlayer]->dirty = true;
                        }
                    }
                    break;
                case SDLK_SPACE:
                    // TODO: rotate when on edge of board; piece can go off edge
                    if (dragging) {
                        if (event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) {
                            Piece_Flip(dragging);
                        } else {
                            Piece_Rotate90(dragging);
                        }
                        dirtyPiece = true;
                    }
                    break;
                case SDLK_LEFT:
                    vx = -1;
                    break;
                case SDLK_RIGHT:
                    vx = 1;
                    break;
                case SDLK_UP:
                    vy = -1;
                    break;
                case SDLK_DOWN:
                    vy = 1;
                    break;
                case SDLK_ESCAPE:
                    goto done;
                default:
                    break;
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                int npx = (event.motion.x - board->x) / board->sw;
                int npy = (event.motion.y - board->y) / board->sh;
                vx = vy = 0;
                if (npx != px || npy != py) {
                    px = npx;
                    py = npy;
                    if (dragging)
                        dirtyPiece = true;
                }
                break;
            }

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT && dragging) {
                    int x, y;
                    x = (event.button.x - board->x) / board->sw;
                    y = (event.button.y - board->y) / board->sh;
                    bool played = PlacePiece(board, dragging, x, y);
                    if (played) {
                        player->moves++;
                        curPlayer = (curPlayer + 1) % numPlayers;
                        player = players[curPlayer];
                    } else {
                        ReturnPiece(dragging);
                    }
                    dragging = 0;
                    dirty = true;
                    bg[curPlayer]->dirty = true;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (dragging) {
                    } else {
                        // TODO:  abstract for each board
                        dragging = GetNextPlayablePiece(player, NULL);
                        bg[curPlayer]->dirty = true;
                        dirty = true;
                        dirtyPiece = true;
                    }
                }
                break;

            case SDL_QUIT:
                goto done;
            }
        }

        if (vx || vy) {
            static uint32_t prev = 0;
            uint32_t now = SDL_GetTicks();
            if (now < prev || now - prev > 70) {
                prev = now;
                px += vx;
                py += vy;
                dirtyPiece = true;
            }
        }
        if (dirtyPiece) {
            if (px < 0)
                px = 0;
            else if (dragging && px > board->nx - dragging->x)
                px = board->nx - dragging->x;
            if (py < 0)
                py = 0;
            else if (dragging && py > board->ny - dragging->y)
                py = board->ny - dragging->y;
            // SDL_WarpMouse(board->x + px * board->sw, board->y + py * board->sh);
        }
    } while (1);

done:
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        fprintf(stderr, "Simple block game <https://github.com/ccoffing/blokus>\n");
        fprintf(stderr, "Copyright (c) 2017 Chuck Coffing <clc@alum.mit.edu>\n");
        fprintf(stderr, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\n");
        fprintf(stderr, "Gameplay:\n");
        fprintf(stderr, "    1-4 players.  Players take turns, starting by placing a piece anchored in\n");
        fprintf(stderr, "    that player's corner.  A player's subsequent pieces must touch corners with\n");
        fprintf(stderr, "    one of the player's already-played pieces, but cannot touch any of that\n");
        fprintf(stderr, "    player's pieces face-to-face.\n");
        fprintf(stderr, "Keys:\n");
        fprintf(stderr, "    Tab          Next piece\n");
        fprintf(stderr, "    Arrow keys   Drag piece\n");
        fprintf(stderr, "    Space        Rotate piece\n");
        fprintf(stderr, "    Shift-Space  Flip piece\n");
        fprintf(stderr, "    Enter        Place piece\n");
        exit(1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "\nUnable to initialize SDL:  %s\n", SDL_GetError());
        return -1;
    }
    atexit(SDL_Quit);

    screen = SDL_SetVideoMode(SCREENX, SCREENY, 0, 0);
    if (screen == NULL) {
        return -1;
    }
    SDL_WM_SetCaption("Blokus", "none");

    InitPieces();
    InitPlayers();
    MainLoop();
    DeinitPlayers();

    return 0;
}

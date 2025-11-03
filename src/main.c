#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Game states
typedef enum {
    MENU,
    GAMEPLAY,
    GAMEOVER,
    PAUSED
} GameState;

// Game constants (adjusted for smoother gameplay)
#define GRAVITY 500.0f
#define JUMP_FORCE -250.0f
#define PIPE_SPEED 120.0f         // slower pipes
#define PIPE_GAP 250              // wider gap between top/bottom
#define PIPE_WIDTH 80
#define FISH_SIZE 40
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 50
#define SMALL_BUTTON_WIDTH 120
#define SMALL_BUTTON_HEIGHT 30
#define SCORE_MULTIPLIER 0.3f     // slower score growth
#define PIPE_Y_MARGIN 50
#define MAX_PIPES 3               // fewer pipes total
#define INITIAL_ACTIVE_PIPES 1
#define DIFFICULTY_INTERVAL 15.0f // slower difficulty increase
#define PIPE_SPEED_INCREMENT 10.0f
#define PIPE_GAP_DECREMENT 5
#define DEFAULT_SPAWN_SPACING 500 // more space between pipes

// Structures
typedef struct {
    Rectangle bounds;
    const char *text;
    Color color;
} Button;

typedef struct {
    Vector2 pos;
    float vy;
    Color color;
} Fish;

typedef struct {
    float x;
    int gapY;
    bool passed;
} Pipe;

// Button creation/drawing
Button CreateButton(float x, float y, const char *text, Color color) {
    Button btn = {
        .bounds = (Rectangle){x, y, BUTTON_WIDTH, BUTTON_HEIGHT},
        .text = text,
        .color = color
    };
    return btn;
}
void DrawButton(const Button *btn) {
    DrawRectangleRec(btn->bounds, btn->color);
    int textWidth = MeasureText(btn->text, 20);
    DrawText(btn->text,
             btn->bounds.x + (btn->bounds.width - textWidth)/2,
             btn->bounds.y + btn->bounds.height/2 - 10,
             20, WHITE);
}
Button CreateSmallButton(float x, float y, const char *text, Color color) {
    Button btn = {
        .bounds = (Rectangle){x, y, SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT},
        .text = text,
        .color = color
    };
    return btn;
}
bool IsButtonPressed(const Button *btn) {
    Vector2 mousePoint = GetMousePosition();
    return (CheckCollisionPointRec(mousePoint, btn->bounds) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

// File save/load for high score
void SaveHighScore(int score) {
    FILE *file = fopen("highscore.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d", score);
        fclose(file);
    }
}
int LoadHighScore(void) {
    int score = 0;
    FILE *file = fopen("highscore.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d", &score);
        fclose(file);
    }
    return score;
}

// Reset game
void ResetGame(Fish *fish, Pipe *pipes, int pipeCount, int *score, int screenWidth, int screenHeight) {
    fish->pos = (Vector2){100, screenHeight/2.0f};
    fish->vy = 0;
    for (int i = 0; i < pipeCount; i++) {
        pipes[i].x = (float)(screenWidth + i * (screenWidth / pipeCount));
        int maxGapY = screenHeight - PIPE_GAP - PIPE_Y_MARGIN;
        if (maxGapY < PIPE_Y_MARGIN) maxGapY = PIPE_Y_MARGIN;
        pipes[i].gapY = GetRandomValue(PIPE_Y_MARGIN, maxGapY);
        pipes[i].passed = false;
    }
    *score = 0;
}

int main(void) {
    const int screenWidth = 480, screenHeight = 700;
    InitWindow(screenWidth, screenHeight, "Flappy Fish");
    SetTargetFPS(60);
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    GameState gameState = MENU;
    int score = 0;
    float playTime = 0.0f;
    int highscore = LoadHighScore();

    Fish fish = {{100, screenHeight/2.0f}, 0, GOLD};
    Pipe pipes[MAX_PIPES];
    int activePipes = INITIAL_ACTIVE_PIPES;
    float pipeSpeed = PIPE_SPEED;
    int pipeGap = PIPE_GAP;
    int spawnSpacing = DEFAULT_SPAWN_SPACING;
    float difficultyTimer = 0.0f;

    ResetGame(&fish, pipes, activePipes, &score, screenWidth, screenHeight);

    Button startButton = CreateButton(screenWidth/2 - BUTTON_WIDTH/2, screenHeight/2 - BUTTON_HEIGHT/2, "START GAME", DARKGREEN);
    Button restartButton = CreateButton(screenWidth/2 - BUTTON_WIDTH/2, screenHeight/2 + BUTTON_HEIGHT, "PLAY AGAIN", DARKGREEN);

    Rectangle menuPauseRect = (Rectangle){ screenWidth - 95, 5, 80, 24 };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int curW = GetScreenWidth(), curH = GetScreenHeight();
        menuPauseRect.x = curW - 95;

        switch (gameState) {
            case MENU:
                if (IsButtonPressed(&startButton)) {
                    gameState = GAMEPLAY;
                    ResetGame(&fish, pipes, activePipes, &score, curW, curH);
                    playTime = 0.0f;
                    pipeSpeed = PIPE_SPEED;
                    pipeGap = PIPE_GAP;
                    activePipes = INITIAL_ACTIVE_PIPES;
                }
                break;

            case GAMEPLAY: {
                if (IsKeyPressed(KEY_ESCAPE) ||
                    (CheckCollisionPointRec(GetMousePosition(), menuPauseRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
                    gameState = PAUSED;
                    break;
                }

                playTime += dt;
                float scoreMultiplier = 1.0f + (playTime * SCORE_MULTIPLIER);

                if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    fish.vy = JUMP_FORCE;

                fish.vy += GRAVITY * dt;
                fish.pos.y += fish.vy * dt;

                if (fish.pos.y < 0) fish.pos.y = 0;
                if (fish.pos.y > curH - FISH_SIZE) fish.pos.y = curH - FISH_SIZE;

                difficultyTimer += dt;
                if (difficultyTimer >= DIFFICULTY_INTERVAL) {
                    difficultyTimer = 0.0f;
                    if (activePipes < MAX_PIPES) activePipes++;
                    pipeSpeed += PIPE_SPEED_INCREMENT;
                    if (pipeGap - PIPE_GAP_DECREMENT >= 150)
                        pipeGap -= PIPE_GAP_DECREMENT;
                }

                for (int i = 0; i < activePipes; i++) {
                    pipes[i].x -= pipeSpeed * dt;

                    if (!pipes[i].passed && fish.pos.x > pipes[i].x + PIPE_WIDTH) {
                        score += (int)(1 * scoreMultiplier);
                        pipes[i].passed = true;
                        if (score > highscore) {
                            highscore = score;
                            SaveHighScore(highscore);
                        }
                    }

                    if (pipes[i].x + PIPE_WIDTH < 0) {
                        pipes[i].x = (float)curW + spawnSpacing;
                        int maxGapY = curH - pipeGap - PIPE_Y_MARGIN;
                        if (maxGapY < PIPE_Y_MARGIN) maxGapY = PIPE_Y_MARGIN;
                        pipes[i].gapY = GetRandomValue(PIPE_Y_MARGIN, maxGapY);
                        pipes[i].passed = false;
                    }

                    Rectangle fishRec = { fish.pos.x, fish.pos.y, FISH_SIZE, FISH_SIZE };
                    Rectangle topPipe = { pipes[i].x, 34, PIPE_WIDTH, pipes[i].gapY - 34 };
                    Rectangle bottomPipe = { pipes[i].x, pipes[i].gapY + pipeGap, PIPE_WIDTH, curH - (pipes[i].gapY + pipeGap) };
                    if (CheckCollisionRecs(fishRec, topPipe) || CheckCollisionRecs(fishRec, bottomPipe)) {
                        gameState = GAMEOVER;
                        break;
                    }
                }
                break;
            }

            case GAMEOVER:
                if (IsButtonPressed(&restartButton)) {
                    gameState = GAMEPLAY;
                    ResetGame(&fish, pipes, activePipes, &score, curW, curH);
                    playTime = 0.0f;
                }
                break;

            case PAUSED:
                if (IsKeyPressed(KEY_ESCAPE) ||
                    (CheckCollisionPointRec(GetMousePosition(), menuPauseRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
                    gameState = GAMEPLAY;
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    gameState = MENU;
                    ResetGame(&fish, pipes, activePipes, &score, curW, curH);
                    playTime = 0.0f;
                }
                break;
        }

        // Draw
        BeginDrawing();
        ClearBackground(SKYBLUE);

        DrawRectangle(0, 0, curW, 34, LIGHTGRAY);
        DrawText("Menu", 8, 6, 18, BLACK);
        const char *pauseText = (gameState == PAUSED) ? "RESUME" : "PAUSE";
        DrawRectangleRec(menuPauseRect, DARKGRAY);
        int pw = MeasureText(pauseText, 14);
        DrawText(pauseText, menuPauseRect.x + (menuPauseRect.width - pw)/2, menuPauseRect.y + 4, 14, WHITE);

        switch (gameState) {
            case MENU:
                DrawText("FLAPPY FISH", curW/2 - MeasureText("FLAPPY FISH", 40)/2, curH/4, 40, WHITE);
                DrawButton(&startButton);
                DrawText(TextFormat("HIGH SCORE: %i", highscore),
                         curW/2 - MeasureText(TextFormat("HIGH SCORE: %i", highscore), 20)/2,
                         curH*3/4, 20, WHITE);
                break;

            case GAMEPLAY:
            case PAUSED:
                for (int i = 0; i < activePipes; i++) {
                    DrawRectangle((int)pipes[i].x, 34, PIPE_WIDTH, pipes[i].gapY - 34, GREEN);
                    DrawRectangle((int)pipes[i].x, pipes[i].gapY + pipeGap, PIPE_WIDTH, curH - (pipes[i].gapY + pipeGap), GREEN);
                }
                DrawRectangleV(fish.pos, (Vector2){FISH_SIZE, FISH_SIZE}, fish.color);
                DrawText(TextFormat("SCORE: %i", score), 20, 40, 30, BLACK);
                DrawText(TextFormat("HIGH: %i", highscore), 20, 80, 25, DARKGRAY);
                DrawText(TextFormat("PIPES: %i", activePipes), 20, 120, 20, DARKBLUE);
                DrawText(TextFormat("SPACING: %i", spawnSpacing), 20, 150, 20, DARKBLUE);
                if (gameState == PAUSED) {
                    DrawRectangle(0, 34, curW, curH-34, (Color){0,0,0,120});
                    DrawText("PAUSED", curW/2 - MeasureText("PAUSED", 40)/2, curH/3, 40, WHITE);
                }
                break;

            case GAMEOVER:
                DrawText("GAME OVER!", curW/2 - MeasureText("GAME OVER!", 40)/2, curH/4, 40, RED);
                DrawText(TextFormat("FINAL SCORE: %i", score),
                         curW/2 - MeasureText(TextFormat("FINAL SCORE: %i", score), 30)/2,
                         curH/3, 30, WHITE);
                DrawText(TextFormat("HIGH SCORE: %i", highscore),
                         curW/2 - MeasureText(TextFormat("HIGH SCORE: %i", highscore), 25)/2,
                         curH/2 - 50, 25, WHITE);
                DrawButton(&restartButton);
                break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

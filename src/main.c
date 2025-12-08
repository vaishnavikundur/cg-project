#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//------------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------------
#define MAX_OBSTACLES 6
#define OBSTACLE_SPACING 20.0f
#define GAP_SIZE 6.5f
#define MOVE_SPEED 0.15f
#define FISH_SPEED 4.0f
#define FISH_GRAVITY -8.0f
#define FISH_JUMP 6.5f
#define FISH_X -5.0f
#define OBSTACLE_RADIUS 0.8f
#define FISH_RADIUS 0.4f
#define HIGH_SCORE_FILE "flappy_fish_highscore.txt"

#define DIFFICULTY_INTERVAL 120.0f  // Increase difficulty every 2 minutes (120 seconds)
#define SPEED_INCREMENT 1.5f       // Add this much to obstacle speed per level
#define GAP_DECREMENT 0.3f         // Reduce gap by this much per level (minimum 4.0f)

#define DARKORANGE (Color){255, 140, 0, 255}

//------------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------------
typedef enum GameState {
    STATE_START,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER
} GameState;

typedef struct Fish {
    Vector3 position;
    float rotation;
    float tailAngle;
    float finAngle;
    float vy; // vertical velocity for gravity/flap
} Fish;

typedef struct Obstacle {
    Vector3 position;
    float gapY;
    bool passed;
} Obstacle;

typedef struct Bubble {
    Vector3 position;
    float speed;
    float size;
    float wobble;
    float alpha;
} Bubble;

//------------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------------
static Fish fish = {0};
static Obstacle obstacles[MAX_OBSTACLES] = {0};
static Bubble bubbles[150] = {0};

static GameState gameState = STATE_START;
static int score = 0;
static int highScore = 0;

static Camera3D camera = {0};
static float gameTime = 0.0f;
static float currentGapSize = GAP_SIZE;  // Track gap size (decreases with difficulty)
static float currentObstacleSpeed = FISH_SPEED;  // Track obstacle speed (increases with difficulty)
static float difficultyTimer = 0.0f;  // Timer for difficulty scaling

// Audio assets
static Music bgMusic = {0};
static Sound collisionSound = {0};

//------------------------------------------------------------------------------------
// Declarations
//------------------------------------------------------------------------------------
void InitGame(void);
void UpdateGame(void);
void DrawGame(void);
void ResetGame(void);

// Simple button helper
typedef struct Button {
    Rectangle rect;
    const char *text;
    Color color;
} Button;

static Button startBtn;
static Button pauseBtn;
static Button resumeBtn;
static Button playAgainBtn;

static void InitButtons(void);
static void UpdateButtonsPositions(void);
static void DrawButton(const Button *b, int fontSize);
static bool ButtonPressed(const Button *b);

void DrawFish(Vector3 position, float rotation, float tail, float fin);
void DrawObstacle(Vector3 p, float gapY);
void DrawOcean(void);

void UpdateBubbles(void);
void DrawBubbles(void);

bool CheckCollision(void);
void SaveHighScore(int s);
int LoadHighScore(void);

// -----------------------
// UI helpers
// -----------------------
static void InitButtons(void)
{
    // default sizes; positions are set/updated per screen size
    startBtn.text = "START GAME";
    startBtn.color = BLUE;

    pauseBtn.text = "PAUSE";
    pauseBtn.color = DARKGRAY;

    resumeBtn.text = "RESUME";
    resumeBtn.color = BLUE;

    playAgainBtn.text = "PLAY AGAIN";
    playAgainBtn.color = BEIGE;

    UpdateButtonsPositions();
}

static void UpdateButtonsPositions(void)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    // center large buttons
    float bw = w * 0.45f; if (bw < 220) bw = 220; if (bw > 500) bw = 500;
    float bh = 80;
    startBtn.rect = (Rectangle){ (w - bw)/2.0f, (h - bh)/2.0f, bw, bh };
    playAgainBtn.rect = (Rectangle){ (w - bw)/2.0f, (h - bh)/2.0f + 80, bw, bh };
    resumeBtn.rect = (Rectangle){ (w - bw)/2.0f, (h - bh)/2.0f + 0, bw, bh };

    // small pause button top-right
    float pw = 110, ph = 36;
    pauseBtn.rect = (Rectangle){ w - pw - 12.0f, 12.0f, pw, ph };
}

static void DrawButton(const Button *b, int fontSize)
{
    DrawRectangleRec(b->rect, b->color);
    int textW = MeasureText(b->text, fontSize);
    DrawText(b->text,
             (int)(b->rect.x + (b->rect.width - textW)/2),
             (int)(b->rect.y + (b->rect.height - fontSize)/2),
             fontSize, WHITE);
}

static bool ButtonPressed(const Button *b)
{
    Vector2 mp = GetMousePosition();
    return (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mp, b->rect));
}

//------------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------------
int main(void)
{
    InitWindow(0, 0, "3D Flappy Fish");
    ToggleFullscreen();
    InitAudioDevice(); // Initialize audio device for music and sounds

    // Camera setup
    camera.position = (Vector3){ 0.0f, 4.0f, -12.0f };
    camera.target   = (Vector3){ 0.0f, 4.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    highScore = LoadHighScore();
    InitGame();

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        UpdateGame();

        BeginDrawing();
        ClearBackground((Color){0,40,80,255});
        DrawGame();
        EndDrawing();
    }

    CloseAudioDevice(); // Close audio device
    CloseWindow();
    return 0;
}

//------------------------------------------------------------------------------------
// Init
//------------------------------------------------------------------------------------
void InitGame(void)
{
    fish.position = (Vector3){ FISH_X, 4, 0 };
    fish.rotation = fish.tailAngle = fish.finAngle = 0;
    fish.vy = 0.0f;

    // Load audio files (optional - game continues if files are missing)
    bgMusic = LoadMusicStream("src/background.wav");
    collisionSound = LoadSound("src/hit.wav");

    // ensure generated gapY is inside play area so fish (with radius) can pass
    float minGapY = 1.0f + FISH_RADIUS; // bottom margin
    float maxGapY = 8.5f - currentGapSize - FISH_RADIUS; // top margin so gap fits
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        // place obstacles in front of the fish with consistent spacing
        obstacles[i].position = (Vector3){ FISH_X + 10.0f + i*OBSTACLE_SPACING, 0, 0 };
        if (maxGapY <= minGapY) obstacles[i].gapY = minGapY; else obstacles[i].gapY = minGapY + ((float)rand()/(float)RAND_MAX) * (maxGapY - minGapY);
        obstacles[i].passed = false;
    }

    for (int i = 0; i < 150; i++)
    {
        bubbles[i].position = (Vector3){
            (float)(rand()%60) - 30,
            (float)(rand()%10),
            (float)(rand()%40) - 20
        };
        bubbles[i].speed = 0.3f + (rand()%100)/100.0f;
        bubbles[i].size  = 0.08f + (rand()%100)/400.0f;
        bubbles[i].wobble = (float)(rand()%360);
        bubbles[i].alpha  = 0.4f + (rand()%60)/100.0f;
    }

    gameState = STATE_START;
    score = 0;
    gameTime = 0;

    // initialize UI buttons for current window size
    InitButtons();
}

//------------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------------
void UpdateGame(void)
{
    gameTime += GetFrameTime();

    // ensure buttons follow current screen size (fullscreen/resized)
    UpdateButtonsPositions();

    fish.tailAngle = sinf(gameTime * 8) * 0.3f;
    fish.finAngle  = sinf(gameTime * 6) * 0.2f;

    if (gameState == STATE_START)
    {
        fish.position.y = 4.0f + sinf(gameTime * 2.0f) * 0.3f;

        // start either from keyboard or start button
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
            IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
            IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
            ButtonPressed(&startBtn))
        {
            gameState = STATE_PLAYING;
        }
    }

    else if (gameState == STATE_PLAYING)
    {
        // Play background music loop during gameplay
        UpdateMusicStream(bgMusic);
        if (!IsMusicStreamPlaying(bgMusic) && bgMusic.frameCount > 0) PlayMusicStream(bgMusic);

        // pause by pressing P or clicking the pause button
        if (IsKeyPressed(KEY_P) || ButtonPressed(&pauseBtn)) { gameState = STATE_PAUSED; return; }

        // FLAPPY physics: pressing flap sets a positive upward velocity; gravity pulls down
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            fish.vy = FISH_JUMP;
        }

        // tilt fish based on vertical velocity (so it faces somewhat up/down but always to the east on X axis)
        float tilt = -fish.vy * 6.0f; // scale to get a nice visual angle
        if (tilt > 40) tilt = 40;
        if (tilt < -40) tilt = -40;
        fish.rotation = tilt;

        // apply gravity and vertical motion (fish only moves vertically)
        fish.vy += FISH_GRAVITY * GetFrameTime();
        fish.position.y += fish.vy * GetFrameTime();

        // keep fish fixed horizontally
        fish.position.x = FISH_X;

        // bounds
        if (fish.position.x < -8) fish.position.x = -8;
        if (fish.position.x >  8) fish.position.x = 8;
        // DO NOT clamp Y — allow CheckCollision() to detect hitting floor/ceiling and end the game

        // Update difficulty timer and increase challenge every 2 minutes
        difficultyTimer += GetFrameTime();
        if (difficultyTimer >= DIFFICULTY_INTERVAL) {
            difficultyTimer = 0.0f;
            // Increase obstacle speed
            currentObstacleSpeed += SPEED_INCREMENT;
            // Decrease gap size (but maintain minimum of 4.0f for playability)
            if (currentGapSize > 4.0f) {
                currentGapSize -= GAP_DECREMENT;
                if (currentGapSize < 4.0f) currentGapSize = 4.0f;
            }
        }

        // Obstacles
        // compute safe range for gapY so the gap stays fully playable
        float minGapY = 1.0f + FISH_RADIUS;
        float maxGapY = 8.5f - currentGapSize - FISH_RADIUS;
        for (int i = 0; i < MAX_OBSTACLES; i++)
        {
            obstacles[i].position.x -= currentObstacleSpeed * GetFrameTime();

            if (!obstacles[i].passed && obstacles[i].position.x < fish.position.x)
            {
                obstacles[i].passed = true;
                score++;
            }

            if (obstacles[i].position.x < FISH_X - 12.0f)
            {
                // find current farthest obstacle so respawn keeps consistent spacing
                float maxX = obstacles[0].position.x;
                for (int j = 1; j < MAX_OBSTACLES; j++) if (obstacles[j].position.x > maxX) maxX = obstacles[j].position.x;
                obstacles[i].position.x = maxX + OBSTACLE_SPACING;
                if (maxGapY <= minGapY) obstacles[i].gapY = minGapY; else obstacles[i].gapY = minGapY + ((float)rand()/(float)RAND_MAX) * (maxGapY - minGapY);
                obstacles[i].passed = false;
            }
        }

        camera.target.y = fish.position.y;

        UpdateBubbles();

        if (CheckCollision())
        {
            gameState = STATE_GAME_OVER;
            if (IsMusicStreamPlaying(bgMusic)) StopMusicStream(bgMusic);
            if (collisionSound.frameCount > 0) PlaySound(collisionSound);
            if (score > highScore) { highScore = score; SaveHighScore(score); }
        }
    }

    else if (gameState == STATE_PAUSED)
    {
        // Resume by button or P; Play again by button
        if (ButtonPressed(&resumeBtn) || IsKeyPressed(KEY_P))
        {
            gameState = STATE_PLAYING;
        }
        else if (ButtonPressed(&playAgainBtn) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP))
        {
            ResetGame();
        }
    }

    else if (gameState == STATE_GAME_OVER)
    {
            // play again button or keyboard
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || ButtonPressed(&playAgainBtn))
            {
                ResetGame();
            }
    }
}

//------------------------------------------------------------------------------------
// Draw
//------------------------------------------------------------------------------------
void DrawGame(void)
{
    BeginMode3D(camera);

    DrawOcean();
    DrawBubbles();

    for (int i = 0; i < MAX_OBSTACLES; i++)
        DrawObstacle(obstacles[i].position, obstacles[i].gapY);

    DrawFish(fish.position, fish.rotation, fish.tailAngle, fish.finAngle);

    EndMode3D();

    // UI
    if (gameState == STATE_START)
    {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.5f));
        // Title text removed so the start button is uncluttered
        if (highScore>0)
            DrawText(TextFormat("High Score: %i",highScore), GetScreenWidth()/2-80, GetScreenHeight()/2+80, 25, GOLD);

        // start button
        DrawButton(&startBtn, 30);
    }
    else if (gameState == STATE_PLAYING)
    {
        DrawText(TextFormat("Score: %i", score), 30, 30, 50, WHITE);
        DrawText(TextFormat("High: %i", highScore), 30, 90, 30, LIGHTGRAY);

        // pause button
        DrawButton(&pauseBtn, 18);
    }
    else if (gameState == STATE_PAUSED)
    {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.4f));
        DrawText("PAUSED", GetScreenWidth()/2 - 120, GetScreenHeight()/2 - 220, 80, WHITE);
        DrawButton(&resumeBtn, 30);
        DrawButton(&playAgainBtn, 26);
    }
    else if (gameState == STATE_GAME_OVER)
    {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(RED,0.3f));
        DrawText("GAME OVER!", GetScreenWidth()/2-180, GetScreenHeight()/2-120, 60, RED);
        DrawText(TextFormat("Score: %i", score), GetScreenWidth()/2-120, GetScreenHeight()/2, 40, WHITE);
        DrawText("Press ARROW key to restart", GetScreenWidth()/2-200, GetScreenHeight()/2+80, 30, LIGHTGRAY);

        // play again button
        DrawButton(&playAgainBtn, 30);
    }

    DrawFPS(GetScreenWidth()-120,10);
}

//------------------------------------------------------------------------------------
// Reset
//------------------------------------------------------------------------------------
void ResetGame(void)
{
    fish.position = (Vector3){ -5,4,0 };
    fish.rotation = 0;
    fish.vy = 0.0f;

    // Reset difficulty to starting values
    currentGapSize = GAP_SIZE;
    currentObstacleSpeed = FISH_SPEED;
    difficultyTimer = 0.0f;

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        obstacles[i].position = (Vector3){ FISH_X + 10.0f + i*OBSTACLE_SPACING, 0,0 };
        float minGapY = 1.0f + FISH_RADIUS;
        float maxGapY = 8.5f - currentGapSize - FISH_RADIUS;
        if (maxGapY <= minGapY) obstacles[i].gapY = minGapY; else obstacles[i].gapY = minGapY + ((float)rand()/(float)RAND_MAX) * (maxGapY - minGapY);
        obstacles[i].passed = false;
    }

    camera.target.y = 4;
    score = 0;
    gameTime = 0;
    gameState = STATE_PLAYING;
}

//------------------------------------------------------------------------------------
// Fish Drawing
//------------------------------------------------------------------------------------
void DrawFish(Vector3 p, float rot, float tail, float fin)
{
    
    rlPushMatrix();
    rlTranslatef(p.x,p.y,p.z);
    rlRotatef(rot,0,0,1);

    DrawSphere((Vector3){0,0,0},0.6f,ORANGE);
    DrawSphere((Vector3){0.3f,0,0},0.4f,ORANGE);  // fixed

    // tail
    rlPushMatrix();
    rlTranslatef(-0.7f,0,0);
    rlRotatef(tail*RAD2DEG,0,1,0);
    DrawCube((Vector3){-0.3f,0,0},0.6f,0.05f,0.5f,DARKORANGE);
    rlPopMatrix();

    // top fin
    rlPushMatrix();
    rlTranslatef(0,0.5f,0);
    rlRotatef(fin*RAD2DEG,1,0,0);
    DrawCube((Vector3){0,0.2f,0},0.4f,0.4f,0.05f,ORANGE);
    rlPopMatrix();

    // side fins
    rlPushMatrix();
    rlTranslatef(-0.2f,-0.2f,0.5f);
    rlRotatef(fin*RAD2DEG*2,0,1,0);
    DrawCube((Vector3){0,0,0.2f},0.3f,0.05f,0.3f,ORANGE);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(-0.2f,-0.2f,-0.5f);
    rlRotatef(-fin*RAD2DEG*2,0,1,0);
    DrawCube((Vector3){0,0,-0.2f},0.3f,0.05f,0.3f,ORANGE);
    rlPopMatrix();

    // eyes
    DrawSphere((Vector3){0.5f,0.2f,0.3f},0.12f,WHITE);
    DrawSphere((Vector3){0.55f,0.2f,0.3f},0.06f,BLACK);

    DrawSphere((Vector3){0.5f,0.2f,-0.3f},0.12f,WHITE);
    DrawSphere((Vector3){0.55f,0.2f,-0.3f},0.06f,BLACK);

    rlPopMatrix();
}

//------------------------------------------------------------------------------------
// Coral Pipes
//------------------------------------------------------------------------------------
void DrawObstacle(Vector3 p, float gapY)
{
    float pipeHeight = 12.0f;
    Color coral = (Color){255,127,80,255};
    Color dark  = (Color){200,90,60,255};

    float bottomH = gapY;
    DrawCylinder((Vector3){p.x,bottomH/2,p.z},OBSTACLE_RADIUS,OBSTACLE_RADIUS,bottomH,12,coral);
    DrawCylinderWires((Vector3){p.x,bottomH/2,p.z},OBSTACLE_RADIUS,OBSTACLE_RADIUS,bottomH,12,dark);

    float topY = gapY + GAP_SIZE;
    float topH = pipeHeight - topY;

    DrawCylinder((Vector3){p.x,topY+topH/2,p.z},OBSTACLE_RADIUS,OBSTACLE_RADIUS,topH,12,coral);
    DrawCylinderWires((Vector3){p.x,topY+topH/2,p.z},OBSTACLE_RADIUS,OBSTACLE_RADIUS,topH,12,dark);
}

//------------------------------------------------------------------------------------
// Ocean Floor & Walls
//------------------------------------------------------------------------------------
void DrawOcean(void)
{
    DrawPlane((Vector3){0,0,0}, (Vector2){100,40}, (Color){101,67,33,255});

    // seaweed
    for (int i=0;i<40;i++)
    {
        float x = (i*5)-45;
        float z = (i%4)*4 - 6;
        float sway = sinf(gameTime*2+i)*0.3f;

        rlPushMatrix();
        rlTranslatef(x,0,z);
        rlRotatef(sway*20,0,0,1);
        DrawCylinder((Vector3){0,0.75f,0},0.1f,0.1f,1.5f,6,DARKGREEN);
        DrawSphere((Vector3){0,1.5f,0},0.15f,GREEN);
        rlPopMatrix();
    }

    DrawPlane((Vector3){0,6,25}, (Vector2){100,12}, (Color){0,60,100,100});
    DrawPlane((Vector3){0,6,-25},(Vector2){100,12}, (Color){0,60,100,100});
}

//------------------------------------------------------------------------------------
// Bubbles
//------------------------------------------------------------------------------------
void UpdateBubbles(void)
{
    for (int i=0;i<150;i++)
    {
        bubbles[i].position.y += bubbles[i].speed * GetFrameTime();

        bubbles[i].wobble += GetFrameTime()*2;
        bubbles[i].position.x += sinf(bubbles[i].wobble)*0.2f * GetFrameTime();

        if (bubbles[i].position.y > 10)
        {
            bubbles[i].position.y = 0;
            bubbles[i].position.x = (rand()%60)-30;
            bubbles[i].position.z = (rand()%40)-20;
        }
    }
}

void DrawBubbles(void)
{
    for (int i=0;i<150;i++)
    {
        Color c = (Color){200,220,255,(unsigned char)(bubbles[i].alpha*180)};
        DrawSphere(bubbles[i].position, bubbles[i].size, c);

        Vector3 shine = {
            bubbles[i].position.x + bubbles[i].size*0.3f,
            bubbles[i].position.y + bubbles[i].size*0.3f,
            bubbles[i].position.z
        };
        DrawSphere(shine, bubbles[i].size*0.3f, Fade(WHITE,0.6f));
    }
}

//------------------------------------------------------------------------------------
// Collision
//------------------------------------------------------------------------------------
bool CheckCollision(void)
{
    if (fish.position.y <= 0.0f || fish.position.y >= 15.5f) return true; // hitting bounds -> game over

    for (int i=0;i<MAX_OBSTACLES;i++)
    {
        // precise horizontal distance between fish and obstacle center
        float dx = fish.position.x - obstacles[i].position.x;
        float dz = fish.position.z - obstacles[i].position.z;
        float horizDistSq = dx*dx + dz*dz;
        // Make collision detection slightly less sensitive to avoid "near misses" triggering a hit.
        const float COLLISION_SLOP = 1.0f; // small forgiving margin
        float threshold = (OBSTACLE_RADIUS + FISH_RADIUS) - COLLISION_SLOP;
        if (threshold < 0.05f) threshold = (OBSTACLE_RADIUS + FISH_RADIUS) * 0.20f; // safety
        if (horizDistSq <= threshold*threshold)
        {
            // fish center must be fully inside the gap (consider fish radius)
            // Give minimal vertical tolerance so grazing the edge doesn't kill immediately
            const float VERTICAL_TOLERANCE = 0.15f;
            float gapBottom = obstacles[i].gapY + FISH_RADIUS + VERTICAL_TOLERANCE;
            float gapTop = obstacles[i].gapY + currentGapSize - FISH_RADIUS - VERTICAL_TOLERANCE;
            if (fish.position.y < gapBottom || fish.position.y > gapTop) return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------------
// Score Save
//------------------------------------------------------------------------------------
void SaveHighScore(int s)
{
    FILE *f = fopen(HIGH_SCORE_FILE,"w");
    if (f) { fprintf(f,"%d",s); fclose(f); }
}

int LoadHighScore(void)
{
    int s=0;
    FILE *f=fopen(HIGH_SCORE_FILE,"r");
    if (f){ fscanf(f,"%d",&s); fclose(f); }
    return s;
}

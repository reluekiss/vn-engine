#include "scene.h"
#include "raylib.h"
#include <assert.h>
#include <stdbool.h>


typedef struct state_t {
    int framesCounter;

    enum currentScreen_t { 
        LOGO = 0, 
        TITLE,
        GAMEPLAY,
        ENDING,
        NUM_SCREENS // This should always be the last member
    } currentScreen;

    struct logo_t {
        Texture2D texture;
    } logo;

    struct title_t {
        bool initial;
        Music music;
    } title;
    
} state;

void updateScreenState(state *state, scenes *sc)
{
    assert(NUM_SCREENS == 4);
    switch(state->currentScreen)
    {
        case LOGO:
        {
            state->framesCounter++;    // Count frames

            if (state->framesCounter > 120)
            {
                state->currentScreen = TITLE;
            }
        } break;
        case TITLE:
        {
            // TODO: Update TITLE screen variables here!
            if (state->title.initial) {
                PlayMusicStream(state->title.music);
                state->title.initial = false;
            } else {
                UpdateMusicStream(state->title.music);
            }

            if (IsKeyPressed(KEY_ENTER))
            {
                state->currentScreen = GAMEPLAY;
            }
        } break;
        case GAMEPLAY:
        {
            // TODO: Update GAMEPLAY screen variables here!
            updateScene(sc);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            {
                sc->currentScene.nextDirective = true;
            }
        } break;
        case ENDING:
        {
            // TODO: Update ENDING screen variables here!

            if (IsKeyPressed(KEY_ENTER))
            {
                state->currentScreen = TITLE;
            }
        } break;
        default: break;
    }
}

int main(void)
{
    // Initialization
    cfg cfg = {
        .screenWidth = GetScreenWidth(),
        .screenHeight = GetScreenHeight(),
        .targetFPS = 60,
        .fontSize = 20,
        .container = { cfg.screenWidth/5, cfg.screenHeight/5, 4 * cfg.screenWidth/5, 4*cfg.screenHeight/5 },
        .borderColor = MAROON,
        .font = GetFontDefault(),
    }; 

    state state = {
        .currentScreen = LOGO,
        .framesCounter = 0,
        .title.initial = true,
    };

    InitWindow(cfg.screenWidth, cfg.screenHeight, "Distant Hill");

    SetTargetFPS(cfg.targetFPS);

    Image logo_image = LoadImage("assets/images/cat.png");
    state.logo.texture = LoadTextureFromImage(logo_image);
    UnloadImage(logo_image);

    InitAudioDevice();
    state.title.music = LoadMusicStream("assets/music/country.mp3");
    
    scenes *sc = loadScenes("assets/scenes");
    
    sc->currentScene = (sceneState){
        .texture = {0},
        .music = NULL,
        .text = NULL,

        .sceneIndex = 0,
        .directiveIndex = 0,

        .nextDirective = false,
        .nextScene = false,
        .sceneInit = true,
    };
     
    // Main game loop
    while (!WindowShouldClose())
    {
        updateScreenState(&state, sc);

        BeginDrawing();

            ClearBackground(RAYWHITE);

            switch(state.currentScreen)
            {
                // TODO: Break different sections into functions
                case LOGO:
                {
                    // TODO: Draw LOGO screen here!
                    ClearBackground(RAYWHITE);
                    DrawTexture(state.logo.texture, cfg.screenWidth/2 + state.logo.texture.width/2, cfg.screenHeight/2 + state.logo.texture.height/2, WHITE);
                    DrawText("LOGO SCREEN", 20, 20, 40, LIGHTGRAY);
                    DrawText("WAIT for 2 SECONDS...", 290, 220, 20, GRAY);

                } break;
                case TITLE:
                {
                    // TODO: Draw TITLE screen here!
                    DrawRectangle(0, 0, cfg.screenWidth, cfg.screenHeight, GREEN);
                    DrawText("TITLE SCREEN", 20, 20, 40, DARKGREEN);
                    DrawText("PRESS ENTER or TAP to JUMP to GAMEPLAY SCREEN", 120, 220, 20, DARKGREEN);

                } break;
                case GAMEPLAY:
                {
                    drawScene(&sc->currentScene, &cfg);
                    DrawText("GAMEPLAY SCREEN", 20, 20, 40, MAROON);
                    DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);

                } break;
                case ENDING:
                {
                    // TODO: Draw ENDING screen here!
                    DrawRectangle(0, 0, cfg.screenWidth, cfg.screenHeight, BLUE);
                    DrawText("ENDING SCREEN", 20, 20, 40, DARKBLUE);
                    DrawText("PRESS ENTER or TAP to RETURN to TITLE SCREEN", 120, 220, 20, DARKBLUE);

                } break;
                default: break;
            }

        EndDrawing();
    }

    // TODO: Unload all loaded data (textures, fonts, audio) here!
    //freeScenes(sc);
    UnloadMusicStream(state.title.music);
    UnloadTexture(state.logo.texture);
    CloseWindow();

    return 0;
}

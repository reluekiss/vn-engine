#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "raylib.h"
#include "boundedtext.h"
#include "../build/lua/include/lua.h"
#include "../build/lua/include/lualib.h"
#include "../build/lua/include/lauxlib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define PATH_BUFFER_SIZE 512
#define BUFFER_SIZE 256
#define MAX_SPRITES 32
#define MAX_CHOICES 10

typedef struct {
    Texture2D texture;
    Vector2 pos;
    char id[BUFFER_SIZE];
    bool hasID;
} Sprite;

typedef struct {
    char text[BUFFER_SIZE];
    char scene[BUFFER_SIZE];
} Choice;

/* Global state */
static Texture2D gBackground;
static bool gHasBackground = false;

static Sprite gSprites[MAX_SPRITES];
static int gSpriteCount = 0;

static Music gMusic;
static bool gHasMusic = false;

static char gDialogText[BUFFER_SIZE] = "";
static char gDialogName[BUFFER_SIZE] = "";
static bool gHasDialog = false;
static Vector2 gDialogPos;
static bool gDialogHasPos = false;

static Choice gChoices[MAX_CHOICES];
static int gChoiceCount = 0;
static char gTitleText[BUFFER_SIZE] = "TITLE SCREEN";
static bool gShowTitleScreen = true;

static lua_State *gL = NULL;

static void ResetScene(void) {
    if (gHasBackground) {
        UnloadTexture(gBackground);
        gHasBackground = false;
    }
    for (int i = 0; i < gSpriteCount; i++)
        UnloadTexture(gSprites[i].texture);
    gSpriteCount = 0;
    if (gHasMusic) {
        StopMusicStream(gMusic);
        UnloadMusicStream(gMusic);
        gHasMusic = false;
    }
    gDialogText[0] = '\0';
    gDialogName[0] = '\0';
    gHasDialog = false;
    gDialogHasPos = false;
}

static void LoadScene(const char *sceneFile) {
    ResetScene();
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/scenes/%s", sceneFile);
    if (luaL_dofile(gL, path) != LUA_OK) {
        const char *error = lua_tostring(gL, -1);
        fprintf(stderr, "Error loading scene: %s\n", error);
    }
}

void LoadBackground(const char *file) {
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/images/%s", file);
    if (gHasBackground) UnloadTexture(gBackground);
    gBackground = LoadTexture(path);
    gHasBackground = true;
}

static int l_load_background(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    LoadBackground(file);
    TraceLog(LOG_INFO, "Loaded background: %s", file);
    return 0;
}

void LoadSprite(const char *file, Vector2 pos, const char *id) {
    if (gSpriteCount >= MAX_SPRITES) return;
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/images/%s", file);
    Texture2D tex = LoadTexture(path);
    gSprites[gSpriteCount].texture = tex;
    gSprites[gSpriteCount].pos = pos;
    if (id) {
        strncpy(gSprites[gSpriteCount].id, id, BUFFER_SIZE - 1);
        gSprites[gSpriteCount].id[BUFFER_SIZE - 1] = '\0';
        gSprites[gSpriteCount].hasID = true;
    } else {
        gSprites[gSpriteCount].hasID = false;
    }
    gSpriteCount++;
}

static int l_load_sprite(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    const char *id = NULL;
    if (lua_gettop(L) >= 4 && lua_isstring(L, 4))
        id = lua_tostring(L, 4);
    Vector2 pos = { (float)x, (float)y };
    LoadSprite(file, pos, id);
    TraceLog(LOG_INFO, "Loaded sprite: %s at (%d, %d) id: %s", file, x, y, id ? id : "none");
    return 0;
}

static int l_unload_sprite(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    for (int i = 0; i < gSpriteCount; i++) {
        if (gSprites[i].hasID && strcmp(gSprites[i].id, id) == 0) {
            UnloadTexture(gSprites[i].texture);
            for (int j = i; j < gSpriteCount - 1; j++)
                gSprites[j] = gSprites[j + 1];
            gSpriteCount--;
            TraceLog(LOG_INFO, "Unloaded sprite with id: %s", id);
            break;
        }
    }
    return 0;
}

static int l_play_music(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    float start = 0.0f;
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 2))
        start = lua_tonumber(L, 2);
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/music/%s", file);
    if (gHasMusic) {
        StopMusicStream(gMusic);
        UnloadMusicStream(gMusic);
    }
    gMusic = LoadMusicStream(path);
    PlayMusicStream(gMusic);
    if (start > 0.0f)
        SeekMusicStream(gMusic, start);
    gHasMusic = true;
    TraceLog(LOG_INFO, "Playing music: %s from %.2f sec", file, start);
    return 0;
}

static int l_play_sound(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/sounds/%s", file);
    Sound s = LoadSound(path);
    PlaySound(s);
    TraceLog(LOG_INFO, "Played sound: %s", file);
    return 0;
}

static int l_show_text(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    strncpy(gDialogText, text, BUFFER_SIZE - 1);
    gDialogText[BUFFER_SIZE - 1] = '\0';
    gHasDialog = true;
    if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
        const char *name = lua_tostring(L, 2);
        strncpy(gDialogName, name, BUFFER_SIZE - 1);
        gDialogName[BUFFER_SIZE - 1] = '\0';
    } else {
        gDialogName[0] = '\0';
    }
    if (lua_gettop(L) >= 4 && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
        gDialogPos.x = (float)lua_tointeger(L, 3);
        gDialogPos.y = (float)lua_tointeger(L, 4);
        gDialogHasPos = true;
    } else {
        gDialogHasPos = false;
    }
    TraceLog(LOG_INFO, "Show text: %s", gDialogText);
    return 0;
}

static int l_clear_text(lua_State *L) {
    gDialogText[0] = '\0';
    gDialogName[0] = '\0';
    gHasDialog = false;
    gDialogHasPos = false;
    return 0;
}

static int l_set_title(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    strncpy(gTitleText, title, BUFFER_SIZE - 1);
    gTitleText[BUFFER_SIZE - 1] = '\0';
    return 0;
}

static int l_set_choices(lua_State *L) {
    if (!lua_istable(L, 1)) return 0;
    gChoiceCount = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) && gChoiceCount < MAX_CHOICES) {
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "text");
            lua_getfield(L, -2, "scene");
            const char *text = luaL_checkstring(L, -2);
            const char *scene = luaL_checkstring(L, -1);
            strncpy(gChoices[gChoiceCount].text, text, BUFFER_SIZE - 1);
            gChoices[gChoiceCount].text[BUFFER_SIZE - 1] = '\0';
            strncpy(gChoices[gChoiceCount].scene, scene, BUFFER_SIZE - 1);
            gChoices[gChoiceCount].scene[BUFFER_SIZE - 1] = '\0';
            gChoiceCount++;
            lua_pop(L, 2); 
        }
        lua_pop(L, 1);
    }
    return 0;
}

static int l_start_game(lua_State *L) {
    gShowTitleScreen = false;
    return 0;
}

int main(void) {
    const int screenWidth = 800, screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "LuaJIT Engine");
    InitAudioDevice();
    Shader spriteOutline = LoadShader(0, TextFormat("assets/shaders/outline-%i.fs", GLSL_VERSION));

    gL = luaL_newstate();
    luaL_openlibs(gL);

    /* Register API functions */
    lua_register(gL, "load_background", l_load_background);
    lua_register(gL, "load_sprite", l_load_sprite);
    lua_register(gL, "unload_sprite", l_unload_sprite);
    lua_register(gL, "play_music", l_play_music);
    lua_register(gL, "play_sound", l_play_sound);
    lua_register(gL, "show_text", l_show_text);
    lua_register(gL, "clear_text", l_clear_text);
    lua_register(gL, "set_title", l_set_title);
    lua_register(gL, "set_choices", l_set_choices);
    lua_register(gL, "start_game", l_start_game);

    /* Load initial script (e.g., title screen setup) */
    if (luaL_dofile(gL, "assets/scenes/script.lua") != LUA_OK) {
        const char *error = lua_tostring(gL, -1);
        fprintf(stderr, "Error loading Lua script: %s\n", error);
        return 1;
    }

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (gHasMusic) UpdateMusicStream(gMusic);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (gShowTitleScreen) {
                int titleFont = 40;
                int titleWidth = MeasureText(gTitleText, titleFont);
                DrawText(gTitleText, (screenWidth - titleWidth) / 2, 50, titleFont, DARKGREEN);

                int choiceFont = 20;
                int spacing = 20;
                int buttonHeight = 40;
                int startY = 150;
                for (int i = 0; i < gChoiceCount; i++) {
                    int textWidth = MeasureText(gChoices[i].text, choiceFont);
                    int buttonWidth = textWidth + 20;
                    int buttonX = (screenWidth - buttonWidth) / 2;
                    int buttonY = startY + i * (buttonHeight + spacing);
                    Rectangle buttonRect = { buttonX, buttonY, buttonWidth, buttonHeight };
                    DrawRectangleRec(buttonRect, LIGHTGRAY);
                    DrawRectangleLines(buttonX, buttonY, buttonWidth, buttonHeight, BLACK);
                    DrawText(gChoices[i].text, buttonX + 10, buttonY + 10, choiceFont, BLACK);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), buttonRect)) {
                        LoadScene(gChoices[i].scene);
                        gShowTitleScreen = false;
                    }
                }
            } else {
                if (gHasBackground) {
                    Texture2D bgTex = gBackground;
                    int windowWidth = GetScreenWidth();
                    int windowHeight = GetScreenHeight();
                    float scale_bg = (float)windowHeight / (float)bgTex.height;
                    float desired_tex_width = (float)windowWidth / scale_bg;
                    float crop_x = (bgTex.width - desired_tex_width) / 2.0f;
                    Rectangle srcRect = { crop_x, 0, desired_tex_width, (float)bgTex.height };
                    Rectangle dstRect = { 0, 0, (float)windowWidth, (float)windowHeight };
                    DrawTexturePro(bgTex, srcRect, dstRect, (Vector2){0, 0}, 0.0f, WHITE);
            
                    float outlineSize = 8.0f;
                    float outlineColor[4] = { 0.2f, 0.2f, 0.2f, 0.2f };
            
                    for (int i = 0; i < gSpriteCount; i++) {
                        Texture2D sprTex = gSprites[i].texture;
                        float drawn_x = (gSprites[i].pos.x - crop_x) * scale_bg;
                        float drawn_y = gSprites[i].pos.y * scale_bg;
                        float sprite_scale = (4.0/3.0 * windowHeight) / (float)sprTex.height;
                        Rectangle sprSrc = { 0, 0, (float)sprTex.width, (float)sprTex.height };
                        Rectangle sprDst = { drawn_x, drawn_y, sprTex.width * sprite_scale, sprTex.height * sprite_scale };
            
                        float textureSize[2] = { (float)sprTex.width, (float)sprTex.height };
                        int outlineSizeLoc = GetShaderLocation(spriteOutline, "outlineSize");
                        int outlineColorLoc = GetShaderLocation(spriteOutline, "outlineColor");
                        int textureSizeLoc = GetShaderLocation(spriteOutline, "textureSize");
                        SetShaderValue(spriteOutline, outlineSizeLoc, &outlineSize, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(spriteOutline, outlineColorLoc, outlineColor, SHADER_UNIFORM_VEC4);
                        SetShaderValue(spriteOutline, textureSizeLoc, textureSize, SHADER_UNIFORM_VEC2);
            
                        BeginShaderMode(spriteOutline);
                        DrawTexturePro(sprTex, sprSrc, sprDst, (Vector2){0, 0}, 0.0f, WHITE);
                        EndShaderMode();
                    }
                }
                if (gHasDialog) {
                    const float defaultXRel = 60.0f / (float)screenWidth;
                    const float defaultYRel = 330.0f / (float)screenHeight;
                    const float defaultWidthRel = 685.0f / (float)screenWidth;
                    const float defaultHeightRel = 100.0f / (float)screenHeight;
                    Rectangle textBox;
                    if (gHasDialog) {
                        float xRel = gDialogPos.x / (float)screenWidth;
                        float yRel = gDialogPos.y / (float)screenHeight;
                        textBox.x = xRel * GetScreenWidth();
                        textBox.y = yRel * GetScreenHeight();
                    } else {
                        textBox.x = defaultXRel * GetScreenWidth();
                        textBox.y = defaultYRel * GetScreenHeight();
                    }
                    textBox.width = defaultWidthRel * GetScreenWidth();
                    textBox.height = defaultHeightRel * GetScreenHeight();
                    
                    int textPadding = 10;
                    Rectangle innerBox = { 
                        textBox.x + textPadding, 
                        textBox.y + textPadding, 
                        textBox.width - 2 * textPadding, 
                        textBox.height - 2 * textPadding 
                    };
                    
                    DrawRectangleRec(textBox, Fade(BLACK, 0.5f));
                    if (gDialogName[0])
                        DrawText(gDialogName, textBox.x + 5, textBox.y - 25, 20, WHITE);
                    DrawTextBoxed(GetFontDefault(), gDialogText, innerBox, 20, 2, true, WHITE);
                }
            }
        EndDrawing();
    }

    if (gHasBackground) UnloadTexture(gBackground);
    for (int i = 0; i < gSpriteCount; i++)
        UnloadTexture(gSprites[i].texture);
    if (gHasMusic) {
        StopMusicStream(gMusic);
        UnloadMusicStream(gMusic);
    }
    lua_close(gL);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

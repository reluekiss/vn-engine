#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "../external/raygui.h"
#include "../external/cc.h"
#include "boundedtext.h"
#include "../build/lua/lua.h"
#include "../build/lua/lualib.h"
#include "../build/lua/lauxlib.h"

#define GLSL_VERSION 330

#define PATH_BUFFER_SIZE 512
#define BUFFER_SIZE 256
#define CACHE_SIZE 32
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
static bool gQuit = false;

omap(char *, Texture2D) backgroundCache;
omap(char *, Music)       musicCache;
omap(char *, Texture2D)   spriteCache;
list(char *) backgroundLRU;
list(char *) musicLRU;
list(char *) spriteLRU;

typedef struct {
    int nLevels;
    lua_Debug frames[CACHE_SIZE];
} LuaCallStack;

typedef struct {
    lua_State *sceneThread;
    LuaCallStack callStack;
    Texture2D background;
    Music music;
    Sprite sprites[CACHE_SIZE];
    int spriteCount;
    bool isPaused;
    bool hasMusic;
    bool hasBackground;
    char dialogText[BUFFER_SIZE];
    char dialogName[BUFFER_SIZE];
    Color textColor;
    Color dialogNameColor;
    bool hasDialog;
    Vector2 dialogPos;
    bool dialogHasPos;
    Choice choices[MAX_CHOICES];
    int choiceCount;
    const char* moduleFolder;
    char bgfile[PATH_BUFFER_SIZE];
    char musicfile[PATH_BUFFER_SIZE];
    char spritefiles[CACHE_SIZE][PATH_BUFFER_SIZE];
} GameState;

static GameState gGameState = {
    .spriteCount = 0,
    .isPaused = false,
    .hasMusic = false,
    .hasBackground = false,
    .dialogText = "",
    .dialogName = "",
    .textColor = WHITE,
    .dialogNameColor = WHITE,
    .hasDialog = false,
    .dialogHasPos = false,
    .choiceCount = 0,
    .moduleFolder = "",
    .bgfile = "",
    .musicfile = "",
    .spritefiles = ""
};

vec(GameState) gameStateStack;
static lua_State *gL = NULL;
static lua_State *gSceneThread = NULL;

enum {
    MODULE,
    SAVE,
    LOAD,
    SETTINGS,
    MAIN,
    QUIT,
    NONE,
} menu = NONE;

enum {
    TITLE,
    GAME,
} screen = TITLE;

/* Exposed Variables */
static char gCurrentScene[BUFFER_SIZE] = "";
static char gLastScene[BUFFER_SIZE] = "";

static void updateLRU(list(char *) *lruList, const char *key) {
    for_each(lruList, el) {
        if (strcmp(*el, key) == 0) {
            erase(lruList, el);
            break;
        }
    }
    insert(lruList, first(lruList), (char *)key);
}

void saveLuaCallStack(lua_State *L, LuaCallStack *stack) {
    int level = 0;
    while (level < CACHE_SIZE && lua_getstack(L, level, &stack->frames[level])) {
        lua_getinfo(L, "nSl", &stack->frames[level]);
        level++;
    }
    stack->nLevels = level;
}
// TODO: find a better way of recreating the call stack other than using the scene thread
void pushGameState(void) {
    gGameState.sceneThread = gSceneThread;
    saveLuaCallStack(gSceneThread, &gGameState.callStack);
    push(&gameStateStack, gGameState);
}

static void LoadScene(const char *sceneFile) {
    strncpy(gLastScene, gCurrentScene, BUFFER_SIZE - 1);
    gLastScene[BUFFER_SIZE - 1] = '\0';
    strncpy(gCurrentScene, sceneFile, BUFFER_SIZE - 1);
    gCurrentScene[BUFFER_SIZE - 1] = '\0';
    lua_pushstring(gL, gLastScene);
    lua_setglobal(gL, "last_scene");

    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/%s", gGameState.moduleFolder, sceneFile);
    gSceneThread = lua_newthread(gL);
    if (luaL_loadfile(gSceneThread, path) != LUA_OK) {
        const char *error = lua_tostring(gSceneThread, -1);
        fprintf(stderr, "Error loading scene: %s\n", error);
        return;
    }
    int nres = 0;
    int status = lua_resume(gSceneThread, gL, 0, &nres);
    if (status != LUA_YIELD && status != LUA_OK) {
        const char *error = lua_tostring(gSceneThread, -1);
        fprintf(stderr, "Error starting scene: %s\n", error);
    }
}

/* Lua API */
static int l_module_init(lua_State *L) {
    gGameState.moduleFolder = luaL_checkstring(L, 1);
    return 0;
}

static int l_load_background(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/images/%s", gGameState.moduleFolder, file);

    Texture2D *cached = get(&backgroundCache, path);
    if (cached) {
        gGameState.background = *cached;
        TraceLog(LOG_INFO, "Loaded cached background: %s", file);
        updateLRU(&backgroundLRU, path);
    } else {
        gGameState.background = LoadTexture(path);
        char *key = strdup(path);
        insert(&backgroundCache, key, gGameState.background);
        insert(&backgroundLRU, first(&backgroundLRU), key);
        TraceLog(LOG_INFO, "Loaded new background: %s", file);
    }
    strncpy(gGameState.bgfile, path, PATH_BUFFER_SIZE);
    gGameState.hasBackground = true;
    return 0;
}

static int l_load_sprite(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    const char *id = NULL;
    if (lua_gettop(L) >= 4 && lua_isstring(L, 4))
        id = lua_tostring(L, 4);
    Vector2 pos = { (float)x, (float)y };

    if (gGameState.spriteCount >= CACHE_SIZE)
        return -1;

    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/images/%s", gGameState.moduleFolder, file);

    Texture2D *cached = get(&spriteCache, path);
    if (cached) {
        gGameState.sprites[gGameState.spriteCount].texture = *cached;
        TraceLog(LOG_INFO, "Loaded cached sprite: %s", file);
        updateLRU(&spriteLRU, path);
    } else {
        gGameState.sprites[gGameState.spriteCount].texture = LoadTexture(path);
        char *key = strdup(path);
        insert(&spriteCache, key, gGameState.sprites[gGameState.spriteCount].texture);
        insert(&spriteLRU, first(&spriteLRU), key);
        TraceLog(LOG_INFO, "Loaded new sprite: %s", file);
    }
    gGameState.sprites[gGameState.spriteCount].pos = pos;
    if (id) {
        strncpy(gGameState.sprites[gGameState.spriteCount].id, id, BUFFER_SIZE - 1);
        gGameState.sprites[gGameState.spriteCount].id[BUFFER_SIZE - 1] = '\0';
        gGameState.sprites[gGameState.spriteCount].hasID = true;
    } else {
        gGameState.sprites[gGameState.spriteCount].hasID = false;
    }
    strncpy(gGameState.spritefiles[gGameState.spriteCount], path, PATH_BUFFER_SIZE);
    gGameState.spriteCount++;
    return 0;
}

static int l_unload_sprite(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    for (int i = 0; i < gGameState.spriteCount; i++) {
        if (gGameState.sprites[i].hasID && strcmp(gGameState.sprites[i].id, id) == 0) {
            UnloadTexture(gGameState.sprites[i].texture);
            if(!erase(&spriteCache, gGameState.spritefiles[i])) TraceLog(LOG_INFO, "Sprite was now in cache: %s", id); 
            for (int j = i; j < gGameState.spriteCount - 1; j++) {
                strncpy(gGameState.spritefiles[j], gGameState.spritefiles[j + 1], PATH_BUFFER_SIZE);
                gGameState.sprites[j] = gGameState.sprites[j + 1];
            }
            gGameState.spriteCount--;
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
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/music/%s", gGameState.moduleFolder, file);

    Music *cached = get(&musicCache, path);
    if (cached) {
        gGameState.music = *cached;
        TraceLog(LOG_INFO, "Loaded cached music: %s", file);
        updateLRU(&musicLRU, path);
    } else {
        gGameState.music = LoadMusicStream(path);
        char *key = strdup(path);
        insert(&musicCache, key, gGameState.music);
        insert(&musicLRU, first(&musicLRU), key);
        TraceLog(LOG_INFO, "Loaded new music: %s", file);
    }
    PlayMusicStream(gGameState.music);
    if (start > 0.0f)
        SeekMusicStream(gGameState.music, start);
    strncpy(gGameState.musicfile, path, PATH_BUFFER_SIZE);
    gGameState.hasMusic = true;
    return 0;
}

static int l_play_sound(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/music/%s", gGameState.moduleFolder, file);
    Sound s = LoadSound(path);
    PlaySound(s);
    TraceLog(LOG_INFO, "Played sound: %s", file);
    return 0;
}

static int l_show_text(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "name");
    const char *char_name = luaL_checkstring(L, -1);
    strncpy(gGameState.dialogName, char_name, BUFFER_SIZE - 1);
    gGameState.dialogName[BUFFER_SIZE - 1] = '\0';
    lua_pop(L, 1);

    lua_getfield(L, 1, "color");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "r");
        int r = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "g");
        int g = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "b");
        int b = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "a");
        int a = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        gGameState.dialogNameColor = (Color){ r, g, b, a };
    } else {
        gGameState.dialogNameColor = WHITE;
    }
    lua_pop(L, 1);

    const char *text = luaL_checkstring(L, 2);
    strncpy(gGameState.dialogText, text, BUFFER_SIZE - 1);
    gGameState.dialogText[BUFFER_SIZE - 1] = '\0';

    if (lua_gettop(L) >= 3 && lua_istable(L, 3)) {
        lua_getfield(L, 3, "r");
        int tr = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, 3, "g");
        int tg = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, 3, "b");
        int tb = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, 3, "a");
        int ta = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        gGameState.textColor = (Color){ tr, tg, tb, ta };
    } else {
        gGameState.textColor = WHITE;
    }

    if (lua_gettop(L) >= 5 && lua_isnumber(L, 4) && lua_isnumber(L, 5)) {
        gGameState.dialogPos.x = (float)lua_tointeger(L, 4);
        gGameState.dialogPos.y = (float)lua_tointeger(L, 5);
        gGameState.dialogHasPos = true;
    } else {
        gGameState.dialogHasPos = false;
    }

    gGameState.hasDialog = true;
    TraceLog(LOG_INFO, "Show text: %s", gGameState.dialogText);
    return lua_yield(L, 0);
}

static int l_clear_text(lua_State *L) {
    (void)L;
    gGameState.dialogText[0] = '\0';
    gGameState.dialogName[0] = '\0';
    gGameState.hasDialog = false;
    gGameState.dialogHasPos = false;
    return 0;
}

static int l_set_choices(lua_State *L) {
    if (!lua_istable(L, 1)) return 0;
    gGameState.choiceCount = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) && gGameState.choiceCount < MAX_CHOICES) {
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "text");
            lua_getfield(L, -2, "scene");
            const char *text = luaL_checkstring(L, -2);
            const char *scene = luaL_checkstring(L, -1);
            strncpy(gGameState.choices[gGameState.choiceCount].text, text, BUFFER_SIZE - 1);
            gGameState.choices[gGameState.choiceCount].text[BUFFER_SIZE - 1] = '\0';
            strncpy(gGameState.choices[gGameState.choiceCount].scene, scene, BUFFER_SIZE - 1);
            gGameState.choices[gGameState.choiceCount].scene[BUFFER_SIZE - 1] = '\0';
            gGameState.choiceCount++;
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    }
    return lua_yield(L, 0);
}

static int l_quit(lua_State *L) {
    (void)L;
    gQuit = true;
    return 0;
}

static int l_pop_state(lua_State *L) {
    if (size(&gameStateStack) == 0) {
        lua_pushboolean(L, false);
        return 1;
    }
    GameState *state = get(&gameStateStack, size(&gameStateStack) - 1);
    gGameState = *state;
    gSceneThread = gGameState.sceneThread;  // restore the coroutine (and its call stack)
    erase(&gameStateStack, size(&gameStateStack) - 1);
    lua_pushboolean(L, true);
    return 1;
}
/* End Lua API */

typedef struct{;
    int font;
    int padding;
    int spacing;
    int buttonHeight; // if 0, computed as font + 2*padding
    bool center;      // if true, center horizontally on screen; if false, use baseRect.x and baseRect.width
    Rectangle baseRect; // used for starting y (and x when not centered)
} OptionsStyle;

void genericChoose(void* data, int* shortcuts, int count, const char* (*getLabel)(int, void*), void (*onSelect)(int, void*), OptionsStyle style) {
    int btnHeight = (style.buttonHeight != 0) ? style.buttonHeight : style.font + 2 * style.padding;
    int textWidth = 0;
    int maxWidth = 0;
    for (int i = 0; i < count; i++) {
        textWidth = MeasureText(getLabel(i, data), style.font);
        maxWidth = maxWidth < textWidth ? textWidth : maxWidth;
    }
    int btnWidth = textWidth + 2 * style.padding;
    int btnX = style.center ? (GetScreenWidth() - btnWidth) / 2 : style.baseRect.x + (style.baseRect.width - btnWidth) / 2;
    for (int i = 0; i < count; i++) {
        int btnY = style.baseRect.y + i * (btnHeight + style.spacing);
        Rectangle btnRect = { (float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight };

        if (IsKeyPressed(shortcuts[i]) || GuiButton(btnRect, getLabel(i, data)))
            onSelect(i, data);
    }
}

const char* getModuleLabel(int index, void* data) {
    FilePathList* scenes = (FilePathList*)data;
    return GetFileName(scenes->paths[index]);
}

void onModuleSelect(int index, void* data) {
    FilePathList* scenes = (FilePathList*)data;
    const char* fileName = GetFileName(scenes->paths[index]);
    LoadScene(fileName);
    screen = GAME;
    menu = NONE;
}

void chooseModule(FilePathList* scenes) {
    int shortCut[] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO };
    OptionsStyle style = {
        .font = 20,
        .padding = 5,
        .spacing = 10,
        .buttonHeight = 0,  // computed as font + 2*padding
        .center = false,
        .baseRect = { (float)GetScreenWidth()/2 - 200, (float)GetScreenHeight()/2 - 100, 400, 250 },
    };
    genericChoose((void*)scenes, shortCut, scenes->count, getModuleLabel, onModuleSelect, style);
}

static inline const char* getSceneLabel(int index, void* data) {
    Choice* choices = (Choice*)data;
    return choices[index].text;
}

static inline void onSceneSelect(int index, void* data) {
    Choice* choices = (Choice*)data;
    gGameState.choiceCount = 0;
    LoadScene(choices[index].scene);
}

void chooseScene() {
    int shortCut[] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO };
    OptionsStyle style = {
        .font = 20,
        .padding = 5,
        .spacing = 10,
        .buttonHeight = 0,
        .center = true,
        .baseRect = { (float)GetScreenWidth()/2 - 200, (float)GetScreenHeight()/2 - 100, 400, 250 },
    };
    genericChoose((void*)gGameState.choices, shortCut, gGameState.choiceCount, getSceneLabel, onSceneSelect, style);
}

static inline const char* getMenuItems(int index, void* data) {
    char** choices = (char**)data;
    return choices[index];
}

static inline void menuSelect(int index, void* data) {
    (void)data;    
    switch (index) {
        case 0: {
            menu = MODULE;
        } break;
        case 1: {
            menu = SETTINGS;
        } break;
        case 2: {
            menu = LOAD;
        } break;
        case 3: {
            gQuit = true;
        } break;
        default: break;
    }
}

void mainMenu() {
    int shortCut[] = { KEY_NULL, KEY_L, KEY_S, KEY_Q }; 
    char* choices[] = { "Select Module", "Load Game", "Settings", "Quit" };
    int count = 4;
    OptionsStyle style = {
        .font = 40,
        .padding = 5,
        .spacing = 10,
        .buttonHeight = 30,  // computed as font + 2*padding
        .center = false,
        .baseRect = { (float)GetScreenWidth()/2 - 200, (float)GetScreenHeight()/2 - 100, 400, 250 },
    };
    genericChoose((void*)choices, shortCut, count, getMenuItems, menuSelect, style);
}

static inline void pauseMenuSelect(int index, void* data) {
    (void)data;    
    switch (index) {
        case 0: {
            gGameState.isPaused = false;
        } break;
        case 1: {
            menu = SAVE;
        } break;
        case 2: {
            menu = LOAD;
        } break;
        case 3: {
            menu = SETTINGS;
            gGameState.isPaused = false;
        } break;
        case 4: {
            if (gGameState.hasBackground) UnloadTexture(gGameState.background);
            gGameState.hasBackground = false;
            for (int i = 0; i < gGameState.spriteCount; i++)
                UnloadTexture(gGameState.sprites[i].texture);
            if (gGameState.hasMusic) {
                StopMusicStream(gGameState.music);
                UnloadMusicStream(gGameState.music);
            }
            cleanup(&backgroundCache);
            cleanup(&musicCache);
            cleanup(&spriteCache);
            cleanup(&gameStateStack);
            cleanup(&backgroundLRU);
            cleanup(&musicLRU);
            cleanup(&spriteLRU);

            gGameState.hasDialog = false;
            gGameState.moduleFolder = "";
            gGameState.isPaused = false;
            menu = NONE;
            screen = TITLE;
        } break;
        case 5: {
            gQuit = true;
        } break;
        default: break;
    }
}

void pauseMenu() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GRAY, 0.8f));
    int shortCut[] = { KEY_R, KEY_G, KEY_L, KEY_S, KEY_M, KEY_Q }; 
    char* choices[] = { "Resume", "Save Game", "Load Game", "Settings", "Main Menu", "Quit" };
    int count = 6;
    OptionsStyle style = {
        .font = 40,
        .padding = 5,
        .spacing = 10,
        .buttonHeight = 30,  // computed as font + 2*padding
        .center = false,
        .baseRect = { (float)GetScreenWidth()/2 - 200, (float)GetScreenHeight()/2 - 100, 400, 250 },
    };
    genericChoose((void*)choices, shortCut, count, getMenuItems, pauseMenuSelect, style);
}

void updateBackground(Shader* spriteOutline) {
    Texture2D bgTex = gGameState.background;
    int windowWidth = GetScreenWidth(), windowHeight = GetScreenHeight();
    float scale_bg = (float)windowHeight / bgTex.height;
    float desired_tex_width = (float)windowWidth / scale_bg;
    float crop_x = (bgTex.width - desired_tex_width) / 2.0f;
    Rectangle srcRect = { crop_x, 0, desired_tex_width, (float)bgTex.height };
    Rectangle dstRect = { 0, 0, (float)windowWidth, (float)windowHeight };
    DrawTexturePro(bgTex, srcRect, dstRect, (Vector2){0,0}, 0.0f, WHITE);

    if (gGameState.spriteCount > 0) {
        float outlineSize = 8.0f;
        float outlineColor[4] = { 0.2f, 0.2f, 0.2f, 0.2f };

        for (int i = 0; i < gGameState.spriteCount; i++) {
            Texture2D sprTex = gGameState.sprites[i].texture;
            float drawn_x = (gGameState.sprites[i].pos.x - crop_x) * scale_bg;
            float drawn_y = gGameState.sprites[i].pos.y * scale_bg;
            float sprite_scale = (4.0/3.0 * windowHeight) / (float)sprTex.height;
            Rectangle sprSrc = { 0, 0, (float)sprTex.width, (float)sprTex.height };
            Rectangle sprDst = { drawn_x, drawn_y, sprTex.width * sprite_scale, sprTex.height * sprite_scale };

            float textureSize[2] = { (float)sprTex.width, (float)sprTex.height };
            int outlineSizeLoc = GetShaderLocation(*spriteOutline, "outlineSize");
            int outlineColorLoc = GetShaderLocation(*spriteOutline, "outlineColor");
            int textureSizeLoc = GetShaderLocation(*spriteOutline, "textureSize");
            SetShaderValue(*spriteOutline, outlineSizeLoc, &outlineSize, SHADER_UNIFORM_FLOAT);
            SetShaderValue(*spriteOutline, outlineColorLoc, outlineColor, SHADER_UNIFORM_VEC4);
            SetShaderValue(*spriteOutline, textureSizeLoc, textureSize, SHADER_UNIFORM_VEC2);

            BeginShaderMode(*spriteOutline);
            DrawTexturePro(sprTex, sprSrc, sprDst, (Vector2){0, 0}, 0.0f, WHITE);
            EndShaderMode();
        }
    }
}

const int screenWidth = 800, screenHeight = 450;
// Proportions for text box
const float defaultXRel = 60.0f / (float)screenWidth;
const float defaultYRel = 330.0f / (float)screenHeight;
const float defaultWidthRel = 685.0f / (float)screenWidth;
const float defaultHeightRel = 100.0f / (float)screenHeight;
bool forward = false;
void updateText() {
    Rectangle textBox;
    if (gGameState.dialogHasPos) {
        textBox.x = gGameState.dialogPos.x + defaultXRel * GetScreenWidth();
        textBox.y = gGameState.dialogPos.y + defaultYRel * GetScreenHeight();
    } else {
        textBox.x = defaultXRel * GetScreenWidth();
        textBox.y = defaultYRel * GetScreenHeight();
    }
    textBox.width = defaultWidthRel * GetScreenWidth();
    textBox.height = defaultHeightRel * GetScreenHeight();
    int textPadding = 10;
    Rectangle innerBox = { textBox.x + textPadding, textBox.y + textPadding,
                           textBox.width - 2 * textPadding, textBox.height - 2 * textPadding };
    DrawRectangleRec(textBox, Fade(BLACK, 0.5f));
    
    if (gGameState.dialogName[0])
        DrawText(gGameState.dialogName, textBox.x + 5, textBox.y - 25, 20, gGameState.dialogNameColor);
    DrawTextBoxed(GetFontDefault(), gGameState.dialogText, innerBox, 20, 2, true, gGameState.textColor);

    int btnWidth = 40, btnHeight = 30;
    Rectangle backBut = { textBox.width + textBox.x - 2*10 - 2*btnWidth, textBox.y + textBox.height - btnHeight - 10, btnWidth, btnHeight };
    if (GuiButton(backBut, "#130#")) {
        l_pop_state(gL);
    }
    Rectangle forwardBut = { textBox.width + textBox.x - 10 - btnWidth, textBox.y + textBox.height - btnHeight - 10, btnWidth, btnHeight };
    if (GuiButton(forwardBut, "#131#")) {
        forward = true;
    }
}

void cachePrefetched(const char *funcName, const char *path) {
    if (strcmp(funcName, "load_background") == 0) {
        Texture2D *cached = get(&backgroundCache, path);
        if (!cached) {
            Texture2D tex = LoadTexture(path);
            char *key = strdup(path);
            insert(&backgroundCache, key, tex);
            TraceLog(LOG_INFO, "Cached background: %s", path);
        }
    } else if (strcmp(funcName, "load_sprite") == 0) {
        Texture2D *cached = get(&spriteCache, path);
        if (!cached) {
            Texture2D tex = LoadTexture(path);
            char *key = strdup(path);
            insert(&spriteCache, key, tex);
            TraceLog(LOG_INFO, "Cached sprite: %s", path);
        }
    } else if (strcmp(funcName, "load_music") == 0) {
        Music *cached = get(&musicCache, path);
        if (!cached) {
            Music music = LoadMusicStream(path);
            char *key = strdup(path);
            insert(&musicCache, key, music);
            TraceLog(LOG_INFO, "Cached music: %s", path);
        }
    }
}

// TODO: Investigate stack prefetching as it doesn't seem to correctly cache things
void prefetchAssets(lua_State *L) {
    lua_Debug ar;
    int level = 0;
    while (lua_getstack(L, level, &ar)) {
        lua_getinfo(L, "nSl", &ar);
        if (ar.name &&
            (strcmp(ar.name, "load_background") == 0 ||
             strcmp(ar.name, "load_sprite") == 0 ||
             strcmp(ar.name, "load_music") == 0)) {
            // Assume the first local variable is the asset file path.
            const char *assetPath = lua_getlocal(L, &ar, 1);
            if (assetPath) {
                cachePrefetched(ar.name, assetPath);
                lua_pop(L, 1); // pop the local
            }
        }
        level++;
    }
}

void invAssets(void) {
    while (size(&backgroundLRU) > CACHE_SIZE) {
        char *key = (char *)last(&backgroundLRU);
        Texture2D *tex = get(&backgroundCache, key);
        if (tex) UnloadTexture(*tex);
        erase(&backgroundCache, key);
        erase(&backgroundLRU, last(&backgroundLRU));
        TraceLog(LOG_INFO, "Evicted background: %s", key);
    }
    while (size(&musicLRU) > CACHE_SIZE) {
        char *key = (char *)last(&musicLRU);
        Music *mus = get(&musicCache, key);
        if (mus) {
            StopMusicStream(*mus);
            UnloadMusicStream(*mus);
        }
        erase(&musicCache, key);
        erase(&musicLRU, last(&musicLRU));
        TraceLog(LOG_INFO, "Evicted music: %s", key);
    }
    while (size(&spriteLRU) > CACHE_SIZE) {
        char *key = (char *)last(&spriteLRU);
        Texture2D *tex = get(&spriteCache, key);
        if (tex) UnloadTexture(*tex);
        erase(&spriteCache, key);
        erase(&spriteLRU, last(&spriteLRU));
        TraceLog(LOG_INFO, "Evicted sprite: %s", key);
    }
}

int main(void) {
    InitWindow(screenWidth, screenHeight, "VN Engine");
    InitAudioDevice();
    Shader spriteOutline = LoadShader(0, TextFormat("src/outline-%i.fs", GLSL_VERSION));

    gL = luaL_newstate();
    luaL_openlibs(gL);

    lua_register(gL, "load_background", l_load_background);
    lua_register(gL, "load_sprite", l_load_sprite);
    lua_register(gL, "unload_sprite", l_unload_sprite);
    lua_register(gL, "play_music", l_play_music);
    lua_register(gL, "play_sound", l_play_sound);
    lua_register(gL, "show_text", l_show_text);
    lua_register(gL, "clear_text", l_clear_text);
    lua_register(gL, "set_choices", l_set_choices);
    lua_register(gL, "quit", l_quit);
    lua_register(gL, "module_init", l_module_init);
    lua_register(gL, "pop_state", l_pop_state);

    FilePathList scenes = LoadDirectoryFilesEx("mods", ".lua", 0);
    init(&backgroundCache);
    init(&musicCache);
    init(&spriteCache);
    init(&gameStateStack);
    init(&backgroundLRU);
    init(&musicLRU);
    init(&spriteLRU);

    SetTargetFPS(60);
    while (!gQuit) {
        if (WindowShouldClose()) gQuit = true;
        BeginDrawing();
        ClearBackground(RAYWHITE);
        switch (screen) {
        case TITLE: {
            switch (menu) {
                case NONE: {
                    mainMenu();
                } break;
                case MODULE: {
                    chooseModule(&scenes);
                } break;
                case LOAD: {} break;
                case SETTINGS: {} break;
                case QUIT: {
                    gQuit = true;
                } break;
            } break;
        } break;
        case GAME: {
            if (gGameState.hasMusic) UpdateMusicStream(gGameState.music);
    
            if (gGameState.hasDialog && gGameState.choiceCount == 0) {
                if (lua_status(gSceneThread) == LUA_YIELD &&
                    ( forward || IsKeyPressed(KEY_SPACE))) {
                    forward = false;
                    int nres = 0;
                    int status = lua_resume(gSceneThread, gL, 0, &nres);
                    if (status == LUA_YIELD) {
                        pushGameState();
                        prefetchAssets(gSceneThread);
                    }
                    if (status != LUA_YIELD && status != LUA_OK) {
                        const char *error = lua_tostring(gSceneThread, -1);
                        fprintf(stderr, "Error resuming scene: %s\n", error);
                    }
                }
            }
    
            if (gGameState.hasBackground) {
                updateBackground(&spriteOutline);
            }
            if (gGameState.hasDialog) {
                updateText();
            }
            if (gGameState.choiceCount > 0) {
                chooseScene();
            }
            int btnWidth = 40, btnHeight = 30;
            Rectangle pauseBut = { screenWidth - btnWidth - 10, 10, btnWidth, btnHeight };
            if (IsKeyPressed(KEY_P) || GuiButton(pauseBut, "#132#")) gGameState.isPaused = true;
            if (gGameState.isPaused) pauseMenu();
            } break;
            default: break;
        } 
        invAssets();
        EndDrawing();
    }

    UnloadDirectoryFiles(scenes);
    if (gGameState.hasBackground) UnloadTexture(gGameState.background);
    for (int i = 0; i < gGameState.spriteCount; i++)
        UnloadTexture(gGameState.sprites[i].texture);
    if (gGameState.hasMusic) {
        StopMusicStream(gGameState.music);
        UnloadMusicStream(gGameState.music);
    }
    lua_close(gL);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

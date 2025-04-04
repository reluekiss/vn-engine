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
    lua_Debug frames[4096];
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

/* --- API Call Recording System --- */

// Define an enumeration for our API call types.
typedef enum {
    API_MODULE_INIT,
    API_LOAD_BACKGROUND,
    API_LOAD_SPRITE,
    API_PLAY_MUSIC,
    API_PLAY_SOUND,
    API_SHOW_TEXT,
    API_CLEAR_TEXT,
    API_POP_STATE
} APICallType;

// Define a union holding parameters for each API function.
typedef struct {
    APICallType type;
    union {
        struct {
            char moduleFolder[BUFFER_SIZE];
        } moduleInit;
        struct {
            char file[PATH_BUFFER_SIZE];
        } loadBackground;
        struct {
            char file[PATH_BUFFER_SIZE];
            int x;
            int y;
            char id[BUFFER_SIZE]; // Empty string if no id provided.
        } loadSprite;
        struct {
            char file[PATH_BUFFER_SIZE];
            float start;
        } playMusic;
        struct {
            char file[PATH_BUFFER_SIZE];
        } playSound;
        struct {
            char name[BUFFER_SIZE];
            char text[BUFFER_SIZE];
            Color nameColor;
            Color textColor;
            int posX;
            int posY;
            bool hasPos;
        } showText;
        // API_CLEAR_TEXT and API_POP_STATE carry no parameters.
    } params;
} APICall;

// Global vector to hold API call records.
vec(APICall) apiCallStack;

// Push helpers for each API call type.
static void pushAPICall_moduleInit(const char *moduleFolder) {
    APICall call;
    call.type = API_MODULE_INIT;
    strncpy(call.params.moduleInit.moduleFolder, moduleFolder, BUFFER_SIZE-1);
    call.params.moduleInit.moduleFolder[BUFFER_SIZE-1] = '\0';
    push(&apiCallStack, call);
}

static void pushAPICall_loadBackground(const char *file) {
    APICall call;
    call.type = API_LOAD_BACKGROUND;
    strncpy(call.params.loadBackground.file, file, PATH_BUFFER_SIZE-1);
    call.params.loadBackground.file[PATH_BUFFER_SIZE-1] = '\0';
    push(&apiCallStack, call);
}

static void pushAPICall_loadSprite(const char *file, int x, int y, const char *id) {
    APICall call;
    call.type = API_LOAD_SPRITE;
    strncpy(call.params.loadSprite.file, file, PATH_BUFFER_SIZE-1);
    call.params.loadSprite.file[PATH_BUFFER_SIZE-1] = '\0';
    call.params.loadSprite.x = x;
    call.params.loadSprite.y = y;
    if (id)
        strncpy(call.params.loadSprite.id, id, BUFFER_SIZE-1);
    else
        call.params.loadSprite.id[0] = '\0';
    call.params.loadSprite.id[BUFFER_SIZE-1] = '\0';
    push(&apiCallStack, call);
}

static void pushAPICall_playMusic(const char *file, float start) {
    APICall call;
    call.type = API_PLAY_MUSIC;
    strncpy(call.params.playMusic.file, file, PATH_BUFFER_SIZE-1);
    call.params.playMusic.file[PATH_BUFFER_SIZE-1] = '\0';
    call.params.playMusic.start = start;
    push(&apiCallStack, call);
}

static void pushAPICall_playSound(const char *file) {
    APICall call;
    call.type = API_PLAY_SOUND;
    strncpy(call.params.playSound.file, file, PATH_BUFFER_SIZE-1);
    call.params.playSound.file[PATH_BUFFER_SIZE-1] = '\0';
    push(&apiCallStack, call);
}

static void pushAPICall_showText(const char *name, const char *text, Color nameColor, Color textColor, int posX, int posY, bool hasPos) {
    APICall call;
    call.type = API_SHOW_TEXT;
    strncpy(call.params.showText.name, name, BUFFER_SIZE-1);
    call.params.showText.name[BUFFER_SIZE-1] = '\0';
    strncpy(call.params.showText.text, text, BUFFER_SIZE-1);
    call.params.showText.text[BUFFER_SIZE-1] = '\0';
    call.params.showText.nameColor = nameColor;
    call.params.showText.textColor = textColor;
    call.params.showText.posX = posX;
    call.params.showText.posY = posY;
    call.params.showText.hasPos = hasPos;
    push(&apiCallStack, call);
}

static void pushAPICall_clearText(void) {
    APICall call;
    call.type = API_CLEAR_TEXT;
    push(&apiCallStack, call);
}

static void pushAPICall_popState(void) {
    APICall call;
    call.type = API_POP_STATE;
    push(&apiCallStack, call);
}

// Replay the stored API calls.
static void replayAPICalls(void) {
    printf("Replaying API calls:\n");
    for (APICall *call = first(&apiCallStack);
         call != end(&apiCallStack);
         call = next(&apiCallStack, call))
    {
        switch(call->type) {
            case API_MODULE_INIT:
                // Reapply module init.
                gGameState.moduleFolder = strdup(call->params.moduleInit.moduleFolder);
                printf("Replayed module_init: %s\n", call->params.moduleInit.moduleFolder);
                break;
            case API_LOAD_BACKGROUND: {
                char path[PATH_BUFFER_SIZE];
                snprintf(path, PATH_BUFFER_SIZE, "mods/%s/images/%s", gGameState.moduleFolder, call->params.loadBackground.file);
                Texture2D *cached = get(&backgroundCache, path);
                if (cached) {
                    gGameState.background = *cached;
                } else {
                    gGameState.background = LoadTexture(path);
                    char *key = strdup(path);
                    insert(&backgroundCache, key, gGameState.background);
                    insert(&backgroundLRU, first(&backgroundLRU), key);
                }
                printf("Replayed load_background: %s\n", call->params.loadBackground.file);
                break;
            }
            case API_LOAD_SPRITE: {
                char path[PATH_BUFFER_SIZE];
                snprintf(path, PATH_BUFFER_SIZE, "mods/%s/images/%s", gGameState.moduleFolder, call->params.loadSprite.file);
                Texture2D *cached = get(&spriteCache, path);
                if (cached) {
                    gGameState.sprites[gGameState.spriteCount].texture = *cached;
                    updateLRU(&spriteLRU, path);
                } else {
                    gGameState.sprites[gGameState.spriteCount].texture = LoadTexture(path);
                    char *key = strdup(path);
                    insert(&spriteCache, key, gGameState.sprites[gGameState.spriteCount].texture);
                    insert(&spriteLRU, first(&spriteLRU), key);
                }
                gGameState.sprites[gGameState.spriteCount].pos.x = call->params.loadSprite.x;
                gGameState.sprites[gGameState.spriteCount].pos.y = call->params.loadSprite.y;
                if (strlen(call->params.loadSprite.id) > 0) {
                    strncpy(gGameState.sprites[gGameState.spriteCount].id, call->params.loadSprite.id, BUFFER_SIZE-1);
                    gGameState.sprites[gGameState.spriteCount].id[BUFFER_SIZE-1] = '\0';
                    gGameState.sprites[gGameState.spriteCount].hasID = true;
                } else {
                    gGameState.sprites[gGameState.spriteCount].hasID = false;
                }
                strncpy(gGameState.spritefiles[gGameState.spriteCount], path, PATH_BUFFER_SIZE);
                gGameState.spriteCount++;
                printf("Replayed load_sprite: %s\n", call->params.loadSprite.file);
                break;
            }
            case API_PLAY_MUSIC: {
                char path[PATH_BUFFER_SIZE];
                snprintf(path, PATH_BUFFER_SIZE, "mods/%s/music/%s", gGameState.moduleFolder, call->params.playMusic.file);
                Music *cached = get(&musicCache, path);
                if (cached) {
                    gGameState.music = *cached;
                    updateLRU(&musicLRU, path);
                } else {
                    gGameState.music = LoadMusicStream(path);
                    char *key = strdup(path);
                    insert(&musicCache, key, gGameState.music);
                    insert(&musicLRU, first(&musicLRU), key);
                }
                PlayMusicStream(gGameState.music);
                if (call->params.playMusic.start > 0.0f)
                    SeekMusicStream(gGameState.music, call->params.playMusic.start);
                strncpy(gGameState.musicfile, path, PATH_BUFFER_SIZE);
                gGameState.hasMusic = true;
                printf("Replayed play_music: %s\n", call->params.playMusic.file);
                break;
            }
            case API_PLAY_SOUND: {
                char path[PATH_BUFFER_SIZE];
                snprintf(path, PATH_BUFFER_SIZE, "mods/%s/music/%s", gGameState.moduleFolder, call->params.playSound.file);
                Sound s = LoadSound(path);
                PlaySound(s);
                printf("Replayed play_sound: %s\n", call->params.playSound.file);
                break;
            }
            case API_SHOW_TEXT: {
                strncpy(gGameState.dialogName, call->params.showText.name, BUFFER_SIZE-1);
                gGameState.dialogName[BUFFER_SIZE-1] = '\0';
                strncpy(gGameState.dialogText, call->params.showText.text, BUFFER_SIZE-1);
                gGameState.dialogText[BUFFER_SIZE-1] = '\0';
                gGameState.dialogNameColor = call->params.showText.nameColor;
                gGameState.textColor = call->params.showText.textColor;
                if (call->params.showText.hasPos) {
                    gGameState.dialogPos.x = (float)call->params.showText.posX;
                    gGameState.dialogPos.y = (float)call->params.showText.posY;
                    gGameState.dialogHasPos = true;
                } else {
                    gGameState.dialogHasPos = false;
                }
                gGameState.hasDialog = true;
                printf("Replayed show_text: %s\n", call->params.showText.text);
                break;
            }
            case API_CLEAR_TEXT:
                gGameState.dialogText[0] = '\0';
                gGameState.dialogName[0] = '\0';
                gGameState.hasDialog = false;
                gGameState.dialogHasPos = false;
                printf("Replayed clear_text\n");
                break;
            case API_POP_STATE:
                if (size(&gameStateStack) > 0) {
                    GameState *state = get(&gameStateStack, size(&gameStateStack) - 1);
                    gGameState = *state;
                    gSceneThread = gGameState.sceneThread;
                    erase(&gameStateStack, size(&gameStateStack) - 1);
                    printf("Replayed pop_state\n");
                }
                break;
            default:
                break;
        }
    }
    cleanup(&apiCallStack);
    init(&apiCallStack);
}

/* --- End API Call Recording System --- */

/* --- Lua API --- */
static int l_module_init(lua_State *L) {
    const char *modFolder = luaL_checkstring(L, 1);
    gGameState.moduleFolder = modFolder;
    pushAPICall_moduleInit(modFolder);
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
    pushAPICall_loadBackground(file);
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
    pushAPICall_loadSprite(file, x, y, id);
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
    pushAPICall_playMusic(file, start);
    return 0;
}

static int l_play_sound(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "mods/%s/music/%s", gGameState.moduleFolder, file);
    Sound s = LoadSound(path);
    PlaySound(s);
    TraceLog(LOG_INFO, "Played sound: %s", file);
    pushAPICall_playSound(file);
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
    Color nameColor = WHITE;
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
        nameColor = (Color){ r, g, b, a };
    }
    lua_pop(L, 1);

    const char *text = luaL_checkstring(L, 2);
    strncpy(gGameState.dialogText, text, BUFFER_SIZE - 1);
    gGameState.dialogText[BUFFER_SIZE - 1] = '\0';

    Color textColor = WHITE;
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
        textColor = (Color){ tr, tg, tb, ta };
    }
    
    int posX = 0, posY = 0;
    bool hasPos = false;
    if (lua_gettop(L) >= 5 && lua_isnumber(L, 4) && lua_isnumber(L, 5)) {
        posX = lua_tointeger(L, 4);
        posY = lua_tointeger(L, 5);
        hasPos = true;
    }
    
    gGameState.dialogNameColor = nameColor;
    gGameState.textColor = textColor;
    if (hasPos) {
        gGameState.dialogPos.x = (float)posX;
        gGameState.dialogPos.y = (float)posY;
        gGameState.dialogHasPos = true;
    } else {
        gGameState.dialogHasPos = false;
    }
    gGameState.hasDialog = true;
    TraceLog(LOG_INFO, "Show text: %s", gGameState.dialogText);
    pushAPICall_showText(char_name, text, nameColor, textColor, posX, posY, hasPos);
    return lua_yield(L, 0);
}

static int l_clear_text(lua_State *L) {
    (void)L;
    gGameState.dialogText[0] = '\0';
    gGameState.dialogName[0] = '\0';
    gGameState.hasDialog = false;
    gGameState.dialogHasPos = false;
    pushAPICall_clearText();
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
    pushAPICall_popState();
    lua_pushboolean(L, true);
    return 1;
}

/* End Lua API */

/* --- OptionsStyle, genericChoose, getModuleLabel, onModuleSelect, chooseModule, getSceneLabel, onSceneSelect, chooseScene, getMenuItems, menuSelect, mainMenu, pauseMenuSelect, pauseMenu, updateBackground, updateText, cachePrefetched, prefetchAssets, invAssets --- */
// (These functions remain unchanged from your code.)

// ...
// (Rest of your code here; see your original code.)
// ...

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
    // Initialize our API call stack
    init(&apiCallStack);

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

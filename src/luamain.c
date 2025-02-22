#include "../build/lua/include/lua.h"
#include "../build/lua/include/lualib.h"
#include "../build/lua/include/lauxlib.h"
#include <raylib.h>
#include <stdio.h>

#define PATH_BUFFER_SIZE 512

static Texture2D gSprite;
static Vector2 gSpritePos;
static bool gSpriteLoaded = false;

// Purely to make a distinction between lua api and C code
void LoadSprite(const char* file, Vector2 pos) {
    if (!gSpriteLoaded) {
        char path[PATH_BUFFER_SIZE];
        snprintf(path, PATH_BUFFER_SIZE, "assets/images/%s", file);
        gSprite = LoadTexture(path); 
        gSpritePos = pos;
        gSpriteLoaded = true;
    }
}

static int l_load_sprite(lua_State *L) {
    const char *file = luaL_checkstring(L, 1);
    Vector2 pos = { 0 };
    pos.x = (float)luaL_checkinteger(L, 2);
    pos.y = (float)luaL_checkinteger(L, 3);
    LoadSprite(file, pos);
    TraceLog(LOG_INFO, "Loading sprite: %s at (%d, %d)", file, (int)pos.x, (int)pos.y);
    return 0;
}

int main(void) {
    const int screenWidth = 800, screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "Lua Engine");
    
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    
    lua_register(L, "load_sprite", l_load_sprite);
    
    if (luaL_dofile(L, "src/script.lua") != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        fprintf(stderr, "Error loading Lua script: %s\n", error);
        return 1;
    }
    
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            if (gSpriteLoaded) {
                DrawTexture(gSprite, (int)gSpritePos.x, (int)gSpritePos.y, WHITE);
            }
            
        EndDrawing();
    }
    
    if (gSpriteLoaded) UnloadTexture(gSprite);
    lua_close(L);
    CloseWindow();
    return 0;
}

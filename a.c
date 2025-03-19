static int l_show_text(lua_State *L) {
    // New API: show_text(character, text, text_color, int x, int y)

    // Parse character table (argument 1)
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "name");
    const char *name = luaL_checkstring(L, -1);
    strncpy(gDialogName, name, BUFFER_SIZE - 1);
    gDialogName[BUFFER_SIZE - 1] = '\0';
    lua_pop(L, 1);

    lua_getfield(L, 1, "color");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "r");
        int cr = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "g");
        int cg = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "b");
        int cb = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        lua_getfield(L, -1, "a");
        int ca = luaL_optinteger(L, -1, 255);
        lua_pop(L, 1);
        gDialogNameColor = (Color){ cr, cg, cb, ca };
    } else {
        gDialogNameColor = WHITE;
    }
    lua_pop(L, 1);

    // Parse text string (argument 2)
    const char *text = luaL_checkstring(L, 2);
    strncpy(gDialogText, text, BUFFER_SIZE - 1);
    gDialogText[BUFFER_SIZE - 1] = '\0';

    // Parse text_color table (argument 3)
    luaL_checktype(L, 3, LUA_TTABLE);
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
    gTextColor = (Color){ tr, tg, tb, ta };

    // Parse relative position (arguments 4 and 5) and adjust relative to window center
    int relX = luaL_checkinteger(L, 4);
    int relY = luaL_checkinteger(L, 5);
    int centerX = GetScreenWidth() / 2;
    int centerY = GetScreenHeight() / 2;
    gDialogPos.x = (float)(centerX + relX);
    gDialogPos.y = (float)(centerY + relY);
    gDialogHasPos = true;

    gHasDialog = true;

    TraceLog(LOG_INFO, "Show text for character: %s", gDialogName);
    return lua_yield(L, 0);
}

#include <assert.h>
#include <stdbool.h>
#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "boundedtext.h"

typedef struct cfg_t {
    int screenWidth;
    int screenHeight;
    int targetFPS;
    int fontSize;
    Rectangle container;
    Color     borderColor;
    Font      font;
} cfg;

typedef enum {
    DIRECTIVE_IMAGE,
    DIRECTIVE_MUSIC,
    DIRECTIVE_TEXT,
    DIRECTIVE_SCENE
} DirectiveType;

typedef struct {
    DirectiveType  type;
    char*          payload;
} Directive;

typedef struct {
    Directive* items;
    size_t     count;
    size_t     capacity;
} directiveDA;

typedef struct scene_t scene;
typedef struct scene_t {
    directiveDA directives;
    char*       sceneName;
    char*       nextSceneName;
    scene*      nextScene;
} scene;

typedef struct sceneState_t {
    Texture2D texture;
    Music     music;
    char*     text;

    int       sceneIndex;
    int       directiveIndex;

    bool      nextDirective;
    bool      nextScene;
    bool      sceneInit;
} sceneState;

typedef struct scenes_t {
    scene** items;
    size_t  count;
    size_t  capacity;
    sceneState currentScene;
} scenes;

extern void freeScenes(scenes* sc);
extern scenes* loadScenes(const char* directory);
extern void updateScene(scenes* sc);
extern void drawScene(sceneState* sc, cfg* cfg);

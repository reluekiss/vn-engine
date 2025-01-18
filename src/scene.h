#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "hashtable.h"

typedef struct cfg_t {
    int screenWidth;
    int screenHeight;
    int targetFPS;
    int fontSize;
} cfg;

typedef struct scene_t scene;
typedef struct scene_t {
    char* musicFilePath;
    Music music;

    char*     textureFilePath;
    Texture2D texture;

    int paracount;
    struct paragraph_t {
        int   count;
        char* text;
    } *paragraphs;

    scene* nextScene;
} scene;

typedef struct scenes_t {
    int    count;
    scene* currentScene;
    struct hashtable *ht;
} scenes;

extern void freeScenes(scenes* sc);
extern scenes* loadScenes(const char* directory);
extern void updateScene(scene* sc);
extern void drawScene(scene* sc, cfg* cfg);

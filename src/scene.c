#include "scene.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

#define da_append(da, item)                                                          \
    do {                                                                             \
        if ((da)->count >= (da)->capacity) {                                         \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");                       \
        }                                                                            \
        (da)->items[(da)->count++] = (item);                                         \
    } while (0)


scenes* loadScenes(const char* directory)
{
    DIR* dir = opendir(directory);
    if (!dir) { perror("Failed to open directory"); return NULL; }

    scenes* sc = calloc(1, sizeof(*sc));
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".txt")) {
            char filepath[256];
            snprintf(filepath, 256, "%s/%s", directory, entry->d_name);

            FILE* file = fopen(filepath, "r");
            if (!file) { perror("Failed to open file"); continue; }

            scene* s = calloc(1, sizeof(*s));
            char*  line = NULL;
            size_t len  = 0;
            while (getline(&line, &len, file) != -1) {
                line[strcspn(line, "\n")] = 0;
                if (strncmp(line, "::image", 7) == 0) {
                    if (getline(&line, &len, file) != -1) {
                        line[strcspn(line, "\n")] = 0;
                        Directive d = { DIRECTIVE_IMAGE, strdup(line) };
                        da_append(&s->directives, d);
                    }
                } else if (strncmp(line, "::music", 7) == 0) {
                    if (getline(&line, &len, file) != -1) {
                        line[strcspn(line, "\n")] = 0;
                        Directive d = { DIRECTIVE_MUSIC, strdup(line) };
                        da_append(&s->directives, d);
                    }
                } else if (strncmp(line, "::text", 6) == 0) {
                    if (getline(&line, &len, file) != -1) {
                        line[strcspn(line, "\n")] = 0;
                        Directive d = { DIRECTIVE_TEXT, strdup(line) };
                        da_append(&s->directives, d);
                    }
                } else if (strncmp(line, "::scene", 7) == 0) {
                    if (getline(&line, &len, file) != -1) {
                        line[strcspn(line, "\n")] = 0;
                        Directive d = { DIRECTIVE_SCENE, strdup(line) };
                        da_append(&s->directives, d);
                        s->nextSceneName = strdup(line);
                    }
                }
            }
            s->sceneName = strdup(entry->d_name);
            fclose(file);
            free(line);
            da_append(sc, s);
        }
    }
    closedir(dir);
    return sc;
}

void freeScenes(scenes* sc) {
    for (int i = 0; i < sc->count; i++) {
        for (int j = 0; j < sc->items[i]->directives.count; j++) {
            free(sc->items[i]->directives.items[j].payload);
            free(&sc->items[i]->directives.items[j]);
        }
        free(sc->items[i]->sceneName);
        free(sc->items[i]->nextSceneName);
    }
    free(sc->items);
}

void updateScene(scenes* sc)
{
    if (sc->currentScene.nextScene) {
        bool nextScene = false;
        bool sceneInit = true;
        sc->currentScene.sceneIndex += 1;
    }
    if (sc->currentScene.sceneInit) {
        PlayMusicStream(sc->currentScene.music);
        sc->currentScene.sceneInit = false;
    } else {
        UpdateMusicStream(sc->currentScene.music);
    }
    // TODO: check if this edge case actually happens
    if (sc->currentScene.nextDirective) {
        if (sc->currentScene.directiveIndex == 0) {
            sc->currentScene.directiveIndex -= 1;
        }
        sc->currentScene.directiveIndex += 1;
        switch (sc->items[sc->currentScene.sceneIndex]->directives.items[sc->currentScene.directiveIndex].type) {
            case DIRECTIVE_IMAGE:
            {
                sc->currentScene.nextDirective = true;
                // TODO: find more robust method for checking if there is a texture or song currently loaded
                if (sc->currentScene.sceneIndex != 0) {
                    UnloadTexture(sc->currentScene.texture);
                }
                Image tmpImage = LoadImage(sc->items[sc->currentScene.sceneIndex]->directives.items[sc->currentScene.directiveIndex].payload);
                sc->currentScene.texture = LoadTextureFromImage(tmpImage);
                UnloadImage(tmpImage);
            } break;
            case DIRECTIVE_MUSIC:
            {
                sc->currentScene.nextDirective = true;
                if (sc->currentScene.sceneIndex != 0) {
                    UnloadMusicStream(sc->currentScene.music);
                }
                sc->currentScene.music = LoadMusicStream(sc->items[sc->currentScene.sceneIndex]->directives.items[sc->currentScene.directiveIndex].payload);
            } break;
            case DIRECTIVE_TEXT:
            {
                sc->currentScene.text = sc->items[sc->currentScene.sceneIndex]->directives.items[sc->currentScene.directiveIndex].payload;
            } break;
            case DIRECTIVE_SCENE:
            {
                sc->currentScene.nextScene = true;
                sc->currentScene.directiveIndex = 0;
            } break;
            default: break;
        }
    }
}

void drawScene(sceneState* sc, cfg* cfg)
{
    DrawTexture(sc->texture, cfg->screenWidth/2 + sc->texture.width/2, cfg->screenHeight/2 + sc->texture.height/2, WHITE);
    
    // TODO: Bounded text
    DrawRectangleLinesEx(cfg->container, 3, cfg->borderColor);
    DrawTextBoxed(cfg->font, sc->text, (Rectangle){ cfg->container.x - 4, cfg->container.y - 4, cfg->container.width + 4, cfg->container.height + 4 }, cfg->fontSize, 2.0f, true, GRAY);
}

#include "scene.h"
#include "hashtable.h"
#include <raylib.h>
#include <stdio.h>

void freeScenes(scenes* sc) {
    // for (int i = 0; i < sc->count; i++) {
    //     free(sc->scene[i].musicFilePath);
    //     free(sc->scene[i].textureFilePath);
    //     UnloadMusicStream(sc->scene[i].music);
    //     UnloadTexture(sc->scene[i].texture);
    //     for (int j = 0; j < sc->scene[i].paracount; j++) {
    //         free(sc->scene[i].paragraphs[j].text);
    //     }
    //     free(sc->scene[i].paragraphs);
    // }
    // free(sc->scene);
}

scenes* loadScenes(const char* directory) {
    DIR* dir = opendir(directory);
    if (!dir) {
        perror("Failed to open directory");
        return NULL;
    }

    struct dirent* entry;
    scenes* sc = malloc(sizeof(scenes));
    sc->count = 0;
    sc->currentScene = NULL;
    sc->ht = hashtable_create(128, NULL);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".txt")) {
            scene* current_scene = malloc(sizeof(scene));
            current_scene->paracount = 0;
            current_scene->paragraphs = NULL;

            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);
            FILE* file = fopen(filepath, "r");
            if (!file) {
                perror("Failed to open file");
                continue;
            }

            char* line = NULL;
            size_t len = 0;
            int paragraph_count = 0;
            int in_paragraph = 0;

            while (getline(&line, &len, file) != -1) {
                line[strcspn(line, "\n")] = 0;

                if (strlen(line) == 0) {
                    if (in_paragraph) {
                        paragraph_count++;
                        in_paragraph = 0;
                    }
                } else if (strncmp(line, "::", 2) == 0) {
                    // TODO: handle scene transitions, image/audio (starting point?) loading.
                    // Scene transition (e.g., "::scene2")
                    line += 2;
                    if (strncmp(line, "image", 5)) {
                        // These are one more due to whitespace
                        line += 6;
                        Image image = LoadImage(line);
                        current_scene->texture = LoadTextureFromImage(image);
                        UnloadImage(image);
                        current_scene->textureFilePath = line;
                    } else if (strncmp(line, "audio", 5)) { 
                        line += 6;
                        current_scene->music = LoadMusicStream(line);    
                        current_scene->musicFilePath = line;
                    } else if (strncmp(line, "next", 4)) {
                        line += 5;
                        // TODO: Keep track of scenes and point the next scene to the current scene (maybe this has to be done at the end?)
                    } else {
                        printf("[WARNING] Unkown scene argument: %s\n", line);
                    }
                } else {
                    if (!in_paragraph) {
                        current_scene->paragraphs = realloc(current_scene->paragraphs, sizeof(struct paragraph_t) * (paragraph_count + 1));
                        current_scene->paragraphs[paragraph_count].count = 0; // Reset count for new paragraph
                        current_scene->paragraphs[paragraph_count].text = NULL;
                        in_paragraph = 1;
                    }
                    size_t current_length = current_scene->paragraphs[paragraph_count].count;
                    current_scene->paragraphs[paragraph_count].text = realloc(current_scene->paragraphs[paragraph_count].text, current_length + strlen(line) + 2);
                    if (current_length == 0) {
                        strcpy(current_scene->paragraphs[paragraph_count].text, line);
                    } else {
                        strcat(current_scene->paragraphs[paragraph_count].text, "\n");
                        strcat(current_scene->paragraphs[paragraph_count].text, line);
                    }
                    current_scene->paragraphs[paragraph_count].count++;
                }
            }
            if (in_paragraph) {
                paragraph_count++;
            }
            current_scene->paracount = paragraph_count;

            hashtable_put(sc->ht, entry->d_name, current_scene);

            fclose(file);
            free(line);
            sc->count++;
        }
    }
    closedir(dir);
    return sc;
}

void updateScene(scene* sc)
{
    bool nextScene = false;
    bool sceneInit = true;
    while (!nextScene)
    {
        if (sceneInit) {
            PlayMusicStream(sc->music);
            sceneInit = false;
        } else {
            UpdateMusicStream(sc->music);
        }
    }
}

void drawScene(scene* sc, cfg* cfg)
{
    DrawTexture(sc->texture, cfg->screenWidth/2 + sc->texture.width/2, cfg->screenHeight/2 + sc->texture.height/2, WHITE);
}

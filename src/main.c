#include "raylib.h"
#include "boundedtext.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define BUFFER_SIZE 256
#define MAX_COMMANDS 50
#define MAX_OPTIONS 2
#define MAX_SPRITES 10

typedef enum { 
    CMD_BG, 
    CMD_SPRITE, 
    CMD_MUSIC, 
    CMD_SOUND, 
    CMD_TEXT, 
    CMD_SCENE, 
    CMD_USPRITE 
} CommandType;

typedef struct {
    char optionText[BUFFER_SIZE];
    char nextScene[BUFFER_SIZE];
} SceneOption;

typedef struct {
    CommandType type;
    char arg1[BUFFER_SIZE];   // For text content or filenames (music, sound)
    char arg2[BUFFER_SIZE];   // For sprite filename

    char name[BUFFER_SIZE];
    bool hasName;
    Vector2 pos;
    bool hasPos;

    char spriteID[BUFFER_SIZE];
    bool hasSpriteID;

    float musicStart;
    bool hasMusicStart;

    int optionCount;
    SceneOption options[MAX_OPTIONS];
} SceneCommand;

typedef struct {
    SceneCommand commands[MAX_COMMANDS];
    int commandCount;
    int currentCommand;

    Texture2D background;
    bool hasBackground;

    struct {
        Texture2D texture;
        Vector2 pos;
        char id[BUFFER_SIZE];
        bool hasID;
    } sprites[MAX_SPRITES];
    int spriteCount;

    Music music;
    bool hasMusic;
} Scene;

static void UnloadSceneAssets(Scene *scene) {
    if (scene->hasBackground) { 
        UnloadTexture(scene->background); 
        scene->hasBackground = false; 
    }
    for (int i = 0; i < scene->spriteCount; i++) {
        UnloadTexture(scene->sprites[i].texture);
    }
    scene->spriteCount = 0;
    if (scene->hasMusic) { 
        StopMusicStream(scene->music); 
        UnloadMusicStream(scene->music); 
        scene->hasMusic = false; 
    }
}

static void ProcessCommand(Scene *scene, SceneCommand *cmd) {
    char path[BUFFER_SIZE];
    switch(cmd->type) {
        case CMD_BG:
            if (scene->hasBackground) UnloadTexture(scene->background);
            snprintf(path, BUFFER_SIZE, "assets/images/%s", cmd->arg1);
            {
                Image img = LoadImage(path);
                scene->background = LoadTextureFromImage(img);
                UnloadImage(img);
                scene->hasBackground = true;
            }
            break;
        case CMD_SPRITE:
            if (scene->spriteCount < MAX_SPRITES) {
                snprintf(path, BUFFER_SIZE, "assets/images/%s", cmd->arg2);
                Image img = LoadImage(path);
                int x = cmd->hasPos ? (int)cmd->pos.x : 0;
                int y = cmd->hasPos ? (int)cmd->pos.y : 0;
                scene->sprites[scene->spriteCount].texture = LoadTextureFromImage(img);
                UnloadImage(img);
                scene->sprites[scene->spriteCount].pos = (Vector2){ x, y };
                if (cmd->hasSpriteID) {
                    strcpy(scene->sprites[scene->spriteCount].id, cmd->spriteID);
                    scene->sprites[scene->spriteCount].hasID = true;
                } else {
                    scene->sprites[scene->spriteCount].hasID = false;
                }
                scene->spriteCount++;
            }
            break;
        case CMD_MUSIC:
            if (scene->hasMusic) { 
                StopMusicStream(scene->music); 
                UnloadMusicStream(scene->music); 
            }
            snprintf(path, BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            scene->music = LoadMusicStream(path);
            PlayMusicStream(scene->music);
            if (cmd->hasMusicStart) {
                SeekMusicStream(scene->music, cmd->musicStart);
            }
            scene->hasMusic = true;
            break;
        case CMD_SOUND:
            snprintf(path, BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            {
                Sound s = LoadSound(path);
                PlaySound(s);
                // Manage sound lifetime here.
            }
            break;
        case CMD_USPRITE:
            for (int i = 0; i < scene->spriteCount; i++) {
                if (scene->sprites[i].hasID && strcmp(scene->sprites[i].id, cmd->spriteID) == 0) {
                    UnloadTexture(scene->sprites[i].texture);
                    for (int j = i; j < scene->spriteCount - 1; j++) {
                        scene->sprites[j] = scene->sprites[j+1];
                    }
                    scene->spriteCount--;
                    break;
                }
            }
            break;
        case CMD_TEXT:
            // Wait for user input.
            break;
        case CMD_SCENE:
            // Wait for user selection.
            break;
    }
}

static void ProcessAutoCommands(Scene *scene) {
    while (scene->currentCommand < scene->commandCount) {
        SceneCommand *cmd = &scene->commands[scene->currentCommand];
        if (cmd->type == CMD_TEXT || cmd->type == CMD_SCENE) break;
        ProcessCommand(scene, cmd);
        scene->currentCommand++;
    }
}

static void LoadSceneFromFile(Scene *scene, const char *sceneFile) {
    UnloadSceneAssets(scene);
    scene->commandCount = 0;
    scene->currentCommand = 0;
    FILE *fp;
    char path[BUFFER_SIZE];
    snprintf(path, BUFFER_SIZE, "assets/scenes/%s", sceneFile);
    fp = fopen(path, "r");
    if (!fp) return;
    char line[BUFFER_SIZE];
    int len;
    while (fgets(line, BUFFER_SIZE, fp)) {
        len = (int)strlen(line);
        if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
        if (strlen(line)==0) continue;
        if (strncmp(line, "::bg", 4)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_BG;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                strcpy(cmd->arg1, line);
            }
        } else if (strncmp(line, "::sprite", 8)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_SPRITE;
            cmd->hasSpriteID = false;
            cmd->hasPos = false;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                while(line[0] == ':') {
                    if (strncmp(line, ":id", 3)==0) {
                        char *p = line + 3;
                        while(*p==' ') p++;
                        strcpy(cmd->spriteID, p);
                        cmd->hasSpriteID = true;
                    } else if (strncmp(line, ":pos", 4)==0) {
                        char *p = line + 4;
                        while(*p==' ') p++;
                        int posX = 0, posY = 0;
                        if (sscanf(p, "%dx%d", &posX, &posY)==2) {
                            cmd->pos = (Vector2){ posX, posY };
                            cmd->hasPos = true;
                        }
                    }
                    long mark = ftell(fp);
                    if (!fgets(line, BUFFER_SIZE, fp)) break;
                    len = (int)strlen(line);
                    if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                    if(line[0] != ':') break;
                }
                strcpy(cmd->arg2, line);
            }
        } else if (strncmp(line, "::Usprite", 9)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_USPRITE;
            cmd->hasSpriteID = false;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                while(line[0] == ':') {
                    if (strncmp(line, ":id", 3)==0) {
                        char *p = line + 3;
                        while(*p==' ') p++;
                        strcpy(cmd->spriteID, p);
                        cmd->hasSpriteID = true;
                    }
                    long mark = ftell(fp);
                    if (!fgets(line, BUFFER_SIZE, fp)) break;
                    len = (int)strlen(line);
                    if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                    if(line[0] != ':') break;
                }
            }
        } else if (strncmp(line, "::music", 7)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_MUSIC;
            cmd->hasMusicStart = false;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                while(line[0] == ':') {
                    if (strncmp(line, ":start", 6)==0) {
                        char *p = line + 6;
                        while(*p==' ') p++;
                        int minutes = 0, seconds = 0;
                        if (sscanf(p, "%d:%d", &minutes, &seconds) == 2) {
                            cmd->musicStart = minutes * 60 + seconds;
                            cmd->hasMusicStart = true;
                        }
                    }
                    if (!fgets(line, BUFFER_SIZE, fp)) break;
                    len = (int)strlen(line);
                    if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                }
                strcpy(cmd->arg1, line);
            }
        } else if (strncmp(line, "::sound", 7)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_SOUND;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                strcpy(cmd->arg1, line);
            }
        } else if (strncmp(line, "::text", 6)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_TEXT;
            cmd->hasName = false;
            cmd->hasPos = false;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                while(line[0] == ':') {
                    if (strncmp(line, ":name", 5)==0) {
                        char *p = line + 5;
                        while(*p == ' ') p++;
                        strcpy(cmd->name, p);
                        cmd->hasName = true;
                    } else if (strncmp(line, ":pos", 4)==0) {
                        char *p = line + 4;
                        while(*p==' ') p++;
                        int posX = 0, posY = 0;
                        if (sscanf(p, "%dx%d", &posX, &posY)==2) {
                            cmd->pos = (Vector2){ posX, posY };
                            cmd->hasPos = true;
                        }
                    }
                    if (!fgets(line, BUFFER_SIZE, fp)) break;
                    len = (int)strlen(line);
                    if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                    if(line[0] != ':') break;
                }
                strcpy(cmd->arg1, line);
            }
        } else if (strncmp(line, "::scene", 7)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_SCENE;
            cmd->optionCount = 0;
            while (cmd->optionCount < MAX_OPTIONS) {
                if (!fgets(line, BUFFER_SIZE, fp)) break;
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                if (strlen(line)==0) break;
                strcpy(cmd->options[cmd->optionCount].optionText, line);
                if (!fgets(line, BUFFER_SIZE, fp)) break;
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                strcpy(cmd->options[cmd->optionCount].nextScene, line);
                cmd->optionCount++;
            }
        }
    }
    fclose(fp);
    while (scene->currentCommand < scene->commandCount) {
        SceneCommand *cmd = &scene->commands[scene->currentCommand];
        if (cmd->type == CMD_TEXT || cmd->type == CMD_SCENE)
            break;
        ProcessCommand(scene, cmd);
        scene->currentCommand++;
    }
}

int main(void)
{
    const int baseWidth = 800, baseHeight = 450;
    InitWindow(baseWidth, baseHeight, "raylib scene system with text params");
    InitAudioDevice();

    Scene scene = {0};
    LoadSceneFromFile(&scene, "scene1.txt");

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        if (scene.hasMusic) UpdateMusicStream(scene.music);

        if (scene.currentCommand < scene.commandCount) {
            SceneCommand *cmd = &scene.commands[scene.currentCommand];
            if (cmd->type == CMD_TEXT) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE)) {
                    scene.currentCommand++;
                    ProcessAutoCommands(&scene);
                }
            } else if (cmd->type == CMD_SCENE) {
                Rectangle dialog = { GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 100, 400, 200 };
                int optWidth = (dialog.width - 60) / MAX_OPTIONS;
                Rectangle optRects[MAX_OPTIONS];
                for (int i = 0; i < cmd->optionCount; i++) {
                    optRects[i].x = dialog.x + 20 + i * (optWidth + 20);
                    optRects[i].y = dialog.y + 50;
                    optRects[i].width = optWidth;
                    optRects[i].height = 50;
                }
                Vector2 mousePoint = GetMousePosition();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    for (int i = 0; i < cmd->optionCount; i++) {
                        if (CheckCollisionPointRec(mousePoint, optRects[i])) {
                            LoadSceneFromFile(&scene, cmd->options[i].nextScene);
                            break;
                        }
                    }
                }
            }
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (scene.hasBackground)
                DrawTexture(scene.background, 0, 0, WHITE);
            for (int i = 0; i < scene.spriteCount; i++)
                DrawTexture(scene.sprites[i].texture, (int)scene.sprites[i].pos.x, (int)scene.sprites[i].pos.y, WHITE);

            if (scene.currentCommand < scene.commandCount) {
                SceneCommand *cmd = &scene.commands[scene.currentCommand];
                if (cmd->type == CMD_TEXT) {
                    const float defaultXRel = 20.0f / (float)baseWidth;
                    const float defaultYRel = 330.0f / (float)baseHeight;
                    const float defaultWidthRel = 760.0f / (float)baseWidth;
                    const float defaultHeightRel = 100.0f / (float)baseHeight;
                    Rectangle textBox;
                    if (cmd->hasPos) {
                        float xRel = cmd->pos.x / (float)baseWidth;
                        float yRel = cmd->pos.y / (float)baseHeight;
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
                    if (cmd->hasName)
                        DrawText(cmd->name, textBox.x + 5, textBox.y - 25, 20, WHITE);
                    DrawTextBoxed(GetFontDefault(), cmd->arg1, innerBox, 20, 2, true, WHITE);
                } else if (cmd->type == CMD_SCENE) {
                    Rectangle dialog = { GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 100, 400, 200 };
                    DrawRectangleRec(dialog, Fade(DARKGRAY, 0.8f));
                    DrawRectangleLinesEx(dialog, 2, WHITE);
                    int optWidth = (dialog.width - 60) / MAX_OPTIONS;
                    for (int i = 0; i < cmd->optionCount; i++) {
                        Rectangle optRect = { dialog.x + 20 + i * (optWidth + 20), dialog.y + 50, optWidth, 50 };
                        DrawRectangleRec(optRect, Fade(LIGHTGRAY, 0.8f));
                        DrawRectangleLinesEx(optRect, 2, WHITE);
                        DrawText(cmd->options[i].optionText, optRect.x + 10, optRect.y + 15, 20, BLACK);
                    }
                }
            }
        EndDrawing();
    }

    UnloadSceneAssets(&scene);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

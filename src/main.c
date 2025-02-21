#include "raylib.h"
#include "boundedtext.h"  
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifndef WINDOWS
#include <dirent.h>
#else
#include <windows.h>
#endif

#define BUFFER_SIZE 256
#define PATH_BUFFER_SIZE 512
#define MAX_COMMANDS 256
#define MAX_OPTIONS 2
#define MAX_SPRITES 10
#define MAX_START_OPTIONS 10

typedef enum { 
    TITLE,
    GAMEPLAY,
} currentScreen;

typedef enum { 
    CMD_BG, 
    CMD_SPRITE, 
    CMD_MUSIC, 
    CMD_SOUND, 
    CMD_TEXT, 
    CMD_SCENE, 
    CMD_USPRITE,
    CMD_END
} CommandType;

typedef struct {
    char optionText[BUFFER_SIZE];
    char nextScene[BUFFER_SIZE];
} SceneOption;

typedef struct {
    CommandType type;
    char arg1[BUFFER_SIZE];   
    char arg2[BUFFER_SIZE];   

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
    
    currentScreen screen;
} Scene;

#ifndef WINDOWS
int GetSceneOptions(const char *directory, char optionTexts[][BUFFER_SIZE], char optionFiles[][BUFFER_SIZE], int maxOptions) {
    DIR *d;
    struct dirent *dir;
    int count = 0;
    d = opendir(directory);
    if (!d) return 0;
    while ((dir = readdir(d)) != NULL && count < maxOptions) {
        if (dir->d_type == DT_DIR)
            continue;
        char *dot = strrchr(dir->d_name, '.');
        if (dot && strcmp(dot, ".txt") == 0) {
            strncpy(optionFiles[count], dir->d_name, BUFFER_SIZE);
            optionFiles[count][BUFFER_SIZE-1] = '\0';
            char name[BUFFER_SIZE];
            strncpy(name, dir->d_name, BUFFER_SIZE);
            name[BUFFER_SIZE-1] = '\0';
            dot = strrchr(name, '.');
            if (dot) *dot = '\0';
            strncpy(optionTexts[count], name, BUFFER_SIZE);
            optionTexts[count][BUFFER_SIZE-1] = '\0';
            count++;
        }
    }
    closedir(d);
    return count;
}
#else
#include <windows.h>
int GetSceneOptions(const char *directory, char optionTexts[][BUFFER_SIZE], char optionFiles[][BUFFER_SIZE], int maxOptions) {
    WIN32_FIND_DATA findFileData;
    char searchPath[PATH_BUFFER_SIZE];
    snprintf(searchPath, PATH_BUFFER_SIZE, "%s\\*.txt", directory);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    int count = 0;
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            strncpy(optionFiles[count], findFileData.cFileName, BUFFER_SIZE);
            optionFiles[count][BUFFER_SIZE-1] = '\0';
            char name[BUFFER_SIZE];
            strncpy(name, findFileData.cFileName, BUFFER_SIZE);
            name[BUFFER_SIZE-1] = '\0';
            char *dot = strrchr(name, '.');
            if (dot) *dot = '\0';
            strncpy(optionTexts[count], name, BUFFER_SIZE);
            optionTexts[count][BUFFER_SIZE-1] = '\0';
            count++;
        }
    } while (FindNextFile(hFind, &findFileData) && count < maxOptions);
    FindClose(hFind);
    return count;
}
#endif

static void DrawBackgroundAndSprites(Scene *scene) {
    if (scene->hasBackground) {
        Texture2D bgTex = scene->background;
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();
        float scale_bg = (float)windowHeight / (float)bgTex.height;
        float desired_tex_width = (float)windowWidth / scale_bg;
        float crop_x = (bgTex.width - desired_tex_width) / 2.0f;
        Rectangle srcRect = { crop_x, 0, desired_tex_width, (float)bgTex.height };
        Rectangle dstRect = { 0, 0, (float)windowWidth, (float)windowHeight };
        DrawTexturePro(bgTex, srcRect, dstRect, (Vector2){0, 0}, 0.0f, WHITE);

        for (int i = 0; i < scene->spriteCount; i++) {
            Texture2D sprTex = scene->sprites[i].texture;
            float drawn_x = (scene->sprites[i].pos.x - crop_x) * scale_bg;
            float drawn_y = scene->sprites[i].pos.y * scale_bg;
            float sprite_scale = (windowHeight) / (float)sprTex.height;
            Rectangle sprSrc = { 0, 0, (float)sprTex.width, (float)sprTex.height };
            Rectangle sprDst = { drawn_x, drawn_y, sprTex.width * sprite_scale, sprTex.height * sprite_scale };
            DrawTexturePro(sprTex, sprSrc, sprDst, (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
}

static int DrawVerticalOptions(Rectangle dialog, int numOptions, char optionTexts[][BUFFER_SIZE], int fontSize, int padding) {
    Vector2 mousePoint = GetMousePosition();
    int clickedOption = -1;
    for (int i = 0; i < numOptions; i++) {
        int textWidth = MeasureText(optionTexts[i], fontSize);
        int textHeight = fontSize;
        float optX = dialog.x + (dialog.width - (textWidth + 2 * padding)) / 2.0f;
        float optY = dialog.y + 50 + i * (textHeight + 2 * padding + 10);
        Rectangle optRect = { optX, optY, textWidth + 2 * padding, textHeight + 2 * padding };
        DrawRectangleRec(optRect, Fade(LIGHTGRAY, 0.8f));
        DrawRectangleLinesEx(optRect, 2, WHITE);
        DrawText(optionTexts[i], optRect.x + padding, optRect.y + padding, fontSize, BLACK);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, optRect)) {
            clickedOption = i;
        }
    }
    return clickedOption;
}

static void DrawTextDialog(SceneCommand *cmd, int baseWidth, int baseHeight) {
    const float defaultXRel = 60.0f / (float)baseWidth;
    const float defaultYRel = 330.0f / (float)baseHeight;
    const float defaultWidthRel = 685.0f / (float)baseWidth;
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
}

static void ProcessCommand(Scene *scene, SceneCommand *cmd) {
    char path[PATH_BUFFER_SIZE];
    switch(cmd->type) {
        case CMD_BG:
            if (scene->hasBackground) UnloadTexture(scene->background);
            snprintf(path, PATH_BUFFER_SIZE, "assets/images/%s", cmd->arg1);
            {
                Image img = LoadImage(path);
                scene->background = LoadTextureFromImage(img);
                UnloadImage(img);
                scene->hasBackground = true;
            }
            break;
        case CMD_SPRITE:
            if (scene->spriteCount < MAX_SPRITES) {
                snprintf(path, PATH_BUFFER_SIZE, "assets/images/%s", cmd->arg2);
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
            snprintf(path, PATH_BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            scene->music = LoadMusicStream(path);
            PlayMusicStream(scene->music);
            if (cmd->hasMusicStart) {
                SeekMusicStream(scene->music, cmd->musicStart);
            }
            scene->hasMusic = true;
            break;
        case CMD_SOUND:
            snprintf(path, PATH_BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            {
                Sound s = LoadSound(path);
                PlaySound(s);
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
            break;
        case CMD_SCENE:
            break;
        case CMD_END:
            scene->screen = TITLE;
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

// TODO: there must be a way to refactor this into something more reasonable
static void LoadSceneFromFile(Scene *scene, const char *sceneFile) {
    scene->commandCount = 0;
    scene->currentCommand = 0;
    FILE *fp;
    char path[PATH_BUFFER_SIZE];
    snprintf(path, PATH_BUFFER_SIZE, "assets/scenes/%s", sceneFile);
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
                        while(*p ==' ') p++;
                        strcpy(cmd->spriteID, p);
                        cmd->hasSpriteID = true;
                    } else if (strncmp(line, ":pos", 4)==0) {
                        char *p = line + 4;
                        while(*p ==' ') p++;
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
                        while(*p ==' ') p++;
                        strcpy(cmd->spriteID, p);
                        cmd->hasSpriteID = true;
                    }
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
                        while(*p == ' ') p++;
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
                        while(*p == ' ') p++;
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
        } else if (strncmp(line, "::end", 5)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_END;
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
    InitWindow(baseWidth, baseHeight, "vn-engine");
    InitAudioDevice();

    Scene scene = {0};
    scene.screen = TITLE;

    static bool showStartDialog = false;
    static char startOptionTexts[MAX_START_OPTIONS][BUFFER_SIZE];
    static char startOptionFiles[MAX_START_OPTIONS][BUFFER_SIZE];
    static int numStartOptions = 0;

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
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        switch (scene.screen) {
            case TITLE:
            {
                DrawText("TITLE SCREEN", 20, 20, 40, DARKGREEN);
                Rectangle startButton = { GetScreenWidth()/2 - 130, GetScreenHeight()/2, 260, 50 };
                DrawRectangleRec(startButton, BLUE);
                DrawText("Choose Starting Scene", startButton.x + 10, startButton.y + 15, 20, WHITE);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), startButton))
                {
                    numStartOptions = GetSceneOptions("assets/scenes", startOptionTexts, startOptionFiles, MAX_START_OPTIONS);
                    showStartDialog = true;
                }
                if (showStartDialog)
                {
                    Rectangle dialog = { GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 150, 400, 300 };
                    DrawRectangleRec(dialog, Fade(DARKGRAY, 0.8f));
                    DrawRectangleLinesEx(dialog, 2, WHITE);
                    int clicked = DrawVerticalOptions(dialog, numStartOptions, startOptionTexts, 20, 10);
                    if (clicked != -1) {
                        LoadSceneFromFile(&scene, startOptionFiles[clicked]);
                        scene.screen = GAMEPLAY;
                        showStartDialog = false;
                    }
                }
            }
            break;
            case GAMEPLAY:
            {
                DrawBackgroundAndSprites(&scene);
                if (scene.currentCommand < scene.commandCount) {
                    SceneCommand *cmd = &scene.commands[scene.currentCommand];
                    if (cmd->type == CMD_TEXT) {
                        DrawTextDialog(cmd, baseWidth, baseHeight);
                    } else if (cmd->type == CMD_SCENE) {
                        Rectangle dialog = { GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 100, 400, 200 };
                        DrawRectangleRec(dialog, Fade(DARKGRAY, 0.8f));
                        DrawRectangleLinesEx(dialog, 2, WHITE);

                        char sceneOptions[MAX_OPTIONS][BUFFER_SIZE];
                        for (int i = 0; i < MAX_OPTIONS; i++) {
                            strncpy(sceneOptions[i], cmd->options[i].optionText, BUFFER_SIZE - 1);
                            sceneOptions[i][BUFFER_SIZE - 1] = '\0';
                        }
                        int clicked = DrawVerticalOptions(dialog, cmd->optionCount, sceneOptions, 20, 10);
                        if (clicked != -1) {
                            LoadSceneFromFile(&scene, cmd->options[clicked].nextScene);
                        }
                    }
                }
            }
            break;
            default: break;
        }
        EndDrawing();
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

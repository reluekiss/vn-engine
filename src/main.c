#include "raylib.h"
#include "boundedtext.h"
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COMMANDS 50
#define MAX_OPTIONS 2
#define MAX_SPRITES 10

typedef enum { CMD_BG, CMD_SPRITE, CMD_MUSIC, CMD_SOUND, CMD_TEXT, CMD_SCENE } CommandType;

typedef struct {
    char optionText[BUFFER_SIZE];
    char nextScene[BUFFER_SIZE];
} SceneOption;

typedef struct {
    CommandType type;
    char arg1[BUFFER_SIZE];  // For text content or other command data
    char arg2[BUFFER_SIZE];  // e.g. sprite filename

    // For text commands:
    char name[BUFFER_SIZE];
    bool hasName;
    Vector2 pos;  // Position (top-left) of the textbox in base resolution pixels (800x450)
    bool hasPos;

    int optionCount;
    SceneOption options[MAX_OPTIONS]; // For scene commands
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
    } sprites[MAX_SPRITES];
    int spriteCount;

    Music music;
    bool hasMusic;
} Scene;

static void UnloadSceneAssets(Scene *scene) {
    if (scene->hasBackground) { UnloadTexture(scene->background); scene->hasBackground = false; }
    for (int i = 0; i < scene->spriteCount; i++) UnloadTexture(scene->sprites[i].texture);
    scene->spriteCount = 0;
    if (scene->hasMusic) { StopMusicStream(scene->music); UnloadMusicStream(scene->music); scene->hasMusic = false; }
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
                scene->sprites[scene->spriteCount].texture = LoadTextureFromImage(img);
                UnloadImage(img);
                int x = 0, y = 0;
                sscanf(cmd->arg1, "%dx%d", &x, &y);
                scene->sprites[scene->spriteCount].pos = (Vector2){ x, y };
                scene->spriteCount++;
            }
            break;
        case CMD_MUSIC:
            if (scene->hasMusic) { StopMusicStream(scene->music); UnloadMusicStream(scene->music); }
            snprintf(path, BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            scene->music = LoadMusicStream(path);
            PlayMusicStream(scene->music);
            scene->hasMusic = true;
            break;
        case CMD_SOUND:
            snprintf(path, BUFFER_SIZE, "assets/music/%s", cmd->arg1);
            {
                Sound s = LoadSound(path);
                PlaySound(s);
                UnloadSound(s);
            }
            break;
        case CMD_TEXT:
            // Text commands wait for user input.
            break;
        case CMD_SCENE:
            // Scene commands wait for user selection.
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
    while (fgets(line, BUFFER_SIZE, fp)) {
        int len = (int)strlen(line);
        if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
        if (strlen(line)==0) continue;
        if (strncmp(line, "::bg", 4)==0) {
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                SceneCommand *cmd = &scene->commands[scene->commandCount++];
                cmd->type = CMD_BG;
                strcpy(cmd->arg1, line);
            }
        } else if (strncmp(line, "::sprite", 8)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_SPRITE;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                strcpy(cmd->arg1, line);  // e.g. "200x200"
            }
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                strcpy(cmd->arg2, line);  // Sprite filename
            }
        } else if (strncmp(line, "::music", 7)==0) {
            SceneCommand *cmd = &scene->commands[scene->commandCount++];
            cmd->type = CMD_MUSIC;
            if (fgets(line, BUFFER_SIZE, fp)) {
                len = (int)strlen(line);
                if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
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
            // Read subsequent lines: parameters (starting with ':') then text content.
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
                    // Peek next line; if it doesn't start with ':' assume it's the text content.
                    long currentPos = ftell(fp);
                    if (!fgets(line, BUFFER_SIZE, fp))
                        break;
                    len = (int)strlen(line);
                    if(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[len-1] = '\0';
                    if(line[0] != ':') { strcpy(cmd->arg1, line); break; }
                }
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
    ProcessAutoCommands(scene);
}

int main(void)
{
    // Base resolution for relative positioning.
    const int baseWidth = 800, baseHeight = 450;
    InitWindow(baseWidth, baseHeight, "raylib scene system with text params");
    InitAudioDevice();

    Scene scene = {0};
    LoadSceneFromFile(&scene, "scene1.txt");

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        if (scene.hasMusic) UpdateMusicStream(scene.music);

        // Handle waiting commands.
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

            // Draw text box if waiting for text.
            if (scene.currentCommand < scene.commandCount) {
                SceneCommand *cmd = &scene.commands[scene.currentCommand];
                if (cmd->type == CMD_TEXT) {
                    // Default textbox relative values (from base resolution)
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

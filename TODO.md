```c
typedef union {
    char c[BUFFER_SIZE];
    Vector2 v;
    bool b;
} argType;

typedef struct {
    CommandType type;
    argType arg1;   
    argType arg2;   
    argType arg3;   

    int optionCount;
    SceneOption options[MAX_OPTIONS];
} SceneCommand;
```
use the above SceneCommand type to refactor the LoadSceneFromFile function, which just needs quite a bit of work ;-;

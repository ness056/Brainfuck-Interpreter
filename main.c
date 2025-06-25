#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    NOOP_TOKEN = 0,
    MAIN_BLOCK_TOKEN,
    MOVE_TOKEN,         // >> or <<
    INCREMENT_TOKEN,    // ++ or --
    INPUT_TOKEN,        // ,
    OUTPUT_TOKEN,       // .
    LOOP_TOKEN          // [
} TokenType;

typedef struct Token {
    TokenType type;
    union {
        int value;
        struct {
            struct Token *children;
            size_t nbChildren;
            size_t reservedChildren;
        };
    };
} Token;

void tokenInit(Token *token, TokenType type) {
    memset(token, 0, sizeof(Token));

    token->type = type;
    if (type == MAIN_BLOCK_TOKEN || type == LOOP_TOKEN) {
        token->reservedChildren = 20;
        token->children = malloc(sizeof(Token) * token->reservedChildren);
    }
}

void tokenFree(Token *token) {
    if (token->type != MAIN_BLOCK_TOKEN && token->type != LOOP_TOKEN) return;

    for (int i = 0; i < token->nbChildren; i++) {
        tokenFree(&token->children[i]);
    }
    free(token->children);
}

// child must be re-init to be used again.
void tokenAppend(Token *parent, Token *child) {
    if (child->type == NOOP_TOKEN) return;

    if (parent->type != MAIN_BLOCK_TOKEN && parent->type != LOOP_TOKEN) {
        perror("Error: parent argument of function tokenAppend must be of type MAIN_BLOCK_TOKEN or LOOP_TOKEN.\n");
        exit(1);
    }

    if (!parent->children) {
        perror("Error: parent->tokens is NULL.\n");
        exit(1);
    }

    if (parent->nbChildren >= parent->reservedChildren) {
        parent->reservedChildren *= 2;
        parent->children = realloc(parent->children,
            sizeof(Token) * parent->reservedChildren);

        if (!parent->children) {
            perror("Error: failed to realloc parent->children.\n");
            exit(1);
        }
    }

    parent->children[parent->nbChildren++] = *child;
    memset(child, 0, sizeof(Token));
}

bool tokenizer(FILE *file, Token *parentToken) {
    Token current;
    tokenInit(&current, NOOP_TOKEN);

    for (;;) {
        int c = fgetc(file);
        if (c == EOF) {
            tokenAppend(parentToken, &current);
            return true;
        } else if (c == ']') {
            tokenAppend(parentToken, &current);
            return false;
        }

        switch (c)
        {
        case '+':
            if (current.type != INCREMENT_TOKEN) {
                tokenAppend(parentToken, &current);
                tokenInit(&current, INCREMENT_TOKEN);
            }
            current.value++;
            break;

        case '-':
            if (current.type != INCREMENT_TOKEN) {
                tokenAppend(parentToken, &current);
                tokenInit(&current, INCREMENT_TOKEN);
            }
            current.value--;
            break;

        case '>':
            if (current.type != MOVE_TOKEN) {
                tokenAppend(parentToken, &current);
                tokenInit(&current, MOVE_TOKEN);
            }
            current.value++;
            break;

        case '<':
            if (current.type != MOVE_TOKEN) {
                tokenAppend(parentToken, &current);
                tokenInit(&current, MOVE_TOKEN);
            }
            current.value--;
            break;

        case ',':
            tokenAppend(parentToken, &current);
            tokenInit(&current, INPUT_TOKEN);
            break;

        case '.':
            tokenAppend(parentToken, &current);
            tokenInit(&current, OUTPUT_TOKEN);
            break;

        case '[':
            tokenAppend(parentToken, &current);
            tokenInit(&current, LOOP_TOKEN);
            if (tokenizer(file, &current)) {
                fprintf(stderr, "Error: a loop is opened but never closed.\n");
                exit(1);
            }
            break;
        }
    }
}

typedef struct {
    unsigned char *numbers;
    int position;
    size_t size;
} Data;

void dataInit(Data *data) {
    data->size = 200;
    data->position = 100;
    data->numbers = calloc(data->size, sizeof(unsigned char));
}

void dataFree(Data *data) {
    free(data->numbers);
}

void dataSet(Data *data, unsigned char value) {
    data->numbers[data->position] = value;
}

unsigned char dataGet(Data *data) {
    return data->numbers[data->position];
}

void dataIncrement(Data *data, int value) {
    unsigned char result = (value + data->numbers[data->position]) % 256;
    data->numbers[data->position] = result;
}

void dataMove(Data *data, int value) {
    data->position += value;
    if (data->position >= data->size) {
        size_t oldSize = data->size;

        data->size += 200;
        data->numbers = realloc(data->numbers, sizeof(unsigned char) * data->size);

        if (!data->numbers) {
            perror("Error: failed to realloc data->numbers.");
            exit(1);
        }

        memset(data->numbers + oldSize, 0, 200);
    } else if (data->position < 0) {
        size_t oldSize = data->size;
        unsigned char *oldArray = data->numbers;

        data->size += 200;
        data->numbers = malloc(sizeof(unsigned char) * data->size);

        memcpy(data->numbers + 200, oldArray, oldSize);
    }
}

void execute(Data *data, Token *token) {
    Token *test;
    if (token->type == MAIN_BLOCK_TOKEN) test = &token->children[1];

    switch (token->type)
    {
    case NOOP_TOKEN:
        break;

    case MAIN_BLOCK_TOKEN:
        for (int i = 0; i < token->nbChildren; i++) {
            execute(data, &token->children[i]);
        }
        break;
        
    case INCREMENT_TOKEN:
        dataIncrement(data, token->value);
        break;
        
    case MOVE_TOKEN:
        dataMove(data, token->value);
        break;
        
    case INPUT_TOKEN:
        int c = fgetc(stdin);
        dataSet(data, c);
        break;
        
    case OUTPUT_TOKEN:
        printf("%c", dataGet(data));
        break;
        
    case LOOP_TOKEN:
        while (dataGet(data) != 0) {
            for (int i = 0; i < token->nbChildren; i++) {
                execute(data, &token->children[i]);
            }
        }
        break;
    
    default:
        fprintf(stderr, "Error: unknown token type %d.\n", token->type);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    FILE *file;

    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "r");
    if (!file) {
        printf("%s", argv[1]);
        perror("Error opening file");
        return 1;
    }
    
    Token mainBlock;
    tokenInit(&mainBlock, MAIN_BLOCK_TOKEN);
    tokenizer(file, &mainBlock);
    fclose(file);

    Data data;
    dataInit(&data);
    execute(&data, &mainBlock);

    tokenFree(&mainBlock);
    dataFree(&data);

    return 0;
}
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h> // For size_t

// Define CommandType and Command if they are not defined elsewhere
typedef enum {
    COMMAND_TYPE_HTTP,
    COMMAND_TYPE_MQTT,
    // Add other command types here if necessary
} CommandType;

typedef struct {
    const char *command;
    unsigned int waitTimeMs;
} Command;

// Declare the function
Command *getCommands(CommandType type, size_t *outSize);

#endif // COMMANDS_H

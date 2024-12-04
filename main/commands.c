#include "commands.h"

// Define the function
Command *getCommands(CommandType type, size_t *outSize) {
    switch (type) {
    case COMMAND_TYPE_HTTP: {
        static Command httpCommands[] = {
            {"AT+SHREQ=?\r\n", 1000},
            {"AT+SHREQ?\r\n", 1000},
            {"AT+CNACT=0,1\r\n", 1000},
            {"AT+SHCONF=\"URL\",\"http://www.httpbin.org\"\r\n", 1000},
            {"AT+SHCONF=\"BODYLEN\",1024\r\n", 0},
            {"AT+SHCONF=\"HEADERLEN\",350\r\n", 0},
            {"AT+SHCONN\r\n", 1000},
            {"AT+SHSTATE?\r\n", 1000},
            {"AT+SHREQ=\"http://www.httpbin.org/get\",1\r\n", 10000},
            {"AT+SHREAD=0,391\r\n", 3000},
            {"AT+SHDISC\r\n", 0}};
        *outSize = sizeof(httpCommands) / sizeof(httpCommands[0]);
        return httpCommands;
    }
    case COMMAND_TYPE_MQTT: {
        static Command mqttCommands[] = {
            {"AT+CMQTTSTART\r\n", 1000},
            {"AT+CMQTTACCQ=0,\"clientId123\"\r\n", 1000},
            {"AT+CMQTTCONNECT=0,\"139.162.164.160\",1883\r\n", 5000},
            {"AT+CMQTTTOPIC=0,22\r\n", 100},
            {"example/topic/path\r\n", 100},
            {"AT+CMQTTPAYLOAD=0,14\r\n", 100},
            {"{\"key\":\"value\"}\r\n", 100},
            {"AT+CMQTTPUB=0,1,60\r\n", 5000},
            {"AT+CMQTTDISC=0,60\r\n", 1000},
            {"AT+CMQTTSTOP\r\n", 1000}};
        *outSize = sizeof(mqttCommands) / sizeof(mqttCommands[0]);
        return mqttCommands;
    }
    default: {
        *outSize = 0;
        return NULL;
    }
    }
}

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "commands.h"

static const int RX_BUF_SIZE = 1024;

// These pins are for the heltec lora 32 v2
#define TXD_PIN (GPIO_NUM_12)
#define RXD_PIN (GPIO_NUM_13)

QueueHandle_t sendControlQueue = NULL;
QueueHandle_t sleepQueue = NULL;
QueueHandle_t receiveMsgQueue = NULL;
QueueHandle_t dataQueue = NULL;

static TimerHandle_t timeoutTimer;

#define queueLength 10
#define COMAND_SLEEP_TIME 15 * 1000

typedef enum
{
    COMMAND_ACTION_NEXT,
    COMMAND_ACTION_RESTART
} CommandAction;

void init_uart(void) {
  const uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  // We won't use a buffer for sending data.
  uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
  uart_param_config(UART_NUM_1, &uart_config);
  uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
}

int sendData(const char *logName, const char *data) {
  const int len = strlen(data);
  const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
  ESP_LOGI(logName, "Wrote %d bytes, data: %s", txBytes, data);

  return txBytes;
}

static void processCommands(const char *taskTag, Command *commands,
                            size_t commandCount,
                            QueueHandle_t sendControlQueue) {
  size_t i = 0;
  uint32_t receivedMessage;

  while (1) {
    // Wait for a message in the sendControlQueue
    if ((!xQueueReceive(sendControlQueue, &receivedMessage, portMAX_DELAY)) ==
        pdTRUE) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    switch (receivedMessage) {
    case COMMAND_ACTION_NEXT:
      if (i < commandCount) {
        sendData(taskTag, commands[i].command);
        if (commands[i].waitTimeMs > 0) {
          vTaskDelay(commands[i].waitTimeMs / portTICK_PERIOD_MS);
        }
        i++;
      } else {
        ESP_LOGI(taskTag, "Reached end of command list.");
      }
      break;

    case COMMAND_ACTION_RESTART:
      ESP_LOGI(taskTag, "Restarting command list.");
      i = 0;
      // Send the first command again
      sendData(taskTag, commands[i].command);
      break;

        default:
            int msg = receivedMessage;
            ESP_LOGW(taskTag, "Received unrecognized command: %d", msg);
            break;
        }
    }
}

static void tx_task(void *arg) {
  static const char *TX_TASK_TAG = "TX_TASK";
  esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

  size_t commandCount;
  CommandType currentType =
      COMMAND_TYPE_HTTP; // Change to COMMAND_TYPE_MQTT for MQTT commands
  Command *commands = getCommands(currentType, &commandCount);

  // Initial delay to allow system to get ready before sending commands
  vTaskDelay(20000 / portTICK_PERIOD_MS);

  if (commands != NULL) {
    processCommands(TX_TASK_TAG, commands, commandCount, sendControlQueue);
  }

  vQueueDelete(sendControlQueue); // Clean up the queue (though this point is
                                  // never reached)
}

static void send_next_command() {
  uint32_t action = COMMAND_ACTION_NEXT;
  ESP_LOGI("Controller", "Sending next command.");
  xQueueSend(sendControlQueue, &action, portMAX_DELAY);
}

static void send_reset_command() {
  uint32_t action = COMMAND_ACTION_RESTART;
  ESP_LOGI("Controller", "Sending reset command.");
  xQueueSend(sendControlQueue, &action, portMAX_DELAY);
}

// Timer callback function for if the controller does not receive any data for
// 15 seconds
void timeout_callback(TimerHandle_t xTimer) {
  // Log timeout and send reset command
  uint32_t resetAction = COMMAND_ACTION_RESTART;
  ESP_LOGI("Controller",
           "No data received for 15 seconds, sending reset command.");
  xQueueSend(sendControlQueue, &resetAction, portMAX_DELAY);

  // Immediately send a next command after the reset
  send_reset_command();

  // Reset the timer
  xTimerReset(timeoutTimer, portMAX_DELAY);
}

static void controller_task(void *arg)
{
    timeoutTimer = xTimerCreate("TimeoutTimer", pdMS_TO_TICKS(COMAND_SLEEP_TIME), pdFALSE, NULL, timeout_callback);
    if (timeoutTimer != NULL)
    {
        xTimerStart(timeoutTimer, portMAX_DELAY);
    }

    // At task startup, send the first next command
    send_next_command();

    while (1)
    {
        uint8_t *receivedData = NULL; // Pointer to received data
        if (xQueueReceive(receiveMsgQueue, &receivedData, portMAX_DELAY) == pdTRUE)
        {
            if (receivedData == NULL)
            {
                continue;
            }

            ESP_LOGI("Controller", "Received data: %s", receivedData);

            // Reset the timer since data was received
            xTimerReset(timeoutTimer, portMAX_DELAY);

            // If the data contains a OK in the string, send COMMAND_ACTION_NEXT
            if (strstr((const char *)receivedData, "OK") != NULL)
            {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                send_next_command();
            }
            // If the data contains ERROR in the string, send COMMAND_ACTION_RESTART
            else if (strstr((const char *)receivedData, "ERROR") != NULL)
            {
                // wait for 0.5 seconds before sending the reset command
                vTaskDelay(500 / portTICK_PERIOD_MS);
                send_reset_command();
            }

            free(receivedData);
        }
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    if (data == NULL)
    {
        ESP_LOGE(RX_TASK_TAG, "Failed to allocate memory for data buffer");
        return;
    }

    ESP_LOGI(RX_TASK_TAG, "Starting RX task");

    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);

        if (rxBytes == -1)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ESP_LOGI(RX_TASK_TAG, "Error reading data");
            continue;
        }
        if (rxBytes == 0) {
            continue;
        }

        data[rxBytes] = 0; // Null-terminate the received data

        // Allocate a new buffer for each queue item
        // Fix so we dont malloc every time in the loop ?
        uint8_t *queueData = (uint8_t *)malloc(rxBytes + 1);
        if (queueData != NULL)
        {
            memcpy(queueData, data, rxBytes + 1); // Copy the data and null-terminator
            if (xQueueSend(receiveMsgQueue, &queueData, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGE(RX_TASK_TAG, "Failed to send data to queue");
                free(queueData);
            }
        }
    }

    free(data);
}

static void deep_sleep_task(void *args)
{
    while (1)
    {
        uint32_t sleepTime = 10;
        xQueueSend(sleepQueue, &sleepTime, 0);
        vTaskDelay(1000000 / portTICK_PERIOD_MS);
    }
}

esp_err_t init_queues() {
  sendControlQueue = xQueueCreate(queueLength, sizeof(uint32_t));
  if (sendControlQueue == NULL)
    return ESP_FAIL;

  receiveMsgQueue = xQueueCreate(queueLength, sizeof(uint8_t *));
  if (receiveMsgQueue == NULL)
    return ESP_FAIL;

  sleepQueue = xQueueCreate(queueLength, sizeof(uint32_t));
  if (sleepQueue == NULL)
    return ESP_FAIL;

  dataQueue = xQueueCreate(queueLength, sizeof(uint8_t *));
  if (dataQueue == NULL)
    return ESP_FAIL;

  return ESP_OK;
}

void app_main(void) {
  init_uart();
  if (init_queues() != ESP_OK) {
    ESP_LOGE("APP_MAIN", "Queue initialization failed, restarting...");
    esp_restart();
  }

    // TODO: Set up priority correctly
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(controller_task, "controller_task", 1024 * 2, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(deep_sleep_task, "deep_sleep_task", 1024 * 2, NULL, configMAX_PRIORITIES - 5, NULL);
}

/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

static const int RX_BUF_SIZE = 1024;

// These pins are for the heltec lora 32 v2
#define TXD_PIN (GPIO_NUM_12)
#define RXD_PIN (GPIO_NUM_13)

void init(void)
{
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
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    while (1) {
        // A simple http request from 
        // https://m2msupport.net/m2msupport/http-testing-with-simcom-sim7070-sim7080-modules/

        sendData(TX_TASK_TAG, "AT+SHREQ=?\r\n");  // Get the support for the commands
        vTaskDelay(1000 / portTICK_PERIOD_MS);   // WAIT=1

        sendData(TX_TASK_TAG, "AT+SHREQ?\r\n");  // Get the current request
        vTaskDelay(1000 / portTICK_PERIOD_MS);   // WAIT=1

        sendData(TX_TASK_TAG, "AT+CNACT=0,1\r\n");  // Activate the network bearer
        vTaskDelay(1000 / portTICK_PERIOD_MS);   // WAIT=1

        sendData(TX_TASK_TAG, "AT+SHCONF=\"URL\",\"http://www.httpbin.org\"\r\n");  // Set up the HTTP URL
        vTaskDelay(1000 / portTICK_PERIOD_MS);                                      // WAIT=1

        sendData(TX_TASK_TAG, "AT+SHCONF=\"BODYLEN\",1024\r\n");  // HTTP body length
        sendData(TX_TASK_TAG, "AT+SHCONF=\"HEADERLEN\",350\r\n");  // HTTP head length

        sendData(TX_TASK_TAG, "AT+SHCONN\r\n");  // HTTP connection
        vTaskDelay(1000 / portTICK_PERIOD_MS);   // WAIT=1

        sendData(TX_TASK_TAG, "AT+SHSTATE?\r\n");  // Get the connection status
        vTaskDelay(1000 / portTICK_PERIOD_MS);   // WAIT=1

        sendData(TX_TASK_TAG, "AT+SHREQ=\"http://www.httpbin.org/get\",1\r\n");  // Make the HTTP request
        vTaskDelay(10000 / portTICK_PERIOD_MS);  // WAIT=10

        sendData(TX_TASK_TAG, "AT+SHREAD=0,391\r\n");  // Read the HTTP response
        vTaskDelay(3000 / portTICK_PERIOD_MS);   // WAIT=3

        sendData(TX_TASK_TAG, "AT+SHDISC\r\n");  // Disconnect HTTP
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

void app_main(void)
{
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
}

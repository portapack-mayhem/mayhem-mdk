#include <iostream>
#include <memory>
#include <cstring>
#include <vector>
#include <queue>
#include <algorithm>

#include "driver/i2c.h"
#include "driver/uart.h"
#include "uart_app.h"

#include "ppi2c/pp_handler.hpp"

static_assert(sizeof(uart_app) % 32 == 0, "app size must be multiple of 32 bytes. fill with 0s");

#define I2C_SLAVE_SDA_IO GPIO_NUM_6
#define I2C_SLAVE_SCL_IO GPIO_NUM_5

#define UART_RX GPIO_NUM_14

#define ESP_SLAVE_ADDR 0x51

#define LED_RED GPIO_NUM_46
#define LED_GREEN GPIO_NUM_0
#define LED_BLUE GPIO_NUM_45

// User commands
#define USER_COMMANDS_START 0x7F01
// UART specific commands
#define COMMAND_UART_REQUESTDATA_SHORT USER_COMMANDS_START
#define COMMAND_UART_REQUESTDATA_LONG (USER_COMMANDS_START + 1)
#define COMMAND_UART_BAUDRATE_INC (USER_COMMANDS_START + 2)
#define COMMAND_UART_BAUDRATE_DEC (USER_COMMANDS_START + 3)
#define COMMAND_UART_BAUDRATE_GET (USER_COMMANDS_START + 4)

void initialize_uart(uint32_t baudrate);
void deinitialize_uart();

uint32_t baudrate = 115200;

std::vector<uint32_t> baudrates = {50, 75, 110, 134, 150, 200, 300, 600,
                                   1200, 2400, 4800, 9600, 14400, 19200,
                                   28800, 38400, 57600, 115200, 230400,
                                   460800, 576000, 921600, 1843200, 3686400};

void initialize_gpio()
{
    gpio_install_isr_service(0);

    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_RED, 1);
    gpio_set_level(LED_GREEN, 1);
    gpio_set_level(LED_BLUE, 1);
}

std::queue<uint8_t> uart_queue;

#define BUF_SIZE (1024)

void initialize_uart(uint32_t baudrate)
{
    uart_config_t uart_config = {
        .baud_rate = (int)baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
};

    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void deinitialize_uart()
{
    ESP_ERROR_CHECK(uart_driver_delete(UART_NUM_1));
}

static void uart_task(void *arg)
{
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    while (true)
    {
        try
        {
            int len = uart_read_bytes(UART_NUM_1, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
            if (len != 0)
            {
                for (int i = 0; i < len; i++)
                    uart_queue.push(data[i]);
            }
        }
        catch (const std::exception &ex)
        {
            std::cout << "Exception: " << std::endl;
            std::cout << ex.what() << std::endl;
        }
    }
}

extern "C" void app_main(void)
{
    initialize_gpio();
    PPHandler::set_module_name("ESP32-S3-PPDEVKIT");
    PPHandler::set_module_version(1);
    PPHandler::add_app(uart_app, sizeof(uart_app));
    PPHandler::add_custom_command(COMMAND_UART_REQUESTDATA_SHORT, nullptr, [](pp_command_data_t data)
                                  {
                                      // 1 bit: more data available
                                      // 7 bit: data length [0 to 4]
                                      // 4 bytes: data from uart_queue optionally filled with 0xFF

                                      const int max_data_length = 4;
                                      data.data->resize(1 + max_data_length);

                                      uint8_t bytesToSend = uart_queue.size() > max_data_length ? max_data_length : uart_queue.size();
                                      bool moreData = uart_queue.size() > max_data_length ? 1 : 0;
                                      (*data.data)[0] = (bytesToSend & 0x7F) | (moreData << 7);

                                      for (int i = bytesToSend; i < max_data_length; i++)
                                          (*data.data)[i + 1] = 0xFF;

                                      for (int i = 0; i < bytesToSend; i++)
                                      {
                                          (*data.data)[i + 1] = uart_queue.front();
                                          uart_queue.pop();
                                      } });
    PPHandler::add_custom_command(COMMAND_UART_REQUESTDATA_LONG, nullptr, [](pp_command_data_t data)
                                  {
                                    // 1 bit: more data available
                                    // 7 bit: data length [0 to 4]
                                    // 4 bytes: data from uart_queue optionally filled with 0xFF

                                    // 1 bit: more data available
                                    // 7 bit: data length [0 to 127]
                                    // 127 bytes: data from uart_queue optionally filled with 0xFF

                                    const int max_data_length = 127;
                                    data.data->resize(1 + max_data_length);                                    

                                    uint8_t bytesToSend = uart_queue.size() > max_data_length ? max_data_length : uart_queue.size();
                                    bool moreData = uart_queue.size() > max_data_length ? 1 : 0;
                                    (*data.data)[0] = (bytesToSend & 0x7F) | (moreData << 7);

                                    for (int i = bytesToSend; i < max_data_length; i++)
                                        (*data.data)[i + 1] = 0xFF;

                                    for (int i = 0; i < bytesToSend; i++)
                                    {
                                        (*data.data)[i + 1] = uart_queue.front();
                                        uart_queue.pop();
                                    } });
    PPHandler::add_custom_command(COMMAND_UART_BAUDRATE_GET, nullptr, [](pp_command_data_t data)
                                  {
                                    data.data->resize(4);
                                    esp_rom_printf("COMMAND_UART_BAUDRATE_GET: %d\n", baudrate);
                                    *(uint32_t *)(*data.data).data() = baudrate; });

    PPHandler::add_custom_command(COMMAND_UART_BAUDRATE_INC, [](pp_command_data_t data)
                                  {
                                      deinitialize_uart();
                                      if (baudrate == baudrates.back())
                                          baudrate = baudrates.front();
                                      else
                                      {
                                          auto it = std::find(baudrates.begin(), baudrates.end(), baudrate);
                                          baudrate = *(it + 1);
                                      }
                                      esp_rom_printf("COMMAND_UART_BAUDRATE_INC: %d\n", baudrate);
                                      initialize_uart(baudrate); }, nullptr);

    PPHandler::add_custom_command(COMMAND_UART_BAUDRATE_DEC, [](pp_command_data_t data)
                                  {
                                    deinitialize_uart();
                                    if (baudrate == baudrates.front())
                                        baudrate = baudrates.back();
                                    else
                                    {
                                        auto it = std::find(baudrates.begin(), baudrates.end(), baudrate);
                                        baudrate = *(it - 1);
                                    }
                                    esp_rom_printf("COMMAND_UART_BAUDRATE_DEC: %d\n", baudrate);

                                    initialize_uart(baudrate); }, nullptr);
	PPHandler::init(I2C_SLAVE_SDA_IO, I2C_SLAVE_SCL_IO, ESP_SLAVE_ADDR);
    initialize_uart(baudrate);
    xTaskCreate(uart_task, "uart_task", 1024 * 2, (void *)0, 10, NULL);
    std::cout << "[PP MDK] PortaPack - Module Develoment Kit is ready." << std::endl;
}


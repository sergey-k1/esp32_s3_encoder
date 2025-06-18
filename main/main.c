#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define GPIO_ENC_LEFT GPIO_NUM_16
#define GPIO_ENC_RIGHT GPIO_NUM_17
#define GPIO_ENC_BUTT GPIO_NUM_15

#define ESP_INTR_FLAG_DEFAULT 0

QueueHandle_t gpioEncQueue = NULL;
QueueHandle_t gpioEncButtQueue = NULL;

// Основной счётчик энкодера
int count = 0;
// Счётчик нажатий на кнопку энкодера
int countButt = 0;
// Счётчик долгих нажатий на кнопку энкодера
int countLongButt = 0;
// Время нажатия на кнопку энкодера, время в микросекундах с момента загрузки
volatile uint64_t encoderButtonPressTime = 0;


// Функция обработки прерывания вращения ручки
void IRAM_ATTR gpioEncoderHander(void * arg)
{
    uint32_t gpioNum = (uint32_t) arg;
    xQueueSendFromISR(gpioEncQueue, &gpioNum, NULL);
}

// Функция обработки прерывания нажатия кнопки энкодера
void IRAM_ATTR gpioEncoderButtHander(void * arg)
{
    uint32_t gpioNum = (uint32_t) arg;
    xQueueSendFromISR(gpioEncButtQueue, &gpioNum, NULL);
    encoderButtonPressTime = esp_timer_get_time();
}


// Задача обработки вращения ручки энкодера
void gpioEncoderTask()
{
    uint32_t gpioNum;
    for (;;) {
        if (xQueueReceive(gpioEncQueue, &gpioNum, portMAX_DELAY)) {
            if (gpio_get_level(GPIO_ENC_LEFT) == gpio_get_level(GPIO_ENC_RIGHT)) {
                count--;
            } else {
                count++;
            }
            printf("Значение энкодера, %d\n", count);
        }
    }
    vTaskDelete(NULL);
}

// Задача обработки нажатия кнопки энкодера
void gpioEncoderButtTask() {
    uint32_t gpioNum;
    uint8_t longPress;
    for (;;) {
        if (xQueueReceive(gpioEncButtQueue, &gpioNum, portMAX_DELAY)) {
            longPress = 0;
            while (gpio_get_level(GPIO_ENC_BUTT)) {
                if ((esp_timer_get_time() - encoderButtonPressTime) > 1e6) {
                    longPress = 1;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            if (longPress) {
                printf("Долгое нажатие на клавишу, %d\n", ++countLongButt);
            } else {
                printf("Нажата кнопка, %d\n", ++countButt);
            }
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Для GPIO_ENC_LEFT
    gpio_reset_pin(GPIO_ENC_LEFT);
    gpio_set_direction(GPIO_ENC_LEFT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ENC_LEFT, GPIO_FLOATING);
    gpio_intr_disable(GPIO_ENC_LEFT);

    // Для GPIO_ENC_RIGHT
    gpio_reset_pin(GPIO_ENC_RIGHT);
    gpio_set_direction(GPIO_ENC_RIGHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ENC_RIGHT, GPIO_FLOATING);
    gpio_intr_enable(GPIO_ENC_RIGHT);
    gpio_set_intr_type(GPIO_ENC_RIGHT, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_ENC_RIGHT, gpioEncoderHander, (void *) GPIO_ENC_RIGHT);

    // Для GPIO_ENC_BUTT
    gpio_reset_pin(GPIO_ENC_BUTT);
    gpio_set_direction(GPIO_ENC_BUTT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ENC_BUTT, GPIO_FLOATING);
    gpio_intr_enable(GPIO_ENC_BUTT);
    gpio_set_intr_type(GPIO_ENC_BUTT, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(GPIO_ENC_BUTT, gpioEncoderButtHander, (void *) GPIO_ENC_BUTT);

    // Создаём очереди для передачи данных их прерываний
    gpioEncQueue = xQueueCreate(10, sizeof(uint32_t));
    gpioEncButtQueue = xQueueCreate(10, sizeof(uint32_t));

    // Создаём задачи
    xTaskCreate(gpioEncoderTask, "GpioEncoderTask", 0x800, NULL, 5, NULL);
    xTaskCreate(gpioEncoderButtTask, "GpioEncodeButtTask", 0x800, NULL, 5, NULL);
}

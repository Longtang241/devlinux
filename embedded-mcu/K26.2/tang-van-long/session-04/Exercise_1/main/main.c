#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"

#define TAG "BTN"

/* =========================
 * GPIO definitions
 * ========================= */

#define BTN_PIN GPIO_NUM_14

#define SEG_A GPIO_NUM_4
#define SEG_B GPIO_NUM_5
#define SEG_C GPIO_NUM_6
#define SEG_D GPIO_NUM_7
#define SEG_E GPIO_NUM_15
#define SEG_F GPIO_NUM_16
#define SEG_G GPIO_NUM_17

#define REGISTER_WIDTH_BITS 32

/* =========================
 * Timing
 * ========================= */

#define DEBOUNCE_MS       50
#define DOUBLE_CLICK_MS   400
#define LONG_PRESS_MS     1000
#define LONG_REPEAT_MS    500

/* =========================
 * Button event
 * ========================= */

typedef struct
{
    int64_t timestamp_us;
    bool is_press;
} btn_event_t;

static QueueHandle_t btn_queue;

/* =========================
 * Counter
 * ========================= */

static int counter = 0;

/* =========================
 * Register-level GPIO
 * ========================= */

/*
 * Set GPIO pin as output using GPIO registers.
 */
static void gpio_register_set_output(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_ENABLE_W1TS_REG = (1UL << pin);
    }
}

/*
 * Set GPIO output HIGH.
 */
static void gpio_register_set_high(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TS_REG = (1UL << pin);
    }
}

/*
 * Set GPIO output LOW.
 */
static void gpio_register_set_low(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TC_REG = (1UL << pin);
    }
}

static void display_write(uint8_t pattern)
{
    uint32_t pins[] ={SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};

    for (int i = 0; i < 7; i++)
    {
        if (pattern & (1 << i))
        {
            gpio_register_set_high(pins[i]);
        }
        else
        {
            gpio_register_set_low(pins[i]);
        }
    }
}

static const uint8_t digit_pattern[10] ={0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

static void display_show_number(int number)
{
    if (number < 0)
    {
        number += 10;
    }

    if (number >= 10)
    {
        number -= 10;
    }

    display_write(digit_pattern[number]);
}

static void counter_increment(void)
{
    counter++;

    if (counter > 9)
    {
        counter = 0;
    }

    display_show_number(counter);
}

static void counter_decrement(void)
{
    counter--;

    if (counter < 0)
    {
        counter = 9;
    }

    display_show_number(counter);
}

static void IRAM_ATTR button_isr(void *arg)
{
    btn_event_t event;

    int level = gpio_get_level(BTN_PIN);

    event.timestamp_us = esp_timer_get_time();
    event.is_press = (level == 0);

    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(
        btn_queue,
        &event,
        &higher_priority_task_woken
    );

    if (higher_priority_task_woken)
    {
        portYIELD_FROM_ISR();
    }
}

static void display_init(void)
{
    gpio_register_set_output(SEG_A);
    gpio_register_set_output(SEG_B);
    gpio_register_set_output(SEG_C);
    gpio_register_set_output(SEG_D);
    gpio_register_set_output(SEG_E);
    gpio_register_set_output(SEG_F);
    gpio_register_set_output(SEG_G);

    display_show_number(0);
}

static void button_init(void)
{
    gpio_config_t io_conf =
    {
        .pin_bit_mask = (1ULL << BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    btn_queue = xQueueCreate(10, sizeof(btn_event_t));

    if (btn_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button queue");
        abort();
    }

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            BTN_PIN,
            button_isr,
            NULL
        )
    );
}

static void gesture_task(void *arg)
{
    btn_event_t event;

    bool pressed = false;
    bool long_started = false;
    bool waiting_second_click = false;
    int64_t first_release_time = 0;
    int64_t last_event_time = 0;

    while (1)
    {
        TickType_t timeout = portMAX_DELAY;
        if (pressed)
        {
            if (!long_started)
            {
                timeout = pdMS_TO_TICKS(LONG_PRESS_MS);
            }
            else
            {
                timeout = pdMS_TO_TICKS(LONG_REPEAT_MS);
            }
        }

        else if (waiting_second_click)
        {
            int64_t elapsed_us =
                esp_timer_get_time() - first_release_time;

            int64_t remaining_us =
                ((int64_t)DOUBLE_CLICK_MS * 1000) - elapsed_us;

            if (remaining_us <= 0)
            {
                counter_increment();

                ESP_LOGI(
                    TAG,
                    "CLICK -> %d",
                    counter
                );

                waiting_second_click = false;

                continue;
            }

            timeout = pdMS_TO_TICKS(
                (remaining_us + 999) / 1000
            );
        }

        if (xQueueReceive(
                btn_queue,
                &event,
                timeout) == pdTRUE)
        {
            if (last_event_time != 0)
            {
                int64_t delta_us =
                    event.timestamp_us - last_event_time;

                if (delta_us < ((int64_t)DEBOUNCE_MS * 1000))
                {
                    continue;
                }
            }

            last_event_time = event.timestamp_us;

            if (event.is_press)
            {
                if (waiting_second_click)
                {
                    waiting_second_click = false;
                }

                pressed = true;
                long_started = false;

                ESP_LOGI(TAG, "PRESS");
            }
            else
            {
                pressed = false;
                if (long_started)
                {
                    ESP_LOGI(
                        TAG,
                        "LONG end -> %d",
                        counter
                    );

                    long_started = false;
                }
                else
                {
                    waiting_second_click = true;
                    first_release_time =
                        event.timestamp_us;
                }
            }
        }
        else if (pressed)
        {
            if (!long_started)
            {
                long_started = true;

                counter_increment();

                ESP_LOGI(
                    TAG,
                    "LONG start -> %d",
                    counter
                );
            }
            else
            {
                counter_increment();

                ESP_LOGI(
                    TAG,
                    "LONG rep -> %d",
                    counter
                );
            }
        }
        else if (waiting_second_click)
        {
            counter_increment();

            ESP_LOGI(
                TAG,
                "CLICK -> %d",
                counter
            );

            waiting_second_click = false;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Session 04 Exercise 1");

    display_init();

    button_init();

    xTaskCreate(
        gesture_task,
        "gesture_task",
        4096,
        NULL,
        5,
        NULL
    );
}
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"


/* ============================================================
 * LCD / SPI configuration
 * ============================================================ */

#define LCD_HOST        SPI2_HOST

#define PIN_SCK         GPIO_NUM_12
#define PIN_MOSI        GPIO_NUM_11
#define PIN_MISO        GPIO_NUM_13
#define PIN_CS          GPIO_NUM_10
#define PIN_RS          GPIO_NUM_9
#define PIN_RST         GPIO_NUM_14
#define PIN_BK_LIGHT    GPIO_NUM_2

#define LCD_H_RES       (480U)
#define LCD_V_RES       (320U)

#define LCD_CLK_HZ      (20 * 1000 * 1000)
#define CHUNK_PIXELS    (1024U)
#define CHUNK_BYTES     (CHUNK_PIXELS * 2U)


/* ============================================================
 * ST7796 commands
 * ============================================================ */

#define CMD_SWRESET     (0x01U)
#define CMD_SLPOUT      (0x11U)
#define CMD_INVON       (0x21U)
#define CMD_DISPON      (0x29U)
#define CMD_CASET       (0x2AU)
#define CMD_RASET       (0x2BU)
#define CMD_RAMWR       (0x2CU)
#define CMD_MADCTL      (0x36U)
#define CMD_COLMOD      (0x3AU)


/* ============================================================
 * MADCTL bits
 * ============================================================ */

#define MADCTL_MY       (0x80U)
#define MADCTL_MX       (0x40U)
#define MADCTL_MV       (0x20U)
#define MADCTL_BGR      (0x08U)

#define COLOUR_RED      (0x001FU)
#define COLOUR_GREEN    (0x07E0U)
#define COLOUR_BLUE     (0xF800U)
#define COLOUR_WHITE    (0xFFFFU)
#define COLOUR_BLACK    (0x0000U)


static const char *TAG = "ST7796";


static spi_device_handle_t lcd_spi;

static void lcd_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << PIN_RS) |
            (1ULL << PIN_RST) |
            (1ULL << PIN_BK_LIGHT),

        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RS, 1));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 1));
    ESP_ERROR_CHECK(gpio_set_level(PIN_BK_LIGHT, 1));
}

static void lcd_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CHUNK_BYTES,
    };

    ESP_ERROR_CHECK( spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_CLK_HZ,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK( spi_bus_add_device(LCD_HOST, &devcfg, &lcd_spi));
}

static void lcd_write_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    ESP_ERROR_CHECK( gpio_set_level(PIN_RS, 0));
    ESP_ERROR_CHECK( spi_device_polling_transmit( lcd_spi, &t));
}

static void lcd_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(gpio_set_level(PIN_RS, 1));
    ESP_ERROR_CHECK(spi_device_polling_transmit( lcd_spi, &t));
}

static void lcd_hardware_reset(void)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(120));
}


/* ============================================================
 * ST7796 initialization
 * ============================================================ */

static void lcd_init(void)
{
    lcd_hardware_reset();
    lcd_write_cmd(CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_write_cmd(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    uint8_t madctl = MADCTL_MV | MADCTL_BGR;
    lcd_write_cmd(CMD_MADCTL);
    lcd_write_data(&madctl, 1);

    uint8_t colmod = 0x55;
    lcd_write_cmd(CMD_COLMOD);
    lcd_write_data(&colmod, 1);

    lcd_write_cmd(CMD_INVON);

    lcd_write_cmd(CMD_DISPON);

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "ST7796 initialized");
}

static void lcd_set_window( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];
    lcd_write_cmd(CMD_CASET);

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)(x0 & 0xFF);

    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)(x1 & 0xFF);

    lcd_write_data(data, sizeof(data));
    lcd_write_cmd(CMD_RASET);

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)(y0 & 0xFF);

    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)(y1 & 0xFF);

    lcd_write_data(data, sizeof(data));

    lcd_write_cmd(CMD_RAMWR);
}

static void lcd_fill_rect( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour)
{
    if (w == 0 || h == 0) {
        return;
    }
    if (x >= LCD_H_RES || y >= LCD_V_RES) {
        return;
    }
    if ((x + w) > LCD_H_RES) {
        w = LCD_H_RES - x;
    }
    if ((y + h) > LCD_V_RES) {
        h = LCD_V_RES - y;
    }
    lcd_set_window(
        x,
        y,
        x + w - 1,
        y + h - 1);

    static uint8_t buffer[CHUNK_BYTES];
    uint8_t high_byte = (uint8_t)(colour >> 8);
    uint8_t low_byte = (uint8_t)(colour & 0xFF);

    for (uint32_t i = 0; i < CHUNK_PIXELS; i++) {
        buffer[i * 2] = high_byte;
        buffer[i * 2 + 1] = low_byte;
    }
    uint32_t total_pixels = (uint32_t)w * h;

    while (total_pixels > 0) {
        uint32_t pixels_this_chunk = total_pixels > CHUNK_PIXELS ? CHUNK_PIXELS : total_pixels;
        size_t bytes_this_chunk = pixels_this_chunk * 2;
        lcd_write_data( buffer, bytes_this_chunk);
        total_pixels -= pixels_this_chunk;
    }
}

static void lcd_draw_test_bars(void)
{
    const uint16_t bar_height = LCD_V_RES / 3;


    /* Top third - RED */
    lcd_fill_rect(
        0,
        0,
        LCD_H_RES,
        bar_height,
        COLOUR_RED
    );


    /* Middle third - GREEN */
    lcd_fill_rect(
        0,
        bar_height,
        LCD_H_RES,
        bar_height,
        COLOUR_GREEN
    );


    /* Bottom third - BLUE */
    lcd_fill_rect(
        0,
        bar_height * 2,
        LCD_H_RES,
        LCD_V_RES - (bar_height * 2),
        COLOUR_BLUE
    );
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ST7796 SPI display");
    lcd_gpio_init();
    lcd_spi_init();
    lcd_init();
    lcd_draw_test_bars();

    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {

        lcd_fill_rect(
            0,
            0,
            LCD_H_RES,
            LCD_V_RES,
            COLOUR_RED
        );

        vTaskDelay(pdMS_TO_TICKS(1000));


        lcd_fill_rect(
            0,
            0,
            LCD_H_RES,
            LCD_V_RES,
            COLOUR_GREEN
        );

        vTaskDelay(pdMS_TO_TICKS(1000));


        lcd_fill_rect(
            0,
            0,
            LCD_H_RES,
            LCD_V_RES,
            COLOUR_BLUE
        );

        vTaskDelay(pdMS_TO_TICKS(1000));


        lcd_fill_rect(
            0,
            0,
            LCD_H_RES,
            LCD_V_RES,
            COLOUR_WHITE
        );

        vTaskDelay(pdMS_TO_TICKS(1000));


        lcd_fill_rect(
            0,
            0,
            LCD_H_RES,
            LCD_V_RES,
            COLOUR_BLACK
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
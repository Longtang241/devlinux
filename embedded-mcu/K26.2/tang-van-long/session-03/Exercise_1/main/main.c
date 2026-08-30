/*
 * ============================================================
 * Assignment - Session 03 - Exercise 1
 *
 * ESP32-S3
 *
 * DISPLAY TYPE:
 *     COMMON CATHODE
 *
 * Common pins:
 *     COM1 -> GND
 *     COM2 -> GND
 *
 * Segment connections:
 *
 *     a -> GPIO4
 *     b -> GPIO5
 *     c -> GPIO6
 *     d -> GPIO7
 *     e -> GPIO15
 *     f -> GPIO16
 *     g -> GPIO17
 *     dp -> not used
 *
 * Button:
 *     GPIO14 -> button -> GND
 *
 * Button uses INTERNAL PULL-UP.
 
 * GPIO is controlled using registers directly.
 * ============================================================
 */


#include <stdint.h>
#include <stdbool.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/*
 * ============================================================
 * GPIO NUMBERS
 * ============================================================
 */

#define SEG_A_PIN               (4U)
#define SEG_B_PIN               (5U)
#define SEG_C_PIN               (6U)
#define SEG_D_PIN               (7U)
#define SEG_E_PIN               (15U)
#define SEG_F_PIN               (16U)
#define SEG_G_PIN               (17U)

#define BTN_PIN                 (14U)

#define NUM_SEGMENTS             (7U)


/*
 * ============================================================
 * TIMING CONSTANTS
 * ============================================================
 */

#define DEBOUNCE_MS             (25U)
#define DOUBLE_CLICK_MS         (350U)
#define LONG_PRESS_MS           (800U)

#define REPEAT_PERIOD_MS        (500U)

#define POLL_PERIOD_MS           (5U)


/*
 * ============================================================
 * BUTTON LOGIC
 * ============================================================
 */

#define BUTTON_RELEASED         (1U)
#define BUTTON_PRESSED          (0U)


/*
 * ============================================================
 * ESP32-S3 REGISTER BASE ADDRESSES
 * ============================================================
 */

#define GPIO_BASE_ADDR          (0x60004000U)
#define IO_MUX_BASE_ADDR        (0x60009000U)


/*
 * ============================================================
 * GPIO REGISTERS
 *
 * GPIO0 ~ GPIO31
 * ============================================================
 */

#define GPIO_OUT_REG_ADDR       (GPIO_BASE_ADDR + 0x0004U)
#define GPIO_OUT_W1TS_REG_ADDR  (GPIO_BASE_ADDR + 0x0008U)
#define GPIO_OUT_W1TC_REG_ADDR  (GPIO_BASE_ADDR + 0x000CU)

#define GPIO_ENABLE_REG_ADDR    (GPIO_BASE_ADDR + 0x0020U)
#define GPIO_ENABLE_W1TS_ADDR   (GPIO_BASE_ADDR + 0x0024U)
#define GPIO_ENABLE_W1TC_ADDR   (GPIO_BASE_ADDR + 0x0028U)

#define GPIO_IN_REG_ADDR        (GPIO_BASE_ADDR + 0x003CU)


#define GPIO_MATRIX_OUT_SEL_BASE_ADDR    \
        (GPIO_BASE_ADDR + 0x0554U)

#define GPIO_MATRIX_OUT_SEL_SHIFT        (0U)
#define GPIO_MATRIX_OUT_SEL_MASK         (0x1FFU)

#define GPIO_MATRIX_OUT_INV_BIT          (9U)
#define GPIO_MATRIX_OEN_SEL_BIT          (10U)
#define GPIO_MATRIX_OEN_INV_BIT          (11U)
#define GPIO_MATRIX_GPIO_OUT_SIGNAL     (0x80U)


/*
 * ============================================================
 * IO MUX REGISTER ADDRESSES
 * ============================================================
 *
 * ESP32-S3:
 *
 * GPIO4  -> +0x14
 * GPIO5  -> +0x18
 * GPIO6  -> +0x1C
 * GPIO7  -> +0x20
 * GPIO14 -> +0x3C
 * GPIO15 -> +0x40
 * GPIO16 -> +0x44
 * GPIO17 -> +0x48
 *
 * ============================================================
 */

#define IO_MUX_GPIO4_ADDR       (IO_MUX_BASE_ADDR + 0x14U)
#define IO_MUX_GPIO5_ADDR       (IO_MUX_BASE_ADDR + 0x18U)
#define IO_MUX_GPIO6_ADDR       (IO_MUX_BASE_ADDR + 0x1CU)
#define IO_MUX_GPIO7_ADDR       (IO_MUX_BASE_ADDR + 0x20U)

#define IO_MUX_GPIO14_ADDR      (IO_MUX_BASE_ADDR + 0x3CU)
#define IO_MUX_GPIO15_ADDR      (IO_MUX_BASE_ADDR + 0x40U)
#define IO_MUX_GPIO16_ADDR      (IO_MUX_BASE_ADDR + 0x44U)
#define IO_MUX_GPIO17_ADDR      (IO_MUX_BASE_ADDR + 0x48U)


/*
 * ============================================================
 * IO MUX BIT POSITIONS
 * ============================================================
 */

#define IO_MUX_PULLDOWN_BIT     (7U)
#define IO_MUX_PULLUP_BIT       (8U)
#define IO_MUX_INPUT_ENABLE_BIT (9U)

#define IO_MUX_MCU_SEL_SHIFT    (12U)
#define IO_MUX_MCU_SEL_MASK     (0x7U << IO_MUX_MCU_SEL_SHIFT)
#define IO_MUX_GPIO_FUNCTION    (1U)


/*
 * ============================================================
 * 7-SEGMENT TABLE
 *
 * Common Cathode:
 *
 *     1 -> segment ON
 *     0 -> segment OFF
 *
 * bit 0 -> a
 * bit 1 -> b
 * bit 2 -> c
 * bit 3 -> d
 * bit 4 -> e
 * bit 5 -> f
 * bit 6 -> g
 *
 * ============================================================
 */

static const uint8_t SEGMENT_MAP[10] =
{
    0x3FU,      /* 0 */
    0x06U,      /* 1 */
    0x5BU,      /* 2 */
    0x4FU,      /* 3 */
    0x66U,      /* 4 */
    0x6DU,      /* 5 */
    0x7DU,      /* 6 */
    0x07U,      /* 7 */
    0x7FU,      /* 8 */
    0x6FU       /* 9 */
};

static const uint32_t SEGMENT_PINS[NUM_SEGMENTS] =
{
    SEG_A_PIN, SEG_B_PIN, SEG_C_PIN, SEG_D_PIN,
    SEG_E_PIN, SEG_F_PIN, SEG_G_PIN
};

#define SEGMENT_GPIO_MASK       \
    ((1U << SEG_A_PIN) |       \
     (1U << SEG_B_PIN) |       \
     (1U << SEG_C_PIN) |       \
     (1U << SEG_D_PIN) |       \
     (1U << SEG_E_PIN) |       \
     (1U << SEG_F_PIN) |       \
     (1U << SEG_G_PIN))

static inline volatile uint32_t *reg_ptr(uint32_t address)
{
    return (volatile uint32_t *)address;
}

static uint32_t io_mux_addr_for_segment_pin(uint32_t pin)
{
    switch (pin)
    {
        case SEG_A_PIN: return IO_MUX_GPIO4_ADDR;
        case SEG_B_PIN: return IO_MUX_GPIO5_ADDR;
        case SEG_C_PIN: return IO_MUX_GPIO6_ADDR;
        case SEG_D_PIN: return IO_MUX_GPIO7_ADDR;
        case SEG_E_PIN: return IO_MUX_GPIO15_ADDR;
        case SEG_F_PIN: return IO_MUX_GPIO16_ADDR;
        case SEG_G_PIN: return IO_MUX_GPIO17_ADDR;
        default:        return 0U;
    }
}

static void iomux_config(
    uint32_t address,
    bool input_enable,
    bool pullup_enable
)
{
    volatile uint32_t *reg = reg_ptr(address);

    uint32_t value = *reg;

    value &= ~IO_MUX_MCU_SEL_MASK;

    value |=
        (IO_MUX_GPIO_FUNCTION << IO_MUX_MCU_SEL_SHIFT);

    if (input_enable)
    {
        value |= (1U << IO_MUX_INPUT_ENABLE_BIT);
    }
    else
    {
        value &= ~(1U << IO_MUX_INPUT_ENABLE_BIT);
    }

    if (pullup_enable)
    {
        value |= (1U << IO_MUX_PULLUP_BIT);
    }
    else
    {
        value &= ~(1U << IO_MUX_PULLUP_BIT);
    }

    value &= ~(1U << IO_MUX_PULLDOWN_BIT);

    *reg = value;
}

static void gpio_matrix_output_config(uint32_t gpio)
{
    uint32_t address =
        GPIO_MATRIX_OUT_SEL_BASE_ADDR +
        (gpio * 4U);

    volatile uint32_t *reg = reg_ptr(address);

    uint32_t value = *reg;

    value &= ~(
        GPIO_MATRIX_OUT_SEL_MASK |
        (1U << GPIO_MATRIX_OUT_INV_BIT) |
        (1U << GPIO_MATRIX_OEN_SEL_BIT) |
        (1U << GPIO_MATRIX_OEN_INV_BIT)
    );

    value |=
        (GPIO_MATRIX_GPIO_OUT_SIGNAL
         << GPIO_MATRIX_OUT_SEL_SHIFT);

    value |=
        (1U << GPIO_MATRIX_OEN_SEL_BIT);

    value &= ~(1U << GPIO_MATRIX_OUT_INV_BIT);

    value &= ~(1U << GPIO_MATRIX_OEN_INV_BIT);

    *reg = value;
}

static void display_init(void)
{
    for (uint32_t i = 0U; i < NUM_SEGMENTS; i++)
    {
        uint32_t pin = SEGMENT_PINS[i];

        iomux_config(io_mux_addr_for_segment_pin(pin), false, false);
        gpio_matrix_output_config(pin);
    }

    volatile uint32_t *enable_reg =
        reg_ptr(GPIO_ENABLE_W1TS_ADDR);

    *enable_reg = SEGMENT_GPIO_MASK;

    volatile uint32_t *clear_reg =
        reg_ptr(GPIO_OUT_W1TC_REG_ADDR);

    *clear_reg = SEGMENT_GPIO_MASK;
}

static void button_init(void)
{
    iomux_config(
        IO_MUX_GPIO14_ADDR,
        true,
        true
    );
}

static uint32_t button_read(void)
{
    volatile uint32_t *reg =
        reg_ptr(GPIO_IN_REG_ADDR);

    uint32_t value = *reg;

    return (value >> BTN_PIN) & 1U;
}

static void display_digit(uint8_t digit)
{
    uint8_t pattern = SEGMENT_MAP[digit];
    uint32_t set_mask = 0U;

    for (uint32_t i = 0U; i < NUM_SEGMENTS; i++)
    {
        if (pattern & (1U << i))
        {
            set_mask |= (1U << SEGMENT_PINS[i]);
        }
    }

    volatile uint32_t *clear_reg = reg_ptr(GPIO_OUT_W1TC_REG_ADDR);
    *clear_reg = SEGMENT_GPIO_MASK;
    volatile uint32_t *set_reg = reg_ptr(GPIO_OUT_W1TS_REG_ADDR);
    *set_reg = set_mask;
}

static void counter_increment(uint8_t *counter)
{
    (*counter)++;
    if (*counter > 9U)
    {
        *counter = 0U;
    }
    display_digit(*counter);
}

static void counter_decrement(uint8_t *counter)
{
    if (*counter == 0U)
    {
        *counter = 9U;
    }
    else
    {
        (*counter)--;
    }
    display_digit(*counter);
}

static void poll_delay(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

void app_main(void)
{
    uint8_t counter = 0U;

    uint32_t stable_state = BUTTON_RELEASED;

    uint32_t last_raw_state = BUTTON_RELEASED;

    int64_t raw_change_time = 0;

    int64_t press_time = 0;

    int64_t next_repeat_time = 0;

    bool pending_click = false;

    int64_t pending_click_time = 0;

    bool long_press_active = false;

    display_init();

    button_init();

    display_digit(counter);

    stable_state = button_read();

    last_raw_state = stable_state;

    raw_change_time = esp_timer_get_time();

    while (true)
    {
        int64_t now = esp_timer_get_time();

        uint32_t raw_state = button_read();

        if (raw_state != last_raw_state)
        {
            last_raw_state = raw_state;

            raw_change_time = now;
        }

        if ((raw_state != stable_state) &&
            ((now - raw_change_time) >=
             ((int64_t)DEBOUNCE_MS * 1000LL)))
        {
            uint32_t previous_state = stable_state;

            stable_state = raw_state;

            if ((previous_state == BUTTON_RELEASED) &&
                (stable_state == BUTTON_PRESSED))
            {
                press_time = now;
                long_press_active = false;
                next_repeat_time =
                    press_time +
                    ((int64_t)LONG_PRESS_MS * 1000LL);
            }

            if ((previous_state == BUTTON_PRESSED) &&
                (stable_state == BUTTON_RELEASED))
            {
                int64_t held_time =
                    now - press_time;

                if (!long_press_active &&
                    held_time <
                    ((int64_t)LONG_PRESS_MS * 1000LL))
                {
                    if (pending_click)
                    {
                        counter_decrement(&counter);

                        pending_click = false;
                    }
                    else
                    {
                        pending_click = true;

                        pending_click_time = now;
                    }
                }

                long_press_active = false;
            }
        }

        if (stable_state == BUTTON_PRESSED)
        {
            if (!long_press_active &&
                ((now - press_time) >=
                 ((int64_t)LONG_PRESS_MS * 1000LL)))
            {
                counter_increment(&counter);

                long_press_active = true;
                next_repeat_time =
                    now +
                    ((int64_t)REPEAT_PERIOD_MS * 1000LL);
            }
            if (long_press_active &&
                (now >= next_repeat_time))
            {
                counter_increment(&counter);


                next_repeat_time +=
                    ((int64_t)REPEAT_PERIOD_MS * 1000LL);
            }
        }

        if (pending_click &&
            ((now - pending_click_time) >=
             ((int64_t)DOUBLE_CLICK_MS * 1000LL)))
        {
            counter_increment(&counter);

            pending_click = false;
        }

        poll_delay(POLL_PERIOD_MS);
    }
}
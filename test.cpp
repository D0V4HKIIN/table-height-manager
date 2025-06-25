#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
// #include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h" // for the interrupts

#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

#define TRIGGER_PIN 2
#define ECHO_PIN 3
#define TIMEOUT_US 50000 // 50 ms timeout

#define I2C_PORT i2c_default
#define I2C_PIN_SDA PICO_DEFAULT_I2C_SDA_PIN
#define I2C_PIN_SCL PICO_DEFAULT_I2C_SCL_PIN

#define PRESET1_PIN 18
#define PRESET2_PIN 19
#define PRESET3_PIN 20
#define SAVE_PIN 21

double preset1 = 750.;
double preset2 = 1145.;
double preset3 = 700.;

#define FLASH_TARGET_OFFSET ((1024 + 512) * 1024) // choosing to start at 1024k + 512k

// This function will be called when it's safe to call flash_range_erase
static void call_flash_range_erase(void *param)
{
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

// This function will be called when it's safe to call flash_range_program
static void call_flash_range_program(void *param)
{
    uint32_t offset = ((uintptr_t *)param)[0];
    const uint8_t *data = (const uint8_t *)((uintptr_t *)param)[1];
    flash_range_program(offset, data, FLASH_PAGE_SIZE);
}

void save_presets()
{
    fprintf(stderr, "a\n");
    double data[FLASH_PAGE_SIZE / sizeof(double)] = {preset1, preset2, preset3};
    // uint8_t data[FLASH_PAGE_SIZE];
    fprintf(stderr, "z %d\n", sizeof(data));
    uint8_t *as_bytes = (uint8_t *)data;
    unsigned int size = sizeof(data);
    fprintf(stderr, "b %d\n", size);

    int writeSize = (size / FLASH_PAGE_SIZE) + 1;                              // how many flash pages we're gonna need to write
    int sectorCount = ((writeSize * FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE) + 1; // how many flash sectors we're gonna need to erase

    fprintf(stderr, "c %d, %d\n", writeSize, sectorCount);
    uint32_t interrupts = save_and_disable_interrupts();
    fprintf(stderr, "d\n");
    int rc = flash_safe_execute(call_flash_range_erase, (void *)FLASH_TARGET_OFFSET, UINT32_MAX);
    fprintf(stderr, "x %d, ok:%d\n", rc, PICO_OK);
    // flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE * sectorCount);
    void *params[] = {(void *)FLASH_TARGET_OFFSET, (void *)data};
    rc = flash_safe_execute(call_flash_range_program, params, UINT32_MAX);
    // flash_range_program(FLASH_TARGET_OFFSET, as_bytes, FLASH_PAGE_SIZE * writeSize);
    fprintf(stderr, "f %d, ok:%d\n", rc, PICO_OK);
    restore_interrupts(interrupts);
    fprintf(stderr, "g\n");
}

void load_presets()
{
    double data[3];
    uint8_t *as_bytes = (uint8_t *)&data;
    unsigned int size = sizeof(data);

    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(as_bytes, flash_target_contents, size);

    preset1 = data[0];
    preset2 = data[1];
    preset3 = data[2];
}

void triangle_up(pico_ssd1306::SSD1306 &display)
{
    for (int y = 0; y < 32; y++)
    {
        for (int x = 15 - y / 2; x < 15 + y / 2; x++)
        {
            display.setPixel(x + 95, y);
        }
    }
}

void triangle_down(pico_ssd1306::SSD1306 &display)
{
    for (int y = 0; y < 32; y++)
    {
        for (int x = y / 2; x < 32 - y / 2; x++)
        {
            display.setPixel(x + 95, y);
        }
    }
}

unsigned int num_smooth = 100;
void read_smooth()
{
    while (1)
    {
        int in = -1;
        scanf("%d", &in);
        num_smooth = in;
        printf("num_smooth is %d\n", in);
    }
}

double measure_distance()
{
    // Trigger the ultrasonic pulse
    gpio_put(TRIGGER_PIN, false);
    sleep_us(2);
    gpio_put(TRIGGER_PIN, true);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, false);

    // Wait for echo pin to go HIGH (start timing)
    absolute_time_t start_time = get_absolute_time();
    absolute_time_t echo_start, echo_end;

    while (!gpio_get(ECHO_PIN))
    {
        if (absolute_time_diff_us(start_time, get_absolute_time()) > TIMEOUT_US)
        {
            printf("Echo did not start (timeout)\n");
            return measure_distance();
        }
    }

    echo_start = get_absolute_time();

    // Wait for echo pin to go LOW (end timing)
    while (gpio_get(ECHO_PIN))
    {
        if (absolute_time_diff_us(echo_start, get_absolute_time()) > TIMEOUT_US)
        {
            printf("Echo did not end (timeout)\n");
            return measure_distance();
        }
    }

    echo_end = get_absolute_time();
    int64_t pulse_duration = absolute_time_diff_us(echo_start, echo_end);
    float distance_mm = pulse_duration / 5.80f;
    return distance_mm;
}

int main()
{
    // init serial
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // buttons
    gpio_init(PRESET1_PIN);
    gpio_pull_down(PRESET1_PIN);
    gpio_init(PRESET2_PIN);
    gpio_pull_down(PRESET2_PIN);
    gpio_init(PRESET3_PIN);
    gpio_pull_down(PRESET3_PIN);
    gpio_init(SAVE_PIN);
    gpio_pull_down(SAVE_PIN);

    // ultrasonic distance sensor
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, false);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    // screen
    i2c_init(I2C_PORT, 1000000); // Use i2c port with baud rate of 1Mhz
    // Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);
    // Create a new display object
    pico_ssd1306::SSD1306 display = pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH32); // 0x3D for 128x64

    // load presets from flash
    load_presets();

    // parallel reading of the serial port
    // multicore_launch_core1(read_smooth);

    double d = preset1;
    double goal = preset1;
    while (true)
    {
        display.clear();

        double m = measure_distance();
        if (std::abs(d - m) < 10.)
        {
            d = m / num_smooth + d * (num_smooth - 1) / num_smooth;
        }
        else // speed things up if there is a large difference
        {
            d = m;
            // drawText(&display, font_16x32, ":0", 95, 0);
        }

        // check preset buttons
        if (gpio_get(PRESET1_PIN))
        {
            goal = preset1;
        }
        else if (gpio_get(PRESET2_PIN))
        {
            goal = preset2;
        }
        else if (gpio_get(PRESET3_PIN))
        {
            goal = preset3;
        }

        // move table
        if (d - goal > 2.)
        {
            triangle_down(display);
        }

        if (d - goal < -2.)
        {
            triangle_up(display);
        }

        // save button
        if (gpio_get(SAVE_PIN))
        {
            fprintf(stderr, "saving\n");
            printf("saving2\n");
            drawText(&display, font_12x16, "Which", 0, 0);
            drawText(&display, font_12x16, "preset?", 0, 16);
            display.sendBuffer();
            while (true)
            {
                if (gpio_get(PRESET1_PIN))
                {
                    preset1 = d;
                    break;
                }
                else if (gpio_get(PRESET2_PIN))
                {
                    preset2 = d;
                    break;
                }
                else if (gpio_get(PRESET3_PIN))
                {
                    preset3 = d;
                    break;
                }
            }
            save_presets();
        }

        char top_str[20];
        char bot_str[20];
        sprintf(top_str, "%.0f mm", d);
        sprintf(bot_str, "%.0f mm", goal);
        drawText(&display, font_12x16, top_str, 0, 0);
        drawText(&display, font_12x16, bot_str, 0, 16);
        display.sendBuffer(); // Send buffer to device and show on screen
    }
}

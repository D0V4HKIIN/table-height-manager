#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

#define TRIGGER_PIN 2
#define ECHO_PIN 3
#define TIMEOUT_US 50000 // 50 ms timeout

#define I2C_PORT i2c_default
#define I2C_PIN_SDA PICO_DEFAULT_I2C_SDA_PIN
#define I2C_PIN_SCL PICO_DEFAULT_I2C_SCL_PIN

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
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, false);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    i2c_init(I2C_PORT, 1000000); // Use i2c port with baud rate of 1Mhz
    // // Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    // // Create a new display object
    pico_ssd1306::SSD1306 display = pico_ssd1306::SSD1306(I2C_PORT, 0x3C, pico_ssd1306::Size::W128xH32); // 0x3D for 128x64

    multicore_launch_core1(read_smooth);

    double d = 150.;
    while (true)
    {
        display.clear();

        // for (int i = 0; i < num_smooth; i++)
        // {
        double m = measure_distance();
        if (std::abs(d - m) < 10.)
        {
            d = m / num_smooth + d * (num_smooth - 1) / num_smooth;
        }
        else // speed things up if there is a large difference
        {
            d = m;
            drawText(&display, font_16x32, ":0", 95, 0);
        }
        // }

        char str[20];
        sprintf(str, "%.0f mm", d);
        drawText(&display, font_16x32, str, 0, 0);
        display.sendBuffer(); // Send buffer to device and show on screen
    }
}

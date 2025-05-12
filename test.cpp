#include <stdio.h>
#include "pico/stdlib.h"

#define TRIGGER_PIN 2
#define ECHO_PIN 3
#define TIMEOUT_US 50000 // 50 ms timeout

#define num_smooth 100

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
    float distance_cm = pulse_duration / 58.0f;
    return distance_cm;
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

    while (true)
    {
        double d = 0.;

        for (int i = 0; i < num_smooth; i++)
        {
            d += measure_distance() / num_smooth;
        }

        printf("Distance: %.2f cm\n", d);
    }
}

#include <stdio.h>
#include <iostream>
#include <ctime>
#include <chrono>
#include "pico/stdlib.h"

// enum {
//     measured
//     waiting,
// }

// int state = 0;

#define trigger 2
#define echo 3

int main()
{
    stdio_init_all();

    // init pins
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(trigger);
    gpio_set_dir(trigger, GPIO_OUT);
    gpio_put(trigger, false);

    gpio_init(echo);
    gpio_set_dir(echo, GPIO_IN);

    while (true)
    {

        gpio_put(trigger, false);
        sleep_us(10);
        gpio_put(trigger, true);
        sleep_us(10);
        gpio_put(trigger, false);

        auto start = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        while (!gpio_get(echo))
        {
            now = std::chrono::steady_clock::now();
            std::cout << std::chrono::duration<double, std::milli>(now - start).count() << std::endl;
        }

        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
}

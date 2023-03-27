#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define HIGH 1
#define LOW 0

#define LEDPin GPIO_NUM_13
#define delayMS 1000

void app_main(void)
{
	gpio_reset_pin(LEDPin);
	gpio_set_direction(LEDPin, GPIO_MODE_OUTPUT);

	while (true) {
		gpio_set_level(LEDPin, HIGH);
		vTaskDelay(delayMS / portTICK_PERIOD_MS);
		gpio_set_level(LEDPin, LOW);
		vTaskDelay(delayMS / portTICK_PERIOD_MS);
	}	
}

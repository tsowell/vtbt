#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "leds.h"

LOG_MODULE_REGISTER(leds, CONFIG_LOG_DEFAULT_LEVEL);

static const struct gpio_dt_spec leds[NUM_LEDS] = {
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led0), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led1), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led2), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led3), gpios, {0}),
};

int
leds_init(void)
{
	int ret;
	for (int i = 0; i < NUM_LEDS; i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			LOG_ERR("led%d pin GPIO port is not ready.", i);
			return -1;
		}

		ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret != 0) {
			LOG_ERR("Configuring led%d GPIO pin failed: %d",
			        i, ret);
			return -1;
		}
	}

	return 0;
}

void
leds_set(int which, int value)
{
	gpio_pin_set_dt(&leds[which], value);
}

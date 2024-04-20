#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "beeper.h"

LOG_MODULE_REGISTER(beeper, CONFIG_LOG_DEFAULT_LEVEL);

K_MUTEX_DEFINE(beeper_mutex);

static const struct pwm_dt_spec pwm_beeper0 =
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_beeper0));

static int keyclick_volume = -1;
static int bell_volume = -1;

int
beeper_init(void)
{
	int ret = pwm_is_ready_dt(&pwm_beeper0);
        if (!ret) {
                printk("Error: Beeper PWM device %s is not ready\n",
                       pwm_beeper0.dev->name);
        }
	return ret;
}

void
beeper_set_bell_volume(int volume)
{
	bell_volume = volume;
}

void
beeper_set_keyclick_volume(int volume)
{
	keyclick_volume = volume;
}

static void
beeper_on(int volume)
{
	const uint32_t pulse = (pwm_beeper0.period / 2U) * (8 - volume) / 8;
	k_mutex_lock(&beeper_mutex, K_FOREVER);
	int ret = pwm_set_dt(&pwm_beeper0, pwm_beeper0.period, pulse);
	k_mutex_unlock(&beeper_mutex);
	if (ret) {
		LOG_ERR("Error %d: failed to set pulse width", ret);
	}
}

static void
beeper_off(void)
{
	k_mutex_lock(&beeper_mutex, K_FOREVER);
	int ret = pwm_set_dt(&pwm_beeper0, pwm_beeper0.period, 0);
	k_mutex_unlock(&beeper_mutex);
	if (ret) {
		LOG_ERR("Error %d: failed to set pulse width", ret);
	}
}

void beeper_off_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	beeper_off();
}

K_WORK_DEFINE(beeper_off_work, beeper_off_work_handler);

void beeper_off_timer_handler(struct k_timer *dummy)
{
	ARG_UNUSED(dummy);

	k_work_submit(&beeper_off_work);
}

K_TIMER_DEFINE(beeper_off_timer, beeper_off_timer_handler, NULL);

void
beeper_sound_keyclick(void)
{
	if (keyclick_volume < 0) {
		return;
	}

	beeper_on(keyclick_volume);

	k_timer_start(&beeper_off_timer, K_MSEC(2), K_FOREVER);
}

void
beeper_sound_bell(void)
{
	if (bell_volume < 0) {
		return;
	}

	beeper_on(bell_volume);

	k_timer_start(&beeper_off_timer, K_MSEC(125), K_FOREVER);
}

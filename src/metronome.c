#include <zephyr/kernel.h>

#include "metronome.h"

#include "vtbt.h"
#include "lk201.h"
#include "uart.h"
#include "beeper.h"

static bool auto_repeat_enabled = true;

static int repeating_keycode = 0;
/* The k_uptime_get() timestamp when the next metronome should be sent. */
static int repeating_next = 0;
/* Set when a keycode has been transmitted while handling another event, so the
 * keycode of a repeating key needs to be resent before resuming metronomes. */
static bool resend = false;

void
metronome_resend(void)
{
	resend = true;
}

void
metronome_auto_repeat_enable(void)
{
	auto_repeat_enabled = true;
}

void
metronome_auto_repeat_disable(void)
{
	auto_repeat_enabled = false;
}

void
metronome_event(const sys_dlist_t *keys_down, const struct event *event)
{
	ARG_UNUSED(event);

	/* Get the most auto-repeat-capable down key. */
	struct key_down *repeating = NULL;
	struct division *division = NULL;
	struct key_down *cn;
	SYS_DLIST_FOR_EACH_CONTAINER((sys_dlist_t *)keys_down, cn, node) {
		division = lk201_division_get_from_keycode(cn->keycode);
		if (division == NULL) {
			continue;
		} else if (cn->inhibit_auto_repeat) {
			continue;
		} else if (division->mode == MODE_AUTO_REPEAT) {
			repeating = cn;
			break;
		}
	}

	if (repeating == NULL) {
		repeating_keycode = 0;
		resend = false;
		return;
	}

	int64_t now = k_uptime_get();

	if (repeating_keycode != repeating->keycode) {
		/* We're already repeating a different key. */
		struct repeat_buffer *repeat_buffer = lk201_repeat_buffer_get(
			division->buffer);
		int timeout = repeat_buffer->timeout;
		if ((now - repeating->time) > timeout) {
			int interval = repeat_buffer->interval;

			if (repeating->repeating && repeating_keycode != 0) {
				if (auto_repeat_enabled) {
					int sent = uart_write_byte(
						repeating->keycode);
					if (sent > 0) {
						beeper_sound_keyclick();
					}
				}
			} else {
				if (auto_repeat_enabled) {
					int sent = uart_write_byte(
						SPECIAL_METRONOME);
					if (sent > 0) {
						beeper_sound_keyclick();
					}
				}
			}
			repeating_keycode = repeating->keycode;
			repeating_next = now + interval;
			resend = false;
			repeating->repeating = true;
		}
		return;
	}

	if ((repeating_next - now) < 0) {
		struct repeat_buffer *repeat_buffer = lk201_repeat_buffer_get(
			division->buffer);
		int interval = repeat_buffer->interval;
		repeating_next = now + interval;
		if (resend) {
			resend = false;
			if (auto_repeat_enabled) {
				int sent = uart_write_byte(repeating->keycode);
				if (sent > 0) {
					beeper_sound_keyclick();
				}
			}
		} else {
			if (auto_repeat_enabled) {
				int sent = uart_write_byte(SPECIAL_METRONOME);
				if (sent > 0) {
					beeper_sound_keyclick();
				}
			}
		}
	}
}

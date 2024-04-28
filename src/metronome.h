#ifndef METRONOME_H
#define METRONOME_H

#include <zephyr/kernel.h>

#include "vtbt.h"

/* Signals the auto-repeater that a keycode has been transmitted and that the
 * current auto-repeating keycode needs to be resent before resuming metronome
 * codes. */
void metronome_resend(void);

void metronome_auto_repeat_enable(void);
void metronome_auto_repeat_disable(void);
void metronome_event(const sys_dlist_t *, const struct event *);

#endif /* METRONOME_H */

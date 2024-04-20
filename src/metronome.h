#ifndef METRONOME_H
#define METRONOME_H

#include <zephyr/kernel.h>

#include "vtbt.h"

void metronome_resend(void);
void metronome_auto_repeat_enable(void);
void metronome_auto_repeat_disable(void);
void metronome_event(const sys_dlist_t *, const struct event *);

#endif /* METRONOME_H */

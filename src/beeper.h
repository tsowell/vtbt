#ifndef BEEPER_H
#define BEEPER_H

/* Volumes are 0 (highest) to 7 (lowest) or -1 (disabled) */

int beeper_init(void);
void beeper_set_bell_volume(int volume);
void beeper_set_keyclick_volume(int volume);
void beeper_sound_bell(void);
void beeper_sound_keyclick(void);

#endif /* BEEPER_H */

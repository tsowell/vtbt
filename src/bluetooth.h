#ifndef BLUETOOTH_H
#define BLUETOOTH_H

int bluetooth_listen(void (*callback)(const uint8_t *report));

#endif /* BLUETOOTH_H */

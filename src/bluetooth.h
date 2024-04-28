#ifndef BLUETOOTH_H
#define BLUETOOTH_H

/* Scan and connect to a Bluetooth keyboard, sending keyboard HID reports to
 * the callback function. */
int bluetooth_listen(void (*callback)(const uint8_t *report));

#endif /* BLUETOOTH_H */

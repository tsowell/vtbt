# Basic emulation of a DEC VT for testing
import binascii
import serial
import time

port = '/dev/esp32'
baud_rate = 4800 
sequence = b'\x01\x00\x00\x00'
sequence_length = len(sequence)

# Open the serial port
with serial.Serial(port, baud_rate) as ser:
    print(f'Waiting for power-on test results')

    received_data = b''
    while True:
        byte = ser.read(size=1)
        if byte:
            received_data += byte 
            if len(received_data) > len(sequence):
                received_data = received_data[-len(sequence):]
            
            if sequence in received_data:
                break

    print('Got power-on test result:', binascii.hexlify(received_data))

    commands = [
        b'\x55',
        b'\x85',
        b'\x85',
        b'\xCB',
        b'\x80',
        b'\xAB', # request keyboard id
        b'\x11\x8F',
        b'\x13\x82',
        b'\x11\x8F',
        b'\x13\x83',
        b'\x11\x8F',
        b'\x13\x84',
        b'\x11\x8F',
        b'\x13\x85',
        b'\x11\x8F',
        b'\x13\x84',
        b'\x11\x8F',
        b'\x13\x85',
        b'\x11\x8F',
        b'\x13\x84',
        b'\x11\x8F',
        b'\x13\x85',
        b'\x11\x8F',
        b'\x13\x88',
        b'\x11\x8F',
        b'\x13\x89',
        b'\x11\x8F',
        b'\x13\x90',
        b'\xFD',
        b'\x0A\x80',
        b'\x12\x81',
        b'\x1A\x80',
        b'\x3A\x81',
        b'\x42\x81',
        b'\x4A\x82',
        b'\x5A\x82',
        b'\x62\x82',
        b'\x6A\x82',
        b'\x72\x82',
        b'\xA2',
        b'\x78\x64\x9E',
        b'\x7A\x64\x9E',
        b'\x7C\x64\x9E',
        b'\xE3',
        b'\x11\x8F',
        b'\x13\x80',
        b'\xA7',
        b'\x11\x8E',
        b'\x13\x81',
        b'\x11\x8F',
        b'\x13\x80',
        b'\xBB', # Enable ctrl keyclick
    ]
    for command in commands:
        ser.write(command)
        time.sleep(0.1)

    while True:
        byte = ser.read(size=1)
        print(binascii.hexlify(byte))
        if byte == b'\xf0': # p
            ser.write(b'\x89') # lock keyboard
            time.sleep(5)
            ser.write(b'\x8b') # unlock keyboard
        elif byte == b'\xc2': # a
            ser.write(b'\x9f') # sound keyclick
        elif byte == b'\xb4': # metronome
            ser.write(b'\x9f') # sound keyclick
        elif byte == b'\xce': # c
            ser.write(b'\xc1') # temporary auto-repeat inhibit
        elif byte == b'\xf7': # \
            ser.write(b'\xe1') # disable auto-repeat
        elif byte == b'\xbf': # `
            ser.write(b'\xe3') # enable auto-repeat


#ifndef _SERIAL_EMU_H
#define _SERIAL_EMU_H

#include <pthread.h>
#include "Arduino.h"
#include "Stream.h"

#define EMU_BUFFER_LEN 10000

class SerialBuffer {
public:
    SerialBuffer();
    size_t write(const char* buffer, size_t size, bool overwrite = true);
    int read();
    int peek();
    size_t available();
private:
    void _inc_read_pos();
    void _inc_write_pos();

    pthread_mutex_t _mutex;
    char _buffer[EMU_BUFFER_LEN];
    volatile size_t _read_pos;
    volatile size_t _write_pos;
};

class SerialEmu : public Stream {
public:
    SerialEmu();

    virtual int available();
    virtual int read();
    virtual int peek();
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

    virtual int emu_available();
    virtual int emu_read();
    virtual int emu_peek();
    virtual size_t emu_write(uint8_t);
    virtual size_t emu_write(const uint8_t *buffer, size_t size);
    virtual size_t emu_write(const char *buffer);

protected:
    size_t _append_rx(const char *buffer, size_t size);
    size_t _append_tx(const char *buffer, size_t size);

    SerialBuffer _rx_buffer;
    SerialBuffer _tx_buffer;
};

#endif

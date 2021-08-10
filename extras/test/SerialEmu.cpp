
#include <pthread.h>
#include "SerialEmu.h"

SerialBuffer::SerialBuffer()
{
    memset(_buffer, 0, sizeof(_buffer));
    _read_pos = 0;
    _write_pos = 0;
}

void SerialBuffer::_inc_read_pos() {
    pthread_mutex_lock(&_mutex);
    _read_pos++;
    if (_read_pos >= sizeof(_buffer)) {
        printf("reset r ");
        _read_pos = 0;
    }
    pthread_mutex_unlock(&_mutex);
}

void SerialBuffer::_inc_write_pos() {
    pthread_mutex_lock(&_mutex);
    _write_pos++;
    if (_write_pos >= sizeof(_buffer)) {
        printf("reset w ");
        _write_pos = 0;
    }
    pthread_mutex_unlock(&_mutex);
}

size_t SerialBuffer::available()
{
    size_t ret;
    pthread_mutex_lock(&_mutex);
    if (_read_pos <= _write_pos) {
        ret = _write_pos - _read_pos;
    } else {
        ret = sizeof(_buffer) - _read_pos + _write_pos;
    }
    pthread_mutex_unlock(&_mutex);
    return ret;
}

size_t SerialBuffer::write(const char *buffer, size_t size, bool overwrite)
{
    size_t written = 0;
    while (size > 0) {
        _buffer[_write_pos] = *buffer;
        _inc_write_pos();
        written++;
        buffer++;
        size--;
    }
    return written;
}

int SerialBuffer::read()
{
    int c = -1;
    if (available()) {
        c = _buffer[_read_pos];
        _inc_read_pos();
    }
    return c;
}

int SerialBuffer::peek()
{
    int c = -1;
    if (available()) {
        c = _buffer[_read_pos];
    }
    return c;
}

SerialEmu::SerialEmu()
{
}

size_t SerialEmu::_append_tx(const char *buffer, size_t size)
{
    return _tx_buffer.write(buffer, size);
}

size_t SerialEmu::_append_rx(const char *buffer, size_t size)
{
    return _rx_buffer.write(buffer, size);
}

int SerialEmu::available()
{
    return _rx_buffer.available();
}

int SerialEmu::read()
{
    return _rx_buffer.read();
}

int SerialEmu::peek()
{
    return _rx_buffer.peek();
}

size_t SerialEmu::write(const uint8_t *buffer, size_t size)
{
    return _append_tx((const char*) buffer, size);
}

size_t SerialEmu::write(uint8_t ch)
{
    char buf[10];
    snprintf(buf, sizeof(buf), "%c", ch);
    return _append_tx(buf, strlen(buf));
}

int SerialEmu::emu_available()
{
    return _tx_buffer.available();
}

int SerialEmu::emu_read()
{
    return _tx_buffer.read();
}

int SerialEmu::emu_peek()
{
    return _tx_buffer.peek();
}

size_t SerialEmu::emu_write(uint8_t ch)
{
    char buf[10];
    snprintf(buf, sizeof(buf), "%c", ch);
    return _append_rx(buf, strlen(buf));
}

size_t SerialEmu::emu_write(const uint8_t *buffer, size_t size)
{
    return _append_rx((const char*) buffer, size);
}

size_t SerialEmu::emu_write(const char *buffer)
{
    return _append_rx(buffer, strlen(buffer));
}

// to keep the linker happy..
size_t Print::write(const uint8_t *buffer, size_t size)
{
    printf("Print::write shouldn't be called\n");
    return size;
}

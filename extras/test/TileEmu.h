
#ifndef _TILE_EMU_H
#define _TILE_EMU_H

#include "SerialEmu.h"

typedef enum {
    ERR_NONE = 0,
    ERR_TIMEOUT,
    ERR_CRC
} emu_err_t;

typedef struct {
    const char* expected;
    const char* response;
} emu_sequence_t;

class TileEmu {
public:
    TileEmu(SerialEmu &serial);
    static void *run(void *context) {
        return ((TileEmu*)context)->_run();
    }   
    void stop();
    void flush();
    void setVerbose(bool verbose);
    void setSequence(emu_sequence_t *sequence);
    void setNextError(emu_err_t err);

private:
    SerialEmu &_serial;
    volatile bool _exit;
    uint8_t _step;
    emu_sequence_t *_sequence;
    bool _verbose;

    void *_run();
    void _process_line(const char *line);
    uint8_t _nmeaChecksum(const char *msg, size_t len);
};

#endif

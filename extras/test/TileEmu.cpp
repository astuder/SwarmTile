
#include "TileEmu.h"

TileEmu::TileEmu(SerialEmu &serial) : _serial(serial)
{
    _exit = false;
    _step = 0;
    _sequence = 0;
    _verbose = false;
}

void TileEmu::flush()
{
    while (_serial.emu_available()) {
        _serial.emu_read();
    }
}

void TileEmu::setVerbose(bool verbose)
{
    _verbose = verbose;
}

void TileEmu::setSequence(emu_sequence_t *sequence)
{
    _step = 0;
    _sequence = sequence;
}

void TileEmu::stop()
{
    _exit = true;
}

void *TileEmu::_run()
{    
    // thread main loop
    if (_verbose) {
        printf("\n");
    }

    _step = 0;

    char ch;
    char line[1000];
    size_t i = 0;
    _exit = false;
    while (_exit == false) {
        if (_serial.emu_available()) {
            ch = _serial.emu_read();
            line[i++] = ch;
            if (i >= sizeof(line)) {
                // overflow of line buffer
                break;
            }
            if (ch == '\n') {
                line[i] = 0;
                _process_line(line);
                _step++;
                if (_sequence[_step].expected == 0) {
                    // last step in expected sequence
                    break;
                } else {
                    // process next command
                    i = 0;
                }
            }
        }
    }

    return 0;
}

void TileEmu::_process_line(const char* line)
{
    char cs_str[] = "*xx\n";

    if (_verbose) {
        printf(">> ");
        printf(line);
    }
    if (_sequence[_step].expected) {
        bool found = false;
        if (_sequence[_step].expected[strlen(_sequence[_step].expected)-1] == '\n') {
            // compare expected command as is
            if (strcmp(line, _sequence[_step].expected)==0) {
                found = true;
            }
        } else {
            // compare expected command plus auto-generated checksum
            snprintf(cs_str, sizeof(cs_str), "*%02x\n", _nmeaChecksum(_sequence[_step].expected, strlen(_sequence[_step].expected)));
            if (strncmp(line, _sequence[_step].expected, strlen(_sequence[_step].expected)) == 0 &&
                strcmp(line + strlen(_sequence[_step].expected), cs_str) == 0) {
                found = true;
            }
        }

        if (found == true) {
            if (_sequence[_step].response) {
                _serial.emu_write(_sequence[_step].response);
                if (_verbose) {
                    printf("<< %s", _sequence[_step].response);
                }
                if (_sequence[_step].response[strlen(_sequence[_step].response)-1] != '\n') {
                    // append checksum and newline to response
                    snprintf(cs_str, sizeof(cs_str), "*%02x\n", _nmeaChecksum(_sequence[_step].response, strlen(_sequence[_step].response)));
                    _serial.emu_write(cs_str);
                    if (_verbose) {
                        printf(cs_str);
                    }
                }
            }
        }
    }
}

uint8_t TileEmu::_nmeaChecksum(const char *msg, size_t len)
{
    size_t i = 0;
    uint8_t cs;
    if (msg[0] == '$') {
        // skip $
        i++;
    }
    for (cs = 0; (i < len) && msg[i]; i++) {
        cs ^= ((uint8_t) msg[i]);
    }
    return cs;
}

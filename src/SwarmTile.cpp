
#include "SwarmTile.h"
#include "Arduino.h"
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#define TILE_TIMEOUT_START { _timeout_start = millis(); }
#define TILE_TIMEOUT_CHECK { if (millis() - _timeout_start > _timeout_ms) return TILE_TIMEOUT; }

static const char _hex[] PROGMEM = "0123456789abcdef";
static inline uint8_t _hexToInt(char c) {
    return isdigit(c) ? c - '0' : (c & 0x0f) + 9;
}

// static int32_t _strToInt(const char* str, size_t len);
static uint64_t _strToUInt(const char* str, size_t len);
static time_t _makeEpoch(tile_datetime_t &datetime);
static void _makeDatetime(tile_datetime_t &datetime, time_t epoch);
static bool _nmeaValidate(const char* msg, size_t len);

SwarmTile::SwarmTile(Stream &str) : _stream(str)
{
    _timeout_ms = TILE_TIMEOUT_MS;
    _rx_buf_pos = 0;
    _tx_pos = 0;
    _tx_checksum = 0;
    _debug = 0;
}

tile_status_t SwarmTile::begin()
{
    _rx_buf_pos = 0;
    memset(_rx_buffer, 0, sizeof(_rx_buffer));
    memset(_rx_fields, 0, sizeof(_rx_fields));
    _tx_pos = 0;
    _tx_checksum = 0;
    _flushStream();
    return TILE_SUCCESS;
}

bool SwarmTile::isReady()
{
    tile_version_t version;

    // verify that Tile completed boot 
    if (getVersion(version) != TILE_SUCCESS) {
        return false;
    }

    return true;
}

bool SwarmTile::isReadyToSend()
{
    tile_datetime_t datetime;

    // verify that Tile completed boot 
    if (!isReady()) {
        return false;
    }

    // verify that Tile acquired date/time, required to send/receive messages
    if (getDateTime(datetime) != TILE_SUCCESS) {
        return false;
    }

    return true;
}

tile_status_t SwarmTile::getVersion(tile_version_t &version)
{
    tile_status_t result;

    memset(&version, 0, sizeof(tile_version_t));
    version.valid = false;

    result = _sendCommand("$FV");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 2) {
        return TILE_PROTOCOL_ERROR;
    }

    strncpy(version.date_str, _rx_fields[1], sizeof(version.date_str)-1);
    strncpy(version.version_str, _rx_fields[2], sizeof(version.version_str)-1);
    version.valid = true;

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::getConfig(tile_config_t &config)
{
    tile_status_t result;

    memset(&config, 0, sizeof(tile_config_t));
    config.valid = false;

    result = _sendCommand("$CS");
    if (result != TILE_SUCCESS) {
        return result;
    }

    uint8_t i = 1;
    while (i <= _rx_field_count) {
        if (strncmp(_rx_fields[i], "AI=", 3) == 0) {
            config.app_id = _strToUInt(_rx_fields[i]+3, strlen(_rx_fields[i])-3);
        } else if (strncmp(_rx_fields[i], "DI=", 3) == 0) {
            config.device_id = _strToUInt(_rx_fields[i]+3, strlen(_rx_fields[i])-3);
        } else if (strncmp(_rx_fields[i], "DN=", 3) == 0) {
            strncpy(config.device_type, _rx_fields[i]+3, sizeof(config.device_type)-1);
        } else {
            // ignore unknown fields
        }
        i++;
    }
    config.valid = true;

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::setGpioMode(uint8_t mode)
{
    char mode_buf[8];
    ltoa(mode, mode_buf, 10);

    _sendBegin();
    _send("$GP ");
    _send(mode_buf);
    _sendEnd();

    return _receiveResponse("$GP");
}

tile_status_t SwarmTile::sleep(tile_sleep_t &sleep)
{
    tile_status_t result;
    char sleep_buf[20];

    sleep.valid = false;

    _sendBegin();
    _send("$SL ");
    if (sleep.seconds != 0) {
        _send("S=");
        _send(ltoa(sleep.seconds, sleep_buf, 10));
    } else if (sleep.wakeup.valid == true) {
        _send("U=");
        snprintf(sleep_buf, sizeof(sleep_buf), "%04d-%02d-%02d %02d:%02d:%02d",
            sleep.wakeup.year, sleep.wakeup.month, sleep.wakeup.day,
            sleep.wakeup.hour, sleep.wakeup.minute, sleep.wakeup.second);
        _send(sleep_buf);
    }
    _sendEnd();

    result = _receiveResponse("$SL");

    if (result == TILE_SUCCESS) {
        if (_rx_field_count == 1 && strcmp(_rx_fields[1], "OK") == 0) {
            // ok
            sleep.valid = true;
            return TILE_SUCCESS;
        }
    } 

    return result;
}

tile_status_t SwarmTile::wake()
{
    tile_status_t result;

    result = _sendCommand("$SL @");   // dummy command to trigger wakeup over serial
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count < 1) {
        return TILE_PROTOCOL_ERROR;
    }

    if (strcmp(_rx_fields[1], "WAKE") != 0) {
        // tile wasn't sleeping
        _setErrorStr("NOTSLEEPING");
        return TILE_COMMAND_ERROR;
    }

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::powerOff()
{
    tile_status_t result;

    result = _sendCommand("$PO");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count < 1) {
        return TILE_PROTOCOL_ERROR;
    }

    if (strcmp(_rx_fields[1], "OK") != 0) {
        // unexpected response
        return TILE_COMMAND_ERROR;
    }

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::getDateTime(tile_datetime_t &datetime)
{
    tile_status_t result;

    memset(&datetime, 0, sizeof(tile_datetime_t));
    datetime.valid = false;

    result = _sendCommand("$DT @");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 2 || strlen(_rx_fields[1]) != 14) {
        return TILE_PROTOCOL_ERROR;
    }

    datetime.year = _strToUInt(_rx_fields[1], 4);
    datetime.month = _strToUInt(_rx_fields[1]+4, 2);
    datetime.day = _strToUInt(_rx_fields[1]+6, 2);
    datetime.hour = _strToUInt(_rx_fields[1]+8, 2);
    datetime.minute = _strToUInt(_rx_fields[1]+10, 2);
    datetime.second = _strToUInt(_rx_fields[1]+12, 2);

    if (strncmp(_rx_fields[2],"V", 1) == 0) {
        datetime.valid = true;
    }

    return TILE_SUCCESS;   
}

tile_status_t SwarmTile::getGeoData(tile_geo_data_t &geo_data)
{
    tile_status_t result;

    memset(&geo_data, 0, sizeof(tile_geo_data_t));
    geo_data.valid = false;

    result = _sendCommand("$GS @");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 5) {
        return TILE_PROTOCOL_ERROR;
    }

    if (strcmp(_rx_fields[5], "NF") == 0) {
        return TILE_NO_GPS_FIX;
    }

    result = _sendCommand("$GN @");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 5) {
        return TILE_PROTOCOL_ERROR;
    }

    // TODO: On SAMD M0 atof adds 11kb to executable! Reimplement?
    geo_data.latitude = atof(_rx_fields[1]);
    geo_data.longitude = atof(_rx_fields[2]);
    geo_data.altitude = atof(_rx_fields[3]);
    geo_data.course = atof(_rx_fields[4]);
    geo_data.speed = atof(_rx_fields[5]);
    
    // todo: add sanity checks?
    geo_data.valid = true;

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::getUnsentCount(tile_msg_count_t &msg_count)
{
    tile_status_t result;

    memset(&msg_count, 0, sizeof(tile_msg_count_t));
    msg_count.valid = false;

    result = _sendCommand("$MT C=U");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 1) {
        return TILE_PROTOCOL_ERROR;
    }

    msg_count.count = _strToUInt(_rx_fields[1], strlen(_rx_fields[1]));
    msg_count.valid = true;

    return TILE_SUCCESS;
}

uint16_t SwarmTile::getUnsentCount()
{
    tile_status_t result;
    tile_msg_count_t count;

    result = getUnsentCount(count);
    if (result == TILE_SUCCESS && count.valid) {
        return count.count;
    } else {
        return 0;
    }
}

tile_status_t SwarmTile::getUnreadCount(tile_msg_count_t &msg_count)
{
    tile_status_t result;

    memset(&msg_count, 0, sizeof(tile_msg_count_t));
    msg_count.valid = false;

    result = _sendCommand("$MM C=U");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 1) {
        return TILE_PROTOCOL_ERROR;
    }

    msg_count.count = _strToUInt(_rx_fields[1], strlen(_rx_fields[1]));
    msg_count.valid = true;

    return TILE_SUCCESS;
}

uint16_t SwarmTile::getUnreadCount()
{
    tile_status_t result;
    tile_msg_count_t count;

    result = getUnreadCount(count);
    if (result == TILE_SUCCESS && count.valid) {
        return count.count;
    } else {
        return 0;
    }
}

tile_status_t SwarmTile::deleteUnsentMsgs(tile_msg_count_t &msg_count)
{
    tile_status_t result;

    memset(&msg_count, 0, sizeof(tile_msg_count_t));
    msg_count.valid = false;

    result = _sendCommand("$MT D=U");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 1) {
        return TILE_PROTOCOL_ERROR;
    }

    msg_count.count = _strToUInt(_rx_fields[1], strlen(_rx_fields[1]));
    msg_count.valid = true;

    return TILE_SUCCESS;
}

uint16_t SwarmTile::deleteUnsentMsgs()
{
    tile_status_t result;
    tile_msg_count_t count;

    result = deleteUnsentMsgs(count);
    if (result == TILE_SUCCESS && count.valid) {
        return count.count;
    } else {
        return 0;
    }
}

tile_status_t SwarmTile::deleteReadMsgs(tile_msg_count_t &msg_count)
{
    tile_status_t result;

    memset(&msg_count, 0, sizeof(tile_msg_count_t));
    msg_count.valid = false;

    result = _sendCommand("$MM D=R");
    if (result != TILE_SUCCESS) {
        return result;
    }

    if (_rx_field_count != 1) {
        return TILE_PROTOCOL_ERROR;
    }

    msg_count.count = _strToUInt(_rx_fields[1], strlen(_rx_fields[1]));
    msg_count.valid = true;

    return TILE_SUCCESS;
}

uint16_t SwarmTile::deleteReadMsgs()
{
    tile_status_t result;
    tile_msg_count_t count;

    result = deleteReadMsgs(count);
    if (result == TILE_SUCCESS && count.valid) {
        return count.count;
    } else {
        return 0;
    }
}

tile_status_t SwarmTile::sendMessage(tile_send_msg_t &send_msg)
{
    tile_status_t result;
    char num_buf[16];

    send_msg.msg_id = 0;
    send_msg.valid = false;

    _sendBegin();
    _send("$TD ");
    if (send_msg.hold_time > 0) {
        _send("HD=");
        _send(ltoa(send_msg.hold_time, num_buf, 10));
        _send(',');
    } else if (send_msg.expiration.valid == true) {
        _send("ET=");
        _send(ultoa(_makeEpoch(send_msg.expiration), num_buf, 10));
        _send(',');
    }
    if (send_msg.message) {
        uint16_t i = 0;
        while (i < send_msg.msg_len) {
            _send(_hex[(send_msg.message[i] >> 4) & 0xf]);
            _send(_hex[send_msg.message[i] & 0xf]);
            i++;
        }
    }
    _sendEnd();

    result = _receiveResponse("$TD");

    if (result == TILE_SUCCESS) {
        if (_rx_field_count == 2 && strcmp(_rx_fields[1], "OK") == 0) {
            // ok
            send_msg.msg_id = _strToUInt(_rx_fields[2], strlen(_rx_fields[2]));
            send_msg.valid = true;
            return TILE_SUCCESS;
        } 
    } 

    return result;
}

tile_status_t SwarmTile::sendMessage(const char* str)
{
    tile_send_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.message = str;
    msg.msg_len = strlen(str);
    return sendMessage(msg);
}

tile_status_t SwarmTile::sendMessage(const char* buf, uint16_t len)
{
    tile_send_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.message = buf;
    msg.msg_len = len;
    return sendMessage(msg);
}

tile_status_t SwarmTile::readMessage(tile_read_msg_t &read_msg)
{
    tile_status_t result;

    read_msg.msg_id = 0;
    read_msg.valid = false;

    if (read_msg.message == 0 || read_msg.msg_max == 0) {
        _setErrorStr("NOREADBUFFER");
        return TILE_COMMAND_ERROR;
    }

    // fill message buffer with 0-bytes for robustness.
    memset(read_msg.message, 0, read_msg.msg_max);

    if (read_msg.order == TILE_OLDEST) {
        result = _sendCommand("$MM R=O");
    } else if (read_msg.order == TILE_NEWEST) {
        result = _sendCommand("$MM R=N");
    } else {
        _setErrorStr("BADPARAM");
        return TILE_COMMAND_ERROR;
    }

    if (result == TILE_SUCCESS) {
        if (_rx_field_count == 3) {
            // unpack message
            const char *hexstr = _rx_fields[1];
            uint16_t i = 0;
            uint16_t j = 0;
            while (hexstr[i] != 0 && j < read_msg.msg_max) {
                read_msg.message[j] = (_hexToInt(hexstr[i]) << 4) | _hexToInt(hexstr[i+1]);
                i += 2;
                j++;
            }
            read_msg.msg_len = j;
            read_msg.msg_id = _strToUInt(_rx_fields[2], strlen(_rx_fields[2]));
            _makeDatetime(read_msg.timestamp, _strToUInt(_rx_fields[3], strlen(_rx_fields[3])));
            read_msg.valid = true;
            return TILE_SUCCESS;
        }
    }

    return result;
}

uint16_t SwarmTile::readMessage(char *buf, uint16_t buf_len, tile_order_t order)
{
    tile_read_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.message = buf;
    msg.msg_max = buf_len;
    msg.order = order;
    if (readMessage(msg) == TILE_SUCCESS) {
        if (msg.valid && msg.msg_len > 0) {
            return msg.msg_len;
        }
    }

    return 0;
}

void SwarmTile::setTimeout(uint16_t timeout_ms)
{
    _timeout_ms = timeout_ms;
}

void SwarmTile::setDebugStream(Stream *debug)
{
    _debug = debug;
}

const char* SwarmTile::getErrorStr()
{
    return _err_str;
}

void SwarmTile::_setErrorStr(const char* str)
{
    memset(_err_str, 0, sizeof(_err_str));
    if (str) {
        strncpy(_err_str, str, sizeof(_err_str)-1);
    }
}

void SwarmTile::_flushStream()
{
    // discard any unsolicited data
    while (_stream.available()) {
        _stream.read();
    }
}

tile_status_t SwarmTile::_readLine()
{
    char ch;

    _rx_buf_pos = 0;
    memset(_rx_buffer, 0, sizeof(_rx_buffer));
    memset(_rx_fields, 0, sizeof(_rx_fields));

    while (1) {
        TILE_TIMEOUT_CHECK
        while (_stream.available()) {
            ch = _stream.read();
            if (_debug) {
                _debug->write(ch);
            }
            if (ch == '\n') {
                // line is complete, exit
                return TILE_SUCCESS;
            }
            if (_rx_buf_pos < sizeof(_rx_buffer) - 1) {
                // store character in line buffer
                _rx_buffer[_rx_buf_pos] = ch;
                _rx_buf_pos++;
            } else {
                // line is too long
                return TILE_RX_OVERFLOW;
            }
        }
    }
}

tile_status_t SwarmTile::_sendCommand(const char *command, bool response)
{
    if (response == true) {
        // clear pending serial buffer to have room for expected response
        _flushStream();
    }

    _sendBegin();
    _send(command);
    _sendEnd();

    if (response == false) {
        return TILE_SUCCESS;
    }

    return _receiveResponse(command);
}

tile_status_t SwarmTile::_receiveResponse(const char *command)
{
    tile_status_t result;
    _setErrorStr(0);

    TILE_TIMEOUT_START

    // read from serial port until we get a response to our command
    while (1) {
        result = _readLine();        
        if (result != TILE_SUCCESS) {
            // will eventually return with timeout
            return result;
        }
        if (strncmp(_rx_buffer, command, 3) == 0) {
            break;
        }
    }

    // check that response is a valid NMEA sentence, including checksum
    if (!_nmeaValidate(_rx_buffer, _rx_buf_pos)) {
        return TILE_PROTOCOL_ERROR;
    }

    // extract fields from response
    result = _parseResponse();
    if (result != TILE_SUCCESS) {
        return result;
    }

    // check that response has at least 1 field
    if (_rx_fields[1] == 0) {
        return TILE_PROTOCOL_ERROR;
    }

    // check that response isn't indicating an error
    if (strncmp("ERR", _rx_fields[1], 3) == 0) {
        if (_rx_field_count >= 2) {
            _setErrorStr(_rx_fields[2]);
        }
        return TILE_COMMAND_ERROR;
    }

    return TILE_SUCCESS;
}

tile_status_t SwarmTile::_parseResponse()
{
    // assumptions:
    // - valid NMEA sentence, including * at end of last field
    // - cmd field1,..,fieldn*crc without extra spaces
    uint16_t i = 0; // character index
    uint8_t f = 0;  // field index
    bool cmd = true;  // still parsing cmd
    _rx_field_count = 0;
    memset(_rx_fields, 0, sizeof(_rx_fields));
    _rx_fields[0] = _rx_buffer;
    while (i < _rx_buf_pos) {
        if (_rx_buffer[i] == '*') {
            // found end of last field
            _rx_buffer[i] = 0;
            break;
        }
        if (cmd) {
            if (_rx_buffer[i] == ' ') {
                // found end of command, start of first field
                _rx_buffer[i] = 0;
                cmd = false;
                f = 1;
                i++;
                _rx_fields[f] = _rx_buffer + i;
            } else {
                i++;
            }
        } else {
            if (_rx_buffer[i] == ',') {
                // found end of current field, start of next field
                _rx_buffer[i] = 0;
                f++;
                if (f >= TILE_NMEA_FIELD_COUNT) {
                    // exceeding supported field count, ignore remaining fields
                    return TILE_PROTOCOL_ERROR;
                }
                i++;
                _rx_fields[f] = _rx_buffer + i;
            } else {
                i++;
            }
        }
    }
    _rx_field_count = f;
    return TILE_SUCCESS;
}

void SwarmTile::_sendBegin()
{
    _tx_pos = 0;
    _tx_checksum = 0;
    _setErrorStr(0);
}

void SwarmTile::_send(char c)
{
    if (_tx_pos == 0 && c == '$') {
        // exclude $ at start of command from checksum
    } else {
        _tx_checksum ^= (uint8_t) c;
    }
    _stream.write(c);
    if (_debug) {
        _debug->write(c);
    }
    _tx_pos++;
}

void SwarmTile::_send(const char *str)
{
    while(*str) {
        _send(*str);
        str++;
    }
}

void SwarmTile::_sendEnd()
{
    _stream.write('*');
    _stream.write(_hex[(_tx_checksum >> 4) & 0x0f]);
    _stream.write(_hex[_tx_checksum & 0x0f]);
    _stream.write('\n');
    _stream.flush();

    if (_debug) {
        _debug->write('*');
        _debug->write(_hex[(_tx_checksum >> 4) & 0x0f]);
        _debug->write(_hex[_tx_checksum & 0x0f]);
        _debug->write('\n');
        _debug->flush();
    }
}

static bool _nmeaValidate(const char *msg, size_t len)
{
    if (len <= 5) {
        // sentence at least 5 characters incl. checksum
        return false;
    }
    // sentence must start with $
    if (msg[0] != '$') {
        return false;
    }
    // 3rd to last character must be *
    if (msg[len-3] != '*') {
        return false;
    }
    // calculate and verify checksum
    uint8_t cs = 0;
    size_t i = 0;
    if (msg[0] == '$') {
        // skip $
        i++;
    }
    while (i < len-3) {
        cs ^= ((uint8_t) msg[i]);
        i++;
    }
    if (msg[len-2] != _hex[(cs >> 4) & 0x0f] || msg[len-1] != _hex[cs & 0x0f]) {
        return false;
    }

    return true;
}

/*
static int32_t _strToInt(const char* str, size_t len)
{
    int val = 0;
    bool neg = false;
    if (*str == '-') {
        // handle negative numbers
        neg = true;
        str++;
        len--;
    }
    while (*str && len > 0 && isdigit(*str)) {
        val = val * 10 + (*str - '0');
        str++;
        len--;
    }
    if (neg == true) {
        val = -val;
    }
    return val;
}
*/

static uint64_t _strToUInt(const char* str, size_t len)
{
    uint64_t val = 0;
    if (len > 2 && str[0] == '0' && str[1] == 'x') {
        // convert from hex
        str += 2;
        len -= 2;
        while (*str && len > 0 && isxdigit(*str)) {
            if (isdigit(*str)) {
                val = (val << 4) + (*str - '0');
            } else {
                val = (val << 4) + ((*str & 0x0f) + 9);
            }
            str++;
            len--;
        }
        return val;
    }
    while (*str && len > 0 && isdigit(*str)) {
        val = val * 10 + (*str - '0');
        str++;
        len--;
    }
    return val;
}

// convert datetime stucture to UTC epoch
// assumes that input is in UTC
static time_t _makeEpoch(tile_datetime_t &datetime)
{
    if (datetime.valid != true) {
        return 0;
    }
    struct tm t;
    t.tm_year = datetime.year - 1900;
    t.tm_mon = datetime.month - 1;
    t.tm_mday = datetime.day;
    t.tm_hour = datetime.hour;
    t.tm_min = datetime.minute;
    t.tm_sec = datetime.second;
    t.tm_isdst = 0;
    return mktime(&t) - _timezone;
}

// convert UTC epoch to datetime structure
static void _makeDatetime(tile_datetime_t &datetime, time_t epoch)
{
    struct tm *t;
    t = gmtime(&epoch);
    datetime.year = t->tm_year + 1900;
    datetime.month = t->tm_mon + 1;
    datetime.day = t->tm_mday;
    datetime.hour = t->tm_hour;
    datetime.minute = t->tm_min;
    datetime.second = t->tm_sec;
}


#ifndef SWARMTILE_H
#define SWARMTILE_H

#include "Arduino.h"
#include <Stream.h>
#include <time.h>

#ifndef TILE_MAX_MSG_SIZE
// max message size for Swarm is 192 bytes
// adjust if your incoming messages are shorter to save RAM
#define TILE_MAX_MSG_SIZE 192
#endif

#ifndef TILE_RX_BUFFER_SIZE
// buffer size for one line of incoming serial data
// adding 40 bytes for overhead in $RD and $TD
#define TILE_RX_BUFFER_SIZE 40 + (TILE_MAX_MSG_SIZE * 2)
#endif

// default timeout for communication with Tile
// note: getUnsentCount is very slow, to real-world testing before reducing timeout!
#define TILE_TIMEOUT_MS 2000
// max number of fields in a serial message, incl. command
#define TILE_NMEA_FIELD_COUNT 8

typedef enum {
    TILE_SUCCESS = 0,
    TILE_TIMEOUT = 1,
    TILE_PROTOCOL_ERROR = 2,
    TILE_COMMAND_ERROR = 3,
    TILE_RX_OVERFLOW = 4,
    TILE_NO_GPS_FIX = 5
} tile_status_t;

typedef enum {
    TILE_OLDEST = 0,
    TILE_NEWEST = 1
} tile_order_t;

typedef struct {
    // output
    char date_str[20];      // firmware date and time as a string, e.g. 2021-03-23-18:25:40
    char version_str[20];   // firmeare version as a string, e.g. v1.0.0
    bool valid;
} tile_version_t;

typedef struct {
    // input/output
    uint16_t year;          // date and time in UTC
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool valid;
} tile_datetime_t;

typedef struct {
    // output
    float latitude;         // latitude in degrees
    float longitude;        // longitude in degrees
    float altitude;         // altitude in meters
    float course;           // course in degrees
    float speed;            // speed in km/h
    bool valid;             // false if there's an error or no fix
} tile_geo_data_t;

typedef struct {
    // output
    uint16_t count;
    bool valid;
} tile_msg_count_t;

typedef struct {
    // input
    const char *message;    // pointer to message to be sent
    uint16_t msg_len;       // length of message (1-192)
    uint32_t hold_time;     // time in seconds before unsent msg is discarded (60-172800), set to 0 if not used
    tile_datetime_t expiration; // UTC time when unsent msgs is discarded, ignored if epxiration.valid != true or hold_time > 0
    // output
    uint64_t msg_id;        // message id assigned by tile
    bool valid;
} tile_send_msg_t;

typedef struct {
    // input
    char *message;          // pointer to buffer to receive message into
    uint16_t msg_max;       // length of provided buffer, incoming msgs can be up to 192 bytes
    tile_order_t order;     // message to be read (TILE_OLDEST or TILE_NEWEST)
    // output
    uint16_t app_id;        // app id of the received message (always 0 for Tile fw pre v1.1.0)
    uint64_t msg_id;        // id of the received message
    uint16_t msg_len;       // number of bytes in received message
    tile_datetime_t timestamp;  // UTC time when message was received
    bool valid;
} tile_read_msg_t;

typedef struct {
    // input
    uint16_t seconds;       // seconds to sleep, 3600 max, set to 0 if unused
    tile_datetime_t wakeup; // UTC  time to wake up, ignored if seconds is set
    // output
    bool valid;
} tile_sleep_t;

typedef struct {
    // output
    uint32_t app_id;        // application ID (always 0 for Tile fw v1.1.0+)
    uint32_t device_id;     // device ID
    char device_type[16];   // device type name
    bool valid;
} tile_config_t;

class SwarmTile
{
public:
    SwarmTile(Stream &str);

    tile_status_t begin();
    void setTimeout(uint16_t timeout_ms);   // timeout in milliseconds, ~65 seconds max should suffice
    void setDebugStream(Stream *debug);     // stream for debug output

    bool isReady();         // returns true when Tile is ready (boot complete)
    bool isReadyToSend();   // returns true when Tile is ready to send messages (acquired date/time)

    const char* getErrorStr();  // returns error string in case of a TILE_COMMAND_ERROR

    tile_status_t getVersion(tile_version_t &version);
    tile_status_t getConfig(tile_config_t &config);
    tile_status_t setGpioMode(uint8_t mode);
    tile_status_t getDateTime(tile_datetime_t &datetime);
    tile_status_t getGeoData(tile_geo_data_t &geo_data);
    tile_status_t getUnsentCount(tile_msg_count_t &msg_count);
    tile_status_t getUnreadCount(tile_msg_count_t &msg_count);
    tile_status_t deleteUnsentMsgs(tile_msg_count_t &msg_count);
    tile_status_t deleteReadMsgs(tile_msg_count_t &msg_count);
    tile_status_t sendMessage(tile_send_msg_t &send_msg);
    tile_status_t readMessage(tile_read_msg_t &read_msg);
    tile_status_t sleep(tile_sleep_t &sleep);
    tile_status_t wake();
    tile_status_t powerOff();

    // simplified API, hides complexity but also some error conditions
    uint16_t getUnsentCount();
    uint16_t getUnreadCount();
    uint16_t deleteUnsentMsgs();
    uint16_t deleteReadMsgs();
    tile_status_t sendMessage(const char* str);
    tile_status_t sendMessage(const char* buf, uint16_t len);
    uint16_t readMessage(char* buf, uint16_t buf_len, tile_order_t = TILE_OLDEST);

private:
    Stream &_stream;    // serial stream of Tile
    Stream *_debug;     // stream for debug output

    // timeout variables unsigned long to match Arduino millis() return type
    unsigned long _timeout_ms;        // timeout for tile operations in milliseconds
    unsigned long _timeout_start;     // start time for determining timeout

    // buffer for serial communication with tile, shared rx/tx to minimize RAM use
    char _rx_buffer[TILE_RX_BUFFER_SIZE];
    uint16_t _rx_buf_pos;

    // NMEA fields in incoming message after parsing, pointers into rx/tx buffer
    const char *_rx_fields[TILE_NMEA_FIELD_COUNT];
    uint16_t _rx_field_count;

    // copy of error message in case of TILE_COMMAND_ERROR
    char _err_str[20];

    // rolling checksum of current command sent to tile
    uint8_t _tx_checksum;
    // counter of bytes sent in current command
    uint16_t _tx_pos;

    void _sendBegin();
    void _send(char c);
    void _send(const char *str);
    void _sendEnd();

    void _flushStream();
    tile_status_t _readLine();
    tile_status_t _sendCommand(const char *command, bool response = true);
    tile_status_t _receiveResponse(const char *command);
    tile_status_t _parseResponse();

    void _setErrorStr(const char* str);
};

#endif
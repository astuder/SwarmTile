# Swarm Tile Library

This is an Arduino library to integrate with the Swarm Tile module.

Swarm is a satellite network designed for low-cost IoT communication. The Swarm Tile satellite data modem transmits and receives data to and from Swarm’s space network and is designed to be embedded into a third-party product.

This library supports `TILE01` running firmware `v1.0.0` or newer.

**Note: This library is NOT developed, supported or endorsed by Swarm Technologies Inc. Please open a GitHub issue if you need help with this library.**

# Documentation

See `Examples` > `SwarmTile` > `BasicSendReceive` for usage of the simplified API.

```
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

    // simplified API, hides complexity but also some features and error conditions
    uint16_t getUnsentCount();
    uint16_t getUnreadCount();
    uint16_t deleteUnsentMsgs();
    uint16_t deleteReadMsgs();
    tile_status_t sendMessage(const char* str);
    tile_status_t sendMessage(const char* buf, uint16_t len);
    tile_status_t sendMessage(uint16_t app_id, const char* str);     // requires FW v1.1.0+    
    tile_status_t sendMessage(uint16_t app_id, const char* buf, uint16_t len);   // requires FW v1.1.0+
    uint16_t readMessage(char* buf, uint16_t buf_len, tile_order_t = TILE_OLDEST);
```

# Known Issues

## Receiving of messages is unverified

We weren't able to verify receiving of messages in the real world. Please let us know if you successfully used `readMessage()`.

## Wakeup immediately after Sleep

The Tile takes a few seconds to enter sleep mode. Calling `wake()` to soon after `sleep()` may result in confusing error messages. 

To avoid this error, sleep for 20 seconds or longer.

## DBXTOHIVEFULL

With Tile FW 1.0.0 and 0.9.7, the example code sometimes puts the Tile into a state where `sendMessage()` returns with a `DBXTOHIVEFULL` error.

To resolve this error send the following command to the Tile: `$RS dbinit*3d`

This issue is resolved with Tile FW 1.1.0 and newer.

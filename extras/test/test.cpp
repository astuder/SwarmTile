#include <stdio.h>
#include <pthread.h>
#include "munit/munit.h"
#include "SwarmTile.h"
#include "SerialEmu.h"
#include "TileEmu.h"

// define get verbose output of NMEA traffic
// #define VERBOSE

SerialEmu serial;
TileEmu tile_emu(serial);
SwarmTile tile(serial);
static bool verbose = false;
pthread_t th;

void print_result(tile_status_t);
void tile_emu_begin(const char *expected, const char *response);
void tile_emu_begin(emu_sequence_t *sequence);
void tile_emu_end(tile_status_t result);

static MunitResult test_nmeaParsing(const MunitParameter params[], void *data)
{
    tile_status_t result;
    tile_version_t version;

    // checksum error
    tile_emu_begin("$FV", "$FV 2021-03-23-18:25:40,v1.0.0*7e\n");
    result = tile.getVersion(version);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(version.valid);

    // missing checksum
    tile_emu_begin("$FV", "$FV 2021-03-23-18:25:40,v1.0.0\n");
    result = tile.getVersion(version);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(version.valid);

    // line not starting with $
    tile_emu_begin("$FV", "FV 2021-03-23-18:25:40,v1.0.0");
    result = tile.getVersion(version);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_TIMEOUT);
    munit_assert_false(version.valid);

    return MUNIT_OK;
}

static MunitResult test_getVersion(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_version_t version;

    // get firmware version
    tile_emu_begin("$FV", "$FV 2021-03-23-18:25:40,v1.0.0");
    result = tile.getVersion(version);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(version.valid);
    munit_assert_string_equal(version.date_str, "2021-03-23-18:25:40");
    munit_assert_string_equal(version.version_str, "v1.0.0");

    // missing response fields
    tile_emu_begin("$FV", "$FV");
    result = tile.getVersion(version);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(version.valid);

    return MUNIT_OK;
}

static MunitResult test_getConfig(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_config_t config;

    // get configuration (pre v1.1.0)
    tile_emu_begin("$CS", "$CS AI=1999,DI=0x00051b,DN=TILE");
    result = tile.getConfig(config);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(config.valid);
    munit_assert_int(config.app_id, ==, 1999);
    munit_assert_int(config.device_id, ==, 0x51b);
    munit_assert_string_equal(config.device_type, "TILE");

    // get configuration (v1.1.0+)
    tile_emu_begin("$CS", "$CS DI=0x00051b,DN=TILE");
    result = tile.getConfig(config);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(config.valid);
    munit_assert_int(config.app_id, ==, 0);
    munit_assert_int(config.device_id, ==, 0x51b);
    munit_assert_string_equal(config.device_type, "TILE");

    // missing response fields
    tile_emu_begin("$CS", "$CS");
    result = tile.getConfig(config);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(config.valid);

    return MUNIT_OK;
}

static MunitResult test_setGpioMode(const MunitParameter params[], void* data)
{
    tile_status_t result;

    // set GPIO mode to a valid value
    tile_emu_begin("$GP 5", "$GP OK");
    result = tile.setGpioMode(5);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);

    // set GPIO mode to an ivalid value
    tile_emu_begin("$GP 42", "$GP ERR,ERR");
    result = tile.setGpioMode(42);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_COMMAND_ERROR);

    // missing response fields
    tile_emu_begin("$GP 5", "$GP");
    result = tile.setGpioMode(5);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);

    return MUNIT_OK;
}

static MunitResult test_sleep(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_sleep_t sleep;

    // sleep seconds
    tile_emu_begin("$SL S=100", "$SL OK");
    sleep.seconds = 100;
    result = tile.sleep(sleep);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(sleep.valid);

    // sleep until datetime
    tile_emu_begin("$SL U=2021-06-13 05:36:33", "$SL OK");
    sleep.wakeup.year = 2021;
    sleep.wakeup.month = 6;
    sleep.wakeup.day = 13;
    sleep.wakeup.hour = 5;
    sleep.wakeup.minute = 36;
    sleep.wakeup.second = 33;
    sleep.wakeup.valid = true;
    sleep.seconds = 0;
    result = tile.sleep(sleep);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(sleep.valid);

    // missing response fields
    tile_emu_begin("$SL S=100", "$SL");
    sleep.seconds = 100;
    result = tile.sleep(sleep);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(sleep.valid);

    return MUNIT_OK;
}

static MunitResult test_wake(const MunitParameter params[], void* data)
{
    tile_status_t result;

    // wake up
    tile_emu_begin("$SL @", "$SL WAKE,SERIAL @ 2021-06-13 14:48:34");
    result = tile.wake();
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);

    // wake up while not asleep
    tile_emu_begin("$SL @", "$SL ERR,NOCOMMAND");
    result = tile.wake();
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_COMMAND_ERROR);

    return MUNIT_OK;
}

static MunitResult test_powerOff(const MunitParameter params[], void* data)
{
    tile_status_t result;

    // power off
    tile_emu_begin("$PO", "$PO OK");
    result = tile.powerOff();
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);

    // power off with error
    tile_emu_begin("$PO", "$PO ERR");
    result = tile.powerOff();
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_COMMAND_ERROR);

    return MUNIT_OK;
}


static MunitResult test_getDateTime(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_datetime_t datetime;

    // get date and time
    tile_emu_begin("$DT @", "$DT 20210611042422,V");
    result = tile.getDateTime(datetime);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(datetime.valid);
    munit_assert_int(datetime.year, ==, 2021);
    munit_assert_int(datetime.month, ==, 6);
    munit_assert_int(datetime.day, ==, 11);
    munit_assert_int(datetime.hour, ==, 4);
    munit_assert_int(datetime.minute, ==, 24);
    munit_assert_int(datetime.second, ==, 22);

    // missing response fields
    tile_emu_begin("$DT @", "$DT");
    result = tile.getDateTime(datetime);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(datetime.valid);

    return MUNIT_OK;
}

static MunitResult test_getGeoData(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_geo_data_t geo_data;

    // get gps data
    emu_sequence_t geo_test1[] = {
        { "$GS @", "$GS 109,214,9,0,G3" },
        { "$GN @", "$GN 37.8921,-122.0155,77,89,2" },
        { 0, 0 }
    };
    tile_emu_begin(geo_test1);
    result = tile.getGeoData(geo_data);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(geo_data.valid);
    munit_assert_float(geo_data.latitude, ==, 37.8921);
    munit_assert_float(geo_data.longitude, ==, -122.0155);
    munit_assert_float(geo_data.altitude, ==, 77.0);
    munit_assert_float(geo_data.course, ==, 89.0);
    munit_assert_float(geo_data.speed, ==, 2.0);

    // no gps fix
    tile_emu_begin("$GS @", "$GS 0,0,0,0,NF");
    result = tile.getGeoData(geo_data);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_NO_GPS_FIX);
    munit_assert_false(geo_data.valid);

    // missing response fields
    emu_sequence_t geo_test2[] = {
        { "$GS @", "$GS 109,214,9,0,G3" },
        { "$GN @", "$GN @" },
        { 0, 0 }
    };
    tile_emu_begin(geo_test2);
    result = tile.getGeoData(geo_data);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(geo_data.valid);

    return MUNIT_OK;
}

static MunitResult test_getUnsentCount(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_msg_count_t msg_count;

    // get # of unsent messages
    tile_emu_begin("$MT C=U", "$MT 12");
    result = tile.getUnsentCount(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(msg_count.valid);
    munit_assert_float(msg_count.count, ==, 12);

    // missing response fields
    tile_emu_begin("$MT C=U", "$MT");
    result = tile.getUnsentCount(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(msg_count.valid);

    return MUNIT_OK;
}

static MunitResult test_getUnreadCount(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_msg_count_t msg_count;

    // get # of unread messages
    tile_emu_begin("$MM C=U", "$MM 3");
    result = tile.getUnreadCount(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(msg_count.valid);
    munit_assert_float(msg_count.count, ==, 3);

    // missing response fields
    tile_emu_begin("$MM C=U", "$MM");
    result = tile.getUnreadCount(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(msg_count.valid);

    return MUNIT_OK;
}

static MunitResult test_deleteUnsentMsgs(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_msg_count_t msg_count;

    // delete unsent messages
    tile_emu_begin("$MT D=U", "$MT 12");
    result = tile.deleteUnsentMsgs(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(msg_count.valid);
    munit_assert_float(msg_count.count, ==, 12);

    // missing response fields
    tile_emu_begin("$MT D=U", "$MT");
    result = tile.deleteUnsentMsgs(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(msg_count.valid);

    return MUNIT_OK;
}

static MunitResult test_deleteReadMsgs(const MunitParameter params[], void* data)
{
    tile_status_t result;
    tile_msg_count_t msg_count;

    // delete read messages
    tile_emu_begin("$MM D=R", "$MM 3");
    result = tile.deleteReadMsgs(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(msg_count.valid);
    munit_assert_float(msg_count.count, ==, 3);

    // missing response fields
    tile_emu_begin("$MM D=R", "$MM");
    result = tile.deleteReadMsgs(msg_count);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_PROTOCOL_ERROR);
    munit_assert_false(msg_count.valid);

    return MUNIT_OK;
}

static MunitResult test_sendMessage(const MunitParameter params[], void* data)
{
    tile_status_t result;

    const char *test_msg = "hello world";
    tile_send_msg_t send;

    // send a message with no (default) hold or expiration time
    memset(&send, 0, sizeof(send));
    send.message = test_msg;
    send.msg_len = strlen(test_msg);
    tile_emu_begin("$TD 68656c6c6f20776f726c64", "$TD OK,5354468575855");
    result = tile.sendMessage(send);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(send.valid);
    munit_assert(send.msg_id == (uint64_t) 5354468575855);

    // send a message with hold time
    memset(&send, 0, sizeof(send));
    send.message = test_msg;
    send.msg_len = strlen(test_msg);
    send.hold_time = 7200;
    tile_emu_begin("$TD HD=7200,68656c6c6f20776f726c64", "$TD OK,5354468575855");
    result = tile.sendMessage(send);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(send.valid);
    munit_assert(send.msg_id == (uint64_t) 5354468575855);

    // send a message with expiration time
    memset(&send, 0, sizeof(send));
    send.message = test_msg;
    send.msg_len = strlen(test_msg);
    send.expiration.year = 2021;
    send.expiration.month = 6;
    send.expiration.day = 13;
    send.expiration.hour = 5;
    send.expiration.minute = 36;
    send.expiration.second = 33;
    send.expiration.valid = true;
    tile_emu_begin("$TD ET=1623562593,68656c6c6f20776f726c64", "$TD OK,5354468575855");
    result = tile.sendMessage(send);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(send.valid);
    munit_assert(send.msg_id == 5354468575855);

    // send a message with bad expiration time
    memset(&send, 0, sizeof(send));
    send.message = test_msg;
    send.msg_len = strlen(test_msg);
    send.expiration.year = 2000;
    send.expiration.month = 1;
    send.expiration.day = 1;
    send.expiration.valid = true;
    tile_emu_begin("$TD ET=946684800,68656c6c6f20776f726c64", "$TD ERR,BADEXPIRETIME,0");
    result = tile.sendMessage(send);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_COMMAND_ERROR);
    munit_assert_false(send.valid);
    munit_assert_string_equal(tile.getErrorStr(), "BADEXPIRETIME");

    return MUNIT_OK;
}


static MunitResult test_readMessage(const MunitParameter params[], void* data)
{
    tile_status_t result;

    tile_read_msg_t read;
    const uint16_t msg_buf_len = 192;
    char msg_buf[msg_buf_len];

    // read newest message (FW pre-v1.1.0)
    memset(&read, 0, sizeof(read));
    memset(msg_buf, 0, sizeof(msg_buf));
    read.message = msg_buf;
    read.msg_max = msg_buf_len;
    read.order = TILE_NEWEST;
    tile_emu_begin("$MM R=N", "$MM 6578616d706c65206d657373616765,21990235111426,1584494275");
    result = tile.readMessage(read);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(read.valid);
    munit_assert_string_equal(read.message, "example message");
    munit_assert_int(read.msg_len, ==, 15);
    munit_assert(read.msg_id == 21990235111426);
    munit_assert_int(read.timestamp.year, ==, 2020);
    munit_assert_int(read.timestamp.month, ==, 3);
    munit_assert_int(read.timestamp.day, ==, 18);
    munit_assert_int(read.timestamp.hour, ==, 1);
    munit_assert_int(read.timestamp.minute, ==, 17);
    munit_assert_int(read.timestamp.second, ==, 55);

    // read newest message (FW v1.1.0+)
    memset(&read, 0, sizeof(read));
    memset(msg_buf, 0, sizeof(msg_buf));
    read.message = msg_buf;
    read.msg_max = msg_buf_len;
    read.order = TILE_NEWEST;
    tile_emu_begin("$MM R=N", "$MM AI=1234,6578616d706c65206d657373616765,21990235111426,1584494275");
    result = tile.readMessage(read);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(read.valid);
    munit_assert_string_equal(read.message, "example message");
    munit_assert_int(read.msg_len, ==, 15);
    munit_assert(read.msg_id == 21990235111426);
    munit_assert_int(read.app_id, ==, 1234);
    munit_assert_int(read.timestamp.year, ==, 2020);
    munit_assert_int(read.timestamp.month, ==, 3);
    munit_assert_int(read.timestamp.day, ==, 18);
    munit_assert_int(read.timestamp.hour, ==, 1);
    munit_assert_int(read.timestamp.minute, ==, 17);
    munit_assert_int(read.timestamp.second, ==, 55);

    // read oldest message
    memset(&read, 0, sizeof(read));
    memset(msg_buf, 0, sizeof(msg_buf));
    read.message = msg_buf;
    read.msg_max = msg_buf_len;
    read.order = TILE_OLDEST;
    tile_emu_begin("$MM R=O", "$MM 6578616d706c65206d657373616765,21990235111426,1584494275");
    result = tile.readMessage(read);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(read.valid);
    munit_assert_string_equal(read.message, "example message");
    munit_assert_int(read.msg_len, ==, 15);
    munit_assert(read.msg_id == 21990235111426);
    munit_assert_int(read.timestamp.year, ==, 2020);
    munit_assert_int(read.timestamp.month, ==, 3);
    munit_assert_int(read.timestamp.day, ==, 18);
    munit_assert_int(read.timestamp.hour, ==, 1);
    munit_assert_int(read.timestamp.minute, ==, 17);
    munit_assert_int(read.timestamp.second, ==, 55);

    // provided buffer shorter than message, message should get truncated
    memset(&read, 0, sizeof(read));
    memset(msg_buf, 0, sizeof(msg_buf));
    read.message = msg_buf;
    read.msg_max = 7;
    read.order = TILE_OLDEST;
    tile_emu_begin("$MM R=O", "$MM 6578616d706c65206d657373616765,21990235111426,1584494275");
    result = tile.readMessage(read);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_SUCCESS);
    munit_assert_true(read.valid);
    munit_assert_string_equal(read.message, "example");
    munit_assert_int(read.msg_len, ==, 7);
    munit_assert(read.msg_id == 21990235111426);
    munit_assert_int(read.timestamp.year, ==, 2020);
    munit_assert_int(read.timestamp.month, ==, 3);
    munit_assert_int(read.timestamp.day, ==, 18);
    munit_assert_int(read.timestamp.hour, ==, 1);
    munit_assert_int(read.timestamp.minute, ==, 17);
    munit_assert_int(read.timestamp.second, ==, 55);

    // no more unread messages available
    memset(&read, 0, sizeof(read));
    memset(msg_buf, 0, sizeof(msg_buf));
    read.message = msg_buf;
    read.msg_max = msg_buf_len;
    read.order = TILE_OLDEST;
    tile_emu_begin("$MM R=O", "$MM ERR,DBXNOMORE");
    result = tile.readMessage(read);
    tile_emu_end(result);
    munit_assert_int(result, ==, TILE_COMMAND_ERROR);
    munit_assert_false(read.valid);
    munit_assert_string_equal(tile.getErrorStr(), "DBXNOMORE");

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
    { (char*) "NMEA message parsing", test_nmeaParsing, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getVersion", test_getVersion, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getConfig", test_getConfig, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "setGpioMode", test_setGpioMode, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "sleep", test_sleep, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "wake", test_wake, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "powerOff", test_powerOff, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getDateTime", test_getDateTime, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getGeoData", test_getGeoData, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getUnsentCount", test_getUnsentCount, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "getUnreadCount", test_getUnreadCount, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "deleteUnsentMsgs", test_deleteUnsentMsgs, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "deleteReadMsgs", test_deleteReadMsgs, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "sendMessage", test_sendMessage, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "readMessage", test_readMessage, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite test_suite = {
    (char*) "",
    test_suite_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    // shorten timeout for faster test run
    tile.setTimeout(100);

#ifdef VERBOSE
    verbose = true;
    tile_emu.setVerbose(true);
#endif

    return munit_suite_main(&test_suite, (void*) "SwarmTile tests", argc, argv);
}

emu_sequence_t short_seq[] = { { 0, 0 }, { 0, 0 } };

void tile_emu_begin(const char *expected, const char *response)
{
    tile_emu.flush();
    short_seq[0].expected = expected;
    short_seq[0].response = response;
    tile_emu.setSequence(short_seq);
    pthread_create(&th, NULL, &TileEmu::run, &tile_emu);
}

void tile_emu_begin(emu_sequence_t *sequence)
{
    tile_emu.flush();
    tile_emu.setSequence(sequence);
    pthread_create(&th, NULL, &TileEmu::run, &tile_emu);    
}

void tile_emu_end(tile_status_t result)
{
    if (verbose) {
        print_result(result);
    }
    tile_emu.stop();
    pthread_join(th, NULL);
}

void print_result(tile_status_t result)
{
    switch (result) {
    case TILE_SUCCESS:
        printf("TILE_SUCCESS");
        break;
    case TILE_TIMEOUT:
        printf("TILE_TIMEOUT");
        break;
    case TILE_PROTOCOL_ERROR:
        printf("TILE_PROTOCOL_ERROR");
        break;
    case TILE_COMMAND_ERROR:
        printf("TILE_COMMAND_ERROR");
        break;
    case TILE_RX_OVERFLOW:
        printf("TILE_RX_OVERFLOW");
        break;
    default:
        printf("unknown result code");
        break;
    }

    printf(" (%d)\n", result);
}
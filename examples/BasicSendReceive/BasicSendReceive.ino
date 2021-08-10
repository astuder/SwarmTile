#include <tilelib.h>

/*
 * BasicSendReceive
 * 
 * This sketch initializes the tile and queues a "Hello world!" message to be sent.
 * It then checks every 30 seconds if the message was sent and whether there was a
 * message received. Any received message will be printed.
 * 
 * The sketch assumes that the tile is connected to Serial3. Change this as needed.
 */

#define TileSerial Serial1  // serial port connected to the tile

#define TILE_DEBUG true      // set to true to log tile diagnostics
#define DEBUG_SERIAL Serial  // serial port to send debug info to

// Helper functions
void printError(const char* func, tile_status_t res);
void printDateTime(tile_datetime_t datetime);
void printMessage(const char* msg, uint16_t len);

// Declare the Tile object
SwarmTile tile(TileSerial);

void setup()
{
  // Most calls will return a result of this type.
  // Results other than TILE_SUCCESS means that there was an error.
  tile_status_t result;
  
  // Start the console serial port
  Serial.begin(9600);
  //  while (!Serial);

  // Start the serial port connected to the satellite modem.
  // The speed of the serial connection with the tile is always 115200 baud.
  TileSerial.begin(115200);

#if TILE_DEBUG
  // Log all communication with tile.
  Serial.println("Enabling tile debug output.");
  DEBUG_SERIAL.begin(9600);
  tile.setDebugStream(&DEBUG_SERIAL);
#endif

  // Example: Begin tile operation and wait for tile to complete boot.
  // On power-up, the tile will take a few seconds to complete the boot process.
  Serial.print("Starting Swarm Tile...");
  tile.begin();
  while (!tile.isReady()) {
    Serial.print(".");
    delay(2000);
  };
  Serial.println("done!");

  // Example: Print the firmware version and date.
  tile_version_t version;
  result = tile.getVersion(version);
  if (result != TILE_SUCCESS) {
    printError("getVersion", result);
    return;
  }
  if (version.valid == true) {
    Serial.print("Firmware Version is ");
    Serial.print(version.version_str);
    Serial.print(" ");
    Serial.print(version.date_str);
    Serial.println(".");
  }

  // Example: Delete any unsent messages.
  // The tile saves unsent messages across power cycles. To avoid piling up test
  // messages, we'll delete unsent messages here.
  Serial.print("Delete unsent messages...");
  uint16_t del_count = tile.deleteUnsentMsgs();
  Serial.print(del_count);
  Serial.println(" msgs deleted.");
    
  // Example: Wait for Tile to be ready to send messages.
  // The tile requires its realtime clock to be initialized before accepting messages.
  // After power-up, this requires a GPS fix, which can take some time.
  Serial.print("Waiting for Tile to be ready to send...");
  while (!tile.isReadyToSend()) {
    Serial.print(".");
    delay(2000);
  };
  Serial.println("ready!");

  // Example: Send a string message.
  // The message will be queued by the Tile and sent during a future satellite pass.
  Serial.println("Queueing a new message to be sent.");
  result = tile.sendMessage("Hello world!");
  if (result != TILE_SUCCESS) {
    printError("sendMessage", result);
    return;
  } else {
      Serial.println("Message is waiting for the next satellite pass!");
  }
}

void loop()
{
  tile_status_t result;

  // Example: Get timestamp and number of pending outgoing and incoming messages
  tile_datetime_t datetime;
  result = tile.getDateTime(datetime);
  if (result != TILE_SUCCESS) {
    printError("getDateTime", result);
  }
  if (datetime.valid) {
    printDateTime(datetime);
  } else {
    Serial.print("invalid-date");
  }

  uint16_t msg_count;
  msg_count = tile.getUnsentCount();
  Serial.print(" unsent: ");
  Serial.print(msg_count);
  msg_count = tile.getUnreadCount();
  Serial.print(" unread: ");
  Serial.println(msg_count);

  // Example: Read received messages.
  uint16_t rcvd_count = 0;
  while (tile.getUnreadCount() > 0) {
    char buffer[192];
    uint16_t msg_len = tile.readMessage(buffer, sizeof(buffer));
    if (msg_len > 0) {
      Serial.print("Message received! Length: ");
      Serial.print(msg_len);
      Serial.println(" bytes");
      printMessage(buffer, msg_len);
    } else {
      Serial.println("Empty message received?!");
    }
    rcvd_count++;
  }

  // Example: Delete all read messages.
  // The tile saves read messages until deleted. To avoid piling up test
  // messages, we'll delete all read messages here.
  if (rcvd_count > 0) {
    Serial.print("Delete read messages...");
    uint16_t deleted_count = tile.deleteReadMsgs();
    Serial.print(deleted_count);
    Serial.println(" msgs deleted");
  }

  // wait 30 seconds
  delay(30000);
}

// Helper function to print verbose error messages
void printError(const char* func, tile_status_t res)
{
  Serial.print(func);
  Serial.print(" failed: ");
  Serial.print(res);
  if (res == TILE_COMMAND_ERROR) {
    Serial.print(" ");
    Serial.print(tile.getErrorStr());
  }
  Serial.println();
}

// Helper function to print timestamps
void printDateTime(tile_datetime_t datetime)
{
  char buf[20];
  snprintf(buf, 20, "%04d%02d%02d-%02d%02d%02d",
           datetime.year, datetime.month, datetime.day,
           datetime.hour, datetime.minute, datetime.second);
  Serial.print(buf);
}

// Helper function to print message content
void printMessage(const char* msg, uint16_t len)
{
  uint16_t i = 0;
  bool isPrintable = true;
  char hexbuf[3];
  while (i < len) {
    if (msg[i] < 16) {
      Serial.print("0");
    }
    Serial.print(msg[i], HEX);
    if (!isprint(msg[i])) {
      isPrintable = false;
    }
    i++;
  }
  Serial.println();
  if (isPrintable) {
    Serial.println(msg);  // assumes buffer has spare 0-bytes at the end
  }
}

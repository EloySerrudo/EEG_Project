#include <SPI.h>
#include <stdlib.h>
#include <ArduinoJson.h>
#include "adsCommand.h"
#include "ads129x.h"
#include "SerialCommand.h"
#include "JsonCommand.h"
#include "SpiDma.h"


#define BAUD_RATE 921600

#define SPI_BUFFER_SIZE 200
#define OUTPUT_BUFFER_SIZE 1000

#define TEXT_MODE 0
#define JSONLINES_MODE 1
#define MESSAGEPACK_MODE 2

#define RESPONSE_OK 200
#define RESPONSE_BAD_REQUEST 400
#define UNRECOGNIZED_COMMAND 406
#define RESPONSE_ERROR 500
#define RESPONSE_NOT_IMPLEMENTED 501
#define RESPONSE_NO_ACTIVE_CHANNELS 502

const char *STATUS_TEXT_OK = "Ok";
const char *STATUS_TEXT_BAD_REQUEST = "Bad request";
const char *STATUS_TEXT_ERROR = "Error";
const char *STATUS_TEXT_NOT_IMPLEMENTED = "Not Implemented";
const char *STATUS_TEXT_NO_ACTIVE_CHANNELS = "No Active Channels";

int protocol_mode = TEXT_MODE;
int max_channels = 0;
int num_active_channels = 0;
bool active_channels[9];  // reports whether channels 1..9 are active
int num_spi_bytes = 0;
int num_timestamped_spi_bytes = 0;
bool is_rdatac = false;
bool base64_mode = true;

char hexDigits[] = "0123456789ABCDEF";
const char b64_alphabets[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";

// microseconds timestamp
#define TIMESTAMP_SIZE_IN_BYTES 4
union {
  char timestamp_bytes[TIMESTAMP_SIZE_IN_BYTES];
  unsigned long timestamp;
} timestamp_union;

// sample number counter
#define SAMPLE_NUMBER_SIZE_IN_BYTES 4
union {
  char sample_number_bytes[SAMPLE_NUMBER_SIZE_IN_BYTES];
  unsigned long sample_number = 0;
} sample_number_union;

// SPI input buffer
uint8_t spi_bytes[SPI_BUFFER_SIZE];
volatile bool spi_data_available;

// char buffer to send via USB
char output_buffer[OUTPUT_BUFFER_SIZE];

const uint8_t PIN_LED = 4;
const char *hardware_type = "unknown";
const char *board_name = "MyEEG";
const char *maker_name = "Eloy";
const char *driver_version = "v0.0.1";

const char *json_rdatac_header = "{\"C\":200,\"D\":\"";
const char *json_rdatac_footer = "\"}";

uint8_t messagepack_rdatac_header[] = { 0x82, 0xa1, 0x43, 0xcc, 0xc8, 0xa1, 0x44, 0xc4 };
size_t messagepack_rdatac_header_size = sizeof(messagepack_rdatac_header);

SerialCommand serialCommand;
JsonCommand jsonCommand;

// PWDN = 21
// RESET = 22
// START = 59
// DRDY = 16
// MOSI = 23
// MISO = 19
// CLK = 18
// CS = 5
// LED 4

void setup() {
  // put your setup code here, to run once:
  Serial.begin(BAUD_RATE);
  pinMode(PIN_LED, OUTPUT);    // Configure the onboard LED for output
  digitalWrite(PIN_LED, LOW);  // default to LED off

  protocol_mode = TEXT_MODE;
  arduinoSetup();
  adsSetup();

  // Setup callbacks for SerialCommand commands
  serialCommand.addCommand("nop", nopCommand);                    // No operation (does nothing)
  serialCommand.addCommand("micros", microsCommand);              // Returns number of microseconds since the program began executing
  serialCommand.addCommand("version", versionCommand);            // Echos the driver version number
  serialCommand.addCommand("status", statusCommand);              // Echos the driver status
  serialCommand.addCommand("serialnumber", serialNumberCommand);  // Echos the board serial number (UUID from the onboard 24AA256UID-I/SN I2S EEPROM)
  serialCommand.addCommand("ledon", ledOnCommand);                 // Turns Arduino Due onboard LED on
  serialCommand.addCommand("ledoff", ledOffCommand);               // Turns Arduino Due onboard LED off
  serialCommand.addCommand("boardledon", boardLedOnCommand);       // Turns HackEEG ADS1299 GPIO4 LED on
  serialCommand.addCommand("boardledoff", boardLedOffCommand);     // Turns HackEEG ADS1299 GPIO4 LED off
  serialCommand.addCommand("wakeup", wakeupCommand);               // Send the WAKEUP command
  serialCommand.addCommand("standby", standbyCommand);             // Send the STANDBY command
  serialCommand.addCommand("reset", resetCommand);                 // Reset the ADS1299
  serialCommand.addCommand("start", startCommand);                 // Send START command
  serialCommand.addCommand("stop", stopCommand);                   // Send STOP command
  serialCommand.addCommand("rdatac", rdatacCommand);               // Enter read data continuous mode, clear the ringbuffer, and read new data into the ringbuffer
  serialCommand.addCommand("sdatac", sdatacCommand);               // Stop read data continuous mode; ringbuffer data is still available
  serialCommand.addCommand("rdata", rdataCommand);                 // Read one sample of data from each active channel
  serialCommand.addCommand("rreg", readRegisterCommand);           // Read ADS129x register, argument in hex, print contents in hex
  serialCommand.addCommand("wreg", writeRegisterCommand);          // Write ADS129x register, arguments in hex
  serialCommand.addCommand("text", textCommand);                   // Sets the communication protocol to text
  serialCommand.addCommand("jsonlines", jsonlinesCommand);         // Sets the communication protocol to JSONLines
  serialCommand.addCommand("messagepack", messagepackCommand);     // Sets the communication protocol to MessagePack
  serialCommand.addCommand("base64", base64ModeOnCommand);         // RDATA commands send base64 encoded data - default
  serialCommand.addCommand("hex", hexModeOnCommand);               // RDATA commands send hex encoded data
  serialCommand.addCommand("help", helpCommand);                   // Print list of commands
  serialCommand.setDefaultHandler(unrecognized);                   // Handler for any command that isn't matched

  // Setup callbacks for JsonCommand commands
  jsonCommand.addCommand("nop", nopCommand);                       // No operation (does nothing)
  jsonCommand.addCommand("micros", microsCommand);                 // Returns number of microseconds since the program began executing
  jsonCommand.addCommand("version", versionCommand);               // Returns the driver version number
  jsonCommand.addCommand("status", statusCommand);                 // Returns the driver status
  jsonCommand.addCommand("serialnumber", serialNumberCommand);     // Returns the board serial number (UUID from the onboard 24AA256UID-I/SN I2S EEPROM)
  jsonCommand.addCommand("ledon", ledOnCommand);                   // Turns Arduino Due onboard LED on
  jsonCommand.addCommand("ledoff", ledOffCommand);                 // Turns Arduino Due onboard LED off
  jsonCommand.addCommand("boardledon", boardLedOnCommand);         // Turns HackEEG ADS1299 GPIO4 LED on
  jsonCommand.addCommand("boardledoff", boardLedOffCommand);       // Turns HackEEG ADS1299 GPIO4 LED off
  jsonCommand.addCommand("reset", resetCommand);                   // Reset the ADS1299
  jsonCommand.addCommand("start", startCommand);                   // Send START command
  jsonCommand.addCommand("stop", stopCommand);                     // Send STOP command
  jsonCommand.addCommand("rdatac", rdatacCommand);                 // Enter read data continuous mode, clear the ringbuffer, and read new data into the ringbuffer
  jsonCommand.addCommand("sdatac", sdatacCommand);                 // Stop read data continuous mode; ringbuffer data is still available
  jsonCommand.addCommand("rdata", rdataCommand);                   // Read one sample of data from each active channel
  jsonCommand.addCommand("rreg", readRegisterCommandDirect);       // Read ADS129x register
  jsonCommand.addCommand("wreg", writeRegisterCommandDirect);      // Write ADS129x register
  jsonCommand.addCommand("text", textCommand);                     // Sets the communication protocol to text
  jsonCommand.addCommand("jsonlines", jsonlinesCommand);           // Sets the communication protocol to JSONLines
  jsonCommand.addCommand("messagepack", messagepackCommand);       // Sets the communication protocol to MessagePack
  jsonCommand.setDefaultHandler(unrecognizedJsonLines);            // Handler for any command that isn't matched

  Serial.println("Ready");
}

void loop() {
  // put your main code here, to run repeatedly:
  switch(protocol_mode) {
    case TEXT_MODE:
      serialCommand.readSerial();
      break;
    case JSONLINES_MODE:
    case MESSAGEPACK_MODE:
      jsonCommand.readSerial();
      break;
    default:
      // do nothing
      ;
  }
  send_samples();
}


long hex_to_long(char *digits) {
  using namespace std;
  char *error;
  long n = strtol(digits, &error, 16);
  if (*error != 0) {
    return -1;  // error
  } else {
    return n;
  }
}

void output_hex_byte(int value) {
  int clipped = value & 0xff;
  char charValue[3];
  sprintf(charValue, "%02X", clipped);
  Serial.print(charValue);
}

void encode_hex(char *output, char *input, int input_len) {
  register int count = 0;
  for (register int i = 0; i < input_len; i++) {
    register uint8_t low_nybble = input[i] & 0x0f;
    register uint8_t highNybble = input[i] >> 4;
    output[count++] = hexDigits[highNybble];
    output[count++] = hexDigits[low_nybble];
  }
  output[count] = 0;
}

void send_response_ok() {
  send_response(RESPONSE_OK, STATUS_TEXT_OK);
}

void send_response_error() {
  send_response(RESPONSE_ERROR, STATUS_TEXT_ERROR);
}

void send_response(int status_code, const char *status_text) {
  switch (protocol_mode) {
    case TEXT_MODE:
      char response[128];
      sprintf(response, "%d %s", status_code, status_text);
      Serial.println(response);
      break;
    case JSONLINES_MODE:
      jsonCommand.sendJsonLinesResponse(status_code, (char *)status_text);
      break;
    case MESSAGEPACK_MODE:
      // all responses are in JSON Lines, MessagePack mode is only for sending samples
      jsonCommand.sendJsonLinesResponse(status_code, (char *)status_text);
      break;
    default:
      // unknown protocol
      ;
  }
}

void versionCommand(unsigned char unused1, unsigned char unused2) {
  send_response(RESPONSE_OK, driver_version);
}

void statusCommand(unsigned char unused1, unsigned char unused2) {
  detectActiveChannels();
  if (protocol_mode == TEXT_MODE) {
    Serial.println("200 Ok");
    Serial.print("Driver version: ");
    Serial.println(driver_version);
    Serial.print("Board name: ");
    Serial.println(board_name);
    Serial.print("Board maker: ");
    Serial.println(maker_name);
    Serial.print("Hardware type: ");
    Serial.println(hardware_type);
    Serial.print("Max channels: ");
    Serial.println(max_channels);
    Serial.print("Number of active channels: ");
    Serial.println(num_active_channels);
    Serial.println();
  } else {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = STATUS_OK;
    root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
    JsonObject status_info = root.createNestedObject(DATA_KEY);
    status_info["driver_version"] = driver_version;
    status_info["board_name"] = board_name;
    status_info["maker_name"] = maker_name;
    status_info["hardware_type"] = hardware_type;
    status_info["max_channels"] = max_channels;
    status_info["active_channels"] = num_active_channels;
    switch (protocol_mode) {
      case JSONLINES_MODE:
      case MESSAGEPACK_MODE:
        jsonCommand.sendJsonLinesDocResponse(doc);
        break;
      default:
        // unknown protocol
        ;
    }
  }
}

void nopCommand(unsigned char unused1, unsigned char unused2) {
  send_response_ok();
}

void microsCommand(unsigned char unused1, unsigned char unused2) {
  unsigned long microseconds = micros();
  if (protocol_mode == TEXT_MODE) {
    send_response_ok();
    Serial.println(microseconds);
    return;
  }
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  root[STATUS_CODE_KEY] = STATUS_OK;
  root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
  root[DATA_KEY] = microseconds;
  switch (protocol_mode) {
    case JSONLINES_MODE:
    case MESSAGEPACK_MODE:
      jsonCommand.sendJsonLinesDocResponse(doc);
      break;
    default:
      // unknown protocol
      ;
  }
}

void serialNumberCommand(unsigned char unused1, unsigned char unused2) {
  send_response(RESPONSE_NOT_IMPLEMENTED, STATUS_TEXT_NOT_IMPLEMENTED);
}

void textCommand(unsigned char unused1, unsigned char unused2) {
  protocol_mode = TEXT_MODE;
  send_response_ok();
}

void jsonlinesCommand(unsigned char unused1, unsigned char unused2) {
  protocol_mode = JSONLINES_MODE;
  send_response_ok();
}

void messagepackCommand(unsigned char unused1, unsigned char unused2) {
  protocol_mode = MESSAGEPACK_MODE;
  send_response_ok();
}

void ledOnCommand(unsigned char unused1, unsigned char unused2) {
  digitalWrite(PIN_LED, HIGH);
  send_response_ok();
}

void ledOffCommand(unsigned char unused1, unsigned char unused2) {
  digitalWrite(PIN_LED, LOW);
  send_response_ok();
}

void boardLedOnCommand(unsigned char unused1, unsigned char unused2) {
  int state = adcRreg(ADS129x::GPIO);
  state = state & 0xF7;
  state = state | 0x80;
  adcWreg(ADS129x::GPIO, state);
  send_response_ok();
}

void boardLedOffCommand(unsigned char unused1, unsigned char unused2) {
  int state = adcRreg(ADS129x::GPIO);
  state = state & 0x77;
  adcWreg(ADS129x::GPIO, state);
  send_response_ok();
}

void base64ModeOnCommand(unsigned char unused1, unsigned char unused2) {
  base64_mode = true;
  send_response(RESPONSE_OK, "Base64 mode on - rdata command will respond with base64 encoded data.");
}

void hexModeOnCommand(unsigned char unused1, unsigned char unused2) {
  base64_mode = false;
  send_response(RESPONSE_OK, "Hex mode on - rdata command will respond with hex encoded data");
}

void helpCommand(unsigned char unused1, unsigned char unused2) {
  if (protocol_mode == JSONLINES_MODE || protocol_mode == MESSAGEPACK_MODE) {
    send_response(RESPONSE_OK, "Help not available in JSON Lines or MessagePack modes.");
  } else {
    Serial.println("200 Ok");
    Serial.println("Available commands: ");
    serialCommand.printCommands();
    Serial.println();
  }
}

void readRegisterCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  char *arg1;
  arg1 = serialCommand.next();
  if (arg1 != NULL) {
    long registerNumber = hex_to_long(arg1);
    if (registerNumber >= 0) {
      int result = adcRreg(registerNumber);
      Serial.print("200 Ok");
      Serial.print(" (Read Register ");
      output_hex_byte(registerNumber);
      Serial.print(") ");
      Serial.println();
      output_hex_byte(result);
      Serial.println();
    } else {
      Serial.println("402 Error: expected hexidecimal digits.");
    }
  } else {
    Serial.println("403 Error: register argument missing.");
  }
  Serial.println();
}

void readRegisterCommandDirect(unsigned char register_number, unsigned char unused1) {
  using namespace ADS129x;
  if (register_number >= 0 and register_number <= 255) {
    unsigned char result = adcRreg(register_number);
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = STATUS_OK;
    root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
    root[DATA_KEY] = result;
    jsonCommand.sendJsonLinesDocResponse(doc);
  } else {
    send_response_error();
  }
}

void writeRegisterCommand(unsigned char unused1, unsigned char unused2) {
  char *arg1, *arg2;
  arg1 = serialCommand.next();
  arg2 = serialCommand.next();
  if (arg1 != NULL) {
    if (arg2 != NULL) {
      long registerNumber = hex_to_long(arg1);
      long registerValue = hex_to_long(arg2);
      if (registerNumber >= 0 && registerValue >= 0) {
        adcWreg(registerNumber, registerValue);
        Serial.print("200 Ok");
        Serial.print(" (Write Register ");
        output_hex_byte(registerNumber);
        Serial.print(" ");
        output_hex_byte(registerValue);
        Serial.print(") ");
        Serial.println();
      } else {
        Serial.println("402 Error: expected hexidecimal digits.");
      }
    } else {
      Serial.println("404 Error: value argument missing.");
    }
  } else {
    Serial.println("403 Error: register argument missing.");
  }
  Serial.println();
}

void writeRegisterCommandDirect(unsigned char register_number, unsigned char register_value) {
  if (register_number >= 0 && register_value >= 0) {
    adcWreg(register_number, register_value);
    send_response_ok();
  } else {
    send_response_error();
  }
}

void wakeupCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  adcSendCommand(WAKEUP);
  send_response_ok();
}

void standbyCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  adcSendCommand(STANDBY);
  send_response_ok();
}

void resetCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  adcSendCommand(RESET);
  //adsSetup();<------EDITADO
  spi_data_available = false;
  adcSendCommand(SDATAC);
  delay(100);  //pause to provide ads129n enough time to boot up...
  adcWreg(ADS129x::GPIO, 0);
  adcWreg(CONFIG3, PD_REFBUF | CONFIG3_const);
  send_response_ok();
}

void startCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  adcSendCommand(START);
  sample_number_union.sample_number = 0;
  send_response_ok();
}

void stopCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  adcSendCommand(STOP);
  send_response_ok();
}

void rdataCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  digitalWrite(PIN_START, HIGH);
  while (digitalRead(IPIN_DRDY) == HIGH);
  digitalWrite(PIN_START, LOW);
  //adcSendCommandLeaveCsActive(RDATA);
  adcSendCommand(RDATA);
  receive_sample();
  if (protocol_mode == TEXT_MODE) {
    send_response_ok();
  }
  send_sample();
  spi_data_available = false;
}

void rdatacCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  detectActiveChannels();
  if (num_active_channels > 0) {
    is_rdatac = true;
    adcSendCommand(RDATAC);
    send_response_ok();
  } else {
    send_response(RESPONSE_NO_ACTIVE_CHANNELS, STATUS_TEXT_NO_ACTIVE_CHANNELS);
  }
}

void sdatacCommand(unsigned char unused1, unsigned char unused2) {
  using namespace ADS129x;
  is_rdatac = false;
  adcSendCommand(SDATAC);
  using namespace ADS129x;
  send_response_ok();
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *command) {
  Serial.println("406 Error: Unrecognized command.");
  Serial.println();
}

// This gets set as the default handler for jsonlines and messagepack, and gets called when no other command matches.
void unrecognizedJsonLines(const char *command) {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  root[STATUS_CODE_KEY] = UNRECOGNIZED_COMMAND;
  root[STATUS_TEXT_KEY] = "Unrecognized command";
  jsonCommand.sendJsonLinesDocResponse(doc);
}

void detectActiveChannels() {                     //set device into RDATAC (continous) mode -it will stream data
  if ((is_rdatac) || (max_channels < 1)) {        //we can not read registers when in RDATAC mode
    //
  } else {
    // Serial.println("Detect active channels: ");
    using namespace ADS129x;
    num_active_channels = 0;
    for (uint8_t i = 1; i <= max_channels; i++) {
      delayMicroseconds(1);
      uint8_t chSet = adcRreg(CHnSET + i);
      active_channels[i] = ((chSet & 0x07) != SHORTED);
      if ((chSet & 0x07) != SHORTED) num_active_channels++;
    }
  }
}

void IRAM_ATTR drdy_interrupt() {
  spi_data_available = true;
}

inline void send_samples(void) {
  if (!is_rdatac) return;
  if (spi_data_available) {
    spi_data_available = false;
    receive_sample();
    if (sample_number_union.sample_number > 0)
      send_sample();
    sample_number_union.sample_number++;
  }
}

inline void receive_sample() {
  digitalWrite(PIN_CS, LOW);
  delayMicroseconds(10);
  memset(spi_bytes, 0, sizeof(spi_bytes));
  timestamp_union.timestamp = micros();
  spi_bytes[0] = timestamp_union.timestamp_bytes[0];
  spi_bytes[1] = timestamp_union.timestamp_bytes[1];
  spi_bytes[2] = timestamp_union.timestamp_bytes[2];
  spi_bytes[3] = timestamp_union.timestamp_bytes[3];
  spi_bytes[4] = sample_number_union.sample_number_bytes[0];
  spi_bytes[5] = sample_number_union.sample_number_bytes[1];
  spi_bytes[6] = sample_number_union.sample_number_bytes[2];
  spi_bytes[7] = sample_number_union.sample_number_bytes[3];

  uint8_t returnCode = spiRec(spi_bytes + TIMESTAMP_SIZE_IN_BYTES + SAMPLE_NUMBER_SIZE_IN_BYTES, num_spi_bytes);

  digitalWrite(PIN_CS, HIGH);
}

inline void send_sample(void) {
  switch (protocol_mode) {
    case JSONLINES_MODE:
      Serial.write(json_rdatac_header);
      base64_encodes(output_buffer, (char *)spi_bytes, num_timestamped_spi_bytes);
      Serial.write(output_buffer);
      Serial.write(json_rdatac_footer);
      Serial.write("\n");
      break;
    case TEXT_MODE:
      if (base64_mode) {
        base64_encodes(output_buffer, (char *)spi_bytes, num_timestamped_spi_bytes);
      } else {
        encode_hex(output_buffer, (char *)spi_bytes, num_timestamped_spi_bytes);
      }
      Serial.println(output_buffer);
      break;
    case MESSAGEPACK_MODE:
      send_sample_messagepack(num_timestamped_spi_bytes);
      break;
  }
}

inline void send_sample_json(int num_bytes) {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  root[STATUS_CODE_KEY] = STATUS_OK;
  root[STATUS_TEXT_KEY] = STATUS_TEXT_OK;
  JsonArray data = root.createNestedArray(DATA_KEY);
  copyArray(spi_bytes, num_bytes, data);
  jsonCommand.sendJsonLinesDocResponse(doc);
}

inline void send_sample_messagepack(int num_bytes) {
  Serial.write(messagepack_rdatac_header, messagepack_rdatac_header_size);
  Serial.write((uint8_t)num_bytes);
  Serial.write(spi_bytes, num_bytes);
}

void arduinoSetup() {
  using namespace ADS129x;
  // prepare pins to be outputs or inputs en adsCommand.h
  pinMode(PIN_START, OUTPUT);
  pinMode(IPIN_DRDY, INPUT);
  // pinMode(PIN_CLKSEL, OUTPUT);// *optional DVDD en OpenBCI
  pinMode(IPIN_PWDN, OUTPUT);   // *optional
  pinMode(IPIN_RESET, OUTPUT);  // *optional
  // digitalWrite(PIN_CLKSEL, HIGH); // internal clock
  //start Serial Peripheral Interface
  spiBegin(PIN_CS);
  spiInit(MSBFIRST, SPI_MODE1, SPI_CLOCK_DIVIDER);  // SPI_MODE1: Pag 60
  //Start ADS1298
  digitalWrite(IPIN_PWDN, HIGH);  // *optional - turn off power down mode
  digitalWrite(IPIN_RESET, HIGH);
  //wait for the ads129n to be ready - it can take a while to charge caps
  delay(5000);  // 100ms delay for power-on-reset (Datasheet, pg. 48)
  // reset pulse
  digitalWrite(IPIN_RESET, LOW);
  delayMicroseconds(6);  // Wait for 2 tCLKs Internal CLK=2048MHz->0.488us
  digitalWrite(IPIN_RESET, HIGH);
  delay(1);  // *optional Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisecond
}

void adsSetup() {  //default settings for ADS1298 and compatible chips
  using namespace ADS129x;
  // Send SDATAC Command (Stop Read Data Continuously mode)
  spi_data_available = false;
  attachInterrupt(digitalPinToInterrupt(IPIN_DRDY), drdy_interrupt, FALLING);
  adcSendCommand(SDATAC);
  delay(100);  //pause to provide ads129n enough time to boot up...
  // delayMicroseconds(2);
  uint8_t val = adcRreg(ID);
  switch (val & B00011111) {
    case B10000:
      hardware_type = "ADS1294";
      max_channels = 4;
      break;
    case B10001:
      hardware_type = "ADS1296";
      max_channels = 6;
      break;
    case B10010:
      hardware_type = "ADS1298";
      max_channels = 8;
      break;
    case B11110:
      hardware_type = "ADS1299";
      max_channels = 8;
      break;
    case B11100:
      hardware_type = "ADS1299-4";
      max_channels = 4;
      break;
    case B11101:
      hardware_type = "ADS1299-6";
      max_channels = 6;
      break;
    default:
      max_channels = 0;
  }
  num_spi_bytes = (3 * (max_channels + 1));  //24-bits header plus 24-bits per channel
  num_timestamped_spi_bytes = num_spi_bytes + TIMESTAMP_SIZE_IN_BYTES + SAMPLE_NUMBER_SIZE_IN_BYTES;
  if (max_channels == 0) {  //error mode
    while (1) {
      digitalWrite(PIN_LED, HIGH);
      delay(500);
      digitalWrite(PIN_LED, LOW);
      delay(500);
    }
  }  //error mode
  // All GPIO set to output 0x0000: (floating CMOS inputs can flicker on and off, creating noise)
  adcWreg(ADS129x::GPIO, 0);
  adcWreg(CONFIG3, PD_REFBUF | CONFIG3_const); // Enable Internal Reference
  // digitalWrite(PIN_START, HIGH);
  // Power-down and Short all Channels
  // for (uint8_t ch = 1; ch <= max_channels; ch++) {
  //   delayMicroseconds(1);
  //   adcWreg(CHnSET + ch, PDn | SHORTED);
  // }
}

/*******************Base64.h CODES*******************************/

int base64_encodes(char *output, char *input, int inputLen) {
  int i = 0, j = 0;
  int encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while (inputLen--) {
    a3[i++] = *(input++);
    if (i == 3) {
      a3_to_a4s(a4, a3);

      for (i = 0; i < 4; i++) {
        output[encLen++] = b64_alphabets[a4[i]];
      }

      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++) {
      a3[j] = '\0';
    }

    a3_to_a4s(a4, a3);

    for (j = 0; j < i + 1; j++) {
      output[encLen++] = b64_alphabets[a4[j]];
    }

    while ((i++ < 3)) {
      output[encLen++] = '=';
    }
  }
  output[encLen] = '\0';
  return encLen;
}

void a3_to_a4s(unsigned char *a4, unsigned char *a3) {
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}
/*
   PROJECT: LoRa Morse Messenger (2026)
   HARDWARE: Integrated ESP32-C3 SUPERMINI + Ra-02 (433MHz)
   LIBRARY: LiquidCrystal_I2C by Frank de Brabander

   ELECTRICAL CONNECTIONS:
   1. LoRa Radio (Internal): CS=D7, RST=D10, DIO0=D1
   2. 20x4 LCD (External): SDA=8, SCL=9, 4-WAY JST WITH POWER AND I2C
   3. Status LEDs (External): TX_LED=D2, RX_LED=D3, ENABLE OR DISABLE LEDS
   
   DEBUG VERSION: Enhanced serial output for breadboard testing
*/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PIN DEFINITIONS ---
#define LORA_CS    7
#define LORA_RST   10
#define LORA_DIO0  1
#define TX_LED     2
#define RX_LED     3

// --- CONSTANTS ---
const unsigned long LED_DUR = 300;
const long BAND = 433E6;
const int MAX_SERIAL_BUF = 100;  // Prevent buffer overflow
const int MAX_MESSAGE_LEN = 256; // Max message length

// --- MORSE LOOKUP TABLE (Stored in Flash Memory) ---
const char* const morse_alpha[] PROGMEM = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
  "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."
};

// --- OBJECTS ---
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- NON-BLOCKING GLOBALS ---
String serialBuf = "";
unsigned long txTime = 0;
unsigned long rxTime = 0;
unsigned long lastDebugPrint = 0;

// --- STATISTICS FOR DEBUGGING ---
unsigned int txCount = 0;
unsigned int rxCount = 0;
unsigned int decodeErrors = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("\n\n========================================"));
  Serial.println(F("LoRa Morse Messenger - DEBUG BUILD"));
  Serial.println(F("========================================"));
  Serial.println(F("Waiting 5 seconds for serial connection..."));
  delay(5000);
  
  // --- PIN INITIALIZATION ---
  Serial.println(F("[SETUP] Configuring GPIO pins..."));
  pinMode(TX_LED, OUTPUT);
  pinMode(RX_LED, OUTPUT);
  digitalWrite(TX_LED, HIGH);
  digitalWrite(RX_LED, HIGH);
  Serial.println(F("[SETUP] GPIO configured: TX_LED=2, RX_LED=3"));

  // --- LED TEST ---
  Serial.println(F("[SETUP] Testing LEDs..."));
  digitalWrite(TX_LED, LOW);
  delay(200);
  digitalWrite(TX_LED, HIGH);
  digitalWrite(RX_LED, LOW);
  delay(200);
  digitalWrite(RX_LED, HIGH);
  Serial.println(F("[SETUP] LED test complete"));

  // --- LCD INITIALIZATION ---
  Serial.println(F("[SETUP] Initializing I2C LCD..."));
  Serial.println(F("[SETUP] I2C Pins: SDA=8, SCL=9 (default)"));
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("LORA MORSE SYSTEM"));
  lcd.setCursor(0, 1);
  lcd.print(F("Initializing..."));
  Serial.println(F("[SETUP] LCD initialized at 0x27"));

  // --- LORA INITIALIZATION ---
  Serial.println(F("[SETUP] Configuring LoRa module..."));
  Serial.print(F("[SETUP] CS="));
  Serial.print(LORA_CS);
  Serial.print(F(", RST="));
  Serial.print(LORA_RST);
  Serial.print(F(", DIO0="));
  Serial.println(LORA_DIO0);
  
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(BAND)) {
    Serial.println(F("[ERROR] LoRa initialization FAILED!"));
    Serial.println(F("[ERROR] Check wiring and power to RA-02"));
    lcd.setCursor(0, 1);
    lcd.print(F("RADIO INIT FAIL     "));
    lcd.setCursor(0, 2);
    lcd.print(F("Check connections   "));
    while (1) {
      // Blink TX LED to indicate error
      digitalWrite(TX_LED, !digitalRead(TX_LED));
      delay(500);
    }
  }

  Serial.println(F("[SETUP] LoRa initialized successfully!"));
  Serial.print(F("[SETUP] Frequency: "));
  Serial.print(BAND / 1E6);
  Serial.println(F(" MHz"));

  // --- LORA PARAMETERS ---
  Serial.println(F("[SETUP] Configuring LoRa parameters..."));
  LoRa.setSpreadingFactor(7);
  Serial.println(F("[SETUP] Spreading Factor: 7"));
  LoRa.setSignalBandwidth(125E3);
  Serial.println(F("[SETUP] Bandwidth: 125 kHz"));
  LoRa.setCodingRate4(5);
  Serial.println(F("[SETUP] Coding Rate: 4/5"));

  // --- FINAL STATUS ---
  lcd.setCursor(0, 1);
  lcd.print(F("                    "));
  lcd.setCursor(0, 3);
  lcd.print(F("STATUS: READY       "));
  
  Serial.println(F("[SETUP] ========================================"));
  Serial.println(F("[SETUP] System Ready!"));
  Serial.println(F("[SETUP] ========================================"));
  Serial.println(F("[INFO] Type text and press ENTER to transmit"));
  Serial.println(F("[INFO] Morse encoding: A-Z, spaces as '/'"));
  Serial.println(F("[INFO] Max message length: 100 characters"));
  Serial.println(F(""));
}

void loop() {
  handleSerialInput();
  handleIncomingLora();
  manageStatusLeds();
  
  // Periodic heartbeat for debugging
  if (millis() - lastDebugPrint > 30000) {  // Every 30 seconds
    printStatistics();
    lastDebugPrint = millis();
  }
}

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialBuf.length() > 0) {
        Serial.print(F("[INPUT] Received: \""));
        Serial.print(serialBuf);
        Serial.println(F("\""));
        transmitMorse(serialBuf);
        serialBuf = "";
      }
    } else if (serialBuf.length() < MAX_SERIAL_BUF) {
      serialBuf += c;
      // Echo character for feedback
      Serial.print(c);
    } else {
      // Buffer full warning
      Serial.println(F("\n[WARN] Input buffer full! Message truncated."));
      transmitMorse(serialBuf);
      serialBuf = "";
    }
  }
}

void transmitMorse(String text) {
  Serial.println(F("[TX] ===== TRANSMISSION START ====="));
  Serial.print(F("[TX] Original text: \""));
  Serial.print(text);
  Serial.println(F("\""));
  
  // Pre-allocate string to reduce fragmentation
  String out = "";
  out.reserve(text.length() * 6);  // Worst case: 6 chars per letter
  
  text.toUpperCase();
  Serial.print(F("[TX] Uppercase: \""));
  Serial.print(text);
  Serial.println(F("\""));

  int charCount = 0;
  int unknownChars = 0;

  for (int i = 0; i < text.length(); i++) {
    if (text[i] >= 'A' && text[i] <= 'Z') {
      char buffer[10];
      strcpy_P(buffer, (char*)pgm_read_ptr(&(morse_alpha[text[i] - 'A'])));
      out += buffer;
      out += " ";
      charCount++;
      
      // Debug: Show each character conversion
      Serial.print(F("[TX] '"));
      Serial.print(text[i]);
      Serial.print(F("' -> "));
      Serial.println(buffer);
      
    } else if (text[i] == ' ') {
      out += "/ ";
      Serial.println(F("[TX] ' ' -> / (word space)"));
    } else {
      unknownChars++;
      Serial.print(F("[WARN] Unknown character: '"));
      Serial.print(text[i]);
      Serial.print(F("' (ASCII: "));
      Serial.print((int)text[i]);
      Serial.println(F(") - skipped"));
    }
  }

  Serial.print(F("[TX] Morse output: \""));
  Serial.print(out);
  Serial.println(F("\""));
  Serial.print(F("[TX] Characters encoded: "));
  Serial.print(charCount);
  Serial.print(F(", Unknown/skipped: "));
  Serial.println(unknownChars);
  Serial.print(F("[TX] Morse length: "));
  Serial.print(out.length());
  Serial.println(F(" chars"));

  // Non-blocking TX indicator (active-low)
  digitalWrite(TX_LED, LOW);
  txTime = millis();

  // Transmit
  Serial.println(F("[TX] Starting LoRa transmission..."));
  LoRa.beginPacket();
  LoRa.print(out);
  LoRa.endPacket();
  txCount++;
  Serial.println(F("[TX] Transmission complete!"));
  Serial.println(F("[TX] ===== TRANSMISSION END =====\n"));

  // LCD Updates
  lcd.setCursor(0, 1);
  lcd.print(F("TX:                 "));
  lcd.setCursor(4, 1);
  lcd.print(text.substring(0, 16));
  lcd.setCursor(0, 2);
  lcd.print(F("                    "));
  lcd.setCursor(0, 2);
  lcd.print(out.substring(0, 20));
}

void handleIncomingLora() {
  int pSize = LoRa.parsePacket();
  
  if (pSize) {
    Serial.println(F("[RX] ===== RECEPTION START ====="));
    Serial.print(F("[RX] Packet size: "));
    Serial.print(pSize);
    Serial.println(F(" bytes"));
    
    // Non-blocking RX indicator
    digitalWrite(RX_LED, LOW);
    rxTime = millis();

    // Read incoming data into fixed buffer
    char incoming[MAX_MESSAGE_LEN];
    int idx = 0;

    while (LoRa.available() && idx < MAX_MESSAGE_LEN - 1) {
      incoming[idx++] = (char)LoRa.read();
    }
    incoming[idx] = '\0';  // Null terminate
    rxCount++;

    Serial.print(F("[RX] Raw morse: \""));
    Serial.print(incoming);
    Serial.println(F("\""));

    // Decode morse to text
    Serial.println(F("[RX] Decoding morse..."));
    String decoded = decodeMorse(String(incoming));
    Serial.print(F("[RX] Decoded text: \""));
    Serial.print(decoded);
    Serial.println(F("\""));

    // Get RSSI
    int rssi = LoRa.packetRssi();
    Serial.print(F("[RX] RSSI: "));
    Serial.print(rssi);
    Serial.println(F(" dBm"));

    // Get SNR
    float snr = LoRa.packetSnr();
    Serial.print(F("[RX] SNR: "));
    Serial.print(snr);
    Serial.println(F(" dB"));

    // Update LCD - Line 1: Decoded text
    lcd.setCursor(0, 1);
    lcd.print(F("RX:                 "));
    lcd.setCursor(4, 1);
    lcd.print(decoded.substring(0, 16));

    // Update LCD - Line 2: Morse code
    lcd.setCursor(0, 2);
    lcd.print(F("                    "));
    lcd.setCursor(0, 2);
    for (int i = 0; i < 20 && incoming[i] != '\0'; i++) {
      lcd.print(incoming[i]);
    }

    // Update LCD - Line 3: RSSI
    lcd.setCursor(0, 3);
    lcd.print(F("RSSI:"));
    lcd.print(rssi);
    lcd.print(F("dBm SNR:"));
    lcd.print(snr, 1);
    lcd.print(F("  "));

    Serial.println(F("[RX] ===== RECEPTION END =====\n"));
  }
}

String decodeMorse(String morse) {
  String decoded = "";
  String symbol = "";
  int symbolsDecoded = 0;
  int symbolsFailed = 0;

  Serial.println(F("[DECODE] Starting morse decode..."));

  for (int i = 0; i < morse.length(); i++) {
    if (morse[i] == ' ') {
      if (symbol.length() > 0) {
        // Look up the symbol
        bool found = false;
        for (int j = 0; j < 26; j++) {
          char buffer[10];
          strcpy_P(buffer, (char*)pgm_read_ptr(&(morse_alpha[j])));
          if (symbol.equals(buffer)) {
            char letter = 'A' + j;
            decoded += letter;
            symbolsDecoded++;
            found = true;
            Serial.print(F("[DECODE] \""));
            Serial.print(symbol);
            Serial.print(F("\" -> '"));
            Serial.print(letter);
            Serial.println(F("'"));
            break;
          }
        }
        if (!found) {
          decoded += '?';
          symbolsFailed++;
          decodeErrors++;
          Serial.print(F("[DECODE] UNKNOWN: \""));
          Serial.print(symbol);
          Serial.println(F("\" -> '?'"));
        }
        symbol = "";
      }
    } else if (morse[i] == '/') {
      decoded += ' ';  // Word space
      Serial.println(F("[DECODE] '/' -> ' ' (word space)"));
      i++;  // Skip the space after /
    } else {
      symbol += morse[i];
    }
  }

  // Handle last symbol if no trailing space
  if (symbol.length() > 0) {
    bool found = false;
    for (int j = 0; j < 26; j++) {
      char buffer[10];
      strcpy_P(buffer, (char*)pgm_read_ptr(&(morse_alpha[j])));
      if (symbol.equals(buffer)) {
        char letter = 'A' + j;
        decoded += letter;
        symbolsDecoded++;
        found = true;
        Serial.print(F("[DECODE] \""));
        Serial.print(symbol);
        Serial.print(F("\" -> '"));
        Serial.print(letter);
        Serial.println(F("'"));
        break;
      }
    }
    if (!found) {
      decoded += '?';
      symbolsFailed++;
      decodeErrors++;
      Serial.print(F("[DECODE] UNKNOWN: \""));
      Serial.print(symbol);
      Serial.println(F("\" -> '?'"));
    }
  }

  Serial.print(F("[DECODE] Summary: "));
  Serial.print(symbolsDecoded);
  Serial.print(F(" decoded, "));
  Serial.print(symbolsFailed);
  Serial.println(F(" failed"));

  return decoded;
}

void manageStatusLeds() {
  if (millis() - txTime > LED_DUR) {
    digitalWrite(TX_LED, HIGH);
  }
  if (millis() - rxTime > LED_DUR) {
    digitalWrite(RX_LED, HIGH);
  }
}

void printStatistics() {
  Serial.println(F("\n[STATS] ===== SYSTEM STATISTICS ====="));
  Serial.print(F("[STATS] Uptime: "));
  Serial.print(millis() / 1000);
  Serial.println(F(" seconds"));
  Serial.print(F("[STATS] Messages transmitted: "));
  Serial.println(txCount);
  Serial.print(F("[STATS] Messages received: "));
  Serial.println(rxCount);
  Serial.print(F("[STATS] Decode errors: "));
  Serial.println(decodeErrors);
  Serial.print(F("[STATS] Free heap: "));
  Serial.print(ESP.getFreeHeap());
  Serial.println(F(" bytes"));
  Serial.println(F("[STATS] ===================================\n"));
}

// === ESP32 I2C Terminal with Contiguous and Single Byte Access Modes ===
#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_SPEED 10000
#define I2C_ADDRESS 0x77

#define EEPROM_KEY 0xA5
#define STATUS_ADDRESS 0x22

#define SOFT_RESET_VALUE 0xAB
#define SOFT_RESET_VALUE_ADDRESS 0x1A

#define PROTECTED_KEY_ADDRESS 0x1B
#define PROTECTED_REGISTERS_KEY 0x4B
#define PROTECTED_KEY_SOFT_RESET_VALUE 0xE2

enum State {
  SELECT_OPERATION,
  ENTER_ADDRESS,
  ENTER_LENGTH,
  ENTER_VALUE
};

State currentState = SELECT_OPERATION;
uint8_t regAddress = 0;
uint8_t byteCount = 1;
uint8_t valuesReceived = 0;
String inputBuffer = "";
int operation = 0;
uint8_t txdata[32];

const char *operationNames[] = {
  "",  // index 0 boş
  "Single Register Read",
  "Contiguous Register Read",
  "Single Register Write",
  "Contiguous Register Write",
  "EEPROM Single Read",
  "EEPROM Contiguous Read",
  "EEPROM Single Write",
  "EEPROM Contiguous Write",
  "STATUS Register Read",
  "SOFT RESET"
};

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, I2C_SPEED);
  printMenu();
}

void loop() {
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        processInput(inputBuffer);
      }
      inputBuffer = "";
    } else {
      inputBuffer += ch;
    }
  }
}

void processInput(String input) {
  switch (currentState) {
    case SELECT_OPERATION:
      operation = input.toInt();
      if (operation >= 1 && operation <= 10) {
        Serial.print("\nSeçiminizi yaptınız: ");
        Serial.println(operationNames[operation]);
        if (operation == 9 || operation == 10 ) {
          executeOperation();
          break;
        }
        Serial.println("Register adresini HEX olarak girin:");
        currentState = ENTER_ADDRESS;
      } else {
        Serial.println("\nGeçersiz seçim.\n");
        printMenu();
      }
      break;

    case ENTER_ADDRESS:
      regAddress = (uint8_t)strtol(input.c_str(), NULL, 16);
      if (operation == 2 || operation == 4 || operation == 6 || operation == 8) {
        Serial.println("Veri uzunluğunu girin (1-32):");
        currentState = ENTER_LENGTH;
      } else if (operation == 3 || operation == 7) {
        Serial.println("Yazılacak değeri HEX olarak girin:");
        currentState = ENTER_VALUE;
      } else {
        executeOperation();
      }
      break;

    case ENTER_LENGTH:
      byteCount = input.toInt();
      if (byteCount < 1 || byteCount > 32) {
        Serial.println("Geçersiz uzunluk. 1-32 aralığında olmalı.");
        printMenu();
        currentState = SELECT_OPERATION;
        return;
      }
      if (operation == 4 || operation == 8) {
        Serial.println("HEX formatta değerleri girin:");
        valuesReceived = 0;
        currentState = ENTER_VALUE;
      } else {
        executeOperation();
      }
      break;

    case ENTER_VALUE:
      if (operation == 4 || operation == 8) {
        // Giriş tek satırda girilmiş olacak: "01 05 AE 3B"
        input.trim();
        valuesReceived = 0;

        // Parçala ve her bir hex değeri txdata dizisine koy
        int startIdx = 0;
        while (startIdx < input.length()) {
          int spaceIdx = input.indexOf(' ', startIdx);
          if (spaceIdx == -1) spaceIdx = input.length();
          String token = input.substring(startIdx, spaceIdx);
          token.trim();

          if (token.length() > 0) {
            txdata[valuesReceived++] = (uint8_t)strtol(token.c_str(), NULL, 16);
          }

          startIdx = spaceIdx + 1;
        }

        // Uzunluk kontrolü
        if (valuesReceived != byteCount) {
          Serial.print("Hata: ");
          Serial.print(byteCount);
          Serial.print(" bayt bekleniyordu, ancak ");
          Serial.print(valuesReceived);
          Serial.println(" bayt girildi.");
          printMenu();
          currentState = SELECT_OPERATION;
        } else {
          executeOperation();
        }
      } else {
        // Tek baytlı değer okuma (Single Write için)
        txdata[valuesReceived++] = (uint8_t)strtol(input.c_str(), NULL, 16);
        if (valuesReceived >= byteCount) {
          executeOperation();
        } else {
          Serial.print("Bayt ");
          Serial.print(valuesReceived + 1);
          Serial.print(" : ");
        }
      }
      break;

      break;
  }
}

void printMenu() {
  Serial.println("\n=== I2C Terminal Menu ===");
  Serial.println("1 - Single Register Read");
  Serial.println("2 - Contiguous Register Read");
  Serial.println("3 - Single Register Write");
  Serial.println("4 - Contiguous Register Write");
  Serial.println("5 - EEPROM Single Read");
  Serial.println("6 - EEPROM Contiguous Read");
  Serial.println("7 - EEPROM Single Write");
  Serial.println("8 - EEPROM Contiguous Write");
  Serial.println("9 - Status Register Read");
  Serial.println("10 - Soft Reset");
  Serial.print("\nSeçiminizi yapın: ");
  currentState = SELECT_OPERATION;
}

void executeOperation() {
  switch (operation) {
    case 1: readRegisterSingle(regAddress); break;
    case 2: readRegisterContiguous(regAddress, byteCount); break;
    case 3: writeRegisterSingle(regAddress, txdata[0]); break;
    case 4: writeRegisterContiguous(regAddress, txdata, byteCount); break;
    case 5: readE2PSingle(regAddress); break;
    case 6: readE2PContiguous(regAddress, byteCount); break;
    case 7: writeE2PSingle(regAddress, txdata[0]); break;
    case 8: writeE2PContiguous(regAddress, txdata, byteCount); break;
    case 9: i2c8500_read_status_cmd(STATUS_ADDRESS); break;
    case 10: i2c8500_soft_reset_cmd(); break;
  }
  printMenu();
}

// === Placeholder Fonksiyonlar ===

void i2c8500_soft_reset_cmd(void) {
  Serial.println("softReset() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(PROTECTED_KEY_ADDRESS);           // Protection key register
  Wire.write(PROTECTED_KEY_SOFT_RESET_VALUE);  // Protection value
  Wire.write(SOFT_RESET_VALUE_ADDRESS);        // Target register address
  Wire.write(SOFT_RESET_VALUE);                // Value to write
  Wire.endTransmission();                      // Stop condition
}

void i2c8500_read_status_cmd(uint8_t reg) {

  Serial.println("statusRegister() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(STATUS_ADDRESS);   // Address to read
  Wire.endTransmission(false);  // Repeated start
  Wire.requestFrom(I2C_ADDRESS, 1);

  if (Wire.available()) {
    uint8_t val = Wire.read();
    Serial.print("Register 0x");
    Serial.print(reg, HEX);
    Serial.print(" = 0x");
    Serial.println(val, HEX);
  }
}

void readRegisterSingle(uint8_t reg) {
  // TODO: Implement
  Serial.println("readRegisterSingle() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(reg);              // Address to read
  Wire.endTransmission(false);  // Repeated start
  Wire.requestFrom(I2C_ADDRESS, 1);

  if (Wire.available()) {
    uint8_t val = Wire.read();
    Serial.print("Register 0x");
    Serial.print(reg, HEX);
    Serial.print(" = 0x");
    Serial.println(val, HEX);
  }
}

void readRegisterContiguous(uint8_t reg, uint8_t len) {
  // TODO: Implement
  Serial.println("readRegisterContiguous() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(reg);  // Start register
  Wire.endTransmission(false);

  Wire.requestFrom(I2C_ADDRESS, len);
  Serial.print("Read registers starting from 0x");
  Serial.print(reg, HEX);
  Serial.print(": ");

  for (uint8_t i = 0; i < len && Wire.available(); i++) {
    uint8_t val = Wire.read();
    Serial.print("0x");
    Serial.print(val, HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void writeRegisterSingle(uint8_t reg, uint8_t value) {
  // TODO: Implement
  Serial.println("writeRegisterSingle() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(PROTECTED_KEY_ADDRESS);        // Protection key register
  Wire.write(PROTECTED_REGISTERS_KEY);        // Protection value
  Wire.write(reg);         // Target register address
  Wire.write(value);       // Value to write
  Wire.endTransmission();  // Stop condition
}

void writeRegisterContiguous(uint8_t reg, uint8_t *data, uint8_t len) {
  // TODO: Implement
  Serial.println("writeRegisterContiguous() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(PROTECTED_KEY_ADDRESS);  // Protection key register
  Wire.write(PROTECTED_REGISTERS_KEY);  // Protection value

  for (uint8_t i = 0; i < len; i++) {
    Wire.write(reg + i);  // Each register address
    Wire.write(data[i]);  // Corresponding value
  }
  Wire.endTransmission();  // Stop condition
}

void readE2PSingle(uint8_t reg) {
  // TODO: Implement
  Serial.println("readE2PSingle() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(reg);         // EEPROM register address
  Wire.endTransmission();  // Stop condition

  delay(10);  // T_ee_rd: EEPROM internal delay before read

  Wire.requestFrom(I2C_ADDRESS, 1);  // Repeated start + read
  if (Wire.available()) {
    uint8_t val = Wire.read();
    Serial.print("Read: 0x");
    Serial.println(val, HEX);
  } else {
    Serial.println("EEPROM read failed");
  }
}

void readE2PContiguous(uint8_t reg, uint8_t len) {
  Serial.print("EEPROM'dan çoklu okuma, başlangıç adresi 0x");
  Serial.print(reg, HEX);
  Serial.println(":");

  for (uint8_t i = 0; i < len; i++) {
    // 1. Write mode ile EEPROM register adresini ayarla
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg + i);     // Her seferinde bir üst adrese git
    Wire.endTransmission();  // STOP condition

    delay(2);

    // 2. Read mode ile tek bayt oku
    Wire.requestFrom(I2C_ADDRESS, (uint8_t)1);

    if (Wire.available()) {
      uint8_t val = Wire.read();
      Serial.print("0x");
      Serial.print(val, HEX);
      Serial.print(" ");
    } else {
      Serial.print("?? ");
    }
  }

  Serial.println();
}

void writeE2PSingle(uint8_t reg, uint8_t value) {
  Serial.println("writeE2PSingle() called");
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(PROTECTED_KEY_ADDRESS);        // Protection key register address
  Wire.write(EEPROM_KEY);        // EEPROM write enable key
  Wire.write(reg);         // EEPROM register address
  Wire.write(value);       // Data to write
  Wire.endTransmission();  // Stop condition
}

void writeE2PContiguous(uint8_t reg, uint8_t *data, uint8_t len) {
  
  Serial.println("writeE2PContiguous() called");
  for (uint8_t i = 0; i < len; i++) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(PROTECTED_KEY_ADDRESS);        // Protection key register address
    Wire.write(EEPROM_KEY);        // EEPROM write enable key
    Wire.write(reg + i);     // Incremental EEPROM address
    Wire.write(data[i]);     // Data to write
    Wire.endTransmission();  // Stop condition
    delay(10);               // T_ee_wr delay after each byte
  }
}
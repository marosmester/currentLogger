#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "src/Adafruit_INA219/Adafruit_INA219.h"
#include "src/Adafruit_GFX_Library/Adafruit_GFX.h"
#include "src/Adafruit_SSD1306/Adafruit_SSD1306.h"

// Serial 
#define WAIT_TIME 2 // seconds
#define WAIT_TIME_SERIAL 5

// OLED 128x64 display:
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C /// see datasheet for Address; usually 0x3D or 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Current sensors:
Adafruit_INA219 ina1(0x40);   // write it like this, with I2C address in the brackets
Adafruit_INA219 ina2(0x41);

// SD card:
/*
 SD card attached to SPI bus on Arduino Mega as follows:
 ** MISO - pin 50
 ** MOSI - pin 51
 ** CLK - pin 52
 ** CS - pin 53 
*/
#define MAX_SIZE 2147483648UL  // exact value of 2 GB in Bytes

/*
Genreal notes:
- unsigned long variable "time" (32bit) will overflow after 136 years (so its okay)
- it must be ensured hardware-wise that the 3 I2C devices all have unique addresses - especially the 2 current sensors.
- the macro MAX_SIZE doesn't really solve the edge case when SD card is almost full, it just limits the .csv file size
*/

String fname; // current file name

void setup() {
  delay(WAIT_TIME*1000);

  // Initialize Serial
  Serial.begin(115200);   // Only for potential debugging, Serial isn't required to solve the task
  int cnt = 0;
  while (!Serial && cnt < WAIT_TIME_SERIAL) {
    delay(1000);
    cnt++;
    Serial.begin(115200); 
  }
  if (Serial) Serial.println("Serial initialized.");

  // Initialize the OLED display
  while (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {      // <------------- try swicthing SSD1306_EXTERNALVCC to SSD1306_SWITCHCAPVCC if OLED doesn't light up
    Serial.println("OLED display: SSD1306 allocation failed ... retrying");
    delay(500);
  }
  Serial.println("OLED diplay initialized");  

  // Initialize the two INA219
  while (!ina1.begin()) {
    Serial.println("Failed to find INA219 chip: ina1 ... retrying");
    delay(500); //ms
  }
  while (! ina2.begin()) {
    Serial.println("Failed to find INA219 chip: ina2 ... retrying");
    delay(500);
  }
  Serial.println("Both sensors are initialized.");
  ina1.setCalibration_16V_400mA();
  ina2.setCalibration_16V_400mA();    

  // Initialize SD card:
  while (!SD.begin(53)) {  // CS pin
    Serial.println("SD card initialization failed ... retrying");
    delay(500);
  }
  Serial.println("SD card initialization done.");
  
  // Scan the SD card for the filename with the highest number:
  File root = SD.open("/");
  if (root) {
    Serial.println("SD card root dir opened successfullly.");
    unsigned int new_num = highestFileNumber(root) + 1;
    Serial.print("new file number= ");
    Serial.println(new_num);
    fname = "DATA" + String(new_num) + ".csv";
  } else {
    Serial.println("Failed to open SD card root dir.");
  }
  
  // Open a new csv file
  File dataFile = SD.open(fname, FILE_WRITE);   // FILE_WRITE opens the file for appending
  if (dataFile) {     // if the file opened okay 
    Serial.println("CSV file opened successfully.");
    dataFile.println("Time,Current_1,Current_2,Voltage_1,Voltage_2");
    dataFile.close();
  } else {
    Serial.println("Failed to open CSV file.");
  }

  // setup internal LED as output
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println();
}

void loop() {
  float current_1 = 0;  //mA
  float current_2 = 0;  //mA
  float voltage_1 = 0;  // V
  float voltage_2 = 0;  // V
  current_1 = ina1.getCurrent_mA();     // based on the implementation, it doesn't seem like getCurrent_mA would produce noisy readings
  current_2 = ina2.getCurrent_mA();     // so I don't think value filtering is required
  voltage_1 = ina1.getBusVoltage_V();
  voltage_2 = ina2.getBusVoltage_V();
  unsigned long time = millis()/1000 - WAIT_TIME/1;
  display_currents(time, current_1, current_2, voltage_1, voltage_2);
  write_currents(time, current_1, current_2, voltage_1, voltage_2);
  delay(1000);  // measurement every 1 sec
}

//---Helper Functions-------------------------------------------------------------------- 

unsigned int highestFileNumber(File dir) {
  unsigned int num = 0;

  //Loop through all files in the current directory:
  while (true) {
    File dataFile = dir.openNextFile(); 
    if (!dataFile) {
      break;  // no more files in the SD card root dir
    }
  
    const String fileName = dataFile.name();
    int dotInd = fileName.indexOf('.');
    String numStr = fileName.substring(4, dotInd);   // 4 because len("DATA") = 4
    int fnumber = numStr.toInt();                 // toInt() returns 0 if it fails
    if (fnumber > num) {
      num = fnumber;
    }
    dataFile.close(); 
  }

  return num;
}

void display_currents(unsigned long time, float i1, float i2, float u1, float u2) {
  // prepare:
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);  

  // write:
  display.print("t= ");
  display.print(time);
  display.println(" s");
  display.print("I1= ");
  display.print(i1, 2);     // number of decimal places
  display.println(" mA");
  display.print("I2= ");
  display.print(i2, 2);     // number of decimal places
  display.println(" mA");
  display.print("U1= ");
  display.print(u1, 2);     // number of decimal places
  display.println(" V");
  display.print("U2= ");
  display.print(u2, 2);     // number of decimal places
  display.println(" V");
  display.display();
}

void write_currents(unsigned long time, float i1, float i2, float u1, float u2) {
  digitalWrite(LED_BUILTIN, HIGH);
  File dataFile = SD.open(fname, FILE_WRITE);
  if (dataFile) {
    if (dataFile.size() < MAX_SIZE) {
      dataFile.print(time);
      dataFile.print(",");
      dataFile.print(i1, 2);
      dataFile.print(",");
      dataFile.print(i2, 2);
      dataFile.print(",");
      dataFile.print(u1, 2);
      dataFile.print(",");
      dataFile.println(u2, 2);
      //dataFile.print(",");
    } else {
      Serial.println("Could not write to SD card, max file size reached.");   // maybe would be nicer to display it on the OLED
    }
    dataFile.close();
  } else{
    Serial.println("Could not detect SD card.");
  }
  digitalWrite(LED_BUILTIN, LOW);
}

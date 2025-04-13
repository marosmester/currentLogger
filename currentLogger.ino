#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Serial 
#define WAIT_TIME 20 // seconds

// OLED 128x64 display:
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C /// see datasheet for Address; usually 0x3D or 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Current sensors:
Adafruit_INA219 ina1;
Adafruit_INA219 ina2;

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
  // Initialize Serial
  Serial.begin(115200);   // Only for potential debugging, Serial isn't required to solve the task
  int cnt = 0;
  while (!Serial && cnt < WAIT_TIME) {
    delay(1000);
    cnt++;
    Serial.begin(115200); 
  }
  if (Serial) Serial.println("Serial initialized.");

  // Initialize the OLED display
  while (!display.begin(SSD1306_EXTERNALVCC, SCREEN_ADDRESS)) {      // <------------- try swicthing SSD1306_EXTERNALVCC to SSD1306_SWITCHCAPVCC if OLED doesn't light up
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

  // Initialize SD card:
  while (!SD.begin(53)) {  // CS pin
    Serial.println("SD card initialization failed ... retrying");
    delay(500);
  }
  Serial.println("SD card initialization done.");
  
  // Scan the SD card for the filename with the highest number:
  File root = SD.open("/");
  unsigned int new_num = highestFileNumber(root) + 1;
  fname = "DATA" + String(new_num) + ".csv";
  
  // Open a new csv file
  File dataFile = SD.open(fname, FILE_WRITE);   // FILE_WRITE opens the file for appending
  if (dataFile) {     // if the file opened okay 
    Serial.println("CSV file opened successfully.");
    dataFile.println("Time,Current_1,Current_2");
    dataFile.close();
  } else {
    Serial.println("Failed to open CSV file.");
  }
  Serial.println();
}

void loop() {
  float current_1 = 0;  //mA
  float current_2 = 0;  //mA
  current_1 = ina1.getCurrent_mA();     // based on the implementation, it doesn't seem like getCurrent_mA would produce noisy readings
  current_2 = ina2.getCurrent_mA();     // so I don't think value filtering is required
  unsigned long time = millis()/1000;
  display_currents(time, current_1, current_2);
  write_currents(time, current_1, current_2);
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

void display_currents(unsigned long time, float c1, float c2) {
  // prepare:
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10); // This needs to be tested on the real display - I did not get to do it  
  //display.setTextSize(2); // Draw 2X-scale text
  // write:
  display.print(time);
  display.print(" ");
  display.print(c1, 2);     // number of decimal places
  display.print(" mA, ");
  display.print(c2, 2);     // number of decimal places
  display.println(" mA");
  display.display();
}

void write_currents(unsigned long time, float c1, float c2) {
  File dataFile = SD.open(fname, FILE_WRITE);
  if (dataFile) {
    if (dataFile.size() < MAX_SIZE) {
      dataFile.print(time);
      dataFile.print(",");
      dataFile.print(c1, 2);
      dataFile.print(",");
      dataFile.println(c2, 2);
    } else {
      Serial.println("Could not write to SD card, max file size reached.");   // maybe would be nicer to display it on the OLED
    }
    dataFile.close();
  } else{
    Serial.println("Could not detect SD card.");
  }
}

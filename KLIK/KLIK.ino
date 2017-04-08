// Includes for GPS
#include <TinyGPS++.h>
#include <AltSoftSerial.h>
#include <SoftwareSerial.h>

// Includes for WIFI
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// Includes NFC
#include <MFRC522.h>

#define SS_PIN 7
#define RST_PIN 2
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
byte nuidPICC[4];

#define YELLOWLED 6

//Defines for GPS
static const uint32_t baud = 9600;

// The TinyGPS++ object
TinyGPSPlus gps;

// The serial connection to the GPS device
AltSoftSerial altSerial;

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
SPI_CLOCK_DIV2); // you can change this clock speed

#define WLAN_SSID       "Mentozer"           // cannot be longer than 32 characters!
#define WLAN_PASS       "12345678"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  2000      // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.
// What page to grab!
#define WEBSITE      "project.cmi.hr.nl"
#define WEBPAGE      "/2016_2017/mlab1_bewo_t3/api/getBzzz.php"
  
float LAT;
float LNG;
bool NFC = false;

uint32_t ip;

//Buttons
int button1 = A0;
int button2 = A1;
int button3 = A2;
int button1Value = 0;
int button2Value = 0;
int button3Value = 0;

void setup(void)
{
  Serial.begin(baud);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  altSerial.begin(baud);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  
  pinMode(YELLOWLED, OUTPUT);
  
  Serial.println(F("Welcome to Klik!\n")); 
  
  /* Initialise the module */
  Serial.println(F("\nInitializing Network..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  /* Wait for DHCP to complete */
  Serial.println(F("Network Initialized"));

  /* Wait for DHCP to complete */
  Serial.println(F("GPS Initialized"));
}

void ReadNetworkData()
{
   ip = 0;
  // Try looking up the website's IP address
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }

  cc3000.printIPdotsRev(ip);  
  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
  if (www.connected()) {
    www.fastrprint(F("GET "));
    String page = WEBPAGE;
    String LATI = String(LAT, 6);
    String LONG = String(LNG, 6);
    String URL = page + "?lat=" + LATI + "&lng=" + LONG; 
    const char * request = URL.c_str();
    www.fastrprint(request);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(WEBSITE); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  String webdata;
  
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      webdata += c;
      lastRead = millis();
    }
  }
  www.stop();
  www.close();

  // Serial.println(webdata);
  if(webdata.indexOf("true") > 0) {
    digitalWrite(YELLOWLED, HIGH);
    delay(1000);
    digitalWrite(YELLOWLED, LOW);
  }
  else if(webdata.indexOf("false") > 0) { 
    digitalWrite(YELLOWLED, LOW);
  }
}

void loop(void)
{
  char c;
  if (Serial.available()) {
    c = Serial.read();
    altSerial.print(c);
  }
  if (altSerial.available()) {
    c = altSerial.read();
    Serial.print(c);
  }
  
  ReadNFC();
  if(!NFC)
  {
    smartDelay(5000);
    if(ReadGPSData())
    {
        ReadNetworkData();
    }
  } else {
    Serial.println("Please press a button");
    while(!getPressedButton()) {
      delay(50);
    }

    int pressedButton = getPressedButton();
    Serial.println("You pressed button " + String(pressedButton));
    // do shit with pressed button
    byte pressedButtonCounter = 0;
    while (pressedButtonCounter < pressedButton) {
      delay(500);
      digitalWrite(YELLOWLED, HIGH);
      delay(500);
      digitalWrite(YELLOWLED, LOW);
      pressedButtonCounter ++;
    }
    
    
  }
}

bool ReadGPSData()
{
  Serial.println("checking GPS");
  if(gps.location.isValid())
  { 
      LAT = gps.location.lat();
      LNG = gps.location.lng();
      //Serial.println(LAT, 6);
      //Serial.println(LNG, 6);
      return true;
  }
  else
  {
    LAT = 51.9175;
    LNG = 4.48354;
    return true;
  }
  return false;
}

void ReadNFC(){
  NFC = false;
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    NFC = true;
  }
  else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (altSerial.available())
      gps.encode(altSerial.read());
  } while (millis() - start < ms);
}

void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

int getPressedButton() {
  button1Value = analogRead(button1);
  button2Value = analogRead(button2);
  button3Value = analogRead(button3);
  int returnvalue = 0;
  if(button1Value > 500) {
    returnvalue = 1;
  } else if(button2Value > 500) {
    returnvalue = 2;
  } else if(button3Value > 500) {
    returnvalue = 3;
  } else {
    returnvalue = false;
  }
  return returnvalue;
}


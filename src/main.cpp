Copyright (c) 2023 fukuen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

#include <M5Stack.h>
#include <LoRa-E5.h>
#include <Preferences.h>
#include <WiFi.h>
#include <driver/adc.h>
//#include <stdio.h>
//#include <stdlib.h>

//WiFiManager
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>  

#define LoRa_APPKEY              "2B7E151628AED2A609CF4F3CABF71588" /*Custom key for this App*/
#define LoRa_FREQ_standard       AS923   /*International frequency band. see*/
#define LoRa_DR                  DR4     /*DR5=5.2kbps //data rate. see at https://www.thethingsnetwork.org/docs/lorawan/regional-parameters/    */
#define LoRa_DEVICE_CLASS        CLASS_A /*CLASS_A for power restriction/low power nodes. Class C for other device applications */
#define LoRa_PORT_BYTES          8       /*node Port for binary values to send, allowing the app to know it is recieving bytes*/
#define LoRa_PORT_STRING         7       /*Node Port for string messages to send, allowing the app to know it is recieving characters/text */
#define LoRa_POWER               14      /*Node Tx (Transmition) power*/
#define LoRa_CHANNEL             0       /*Node selected Tx channel. Default is 0, we use 2 to show only to show how to set up*/
#define LoRa_ADR_FLAG            true    /*ADR(Adaptative Dara Rate) status flag (True or False). Use False if your Node is moving*/
/*Time to wait for transmiting a packet again*/
#define Tx_delay_s               9.5     /*delay between transmitions expressed in seconds*/
/*Packet information*/
#define PAYLOAD_FIRST_TX         10   /*bytes to send into first packet*/
//#define Tx_and_ACK_RX_timeout    6000 /*6000 for SF12,4000 for SF11,3000 for SF11, 2000 for SF9/8/, 1500 for SF7. All examples consering 50 bytes payload and BW125*/     
#define Tx_and_ACK_RX_timeout    10000 /*6000 for SF12,4000 for SF11,3000 for SF11, 2000 for SF9/8/, 1500 for SF7. All examples consering 50 bytes payload and BW125*/     

LoRaE5Class lorae5;

unsigned char buffer_binary[128] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20};
char buffer_char[50] = "I am sending this message to a LoRa Gateway.";/**/
char char_temp[256] = { 0 };//256 is ok size for recieving command data
char buffer[512] = { 0 };
char temp[10] = { 0 };
char ar[80];

time_t nowSec;
time_t lastSec;
struct tm timeinfo;

Preferences prefs;
int upcount = -1;

/*defines dependensies to avoid a CRASH of the program*/
#ifdef PRINT_TO_USER_TIME_DIFFERENCE
  #ifndef PRINT_TO_USER
    #define PRINT_TO_USER
  #endif
#endif 
#ifdef LORA_DEBUG_AND_PRINT
  #ifndef PRINT_TO_USER
    #define PRINT_TO_USER
  #endif
#endif 

int getCount() {
  prefs.begin("counter");
  upcount = prefs.getInt("upcount");
  prefs.end();
  return upcount;
}

void setCount(int upcount) {
  if (upcount < 0) return;
  prefs.begin("counter");
  prefs.putInt("upcount", upcount);
  prefs.end();
}

int getCurrentCount() {
  char substr[30];
  char* pstr;
  lorae5.at_send_check_response("AT+LW=ULDL\r\n", AT_NO_ACK, DEFAULT_TIMEWAIT, buffer);
  Serial.println(buffer);
  pstr=strstr(buffer, "+LW: ULDL, ");//searchs for "+LW:"
  if(pstr!= NULL){
    strncpy(substr,(pstr+11),9);//Copy uplink counter
    for (int i = 0; i < 9; i++) {
      if (substr[i] == ',') {
        substr[i]='\0'; //add the end of the string
        break;
      }
    }
    Serial.printf("Uplink count: %s\n", substr);
    return atoi(substr);
  }
  return -1;
}

void setCurrentCount(int current) {
  char cmd[30];
  Serial.printf("Uplink count: %d\n", current);
  sprintf(cmd, "AT+LW=ULDL, %d, 0\r\n", current);
  lorae5.at_send_check_response(cmd, AT_NO_ACK, DEFAULT_TIMEWAIT, NULL);
}

void getTemperature(char *temp) {
  char* pstr;
  memset(temp, 0, 10);
  lorae5.at_send_check_response("AT+TEMP=?\r\n", AT_NO_ACK, DEFAULT_TIMEWAIT, buffer);
  Serial.println(buffer);
  pstr=strstr(buffer, "+TEMP:");//searchs for "+TEMP:"
  if(pstr!= NULL){
    strncpy(temp, (pstr+7), 10);//Copy temperature
  }
  for (int i = 0; i < 40; i++) {
    if (temp[i] == '\r') {
      temp[i] = 0;
    }
  }
}

void getRtc(char *time) {
  char* pstr;
  memset(time, 0, 20);
  lorae5.at_send_check_response("AT+RTC\r\n", AT_NO_ACK, DEFAULT_TIMEWAIT, buffer);
  Serial.println(buffer);
  pstr=strstr(buffer, "+RTC:");//searchs for "+RTC:"
  if(pstr!= NULL){
    strncpy(time, (pstr+6), 19);//Copy Time
  }
  for (int i = 0; i < 40; i++) {
    if (time[i] == '\r') {
      time[i] = 0;
    }
  }
}

void setRtc(char *time) {
  char cmd[40];
  memset(cmd, 0, 40);
  sprintf(cmd, "AT+RTC=\"%s\"\r\n", time);
  lorae5.at_send_check_response(cmd, AT_NO_ACK, DEFAULT_TIMEWAIT, NULL);
}

void printCurrentTime() {
  nowSec = time(nullptr);
 
  gmtime_r(&nowSec, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

char* convNowSec() { 
  nowSec = time(nullptr);

  gmtime_r(&nowSec, &timeinfo);
  snprintf(ar, 80, "%04d-%02d-%02d %02d:%02d:%02d",
  timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return ar;
}

void setClock() {
  configTime(9 * 3600, 0, "ntp.nict.jp", "pool.ntp.org", "time.nist.gov");
 
  Serial.print("Waiting for NTP time sync: ");
  nowSec = time(nullptr);
  while (nowSec < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    yield();
    nowSec = time(nullptr);
  }
  Serial.println();
 
//  printCurrentTime();
  Serial.println(convNowSec());
}

void LoRa_setup(void){
  lorae5.setDeviceMode(LWABP);/*LWOTAA or LWABP. We use LWOTAA in this example*/
  lorae5.setDataRate((_data_rate_t)LoRa_DR, (_physical_type_t)LoRa_FREQ_standard);
  lorae5.setKey(LoRa_APPKEY, LoRa_APPKEY, LoRa_APPKEY); /*Only App key is seeted when using OOTA*/
  lorae5.setClassType((_class_type_t) LoRa_DEVICE_CLASS); /*set device class*/
  lorae5.setPort(LoRa_PORT_BYTES);/*set the default port for transmiting data*/
  lorae5.setPower(LoRa_POWER); /*sets the Tx power*/
  lorae5.setChannel(LoRa_CHANNEL);/*selects the channel*/
  lorae5.setAdaptiveDataRate(LoRa_ADR_FLAG);/*Enables adaptative data rate*/  
}

void setup(void) {
  M5.begin();
  Serial.begin(115200);
//  Serial1.begin(9600, SERIAL_8N1, 21, 22);
  M5.Lcd.setBrightness(200);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("Connecting to WiFi");
  // for BtnA bug
  adc_power_acquire(); // ADC Power ON

  lorae5.Debug(lora_DEBUG);

  /*Init the LoRa class after initing the serial print port */
  lorae5.init(21, 22);/* call lorae5.init(Arduino_Tx_PIN,Arduino_Rx_PIN) to use software serial. Example: lorae5.init(D2,D3) */ /*First get device EUI for later printing*/
  lorae5.getId(char_temp, DevEui, 256); //,DevEui); /*100 ms is more than enough to get a response from the module*/
  /*set up device. You must set up all your parameters BEFORE Joining.
   If you make any change (outside channel or port setup), you should join again the network for proper working*/
  LoRa_setup();
    /*Now shows you the device actual DevEUI and AppEUI got at the time you call the function */
  #ifdef PRINT_TO_USER 
  Serial.print("\r\nCurrent DevEui: ");/*to print the obtained characters*/
  Serial.print(char_temp);/*to print the obtained characters*/
  #endif
  setCurrentCount(getCount());
  /*Enters in a while Loop until the join process is completed*/ 
//  while(lorae5.setOTAAJoin(JOIN, 10000)==0);//will attempt to join network until the ends of time. https://www.thethingsnetwork.org/docs/lorawan/message-types/
   /*POWER DOWN the LoRa module until next Tx (Transmition) cicle*/

  WiFi.setSleep(false);
  WiFiManager wifiManager;
  if (!M5.BtnA.isPressed() && !M5.BtnB.isPressed() && M5.BtnC.isPressed()) {
//    wifiManager.resetSettings();
    wifiManager.startConfigPortal("M5STACK-DEV");
  } else {
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.autoConnect("M5STACK-DEV");
  }

  if (WiFi.isConnected()) {
    setClock();
    setRtc(ar);
    WiFi.disconnect();
  }
  M5.Lcd.setBrightness(0);

  lorae5.setDeviceLowPower();
  lastSec = time(nullptr);
}

void loop(void) {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    Serial.println("BtnA pressed!");
    lorae5.setDeviceWakeUp();

    getTemperature(temp);
    Serial.printf("Temp: %s\n", temp);
    getRtc(ar);
    Serial.printf("Rtc: %s\n", ar);

    lorae5.setPort(LoRa_PORT_STRING);
    memset(buffer, 0, 20);
    sprintf(buffer, "{'temp':%s}", temp);
    lorae5.transferPacketWithConfirmed(buffer, Tx_and_ACK_RX_timeout);
    setCount(getCurrentCount());

    lorae5.setDeviceLowPower();
  }
  if (M5.BtnB.wasPressed()) {
    Serial.println("BtnB pressed!");
  }
  if (M5.BtnC.wasPressed()) {
    Serial.println("BtnC pressed!");
    lorae5.setDeviceWakeUp();
    lorae5.setPort(LoRa_PORT_BYTES);
    lorae5.transferPacketWithConfirmed(buffer_binary, PAYLOAD_FIRST_TX, Tx_and_ACK_RX_timeout);
    setCount(getCurrentCount());
    lorae5.setDeviceLowPower();
  }

//  lorae5.at_send_check_response("AT+\r\n","AT",DEFAULT_TIMEWAIT, NULL);

  nowSec = time(nullptr);
  if (difftime(nowSec, lastSec) > 60) {
    gmtime_r(&nowSec, &timeinfo);
    Serial.print("\nCurrent time: ");
    Serial.print(asctime(&timeinfo));
    gmtime_r(&lastSec, &timeinfo);
    Serial.print("\nLast time: ");
    Serial.print(asctime(&timeinfo));
    /*Wake Up the LoRa module*/
    lorae5.setDeviceWakeUp();/*if the module is not in sleep state, this command does nothing*/
    /*--------sending bytes message*/
    lorae5.setPort(LoRa_PORT_BYTES);
    lorae5.transferPacketWithConfirmed(buffer_binary, PAYLOAD_FIRST_TX, Tx_and_ACK_RX_timeout);
    /*POWER DOWN the LoRa module until next Tx Transmition (Tx) cicle*/
    setCount(getCurrentCount());
    lorae5.setDeviceLowPower();
    lastSec = time(nullptr);
  }
  /*Sleeps until the next tx cicle*/
//  delay((unsigned int)(Tx_delay_s*1000));/*Convert the value in seconds to miliseconds*/
  delay(100);
}

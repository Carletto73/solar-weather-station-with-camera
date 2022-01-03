#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include "DHT.h"
#include "time.h"
#include <EEPROM.h>

// Initialize with your wifi
const char* ssid     = "yout_net"; // Write your SSID
const char* password = "your_password";  //Write your Password

// Initialize Telegram BOT
String BOTtoken = "your_telegram_token";  // your Bot Token (Get from Botfather)
String CHAT_ID = "your_chat_id";

// Initialize IFTTT
const char* IFTTT_URL = "your_IFTTT_URL";
const char* server = "maker.ifttt.com";

// Initialize the NTP function
const char* ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

int hour;
int day;
int month;
long current;
struct tm timeinfo;

// Variables declarations
bool sendPhoto = false;
float humidity = 0;
float temperature = 0;
int humidity_int = 0;
int temperature_int = 0;
String humidity_text = "";
String temperature_text = "";
String vbatt_text = "";
int vbatt_raw = 0;
float vbatt_calc = 0;
unsigned int  minutefromserver = 0;
unsigned int hourfromserver = 0;
unsigned int dayfromserver = 0;
unsigned int monthfromserver = 0;
bool sendmonthlynotification = false;
long battalarm_calc = 0;
int last_month_notified = 0;
int periodSamples = 0;
int life_signal = 0;
int daily_photo = 0;
int daily_photo_hour = 0;
int last_day_daily_photo = 0;
int parametrization_time = 0;
int flash = 0;
int battalarm = 0;
int last_day_battalarm = 0;
int periodic_photo = 0;

// Wifi connections
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

//Address of flash LED embedded 
#define FLASH_LED_PIN 4

// GPIO33 (ADC input for monitoring battery level)
#define vbatt_pin 14

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.
#define DHTPIN 13     
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

//***************************************************************
//                       MAIN ROUTINE                           *
//***************************************************************
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

// Set 60 cycles without a long sleep time between samples (15 minutes)
    if (periodic_photo == 0) {
      parametrization_time = 60;
      EEPROM.write(6, parametrization_time); // Number of cycles with limited sleep time (15 second). To speed the parametrization)
      EEPROM.commit();
    }

// Print the received message
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    Serial.print("Massage received from Telegram: ");
    Serial.print(text);
    Serial.print(" from: ");
    Serial.println(from_name);
    
//***************************************************************
    if (text == "/flashON") {
      Serial.println("Switch flash LED on");
      flash = 1;
      EEPROM.write(7, flash); // Enable/disable the flash during the photo
      EEPROM.commit();
      String textAnswer = "Flash on shooting ENABLED\n";
      textAnswer += "Type /flashOFF to disable\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/flashOFF") {
      Serial.println("Switch flash LED off");
      flash = 0;
      EEPROM.write(7, flash); // Enable/disable the flash during the photo
      EEPROM.commit();
      String textAnswer = "Flash on shooting DISABLED\n";
      textAnswer += "Type /flashON to enable\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/lifesigON") {
      Serial.println("Enable flash after the boot");
      life_signal = 1;
      EEPROM.write(2, life_signal); // Enable/disable the flash signal after the boot
      EEPROM.commit();
      String textAnswer = "Flash after the boot ENABLED\n";
      textAnswer += "Type /lifesigOFF to disable\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/lifesigOFF") {
      Serial.println("Disable flash after the boot");
      life_signal = 0;
      EEPROM.write(2, life_signal); // Enable/disable the flash signal after the boot
      EEPROM.commit();
      String textAnswer = "Flash after the boot DISABLED\n";
      textAnswer += "Type /lifesigON to enable\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/dailyON") {
      Serial.println("Enable daily photo");
      daily_photo = 1;
      EEPROM.write(3, daily_photo); // Enable/disable daily photo
      last_day_daily_photo = 99; // In this way a new dayly notification is forced
      EEPROM.write(5, last_day_daily_photo); // Last day in which the daily photo was made. Force to re-check the moment of the daily photo
      periodic_photo = 0;
      EEPROM.write(10, periodic_photo); // Number of periodic photos remaning
      EEPROM.commit();
      String textAnswer = "Daily photo ENABLED\n";
      textAnswer += "Type /dailyOFF to disable\n";
      textAnswer += "Remaining photos to take after waking up SET to 0\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/dailyOFF") {
      Serial.println("Disable daily photo");
      daily_photo = 0;
      EEPROM.write(3, daily_photo); // Enable/disable daily photo
      last_day_daily_photo = 99; // In this way a new dayly notification is forced
      EEPROM.write(5, last_day_daily_photo); // Last day in which the daily photo was made. Force to re-check the moment of the daily photo
      EEPROM.commit();
      String textAnswer = "Daily photo DISABLED\n";
      textAnswer += "Type /dailyON to enable\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
    else if (text == "/data") {
      Serial.println("Request to show the data");
      sendDataOnTelegram();
    }
//***************************************************************
    else if (text == "/config") {
      Serial.println("Request to show the data");
      sendParametersOnTelegram();
    }
//***************************************************************
    else if ( text.indexOf("Dailyh:") !=-1 ) {
      String textValuePart = text.substring(7);
      int valuePart = textValuePart.toInt();
      if ((valuePart >= 0) && (valuePart < 24)) {
        daily_photo_hour = valuePart;
        EEPROM.write(4, daily_photo_hour); // Hour of daily photo
        last_day_daily_photo = 99; // In this way a new dayly notification is forced
        EEPROM.write(5, last_day_daily_photo); // Last day in which the daily photo was made. Force to re-check the moment of the daily photo
        EEPROM.commit();
        String textAnswer = "Hour for daily photo SET to " + String(valuePart) + ":00"; 
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
      else {
        String textAnswer = "Hour NOT set\n";
        textAnswer += "Type a valid number (0 ÷ 23)";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
    }
//***************************************************************
    else if ( text.indexOf("Period:") !=-1 ) {
      String textValuePart = text.substring(7);
      int valuePart = textValuePart.toInt();
      if ((valuePart > 0) && (valuePart < 61)) {
        periodSamples = valuePart;
        EEPROM.write(1, periodSamples); // Period from two samples (from 1 to 60 minutes)
        EEPROM.commit();
        String textAnswer = "Period SET to " + String(valuePart); 
        if (periodSamples == 1)textAnswer += " minute\n";
        if (periodSamples > 1)textAnswer += " minutes\n";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
      else {
        String textAnswer = "Period NOT set\n";
        textAnswer += "Type a valid number (1 ÷ 60)";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
    }
//***************************************************************
    else if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
//***************************************************************
    else if (text == "/stop") {
      parametrization_time = 0;
      EEPROM.write(6, parametrization_time); // Number of cycles with limited sleep time (15 second). To speed the parametrization)
      periodic_photo = 0;
      EEPROM.write(10, periodic_photo); // Number of periodic photos remaning
      EEPROM.commit();
      Serial.println("Stop fast wake up time");
    }
//***************************************************************
    else if ( text.indexOf("Batalarm:") !=-1 ) {
      String textValuePart = text.substring(9);
      int valuePart = textValuePart.toInt();
      if ((valuePart > 25) && (valuePart < 41)) {
        battalarm = valuePart;
        EEPROM.write(8, battalarm); // Voltage level battery alarm
        last_day_battalarm = 99; // In this way a new battery alarm notification is forced
        EEPROM.write(9, last_day_battalarm); // Last day in which the alarm battery was notified
        EEPROM.commit();
        String textAnswer = "Alarm battery SET to " + String(battalarm) + "00 mV"; 
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
      else {
        String textAnswer = "Alarm battery NOT set\n";
        textAnswer += "Type a valid number (26 ÷ 40)";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
    }
//***************************************************************
    else if ( text.indexOf("Photonum:") !=-1 ) {
      String textValuePart = text.substring(9);
      int valuePart = textValuePart.toInt();
      if ((valuePart > 1) && (valuePart < 251)) {
        periodic_photo = valuePart;
        EEPROM.write(10, periodic_photo); // Number of periodic photos remaning
        parametrization_time = 0;
        EEPROM.write(6, parametrization_time); // Number of cycles with limited sleep time (15 second). To speed the parametrization)
        daily_photo = 0;
        EEPROM.write(3, daily_photo); // Enable/disable daily photo
        EEPROM.commit();
        String textAnswer = "Remaining photos to take after waking up SET to " + String(periodic_photo) +"\n"; 
        textAnswer += "Daily photo DISABLED\n";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
      else {
        String textAnswer = "Number of photo take after the wake up NOT set\n";
        textAnswer += "Type a valid number (2 ÷ 250)";
        bot.sendMessage(bot.messages[i].chat_id, textAnswer, "");
      }
    }
//***************************************************************
    else {
      String textAnswer = "Welcome , " + from_name + "!\n";
      textAnswer += "Use the following commands to interact with the ESP32-CAM \n";
      textAnswer += "/data : shows the data\n";
      textAnswer += "/config : shows the parameters\n";
      textAnswer += "/photo : takes a new photo\n";
      textAnswer += "/flashON : flash ON\n";
      textAnswer += "/flashOFF : flash OFF\n";
      textAnswer += "/lifesigON : flash after boot ON\n";
      textAnswer += "/lifesigOFF : flash after boot OFF\n";
      textAnswer += "/dailyON : daily photo ON\n";
      textAnswer += "/dailyOFF : daily photo OFF\n";
      textAnswer += "/stop : stop fast wake up time\n";
      textAnswer += "Period:xx minutes sampling\n";
      textAnswer += "Dailyh:xx hour of daily photo\n";
      textAnswer += "Batalarm:xx alarm battery level\n";
      textAnswer += "Photonum:xxx n° photos remaining\n";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
//***************************************************************
  }
}

void setup(){
// Init Serial Monitor
  Serial.begin(115200);

//Initialize EEPROM
  EEPROM.begin(512);

// Set LED Flash as output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, 0);

// Battery voltage reading
  vbatt_raw = analogRead(vbatt_pin);
  vbatt_calc = (vbatt_raw * 0.0010931);

// Init DHT sensor
  dht.begin();

// Config and init the camera
  configInitCamera();

// Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

// Add root certificate for api.telegram.org  
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

// WiFi connection with 10 seconds of timeout
  int connection_timeout = 10 * 10; // 10 seconds
  while(WiFi.status() != WL_CONNECTED  && (connection_timeout-- > 0)) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  if(WiFi.status() != WL_CONNECTED) {
     Serial.println("Connection timeout. Failed to connect, going back to sleep");
  }

// Update the monitor with the IP address
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

// init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

// Read the EEPROM
  last_month_notified = EEPROM.read(0); // Last month in which the notification of attendance was made
  periodSamples = EEPROM.read(1); // Period from two samples (from 1 to 60 minutes)
  life_signal = EEPROM.read(2); // Enable/disable the flash signal after the boot
  daily_photo = EEPROM.read(3); // Enable/disable daily photo
  daily_photo_hour = EEPROM.read(4); // Hour of daily photo
  last_day_daily_photo = EEPROM.read(5); // Last day in which the daily photo was made
  parametrization_time = EEPROM.read(6); // Number of cycles with limited sleep time (to speed the parametrization)
  flash = EEPROM.read(7); // Enable/disable the flash during the photo
  battalarm = EEPROM.read(8); // Voltage level battery alarm
  last_day_battalarm = EEPROM.read(9); // Last day in which the alarm battery was notified
  periodic_photo = EEPROM.read(10); // Number of periodic photos remaning

// Check if the monthly notification must be done
  if ((month != last_month_notified) && (hour > 9)) {
    sendmonthlynotification = true;
    last_month_notified = month;
    EEPROM.write(0, last_month_notified); // Last month in which the notification of attendance was made
    EEPROM.commit();
  }

// After the Wifi connection the flash LED blink (if enabled)
  if (life_signal == 1) digitalWrite(FLASH_LED_PIN, 1);
  delay(1);
  digitalWrite(FLASH_LED_PIN, 0);
}

void loop() {
//***************************************************************
// The picture must be done the cycle before the telegram message ?!?I don't know why?!?
  if (sendPhoto) {
    Serial.println("Preparing photo");
    if (flash) {
      digitalWrite(FLASH_LED_PIN, 1);
      delay(500);
    }
    sendPhotoTelegram(); 
    digitalWrite(FLASH_LED_PIN, 0);
    sendPhoto = false; 
  }
//***************************************************************
  if (sendmonthlynotification) {
    sendmonthlynotification = false;
    Serial.println("Montly notification from ESP32-CAM");
    String textAnswer = "Monthly notification from ESP32-CAM\n";
    bot.sendMessage(CHAT_ID, textAnswer, "");
    sendDataOnTelegram();
  }
//***************************************************************

// Reading temperature or humidity takes about 250 milliseconds!
// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();
  humidity_int = humidity * 100;
  humidity_text = "=" + String(humidity_int) + "/100";

// Read temperature as Celsius (the default)
  temperature = dht.readTemperature();
  temperature_int = temperature * 100;
  temperature_text = "=" + String(temperature_int) + "/100";

// Send calculation for voltage battery log 
  vbatt_text = "=" + String(vbatt_raw) + "*10931/10000000";

// Read time
//  printLocalTime();
  Serial.print("Time from variables:  ");
  Serial.print(day);
  Serial.print(".");
  Serial.print(month);
  Serial.print(" --- ");
  Serial.print(hour);

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  else
  {
// IFTTT event. It update the Google Sheet
    makeIFTTTRequest();
  }

// Check the Telegram messages
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    Serial.println("got response");
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

// Check if the daily photo must be done
  if ((day != last_day_daily_photo) && (hour >= daily_photo_hour)) {
    if (daily_photo) sendPhoto = true;
    last_day_daily_photo = day;
    EEPROM.write(5, last_day_daily_photo); // Last day in which the daily photo was made
    EEPROM.commit();
  }

// Check if alarm battery must be notified
  if ((day != last_day_battalarm) && (hour > 10)) {
    battalarm_calc = (battalarm * 1000000) / 10931;
    if (vbatt_raw < battalarm_calc) {
      last_day_battalarm = day;
      EEPROM.write(9, last_day_battalarm); // Last day in which the alarm battery was notified
      EEPROM.commit();
      String textAnswer = "!!ALARM BATTERY !!\n"; 
      textAnswer += "Voltage level: " + String(vbatt_calc, 3) + " Vdc";
      bot.sendMessage(CHAT_ID, textAnswer, "");
    }
  }

// Check if alarm battery must be notified
  if (periodic_photo > 0) {
    periodic_photo--;
    EEPROM.write(10, periodic_photo); // Number of periodic photos remaning
    EEPROM.commit();
    Serial.println("Preparing periodic photo");
    if (flash) {
      digitalWrite(FLASH_LED_PIN, 1);
      delay(500);
    }
    sendPhotoTelegram(); 
    digitalWrite(FLASH_LED_PIN, 0);
  }

// Sleep
  if (!sendPhoto) {
    if (parametrization_time > 0) {
      parametrization_time--;
      EEPROM.write(6, parametrization_time); // Number of cycles with limited sleep time (to speed the parametrization)
      EEPROM.commit();
      esp_sleep_enable_timer_wakeup(5 * 1000000);    
    }
    else {
      esp_sleep_enable_timer_wakeup(((periodSamples *60) - 10) * 1000000);    
      //esp_sleep_enable_timer_wakeup(20 * 1000000);    
    }
    Serial.println("Going to sleep now");
    Serial.print("Time left awake: ");
    Serial.print(millis());
    Serial.println("mS");
    Serial.println();
    Serial.println();
    Serial.println();
    esp_deep_sleep_start();
  }
}

void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";
    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void makeIFTTTRequest() {
  Serial.print("Connecting to "); 
  Serial.print(server);
  
  WiFiClient client;
  int retries = 5;
  while(!!!client.connect(server, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if(!!!client.connected()) {
    Serial.println("Failed to connect...");
  }
  
  Serial.print("Request resource: "); 
  Serial.println(IFTTT_URL);

  String jsonObject = String("{\"value1\":\"") + humidity_text + "\",\"value2\":\"" + temperature_text + "\",\"value3\":\"" + vbatt_text + "\"}";
                      
  client.println(String("POST ") + IFTTT_URL + " HTTP/1.1");
  client.println(String("Host: ") + server); 
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);
        
  int timeout = 5 * 10; // 5 seconds             
  while(!!!client.available() && (timeout-- > 0)){
    delay(100);
  }
  if(!!!client.available()) {
    Serial.println("No response...");
  }
  while(client.available()){
    Serial.write(client.read());
  }
  
  Serial.println("\nClosing Connection");
  client.stop(); 
}

void printLocalTime()
{
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
  hour = timeinfo.tm_hour;
  day = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;
}

void sendDataOnTelegram()
{
  String textAnswer = "Data from ESP32-CAM\n";
  textAnswer += "Humidity: " + String(humidity,1) + " %\n";
  textAnswer += "Temperature: " + String(temperature,1) + " °C\n";
  textAnswer += "Battery voltage level: " + String(vbatt_calc, 3) + " Vdc\n";
  textAnswer += "Type /config to shows the paramet.\n";
  textAnswer += "Type /command for parametrization\n";
  bot.sendMessage(CHAT_ID, textAnswer, "");
}

void sendParametersOnTelegram()
{
  String textAnswer = "Parameters of ESP32-CAM\n";
  textAnswer += "Time between samples: " + String(periodSamples);
  if (periodSamples == 1) textAnswer += " minute\n";
  if (periodSamples > 1) textAnswer += " minutes\n";
  textAnswer += "Life signal after boot: ";
  if (life_signal == 1) textAnswer += " ON\n";
  if (life_signal != 1) textAnswer += " OFF\n";
  textAnswer += "Flash on shooting: ";
  if (flash == 1) textAnswer += " ON\n";
  if (flash != 1) textAnswer += " OFF\n";
  textAnswer += "Daily photo: ";
  if (daily_photo == 1) textAnswer += " ON\n";
  if (daily_photo != 1) textAnswer += " OFF\n";
  textAnswer += "Hour of daily photo: " + String(daily_photo_hour) + ":00\n";
  textAnswer += "Alarm battery level: " + String(battalarm) + "00 mV\n"; 
  textAnswer += "Periodic photos remaning: " + String(periodic_photo) + " \n";
  textAnswer += "Type /data to shows the data\n";
  textAnswer += "Type /command for parametrization\n";
  bot.sendMessage(CHAT_ID, textAnswer, "");
}

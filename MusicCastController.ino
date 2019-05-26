/*
           ____  _____ __  ____________  ____  ___  ____________
          / __ \/ ___// / / / ____/ __ \/ __ \/   |/_  __/_  __/
         / / / /\__ \/ /_/ / __/ / /_/ / /_/ / /| | / /   / /   
        / /_/ /___/ / __  / /___/ _, _/ _, _/ ___ |/ /   / /    
        \____//____/_/ /_/_____/_/ |_/_/ |_/_/  |_/_/   /_/     
                                       
*/                                                                     
/* This document contains all of the script to run the system.           */
/* -- To change system variables look in variables.h                     */
/* -- To add wifi passwords, IP address etc look in secrets.h            */
/*                                                                       */

/*-----------------------------------------------------------------------*/
// LIBRARYS                                                              //
/*-----------------------------------------------------------------------*/
//External librarys
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Bounce2.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

//Other files in the folder
#include "hardware.h"
#include "variables.h"
#include "secrets.h"

/*-----------------------------------------------------------------------*/
// SETUP MUSICCAST CONNECTION                                            //
/*-----------------------------------------------------------------------*/
// HTTP Client Details
HTTPClient musiccast;
int httpCode;
String baseAddress = "http://" +
                      String(IP_ADDRESS) +
                      "/YamahaExtendedControl/v2/";

/*-----------------------------------------------------------------------*/
// MUSICCAST SYSTEM VARIABLES                                            //
/* store all information from the musiccast system                       */
/*-----------------------------------------------------------------------*/
String powerStatus, inputSource, toneControlMode, playback, repeat; 
String shuffle, artist, album, track, networkName, tunerBand;
int sleep, currentVolume, maxVolume, bass, treble, balance, totalTime; 
int trackNo, totalTracks;
long playTime;
bool mute;

/*-----------------------------------------------------------------------*/
// SYSTEM VARIABLES                                                      //
/*-----------------------------------------------------------------------*/
int tempVolume, inputNo, maxInputNo, button, lastButton;
String lastPowerStatus;
boolean lastMute;
int lastVolume, lastInputNo, selectedInputNo;
boolean volumeUpdated = false;
long lastMuteMillis = 0, lastInputMillis = 0, lastInputChangeMillis = 0; 
long lastVolumeChange = 0, lastOffChange = 0;

long lastDebounceMillis = 0;
int buttonState;
int lastButtonState = LOW; 

long timeSinceLastUpdate = 0;
bool updateDisplay = true;
bool firstLoop = true;

String inputArray[arraySize];

/*-----------------------------------------------------------------------*/
// HARDWARE DECLARATION                                                  //
/*-----------------------------------------------------------------------*/
// Button Setup
Bounce muteButton = Bounce();
Bounce inputChangeButton = Bounce(); 

// Encoder Setup
Encoder volumeEncoder(volumeEncoderDT_Pin, volumeEncoderCLK_Pin);
Encoder sourceEncoder(sourceEncoderDT_Pin, sourceEncoderCLK_Pin);
#define ENCODER_OPTIMIZE_INTERRUPTS

// Display setup
Adafruit_SSD1306 display(OLED_RESET);


/*///////////////////////////////////////////////////////////////////////*/
/* SETUP                                                                 */
/*///////////////////////////////////////////////////////////////////////*/
void setup() {
  //Setup Serial Connection
  #ifdef serialDebug
    Serial.begin(19200);
  #endif

  //Set display up
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(2);
  display.clearDisplay();
  display.drawBitmap(0,0,musicCastLogo,128,32,WHITE);
  display.display();
  delay(1000);

  //Connect to Wifi
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  #ifdef serialDebug
    Serial.write(12);
    Serial.print("Connecting.");
    Serial.write(4);
  #endif
  int wifiDots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (wifiDots > 3) {
      wifiDots = 0;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10,0);
    display.print("Connecting");
    for (int i = 0; i < wifiDots; i++) {
      display.print(".");
    }
    #ifdef serialDebug
      Serial.print(".");
      Serial.write(4);
    #endif
    wifiDots++;
    display.display();
    delay(100);
  }
  #ifdef serialDebug
    Serial.println();
  #endif

  //Setup Buttons
  #ifdef serialDebug
    Serial.println("Running Hardware Check...");
    Serial.write(4);
  #endif
  
  pinMode(mute_Pin,INPUT_PULLUP);
  muteButton.attach(mute_Pin);
  muteButton.interval(5); // interval in ms
  
  pinMode(inputChange_Pin,INPUT_PULLUP);
  inputChangeButton.attach(inputChange_Pin);
  inputChangeButton.interval(5); // interval in ms
  #ifdef serialDebug
    Serial.println("Getting Device Info from Musiccast...");
    Serial.write(4);
  #endif
  //get Musiccast Device Info
  getDeviceName(); //Get Device Name
  getAvailableInputs(); //Get List of available Inputs
  getDeviceStatus();
  getSourceNo();

  //cascade initial conditions
  selectedInputNo = inputNo;
  lastInputNo = inputNo;
  lastVolume = currentVolume;
  volumeEncoder.write(currentVolume*4);
  sourceEncoder.write(selectedInputNo*4);
  lastOffChange = millis();
}

/*///////////////////////////////////////////////////////////////////////*/
/* MAIN LOOP                                                             */
/*///////////////////////////////////////////////////////////////////////*/
void loop() {
  //read resistor ladder buttons;
  int tempButton = analogRead(buttonPin);
  int button = 6;
  if (850 >= tempButton && tempButton > 800) {
    button = 3;
  } else if (750 >= tempButton && tempButton > 700) {
    button = 2;
  } else if (575 >= tempButton && tempButton > 525) {
    button = 1;
  } else if (50 >= tempButton) {
    button = 0;
  }
  
  //Update all button status
  muteButton.update();
  inputChangeButton.update();
  // Check WiFi Status and only run the main device loop if there is an 
  // internet connection
  if (WiFi.status() == WL_CONNECTED) { 
    if (updateDisplay) { //Update the display when new data is available
      updateDisplay = false;
      serialDashboardUpdate();
    }

      //Volume Change Encoder code
    //If the system is muted or has received an updated value from 
    // musicCast keep the encoder held at the previous volume
    if (mute || volumeUpdated) {
      volumeEncoder.write(currentVolume*4);
      volumeUpdated = false;
    } //update the volume and check it is within the min - max range
    
    currentVolume = floor(volumeEncoder.read()/4);
    
    if (currentVolume > maxVolume) {
      currentVolume = maxVolume;
      volumeEncoder.write(maxVolume*4);
    } else if (currentVolume < 0) {
      currentVolume = 0;
      volumeEncoder.write(0);
    }

    if (lastVolume != currentVolume && powerStatus != "on") {
      lastOffChange = millis();
    }
  
    //Source Change Encoder code
    selectedInputNo = floor(sourceEncoder.read()/4); //read encoder value
  
    //check value does not exceed allowable values
    if (selectedInputNo > maxInputNo) {
      sourceEncoder.write(0);
      selectedInputNo = 0;
    } else if (selectedInputNo < 0) {
      sourceEncoder.write((maxInputNo)*4);
      selectedInputNo = (maxInputNo);
    }
  
    //if value has changed update inactivity timmer to 0 set last source 
    // as current
    if (selectedInputNo != lastInputNo) {
      lastOffChange = millis();
      lastInputChangeMillis = millis();
      lastInputNo = selectedInputNo;
      updateDisplay = true;
    }
  
    //if inactivity timmer exceeds 10seconds and the displayed source is 
    // not the currently Selected source set the displayed source to the 
    // currently selected and update the encoder
    if (millis() - lastInputChangeMillis > sourceHangTime && 
        selectedInputNo != inputNo) {
        selectedInputNo = inputNo;
        sourceEncoder.write(selectedInputNo*4);
    }
  
        // If the switch changed, due to noise or pressing:
    if (button != lastButtonState) {
      lastDebounceMillis = millis();
      if (powerStatus != "on") {
        lastOffChange = millis();
      }
    }
  
    if ((millis() - lastDebounceMillis) > debounceDelay) {
      if (buttonState != button) {
        buttonState = button;
        updateDisplay = true;
        switch (button) {
          case 0:
            powerFunc("toggle"); //power toggle
            break;
          case 1:
              playPauseFunc("toggle");
            break;
          case 2:
              playbackFunc("previous"); //Previous Song
            break;
          case 3:
              playbackFunc("next"); //next song
            break;
          default:
            // statements
            break;
        }
      }
    }


    if ( muteButton.fell() ) { //Button2 Function
      muteFunc("toggle"); //mute toggle
      updateDisplay = true;
      //playPauseFunc("toggle");
      //changeInput("toggleBack");
      //playbackFunc("next"); //next song
      //playbackFunc("previous"); //Previous Song
      //playbackFunc("stop"); //Stop Current source
      //muteFunc("toggle"); //mute toggle
      //shuffleFunc("toggle"); //shuffle toggle
      //repeatFunc("toggle"); //repeat toggle
    }

    if ( inputChangeButton.fell() ) { //Button3 Function
      lastInputChangeMillis = millis();
      if(selectedInputNo != inputNo) {
        inputNo = selectedInputNo;
        changeInput(inputArray[selectedInputNo]); //Change Source Function
      }      
      updateDisplay = true; //Tell the display new info is available
    }

    //If the volume has changed send a request to change it on the system
    if (currentVolume != lastVolume) {
      tempVolume = currentVolume;
      lastVolumeChange = millis();
      setVolume(currentVolume); //Set volume function
      updateDisplay = true;
    }

    if (lastMute != mute) {
      updateDisplay = true;
      lastVolumeChange = millis();
    }

    ////Update device info at regular intervals
    if (millis() - timeSinceLastUpdate >= updateInterval || firstLoop) { 
      timeSinceLastUpdate = millis();
      //Poll System for updates
      getDeviceStatus();
      getSongInfo();
      getSourceNo();
      updateDisplay = true; //tell the display to update
    }

    if (lastPowerStatus != powerStatus) {
      lastOffChange = millis();
      updateDisplay = true;
    }

  } //Connected to internet

  
  lastVolume      = currentVolume;
  lastButtonState = button;
  lastMute        = mute;
  lastPowerStatus = powerStatus;
  
  firstLoop       = false;

}

/*///////////////////////////////////////////////////////////////////////*/
/* Send Musiccast Command                                                */
/*///////////////////////////////////////////////////////////////////////*/
void volumeIncrement(int increment) { //change the volume by the increment
  getDeviceStatus();
  tempVolume = tempVolume + increment;
  if (tempVolume > maxVolume) {
    tempVolume = maxVolume;
  } else if (tempVolume < 0) {
    tempVolume = 0;
  }
  setVolume(tempVolume);
}

void setVolume(int volume) { // set the volume from 0 to maxVolume 
                             // (Will reject values over max and min)
  musiccast.begin(baseAddress + "main/setVolume?volume=" + volume);
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());

    if (root["response_code"] == 0) { //Music Cast Successful request
      //Volume Updated
    } else {
      //Volume NOT updated
    }
  }
  musiccast.end();   //Close connection
}

void playbackFunc(String playbackType) { // playbackType = "stop" "play" 
                                         // "previous" "next"
  unsigned int bufferSizeSong;
  
  if (inputSource == "tuner") {
    if (tunerBand == "fm") {
      String temp_playbackType = playbackType;
      if (playbackType == "previous") {
        temp_playbackType = "auto_down";
      } else if (playbackType == "next") {
        temp_playbackType = "auto_up";
      }
      musiccast.begin(baseAddress +
                      "tuner/setFreq?band=fm&tuning=" + 
                      temp_playbackType); //Try Tuner
      bufferSizeSong = size_t(JSON_OBJECT_SIZE(1) + 20);
    } else if (tunerBand == "dab") {
      musiccast.begin(baseAddress + 
                      "tuner/setDabService?dir=" + 
                      playbackType); //Try DAB
      bufferSizeSong = size_t(JSON_OBJECT_SIZE(1) + 20);
    }
  } else if (inputSource == "cd" || inputSource == "audio_cd") {
    musiccast.begin(baseAddress + 
                    "cd/setPlayback?playback=" + 
                    playbackType);
    bufferSizeSong = size_t(JSON_OBJECT_SIZE(1) + 20);
  } else if ( inputSource == "server"     || 
              inputSource == "net_radio"  || 
              inputSource == "rhapsody"   || 
              inputSource == "napster"    || 
              inputSource == "pandora"    || 
              inputSource == "siriusxm"   || 
              inputSource == "spotify"    || 
              inputSource == "juke"       || 
              inputSource == "airplay"    || 
              inputSource == "radiko"     || 
              inputSource == "qobuz"        ) {
    musiccast.begin(baseAddress + 
                    "netusb/setPlayback?playback=" + 
                    playbackType);
    bufferSizeSong = size_t(JSON_OBJECT_SIZE(1) + 20);
  } else {
    return; //exit without doing anything
  }
  
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(bufferSizeSong);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
    if (root["response_code"] == 0) { //Music Cast Successful request
      //Request Succesful
    } else {
      //NOT updated
    }
  }
  musiccast.end();   //Close connection
  
  
  timeSinceLastUpdate = millis() - updateInterval + 50; 
  //Gives the system 50ms to sort its shit out before polling for
  // a change of track;
}

void muteFunc(String muteRequest) { // muteRequest true = mute, 
                                    // false = unmute, toggle = toggle
  getDeviceStatus();
  if (muteRequest == "toggle") {
    mute = !mute;
  } else if (muteRequest == "on" && powerStatus =="standby") {
    mute = true;
  } else if (muteRequest == "false" && mute) {
    mute = false;
  } else {
    return;
  }
  String muteString ="false";
  if (mute) {
    muteString ="true";
  }
  musiccast.begin(baseAddress + "main/setMute?enable=" + muteString);
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
    if (root["response_code"] == 0) { //Music Cast Successful request
      //Muted
    } else {
      //NOT updated
    }
  }
  musiccast.end();   //Close connection
}

void powerFunc(String powerRequest) { // powerRequest = "on", "off", 
                                      // "toggle"
  getDeviceStatus();
  if (powerRequest == "toggle") {
    if (powerStatus == "on") {
      powerRequest = "standby";
    } else if (powerStatus == "off" || powerStatus == "standby") {
      powerRequest = "on";
    }
  }
  musiccast.begin(baseAddress + "main/setPower?power=" + powerRequest);
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
    if (root["response_code"] == 0) { //Music Cast Successful request
      //Muted
    } else {
      //NOT updated
    }
  }
  musiccast.end();   //Close connection
}

void changeInput(String inputName) { // inputName = "input_name", 
                                     // "toggleForward", "toggleBack"
  getDeviceStatus();
  getSourceNo();
  String tempInput = inputName;
  int nextInput;
  if (inputName == "toggleForward") {
    nextInput = inputNo + 1;
    if (nextInput > maxInputNo) {
      nextInput = 0;
    }
    tempInput = inputArray[nextInput];
  } else if (inputName == "toggleBack") {
    nextInput = inputNo - 1;
    if (nextInput < 0) {
      nextInput = maxInputNo;
    }
    tempInput = inputArray[nextInput];
  }
  String tunerTemp = tempInput;
  int tunerSource = false;
  if (tempInput == "dab" || 
      tempInput == "rds" || 
      tempInput == "fm" || 
      tempInput == "am") {
    tunerSource = true;
    tempInput = "tuner";

  }
  
  musiccast.begin(baseAddress + "main/setInput?input=" + tempInput);
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());

    if (root["response_code"] == 0) { //Music Cast Successful request
      //Input Updated
    } else {
      //Input NOT updated
    }
  }
  musiccast.end();   //Close connection
  if (tunerSource) {
    musiccast.begin(baseAddress +
                    "tuner/recallPreset?zone=main&band=" + 
                    tunerTemp + "&num=1");
    httpCode = musiccast.GET();
    //Check the returning code
    if (httpCode > 0) {
      // Parsing
      DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
      JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
  
      if (root["response_code"] == 0) { //Music Cast Successful request
        //Input Updated
      } else {
        //Input NOT updated
      }
    }
    musiccast.end();
  }
}

void repeatFunc(String repeatRequest) { // powerRequest = "on", "off", 
                                        // "toggle"
  getSongInfo();
  bool toggle = false;
  if (repeatRequest == "on" && repeat == "off") {
    toggle = true;
  } else if (repeatRequest == "on" && repeat == "off") {
    toggle = true;
  } else if (repeatRequest == "toggle") {
    toggle = true;
  }
  if (toggle) {
    musiccast.begin(baseAddress + "netusb/toggleRepeat");
    httpCode = musiccast.GET();
    //Check the returning code
    if (httpCode > 0) {
      // Parsing
      DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
      JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
      if (root["response_code"] == 0) { //Music Cast Successful request
        //Muted
      } else {
        //NOT updated
      }
    }
    musiccast.end();   //Close connection
  }
}

void shuffleFunc(String shuffleRequest) { // powerRequest = "on", "off", 
                                          // "toggle"
  getSongInfo();
  bool toggle = false;
  if (shuffleRequest == "on" && shuffle == "off") {
    toggle = true;
  } else if (shuffleRequest == "on" && shuffle == "off") {
    toggle = true;
  } else if (shuffleRequest == "toggle") {
    toggle = true;
  }
  if (toggle) {
    musiccast.begin(baseAddress + "netusb/toggleShuffle");
    httpCode = musiccast.GET();
    //Check the returning code
    if (httpCode > 0) {
      // Parsing
      DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + 20);
      JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
      if (root["response_code"] == 0) { //Music Cast Successful request
        //Muted
      } else {
        //NOT updated
      }
    }
    musiccast.end();   //Close connection
  }
}

void playPauseFunc(String playPauseRequest) { // playPauseRequest = "play" 
                                              // "pause", "toggle"
  if (playPauseRequest == "toggle") {
    if(playback == "play") {
      playbackFunc("pause");
    } else if (playback == "pause" || playback == "stop") {
      playbackFunc("play");
    }
  } else if (playPauseRequest == "play") {
    playbackFunc("play");
  } else if (playPauseRequest == "pause") {
    playbackFunc("pause");
  }
}

/*///////////////////////////////////////////////////////////////////////*/
/* Get Musiccast Device Info                                             */
/*///////////////////////////////////////////////////////////////////////*/
void getDeviceStatus() {
  //Get Device Status (Power, Volume, Current Input, Tone, Balance);
  musiccast.begin(baseAddress + "main/getStatus");
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSizeStatus = JSON_OBJECT_SIZE(3) + 
                                    JSON_OBJECT_SIZE(14) + 
                                    290;
    DynamicJsonBuffer jsonBuffer(bufferSizeStatus);
  
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());
  
    if (root["response_code"] == 0) { //Music Cast Successful request
      powerStatus = root["power"].as<String>(); // "standby"
      sleep = root["sleep"]; // 0
      currentVolume = root["volume"]; // 13
      volumeUpdated = true;
      mute = root["mute"]; // false
      maxVolume = root["max_volume"]; // 60
      inputSource = root["input"].as<String>(); // "spotify"

      JsonObject& tone_control = root["tone_control"];
      toneControlMode = tone_control["mode"].as<String>(); // "manual"
      bass = tone_control["bass"]; // 2
      treble = tone_control["treble"]; // 1
      balance = root["balance"]; // 0
      
    } else if (root["response_code"] == 1) {
      //Device is initializing
    }
  }
  musiccast.end();   //Close connection
  if (tempVolume != currentVolume) {
    lastVolumeChange = millis();
  }
  tempVolume = currentVolume;
}

void getSongInfo() {
  //Get Current Song Info
  unsigned int bufferSizeSong;
  if (inputSource == "cd" || inputSource == "audio_cd") {
    musiccast.begin(baseAddress + "cd/getPlayInfo");
     bufferSizeSong = size_t( JSON_ARRAY_SIZE(2) + 
                              JSON_ARRAY_SIZE(3) + 
                              JSON_OBJECT_SIZE(15) + 
                              250);
  } else if (inputSource == "tuner") {
    musiccast.begin(baseAddress + "tuner/getPlayInfo");
    bufferSizeSong = size_t(  2*JSON_OBJECT_SIZE(4) + 
                              JSON_OBJECT_SIZE(6) + 
                              JSON_OBJECT_SIZE(16) + 
                              450);
  } else {
    musiccast.begin(baseAddress + "netusb/getPlayInfo");
    bufferSizeSong = size_t(  2*JSON_ARRAY_SIZE(0) + 
                              JSON_OBJECT_SIZE(18) + 
                              420);
  }
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    DynamicJsonBuffer jsonBuffer(bufferSizeSong);

    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());

    if (root["response_code"] == 0) { //Music Cast Successful request
      tunerBand = root["band"].as<String>(); // "fm"
      playback = root["playback"].as<String>(); // "play"
      repeat = root["repeat"].as<String>(); // "off"
      shuffle = root["shuffle"].as<String>(); // "off"
      artist = root["artist"].as<String>(); // "Arctic Monkeys"
      album = root["album"].as<String>();
      track = root["track"].as<String>(); // "Science Fiction"
      trackNo = root["track_number"]; // "1"
      totalTracks = root["total_tracks"];
      playTime = root["play_time"];
      totalTime = root["total_time"];
    }
  }
  musiccast.end();   //Close connection
}

void getDeviceName() {
  musiccast.begin(baseAddress + "system/getNetworkStatus");
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSizeStatus = 2*JSON_OBJECT_SIZE(3) + 
                                    2*JSON_OBJECT_SIZE(5) + 
                                    JSON_OBJECT_SIZE(15) + 620;
    DynamicJsonBuffer jsonBuffer(bufferSizeStatus);

    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());

    if (root["response_code"] == 0) { //Music Cast Successful request
      networkName = root["network_name"].as<String>();
    }
  }
  musiccast.end();   //Close connection
}

void getAvailableInputs() {
  musiccast.begin(baseAddress + "system/getFeatures");
  httpCode = musiccast.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSizeInputs = 6*JSON_ARRAY_SIZE(1) + 
                                    3*JSON_ARRAY_SIZE(2) + 
                                    4*JSON_ARRAY_SIZE(3) + 
                                    JSON_ARRAY_SIZE(9) + 
                                    JSON_ARRAY_SIZE(11) + 
                                    2*JSON_ARRAY_SIZE(16) + 
                                    5*JSON_OBJECT_SIZE(1) + 
                                    2*JSON_OBJECT_SIZE(2) + 
                                    JSON_OBJECT_SIZE(3) + 
                                    6*JSON_OBJECT_SIZE(4) + 
                                    16*JSON_OBJECT_SIZE(5) + 
                                    JSON_OBJECT_SIZE(7) + 
                                    2*JSON_OBJECT_SIZE(8) + 
                                    3010;
    DynamicJsonBuffer jsonBuffer(bufferSizeInputs);
    JsonObject& root = jsonBuffer.parseObject(musiccast.getString());

    if (root["response_code"] == 0) { //Music Cast Successful request
      JsonObject& system = root["system"];
      JsonArray& system_input_list = system["input_list"];
      int j = 0;
      for (int i = 0; i < 100; i++) {
        bool enabled = system_input_list[i]["distribution_enable"];
        String temp_source = system_input_list[i]["id"].as<String>();
        if (temp_source == "" || j >= arraySize) {
          maxInputNo = j-1;
          #ifdef serialDebug
            Serial.print("End of inputs at j = ");
            Serial.println(j);
            Serial.write(4);
          #endif
          break;
        } else if (temp_source == "xxxxx" //to ensure it will compile
                    #ifndef spotify
                      || temp_source == "spotify"
                    #endif
                    #ifndef napster
                      || temp_source == "napster"
                    #endif
                    #ifndef juke
                     || temp_source == "juke"
                    #endif
                    #ifndef qobuz
                     || temp_source == "qobuz"
                    #endif
                    #ifndef tidal
                     || temp_source == "tidal"
                    #endif
                    #ifndef deezer
                     || temp_source == "deezer"
                    #endif
                    #ifndef mc_link
                     || temp_source == "mc_link"
                    #endif
                    #ifndef server
                     || temp_source == "server"
                    #endif
                    #ifndef bluetooth
                     || temp_source == "bluetooth"
                    #endif
                    #ifndef aux
                     || temp_source == "aux"
                    #endif
                    #ifndef aux1
                     || temp_source == "aux1"
                    #endif
                    #ifndef aux2
                     || temp_source == "aux2"
                    #endif
                    #ifndef net_radio
                     || temp_source == "net_radio"
                    #endif
                    #ifndef usb
                     || temp_source == "usb"
                    #endif
                    #ifndef cd
                     || temp_source == "cd"
                    #endif
                    #ifndef optical
                     || temp_source == "optical"
                    #endif
                    #ifndef airplay
                     || temp_source == "airplay"
                    #endif
                    ) {
            //Do Nothing if not defined i.e. don't add to source list
        } else { //Source is defined so add to the list
          if (temp_source == "tuner") {
            #ifdef dab
              inputArray[j] = "dab";
            #endif
            #ifdef fm
              inputArray[j] = "fm";
            #endif
          } else {
            inputArray[j] = temp_source;
          }
          #ifdef serialDebug
            Serial.print("New Input Source: ");
            Serial.println(temp_source);
            Serial.write(4);
          #endif
          j++;
        }
      }
    }
  }
  musiccast.end();   //Close connection
}

/*///////////////////////////////////////////////////////////////////////*/
/* Other Functions                                                       */
/*///////////////////////////////////////////////////////////////////////*/
void getSourceNo() { //Calculates the array number of the current source
  for (int i = 0; i <= maxInputNo; i++) {
    String tempInputSource = inputSource;
    if (inputSource == "tuner") { //because DAB FM etc appear as tuner in 
                                  // the source we need to compare tghe 
                                  // inputArray to the tunerBand
      tempInputSource = tunerBand;
    }
    
    if (tempInputSource == inputArray[i]) {
      inputNo = i;
      break;
    }
  }
}

void serialDashboardUpdate() {
  #ifdef serialDebug
    //Write Data to the serial
    Serial.write(12); //Clear terminal window
    Serial.println(timeSinceLastUpdate);
    Serial.println(networkName);
    Serial.print("Power Status: ");
    Serial.println(powerStatus);
    Serial.print("Input Source: ");
    if (inputSource == "tuner") {
      Serial.println(tunerBand);
    } else {
      Serial.println(inputSource);
    }
    Serial.print("Selected Input: ");
    Serial.println(inputArray[selectedInputNo]);
    Serial.print("Volume: ");
    if (mute) {
      Serial.println("Mute");
    } else {
      Serial.print(tempVolume);
      Serial.print(" / ");
      Serial.println(maxVolume);
    }
    Serial.print("Playback status: ");
    if (inputSource == "airplay" || 
        inputSource == "aux1" || 
        inputSource == "aux2") {
      Serial.println("N/A"); //airplay can't be controlled coz Apple
    } else {
      Serial.println(playback);
    }
    if (playback != "stop") {
      if(artist != "") {
        Serial.print("Artist: ");
        Serial.println(artist);
      }
      if(album != "") {
        Serial.print("Album: ");
        Serial.println(album);
      }
      if(track != "") {
        Serial.print("Track: ");
        Serial.println(track);
      }
      if (inputSource == "cd") {
        Serial.print("Track No: ");
        Serial.println(trackNo);
        Serial.print("Play Time: ");
        int minutes = floor(playTime / 60);
        Serial.print(minutes);
        Serial.print(":");
        int seconds = playTime % 60;
        if (seconds < 10) {
          Serial.print("0");
        }
        Serial.print(seconds);
        if (totalTime > 0) {
          Serial.print(" | ");
          Serial.println(totalTime);
        }
        Serial.println("");
      }
    }
    Serial.write(4); //Update Terminal Window 
  #endif
  display.clearDisplay();
  if (powerStatus =="on") { //Song Display
    String songDetails;
    if (artist != "") {
      songDetails = artist;
    }
    if  (track != "") {
      songDetails += " - " + track;
    }
    display.setTextSize(2);
    display.setTextColor(WHITE);
    if (playback == "play") {
      display.fillTriangle(10, 4, 14, 8, 10, 12, WHITE);
    } else if (playback == "pause") {
      display.fillRect(10, 5, 2, 8, WHITE);
      display.fillRect(14, 5, 2, 8, WHITE);
    } else if (playback == "stop") {
      display.fillRect(10, 5, 6, 8, WHITE);
    } else {
      display.setCursor(10,0);
    }
    display.setCursor(20,0);
    String temp = inputArray[selectedInputNo];
    String upper = String(temp.charAt(0));
    upper.toUpperCase();
    temp.remove(0,1);
    display.print(upper + temp);
    display.setTextSize(1);
    if (millis() - lastVolumeChange <= volumeHangTime || 
                   (mute && muteOverride)) {
      display.setCursor(10,24); 
      if (tempVolume <= 0 || mute) {
        display.print("MUTE");
      } else if (tempVolume >= maxVolume) {
        display.print("MAX");
      } else {
        display.print("Volume: " + String(tempVolume));
      }    
    } else {
      display.setCursor(10,16);
      display.print(songDetails.substring(0,19));
      display.setCursor(10,24);
      if (songDetails.substring(19,20) == " ") {
        display.print(songDetails.substring(20));
      } else {
        display.print(songDetails.substring(19));
      }
    }
    display.display();
  } else { //if powerStatus != "on"
    if (millis() - lastOffChange <= offHangTime) {
      display.setTextSize(1);
      display.setCursor(10,0);
      display.print("System Off");
    }
    display.display();
    
  }
}

/*-----------------------------------------------------------------------*/
// END OF FILE                                                           //
/*-----------------------------------------------------------------------*/

//#define debug
//#define debugWLAN
//#define testline
//#define MQTTdebug

// Konfigurationen +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  #ifndef STASSID
  #define STASSID "IoT"
  #define STAPSK  "_kV+07hUu7=n"
  #endif

  byte mac[] = {0x02, 0xE0, 0xFB, 0x9A, 0x78, 0x40};

  uint16_t interval           = 5000;                 // [ms] Interval in dem die Uhrzeit vom Internet akuallisiert wird
  uint8_t regenbogeninterval  = 50;

  #define LDR         A0
  //#define LED_PIN    GPIO3 = RXD0 -> ist fix für esp8266
  #define ModePin     D1
  #define UpPin       D2
  #define DownPin     D3

  #define LED_COUNT 114

  const char* url = "http://time.strangetec.com";
  const byte port = 80;
// Konfigurationen  Ende -----------------------------------------------------------------------

// Initialisierungen +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  #include <ESP8266WiFi.h>
  //#include <ESP8266WiFiMulti.h>
  #include <ESP8266HTTPClient.h>
  #include <NeoPixelBrightnessBus.h>          // https://github.com/Makuna/NeoPixelBus
  #include <Ethernet.h>

  #include "constants.h"

  int16_t	matrix[10] = {0,0,0,0,0,0,0,0,0,0};
  RgbColor  farbe;
  uint8_t   LdrVal = 0;
  uint8_t   ColCount = 0;
  uint16_t  regenbogenfarbe = 0;
  uint32_t  regenbogentimer;
  uint32_t  previousMillis = 0;  
  uint8_t   Brightness = 150;
  uint8_t   pixel = 0;
  uint8_t   oldSecond = 0;
  uint8_t   SwitchMode = 1;
  uint8_t   ColorSw = 0;
  bool      actMode = false;
  bool      oldAct = false;
  bool      actUp = false;
  bool      oldUp = false;
  bool      actDown = false;

  bool      ClockOnOff  = true;
  bool      remoteBrightnessSw  = false;
  uint8_t   remoteBrightnessVal = 100;

  const char* ssid     = STASSID;
  const char* password = STAPSK;

  //ESP8266WiFiMulti WiFiMulti;
  WiFiClient client;

  typedef struct datetimestruct DatetimeStruct;
  DatetimeStruct data;

  //NeoPixelBrightnessBus<NeoGrbwFeature, Neo800KbpsMethod> strip(LED_COUNT);
  NeoPixelBrightnessBus<NeoGrbwFeature, NeoSk6812Method> strip(LED_COUNT, PixelPin);

// Initialisierungen ---------------------------------------------------------------------------

// setup +
void setup() {

  Serial.begin(115200);
  Serial.println();

  pinMode(ModePin,  INPUT);
  pinMode(UpPin,    INPUT);
  pinMode(DownPin,  INPUT);

  strip.Begin();            // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.SetBrightness(50);  // Set BRIGHTNESS to about 1/5 (max = 255)
  strip.ClearTo(black);     // Set all pixel colors to 'off'
  mergeMatrix(View_Startup);
  writeNeo(RGBblue);
  strip.Show();             // Turn OFF all pixels ASAP

  // We start by connecting to a WiFi network
  //WiFiMulti.mode(WIFI_STA);
  //WiFiMulti.addAP(ssid, password);
  //WiFiMulti.begin(STASSID, STAPSK);
  WiFi.begin(ssid, password);

  Serial.println("Wait for WiFi... ");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    clearMatrix();
    mergeMatrix(View_keinNetz);
    writeNeo(RGBred);
    strip.Show();
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // handle mqtt
  device.setName("Qlock Roman");
  device.setSoftwareVersion("1.0.2");

  // Clock on off
  clocksw.setName("Clock On/Off");
  clocksw.onStateChanged(onSwitchStateChanged);
  // Menü Button
  menusw.setName("Menu Button");
  menusw.onStateChanged(onSwitchStateChanged);
  // Up Button
  upsw.setName("Up Button");
  upsw.onStateChanged(onSwitchStateChanged);
  // Down Button
  downsw.setName("Down Button");
  downsw.onStateChanged(onSwitchStateChanged);
  
  // brightness set local / remote, remote brightness
  brightVal.setName("brightness remote");
  brightVal.setRetain(true);
  brightVal.setSpeedRangeMin(6);
  brightVal.setSpeedRangeMax(255);

  // handle brightness remote
  brightVal.onStateChanged(onBrightStateChanged);
  brightVal.onSpeedChanged(onBrightValChanged);

  #ifdef MQTTdebug
    mqtt.onMessage(onMqttMessage);
    mqtt.onConnected(onMqttConnected);
    mqtt.onConnectionFailed(onMqttConnectionFailed);
  #endif

  mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

  delay(500);
}
// setup -

// loop start ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void loop() {

  // *** Testing all in row ***
  #ifdef testline
    clearMatrix();
    mergeMatrix(View_keinNetz);
    writeNeo(RGBgreen);
    strip.Show();
  #else
  // ****** end testing *******

  // Internetz verbindung start ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();
      #ifdef debug  
        Serial.print("connecting to ");
        Serial.print(url);
        Serial.print(':');
        Serial.println(port);
      #endif

      // Use WiFiClient class to create TCP connections
      WiFiClient client;
      HTTPClient http;

      if (!client.connect(url, port)) {
        #ifdef debug 
          Serial.println("connection failed");
          Serial.println("wait 5 sec...");
        #endif
        clearMatrix();
        mergeMatrix(View_keinNetz);
        writeNeo(RGBred);
        strip.Show();
        delay(5000);
        return;
      }

      #ifdef debug 
        //read back one line from server
        Serial.println("receiving from remote server");
      #endif  

      http.begin(client, url);                                              //Specify request destination
      
      int httpCode = http.GET();                                            //Send the request
      
      #ifdef debugWLAN 
        Serial.println(httpCode);                                             //Print HTTP return code
      #endif
      if (httpCode > 0) {
          String timestamp = http.getString();                              //Get the response payload
          #ifdef debugWLAN       
            Serial.println(timestamp);                                      //Print request response payload
          #endif
          data.year = (unsigned int) timestamp.substring(2,4).toInt();
          data.month = (unsigned int) timestamp.substring(5,7).toInt();
          data.day = (unsigned int) timestamp.substring(8,10).toInt();
          data.wday = (unsigned int) timestamp.substring(11,12).toInt() + 1; // RTC from 1-7 instead 0-6
          data.hour = (unsigned int) timestamp.substring(13,15).toInt();
          data.minute = (unsigned int) timestamp.substring(16,18).toInt();
          data.second = (unsigned int) timestamp.substring(19,21).toInt();
          #ifdef debug       
            Serial.print("Es ist genau ");
            Serial.print(data.hour);
            Serial.print(" Uhr, ");
            Serial.print(data.minute);
            Serial.print(" Minuten und ");
            Serial.print(data.second);
            Serial.println(" Sekunden");
          #endif
      }
      
      #ifdef debugWLAN 
        Serial.println("closing connection");
      #endif
      client.stop();
    }  
  // Internetz verbindung ende -------------------------------------------------------------------

  // Taster einlesen +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    actMode	= digitalRead(ModePin);
    actUp 	= digitalRead(UpPin);
    actDown	= digitalRead(DownPin);

    if ((actMode == true or actModeRemote == true) and oldAct == false) {SwitchMode++; oldAct = true;}
    if (actMode == false and oldAct == true) {
      oldAct = false;
      actModeRemote = false;
      mqtt.publish("homeassistant/switch/005ce674b7eb/menusw/state", "OFF");
    }
    if (SwitchMode > 3) {SwitchMode = 1;}

    if ((actUp == true or actUpRemote == true) and oldUp == false) {ColorSw++; oldUp = true;}
    if (actUp == false and oldUp == true) {
      oldUp = false; 
      actUpRemote = false;
      mqtt.publish("homeassistant/switch/005ce674b7eb/upsw/state", "OFF");
    }
    if (ColorSw > 10) {ColorSw = 0;}
  // Taster einlesen ende ------------------------------------------------------------------------

  // Helligkeit berechnen ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (remoteBrightnessSw) {
      strip.SetBrightness(remoteBrightnessVal); 
    }
    else {
      LdrVal = analogRead(LDR);
      // ldr < 50 = dunkel, > 950 = maxhell
      if (LdrVal < 50) {
        LdrVal = 50;
      }
      if (LdrVal > 950) {
        LdrVal = 1024;
      }
      uint8_t BrightnessRaw = map(LdrVal, 50, 1024, 16, 254);
      if (BrightnessRaw < Brightness){Brightness = Brightness -1;}
      if (BrightnessRaw > Brightness){Brightness = Brightness +1;} 
      strip.SetBrightness(Brightness);

      #ifdef debug
        Serial.print("LDR Val: "); Serial.println(LdrVal);
        Serial.print("Brightness: "); Serial.println(Brightness);
      #endif
    }
    

  // Helligkeit berechnen ende -------------------------------------------------------------------

  // Neo's setzen start ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    generateClockMatrix(data.hour, data.minute);
    writeNeo(ignoreColor);
    strip.Show();
    
  // Neo's setzen ende ---------------------------------------------------------------------------

  #endif    // testline if

}
// loop ende -----------------------------------------------------------------------------------


// *********************************************************************************************
// *********************************************************************************************


void mergeMatrix(int in[]) {
  int pix = 11;
	for(int row=0; row<10; row++) 
  {
    for(int col=0; col<pix; col++) 
    {
      if(bitRead(in[row], col) == 1)
      {
        bitWrite(matrix[row], col, 1);
      }
    }
	}
}


void generateClockMatrix(unsigned short h, unsigned short m) {
	if(h >= 12)
		h -= 12;
	
	clearMatrix();
	// Standard-Text (immer sichtbar)
	mergeMatrix(View_EsIst);
  	//// Einzelne Minuten (1-4)
	switch(m%5){
		case 1:mergeMatrix(View_Min_1);break;
		case 2:mergeMatrix(View_Min_2);break;
		case 3:mergeMatrix(View_Min_3);break;
		case 4:mergeMatrix(View_Min_4);break;
	}
	// Minuten: 5er-Schritte
	int h_offset = 0;
	switch(m/5){
		case  0:mergeMatrix(View_Uhr);break; // ??:00
		case  1:mergeMatrix(View_Min05);break; // ??:05
		case  2:mergeMatrix(View_Min10);break; // ??:15
		case  3:mergeMatrix(View_Min15);break; // ??:15
		case  4:mergeMatrix(View_Min20);break; // ??:20
		case  5:mergeMatrix(View_Min25);h_offset=1;break; // ??:25
		case  6:mergeMatrix(View_Min30);h_offset=1;break; // ??:30
		case  7:mergeMatrix(View_Min35);h_offset=1;break; // ??:35
		case  8:mergeMatrix(View_Min40);h_offset=1;break; // ??:40
		case  9:mergeMatrix(View_Min45);h_offset=1;break; // ??:45
		case 10:mergeMatrix(View_Min50);h_offset=1;break; // ??:50
		case 11:mergeMatrix(View_Min55);h_offset=1;break; // ??:55
	}
	// Stunden
	switch(h + h_offset){
		case  1:mergeMatrix(View_Hour01);break;
		case  2:mergeMatrix(View_Hour02);break;
		case  3:mergeMatrix(View_Hour03);break;
		case  4:mergeMatrix(View_Hour04);break;
		case  5:mergeMatrix(View_Hour05);break;
		case  6:mergeMatrix(View_Hour06);break;
		case  7:mergeMatrix(View_Hour07);break;
		case  8:mergeMatrix(View_Hour08);break;
		case  9:mergeMatrix(View_Hour09);break;
		case 10:mergeMatrix(View_Hour10);break;
		case 11:mergeMatrix(View_Hour11);break;
		default:mergeMatrix(View_Hour12);break;
	}
	// Stunden-Korrektur 'Eins' -> 'Ein'
	if(h%12 == 1 and m/5 == 0) {bitWrite(matrix[5], 7, 0);}
}


void writeNeo(RgbColor color) {
  byte pixE = 11;
	byte px   = 0;
  byte res  = 0;
  for(byte row=0; row<10; row++)
  {
    for(byte col=0; col<pixE; col++)
      {
      res = col;
      if(bitRead(matrix[row], res) == 1 ) {

        if (color != ignoreColor) {
          strip.SetPixelColor(px, RgbColor(color));
        }
        else {
        // Farbe berechnen +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
          switch (SwitchMode) {
            
            case 1: // sekuendlich angepasste farben
              if (data.second != oldSecond) {
                oldSecond = data.second;
                regenbogenfarbe = (regenbogenfarbe + 1);
                if (regenbogenfarbe == 360) {
                  regenbogenfarbe = 0;
                  }
                }
              strip.SetPixelColor(px, HslColor((float)regenbogenfarbe / 360.0f, 1.0f, 0.25f));
              break;
            
            case 2: // Regenbogenfarben
              if (millis() - regenbogentimer  > regenbogeninterval) {
                regenbogentimer = millis();
                regenbogenfarbe = (regenbogenfarbe + 1);
                if (regenbogenfarbe == 360) {
                  regenbogenfarbe = 0;
                }
              }
              strip.SetPixelColor(px, HslColor(((float)regenbogenfarbe - (float)px) / 360.0f, 1.0f, 0.25f));
              break;

            case 3: // Fixe Farben
              strip.SetPixelColor(px, HslColor(RgbColor(FarbVal[ColorSw])));
              break;
          }                
        }
        // Farbe berechnen ende ------------------------------------------------------------------------

        #ifdef debug
          if(pixE == 13 and px == 0) {Serial.print(" ");}
          Serial.print(" X ");
        #endif
      }
      else
      {
        #ifdef debug
          if(pixE == 13 and px == 0) {Serial.print(" ");}
          Serial.print(" . ");
        #endif
        strip.SetPixelColor(px, black);
      }
    px++;
    }
		#ifdef debug
			Serial.println();
		#endif
  }
}


void clearMatrix() {
  for(int row=0; row<10; row++)
  {
	  matrix[row] = View_Blank[row];
  }
}

/*
void onBeforeSwitchStateChanged(bool state, HASwitch* s)
{
    // this callback will be called before publishing new state to HA
    // in some cases there may be delay before onStateChanged is called due to network latency
}
*/
void onSwitchStateChanged(bool state, HASwitch* s)
{
  const char* SwitchId = s -> uniqueId();
  if (SwitchId == "clock") {
  }
  if (SwitchId == "menusw") {
    actModeRemote = true;
  }
  if (SwitchId == "upsw") {
    actUpRemote = true;
  }
  if (SwitchId == "downsw") {
    actDownRemote = false;
  }
}

void onBrightStateChanged(bool state) {
  remoteBrightnessSw = state;
}

void onBrightValChanged(uint16_t brightness) {
  remoteBrightnessVal = brightness;
}
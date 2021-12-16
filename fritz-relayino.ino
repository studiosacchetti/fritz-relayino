#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>


/**** CONFIGURATION ****************************/
#define    RING_LED_PIN                         2             // LED notification if call was missed/not picked up
#define    RELAY_PIN                            5             // relay pin to actuate device
#define    CLEAR_RING_BUTTON_PIN                8             // Attached button to clear missed calls
#define    CLEAR_MISESED_CALL_ON_NEW_ACTION     0             // Clear missed call state if new action was done
#define    FRITZBOX_HOSTNAME                    "fritz.box"   // Host name of FritzBox
#define    FRITZBOX_PORT                        1012          // Port with running FritzBox call monitor
#define    RETRY_TIMEOUT                        5000          // Retry connection to FB every x seconds
#define    CALL_DURATION_UPDATE_INTERVAL        1000          // Update ongoing call duration on LCD
#define    DEBUG                                1             // Enable serial debugging
#define    SERIAL_BAUD_RATE                     115200        // Baud rate for serial communication
#define    CHECKCONNECTION                      10000         // Milliseconds
#define    DEVICE_NAME                          "ESP-relay"   // Device name

const char* ssid     = "ssnet_nomap";                         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "pippo2021";                           // The password of the Wi-Fi network
//const char* IP = "192.168.0.254";                           // IP address of your router. This should be "192.168.179.1" for most FRITZ!Boxes
/***********************************************/


/**** GLOBAL VARIABLES *************************/
WiFiClient client;

unsigned long next;
unsigned long callstart;
unsigned long calllaststatus;
unsigned long connectioncheck;

byte missedcallcount;


boolean call_connected;
boolean lastcallwasmissedcall;


/***********************************************/


void setup() {
  Serial.begin(SERIAL_BAUD_RATE);                 // Start the Serial communication to send messages to the computer
  delay(10);

  WiFi.hostname(DEVICE_NAME);
  //WiFi.config(STATIC_IP, SUBNET, GATEWAY, DNS);
  WiFi.begin(ssid, password);             // Connect to the network
  //WiFi.mode(WIFI_STA);
#ifdef DEBUG
  Serial.println(F("[FritzBox CallMonitor]\n"));
  Serial.print("Connecting to ");
  Serial.print(ssid);
#endif
  next = 0;
  call_connected = false;
  missedcallcount = 0;
  lastcallwasmissedcall = 0;

  //pinMode(CLEAR_RING_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RING_LED_PIN, OUTPUT);
  digitalWrite(RING_LED_PIN, HIGH);


  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
#ifdef DEBUG
    Serial.print('.');
#endif
  }

#ifdef DEBUG
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("Subnet Mask: "));
  Serial.println(WiFi.subnetMask());
  Serial.print(F("Gateway IP: "));
  Serial.println(WiFi.gatewayIP());
#endif

}

/**** METHOD: RESETETHERNET ********************/
void resetwifi() {
  client.stop();
  delay(1000);
  //WiFi.begin(mac);
  //delay(1000);
}
/***********************************************/

/**** METHOD: LCDON ****************************/
void relayon() {
  digitalWrite(RING_LED_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.print("ledon");
}
/***********************************************/

/**** METHOD: LCDOFF ***************************/
void relayoff() {
  digitalWrite(RING_LED_PIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);
  Serial.print("ledoff");
}
/***********************************************/


/**** METHOD: MISSEDCALLLEDON ******************/
void missedcallledon() {
  missedcallcount++;
  digitalWrite(RING_LED_PIN, LOW);

}
/***********************************************/

/**** METHOD: MISSEDCALLLEDOFF *****************/
void missedcallledoff() {
  if (missedcallcount > 0)  {
    digitalWrite(RING_LED_PIN, HIGH);
    missedcallcount = 0;
  }
}
/***********************************************/


/**** METHOD: LOOP *****************************/
void loop() {

  if ((millis() - next) > RETRY_TIMEOUT) {
    next = millis();
#ifdef DEBUG
    Serial.println(F("Trying to connect..."));
#endif

    if (client.connect(FRITZBOX_HOSTNAME, FRITZBOX_PORT)) {

      if (client.connected()) {
#ifdef DEBUG
        Serial.println(F("Connected successfully"));
#endif
        connectioncheck = millis();
      }

      while (client.connected()) {
        // if in a call
        if (call_connected && ((millis() - calllaststatus) > CALL_DURATION_UPDATE_INTERVAL)) {
          //unsigned long seconds = (millis() - callstart) / 1000;
          //Serial.print(seconds);
        }

        if (!digitalRead(CLEAR_RING_BUTTON_PIN)) {
          missedcallledoff();
        }

        int size;

        while ((size = client.available()) > 0) {
          uint8_t* msg = (uint8_t*)malloc(size);
          size = client.read(msg, size);
          msg[size - 1] = '\0';

#ifdef DEBUG
          Serial.print(F("->Msg: "));
          Serial.println((char*)msg);
#endif

          // Copy of msg needed for strtok/strtok_r because of modification of original value (see https://www.securecoding.cert.org/confluence/display/seccode/STR06-C.+Do+not+assume+that+strtok%28%29+leaves+the+parse+string+unchanged)
          uint8_t* copymsgforsplit = (uint8_t*)malloc(size);
          memcpy(copymsgforsplit, msg, size);

          // Analyze incoming msg
          int i = 0;
          char *pch, *ptr;
          char type[11];
          pch = strtok_r ((char*)copymsgforsplit, ";", &ptr);

          while (pch != NULL) {

#ifdef DEBUG
            Serial.print(F("    ->Splitted part "));
            Serial.print(i);
            Serial.print(F(": "));
            Serial.println(pch);
#endif

            switch (i) {
              case 0:    // Date and Time
                if (CLEAR_MISESED_CALL_ON_NEW_ACTION)              {
                  missedcallledoff();
                }
                lastcallwasmissedcall = false;
                break;
              case 1:    // TYPE

                if (((strcmp(type, "RING")  == 0) && (strcmp(pch, "DISCONNECT")  == 0)) || (strstr((char*)msg, ";40;") && strcmp(type, "RING")  == 0 && strcmp(pch, "CONNECT") == 0))                {
                  missedcallledon();
                  lastcallwasmissedcall = true;
                }
                strcpy(type, pch);

                if (strcmp(type, "RING")  == 0) {
                  relayon();
                }

                if (strcmp(type, "CONNECT")  == 0) {
                  call_connected = true;
                  relayoff();
                }

                if (strcmp(type, "DISCONNECT")  == 0) {
                  call_connected = false;
                  /*if (!lastcallwasmissedcall) {
                    }*/
                }

                break;
              case 2:    // ConnectionID
                // Currently not needed...
                break;
              case 3: // numero del chiamante o durata chiamata
                break;
              case 4: // Connected with number
                break;
              case 5: // Calling number
                /*if (strcmp(type, "CALL")  == 0) {
                  }*/
                break;
              default:
                break;
            }
            i++;
            pch = strtok_r (NULL, ";", &ptr); // Split next part
          }

          free(msg);
          free(copymsgforsplit);
        }


        // Check connection
        if ((millis() - connectioncheck) > CHECKCONNECTION) {
#ifdef DEBUG
          Serial.println(F("Checking connection..."));
#endif
          connectioncheck = millis();

          // Send dummy data to "refresh" connection state
          client.write("x");
        }
      }
      //close:
      //disconnect client
#ifdef DEBUG
      Serial.println(F("Disconnected"));
#endif
      resetwifi();
    } else {
#ifdef DEBUG
      Serial.println(F("Connection failed, retrying..."));
#endif


    }
  }
}

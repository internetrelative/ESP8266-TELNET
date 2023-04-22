#include <ESP8266WiFi.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#define MAX_CLIENTS 2

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// set both client and server to end with New Line aka LF , enable local echo on client terminal

WiFiServer server(23); // Telnet port
IPAddress clientIP;

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long lastTime = millis();
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
    // Restart the ESP8266 after 10s  has passed until WiFi is connected
    if (millis() - lastTime >= 20000)
    {
      ESP.restart();
    }
  }
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void setup()
{
  Serial.begin(115200);
  initWiFi();
  Serial.println("\nConnected to WiFi\n\n");
  server.begin();
  Serial.println("TCP TELNET SERVER STARTED");
}

unsigned long previousMillis = 0;
unsigned long interval = 30000;

WiFiClient client[MAX_CLIENTS];
unsigned long int lastUpTime[MAX_CLIENTS] = {0};
void loop()
{
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    previousMillis = currentMillis;
  }

  else
  {
    uint8_t i;
    // If there's client waiting to connect to server
    if (server.hasClient())
    {
      for (i = 0; i < MAX_CLIENTS; i++)
      {
        // if there's available slot and client doesn't exist
        if (!client[i] || !client[i].connected())
        {
          client[i] = server.available();

          /////////////////////////////// KEEP ALIVE ///////////////////////////////
          client[i].keepAlive(5,5,2);
          Serial.printf("Keep Alive is enabled: %d\n", client[i].isKeepAliveEnabled());
          /////////////////////////////////////////////////////////////////////////

          Serial.printf("New client: %d %s\n\r", i, client[i].remoteIP().toString().c_str());
          client[i].write("Server -> Hello World\n\r"); // Greet user when they connect
          lastUpTime[i] = millis();
          break;
        }
      }
      if (i >= MAX_CLIENTS)
      {
        // reject client if there's no available slot
        Serial.println("Rejecting new client");
        server.available().stop();
      }
    }

    else
    {
      for (i = 0; i < MAX_CLIENTS; i++)
      {
        if (client[i] && client[i].connected())
        {
          // if client is connected, update lastUpTime
          lastUpTime[i] = millis();

          ///////////////////////////////////////  RECEIVING  DATA /////////////////////////////////////////
          bool firstLine = true;
          String inputString = "";

          while (client[i].available()) // if there's data from client
          {
            char c = client[i].read();

            if (isAscii(c) && (int(c) > 31 && int(c) < 127))
            {
              if (firstLine)
              {
                Serial.printf("Client %d -> ", i);
                firstLine = false;
              }
              Serial.write(c);
              inputString += c;
            }
            if (c == '\n')
            {
              firstLine = true;
              Serial.printf("\n\r");

              const char *cstr = inputString.c_str();
              if (strcmp(cstr, "restart") == 0)
              {
                ESP.restart();
              }
            }
          }
          ////////////////////////////////////////////////////////////////////////////////////////////////////
        }
      }
    }

    // check if client is disconnected
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      // check client is disconnected for more than 1000ms before closing the connection
      if (lastUpTime[i] != 0 && millis() - lastUpTime[i] >= 1000)
      {
        Serial.printf("Client %d disconnected\n\r", i);
        client[i].stop();
        lastUpTime[i] = 0;
      }
    }
  }

  // incoming serial data
  while (Serial.available())
  {
    String inputString = Serial.readStringUntil('\n');
    const char *cstr = inputString.c_str();

    if (strcmp(cstr, "restart") == 0)
    {
      ESP.restart();
    }

    ///////////////////////////////// SENDING DATA ///////////////////////////////////
    // send to all clients that are connected
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
      if (client[i] && client[i].connected())
      {
        lastUpTime[i] = millis();
        clientIP = client[i].remoteIP();
        Serial.printf("Sending to client %d -> %s\n\r", i, cstr);
        client[i].write("Server -> ");
        client[i].write(cstr);
        client[i].write("\n\r");
      }
    }
    /////////////////////////////////////////////////////////////////////////////////////
  }
}

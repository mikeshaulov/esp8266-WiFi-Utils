#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <DNSServer.h>
#include <EEPROM.h>
#include <string.h>
#include <ESP8266WebServer.h>
#include "FS.h"

#define DEBUG_PRINT(x) Serial.println(x)

class CLight
{
  public:
  int m_pin;

  CLight(int pin) : m_pin(pin)
  {
    pinMode(m_pin, OUTPUT);
  }

  void TurnOn()
  {
    digitalWrite(m_pin, HIGH);
  }

  void TurnOff()
  {
    digitalWrite(m_pin, LOW);
  }
};

static const char MAGIC_EEPROM_BUF[] = {0x9b, 0x3d, 0xb3, 0x62, 0xcd, 0xb9, 0xdb, 0x45, 0xac, 0xe5, 0x30, 0xd8, 0x4a, 0xfb, 0x04, 0x39};


class WiFiSTAConfigurator
{
  #define MAX_LEN 64
  
  public:
    /*
     * Constructor
     */
    WiFiSTAConfigurator() : m_configured(false){}
  
    /*
     * Reads configuration from flah
     */
    bool readCfg()
    {
      // read magic buffer to see if we ever stored a value
      EEPROM.begin(sizeof(MAGIC_EEPROM_BUF) + MAX_LEN + MAX_LEN);
      char magic_read[sizeof(MAGIC_EEPROM_BUF)];
      EEPROM.get(0, magic_read);
      if(memcmp(magic_read,MAGIC_EEPROM_BUF,sizeof(MAGIC_EEPROM_BUF)))
      {
        DEBUG_PRINT("magic buff was not found, SSID was not stored");
      }

      // read ssid
      char ssid_read[MAX_LEN];
      EEPROM.get(sizeof(MAGIC_EEPROM_BUF), ssid_read);
      int ssidLen = strlen(ssid_read);

      // check if ssid was found
      if(ssidLen == 0) 
      {
        DEBUG_PRINT("ssid size if 0, although magic field exists");
        return false;
      }
      else if(ssidLen > MAX_LEN)
      {
        DEBUG_PRINT("ssid size exceeds size, although magic field exists");
        return false;
      }
      // assign ssid
      m_ssid = (const char*) ssid_read;
      
      // read passwrod
      char pass_read[MAX_LEN];
      EEPROM.get(sizeof(MAGIC_EEPROM_BUF) + MAX_LEN, pass_read);
      int passLen = strlen(pass_read);

      // check pass, pass can be null for open WiFis
      if(passLen > MAX_LEN)
      {
        DEBUG_PRINT("pass size exceeds size, although magic field exists");
        return false;
      }

      m_pass = pass_read;

      m_configured = true;
      
      return true;
    }

    void writeCfg()
    {
      EEPROM.begin(sizeof(MAGIC_EEPROM_BUF) + MAX_LEN + MAX_LEN);
      EEPROM.put(0, MAGIC_EEPROM_BUF);
      EEPROM.put(sizeof(MAGIC_EEPROM_BUF), m_ssid);
      EEPROM.put(sizeof(MAGIC_EEPROM_BUF) + MAX_LEN, m_pass);
    }
    
    /*
     * return the SSID network, if not defined or not loaded will return an empty string
     */
    const String& SSID() {return m_ssid;}

    /*
     * return the SSID network password, if not defined or not loaded will return an empty string
     */
    const String& Password() {return m_pass;}

    /*
     * returns if configured
     */
    bool isConfigured() {return m_configured;}


    /*
     * return the SSID network, if not defined or not loaded will return an empty string
     */
    void setSSID(const String& ssid) {m_ssid = ssid;}

    /*
     * return the SSID network password, if not defined or not loaded will return an empty string
     */
    void setPassword(const String& pass) {m_pass = pass;}

    
  private:
    // magic buffer to check if the memory was initialized
    bool m_configured;
    String m_ssid;
    String m_pass;
    
};

class WiFiAPModule
{
  public:
    WiFiAPModule(String ssid) : m_ssid(ssid){}

    void init()
    {       
       DEBUG_PRINT("initializing WiFi module to AP mode");
       /* Soft AP network parameters */
       IPAddress _apIP(192, 168, 4, 1);
       IPAddress _netMsk(255, 255, 255, 0);

       WiFi.mode(WIFI_AP);
       WiFi.softAPConfig(_apIP, _apIP, _netMsk);
       WiFi.softAP(m_ssid.c_str());

       // wait for setup
       delay(1000);

       DEBUG_PRINT("AP IP address: ");
       DEBUG_PRINT(WiFi.softAPIP());

       DEBUG_PRINT("setting up DNS server");
       // redirect dns quries to local ip address
       _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
       _dnsServer.start(53, "*", _apIP);       

       DEBUG_PRINT("setting up DNS server");
    }

    void handleRequests()
    {
      _dnsServer.processNextRequest();
    }
    
  private:
    String m_ssid;
    
    /* DNS Server */
    DNSServer _dnsServer;    
};





class SSIDConfigurationWebServer : public ESP8266WebServer
{
public:
  SSIDConfigurationWebServer(int port) : ESP8266WebServer(port){}

  void init()
  {    
    // Init static server pointer. The static handlers depend on it
    __server = this;
    // Setup web pages: root, wifi config pages, SO captive portal detectors and not found. 
    on("/", handleRoot);
    on("/wifisave", handleWifiSave);
    onNotFound ( handleNotFound );
    begin(); // Web server start
    DEBUG_PRINT("HTTP server started");
  }

private:
  static SSIDConfigurationWebServer* __server;
  
  static void handleRoot() {
      __server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      __server->sendHeader("Pragma", "no-cache");
      __server->sendHeader("Expires", "-1");
      __server->send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
      __server->sendContent(
        "<html><head></head><body>"
        "<h1>HELLO WORLD!!</h1>");
      __server->client().stop();
    }

  static void handleWifiSave() {}

  static void handleNotFound() {}
};

SSIDConfigurationWebServer* SSIDConfigurationWebServer::__server = NULL;


WiFiSTAConfigurator staConfig;
WiFiAPModule wifiAPModule("TestAP");
// Web server
SSIDConfigurationWebServer server(80);

CLight light1(4);
CLight light2(5);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  light1.TurnOn();
  light2.TurnOn();
  //SPIFFS.begin();

//  if(!staConfig.readCfg())
//  {
//    wifiAPModule.init();
//    server.init();
//  }
}

void loop() {
//  wifiAPModule.handleRequests();
//  server.handleClient();
  
}

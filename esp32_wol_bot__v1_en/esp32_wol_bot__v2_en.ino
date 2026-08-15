/*
 * ============================================================
 *  ESP32 Wake-on-LAN Telegram Bot v1.0 (PRO + Sleep Edition)
 *  
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiUDP.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

// ============================================================
//  SETTINGS
// ============================================================

const char* BOT_TOKEN = "YOUR_BOT_TOKEN_HERE";
const char* ALLOWED_CHAT_ID = "YOUR_CHAT_ID_HERE";
static const uint8_t TARGET_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
const bool ENABLE_SLEEP_COMMAND = true; 


#define AP_NAME "ESP32-WOL-Setup"
#define WOL_PORT 9

// ============================================================
//  GLOBAL VARIABLES
// ============================================================

WiFiClientSecure secureClient;
WiFiUDP udp;
int32_t last_update_id = 0;
bool pending_reboot = false;

// ============================================================
//  FUNCTIONS
// ============================================================

String macToString(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String uptimeString() {
  unsigned long sec = millis() / 1000;
  unsigned long d = sec / 86400; sec %= 86400;
  unsigned long h = sec / 3600;  sec %= 3600;
  unsigned long m = sec / 60;    sec %= 60;

  String result;
  if (d > 0) result += String(d) + "d ";
  if (h > 0) result += String(h) + "h ";
  result += String(m) + "m " + String(sec) + "s";
  return result;
}

void sendMagicPacket(const uint8_t mac[6]) {
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; i++) {
    memcpy(&packet[6 + i * 6], mac, 6);
  }
  udp.beginPacket(IPAddress(255, 255, 255, 255), WOL_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
}

void sendSleepPacket(const uint8_t mac[6]) {
  char packet[64];
  snprintf(packet, sizeof(packet), "SLEEP:%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  udp.beginPacket(IPAddress(255, 255, 255, 255), WOL_PORT);
  udp.write((uint8_t*)packet, strlen(packet));
  udp.endPacket();
}

void sendTelegramMessage(const String& chat_id, const String& text) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
  
  http.begin(secureClient, url);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<512> doc;
  doc["chat_id"] = chat_id;
  doc["text"] = text;
  
  String payload;
  serializeJson(doc, payload);
  
  http.POST(payload);
  http.end();
}

void handleCommand(const String& chat_id, const String& text) {
  if (chat_id != ALLOWED_CHAT_ID) {
    sendTelegramMessage(chat_id, "🚫 Access denied.");
    return;
  }

  if (text.startsWith("/wake")) {
    sendMagicPacket(TARGET_MAC);
    sendTelegramMessage(chat_id, "🚀 Magic Packet sent to MAC: " + macToString(TARGET_MAC));
  } 
  else if (text.startsWith("/sleep")) {
    if (!ENABLE_SLEEP_COMMAND) {
      sendTelegramMessage(chat_id, "❌ The /sleep command is disabled in settings.");
      return;
    }
    
    sendTelegramMessage(chat_id, "⏳ Sending sleep command and waiting for PC response...");
    
    while (udp.parsePacket()) {
      udp.flush();
    }
    
    sendSleepPacket(TARGET_MAC);
    
    bool ack_received = false;
    unsigned long start = millis();
    
    while (millis() - start < 3000) {
      esp_task_wdt_reset();
      
      int packetSize = udp.parsePacket();
      if (packetSize) {
        char incomingPacket[255];
        int len = udp.read(incomingPacket, 255);
        if (len > 0) {
          incomingPacket[len] = 0;
        }
        if (String(incomingPacket) == "SLEEP_ACK") {
          ack_received = true;
          break;
        }
      }
      delay(10);
    }
    
    if (ack_received) {
      sendTelegramMessage(chat_id, "✅ PC accepted the command and is shutting down.");
    } else {
      sendTelegramMessage(chat_id, "⚠️ PC did not respond. It might be already off or the script is not running.");
    }
  }
  else if (text.startsWith("/status")) {
    String reply = "📊 Board Status\n"
                   "• State: Online\n"
                   "• IP: " + WiFi.localIP().toString() + "\n"
                   "• Uptime: " + uptimeString() + "\n"
                   "• RSSI: " + String(WiFi.RSSI()) + " dBm\n"
                   "• RAM: " + String(ESP.getFreeHeap() / 1024) + " KB";
    sendTelegramMessage(chat_id, reply);
  } 
  else if (text.startsWith("/reboot")) {
    sendTelegramMessage(chat_id, "🔄 Rebooting...");
    pending_reboot = true;
  } 
  else {
    String helpMsg = "👋 Доsтупные команды:\n"
                     "/wake — turn on PC\n";
    if (ENABLE_SLEEP_COMMAND) {
      helpMsg += "/sleep — turn off PC\n";
    }
    helpMsg += "/status — sтатуs\n"
               "/reboot — reboot";
    sendTelegramMessage(chat_id, helpMsg);
  }
}

void pollTelegram() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?limit=5&timeout=40&offset=" + String(last_update_id);
  
  secureClient.setTimeout(45);
  http.setReuse(false); 

  if (http.begin(secureClient, url)) {
    int httpCode = http.GET();
    String payload = "";
    
    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
    }
    http.end(); 
    
    if (payload.length() > 0) {
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error && doc["ok"].as<bool>()) {
        JsonArray result = doc["result"].as<JsonArray>();
        
        for (JsonObject update : result) {
          int32_t update_id = update["update_id"].as<int32_t>();
          if (update_id >= last_update_id) {
            last_update_id = update_id + 1; 
            
            if (update.containsKey("message")) {
              String chat_id = update["message"]["chat"]["id"].as<String>();
              String text = update["message"]["text"].as<String>();
              handleCommand(chat_id, text);
            }
          }
        }
      }
    }
  }
}

// ============================================================
//  SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(115200);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  
  if (!wm.autoConnect(AP_NAME)) {
    delay(1000);
    ESP.restart();
  }

  secureClient.setInsecure();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }

  udp.begin(WOL_PORT);
  WiFi.setAutoReconnect(true);

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 120000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

void loop() {
  esp_task_wdt_reset();

  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    return;
  }

  if (pending_reboot) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?limit=1&timeout=1&offset=" + String(last_update_id);
    http.begin(secureClient, url);
    http.GET();
    http.end();

    delay(1000);
    ESP.restart();
  }

  pollTelegram();
}

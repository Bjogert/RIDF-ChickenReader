#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Optional: for ESP32 unique ID helpers
#include <esp_system.h>

// WiFi Configuration - From secrets.h
const char* ssid = Secrets::WIFI_SSID;
const char* password = Secrets::WIFI_PASSWORD;

// MQTT Configuration - From secrets.h
const char* mqtt_server = Secrets::MQTT_SERVER;
const int mqtt_port = 1883;
const char* mqtt_user = Secrets::MQTT_USER;
const char* mqtt_password = Secrets::MQTT_PASSWORD;

// NEST_TAG identifies this device (e.g. "A", "B", "C" or "1", "2", "3")
// You can set this via PlatformIO build_flags: -DNEST_TAG=\"A\"
#ifndef NEST_TAG
#define NEST_TAG "A"
#endif

// Build a unique client ID per device using NEST_TAG + MAC (no colons)
static char mqtt_client_id[64];
static void buildClientId() {
  String mac = WiFi.macAddress(); // format: XX:XX:XX:XX:XX:XX
  mac.replace(":", "");
  snprintf(mqtt_client_id, sizeof(mqtt_client_id), "chicken_%s_%s", NEST_TAG, mac.c_str());
}

// MQTT Topics (per nest tag). Example when NEST_TAG=="A": chickens/nestA/status
static char topic_nest_status[64];
static char topic_nest_occupant[64];
static char topic_nest_occupants[64];  // NEW: Simple comma-separated format
static char topic_nest_duration[64];
static char topic_chicken_visits[64];
static char topic_chicken_leaderboard[64];
static char topic_chicken_changes[64];
static char topic_system_status[64]; // per-device system heartbeat

static void initTopics() {
  // Compose like: chickens/nest<NEST_TAG>/...
  snprintf(topic_nest_status, sizeof(topic_nest_status), "chickens/nest%s/status", NEST_TAG);
  snprintf(topic_nest_occupant, sizeof(topic_nest_occupant), "chickens/nest%s/occupant", NEST_TAG);
  snprintf(topic_nest_occupants, sizeof(topic_nest_occupants), "chickens/nest%s/occupants", NEST_TAG);
  snprintf(topic_nest_duration, sizeof(topic_nest_duration), "chickens/nest%s/duration", NEST_TAG);
  // Per-nest visit/change/leaderboard topics to avoid cross-device collisions
  snprintf(topic_chicken_visits, sizeof(topic_chicken_visits), "chickens/nest%s/visits", NEST_TAG);
  snprintf(topic_chicken_leaderboard, sizeof(topic_chicken_leaderboard), "chickens/nest%s/leaderboard", NEST_TAG);
  snprintf(topic_chicken_changes, sizeof(topic_chicken_changes), "chickens/nest%s/changes", NEST_TAG);
  snprintf(topic_system_status, sizeof(topic_system_status), "chickens/nest%s/system/status", NEST_TAG);
}

// MQTT Client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Scoring System Variables
struct ChickenStats {
  int visits;
  unsigned long totalTime;
  unsigned long lastVisit;
  String name;
};

ChickenStats chickenStats[15]; // One for each chicken in database

// RFID Reader Configuration for ESP32 D1 Mini
#define RFID_RX_PIN 16      // GPIO16 (D0) - connect to RFID TX
#define RFID_TX_PIN 17      // GPIO17 (D1) - not used (EL125 has no RX)
#define RFID_RESET_PIN 18   // GPIO18 - connect to RFID RES pin
#define RFID_BAUD 9600

// Signal strengthening settings
#define RFID_BUFFER_SIZE 256
#define MIN_TAG_LENGTH 8
#define MAX_TAG_LENGTH 20
#define READ_TIMEOUT_MS 1000

// Create UART for RFID communication
HardwareSerial rfidSerial(1);

// Data validation variables
int consecutiveValidReads = 0;
String lastValidTag = "";
unsigned long lastValidReadTime = 0;

// Smart tracking variables
String currentChicken = "";
unsigned long chickenEnterTime = 0;
unsigned long lastPresenceCheck = 0;
unsigned long lastResetTime = 0;
bool nestOccupied = false;
bool waitingForPresenceConfirmation = false;

// Multi-chicken detection variables
int quickChanges = 0;
unsigned long lastChangeTime = 0;
bool multiChickenMode = false;
String detectedChickens[15]; // Track ALL chickens in database (expanded from 5 to 15)
int chickenCount = 0;
unsigned long lastMultiChickenDetection = 0; // Track when we last detected multiple chickens
unsigned long singleChickenReadings = 0; // Count consecutive single-chicken readings
#define MULTI_CHICKEN_TIMEOUT 60000 // 60 seconds to confirm all chickens have left
#define SINGLE_READINGS_THRESHOLD 10 // Number of single readings before considering exit

// Chicken Database - Add your real chickens here
struct Chicken {
  String tagID;
  String name;
  int number;
};

// Define your actual chickens with their real tag IDs
Chicken chickenDatabase[] = {
  {"2003E98C8", "Lady Kluck", 1},      // ✓ CONFIRMED - working tag
  {"2003EF40D", "Ronny", 2},           // ✓ SCANNED - new tag added
  {"2003F2676", "Ada", 3},             // ✓ SCANNED - new tag added
  {"2003E98F1", "Ms.Foster", 4},       // ✓ SCANNED - new tag added
  {"2003E586A", "Kiwi", 5},            // ✓ SCANNED - new tag added
  {"2003E956D", "Skrik", 6},           // ✓ SCANNED - new tag added
  {"200336896", "Lady Klick", 7},      // ✓ SCANNED - updated name (was Panik)
  {"20032D5A4A", "Gästrid", 8},        // ✓ SCANNED - new tag added (note: 10 chars)
  {"2003E66AE", "Chick_1_2025", 9},    // ✓ SCANNED - new tag added
  {"2003E58C1", "Chick_2_2025", 10},   // ✓ SCANNED - new tag added
  {"2003E609A", "Chick_3_2025", 11},   // ✓ SCANNED - new tag added
  {"2003F3CA0", "Chick_4_2025", 12},   // ✓ SCANNED - new tag added
  {"2003E6C2F", "Chick_5_2025", 13},   // ✓ SCANNED - new tag added
  {"2003E9525", "Chick_6_2025", 14},   // ✓ SCANNED - new tag added
  {"2003E81EE", "Tuppen", 15},         // ✓ SCANNED - new tag added
  // All 15 chickens now have valid tags!
};

int totalChickens = sizeof(chickenDatabase) / sizeof(chickenDatabase[0]);

// Function forward declarations
void updateChickenStats(int chickenNumber, unsigned long duration);
void publishLeaderboard();
void publishChickenChange(String previousChicken, String newChicken, unsigned long duration);
void publishSimpleOccupants(); // NEW: Simple comma-separated occupants
Chicken* findChickenByTag(String tagID);

// WiFi connection function
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
}

// MQTT connection function
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    
  if (mqtt.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      
      // Publish system online status
      mqtt.publish(topic_system_status, "online");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Function to ensure MQTT connection
void ensureMQTTConnection() {
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();
}

// Function to publish nest status
void publishNestStatus(String status, String occupant = "", int duration = 0) {
  if (!mqtt.connected()) return;
  
  // Create JSON payload
  JsonDocument doc;
  doc["status"] = status;
  doc["timestamp"] = millis();
  
  if (occupant != "") {
    doc["occupant"] = occupant;
  }
  
  // If multiple chickens detected, add the specific chicken list
  if (status == "multiple" && chickenCount > 0) {
    JsonArray chickens = doc["chickens"].to<JsonArray>();
    for (int i = 0; i < chickenCount; i++) {
      Chicken* chicken = findChickenByTag(detectedChickens[i]);
      if (chicken) {
        chickens.add(chicken->name);
      }
    }
    doc["chicken_count"] = chickenCount;
    
    // Also create a comma-separated list for the occupant field
    String chickenList = "";
    for (int i = 0; i < chickenCount; i++) {
      Chicken* chicken = findChickenByTag(detectedChickens[i]);
      if (chicken) {
        if (i > 0) chickenList += ", ";
        chickenList += chicken->name;
      }
    }
    doc["occupant"] = chickenList;
    occupant = chickenList; // Update occupant for the separate topic
  }
  
  if (duration > 0) {
    doc["duration"] = duration;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  mqtt.publish(topic_nest_status, payload.c_str());
  mqtt.publish(topic_nest_occupant, occupant.c_str());
  
  // NEW: Also publish simple occupants format
  publishSimpleOccupants();
  
  // Debug output
  Serial.println("MQTT Published:");
  Serial.println("  Topic: " + String(topic_nest_status) + " | Payload: " + payload);
  Serial.println("  Topic: " + String(topic_nest_occupant) + " | Payload: " + occupant);
  
  if (duration > 0) {
    mqtt.publish(topic_nest_duration, String(duration).c_str());
    Serial.println("  Topic: " + String(topic_nest_duration) + " | Payload: " + String(duration));
  }
}

// Function to publish chicken visit data
void publishChickenVisit(String chickenName, int chickenNumber, unsigned long duration) {
  if (!mqtt.connected()) return;
  
  JsonDocument doc;
  doc["chicken_name"] = chickenName;
  doc["chicken_number"] = chickenNumber;
  doc["duration"] = duration;
  doc["timestamp"] = millis();
  doc["date"] = "2025-07-26"; // You might want to use NTP for real dates
  
  String payload;
  serializeJson(doc, payload);
  
  mqtt.publish(topic_chicken_visits, payload.c_str());
  
  // Update chicken stats
  updateChickenStats(chickenNumber, duration);
}

// Function to publish chicken change events
void publishChickenChange(String previousChicken, String newChicken, unsigned long duration) {
  if (!mqtt.connected()) return;
  
  JsonDocument doc;
  doc["event"] = "chicken_change";
  doc["previous_chicken"] = previousChicken;
  doc["new_chicken"] = newChicken;
  doc["previous_duration"] = duration;
  doc["timestamp"] = millis();
  doc["date"] = "2025-07-26";
  
  String payload;
  serializeJson(doc, payload);
  
  mqtt.publish(topic_chicken_changes, payload.c_str());
}

// NEW: Function to publish simple comma-separated occupants format
void publishSimpleOccupants() {
  if (!mqtt.connected()) return;
  
  String occupantsList = "";
  
  if (!nestOccupied) {
    // Empty nest
    occupantsList = "Empty";
  } else if (multiChickenMode && chickenCount > 0) {
    // Multiple chickens - create comma-separated list
    for (int i = 0; i < chickenCount; i++) {
      Chicken* chicken = findChickenByTag(detectedChickens[i]);
      if (chicken) {
        if (i > 0) occupantsList += ",";
        occupantsList += chicken->name;
      }
    }
  } else {
    // Single chicken
    Chicken* chicken = findChickenByTag(currentChicken);
    if (chicken) {
      occupantsList = chicken->name;
    } else {
      occupantsList = "Empty";
    }
  }
  
  // Publish simple format to new topic
  mqtt.publish(topic_nest_occupants, occupantsList.c_str());
  
  Serial.println("MQTT Simple Occupants: " + String(topic_nest_occupants) + " | " + occupantsList);
}

// Function to update chicken statistics
void updateChickenStats(int chickenNumber, unsigned long duration) {
  if (chickenNumber < 1 || chickenNumber > 15) return;
  
  int index = chickenNumber - 1;
  chickenStats[index].visits++;
  chickenStats[index].totalTime += duration;
  chickenStats[index].lastVisit = millis();
  chickenStats[index].name = chickenDatabase[index].name;
  
  // Publish updated leaderboard every 10 visits across all chickens
  static int totalVisits = 0;
  totalVisits++;
  if (totalVisits % 10 == 0) {
    publishLeaderboard();
  }
}

// Function to publish leaderboard
void publishLeaderboard() {
  if (!mqtt.connected()) return;
  
  JsonDocument doc;
  JsonArray leaderboard = doc["leaderboard"].to<JsonArray>();
  
  // Create array of chicken stats for sorting
  ChickenStats sortedStats[15];
  for (int i = 0; i < 15; i++) {
    sortedStats[i] = chickenStats[i];
  }
  
  // Simple bubble sort by visit count
  for (int i = 0; i < 14; i++) {
    for (int j = 0; j < 14 - i; j++) {
      if (sortedStats[j].visits < sortedStats[j + 1].visits) {
        ChickenStats temp = sortedStats[j];
        sortedStats[j] = sortedStats[j + 1];
        sortedStats[j + 1] = temp;
      }
    }
  }
  
  // Add top 10 to JSON
  for (int i = 0; i < 10 && i < 15; i++) {
    if (sortedStats[i].visits > 0) {
      JsonObject chicken = leaderboard.add<JsonObject>();
      chicken["rank"] = i + 1;
      chicken["name"] = sortedStats[i].name;
      chicken["visits"] = sortedStats[i].visits;
      chicken["total_time"] = sortedStats[i].totalTime;
      chicken["avg_time"] = sortedStats[i].visits > 0 ? sortedStats[i].totalTime / sortedStats[i].visits : 0;
    }
  }
  
  doc["updated"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  mqtt.publish(topic_chicken_leaderboard, payload.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== Smart Chicken RFID Monitor v3.0 ===");
  Serial.println("ESP32 D1 Mini - 15 Chicken System");
  Serial.println("Features: Enter/Exit tracking, MQTT, Scoring");
  Serial.println();

  // Initialize topics and unique MQTT client id early
  initTopics();
  buildClientId();
  
  // Initialize chicken stats
  for (int i = 0; i < 15; i++) {
    chickenStats[i].visits = 0;
    chickenStats[i].totalTime = 0;
    chickenStats[i].lastVisit = 0;
    chickenStats[i].name = chickenDatabase[i].name;
  }
  
  // Setup reset pin
  pinMode(RFID_RESET_PIN, OUTPUT);
  digitalWrite(RFID_RESET_PIN, HIGH); // Keep reader active
  
  // Initialize RFID Serial with improved settings
  rfidSerial.begin(RFID_BAUD, SERIAL_8N1, RFID_RX_PIN, RFID_TX_PIN);
  rfidSerial.setRxBufferSize(RFID_BUFFER_SIZE); // Larger buffer for better reliability
  delay(500);
  
  // Additional UART stability settings
  Serial.println("RFID UART Buffer Size: " + String(RFID_BUFFER_SIZE));
  Serial.println("Signal validation: Enabled");
  
  // Connect to WiFi
  connectWiFi();
  
  // Setup MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
  connectMQTT();
  
  Serial.println("System Status: READY");
  Serial.print("Monitoring: Nesting Box #");
  Serial.println(NEST_TAG);
  Serial.println("Smart Logic: Enter/Exit detection");
  Serial.println("Reset Control: Enabled on GPIO18");
  Serial.println("MQTT: Connected to Home Assistant");
  Serial.println("Scoring: Active");
  Serial.println("Note: EL125 is read-only (no RX pin)");
  Serial.println("=====================================");
  
  // Publish initial system status
  publishNestStatus("empty");
}

// Function to reset the RFID reader to force a new read
void resetReader() {
  Serial.println("→ Resetting RFID reader for fresh read...");
  
  // Clear any pending data first
  while (rfidSerial.available()) {
    rfidSerial.read();
  }
  
  // Reset the reader with extended timing for stationary tag detection
  digitalWrite(RFID_RESET_PIN, LOW);   // Reset the reader
  delay(200);                          // Longer reset hold for complete power cycle
  digitalWrite(RFID_RESET_PIN, HIGH);  // Release reset
  delay(1000);                         // Extended restart time for EL125 to stabilize and begin multiple scan cycles
  
  // Clear validation state to force fresh detection
  consecutiveValidReads = 0;
  lastValidTag = "";
  
  Serial.println("✓ RFID reader reset complete - extended scanning window active...");
}

// Function to extract readable tag ID from raw RFID data with validation
String extractTagID(String rawData) {
  // Clean the data first
  rawData.trim();
  rawData.toUpperCase();
  
  // Check if this is ASCII-encoded format (starts with 02 and ends with 03)
  if (rawData.startsWith("02") && rawData.endsWith("03") && rawData.length() >= 6) {
    // Remove start (02) and end (03) bytes
    String asciiHex = rawData.substring(2, rawData.length() - 2);
    
    // Convert ASCII hex to actual string
    String tagID = "";
    for (int i = 0; i < asciiHex.length(); i += 2) {
      if (i + 1 < asciiHex.length()) {
        String hexPair = asciiHex.substring(i, i + 2);
        char c = (char)strtol(hexPair.c_str(), NULL, 16);
        if (c >= 32 && c <= 126) { // Printable ASCII
          tagID += c;
        }
      }
    }
    
    // Extract clean tag ID - remove non-hex characters and standardize format
    String cleanTagID = "";
    
    // Only keep valid hex characters
    for (int i = 0; i < tagID.length(); i++) {
      char c = tagID.charAt(i);
      if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
        cleanTagID += c;
      }
    }
    
    // Remove leading zeros and get the meaningful part
    String finalTag = "";
    bool foundStart = false;
    for (int i = 0; i < cleanTagID.length(); i++) {
      char c = cleanTagID.charAt(i);
      if (!foundStart && c != '0') foundStart = true;
      if (foundStart) finalTag += c;
    }
    
    // Ensure minimum length of 8 characters by padding with leading zeros if needed
    while (finalTag.length() < 8 && finalTag.length() > 0) {
      finalTag = "0" + finalTag;
    }
    
    // Validate length
    if (finalTag.length() >= 8 && finalTag.length() <= 16) {
      return finalTag;
    }
  }
  
  // Fallback to old method for non-ASCII format
  String cleanData = "";
  for (int i = 0; i < rawData.length(); i++) {
    char c = rawData.charAt(i);
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
      cleanData += c;
    }
  }
  
  // Validate length
  if (cleanData.length() < MIN_TAG_LENGTH || cleanData.length() > MAX_TAG_LENGTH) {
    return ""; // Invalid length
  }
  
  // For EL125, extract the meaningful part (typically middle section)
  if (cleanData.length() >= 10) {
    String result = cleanData.substring(6, 16);
    return result;
  }
  
  return cleanData;
}

// Improved RFID reading with error checking and validation
String readRFIDWithValidation() {
  if (!rfidSerial.available()) {
    return "";
  }
  
  String rawData = "";
  int bytesRead = 0;
  unsigned long startTime = millis();
  
  // Read with timeout and validation
  while ((millis() - startTime) < READ_TIMEOUT_MS && bytesRead < RFID_BUFFER_SIZE) {
    if (rfidSerial.available()) {
      uint8_t byte = rfidSerial.read();
      bytesRead++;
      
      // Convert to hex
      if (byte < 0x10) rawData += "0";
      rawData += String(byte, HEX);
      
      // Small delay to ensure we get complete data
      delay(2);
    } else {
      // If no more data coming and we have some, break
      if (bytesRead > 0) {
        delay(10); // Wait a bit more for potential additional bytes
        if (!rfidSerial.available()) break;
      }
    }
  }
  
  if (bytesRead == 0) {
    return "";
  }
  
  // Extract and validate tag ID
  String tagID = extractTagID(rawData);
  
  if (tagID.length() == 0) {
    return "";
  }
  
  // Additional validation - must be consistent across reads
  if (tagID == lastValidTag && (millis() - lastValidReadTime) < 2000) {
    consecutiveValidReads++;
  } else {
    consecutiveValidReads = 1;
    lastValidTag = tagID;
  }
  
  lastValidReadTime = millis();
  
  // Only return tag if we have confident reads
  if (consecutiveValidReads >= 1) { // Reduced from 2 to 1 for better responsiveness
    return tagID;
  }
  
  return "";
}

// Function to look up chicken by tag ID
Chicken* findChickenByTag(String tagID) {
  for (int i = 0; i < totalChickens; i++) {
    if (chickenDatabase[i].tagID == tagID) {
      return &chickenDatabase[i];
    }
  }
  return nullptr; // Not found = garbled/unknown tag
}

// Function to validate if tag ID is a real chicken
bool isValidChicken(String tagID) {
  return findChickenByTag(tagID) != nullptr;
}

// Function to get chicken info string
String getChickenInfo(String tagID) {
  Chicken* chicken = findChickenByTag(tagID);
  if (chicken != nullptr) {
    return String(chicken->number) + " (" + chicken->name + ")";
  }
  return "UNKNOWN";
}

// Function to add chicken to recent detection list
void addChickenToList(String tagID) {
  // Only add valid chickens
  if (!isValidChicken(tagID)) {
    return;
  }
  
  // Check if already in list
  for (int i = 0; i < chickenCount; i++) {
    if (detectedChickens[i] == tagID) {
      return; // Already in list
    }
  }
  
  // Add new chicken if space available
  if (chickenCount < 15) { // Expanded from 5 to 15 to track all chickens
    detectedChickens[chickenCount] = tagID;
    chickenCount++;
  }
}

// Function to check for multi-chicken indicators
bool detectMultipleChickens(String tagID, unsigned long sessionDuration) {
  // Only process valid chickens
  if (!isValidChicken(tagID)) {
    return false;
  }
  
  // IMPORTANT: Add the current chicken to the list first (the one already in nest)
  // This ensures we don't lose track of the chicken that was already present
  if (!currentChicken.isEmpty() && isValidChicken(currentChicken)) {
    addChickenToList(currentChicken);
  }
  
  // Indicator 1: Very quick changes (less than 10 seconds)
  if (sessionDuration < 10) {
    quickChanges++;
    
    // If 3+ quick changes in short time = multiple chickens
    if (quickChanges >= 3) {
      return true;
    }
  } else {
    quickChanges = 0; // Reset counter for longer sessions
  }
  
  // Indicator 2: Multiple different chickens detected recently (lowered threshold)
  addChickenToList(tagID); // Add the new chicken too
  if (chickenCount >= 2) { // Any 2+ chickens = multi-chicken mode
    return true;
  }
  
  return false;
}

// Function to reset multi-chicken detection after timeout
void resetMultiChickenDetection() {
  quickChanges = 0;
  chickenCount = 0;
  multiChickenMode = false;
  singleChickenReadings = 0;
  lastMultiChickenDetection = 0;
  for (int i = 0; i < 15; i++) { // Clear all 15 slots (expanded from 5)
    detectedChickens[i] = "";
  }
}

// Function to get chicken ID from tag (you can customize this mapping)
String getChickenID(String tagID) {
  Chicken* chicken = findChickenByTag(tagID);
  if (chicken != nullptr) {
    return String(chicken->number);
  }
  return "??"; // Unknown chicken
}

void loop() {
  // Ensure MQTT connection
  ensureMQTTConnection();
  
  // Heartbeat every 5 minutes (300 seconds)
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 300000) {
    String status;
    if (!nestOccupied) {
      status = "Empty";
      publishNestStatus("empty");
    } else if (multiChickenMode) {
      status = "Multiple chickens detected";
      publishNestStatus("multiple", "multiple_chickens");
    } else {
      status = "Occupied by " + getChickenInfo(currentChicken);
      Chicken* chicken = findChickenByTag(currentChicken);
      if (chicken) {
        publishNestStatus("occupied", chicken->name);
      }
    }
    Serial.println("[" + String(millis()/60000) + "min] " + status);
    
    // Also publish system heartbeat
    mqtt.publish(topic_system_status, "online");
    
    lastHeartbeat = millis();
  }
  
  // Smart presence check every 30 seconds if nest is occupied
  if (nestOccupied && (millis() - lastPresenceCheck > 30000)) {
    String currentChickenInfo = getChickenInfo(currentChicken);
    Serial.println("Checking if " + currentChickenInfo + " is still present...");
    resetReader();
    lastResetTime = millis();
    waitingForPresenceConfirmation = true;
    lastPresenceCheck = millis();
  }
  
  // Check if chicken has left after reset (no detection within 8 seconds after reset)
  if (waitingForPresenceConfirmation && (millis() - lastResetTime > 8000)) {
    // No detection after reset = chicken has left
    unsigned long sessionDuration = (millis() - chickenEnterTime) / 1000;
    
    if (multiChickenMode) {
      Serial.println("*** MULTIPLE CHICKENS LEFT NEST! ***");
      String lastChickenInfo = getChickenInfo(currentChicken);
      Serial.println("Last detected: " + lastChickenInfo);
      Serial.println("Multi-chicken session duration: " + String(sessionDuration) + " seconds");
      
      // Publish multi-chicken session end
      publishNestStatus("empty");
      
    } else {
      Serial.println("*** CHICKEN LEFT NEST! ***");
      String chickenInfo = getChickenInfo(currentChicken);
      Serial.println("Chicken: " + chickenInfo);
      Serial.println("Session Duration: " + String(sessionDuration) + " seconds");
      
      // Publish single chicken visit
      Chicken* chicken = findChickenByTag(currentChicken);
      if (chicken) {
        publishChickenVisit(chicken->name, chicken->number, sessionDuration);
        publishNestStatus("empty");
      }
    }
    Serial.println("Status: EMPTY");
    Serial.println("===================");
    
    // Reset state
    nestOccupied = false;
    currentChicken = "";
    chickenEnterTime = 0;
    waitingForPresenceConfirmation = false;
    resetMultiChickenDetection();
  }
  
  // Check for RFID data with improved validation
  if (rfidSerial.available()) {
    // Use improved reading function
    String tagID = readRFIDWithValidation();
    
    if (tagID.length() == 0) {
      // Invalid or incomplete read, ignore
      return;
    }
    
    String chickenID = getChickenID(tagID);
    String chickenInfo = getChickenInfo(tagID);
    unsigned long currentTime = millis();
    
    // Check if this is a valid chicken
    if (!isValidChicken(tagID)) {
      Serial.println("! Unknown tag: " + tagID + " (ignored)");
      return; // Ignore unknown chickens
    }
    
    if (!nestOccupied) {
      // Chicken entering nest
      nestOccupied = true;
      currentChicken = tagID; // Store the full tag ID
      chickenEnterTime = currentTime;
      lastPresenceCheck = currentTime;
      waitingForPresenceConfirmation = false; // Not waiting when chicken enters
      
      Serial.println("*** CHICKEN ENTERED NEST! ***");
      Serial.println("Chicken: " + chickenInfo + " | Tag: " + tagID);
      Serial.println("Time: " + String(currentTime/1000) + "s");
      Serial.println("Status: OCCUPIED");
      Serial.println("===================");
      
      // Publish chicken entry
      Chicken* chicken = findChickenByTag(tagID);
      if (chicken) {
        publishNestStatus("occupied", chicken->name);
      }
      
    } else if (currentChicken == tagID) {
      // Same chicken still present - just update check time
      lastPresenceCheck = currentTime;
      waitingForPresenceConfirmation = false; // Cancel the waiting state
      
      // IMPORTANT: Handle multi-chicken mode carefully due to hardware limitations
      if (multiChickenMode) {
        // Increment counter for consecutive single-chicken readings
        singleChickenReadings++;
        
        // Only exit multi-chicken mode after many consecutive single readings
        // AND enough time has passed to be confident other chickens have left
        if (singleChickenReadings >= SINGLE_READINGS_THRESHOLD && 
            (millis() - lastMultiChickenDetection) > MULTI_CHICKEN_TIMEOUT) {
          
          Serial.println("*** EXITING MULTI-CHICKEN MODE ***");
          Serial.println("Only " + chickenInfo + " detected for " + String(singleChickenReadings) + " consecutive readings");
          Serial.println("Time since last multi-chicken activity: " + String((millis() - lastMultiChickenDetection)/1000) + "s");
          
          // Reset multi-chicken detection
          resetMultiChickenDetection();
          multiChickenMode = false;
          singleChickenReadings = 0;
          
          // Publish single chicken status
          Chicken* chicken = findChickenByTag(tagID);
          if (chicken) {
            publishNestStatus("occupied", chicken->name);
            Serial.println("MQTT: Updated to single chicken mode - " + chicken->name);
          }
          
          Serial.println("Status: OCCUPIED BY SINGLE CHICKEN");
          Serial.println("===================");
        } else {
          // Still in multi-chicken mode, just show progress
          Serial.println("✓ " + chickenInfo + " detected (single reading #" + String(singleChickenReadings) + 
                        "/" + String(SINGLE_READINGS_THRESHOLD) + ", timeout in " + 
                        String((MULTI_CHICKEN_TIMEOUT - (millis() - lastMultiChickenDetection))/1000) + "s)");
        }
      } else {
        Serial.println("✓ " + chickenInfo + " confirmed present");
      }
      
    } else {
      // Different chicken detected while nest occupied
      unsigned long sessionDuration = (currentTime - chickenEnterTime) / 1000;
      
      // Check if this indicates multiple chickens
      if (detectMultipleChickens(tagID, sessionDuration)) {
        if (!multiChickenMode) {
          // First time detecting multiple chickens
          multiChickenMode = true;
          lastMultiChickenDetection = millis(); // Record when we detected multiple chickens
          singleChickenReadings = 0; // Reset counter
          
          Serial.println("*** MULTIPLE CHICKENS DETECTED! ***");
          Serial.println("Rapid changes detected - cuddling chickens!");
          Serial.println("Chickens seen: ");
          for (int i = 0; i < chickenCount; i++) {
            String info = getChickenInfo(detectedChickens[i]);
            Serial.println("  " + info);
          }
          Serial.println("Status: MULTIPLE CHICKENS IN NEST");
          Serial.println("===================");
          
          // Publish multi-chicken detection to MQTT
          publishNestStatus("multiple", "multiple_chickens");
          
        } else {
          // Already in multi-chicken mode, but show updated list
          lastMultiChickenDetection = millis(); // Update timestamp for continued activity
          singleChickenReadings = 0; // Reset single-chicken counter
          
          Serial.println("~ Multi-chicken activity continues ~");
          Serial.println("Updated chicken list:");
          for (int i = 0; i < chickenCount; i++) {
            String info = getChickenInfo(detectedChickens[i]);
            Serial.println("  " + info);
          }
          Serial.println("---");
          
          // Update MQTT with continued multi-chicken activity
          publishNestStatus("multiple", "multiple_chickens");
        }
      } else {
        // Normal chicken change - publish the previous chicken's visit first
        String prevChickenInfo = getChickenInfo(currentChicken);
        String newChickenInfo = getChickenInfo(tagID);
        
        Serial.println(">>> CHICKEN CHANGE! <<<");
        Serial.println("Previous: " + prevChickenInfo + " (was there " + String(sessionDuration) + "s)");
        Serial.println("New: " + newChickenInfo + " | Tag: " + tagID);
        Serial.println("Status: OCCUPIED BY NEW CHICKEN");
        Serial.println("===================");
        
        // Publish detailed chicken change event to MQTT
        Chicken* prevChicken = findChickenByTag(currentChicken);
        Chicken* newChicken = findChickenByTag(tagID);
        
        if (prevChicken && newChicken) {
          // Publish the previous chicken's visit
          publishChickenVisit(prevChicken->name, prevChicken->number, sessionDuration);
          
          // Publish the chicken change event  
          publishChickenChange(prevChicken->name, newChicken->name, sessionDuration);
          
          // Update nest status with new chicken - IMMEDIATELY update occupant
          publishNestStatus("occupied", newChicken->name);
          
          // Also publish directly to occupant topic to ensure it updates
          mqtt.publish(topic_nest_occupant, newChicken->name.c_str());
          
          // NEW: Also update simple occupants format immediately
          publishSimpleOccupants();
          
          Serial.println("MQTT: Updated occupant to " + newChicken->name);
        }
      }
      
      // Update to new chicken
      currentChicken = tagID; // Store the full tag ID
      chickenEnterTime = currentTime;
      lastPresenceCheck = currentTime;
      waitingForPresenceConfirmation = false; // Cancel waiting state
    }
  }
  
  delay(100);
}

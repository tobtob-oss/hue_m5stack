#include <M5Unified.h>
#define WIFI_SSID "******"
#define WIFI_PASS "*********"
#define MOOD_SERVER_IP "192.168.0.15"
#define MOOD_SERVER_PORT 8866


#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

WiFiClient wifi;
HttpClient client = HttpClient(wifi, MOOD_SERVER_IP, MOOD_SERVER_PORT); 

// Room and Mood lists
String rooms[20];  // Array to hold room names (max 20)
int roomCount = 0;

String moods[20];  // Array to hold mood names (max 20)
int moodCount = 0;

// Menu state variables
enum MenuMode { ROOM_SELECTION, MOOD_SELECTION };
MenuMode currentMode = ROOM_SELECTION;
int currentRoomIndex = 0;
int currentMoodIndex = 0;
String selectedRoom = "";
String selectedMood = "";

// Function declarations
void displayMenu();
void sendMoodCommand( String room, String mood );
void getAllMoods();
void getAllRooms();

void setup(void)
{
  M5.begin();

  /// For models with EPD : refresh control
  M5.Display.setEpdMode(epd_mode_t::epd_fastest); // fastest but very-low quality.

  if (M5.Display.width() < M5.Display.height())
  { /// Landscape mode.
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }

  /// connect to WIFI
  M5.Display.println("Connecting to WiFi");
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    M5.Display.print(".");
    M5.delay(100);
  }
  M5.Display.clear();

  M5.Display.println("HTTP configured");
  M5.delay(1000);
  
  // Get rooms from server
  M5.Display.println("Getting rooms...");
  getAllRooms();
  M5.Display.printf("Loaded %d rooms\n", roomCount);
  M5.delay(1000);
  
  // Get moods from server
  M5.Display.println("Getting moods...");
  getAllMoods();
  M5.Display.printf("Loaded %d moods\n", moodCount);
  M5.delay(2000);
  
  // Initialize display
  M5.Display.clear();
  M5.Display.setTextSize(2);
  displayMenu();
}

void loop(void)
{
  M5.delay(1);
  M5.update();

  // Button A - Scroll Left (Previous item)
  if (M5.BtnA.wasClicked()) {
    if (currentMode == ROOM_SELECTION) {
      currentRoomIndex--;
      if (currentRoomIndex < 0) {
        currentRoomIndex = roomCount - 1;  // Wrap to last item
      }
    } else {  // MOOD_SELECTION
      currentMoodIndex--;
      if (currentMoodIndex < 0) {
        currentMoodIndex = moodCount - 1;  // Wrap to last item
      }
    }
    displayMenu();
  }

  // Button B - Select current item
  if (M5.BtnB.wasClicked()) {
    if (currentMode == ROOM_SELECTION) {
      // Select room and move to mood selection
      selectedRoom = rooms[currentRoomIndex];
      currentMode = MOOD_SELECTION;
      currentMoodIndex = 0;  // Reset to first mood
      M5_LOGI("Room selected: %s", selectedRoom.c_str());
    } else {  // MOOD_SELECTION
      // Select mood and trigger API call
      selectedMood = moods[currentMoodIndex];
      M5_LOGI("Mood selected: %s", selectedMood.c_str());
      
      // TODO: Send REST API call with selected room and mood
      sendMoodCommand(selectedRoom, selectedMood);
      
      // Show confirmation
      M5.Display.clear();
      M5.Display.setCursor(10, 60);
      M5.Display.println("Sent:");
      M5.Display.setCursor(10, 90);
      M5.Display.printf("%s\n", selectedRoom.c_str());
      M5.Display.setCursor(10, 120);
      M5.Display.printf("%s\n", selectedMood.c_str());
      M5.delay(2000);
      
      // Return to room selection
      currentMode = ROOM_SELECTION;
      displayMenu();
    }
  }

  // Button C - Scroll Right (Next item)
  if (M5.BtnC.wasClicked()) {
    if (currentMode == ROOM_SELECTION) {
      currentRoomIndex++;
      if (currentRoomIndex >= roomCount) {
        currentRoomIndex = 0;  // Wrap to first item
      }
    } else {  // MOOD_SELECTION
      currentMoodIndex++;
      if (currentMoodIndex >= moodCount) {
        currentMoodIndex = 0;  // Wrap to first item
      }
    }
    displayMenu();
  }
}


void displayMenu() {
  M5.Display.clear();
  M5.Display.setTextSize(3);
  
  // Display mode indicator at top
  M5.Display.setCursor(10, 10);
  if (currentMode == ROOM_SELECTION) {
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.println("SELECT ROOM:");
    M5.Display.setTextColor(TFT_WHITE);
  } else {
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.println("SELECT MOOD:");
    M5.Display.setTextColor(TFT_WHITE);
    // Show selected room at top
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 35);
    M5.Display.printf("Room: %s", selectedRoom.c_str());
    M5.Display.setTextSize(2);
  }
  

  // Display navigation arrows and current item
  int centerY = 100;
  M5.Display.setCursor(10, centerY);
  M5.Display.print("<");

  M5.Display.setCursor(30, centerY);  // Center horizontally
  M5.Display.setTextColor(TFT_GREEN);
  if (currentMode == ROOM_SELECTION) {
    M5.Display.print(rooms[currentRoomIndex]);
  } else {
    M5.Display.print(moods[currentMoodIndex]);
  }
  M5.Display.setTextColor(TFT_WHITE);
  
  M5.Display.setCursor(300, centerY);
  M5.Display.print(">");
  
  // Display button hints at bottom
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 220);
  M5.Display.print("A:Prev  B:Select  C:Next");
}

void getAllMoods() {
  M5_LOGI("Fetching moods from server...");
  
  // Make HTTP GET request to /moods endpoint
  client.beginRequest();
  client.get("/moods");
  client.endRequest();
  
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();
  
  M5_LOGI("Moods response - Status: %d", statusCode);
  
  if (statusCode != 200) {
    M5_LOGE("Failed to get moods: %d", statusCode);
    M5_LOGE("Response: %s", response.c_str());
    return;
  }
  
  // Parse JSON response
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);    
  if (error) {
    M5_LOGE("JSON parsing failed: %s", error.c_str());
    return;
  }
  
  // Extract mood names from JSON array
  JsonArray moodsArray = doc.as<JsonArray>();
  moodCount = 0;
  
  for (JsonObject moodObj : moodsArray) {
    if (moodCount < 20) {  // Max 20 moods
      String moodName = moodObj["name"].as<String>();
      moods[moodCount] = moodName;
      M5_LOGI("Loaded mood %d: %s", moodCount, moodName.c_str());
      moodCount++;
    }
  }
  
  M5_LOGI("Total moods loaded: %d", moodCount);
}

void getAllRooms() {
  M5_LOGI("Fetching rooms from server...");
  
  // Make HTTP GET request to /rooms endpoint
  client.beginRequest();
  client.get("/rooms");
  client.endRequest();
  
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();
  
  M5_LOGI("Rooms response - Status: %d", statusCode);
  
  if (statusCode != 200) {
    M5_LOGE("Failed to get rooms: %d", statusCode);
    M5_LOGE("Response: %s", response.c_str());
    return;
  }
  
  // Parse JSON response
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);  
  if (error) {
    M5_LOGE("JSON parsing failed: %s", error.c_str());
    return;
  }

  // Extract room names and IDs from JSON array
  JsonArray roomsArray = doc.as<JsonArray>();
  roomCount = 0;
  
  for (JsonObject roomObj : roomsArray) {
    if (roomCount < 20) {  // Max 20 rooms
      String roomName = roomObj["name"].as<String>();
      rooms[roomCount] = roomName;
      M5_LOGI("Loaded room %d: %s", roomCount, roomName.c_str());
      roomCount++;
    }
  }
  
  M5_LOGI("Total rooms loaded: %d", roomCount);
}

void sendMoodCommand( String room, String mood ) {
  M5_LOGI("Sending mood command - Room: %s, Mood: %s", room.c_str(), mood.c_str());
  
  // Construct JSON body
  JsonDocument doc;
  doc["name"] = mood;
  
  String jsonBody;
  serializeJson(doc, jsonBody);
  
  M5_LOGI("Request body: %s", jsonBody.c_str());
  
  // Make HTTP POST request to /mood endpoint
  client.beginRequest();
  client.post("/mood");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", jsonBody.length());
  client.beginBody();
  client.print(jsonBody);
  client.endRequest();
  
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();
  
  M5_LOGI("Mood command response - Status: %d", statusCode);
  M5_LOGI("Response: %s", response.c_str());
  
  if (statusCode != 200) {
    M5_LOGE("Failed to send mood command: %d", statusCode);
    M5.Display.clear();
    M5.Display.setCursor(10, 60);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("ERROR!");
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(10, 90);
    M5.Display.printf("Status: %d\n", statusCode);
    M5.delay(3000);
    return;
  }
  
  // Parse JSON response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  
  if (error) {
    M5_LOGE("JSON parsing failed: %s", error.c_str());
    M5.Display.clear();
    M5.Display.setCursor(10, 60);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("ERROR!");
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(10, 90);
    M5.Display.println("Invalid JSON");
    M5.delay(3000);
    return;
  }
  
  // Check success status from results array
  JsonArray results = responseDoc["results"].as<JsonArray>();
  bool allSuccess = true;
  int totalLightsAffected = 0;
  
  for (JsonObject result : results) {
    bool success = result["success"].as<bool>();
    String resultRoom = result["room"].as<String>();
    int lightsAffected = result["lights_affected"].as<int>();
    
    M5_LOGI("Room: %s, Success: %d, Lights: %d", resultRoom.c_str(), success, lightsAffected);
    
    if (!success) {
      allSuccess = false;
      M5_LOGE("Failed for room: %s", resultRoom.c_str());
    }
    
    totalLightsAffected += lightsAffected;
  }
  
  // Display result
  M5.Display.clear();
  M5.Display.setCursor(10, 40);
  
  if (!allSuccess) {
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("ERROR!");
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(10, 70);
    M5.Display.println("Some lights failed");
  } else {
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.println("SUCCESS!");
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(10, 70);
    M5.Display.printf("Mood: %s\n", mood.c_str());
    M5.Display.setCursor(10, 100);
    M5.Display.printf("Lights: %d\n", totalLightsAffected);
  }
  
  M5.delay(2000);
}


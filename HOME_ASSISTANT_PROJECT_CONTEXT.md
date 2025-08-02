# Chicken RFID Monitoring System - Home Assistant Integration

## Project Overview

This is an intelligent chicken coop monitoring system that tracks 14 chickens using RFID technology to monitor egg-laying behavior and productivity. The system has evolved from basic tag detection to sophisticated multi-chicken tracking with smart session management.

## Hardware Setup

- **ESP32 D1 Mini** - Main controller
  - [D1 Mini ESP32 CH9102X](https://www.hobbyelectronica.nl/en/product/d1-mini-esp32-ch9102x/?srsltid=AfmBOoruK0NZgmOS6u9iHdyrAqz3tYRwKyM4ZRi08tEqQET34rcvxian) - Tested hardware
- **EL125 125KHz RFID Reader** - Detects chickens wearing RFID tags
  - [125KHz RFID Long Distance Module (40cm range)](https://www.electrodragon.com/product/125khz-rfid-long-distance-module-40cm-serial/) - Exact model used
  - **CRITICAL DUAL POWER SETUP**:
    - VCC → 3.3V (RFID reader power)
    - VIN → VIN (ESP32 power)
  - GND → GND  
  - TX → GPIO16 (ESP32 RX)
  - RES → GPIO18 (Reset control for continuous monitoring)
  - **MODE pin → 3.3V** (Required for proper operation)
- **RFID Tags** - Each chicken wears a unique 125KHz tag
- **Power Supply** - Wall USB adapter for stable power
- **Connection Method** - Direct soldering for maximum stability

## System Architecture

### Core Features Implemented
1. **Smart Enter/Exit Detection** - Uses RES pin reset control to detect when chickens leave
2. **Multi-Chicken Detection** - Detects when multiple chickens are "cuddling" in the same nest
3. **Anti-Spam Logic** - No continuous spam; only publishes on state changes
4. **Chicken Database** - Maps RFID tags to real chicken names and numbers
5. **Session Tracking** - Tracks duration of each visit
6. **Garbled Data Filtering** - Ignores invalid/corrupted RFID reads

### Smart Logic Details
- **Presence Checking**: Every 30 seconds, the system resets the RFID reader to check if chicken is still present
- **Exit Detection**: If no tag detected within 8 seconds after reset = chicken has left
- **Multi-Chicken Detection**: Rapid tag changes (2+ chickens in quick succession) indicates multiple chickens
- **Database Validation**: Only known chicken tags are processed; garbled data is ignored

## Chicken Database (14 chickens currently defined)

```cpp
Chicken chickenDatabase[] = {
  {"2003E98C8", "Lady Kluck", 1},      // ✓ CONFIRMED - working tag
  {"2003EF40D", "Ronny", 2},           // ✓ SCANNED - new tag added
  {"2003F2676", "Ada", 3},             // ✓ SCANNED - new tag added
  {"2003E98F1", "Ms.Foster", 4},       // ✓ SCANNED - new tag added
  {"2003E586A", "Kiwi", 5},            // ✓ SCANNED - new tag added
  {"2003E956D", "Skrik", 6},           // ✓ SCANNED - new tag added
  {"200336896", "Panik", 7},           // ✓ SCANNED - new tag added
  {"20032D5A4A", "Gästrid", 8},        // ✓ SCANNED - new tag added (note: 10 chars)
  {"2003E66AE", "Chick_1_2025", 9},    // ✓ SCANNED - new tag added
  {"2003E58C1", "Chick_2_2025", 10},   // ✓ SCANNED - new tag added
  {"2003E609A", "Chick_3_2025", 11},   // ✓ SCANNED - new tag added
  {"2003F3CA0", "Chick_4_2025", 12},   // ✓ SCANNED - new tag added
  {"2003E6C2F", "Chick_5_2025", 13},   // ✓ SCANNED - new tag added
  {"2003E9525", "Chick_6_2025", 14},   // ✓ SCANNED - new tag added
  // All 14 chickens now have valid tags!
};
```

## MQTT Topics Published

### Real-time Status Topics
- `chickens/nest1/status` - Nest status JSON: empty/occupied/multiple
- `chickens/nest1/occupant` - Current chicken name or "multiple_chickens"
- `chickens/nest1/duration` - Current session duration (when chicken leaves)
- `chickens/system/status` - System online/offline status

### Event & Analytics Topics  
- `chickens/visits` - Individual visit data with chicken info, duration, timestamp
- `chickens/changes` - **NEW!** Detailed chicken change events within sessions
- `chickens/leaderboard` - Top 10 most productive chickens with stats

## MQTT Data Formats

### Nest Status (chickens/nest1/status)
```json
// Single chicken occupied
{
  "status": "occupied",
  "occupant": "Lady Kluck", 
  "timestamp": 12345678,
  "duration": 120
}

// Multiple chickens detected - ENHANCED!
{
  "status": "multiple",
  "occupant": "Ronny, Ada, Skrik",  // Comma-separated list for easy display
  "chickens": ["Ronny", "Ada", "Skrik"],  // Array for advanced processing
  "chicken_count": 3,
  "timestamp": 12345678
}

// Empty nest
{
  "status": "empty",
  "timestamp": 12345678
}
```

### Visit Data (chickens/visits)
```json
{
  "chicken_name": "Lady Kluck",
  "chicken_number": 1,
  "duration": 1800,
  "timestamp": 12345678,
  "date": "2025-07-26"
}
```

### **NEW: Chicken Change Events (chickens/changes)**
```json
{
  "event": "chicken_change",
  "previous_chicken": "Ronny",
  "new_chicken": "Chick_2_2025", 
  "previous_duration": 4,
  "timestamp": 12345678,
  "date": "2025-07-26"
}
```

### Leaderboard (chickens/leaderboard)
```json
{
  "leaderboard": [
    {
      "rank": 1,
      "name": "Lady Kluck",
      "visits": 15,
      "total_time": 18000,
      "avg_time": 1200
    },
    {
      "rank": 2, 
      "name": "Ada",
      "visits": 12,
      "total_time": 14400,
      "avg_time": 1200
    }
  ],
  "updated": 12345678
}
```

## Home Assistant Implementation Requirements

## ⚠️ **IMPORTANT UPDATE FOR HOME ASSISTANT AI:**

**The ESP32 code has been significantly enhanced** to publish **real-time MQTT messages** that mirror exactly what appears in the serial monitor. You now have access to **much more detailed chicken activity data**.

### **NEW Enhanced MQTT Publishing (January 2025):**

1. **Chicken Change Events** - When chickens swap places during a session:
   - **Topic**: `chickens/changes` 
   - **Data**: Previous chicken, new chicken, duration of previous stay
   - **Use Case**: Track short visits, detect social behaviors, complete activity logs

2. **Enhanced Multi-Chicken Detection** - **NEW FEATURE!**:
   - **Topic**: `chickens/nest1/status` with `status: "multiple"`
   - **Enhanced Data**: Now includes specific chicken names in TWO formats:
     - `occupant`: "Ronny, Ada, Skrik" (comma-separated for easy display)
     - `chickens`: ["Ronny", "Ada", "Skrik"] (array for advanced processing)
     - `chicken_count`: 3 (total number of chickens detected)
   - **Use Case**: Display exactly which chickens are cuddling together, social behavior analysis

3. **Multi-Chicken Updates** - Real-time chicken list changes:
   - **Enhanced**: When new chickens join the cuddle session, the system updates with the complete list
   - **Use Case**: Live multi-chicken activity monitoring, track which chickens like to socialize

4. **Complete Visit Tracking** - No more lost data:
   - **Previous**: Only final chicken's visit was recorded
   - **Now**: Every chicken change creates a visit record
   - **Example**: Ronny (4s) → Chick_2_2025 (ongoing) = 2 separate tracked visits

### **Critical Implementation Notes:**

1. **All Serial Monitor Events Now Have MQTT Equivalent**:
   - `*** CHICKEN ENTERED NEST! ***` → `chickens/nest1/status` = "occupied"
   - `>>> CHICKEN CHANGE! <<<` → `chickens/changes` + `chickens/visits` + `chickens/nest1/status`
   - `*** MULTIPLE CHICKENS DETECTED! ***` → `chickens/nest1/status` = "multiple"
   - `*** CHICKEN LEFT NEST! ***` → `chickens/visits` + `chickens/nest1/status` = "empty"

2. **Data Flow is Now Complete**:
   - **Short visits** (like 4-second Ronny visit) are **no longer lost**
   - **Chicken changes** within sessions are **fully tracked**
   - **Multi-chicken sessions** have **real-time updates**

### 1. MQTT Sensors Needed
Create sensors to receive the MQTT data:

```yaml
mqtt:
  sensor:
    - name: "Nest 1 Status"
      state_topic: "chickens/nest1/status"
      value_template: "{{ value_json.status }}"
      json_attributes_topic: "chickens/nest1/status"
      
    - name: "Nest 1 Occupant"
      state_topic: "chickens/nest1/occupant"
      
    - name: "Current Visit Duration"
      state_topic: "chickens/nest1/duration"
      unit_of_measurement: "seconds"
      
    - name: "Chicken Visits"
      state_topic: "chickens/visits"
      value_template: "{{ value_json.chicken_name }}"
      json_attributes_topic: "chickens/visits"
      
    - name: "Chicken Changes"
      state_topic: "chickens/changes"
      value_template: "{{ value_json.event }}"
      json_attributes_topic: "chickens/changes"
      
    - name: "Chicken Leaderboard"
      state_topic: "chickens/leaderboard"
      value_template: "{{ value_json.updated }}"
      json_attributes_topic: "chickens/leaderboard"
      
    # NEW: Enhanced Multi-Chicken Sensors
    - name: "Chickens in Nest Display"
      state_topic: "chickens/nest1/status"
      value_template: >
        {% if value_json.status == 'multiple' %}
          {{ value_json.occupant }} ({{ value_json.chicken_count }} chickens cuddling)
        {% elif value_json.status == 'occupied' %}
          {{ value_json.occupant }}
        {% else %}
          Empty
        {% endif %}
      json_attributes:
        - chickens
        - chicken_count
        - status
        
    - name: "Multi-Chicken Count"
      state_topic: "chickens/nest1/status"
      value_template: >
        {% if value_json.status == 'multiple' %}
          {{ value_json.chicken_count }}
        {% else %}
          0
        {% endif %}
```

### 1.1 **NEW: Multi-Chicken Social Behavior Tracking**
```yaml
# Automation to log multi-chicken sessions
automation:
  - alias: "Log Cuddling Chickens"
    trigger:
      - platform: mqtt
        topic: "chickens/nest1/status"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.status == 'multiple' }}"
    action:
      - service: logbook.log
        data:
          name: "Chicken Social Behavior"
          message: >
            🐔 Social Activity: {{ trigger.payload_json.chicken_count }} chickens cuddling together: 
            {{ trigger.payload_json.occupant }}
          entity_id: sensor.nest_1_status

# Track which chickens appear together most often
template:
  - trigger:
      - platform: mqtt
        topic: "chickens/nest1/status"
    sensor:
      - name: "Most Recent Cuddle Group"
        state: >
          {% if trigger.payload_json.status == 'multiple' %}
            {{ trigger.payload_json.occupant }}
          {% else %}
            {{ this.state }}
          {% endif %}
        attributes:
          chicken_list: >
            {% if trigger.payload_json.status == 'multiple' %}
              {{ trigger.payload_json.chickens }}
            {% else %}
              {{ state_attr(this.entity_id, 'chicken_list') }}
            {% endif %}
          count: >
            {% if trigger.payload_json.status == 'multiple' %}
              {{ trigger.payload_json.chicken_count }}
            {% else %}
              {{ state_attr(this.entity_id, 'count') }}
            {% endif %}
```

### 2. Scoreboard Implementation Goals

#### Daily Leaderboard Card
- Show top 10 chickens by visits today
- Display visit count, total time, average time per visit
- Update in real-time as new data arrives
- Color coding: Gold/Silver/Bronze for top 3

#### Productivity Analytics
- **Daily Champion** - Chicken with most visits today
- **Weekly Champion** - Most consistent performer
- **Total Time Champion** - Longest total nesting time
- **Quick Visitor** - Shortest average visit (possible egg-layer)
- **Marathon Sitter** - Longest single session

#### Visual Components Needed
1. **Leaderboard Table** - Sortable by different metrics
2. **Real-time Status Card** - Current nest occupancy
3. **Charts/Graphs**:
   - Daily visit trends
   - Individual chicken performance over time
   - Visit duration distribution
   - Hourly activity patterns

#### Automation Ideas
- **Daily Champion Announcement** - At 6 PM, announce the day's most productive chicken
- **Productivity Alerts** - Notify if a usually active chicken hasn't visited today
- **Egg Prediction** - Based on visit patterns, predict when eggs might be ready
- **Social Behavior Tracking** - Use `chickens/changes` to identify which chickens like to cuddle
- **Real-time Activity Feed** - Display live chicken changes and multi-chicken activities

### 3. **NEW: Enhanced Features You Can Now Build**

#### Real-Time Activity Stream
Use the `chickens/changes` topic to create a live feed:
```yaml
# Example automation to log chicken changes
automation:
  - alias: "Log Chicken Changes"
    trigger:
      platform: mqtt
      topic: "chickens/changes"
    action:
      service: notify.persistent_notification
      data:
        message: >
          🐔 Chicken Change! {{ trigger.payload_json.previous_chicken }} 
          ({{ trigger.payload_json.previous_duration }}s) → 
          {{ trigger.payload_json.new_chicken }}
```

#### Social Behavior Analysis
Track which chickens frequently follow each other:
- Monitor `chickens/changes` for patterns
- Identify "social pairs" who often visit consecutively  
- Create chicken friendship networks

#### Complete Visit Accuracy
With the new change tracking, you now get:
- **100% visit accuracy** - no more lost short visits
- **Social interaction data** - who visits with whom
- **Behavioral patterns** - rapid changes vs long sessions
- **Real-time updates** - matches exactly what ESP32 serial monitor shows

### 4. Data Storage & History

Since you'll want historical data for trends:
- Use Home Assistant's built-in database to store visit history
- Consider creating utility_meter sensors for daily/weekly/monthly totals
- Use template sensors to calculate running averages and trends

### 5. Dashboard Layout Suggestions

#### Main Chicken Dashboard
- **Current Status**: Big status card showing nest occupancy
- **Today's Leaderboard**: Top chickens today with visit counts
- **Live Activity**: Recent visits with timestamps
- **Quick Stats**: Total visits today, most active hour, etc.

#### Individual Chicken Profiles
- Create individual pages for each chicken with:
  - Personal visit history
  - Average session duration
  - Best/worst days
  - Comparison to flock average

### 6. Advanced Features to Consider

- **Chicken Health Monitoring**: Alert if patterns change dramatically
- **Seasonal Analysis**: Compare laying patterns across seasons
- **Flock Competition**: Monthly challenges and achievements
- **Integration with Weather**: Correlate activity with weather data
- **Egg Collection Tracking**: Manual input to correlate visits with actual eggs found

## Technical Notes for Implementation

- **Timestamp Handling**: ESP32 sends millis() - you may want to convert to real timestamps
- **Data Validation**: Always check if chicken exists in database before processing
- **Multi-Chicken Sessions**: These don't count toward individual scoring (marked as "multiple_chickens")
- **System Monitoring**: Track ESP32 connectivity and system health

## User's Goals

The user wants to:
1. **Track which chickens are the most productive layers**
2. **Get daily/weekly productivity reports**
3. **Identify patterns in laying behavior** 
4. **Have fun competitive element between chickens**
5. **Monitor flock health through activity patterns**

## Next Steps for Home Assistant AI

1. Set up the MQTT sensors to receive data from the ESP32
2. Create a leaderboard card showing top performers
3. Implement daily/weekly scoring systems
4. Add automation for daily champion announcements
5. Create charts for activity trends
6. Build individual chicken profile pages
7. **NEW: Implement real-time activity feed using `chickens/changes`**
8. **NEW: Add social behavior tracking and chicken interaction analysis**

The ESP32 code is now ready to publish all necessary data via MQTT. Your job is to create the Home Assistant dashboard and analytics to make this data useful and engaging!

---

## 🚨 **CRITICAL UPDATE SUMMARY FOR HOME ASSISTANT AI:**

**The chicken monitoring system now publishes COMPLETE real-time data via MQTT.** 

**You MUST add the new `chickens/changes` sensor** to capture chicken change events. This gives you:

- **Complete visit tracking** (no more lost short visits)
- **Real-time activity feed** (exactly what serial monitor shows)
- **Social behavior data** (which chickens visit together)
- **Enhanced analytics** (rapid changes, cuddling detection, behavioral patterns)

**The system is now production-ready for a comprehensive Home Assistant chicken productivity dashboard!**

---

## ⚠️ Important Disclaimer

**This project and documentation were developed with AI assistance (GitHub Copilot/Vibe).** All code, configurations, and instructions are provided "as-is" without any warranties or guarantees. Users implement this Home Assistant integration at their own risk. The original author takes no responsibility for system failures, data loss, or any other issues that may arise from using this implementation.

**Always test thoroughly in a development environment before deploying to production Home Assistant instances.**

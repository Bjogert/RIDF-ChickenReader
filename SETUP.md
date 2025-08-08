# 🔧 Setup Instructions

Follow these steps to get your Chicken RFID Monitoring System running.

## 📋 Prerequisites

- **ESP32 D1 Mini board**
  - [D1 Mini ESP32 CH9102X](https://www.hobbyelectronica.nl/en/product/d1-mini-esp32-ch9102x/?srsltid=AfmBOoruK0NZgmOS6u9iHdyrAqz3tYRwKyM4ZRi08tEqQET34rcvxian) - Recommended model
- **EL125 125KHz RFID Reader** 
  - [125KHz RFID Long Distance Module (40cm range)](https://www.electrodragon.com/product/125khz-rfid-long-distance-module-40cm-serial/) - Exact model used
- **125KHz RFID tags** (one per chicken)
- **PlatformIO IDE** or VS Code with PlatformIO extension
- **MQTT broker** (like Mosquitto or Home Assistant)

## 🔌 Hardware Assembly

### 1. Wiring Connections
**Connect ESP32 D1 Mini to EL125 RFID Reader:**

| ESP32 Pin | RFID Reader Pin | Purpose |
|-----------|-----------------|---------|
| VIN (5V)  | VCC            | Reader Power |
| 3.3V      | MODE           | Mode Control |
| GPIO16    | TX             | Data Transmission |
| GPIO18    | RES            | Reset Control |
| GND       | GND            | Ground |

### 2. Power Requirements
- **Use wall USB adapter** for stable power supply
- **Direct soldering recommended** for outdoor/permanent installations
- The dual power setup (VIN + 3.3V to MODE) is critical for reliable operation

## 💻 Software Setup

### 1. Clone Repository
```bash
git clone [your-repo-url]
cd RIDF-ChickenReader
```

### 2. Create Credentials File
```bash
# Copy the template
cp include/secrets.h.template include/secrets.h

# Edit with your credentials
notepad include/secrets.h  # Windows
# or
nano include/secrets.h     # Linux/Mac
```

### 3. Configure Your Network
Edit `include/secrets.h`:
```cpp
namespace Secrets {
  const char* WIFI_SSID = "YOUR_WIFI_NAME";
  const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
  
  const char* MQTT_SERVER = "192.168.1.100";  // Your MQTT broker IP
  const char* MQTT_USER = "your_mqtt_username";
  const char* MQTT_PASSWORD = "your_mqtt_password";
}
```

### 4. Install and Upload
1. Open project in PlatformIO
2. Build project (Ctrl+Alt+B)
3. Upload to ESP32 (Ctrl+Alt+U)
4. Open Serial Monitor (115200 baud) to see output

### 5. Multi‑Nest (A/B/C) Builds
This firmware supports multiple identical devices by assigning a nest tag via build flags.

- Predefined environments: `nestA`, `nestB`, `nestC` in `platformio.ini`
- Each publishes to `chickens/nestA|nestB|nestC/...` and uses a unique MQTT client ID

PowerShell examples:
```powershell
# Build & upload Nest A
pio run -e nestA; pio run -e nestA -t upload

# Build & upload Nest B
pio run -e nestB; pio run -e nestB -t upload

# Build & upload Nest C
pio run -e nestC; pio run -e nestC -t upload
```

Prefer numbers? Duplicate one env and set `build_flags = -DNEST_TAG="1"` (or 2/3/4...).

## 🐔 Chicken Configuration

### 1. Scan Your RFID Tags
1. Power up the system
2. Hold each RFID tag near the reader
3. Note the tag ID from serial monitor output
4. Update the chicken database in `main.cpp`

### 2. Update Chicken Database
Edit the `chickenDatabase[]` array in `src/main.cpp`:

```cpp
Chicken chickenDatabase[] = {
  {"YOUR_TAG_ID_1", "Chicken_Name_1", 1},
  {"YOUR_TAG_ID_2", "Chicken_Name_2", 2},
  // Add more chickens...
};
```

### 3. Test Individual Chickens
- Place each tag near reader
- Verify correct chicken name appears in serial monitor
- Check MQTT messages are published

## 🏠 Home Assistant Integration

### 1. MQTT Broker Setup
Ensure you have an MQTT broker running:
- **Home Assistant**: Built-in Mosquitto add-on
- **Standalone**: Install Mosquitto on Raspberry Pi or server

### 2. Configure MQTT Sensors (Per‑Nest)
Add to your Home Assistant `configuration.yaml` (adjust A→B/C as needed):
```yaml
mqtt:
  sensor:
    - name: "Nest A Status"
      state_topic: "chickens/nestA/status"
      value_template: "{{ value_json.status }}"
      json_attributes_topic: "chickens/nestA/status"

    - name: "Nest A Occupant"
      state_topic: "chickens/nestA/occupant"

    - name: "Nest A Occupants"
      state_topic: "chickens/nestA/occupants"

    - name: "Nest A Last Visit Duration"
      state_topic: "chickens/nestA/duration"
      unit_of_measurement: "s"

    # Duplicate for Nest B and C
    - name: "Nest B Status"
      state_topic: "chickens/nestB/status"
      value_template: "{{ value_json.status }}"
      json_attributes_topic: "chickens/nestB/status"
    - name: "Nest C Status"
      state_topic: "chickens/nestC/status"
      value_template: "{{ value_json.status }}"
      json_attributes_topic: "chickens/nestC/status"
```

### 3. Complete HA Setup
Follow the detailed guide in [`HOME_ASSISTANT_PROJECT_CONTEXT.md`](HOME_ASSISTANT_PROJECT_CONTEXT.md) for:
- Full sensor configurations
- Dashboard setup
- Automation examples
- Analytics and leaderboards

### 4. InfluxDB/Analytics Tips
- Subscribe and write all `chickens/nest*/visits` topics; add a tag like `nest=A|B|C` derived from the topic
- Compute per‑chicken "favorite nest" by counting visit occurrences per nest
- Build a global leaderboard by aggregating across all nests

## 🔍 Testing & Verification

### 1. Hardware Test
- Check serial monitor output at 115200 baud
- Verify WiFi connection established
- Confirm MQTT broker connection

### 2. RFID Test
- Place tag near reader
- Should see: `*** CHICKEN ENTERED NEST! ***`
- Remove tag, wait 8 seconds
- Should see: `*** CHICKEN LEFT NEST! ***`

### 3. MQTT Test
Use MQTT client (like MQTT Explorer) to verify messages:
- `chickens/nest1/status`
- `chickens/visits`
- `chickens/system/status`

## 🐛 Troubleshooting

### No RFID Detection
- ✅ Check dual power wiring (VIN + 3.3V to MODE)
- ✅ Verify GPIO16 → RFID TX connection
- ✅ Ensure 125KHz tags (not 13.56MHz)
- ✅ Test with tag very close to reader

### WiFi Issues
- ✅ Check credentials in `secrets.h`
- ✅ Verify 2.4GHz network (ESP32 doesn't support 5GHz)
- ✅ Check signal strength at installation location

### MQTT Problems
- ✅ Verify broker IP address and port
- ✅ Test broker with separate MQTT client
- ✅ Check firewall settings
- ✅ Verify credentials if authentication enabled

### Intermittent Operation
- ✅ Replace breadboard with direct soldering
- ✅ Check power supply stability
- ✅ Verify all ground connections
- ✅ Consider power supply upgrade

## 📊 Monitoring

### Serial Monitor Output
Normal operation shows:
```
🐔 Chicken Monitor Started!
WiFi Connected: 192.168.1.123
MQTT Connected: OK
System Ready - Monitoring for chickens...
```

### MQTT Activity
Monitor these topics for activity:
- `chickens/system/status` - System health
- `chickens/nest1/status` - Real-time nest status
- `chickens/visits` - Visit logging

## 🚀 Next Steps

1. **Test System**: Verify basic operation with one chicken
2. **Add All Chickens**: Scan and configure all RFID tags
3. **Install Hardware**: Mount in chicken coop securely
4. **Configure Home Assistant**: Set up dashboard and automation
5. **Monitor & Optimize**: Watch patterns and adjust as needed

---

**Need Help?** Check the main README.md or create an issue in the repository!

## ⚠️ Important Disclaimer

**This project was developed with AI assistance (GitHub Copilot/Vibe).** The code and instructions are provided "as-is" without warranties. Users implement at their own risk. Always test thoroughly before deploying in production environments.

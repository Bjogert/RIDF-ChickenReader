# 🐔 Chicken RFID Monitoring System

An intelligent chicken coop monitoring system that tracks individual chickens using RFID technology to monitor egg-laying behavior and productivity. Features multi-chicken detection, MQTT integration with Home Assistant, and comprehensive analytics.

## 🎯 Features

- **Individual Chicken Tracking** - Each chicken has a unique RFID tag
- **Smart Enter/Exit Detection** - Automatic presence detection with reset control
- **Multi-Chicken Detection** - Detects when multiple chickens are together ("cuddling")
- **Real-time MQTT Publishing** - Complete integration with Home Assistant
- **Visit Analytics** - Track visit duration, frequency, and patterns
- **Anti-Spam Logic** - Only publishes on actual state changes
- **Robust Hardware Design** - Dual power configuration for maximum stability

## 🔧 Hardware Requirements

### Components
- **ESP32 D1 Mini** - Main controller
  - [D1 Mini ESP32 CH9102X](https://www.hobbyelectronica.nl/en/product/d1-mini-esp32-ch9102x/?srsltid=AfmBOoruK0NZgmOS6u9iHdyrAqz3tYRwKyM4ZRi08tEqQET34rcvxian) - Tested hardware
- **EL125 125KHz RFID Reader** - Long-range tag detection (40cm range)
  - [125KHz RFID Long Distance Module](https://www.electrodragon.com/product/125khz-rfid-long-distance-module-40cm-serial/) - Exact model used
- **125KHz RFID Tags** - One per chicken (14 chickens supported)
- **Wall USB Power Supply** - For stable power delivery

### Shopping List (Approximate Cost)
| Component | Quantity | Est. Cost |
|-----------|----------|-----------|
| ESP32 D1 Mini | 1 | €8-12 |
| EL125 RFID Reader | 1 | €20-25 |
| 125KHz RFID Tags | 15+ | €1-2 each |
| USB Power Supply | 1 | €5-10 |
| **Total** | - | **€50-70** |

> 💡 **Tip:** Order a few extra RFID tags as spares - chickens can be rough on equipment!

### Wiring Diagram
```
ESP32 D1 Mini ↔ EL125 RFID Reader
─────────────────────────────────
VIN (5V)      ↔ VCC (Reader Power)
3.3V          ↔ MODE Pin (Required!)
GPIO16        ↔ TX (Data)
GPIO18        ↔ RES (Reset Control)
GND           ↔ GND
```

> **⚠️ Critical:** The dual power setup (VIN + 3.3V) is essential for stable operation!

### Connection Method
- **Direct Soldering Recommended** - For maximum reliability in outdoor conditions
- Breadboard connections may cause intermittent failures

## 📡 MQTT Integration

The system publishes real-time data to these MQTT topics:

### Real-time Status
- `chickens/nest1/status` - JSON with nest status (empty/occupied/multiple)
- `chickens/nest1/occupant` - Current chicken name(s)
- `chickens/nest1/duration` - Visit duration when chicken leaves

### Analytics & Events
- `chickens/visits` - Individual visit records with duration and timestamps
- `chickens/changes` - Real-time chicken change events within sessions
- `chickens/leaderboard` - Top performing chickens with statistics

### Example MQTT Messages

**Single Chicken:**
```json
{
  "status": "occupied",
  "occupant": "Lady Kluck",
  "timestamp": 1643723400,
  "duration": 120
}
```

**Multiple Chickens:**
```json
{
  "status": "multiple", 
  "occupant": "Ronny, Ada, Skrik",
  "chickens": ["Ronny", "Ada", "Skrik"],
  "chicken_count": 3,
  "timestamp": 1643723400
}
```

## 🏠 Home Assistant Integration

Complete Home Assistant setup is documented in [`HOME_ASSISTANT_PROJECT_CONTEXT.md`](HOME_ASSISTANT_PROJECT_CONTEXT.md), including:

- MQTT sensor configurations
- Dashboard layouts and cards
- Automation examples
- Leaderboard and analytics setup
- Social behavior tracking

## 🚀 Quick Start

### 1. Hardware Setup
1. Wire the ESP32 and RFID reader according to the diagram above
2. Ensure dual power configuration (VIN + 3.3V to MODE pin)
3. Use direct soldering for outdoor installations

### 2. Software Setup
1. Clone this repository
2. Copy `include/secrets.h.template` to `include/secrets.h`
3. Edit `secrets.h` with your WiFi and MQTT credentials
4. Upload to ESP32 using PlatformIO

### 3. Configure Your Chickens
Edit the `chickenDatabase[]` array in `main.cpp` with your chickens' RFID tag IDs:

```cpp
Chicken chickenDatabase[] = {
  {"2003E98C8", "Lady Kluck", 1},
  {"2003EF40D", "Ronny", 2},
  // Add your chickens here...
};
```

### 4. Scan Your Tags
1. Hold each RFID tag near the reader
2. Check serial monitor output for tag IDs
3. Update the database with actual tag IDs

## 📊 Chicken Database

Currently configured for 14 chickens with real scanned tag IDs:

- **Lady Kluck** (Tag: 2003E98C8) - Lead hen
- **Ronny** (Tag: 2003EF40D) - Social butterfly  
- **Ada** (Tag: 2003F2676) - Reliable layer
- **Ms.Foster** (Tag: 2003E98F1) - Experienced
- **Kiwi** (Tag: 2003E586A) - Energetic
- **Skrik** (Tag: 2003E956D) - Cautious
- **Panik** (Tag: 200336896) - Nervous type
- **Gästrid** (Tag: 20032D5A4A) - Visitor favorite
- **Chick_1_2025** through **Chick_6_2025** - Young generation

## 🔍 System Logic

### Presence Detection
- **Check Interval:** Every 30 seconds
- **Exit Detection:** 8 seconds after reset without tag = chicken left
- **Multi-Chicken Mode:** Triggered when 2+ different chickens detected rapidly

### Data Flow
1. **Enter Event:** `*** CHICKEN ENTERED NEST! ***`
2. **Change Event:** `>>> CHICKEN CHANGE! <<<` (within session)
3. **Multi-Chicken:** `*** MULTIPLE CHICKENS DETECTED! ***`
4. **Exit Event:** `*** CHICKEN LEFT NEST! ***`

## 🛠️ Development

### Project Structure
```
RIDF-ChickenReader/
├── src/
│   └── main.cpp              # Main application code
├── include/
│   ├── secrets.h.template    # Credentials template
│   └── secrets.h            # Your credentials (git-ignored)
├── platformio.ini           # PlatformIO configuration
├── HOME_ASSISTANT_PROJECT_CONTEXT.md  # HA integration guide
└── README.md               # This file
```

### Key Functions
- `findChickenByTag()` - Database lookup
- `resetReader()` - Hardware reset for presence checking  
- `detectMultipleChickens()` - Multi-chicken session handling
- `publishNestStatus()` - MQTT publishing
- `extractTagID()` - ASCII to hex tag conversion

## 🐛 Troubleshooting

### Common Issues

**No RFID Detection:**
- Check dual power wiring (VIN + 3.3V to MODE)
- Verify GPIO16 connection to RFID TX
- Ensure direct soldered connections

**Intermittent Failures:**
- Replace breadboard connections with direct soldering
- Check power supply stability
- Verify 3.3V on MODE pin

**MQTT Not Publishing:**
- Check WiFi credentials in `secrets.h`
- Verify MQTT broker IP and credentials
- Monitor serial output for connection status

### Debug Output
Enable detailed logging by monitoring the serial output at 115200 baud. The system provides comprehensive debugging information for all operations.

## 📈 Future Enhancements

- **Door Monitoring** - Track chickens entering/exiting coop
- **Weather Integration** - Correlate activity with weather patterns
- **Health Monitoring** - Alert on unusual behavior patterns
- **Mobile App** - Direct chicken status monitoring
- **Multiple Nests** - Expand to monitor multiple laying boxes

## 🤝 Contributing

Contributions welcome! This project is perfect for:
- IoT enthusiasts
- Chicken keepers
- Home automation fans
- Agricultural technology developers

## 📝 License

This project is open source. Feel free to modify and adapt for your own chicken monitoring needs!

## ⚠️ Disclaimer

**This project was developed with AI assistance (GitHub Copilot/Vibe).** The code is provided "as-is" without any warranties or guarantees. Users implement this system at their own risk. The original author takes no responsibility for:

- Hardware failures or damage
- Data loss or system malfunctions  
- Livestock safety or monitoring accuracy
- Any damages resulting from use of this system

**Always test thoroughly before relying on this system for critical monitoring applications.**

---

**Happy Chicken Monitoring! 🐔📡**

# SmokerTemp Controller - ESP8266 Temperature Monitoring System

A web-based temperature monitoring and control system for ESP8266, designed for temperature control applications such as smokers, ovens, or other heating systems.

## Features Overview

### 1. Real-Time Temperature Monitoring
- **Thermistor-based sensing**: Uses NTC 100kΩ thermistor with Steinhart-Hart equation for accurate temperature calculation
- **Live temperature display**: Updates every 3 seconds on the web interface
- **Temperature range**: Calibrated for 50°F - 300°F
- **Relay control**: Automatic on/off control based on desired temperature with 5°F hysteresis

### 2. Interactive Web Interface
- **Responsive design**: Works on desktop, tablet, and mobile browsers
- **Real-time graphing**: Temperature vs. time chart using Highcharts library
- **Dual temperature tracking**: Displays both current and desired temperatures
- **Visual relay status**: Color-coded indicator (red = ON, grey = OFF)

### 3. Temperature Control System
- **Set-point control**: User-configurable desired temperature (0-500°F range)
- **Adjustable hysteresis**: User-configurable dead-band (1-20°F) to prevent relay chattering
- **Default hysteresis**: 5°F (can be changed via web interface)
- **Persistence**: Hysteresis setting persists in ESP8266 RAM until power cycle
- **Automatic relay management**: 
  - Turns ON when temperature < (desired - hysteresis)
  - Turns OFF when temperature > (desired + hysteresis)

### 4. Data Logging System
- **Automatic logging**: Records data every 30 seconds
- **CSV format**: Easy export to Excel for analysis
- **Logged parameters**:
  - Time stamp (12-hour format with AM/PM)
  - Current temperature (°F)
  - Desired temperature (°F)
  - Relay status (ON/OFF)
- **Copy to clipboard**: One-click copy of all logged data
- **Clear data**: Reset log without page refresh
- **Scrollable view**: Fixed-height display with automatic scrolling

### 5. Temperature Calibration (Linear Regression)
Advanced calibration system using 5-point linear regression for improved accuracy.

#### How It Works:
1. **Collect calibration points**: Enter 5 pairs of sensor readings and actual temperatures
2. **Linear regression calculation**: JavaScript calculates best-fit line using least squares method
3. **Quality assessment**: R² (coefficient of determination) indicates calibration quality:
   - R² > 0.95: Excellent fit
   - R² > 0.85: Good fit
   - R² ≤ 0.85: Fair fit
4. **Apply calibration**: New slope and offset sent to ESP8266
5. **Persistence**: Calibration stays in ESP8266 RAM until power cycle

#### Calibration Equation:
```
Temp_calibrated = m × Temp_sensor + c
```
Where:
- `m` = slope (multiplicative correction factor)
- `c` = offset (additive correction factor)

#### Default Calibration:
- Slope (m) = 1.025
- Offset (c) = -8.155
- Based on empirical testing in 170°F - 220°F range

#### Usage Instructions:
1. **Gather reference data**: Use a calibrated thermometer to measure actual temperatures
2. **Enter 5 data points** in the calibration table:
   - Point 1-5: Sensor reading vs. Actual temperature
   - Points can be in any order
   - Spread points across your operating range for best results
3. **Click "Perform Calibration"**: System calculates and applies new calibration
4. **Review results**: Check R² value and equation
5. **Refresh page**: Calibration persists and displays automatically
6. **Reset if needed**: "Reset to Default" button restores original calibration

#### Validation:
- All 5 points must be filled
- Values must be valid numbers
- Temperature range: 0-500°F
- Prevents NaN or infinite values

## Hardware Requirements

### Components:
- ESP8266 (e.g., NodeMCU, Wemos D1 Mini)
- NTC 100kΩ thermistor (3950 B-coefficient)
- 9.97kΩ series resistor (for voltage divider)
- Relay module (for heating element control)
- Power supply (appropriate for ESP8266 and relay)

### Pin Configuration:
- `A0` - Thermistor analog input
- `D1` - Digital pin (kept HIGH for thermistor power)
- `D2` - Relay control output
- `D4` - Status LED (inverted logic - LOW = ON)

### Circuit Design:
```
VCC (3.3V) ─── D1 (HIGH) ─── Thermistor ─── A0 ─── 9.97kΩ ─── GND
```

## Software Configuration

### WiFi Settings:
Edit in code (lines 14-15):
```cpp
const char *ssid = "YourWiFiSSID"; 
const char *password = "YourWiFiPassword";
```

### Thermistor Parameters:
```cpp
#define SERIESRESISTOR 9970        // Series resistor (Ω)
#define THERMISTORNOMINAL 100000   // Thermistor resistance at 25°C (Ω)
#define TEMPERATURENOMINAL 25      // Temperature for nominal resistance (°C)
#define BCOEFFICIENT 3950          // Beta coefficient
```

### Control Parameters:
```cpp
uint16_t hysteresis = 5;  // Temperature dead-band (°F) - default value
```
Note: Hysteresis can be changed at runtime via the web interface (1-20°F range).

### Update Intervals:
- Temperature reading: Continuous (loop cycle)
- Graph/display update: 3 seconds
- Data logging: 30 seconds
- Relay status check: 3 seconds

## Web Interface URLs

### Access Points:
- **Main page**: `http://<ESP8266_IP>/` or `http://esp8266.local/`
- **Set temperature**: `http://<ESP8266_IP>/setTemp?temp=<value>`
- **Get current temp**: `http://<ESP8266_IP>/getTemp`
- **Get desired temp**: `http://<ESP8266_IP>/getDesiredTemp`
- **Get relay status**: `http://<ESP8266_IP>/getRelayStatus`
- **Set calibration**: `http://<ESP8266_IP>/setCalibration?slope=<m>&offset=<c>`
- **Get calibration**: `http://<ESP8266_IP>/getCalibration`
- **Set hysteresis**: `http://<ESP8266_IP>/setHysteresis?value=<1-20>`
- **Get hysteresis**: `http://<ESP8266_IP>/getHysteresis`

## Installation & Setup

### 1. Hardware Setup:
1. Connect thermistor to A0 via voltage divider
2. Connect relay to D2
3. Connect LED indicator (optional)
4. Power ESP8266 appropriately

### 2. Software Upload:
1. Install ESP8266 board support in Arduino IDE
2. Install required libraries (none needed - uses built-in libraries)
3. Update WiFi credentials in code
4. Upload sketch to ESP8266
5. Open Serial Monitor (9600 baud) to see IP address

### 3. Web Access:
1. Connect to same WiFi network
2. Navigate to displayed IP address or `http://esp8266.local`
3. Set desired temperature
4. Monitor real-time temperature and relay status

## Usage Tips

### For Best Calibration Results:
- Spread calibration points across your operating range
- Use a reliable reference thermometer
- Allow system to stabilize at each temperature
- More points at extremes improve accuracy
- Re-calibrate if you change thermistor or circuit

### Data Logging Best Practices:
- Start logging at beginning of cook/session
- Copy data to clipboard before clearing
- Paste into Excel for analysis and graphing
- 30-second interval balances detail vs. data volume

### Temperature Control:
- Allow 5-10 minutes for system stabilization
- Adjust hysteresis based on your application:
  - **Smaller hysteresis (1-3°F)**: Tighter temperature control, more relay cycling
  - **Larger hysteresis (10-20°F)**: Less relay cycling, wider temperature swings
  - **Default (5°F)**: Balanced for most applications
- Hysteresis setting persists until ESP8266 power cycle
- Adjust desired temp in small increments for fine control



## Technical Details

### Temperature Calculation Algorithm:
1. Read ADC value (0-1023) from A0
2. Average 5 readings for noise reduction
3. Convert to resistance using voltage divider formula
4. Apply Steinhart-Hart equation for thermistor
5. Convert Kelvin → Celsius → Fahrenheit
6. Apply calibration: `Temp_cal = m × Temp_raw + c`
7. Round to nearest integer

### Linear Regression Math:
Uses least squares method to minimize residuals:
```
m = (n·ΣXY - ΣX·ΣY) / (n·ΣX² - (ΣX)²)
c = (ΣY - m·ΣX) / n
R² = 1 - (SS_residual / SS_total)
```

### Data Persistence:
- **Calibration**: ESP8266 RAM (volatile - resets on power cycle)
- **Hysteresis**: ESP8266 RAM (volatile - resets on power cycle)
- **Data log**: Browser memory (cleared on page refresh)
- **Desired temperature**: ESP8266 RAM (persists during session)





---

**For support or questions, refer to project documentation or course materials.**

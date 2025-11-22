#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define SERIESRESISTOR 9970    
#define THERMISTORPIN A0 
#define DIGITALPIN D1
#define RELAYPIN D2
#define TEMPERATURENOMINAL 25 
#define BCOEFFICIENT 3950
#define THERMISTORNOMINAL 100000 
#define LED_PIN D4  // GPIO2 (D4)
#define MAX_POINTS 9

const char *ssid = ""; 
const char *password = ""; // 

ESP8266WebServer server(80);

int samples[5];
uint16_t desiredTemp = 0;  
uint16_t currentTemp = 0;
bool input = false;
bool isValid = true;
bool relayStatus = false;

// Calibration variables (default values)
float calSlope = 1.025;
float calOffset = -8.155;

// Hysteresis variable (default value)
uint16_t hysteresis = 5;

bool ledState = false;  // Track current LED state
unsigned long loopCounter = 0;
const unsigned long toggleInterval = 50;  // Number of loop iterations before toggle

float calculateTemperature() {
  uint8_t i;
  float reading;
  reading = 0;

  for (i=0; i<5; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
    reading += samples[i];
  }
 
  reading /= 5;
 
  // convert the value to resistance
  reading = (1023 / reading) - 1;     // (1023/ADC - 1) 
  reading = SERIESRESISTOR / reading;  // 10K / (1023/ADC - 1)

  float steinhart, tempC, tempF;
  steinhart = reading / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
 
  tempC = steinhart - 273.15;                  // convert Kelvin to C
  tempF = (tempC * 9.0/5.0) + 32.0;                  // convert C to F

  //Serial.print("Temp: "); 
  //Serial.println(tempF);
  tempF = calSlope * tempF + calOffset;   // calibration linear equation; works well 170F-220F
  return (uint16_t)roundf(tempF);
}

void mainPage() {
  // Main page layout
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ECE 554 Project</title>
    <script src="https://code.highcharts.com/highcharts.js"></script>
    <style>
      body {
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        min-height: 100vh;
        margin: 0;
        padding-top: 80px;
        font-family: Tahoma, sans-serif;
        text-align: center;
        background-image: url('https://static.vecteezy.com/system/resources/previews/004/968/002/non_2x/cute-abstract-modern-background-free-vector.jpg'); 
        background-size: cover; 
        background-position: center;
        background-repeat: no-repeat;
        color: #000000;
      }
      header {
        position: absolute;
        top: 0px;
        width: 100%;
        text-align: center;
        background-color: rgba(255, 255, 255, 0.8);
        padding: 5px 0;
      }
      h1 {
        margin: 0;
        font-size: 2rem;
      } 
      button {
        font-size: 1.5rem;
        border: none;
        background: none;
        cursor: pointer;
      }
      form {
        margin: 10px 0;
      }
      .relay-light {
        width: 20px;
        height: 20px;
        border-radius: 50%;
        display: inline-block;
      }
      .data-container {
        width: 80%;
        margin-top: 30px;
        text-align: center;
      }
      .data-box {
        width: 100%;
        height: 300px;
        overflow-y: auto;
        border: 2px solid #333;
        background-color: rgba(255, 255, 255, 0.9);
        padding: 10px;
        font-family: 'Courier New', monospace;
        font-size: 0.9rem;
        text-align: left;
        white-space: pre;
        margin-top: 10px;
      }
      .button-container {
        margin-top: 10px;
      }
      .action-button {
        padding: 10px 20px;
        margin: 5px;
        font-size: 1rem;
        cursor: pointer;
        background-color: #337BFF;
        color: white;
        border: none;
        border-radius: 5px;
      }
      .action-button:hover {
        background-color: #2563D5;
      }
      .clear-button {
        background-color: #FF5733;
      }
      .clear-button:hover {
        background-color: #D84315;
      }
      .cal-container {
        width: 80%;
        margin-top: 30px;
        margin-bottom: 30px;
        text-align: center;
        background-color: rgba(255, 255, 255, 0.85);
        padding: 20px;
        border-radius: 10px;
        border: 2px solid #337BFF;
      }
      .cal-input {
        width: 80px;
        padding: 5px;
        margin: 2px;
        font-size: 0.9rem;
        text-align: center;
      }
      .cal-equation {
        font-family: 'Courier New', monospace;
        font-size: 1.1rem;
        margin: 10px 0;
        font-weight: bold;
      }
      .cal-table {
        margin: 20px auto;
        border-collapse: collapse;
        background-color: white;
      }
      .cal-table th, .cal-table td {
        border: 1px solid #333;
        padding: 8px 12px;
        text-align: center;
      }
      .cal-table th {
        background-color: #337BFF;
        color: white;
        font-weight: bold;
      }
      .cal-results {
        background-color: rgba(51, 123, 255, 0.1);
        padding: 15px;
        border-radius: 5px;
        margin: 15px 0;
        border: 2px solid #337BFF;
      }
    </style>
  </head>
  <body>
    <header>
      <h1>SmokerTemp Controller</h1>
    </header>
  )rawliteral";
  
  // Add error message if input is invalid
  if (isValid == false) {
    page += "<h3 style=\"color: red;\">Invalid Input: Please type a value between 0 and 500.</h3>"; 
  }

  // Add input form
  page += "<form action=\"/setTemp\" method=\"GET\">";
  page += "<h3 style=\"display: inline; margin-right: 10px;\">Desired Temperature (&deg;F):</h3>";
  page += "<input type=\"text\" name=\"temp\" style=\"display: inline; margin-right: 10px;\">";
  page += "<input type=\"submit\" value=\"Set\" style=\"display: inline;\">";
  page += "</form>";

  // Display current and desired temperature values
  page += "<h3 id=\"currentTemp\" style=\"margin-bottom: 5px;\">Current Temperature: <span id='ctempValue'></span></h3>";

  if (input == false) {
    page += "<h3 style=\"margin-top: 0; margin-bottom: 5px;\">Desired Temperature: N/A. Please enter a temperature value.</h3>";
  } 
  else {
    page += "<h3 id=\"desiredTemp\" style=\"margin-top: 0; margin-bottom: 5px;\">Desired Temperature: <span id='dtempValue'></span></h3>";
  }
  
  // Relay Status Light
  page += R"rawliteral(
  <div id="relay-color" class="relay-light"></div>
  <span id="relay-text" style="font-weight: bold;"></span>

  <script>
    // Data logging array
    var dataLog = [];

    function updateRelayStatus() {
      fetch('/getRelayStatus') 
        .then(response => response.text()) 
        .then(status => {
          var relayStatus = (status === 'true'); // check true or false

          // Get the color and text
          var relayLight = document.getElementById('relay-color');
          var statusText = document.getElementById('relay-text');

          if (relayStatus) {
            relayLight.style.backgroundColor = 'red'; 
            statusText.innerText = 'Relay ON';
          } 
          else {
            relayLight.style.backgroundColor = 'grey'; 
            statusText.innerText = 'Relay OFF';
          }
        });
    }

    setInterval(updateRelayStatus, 3000); // 3 seconds
  </script>
  )rawliteral";
  page += "<hr style=\"border: 0; border-top: 1px solid transparent; width: 80%; margin-top: 20px; margin-bottom: 20px;\">";
  
  // Plot
  page += R"rawliteral(
  <div id="chart-temperature" class="container" style="width: 80%; height: 400px;"></div>

  <script>
    // Initialize Highcharts
    var chartT = Highcharts.chart('chart-temperature', {
      chart: { type: 'line' },
      title: { text: 'Temperature Readings' },
      xAxis: {
        title: { text: 'Time (EST)' },
        type: 'datetime',
        dateTimeLabelFormats: {
          second: '%I:%M:%S' 
        },
        labels: {
          formatter: function () {
            let estOffset = 18000000; 
            let localTime = this.value - estOffset; // Convert to EST
            return Highcharts.dateFormat('%I:%M:%S', localTime); 
          }
        }
      },
      yAxis: { title: { text: 'Temperature (°F)' },
              min: 50,  // Set the minimum value of the y-axis
              max: 300  // Set the maximum value of the y-axis 
      },
      series: [
        { 
          name: 'Current Temperature', 
          data: [], 
          color: '#FF5733', 
          marker: { enabled: true, radius: 4, symbol: 'circle' } 
        },
        { 
          name: 'Desired Temperature', 
          data: [], 
          color: '#337BFF', 
          dataLabels: { enabled: false },
          marker: { enabled: false } 
        }
      ],
      plotOptions: {
        series: {
          dataLabels: {
            enabled: true,
            style: {
              color: '#000000', 
              fontSize: '12px', 
              fontWeight: 'bold', 
            }
          }
        }
      },
      tooltip: {
        enabled: false
      },
    });
  </script>
  )rawliteral";

  // Function to update desired temperature graph (if valid)
  if (input) {
    page += R"rawliteral(
    <script>
      function graphDesiredTemperature() {
        fetch('/getDesiredTemp')
          .then(response => response.text())
          .then(dtemp => {
            // Update the desired temperature on the webpage
            document.getElementById('desiredTemp').innerHTML = "Desired Temperature: " + dtemp + "&deg;F";
            
            var yDesired = parseFloat(dtemp);
            var xDesired = new Date().getTime();
            chartT.series[1].addPoint([xDesired, yDesired], true, chartT.series[1].data.length > 500);
          })
      }

      setInterval(graphDesiredTemperature, 3000);
    </script>
    )rawliteral";
  }

  // Function to update current temperature value and graph
  page += R"rawliteral(
  <script>
    function graphCurrentTemperature() {
      fetch('/getTemp')
        .then(response => response.text())
        .then(ctemp => {
          // Update the current temperature on the webpage
          document.getElementById('currentTemp').innerHTML = "Current Temperature: " + ctemp + "&deg;F";

          // Add data point to Highcharts graph
          var xCurrent = new Date().getTime();   
          var yCurrent = parseFloat(ctemp);      
          chartT.series[0].addPoint([xCurrent, yCurrent], true, chartT.series[0].data.length > 500);
        })
    }

    setInterval(graphCurrentTemperature, 3000); // 3 seconds
  </script>
  )rawliteral";

  // Hysteresis control section
  page += R"rawliteral(
  <div style="margin-top: 30px; text-align: center;">
    <h3 style="display: inline; margin-right: 10px;">Hysteresis (&deg;F):</h3>
    <input type="text" id="hysteresisInput" style="display: inline; margin-right: 10px; width: 80px;" placeholder="5">
    <input type="button" value="Apply" style="display: inline;" onclick="applyHysteresis()">
    <p style="margin: 5px 0; font-size: 0.9rem; color: #555;">Enter a value between 1 and 20°F. Current: <span id="currentHysteresis"></span>°F</p>
  </div>

  <script>
    function getHysteresis() {
      fetch('/getHysteresis')
        .then(response => response.text())
        .then(value => {
          document.getElementById('currentHysteresis').textContent = value;
          document.getElementById('hysteresisInput').placeholder = value;
        });
    }
    
    function applyHysteresis() {
      var hystValue = document.getElementById('hysteresisInput').value.trim();
      
      if (hystValue === '') {
        return; // Ignore empty input
      }
      
      var hystNum = parseFloat(hystValue);
      
      // Validate range
      if (isNaN(hystNum) || hystNum < 1 || hystNum > 20) {
        return; // Ignore invalid input
      }
      
      // Send to ESP8266
      fetch('/setHysteresis?value=' + hystNum)
        .then(response => response.text())
        .then(result => {
          if (result === 'OK') {
            document.getElementById('currentHysteresis').textContent = hystNum;
            document.getElementById('hysteresisInput').value = '';
            document.getElementById('hysteresisInput').placeholder = hystNum;
          }
        });
    }
    
    // Get current hysteresis on page load
    getHysteresis();
  </script>
  )rawliteral";
  
  // Data logging section with combined fetch
  page += R"rawliteral(
  <script>
    function logData() {
      // Fetch all data simultaneously
      Promise.all([
        fetch('/getTemp').then(r => r.text()),
        fetch('/getDesiredTemp').then(r => r.text()),
        fetch('/getRelayStatus').then(r => r.text())
      ]).then(([currentTemp, desiredTemp, relayStatus]) => {
        // Get current time in 12-hour format
        var now = new Date();
        var hours = now.getHours();
        var minutes = now.getMinutes();
        var seconds = now.getSeconds();
        var ampm = hours >= 12 ? 'PM' : 'AM';
        hours = hours % 12;
        hours = hours ? hours : 12; // 0 should be 12
        minutes = minutes < 10 ? '0' + minutes : minutes;
        seconds = seconds < 10 ? '0' + seconds : seconds;
        var timeStr = hours + ':' + minutes + ':' + seconds + ' ' + ampm;
        
        // Format relay status
        var relayStr = (relayStatus === 'true') ? 'ON' : 'OFF';
        
        // Add to data log array
        dataLog.push({
          time: timeStr,
          currentTemp: currentTemp,
          desiredTemp: desiredTemp,
          relayStatus: relayStr
        });
        
        // Update display
        updateDataDisplay();
      });
    }
    
    function updateDataDisplay() {
      var csvText = 'Time,Current Temp (°F),Desired Temp (°F),Relay Status\n';
      dataLog.forEach(function(entry) {
        csvText += entry.time + ',' + entry.currentTemp + ',' + entry.desiredTemp + ',' + entry.relayStatus + '\n';
      });
      document.getElementById('dataBox').textContent = csvText;
      
      // Update data count
      document.getElementById('dataCount').textContent = 'Total Data Points: ' + dataLog.length;
    }
    
    function copyToClipboard() {
      var dataBox = document.getElementById('dataBox');
      var textToCopy = dataBox.textContent;
      
      navigator.clipboard.writeText(textToCopy).then(function() {
        alert('Data copied to clipboard!');
      }, function() {
        // Fallback for older browsers
        var textArea = document.createElement('textarea');
        textArea.value = textToCopy;
        document.body.appendChild(textArea);
        textArea.select();
        document.execCommand('copy');
        document.body.removeChild(textArea);
        alert('Data copied to clipboard!');
      });
    }
    
    function clearData() {
      if (confirm('Are you sure you want to clear all data?')) {
        dataLog = [];
        updateDataDisplay();
      }
    }
    
    // Start logging data every 30 seconds
    setInterval(logData, 30000);
    // Log initial data point immediately
    setTimeout(logData, 2000);
  </script>

  <div class="data-container">
    <h2>Data Log</h2>
    <p id="dataCount">Total Data Points: 0</p>
    <div id="dataBox" class="data-box">Time,Current Temp (°F),Desired Temp (°F),Relay Status
</div>
    <div class="button-container">
      <button class="action-button" onclick="copyToClipboard()">Copy to Clipboard</button>
      <button class="action-button clear-button" onclick="clearData()">Clear Data</button>
    </div>
  </div>

  <div class="cal-container">
    <h2>Temperature Calibration (Linear Regression)</h2>
    <p style="font-size: 0.9rem; color: #555;">Enter 5 calibration points: sensor readings and corresponding actual temperatures</p>
    
    <table class="cal-table">
      <thead>
        <tr>
          <th>Point</th>
          <th>Sensor Reading (°F)</th>
          <th>Actual Temperature (°F)</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td>1</td>
          <td><input type="text" id="sensor1" class="cal-input" placeholder="0"></td>
          <td><input type="text" id="actual1" class="cal-input" placeholder="0"></td>
        </tr>
        <tr>
          <td>2</td>
          <td><input type="text" id="sensor2" class="cal-input" placeholder="0"></td>
          <td><input type="text" id="actual2" class="cal-input" placeholder="0"></td>
        </tr>
        <tr>
          <td>3</td>
          <td><input type="text" id="sensor3" class="cal-input" placeholder="0"></td>
          <td><input type="text" id="actual3" class="cal-input" placeholder="0"></td>
        </tr>
        <tr>
          <td>4</td>
          <td><input type="text" id="sensor4" class="cal-input" placeholder="0"></td>
          <td><input type="text" id="actual4" class="cal-input" placeholder="0"></td>
        </tr>
        <tr>
          <td>5</td>
          <td><input type="text" id="sensor5" class="cal-input" placeholder="0"></td>
          <td><input type="text" id="actual5" class="cal-input" placeholder="0"></td>
        </tr>
      </tbody>
    </table>
    
    <div class="button-container">
      <button class="action-button" onclick="performCalibration()">Perform Calibration</button>
      <button class="action-button clear-button" onclick="clearCalPoints()">Clear All Points</button>
      <button class="action-button clear-button" onclick="resetCalibration()">Reset to Default</button>
    </div>
    
    <div id="calResults" class="cal-results" style="display: none;">
      <h3>Calibration Results</h3>
      <p class="cal-equation" id="calEquation"></p>
      <p id="calValues" style="font-size: 1rem;"></p>
      <p id="calR2" style="font-size: 1rem;"></p>
    </div>
    
    <p id="calStatus" style="margin-top: 10px; font-weight: bold;"></p>
  </div>

  <script>
    // Calibration default values
    const DEFAULT_SLOPE = 1.025;
    const DEFAULT_OFFSET = -8.155;
    
    // Initialize calibration - fetch current values from ESP8266
    function initCalibration() {
      fetch('/getCalibration')
        .then(response => response.text())
        .then(result => {
          let values = result.split(',');
          let slope = parseFloat(values[0]);
          let offset = parseFloat(values[1]);
          console.log('Current ESP8266 calibration: m=' + slope + ', c=' + offset);
          
          // Display current calibration if not default
          if (slope !== DEFAULT_SLOPE || offset !== DEFAULT_OFFSET) {
            document.getElementById('calResults').style.display = 'block';
            let sign = offset >= 0 ? '+' : '';
            document.getElementById('calEquation').textContent = 
              'Temp_calibrated = ' + slope.toFixed(4) + ' × Temp_sensor ' + sign + ' ' + offset.toFixed(4);
            document.getElementById('calValues').textContent = 
              'Slope (m) = ' + slope.toFixed(4) + ',  Offset (c) = ' + offset.toFixed(4);
            document.getElementById('calR2').textContent = 
              'Active calibration (persists until ESP8266 reset)';
          }
        })
        .catch(error => {
          console.log('Could not fetch calibration from ESP8266');
        });
    }
    
    function sendCalibrationToESP(slope, offset) {
      fetch('/setCalibration?slope=' + slope + '&offset=' + offset)
        .then(response => response.text())
        .then(result => {
          console.log('Calibration sent to ESP8266: m=' + slope + ', c=' + offset);
        });
    }
    
    function clearCalPoints() {
      for (let i = 1; i <= 5; i++) {
        document.getElementById('sensor' + i).value = '';
        document.getElementById('actual' + i).value = '';
      }
      document.getElementById('calResults').style.display = 'none';
      document.getElementById('calStatus').textContent = '';
    }
    
    function performCalibration() {
      // Collect all data points
      let sensorReadings = [];
      let actualTemps = [];
      
      for (let i = 1; i <= 5; i++) {
        let sensor = document.getElementById('sensor' + i).value.trim();
        let actual = document.getElementById('actual' + i).value.trim();
        
        if (sensor === '' || actual === '') {
          document.getElementById('calStatus').style.color = 'red';
          document.getElementById('calStatus').textContent = 'Error: All 5 points must be filled!';
          return;
        }
        
        let sensorVal = parseFloat(sensor);
        let actualVal = parseFloat(actual);
        
        if (isNaN(sensorVal) || isNaN(actualVal)) {
          document.getElementById('calStatus').style.color = 'red';
          document.getElementById('calStatus').textContent = 'Error: Please enter valid numbers!';
          return;
        }
        
        if (sensorVal < 0 || sensorVal > 500 || actualVal < 0 || actualVal > 500) {
          document.getElementById('calStatus').style.color = 'red';
          document.getElementById('calStatus').textContent = 'Error: Temperature values must be between 0 and 500°F!';
          return;
        }
        
        sensorReadings.push(sensorVal);
        actualTemps.push(actualVal);
      }
      
      // Perform linear regression: y = mx + c
      // where y = actual temp, x = sensor reading
      let n = 5;
      let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
      
      for (let i = 0; i < n; i++) {
        sumX += sensorReadings[i];
        sumY += actualTemps[i];
        sumXY += sensorReadings[i] * actualTemps[i];
        sumX2 += sensorReadings[i] * sensorReadings[i];
        sumY2 += actualTemps[i] * actualTemps[i];
      }
      
      // Calculate slope (m) and offset (c)
      let slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
      let offset = (sumY - slope * sumX) / n;
      
      // Calculate R-squared (coefficient of determination)
      let meanY = sumY / n;
      let ssTotal = 0;
      let ssResidual = 0;
      
      for (let i = 0; i < n; i++) {
        let predicted = slope * sensorReadings[i] + offset;
        ssResidual += Math.pow(actualTemps[i] - predicted, 2);
        ssTotal += Math.pow(actualTemps[i] - meanY, 2);
      }
      
      let r2 = 1 - (ssResidual / ssTotal);
      
      // Validate calculated values
      if (isNaN(slope) || isNaN(offset) || !isFinite(slope) || !isFinite(offset)) {
        document.getElementById('calStatus').style.color = 'red';
        document.getElementById('calStatus').textContent = 'Error: Could not calculate calibration. Check your data points.';
        return;
      }
      
      // Display results
      document.getElementById('calResults').style.display = 'block';
      
      let sign = offset >= 0 ? '+' : '';
      document.getElementById('calEquation').textContent = 
        'Temp_calibrated = ' + slope.toFixed(4) + ' × Temp_sensor ' + sign + ' ' + offset.toFixed(4);
      
      document.getElementById('calValues').textContent = 
        'Slope (m) = ' + slope.toFixed(4) + ',  Offset (c) = ' + offset.toFixed(4);
      
      document.getElementById('calR2').textContent = 
        'R² (goodness of fit) = ' + r2.toFixed(4) + 
        (r2 > 0.95 ? ' (Excellent fit)' : r2 > 0.85 ? ' (Good fit)' : ' (Fair fit)');
      
      // Send to ESP8266
      sendCalibrationToESP(slope, offset);
      
      document.getElementById('calStatus').style.color = 'green';
      document.getElementById('calStatus').textContent = 'Calibration applied successfully!';
      
      setTimeout(function() {
        document.getElementById('calStatus').textContent = '';
      }, 5000);
    }
    
    function resetCalibration() {
      if (confirm('Reset to default calibration (m=1.025, c=-8.155)?')) {
        sendCalibrationToESP(DEFAULT_SLOPE, DEFAULT_OFFSET);
        clearCalPoints();
        
        document.getElementById('calResults').style.display = 'block';
        document.getElementById('calEquation').textContent = 
          'Temp_calibrated = ' + DEFAULT_SLOPE + ' × Temp_sensor - ' + Math.abs(DEFAULT_OFFSET);
        document.getElementById('calValues').textContent = 
          'Slope (m) = ' + DEFAULT_SLOPE + ',  Offset (c) = ' + DEFAULT_OFFSET;
        document.getElementById('calR2').textContent = 
          'Default calibration restored';
        
        document.getElementById('calStatus').style.color = 'green';
        document.getElementById('calStatus').textContent = 'Calibration reset to default values!';
        
        setTimeout(function() {
          document.getElementById('calStatus').textContent = '';
        }, 3000);
      }
    }
    
    // Initialize calibration on page load
    initCalibration();
  </script>
  )rawliteral";
    
  page += "</body>";
  page += "</html>";
  
  server.send(200, "text/html", page); 
}

// Form input for desired temperature
void storeTemp() {
  if (server.hasArg("temp")) {
    String tempInput = server.arg("temp");  // Get user input as a String

    // Check if the input is a valid number and falls within the 0-500 range
    float tempValue = tempInput.toFloat();
    if (tempValue >= 0 && tempValue <= 500) {
      desiredTemp = (uint16_t)roundf(tempValue);  // Save the valid input as desiredTemp 
      input = true;
      isValid = true;
    }
    else {
      isValid = false;
    }
  }

  server.sendHeader("Location", "/"); // Redirect to the main page
  server.send(303); 
}

void getTemperature() {
  server.send(200, "text/plain", String(currentTemp));
}

void getDesiredTemperature() {
  server.send(200, "text/plain", String(desiredTemp));
}

void getRelayStatus() {
  server.send(200, "text/plain", relayStatus ? "true" : "false");
}

void setCalibration() {
  if (server.hasArg("slope") && server.hasArg("offset")) {
    float slope = server.arg("slope").toFloat();
    float offset = server.arg("offset").toFloat();
    
    // Validate ranges
    if (slope >= -2.0 && slope <= 2.0 && offset >= -50.0 && offset <= 50.0) {
      calSlope = slope;
      calOffset = offset;
      server.send(200, "text/plain", "OK");
      Serial.print("Calibration updated: m=");
      Serial.print(calSlope);
      Serial.print(", c=");
      Serial.println(calOffset);
    } else {
      server.send(400, "text/plain", "Invalid range");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void getCalibration() {
  String response = String(calSlope) + "," + String(calOffset);
  server.send(200, "text/plain", response);
}

void setHysteresis() {
  if (server.hasArg("value")) {
    uint16_t value = server.arg("value").toInt();
    
    // Validate range 1-20
    if (value >= 1 && value <= 20) {
      hysteresis = value;
      server.send(200, "text/plain", "OK");
      Serial.print("Hysteresis updated: ");
      Serial.println(hysteresis);
    } else {
      server.send(400, "text/plain", "Invalid");
    }
  } else {
    server.send(400, "text/plain", "Missing parameter");
  }
}

void getHysteresis() {
  server.send(200, "text/plain", String(hysteresis));
}

void setRelayStatus() {
  if (currentTemp < (desiredTemp - hysteresis)) { 
    relayStatus = true;
    digitalWrite(RELAYPIN, HIGH); 
  }
  else if (currentTemp > (desiredTemp + hysteresis)) { 
    relayStatus = false; 
    digitalWrite(RELAYPIN, LOW);
  }
  //Serial.println(relayStatus);
}

void setup(void) {
  // Keep thermistor digital pin (D1) high
  pinMode(DIGITALPIN, OUTPUT); 
  digitalWrite(DIGITALPIN, HIGH);  

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Turn OFF (inverted logic)

  pinMode(RELAYPIN, OUTPUT); 
  digitalWrite(RELAYPIN, LOW);  

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.begin(9600); // baud rate
  // Wait until wifi is connected
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(1000);
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // Set up server routes
  server.on("/", mainPage);
  server.on("/setTemp", storeTemp);
  server.on("/getTemp", getTemperature);
  server.on("/getDesiredTemp", getDesiredTemperature);
  server.on("/getRelayStatus", getRelayStatus);
  server.on("/setCalibration", setCalibration);
  server.on("/getCalibration", getCalibration);
  server.on("/setHysteresis", setHysteresis);
  server.on("/getHysteresis", getHysteresis);

  server.begin();
  Serial.println("Webpage is running.");
  digitalWrite(LED_PIN, HIGH);
}

void toggleLED() {
  ledState = !ledState;  // Flip the state
  digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // Active LOW LED
}

void loop(void) {
  MDNS.update();
  server.handleClient();
  currentTemp = calculateTemperature();
  setRelayStatus();
  loopCounter++;
  if (loopCounter > toggleInterval) {
    toggleLED();
    loopCounter = 0;
  }
  // delay(1000); //delay doesnt work well, check online, there is a fix.. 
}
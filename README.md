Code for the world's first IoT-Based Freevalve Control System, built around Wesley Kagan's original solenoid actuation logic. You may visit his repository to view his original main program along with the codes for the cam mapping tool and valve height calibration, both of which I have used to help create my program as well: https://github.com/Tamaren1/Freevalve_Arduino

Essentially I just refined his code, added three hall effect sensors as position sensors and an oxygen sensor for AFR, and created a web server that sends real-time engine data from my ESP32 MCU to a Google Sheets document I created to monitor my telemetry. For your microcontroller to access the internet, just replace YourWiFiName and YourWiFiPassword with your network SSID and network password. Meanwhile, here are the steps to take in order to harness the cloud feature of the Progressive Web-Based Application:

1. Create a Google Sheets document and label the column headers with the vital engine parameters. Also label the tab.
2. Go to Extensions and select Apps Script.
3. Open a new project and type in this code (edit YourTabName to reflect the name of your tab):

function doGet(e) {

var sheet =
SpreadsheetApp
.getActiveSpreadsheet()
.getSheetByName("YourTabName");

sheet.appendRow([
new Date(),
e.parameter.rpm,
e.parameter.torque,
e.parameter.power,
e.parameter.afr,
e.parameter.intake,
e.parameter.exhaust
]);

return ContentService.createTextOutput("OK");
}

4. Select Deploy and click New Deployment to execute everything.
5. Watch the Google Sheet get slowly filled every time the serial monitor in Arduino IDE notes sent telemetry from the Web Dashboard.

#include <WiFi.h>
#include <HTTPClient.h>

/* ===============================
WIFI CONFIG
================================ */
char ssid[] = "YourWiFiName";
char pass[] = "YourWiFiPassword";

/* ===============================
PIN DEFINITIONS
================================ */
#define HALL_PIN 34
#define OXYGEN_PIN 35
#define INTAKE_HALL_PIN 32
#define EXHAUST_HALL_PIN 33
#define INTAKE_PIN 25
#define EXHAUST_PIN 26

WiFiServer server(80);

String googleScript =
"https://script.google.com/macros/s/AKfycbzCeph9GAX7xP4e_JRebJVeBhgRapqxuXBoQlFJDwKKv-UleiFf5aPInkCuvFadE8G1aw/exec";

unsigned long lastTelemetry = 0;
const unsigned long telemetryInterval = 5000;

/* ===============================
SESSION LOCK
One browser tab at a time.
The active session is identified by a random token
generated when the first client loads the page.
/ping?token=XXX must arrive within 4 s or the
session is considered dead and the next visitor
may claim a new one.
================================ */
String activeToken = "";
unsigned long lastPing = 0;
const unsigned long PING_TIMEOUT_MS = 4000;

String generateToken() {
String t = "";
for (int i = 0; i < 16; i++) {
t += String(random(0, 16), HEX);
}
return t;
}

bool sessionAlive() {
if (activeToken == "") return false;
return (millis() - lastPing) < PING_TIMEOUT_MS;
}

/* ===============================
ENGINE STATE
================================ */
float rpm = 0;
float crankFiltered = 0;

unsigned long lastTime = 0;
bool hallHigh = false;

const int hallThreshold = 2000;
const int MAGNETS = 59;

/* ===== BASE TIMING VARIABLES ===== */
int hallcounter = 0;
unsigned int degree = 0;
unsigned int state = 0;
unsigned long lastGap = 0;

/* ===============================
ENGINE MEASUREMENTS
================================ */

float torqueNm = 0.0;
float powerHP = 0.0;
float omega = 0.0;

/* ===============================
AFR
================================ */
float afr = 14.7;
float oxygenVoltage = 0.0;
const float AFR_MIN = 10.2;
const float AFR_MAX = 20.0;

/* ===============================
VALVES
================================ */
float intakeOpenTimeMs = 10.7;
float exhaustOpenTimeMs = 10.7;
float intakeLift = 0.0;
float exhaustLift = 0.0;

/* ===============================
FILTERS / SENSOR CALIBRATION
================================ */
float intakeFiltered = 0.0;
float exhaustFiltered = 0.0;
const float alpha = 0.2;

// Calibrate these endpoints against the actual installed valve-position sensors.
// These defaults map ADC 0..4095 to 0..12 mm; they are not a substitute for calibration.
const float LIFT_ADC_MIN = 0.0;
const float LIFT_ADC_MAX = 4095.0;
const float LIFT_MM_MIN = 0.0;
const float LIFT_MM_MAX = 12.0;

bool engineOff = true;
const float GOV_LIMIT = 3520.0;
unsigned long lastValidCrankPulse = 0;
const unsigned long ENGINE_TIMEOUT_US = 500000UL;

void triggerThrottle(int level);
String getThrottleStatus();
void updateSensorTelemetry();

/* ===============================
SERIAL ENGINE CONTROL STATUS
================================ */
void triggerThrottle(int level) {
// This command is only a software status/diagnostic control. It does not
// physically control the engine throttle or simulate engine speed.
if (level <= 0) {
engineOff = true;
digitalWrite(INTAKE_PIN, LOW);
digitalWrite(EXHAUST_PIN, LOW);
Serial.println("Engine marked OFF; actuators de-energized.");
return;
}
engineOff = false;
Serial.println("Diagnostic command received. RPM remains sensor-measured.");
}

/* ===============================
THROTTLE STATUS
================================ */
String getThrottleStatus() {
if (engineOff || rpm <= 0.0) return "ENGINE OFF";
return "RUNNING - SENSOR DATA";
}

/* ===============================
CSS / HTML LOGO
Recreated entirely in code.
================================ */

String logoHTML() {

return R"rawliteral(
<div class="freevalve-logo">

<div class="logo-circuit">

<span class="node n1"></span>
<span class="node n2"></span>
<span class="node n3"></span>
<span class="node n4"></span>

<span class="line l1"></span>
<span class="line l2"></span>
<span class="line l3"></span>

</div>

<div class="logo-text">

<div class="logo-top">
THE WORLD'S FIRST
</div>

<div class="logo-main">
IOT-BASED FREEVALVE
</div>

<div class="logo-bottom">
CONTROL SYSTEM
</div>

</div>

</div>
)rawliteral";
}

/* ===============================
LOCKED PAGE
================================ */
String lockedPage() {

String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Access Denied</title>

<style>

*, *::before, *::after {
box-sizing: border-box;
margin: 0;
padding: 0;
}

body {

background: #080c10;

color: #c8d8e8;

font-family:
'Segoe UI',
Arial,
sans-serif;

display: flex;

flex-direction: column;

align-items: center;

justify-content: center;

min-height: 100vh;

gap: 24px;

padding: 20px;

text-align: center;
}

/* ===============================
FREEVALVE CODE-BASED LOGO
================================ */

.freevalve-logo {

position: relative;

display: flex;

flex-direction: column;

align-items: center;

width: min(760px, 94vw);

padding: 10px 0 4px;

filter:
drop-shadow(
0 0 10px
rgba(204,0,0,0.35)
);
}

.logo-circuit {

position: absolute;

width: 100%;

height: 105px;

left: 0;

top: 0;

pointer-events: none;
}

.logo-circuit .line {

position: absolute;

height: 4px;

background: #cc0000;

box-shadow:
0 0 8px rgba(204,0,0,0.8);
}

.logo-circuit .l1 {

width: 80px;

left: 3%;

top: 29px;

transform: rotate(-32deg);
}

.logo-circuit .l2 {

width: 72px;

right: 3%;

top: 29px;

transform: rotate(32deg);
}

.logo-circuit .l3 {

width: 70px;

left: 50%;

top: 7px;

transform:
translateX(-50%)
rotate(90deg);
}

.logo-circuit .node {

position: absolute;

width: 13px;

height: 13px;

border:
3px solid #cc0000;

border-radius: 50%;

background: #080c10;

box-shadow:
0 0 9px rgba(204,0,0,0.8);
}

.logo-circuit .n1 {

left: 1.8%;

top: 7px;
}

.logo-circuit .n2 {

left: 50%;

top: -2px;

transform: translateX(-50%);
}

.logo-circuit .n3 {

right: 1.8%;

top: 7px;
}

.logo-circuit .n4 {

left: 50%;

bottom: 4px;

transform: translateX(-50%);
}

.logo-text {

position: relative;

z-index: 2;

text-align: center;

line-height: 1;

padding-top: 13px;
}

.logo-top {

font-size:
clamp(12px, 2vw, 19px);

font-weight: 900;

letter-spacing:
clamp(2px, 0.7vw, 6px);

color: #f2f2f2;

text-shadow:
0 0 8px rgba(255,255,255,0.25);

margin-bottom: 7px;
}

.logo-main {

font-size:
clamp(24px, 5vw, 48px);

font-weight: 950;

letter-spacing:
clamp(1px, 0.25vw, 3px);

color: #cc0000;

text-shadow:
2px 2px 0 #450000,
0 0 12px rgba(204,0,0,0.45);

transform:
skewX(-4deg);

margin-bottom: 5px;
}

.logo-bottom {

font-size:
clamp(19px, 4vw, 40px);

font-weight: 950;

letter-spacing:
clamp(1px, 0.3vw, 4px);

color: #f2f2f2;

text-shadow:
2px 2px 0 #333;

transform:
skewX(-4deg);
}

.box {

background: #0d1117;

border:
1px solid #1e2a35;

border-top:
3px solid #cc0000;

border-radius: 8px;

padding: 32px 40px;

max-width: 420px;

box-shadow:
0 0 30px
rgba(204,0,0,0.15);
}

h1 {

font-size: 15px;

letter-spacing: 4px;

color: #cc0000;

text-transform: uppercase;

margin-bottom: 14px;
}

p {

font-size: 13px;

color: #5a7080;

line-height: 1.7;

letter-spacing: 0.5px;
}

.dot {

width: 12px;

height: 12px;

background: #cc0000;

border-radius: 50%;

display: inline-block;

margin-right: 8px;

animation:
blink 1s infinite;

vertical-align: middle;
}

@keyframes blink {

0%,100% {
opacity: 1;
}

50% {
opacity: 0.2;
}

}

.retry {

margin-top: 20px;

font-size: 12px;

color: #2a4050;
}

</style>

</head>

<body>

)rawliteral";

html += logoHTML();

html += R"rawliteral(

<div class="box">

<h1>
<span class="dot"></span>
Session Active on Another Device
</h1>

<p>

The Freevalve Dashboard is
currently open on another device.

<br><br>

Close that tab or browser session
first, then reload this page to
claim access.

</p>

<div class="retry">

This page checks again automatically
every 5 seconds.

</div>

</div>

<script>

setTimeout(
() => location.reload(),
5000
);

</script>

</body>

</html>

)rawliteral";

return html;
}

/* ===============================
PROGRESSIVE WEB-BASED APP
================================ */
String page(String token) {

String p = R"rawliteral(
<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Freevalve Dashboard</title>

<style>

*, *::before, *::after {
box-sizing: border-box;
margin: 0;
padding: 0;
}

:root {

--red: #cc0000;
--red2: #ff2020;
--green: #00ff88;
--cyan: #00e5ff;
--bg: #080c10;
--panel: #0d1117;
--border: #1e2a35;
--text: #c8d8e8;
--dim: #5a7080;

}

body {

background: var(--bg);

color: var(--text);

font-family:
'Segoe UI',
'Arial',
sans-serif;

min-height: 100vh;

display: flex;

flex-direction: column;

align-items: center;

padding: 0 0 40px;
}

/* ===============================
HEADER
================================ */

.header {

width: 100%;

background:
linear-gradient(
180deg,
#0a0a0a 0%,
#100808 100%
);

border-bottom:
2px solid var(--red);

padding:
18px 24px 18px;

display: flex;

flex-direction: column;

align-items: center;

gap: 10px;

box-shadow:
0 4px 24px
rgba(204,0,0,0.25);
}

/* ===============================
CODE-BASED LOGO
================================ */

.freevalve-logo {

position: relative;

display: flex;

flex-direction: column;

align-items: center;

width: min(860px, 96vw);

min-height: 105px;

padding:
10px 0 4px;

filter:
drop-shadow(
0 0 12px
rgba(204,0,0,0.45)
);
}

.logo-circuit {

position: absolute;

width: 100%;

height: 105px;

left: 0;

top: 0;

pointer-events: none;
}

.logo-circuit .line {

position: absolute;

height: 4px;

background: var(--red);

box-shadow:
0 0 9px
rgba(204,0,0,0.85);
}

.logo-circuit .l1 {

width: 100px;

left: 2%;

top: 31px;

transform: rotate(-32deg);
}

.logo-circuit .l2 {

width: 100px;

right: 2%;

top: 31px;

transform: rotate(32deg);
}

.logo-circuit .l3 {

width: 82px;

left: 50%;

top: 8px;

transform:
translateX(-50%)
rotate(90deg);
}

.logo-circuit .node {

position: absolute;

width: 15px;

height: 15px;

border:
3px solid var(--red);

border-radius: 50%;

background: var(--bg);

box-shadow:
0 0 10px
rgba(204,0,0,0.9);
}

.logo-circuit .n1 {

left: 0.5%;

top: 7px;
}

.logo-circuit .n2 {

left: 50%;

top: -3px;

transform: translateX(-50%);
}

.logo-circuit .n3 {

right: 0.5%;

top: 7px;
}

.logo-circuit .n4 {

left: 50%;

bottom: 2px;

transform: translateX(-50%);
}

.logo-text {

position: relative;

z-index: 2;

text-align: center;

line-height: 1;

padding-top: 12px;
}

.logo-top {

font-size:
clamp(11px, 1.6vw, 17px);

font-weight: 900;

letter-spacing:
clamp(2px, 0.55vw, 5px);

color: #f2f2f2;

text-shadow:
0 0 8px
rgba(255,255,255,0.25);

margin-bottom: 7px;
}

.logo-main {

font-size:
clamp(25px, 4.6vw, 50px);

font-weight: 950;

letter-spacing:
clamp(1px, 0.2vw, 3px);

color: var(--red);

text-shadow:
2px 2px 0 #450000,
0 0 14px
rgba(204,0,0,0.5);

transform: skewX(-4deg);

margin-bottom: 5px;
}

.logo-bottom {

font-size:
clamp(20px, 3.7vw, 42px);

font-weight: 950;

letter-spacing:
clamp(1px, 0.3vw, 4px);

color: #f2f2f2;

text-shadow:
2px 2px 0 #333;

transform: skewX(-4deg);
}

.header-sub {

font-size: 11px;

letter-spacing: 4px;

color: var(--dim);

text-transform: uppercase;

text-align: center;
}

/* ===============================
CONTENT
================================ */

.content {

width: 100%;

max-width: 900px;

padding:
20px 16px 0;
}

.section-title {

font-size: 11px;

letter-spacing: 3px;

color: var(--dim);

text-transform: uppercase;

border-bottom:
1px solid var(--border);

padding-bottom: 6px;

margin: 24px 0 14px;
}

/* ===============================
GAUGES
================================ */

.gauge-grid {

display: grid;

grid-template-columns:
repeat(
auto-fit,
minmax(130px, 1fr)
);

gap: 10px;
}

.gauge {

background: var(--panel);

border:
1px solid var(--border);

border-top:
3px solid var(--red);

border-radius: 6px;

padding:
14px 10px 12px;

display: flex;

flex-direction: column;

align-items: center;

gap: 6px;

box-shadow:
inset 0 0 20px
rgba(0,0,0,0.4);

position: relative;
}

.gauge::before {

content: '';

position: absolute;

top: 0;
left: 0;
right: 0;

height: 3px;

background:
linear-gradient(
90deg,
var(--red2),
transparent
);

border-radius:
6px 6px 0 0;
}

.gauge-label {

font-size: 10px;

letter-spacing: 2px;

color: var(--dim);

text-transform: uppercase;
}

.gauge-value {

font-size: 28px;

font-weight: 700;

color: var(--green);

font-family:
'Courier New',
monospace;

text-shadow:
0 0 12px
rgba(0,255,136,0.5);

line-height: 1;
}

.gauge-unit {

font-size: 10px;

color: var(--dim);

letter-spacing: 1px;
}

/* ===============================
STATUS
================================ */

.status-bar {

background: var(--panel);

border:
1px solid var(--border);

border-left:
4px solid var(--red);

border-radius: 4px;

padding: 8px 16px;

font-size: 12px;

letter-spacing: 2px;

color: var(--red2);

text-transform: uppercase;

margin-bottom: 4px;

min-height: 34px;
}

/* ===============================
VALVE CONTROLS
================================ */

.valve-grid {

display: grid;

grid-template-columns:
1fr 1fr;

gap: 12px;
}

.valve-block {

background: var(--panel);

border:
1px solid var(--border);

border-radius: 6px;

padding:
16px 12px;

display: flex;

flex-direction: column;

align-items: center;

gap: 12px;
}

.valve-block-title {

font-size: 10px;

letter-spacing: 2px;

color: var(--dim);

text-transform: uppercase;
}

.pm-control {

display: flex;

align-items: center;

gap: 14px;
}

.pm-btn {

width: 38px;

height: 38px;

border-radius: 50%;

border:
2px solid var(--red);

background: #160404;

color: var(--red2);

font-size: 22px;

font-weight: 700;

cursor: pointer;

display: flex;

align-items: center;

justify-content: center;

transition:
background 0.15s,
box-shadow 0.15s;

line-height: 1;

user-select: none;
}

.pm-btn:active {

background: #2a0606;

box-shadow:
0 0 12px
rgba(204,0,0,0.6);
}

.pm-display {

min-width: 72px;

text-align: center;

font-family:
'Courier New',
monospace;

font-size: 20px;

font-weight: 700;

color: var(--green);

background: #040a0a;

border:
1px solid var(--border);

border-radius: 4px;

padding: 6px 10px;

text-shadow:
0 0 8px
rgba(0,255,136,0.4);
}

.pm-unit {

font-size: 10px;

color: var(--dim);

letter-spacing: 1px;
}

/* ===============================
GRAPH
================================ */

.graph-wrap {

position: relative;

width: 100%;

padding-left: 48px;

padding-bottom: 36px;
}

#graph {

width: 100%;

height: 260px;

background: #020608;

border:
1px solid var(--border);

border-radius: 4px;

display: block;
}

.y-axis-label {

position: absolute;

left: 0;

top: 50%;

transform:
translateY(-50%)
rotate(-90deg);

font-size: 10px;

letter-spacing: 2px;

color: var(--dim);

text-transform: uppercase;

white-space: nowrap;
}

.x-axis-label {

position: absolute;

bottom: 4px;

left: 50%;

transform:
translateX(-50%);

font-size: 10px;

letter-spacing: 2px;

color: var(--dim);

text-transform: uppercase;
}

.graph-legend {

display: flex;

justify-content: center;

gap: 20px;

margin-top: 8px;
}

.legend-item {

display: flex;

align-items: center;

gap: 6px;

font-size: 11px;

color: var(--dim);
}

.legend-dot {

width: 10px;

height: 10px;

border-radius: 50%;
}

/* ===============================
SESSION EXPIRED
================================ */

#expired-overlay {

display: none;

position: fixed;

inset: 0;

background:
rgba(8,12,16,0.96);

z-index: 99999;

flex-direction: column;

align-items: center;

justify-content: center;

gap: 20px;

text-align: center;

padding: 20px;
}

#expired-overlay.show {

display: flex;
}

.exp-box {

background: #0d1117;

border:
1px solid #1e2a35;

border-top:
3px solid #cc0000;

border-radius: 8px;

padding:
32px 40px;

max-width: 400px;
}

.exp-box h1 {

font-size: 13px;

letter-spacing: 3px;

color: #cc0000;

text-transform: uppercase;

margin-bottom: 12px;
}

.exp-box p {

font-size: 12px;

color: #5a7080;

line-height: 1.8;
}

/* ===============================
TOAST
================================ */

#toast {

position: fixed;

top: 20px;

right: 20px;

background: var(--green);

color: #000;

padding:
12px 20px;

border-radius: 6px;

font-weight: 700;

font-size: 13px;

letter-spacing: 1px;

box-shadow:
0 0 20px
rgba(0,255,136,0.6);

opacity: 0;

transform:
translateY(-16px);

transition:
opacity 0.3s ease,
transform 0.3s ease;

z-index: 9999;

pointer-events: none;
}

#toast.show {

opacity: 1;

transform:
translateY(0);
}

/* ===============================
MOBILE
================================ */

@media (max-width: 500px) {

.valve-grid {

grid-template-columns: 1fr;
}

.gauge-value {

font-size: 22px;
}

.logo-circuit .line {

height: 3px;
}

.logo-circuit .node {

width: 11px;

height: 11px;

border-width: 2px;
}

}

</style>

</head>

<body>

<div class="header">

)rawliteral";

p += logoHTML();

p += R"rawliteral(

<div class="header-sub">

Progressive Web-Based Application
&nbsp;|&nbsp;
Bravus BR220-ND

</div>

</div>

<div class="content">

<div class="section-title">
Live Engine Data
</div>

<div class="gauge-grid">

<div class="gauge">

<div class="gauge-label">
RPM
</div>

<div class="gauge-value" id="rpm">
0
</div>

<div class="gauge-unit">
rev/min
</div>

</div>

<div class="gauge">

<div class="gauge-label">
Torque
</div>

<div class="gauge-value" id="torque">
0.0
</div>

<div class="gauge-unit">
Nm
</div>

</div>

<div class="gauge">

<div class="gauge-label">
Power
</div>

<div class="gauge-value" id="power">
0.00
</div>

<div class="gauge-unit">
HP
</div>

</div>

<div class="gauge">

<div class="gauge-label">
AFR
</div>

<div class="gauge-value" id="afr">
0.0
</div>

<div class="gauge-unit">
air/fuel
</div>

</div>

<div class="gauge">

<div class="gauge-label">
Intake Lift
</div>

<div class="gauge-value" id="in">
0.0
</div>

<div class="gauge-unit">
mm
</div>

</div>

<div class="gauge">

<div class="gauge-label">
Exhaust Lift
</div>

<div class="gauge-value" id="ex">
0.0
</div>

<div class="gauge-unit">
mm
</div>

</div>

</div>

<div class="section-title">
Engine Status
</div>

<div class="status-bar" id="status">
IDLE
</div>

<div class="section-title">
Valve Open Time Control
</div>

<div class="valve-grid">

<div class="valve-block">

<div class="valve-block-title">
Intake Open Time
</div>

<div class="pm-control">

<button
class="pm-btn"
onclick="adjustValve('intake',-0.1)">
&#8722;
</button>

<div
class="pm-display"
id="inDisplay">
10.7
</div>

<button
class="pm-btn"
onclick="adjustValve('intake', 0.1)">
&#43;
</button>

</div>

<div class="pm-unit">
milliseconds
</div>

</div>

<div class="valve-block">

<div class="valve-block-title">
Exhaust Open Time
</div>

<div class="pm-control">

<button
class="pm-btn"
onclick="adjustValve('exhaust',-0.1)">
&#8722;
</button>

<div
class="pm-display"
id="exDisplay">
10.7
</div>

<button
class="pm-btn"
onclick="adjustValve('exhaust', 0.1)">
&#43;
</button>

</div>

<div class="pm-unit">
milliseconds
</div>

</div>

</div>

<div class="section-title">
Performance Graph
</div>

<div class="graph-wrap">

<div class="y-axis-label">
Values
</div>

<canvas id="graph"></canvas>

<div class="x-axis-label">
Time (live clock)
</div>

</div>

<div class="graph-legend">

<div class="legend-item">

<div
class="legend-dot"
style="background:#00ff00">
</div>

RPM

</div>

<div class="legend-item">

<div
class="legend-dot"
style="background:#00e5ff">
</div>

Torque (Nm)

</div>

<div class="legend-item">

<div
class="legend-dot"
style="background:#ff2020">
</div>

Power (HP)

</div>

</div>

</div>

<!-- SESSION EXPIRED -->

<div id="expired-overlay">

)rawliteral";

p += logoHTML();

p += R"rawliteral(

<div class="exp-box">

<h1>
Session Expired
</h1>

<p>

Your session was claimed by another device,
<br>

or your tab was inactive too long.

<br><br>

Reload this page to reclaim access.

</p>

</div>

</div>

<div id="toast"></div>

<script>

/* ══════════════════════════════════
SESSION TOKEN
══════════════════════════════════ */

const SESSION_TOKEN = ")rawliteral";

p += token;

p += R"rawliteral(";

/* ══════════════════════════════════
HEARTBEAT PING
══════════════════════════════════ */

let sessionDead = false;

function sendPing() {

if (sessionDead)
return;

fetch(
'/ping?token=' +
SESSION_TOKEN
)

.then(r => r.json())

.then(d => {

if (!d.ok) {

sessionDead = true;

document
.getElementById(
"expired-overlay"
)
.classList
.add("show");
}

})

.catch(() => {});
}

setInterval(sendPing, 2000);

sendPing();

/* ══════════════════════════════════
RELEASE SESSION
══════════════════════════════════ */

window.addEventListener(
'beforeunload',
() => {

navigator.sendBeacon(
'/release?token=' +
SESSION_TOKEN
);

}
);

/* ══════════════════════════════════
DATA STORAGE
══════════════════════════════════ */

let rpmData = [];
let torqueData = [];
let powerData = [];
let timeLabels = [];

const maxPoints = 60;

/* ══════════════════════════════════
VALVE STATE
══════════════════════════════════ */

let intakeMs = 10.7;
let exhaustMs = 10.7;

const MS_MIN = 10.7;
const MS_MAX = 26.0;

/* ══════════════════════════════════
TOAST
══════════════════════════════════ */

let toastTimer = null;

function showToast(msg) {

let t =
document.getElementById("toast");

t.innerText = msg;

t.classList.add("show");

if (toastTimer)
clearTimeout(toastTimer);

toastTimer =
setTimeout(
() =>
t.classList.remove("show"),
2000
);
}

/* ══════════════════════════════════
PLUS / MINUS VALVE CONTROLS
══════════════════════════════════ */

function adjustValve(type, delta) {

if (type === "intake") {

intakeMs =
Math.round(
(intakeMs + delta) * 10
) / 10;

if (intakeMs < MS_MIN)
intakeMs = MS_MIN;

if (intakeMs > MS_MAX)
intakeMs = MS_MAX;

document
.getElementById("inDisplay")
.innerText =
intakeMs.toFixed(1);

fetch(
'/set?intake=' +
intakeMs +
'&token=' +
SESSION_TOKEN
)

.then(() =>
showToast(
"Saved — Intake " +
intakeMs.toFixed(1) +
" ms"
)
)

.catch(() =>
showToast("Save failed")
);

}

else {

exhaustMs =
Math.round(
(exhaustMs + delta) * 10
) / 10;

if (exhaustMs < MS_MIN)
exhaustMs = MS_MIN;

if (exhaustMs > MS_MAX)
exhaustMs = MS_MAX;

document
.getElementById("exDisplay")
.innerText =
exhaustMs.toFixed(1);

fetch(
'/set?exhaust=' +
exhaustMs +
'&token=' +
SESSION_TOKEN
)

.then(() =>
showToast(
"Saved — Exhaust " +
exhaustMs.toFixed(1) +
" ms"
)
)

.catch(() =>
showToast("Save failed")
);
}
}

/* ══════════════════════════════════
GRAPH
══════════════════════════════════ */

function drawGraph() {

const canvas =
document.getElementById("graph");

const W = canvas.width;
const H = canvas.height;

const ctx =
canvas.getContext("2d");

ctx.clearRect(
0, 0, W, H
);

const bg =
ctx.createLinearGradient(
0, 0, 0, H
);

bg.addColorStop(
0,
"#020c10"
);

bg.addColorStop(
1,
"#000204"
);

ctx.fillStyle = bg;

ctx.fillRect(
0, 0, W, H
);

const PAD_L = 52;
const PAD_R = 16;
const PAD_T = 20;
const PAD_B = 40;

const gW =
W - PAD_L - PAD_R;

const gH =
H - PAD_T - PAD_B;

/* Y GRID + LABELS */

ctx.font =
"10px 'Courier New'";

ctx.textAlign = "right";

for (let i = 0; i <= 5; i++) {

const y =
PAD_T +
(i / 5) * gH;

const val =
Math.round(
(1 - i / 5) * 3600
);

ctx.strokeStyle =
"#111d25";

ctx.lineWidth = 1;

ctx.beginPath();

ctx.moveTo(
PAD_L,
y
);

ctx.lineTo(
PAD_L + gW,
y
);

ctx.stroke();

ctx.fillStyle =
"#4a6070";

ctx.fillText(
val,
PAD_L - 6,
y + 4
);
}

/* X TIME LABELS */

ctx.textAlign = "center";

ctx.fillStyle =
"#4a6070";

const step =
Math.max(
1,
Math.floor(
timeLabels.length / 6
)
);

for (
let i = 0;
i < timeLabels.length;
i += step
) {

const x =
PAD_L +
(i / (maxPoints - 1)) *
gW;

ctx.fillText(
timeLabels[i],
x,
H - PAD_B + 16
);
}

/* AXIS BORDER */

ctx.strokeStyle =
"#1e2a35";

ctx.lineWidth = 1;

ctx.strokeRect(
PAD_L,
PAD_T,
gW,
gH
);

function plot(
data,
color,
minV,
maxV
) {

if (data.length < 2)
return;

ctx.beginPath();

ctx.strokeStyle = color;

ctx.lineWidth = 2.5;

ctx.shadowColor = color;

ctx.shadowBlur = 6;

for (
let i = 0;
i < data.length;
i++
) {

let n =
(data[i] - minV) /
(maxV - minV);

n =
Math.min(
1,
Math.max(0, n)
);

const x =
PAD_L +
(i / (maxPoints - 1)) *
gW;

const y =
PAD_T +
(1 - n) *
gH;

if (i === 0)
ctx.moveTo(x, y);
else
ctx.lineTo(x, y);
}

ctx.stroke();

ctx.shadowBlur = 0;
}

plot(
rpmData,
"#00ff00",
0,
3600
);

plot(
torqueData,
"#00e5ff",
0,
15
);

plot(
powerData,
"#ff2020",
0,
7.5
);
}

/* ══════════════════════════════════
CLOCK
══════════════════════════════════ */

function nowLabel() {

const d = new Date();

return (
String(
d.getHours()
).padStart(2,'0')
+
':'
+
String(
d.getMinutes()
).padStart(2,'0')
+
':'
+
String(
d.getSeconds()
).padStart(2,'0')
);
}

/* ══════════════════════════════════
RESIZE GRAPH
══════════════════════════════════ */

function resizeCanvas() {

const c =
document.getElementById(
"graph"
);

c.width =
c.offsetWidth;

c.height =
c.offsetHeight;
}

window.addEventListener(
'resize',
() => {

resizeCanvas();

drawGraph();

}
);

resizeCanvas();

/* ══════════════════════════════════
DATA UPDATE
══════════════════════════════════ */

function update() {

if (sessionDead)
return;

fetch(
'/data?token=' +
SESSION_TOKEN
)

.then(r => r.json())

.then(d => {

if (d.locked) {

sessionDead = true;

document
.getElementById(
"expired-overlay"
)
.classList
.add("show");

return;
}

document
.getElementById("rpm")
.innerText =
Math.round(d.rpm);

document
.getElementById("torque")
.innerText =
Number(d.torque)
.toFixed(1);

document
.getElementById("power")
.innerText =
Number(d.power)
.toFixed(2);

document
.getElementById("afr")
.innerText =
Number(d.afr)
.toFixed(1);

document
.getElementById("in")
.innerText =
Number(d.intake)
.toFixed(1);

document
.getElementById("ex")
.innerText =
Number(d.exhaust)
.toFixed(1);

rpmData.push(
Number(d.rpm)
);

torqueData.push(
Number(d.torque)
);

powerData.push(
Number(d.power)
);

timeLabels.push(
nowLabel()
);

if (
rpmData.length >
maxPoints
) {

rpmData.shift();

torqueData.shift();

powerData.shift();

timeLabels.shift();
}

drawGraph();

let parts = [];

if (
d.throttle &&
d.throttle !== "IDLE"
) {

parts.push(
d.throttle
);
}

if (
Number(d.rpm) === 0
) {

parts.push(
"ENGINE OFF"
);
}

if (
d.afr < 12 &&
Number(d.rpm) > 0
) {

parts.push(
"RICH"
);
}

if (
d.afr > 15 &&
Number(d.rpm) > 0
) {

parts.push(
"LEAN"
);
}

document
.getElementById("status")
.innerText =
parts.length
? parts.join(" | ")
: "IDLE";

})

.catch(() => {});
}

setInterval(
update,
300
);

</script>

</body>

</html>

)rawliteral";

return p;
}

/* ===============================
SEND TELEMETRY
================================ */
void sendTelemetry() {

if (WiFi.status() != WL_CONNECTED)
return;

HTTPClient http;

String url =
googleScript +
"?rpm=" +
String(rpm) +
"&torque=" +
String(torqueNm) +
"&power=" +
String(powerHP) +
"&afr=" +
String(afr) +
"&intake=" +
String(intakeLift) +
"&exhaust=" +
String(exhaustLift);

http.begin(url);

int httpCode =
http.GET();

Serial.print(
"Telemetry Sent. HTTP Code: "
);

Serial.println(
httpCode
);

http.end();
}

/* ===============================
SETUP
================================ */
void setup() {

Serial.begin(115200);

pinMode(
HALL_PIN,
INPUT
);

pinMode(
OXYGEN_PIN,
INPUT
);

pinMode(
INTAKE_HALL_PIN,
INPUT
);

pinMode(
EXHAUST_HALL_PIN,
INPUT
);

pinMode(
INTAKE_PIN,
OUTPUT
);

pinMode(
EXHAUST_PIN,
OUTPUT
);

WiFi.begin(
ssid,
pass
);

while (
WiFi.status() !=
WL_CONNECTED
) {

delay(500);

Serial.print(".");
}

Serial.println();

Serial.println(
"CONNECTED!"
);

Serial.print(
"IP Address: "
);

Serial.println(
WiFi.localIP()
);

server.begin();

Serial.println(
"Web Server Started"
);

Serial.println();

}

/* ===============================
LOOP
================================ */
void loop() {

/* ===== SERIAL INPUT ===== */

static String serialBuf = "";

while (Serial.available()) {

char c =
Serial.read();

if (
c == '\n' ||
c == '\r'
) {

if (
serialBuf.length() > 0
) {

serialBuf.trim();

int val =
serialBuf.toInt();

if (val >= 0)
triggerThrottle(val);

serialBuf = "";
}

} else {

serialBuf += c;
}
}

/* ===== UPDATE SENSOR TELEMETRY ===== */
updateSensorTelemetry();

/* ===== TELEMETRY ===== */

if (
millis() -
lastTelemetry >=
telemetryInterval
) {

lastTelemetry =
millis();

sendTelemetry();
}

/* ===== WEB SERVER ===== */

WiFiClient client =
server.available();

if (!client)
goto hall_section;

{

String req =
client.readStringUntil('\r');

client.readStringUntil('\n');

/* ===============================
QUERY PARAMETER HELPER
================================ */

auto getParam =
[&](String key) -> String {

String search =
key + "=";

int idx =
req.indexOf(search);

if (idx < 0)
return "";

int start =
idx +
search.length();

int end =
req.indexOf(
'&',
start
);

if (end < 0)
end =
req.indexOf(
' ',
start
);

if (end < 0)
return req.substring(
start
);

return req.substring(
start,
end
);
};

/* ===============================
/ping
================================ */

if (
req.indexOf("/ping") >= 0
) {

String tok =
getParam("token");

bool ok = false;

if (
activeToken == "" ||
!sessionAlive()
) {

activeToken = tok;

lastPing =
millis();

ok = true;

}

else if (
tok == activeToken
) {

lastPing =
millis();

ok = true;
}

client.println(
"HTTP/1.1 200 OK"
);

client.println(
"Content-Type: application/json"
);

client.println();

client.println(
ok
? "{\"ok\":true}"
: "{\"ok\":false}"
);
}

/* ===============================
/release
================================ */

else if (
req.indexOf("/release") >= 0
) {

String tok =
getParam("token");

if (
tok == activeToken
) {

activeToken = "";

lastPing = 0;

Serial.println(
"Session released by client."
);
}

client.println(
"HTTP/1.1 200 OK"
);

client.println();
}

/* ===============================
/data
================================ */

else if (
req.indexOf("/data") >= 0
) {

String tok =
getParam("token");

if (
sessionAlive() &&
tok != activeToken
) {

client.println(
"HTTP/1.1 200 OK"
);

client.println(
"Content-Type: application/json"
);

client.println();

client.println(
"{\"locked\":true}"
);

}

else {

String json = "{";

json +=
"\"rpm\":" +
String(rpm) +
",";

json +=
"\"torque\":" +
String(torqueNm, 1) +
",";

json +=
"\"power\":" +
String(powerHP, 2) +
",";

json +=
"\"afr\":" +
String(afr, 1) +
",";

json +=
"\"intake\":" +
String(intakeLift, 1) +
",";

json +=
"\"exhaust\":" +
String(exhaustLift, 1) +
",";

json +=
"\"throttle\":\"" +
getThrottleStatus() +
"\"";

json += "}";

client.println(
"HTTP/1.1 200 OK"
);

client.println(
"Content-Type: application/json"
);

client.println();

client.println(
json
);
}
}

/* ===============================
/set
================================ */

else if (
req.indexOf("/set") >= 0
) {

String tok =
getParam("token");

if (
!sessionAlive() ||
tok == activeToken
) {

if (
req.indexOf(
"intake="
) >= 0
) {

intakeOpenTimeMs =
getParam(
"intake"
).toFloat();
intakeOpenTimeMs = constrain(intakeOpenTimeMs, 10.7f, 26.0f);

}

if (
req.indexOf(
"exhaust="
) >= 0
) {

exhaustOpenTimeMs =
getParam(
"exhaust"
).toFloat();
exhaustOpenTimeMs = constrain(exhaustOpenTimeMs, 10.7f, 26.0f);

}

Serial.print("Valve open-time updated. Intake: ");
Serial.print(intakeOpenTimeMs, 1);
Serial.print(" ms; exhaust: ");
Serial.print(exhaustOpenTimeMs, 1);
Serial.println(" ms");
}

client.println(
"HTTP/1.1 200 OK"
);

client.println();
}

/* ===============================
GET /
================================ */

else {

client.println(
"HTTP/1.1 200 OK"
);

client.println(
"Content-Type: text/html"
);

client.println();

if (
sessionAlive()
) {

client.println(
lockedPage()
);

}

else {

activeToken =
generateToken();

lastPing =
millis();

Serial.print(
"New session started: "
);

Serial.println(
activeToken
);

client.println(
page(activeToken)
);
}
}

client.stop();
}

hall_section:

/* ===============================
CRANK HALL SENSOR / RPM / VALVE TIMING
================================ */
int raw = analogRead(HALL_PIN);
crankFiltered += alpha * (raw - crankFiltered);
bool currentState = (crankFiltered > hallThreshold);
unsigned long nowUs = micros();

if (currentState && !hallHigh) {
hallHigh = true;
unsigned long dt = (lastTime == 0) ? 0 : nowUs - lastTime;
lastTime = nowUs;
lastValidCrankPulse = nowUs;

if (dt > 0) {
// 59 physical tooth positions, with one missing tooth. RPM is estimated
// from the measured pulse interval and the configured tooth count.
float measuredRpm = 60000000.0f / ((float)dt * MAGNETS);
if (measuredRpm >= 0.0f && measuredRpm <= 12000.0f) rpm = measuredRpm;
}

hallcounter++;
unsigned long gap = dt;
bool missingTooth = (lastGap > 0 && gap > lastGap * 1.5f);
if (missingTooth) {
Serial.println("Crank missing-tooth gap detected");
hallcounter = 1;
state++;
}
if (!missingTooth && gap > 0) lastGap = gap;

degree = (hallcounter * 6U) % 360U;

if ((state % 2U) == 0U) {
digitalWrite(INTAKE_PIN, (degree >= 6 && degree <= 180) ? HIGH : LOW);
digitalWrite(EXHAUST_PIN, LOW);
} else {
digitalWrite(EXHAUST_PIN, (degree >= 180 && degree <= 354) ? HIGH : LOW);
digitalWrite(INTAKE_PIN, LOW);
}
}
if (!currentState) hallHigh = false;

/* ===============================
VALVE POSITION SENSORS
================================ */
int inRaw = analogRead(INTAKE_HALL_PIN);
int exRaw = analogRead(EXHAUST_HALL_PIN);
intakeFiltered += alpha * (inRaw - intakeFiltered);
exhaustFiltered += alpha * (exRaw - exhaustFiltered);

intakeLift = (intakeFiltered - LIFT_ADC_MIN) *
(LIFT_MM_MAX - LIFT_MM_MIN) / (LIFT_ADC_MAX - LIFT_ADC_MIN) + LIFT_MM_MIN;
exhaustLift = (exhaustFiltered - LIFT_ADC_MIN) *
(LIFT_MM_MAX - LIFT_MM_MIN) / (LIFT_ADC_MAX - LIFT_ADC_MIN) + LIFT_MM_MIN;
intakeLift = constrain(intakeLift, LIFT_MM_MIN, LIFT_MM_MAX);
exhaustLift = constrain(exhaustLift, LIFT_MM_MIN, LIFT_MM_MAX);

/* ===============================
OXYGEN SENSOR
================================ */
oxygenVoltage = analogRead(OXYGEN_PIN) * (3.3f / 4095.0f);

if (engineOff || oxygenVoltage < 0.1f) {
afr = 0.0f;
} else {
afr = 14.7f + (oxygenVoltage - 0.45f) * 10.0f;
afr = constrain(afr, AFR_MIN, AFR_MAX);
}

// ---- Gate valve position sensor readings on engine state ----
if (engineOff) {
intakeLift = 0.0f;
exhaustLift = 0.0f;
}

/* ===============================
ENGINE STOP TIMEOUT / TORQUE LIMITATION
================================ */
if (lastValidCrankPulse == 0 || (nowUs - lastValidCrankPulse) > ENGINE_TIMEOUT_US) {
rpm = 0.0f;
engineOff = true;
digitalWrite(INTAKE_PIN, LOW);
digitalWrite(EXHAUST_PIN, LOW);
} else {
engineOff = false;
}

torqueNm = 0.0f;
powerHP = 0.0f;
}

void updateSensorTelemetry() {
}

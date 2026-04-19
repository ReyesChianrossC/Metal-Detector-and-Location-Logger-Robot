/*
 * ESP32-CAM Direct Web Server
 * 
 * Serves HTML/CSS directly from ESP32-CAM and streams video via MJPEG
 * No external server needed - just connect to ESP32-CAM's IP address
 * 
 * Hardware: AI-Thinker ESP32-CAM with OV2640 camera
 */

#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <time.h>
#include "esp_camera.h"
#include "camera_pins.h"
 
#ifndef PI
#define PI 3.14159265358979323846
#endif

// ===================== PHILIPPINE TIME (NTP) =====================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;  // UTC+8 for Philippines
const int daylightOffset_sec = 0;     // No daylight saving in PH

// ======================== CONFIGURATION ========================
// WiFi credentials - Change these to your laptop hotspot settings
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Camera settings
#define METAL_IN_PIN 14   // GPIO14 - Metal detector input pin
#define FIND_ME_PIN 13    // GPIO13 - Output to Arduino to trigger "Find Me" buzzer

bool findMeActive = false;  // Is Find Me mode on?
#define CAMERA_MODEL_AI_THINKER
#define FRAME_SIZE FRAMESIZE_VGA  // Options: QVGA, VGA, SVGA, XGA
#define JPEG_QUALITY 10            // 0-63, lower means higher quality
// ===============================================================

WebServer server(80);

// Forward declarations
void handleRoot();
void handleStream();
void handleOptions();
void handleNotFound();
void handleMetalStatus();
void initTimeTracking();
void processMetalDetection();  // CRITICAL: Can be called from handleStream and loop
 
 // Metal detection state
 struct MetalDetection {
   bool detected;
   bool previousState;
   int detectionCount;
   float confidence;
   unsigned long lastDetectionTime;
   unsigned long detectionStartTime;
   unsigned long detectedFlagHoldUntil;  // Keep detected flag true until this time
   char detectionTimeStr[12];            // Real Philippine time when detected (e.g., "11:06:30 PM")
 };
 
 MetalDetection metalDetector = {false, false, 0, 0.0, 0, 0, 0, "--:--"};
 
// ===================== TIME TRACKING =====================
unsigned long robotStartTime = 0;  // When robot started
bool ntpSynced = false;            // Has NTP time been synced?

// HTML page with embedded CSS and JavaScript - G.E.M.S. Interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>G.E.M.S. Interface</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@400;500;600;700;900&family=Montserrat:wght@300;400;500;600&display=swap" rel="stylesheet">
    <style>
/* G.E.M.S. Interface Stylesheet */
:root {
  --primary-cyan: #ff3333;
  --secondary-cyan: #cc6633;
  --dark-cyan: #8B4513;
  --bg-dark: #000000;
  --bg-panel: rgba(20, 5, 0, 0.85);
  --text-primary: #ff3333;
  --text-secondary: #cc6633;
  --green: #00ff88;
  --red: #ff0044;
  --yellow: #ffcc00;
}
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}
html, body {
  width: 100%;
  height: 100%;
  overflow: hidden;
}
body {
  font-family: 'Orbitron', 'Montserrat', monospace;
  background: var(--bg-dark);
  color: var(--text-primary);
  position: relative;
  -webkit-tap-highlight-color: transparent;
  touch-action: manipulation;
}
.video-background {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 1;
  overflow: hidden;
}
.video-background img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  filter: brightness(1.0);
  background: #000;
  min-height: 100vh;
  display: block;
  opacity: 1;
}
.video-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at center, transparent 0%, rgba(0, 0, 0, 0.1) 100%);
  pointer-events: none;
}
.gold-flash {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle at center, rgba(255, 215, 0, 0.7) 0%, rgba(255, 215, 0, 0.3) 50%, transparent 100%);
  pointer-events: none;
  opacity: 0;
  transition: opacity 3s ease-out;
}
.gold-flash.active {
  opacity: 1;
  transition: opacity 0s;
}
.gems-hud {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 10;
  pointer-events: none;
}
.gems-hud * {
  pointer-events: auto;
}
.corner {
  position: absolute;
  width: 5rem;
  height: 5rem;
  border: 0.125rem solid var(--primary-cyan);
  box-shadow: 0 0 1rem var(--primary-cyan);
}
.corner-tl {
  top: 1rem;
  left: 1rem;
  border-right: none;
  border-bottom: none;
  animation: pulse-glow 3s ease-in-out infinite;
}
.corner-tr {
  top: 1rem;
  right: 1rem;
  border-left: none;
  border-bottom: none;
  animation: pulse-glow 3s ease-in-out infinite 0.5s;
}
.corner-bl {
  bottom: 1rem;
  left: 1rem;
  border-right: none;
  border-top: none;
  animation: pulse-glow 3s ease-in-out infinite 1s;
}
.corner-br {
  bottom: 1rem;
  right: 1rem;
  border-left: none;
  border-top: none;
  animation: pulse-glow 3s ease-in-out infinite 1.5s;
}
@keyframes pulse-glow {
  0%, 100% {
    opacity: 1;
    box-shadow: 0 0 1rem var(--primary-cyan);
  }
  50% {
    opacity: 0.6;
    box-shadow: 0 0 0.5rem var(--primary-cyan);
  }
}
.hud-header {
  position: absolute;
  top: 1.5rem;
  left: 50%;
  transform: translateX(-50%);
  width: 90%;
  max-width: 70rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 2rem;
  background: var(--bg-panel);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  box-shadow: 0 0 1.5rem rgba(0, 243, 255, 0.3);
}
.header-left, .header-center, .header-right {
  display: flex;
  align-items: center;
  gap: 1rem;
}
.system-name {
  font-size: 1.5rem;
  font-weight: 700;
  letter-spacing: 0.2em;
  color: var(--primary-cyan);
  text-shadow: 0 0 1rem var(--primary-cyan);
}
.system-subtitle {
  font-size: 0.625rem;
  font-weight: 400;
  letter-spacing: 0.1em;
  color: var(--text-secondary);
  margin-left: -0.5rem;
}
.system-status {
  font-size: 0.75rem;
  color: var(--green);
  background: rgba(0, 255, 136, 0.2);
  padding: 0.25rem 0.75rem;
  border-radius: 0.25rem;
  border: 0.0625rem solid var(--green);
  animation: blink 2s ease-in-out infinite;
}
@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
.time {
  font-size: 1.25rem;
  font-weight: 500;
  color: var(--primary-cyan);
  font-family: 'Orbitron', monospace;
}
.system-label {
  font-size: 0.875rem;
  color: var(--text-secondary);
}
.status-indicator {
  width: 0.75rem;
  height: 0.75rem;
  border-radius: 50%;
  background: var(--green);
  box-shadow: 0 0 1rem var(--green);
  animation: pulse 2s ease-in-out infinite;
}
@keyframes pulse {
  0%, 100% { transform: scale(1); opacity: 1; }
  50% { transform: scale(1.2); opacity: 0.7; }
}
.hud-panel {
  position: absolute;
  top: 5.5rem;
  width: 3rem;
  height: 3rem;
  display: grid;
  grid-template-columns: 1fr 1fr;
  grid-template-rows: 1fr;
  gap: 0.2rem;
  background: var(--bg-panel);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  padding: 0.3rem;
  box-shadow: 0 0 1.5rem rgba(0, 243, 255, 0.2);
  box-sizing: border-box;
  aspect-ratio: 1 / 1;
}
.hud-left {
  display: none;
}
.hud-panel.hud-right {
  position: fixed !important;
  right: 1rem !important;
  bottom: 6rem !important;
  top: auto !important;
  left: auto !important;
  width: auto !important;
  height: auto !important;
  display: flex !important;
  flex-direction: column;
  gap: 0.5rem;
  z-index: 1000;
  background: transparent;
  border: none;
  box-shadow: none;
  aspect-ratio: auto;
}
.button-container {
  width: 3.5rem;
  height: 3.5rem;
  display: flex;
  margin: 0;
  padding: 0;
}
.hud-button {
  width: 100%;
  height: 100%;
  background: rgba(255, 51, 51, 0.3);
  border: 2px solid var(--primary-cyan);
  border-radius: 0.5rem;
  color: var(--text-primary);
  font-size: 1.5rem;
  cursor: pointer;
  transition: all 0.3s ease;
  display: flex;
  align-items: center;
  justify-content: center;
}
.hud-button:hover {
  background: rgba(255, 51, 51, 0.5);
  box-shadow: 0 0 1rem var(--primary-cyan);
  transform: scale(1.05);
}
.hud-button:active {
  transform: scale(0.95);
}
.find-me-btn.active {
  background: var(--red);
  box-shadow: 0 0 2rem var(--red);
  animation: pulse 1s infinite;
}
@keyframes pulse {
  0%, 100% { box-shadow: 0 0 1rem var(--red); }
  50% { box-shadow: 0 0 2rem var(--red); }
}
.panel-title {
  font-size: 0.5rem;
  font-weight: 600;
  letter-spacing: 0.08em;
  color: var(--primary-cyan);
  margin-bottom: 0.4rem;
  padding-bottom: 0.25rem;
  border-bottom: 0.0625rem solid var(--dark-cyan);
  text-shadow: 0 0 0.5rem var(--primary-cyan);
}
.panel-content {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}
.data-line {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 0.45rem;
  padding: 0.25rem 0;
  border-bottom: 0.0625rem solid rgba(0, 243, 255, 0.1);
}
.data-label {
  color: var(--text-secondary);
  font-weight: 400;
  font-size: 0.45rem;
}
.data-value {
  color: var(--primary-cyan);
  font-weight: 600;
  font-family: 'Orbitron', monospace;
  font-size: 0.45rem;
}
.data-value.green {
  color: var(--green);
  text-shadow: 0 0 0.5rem var(--green);
}
.data-value.cyan {
  color: var(--secondary-cyan);
  text-shadow: 0 0 0.5rem var(--secondary-cyan);
}
.panel-divider {
  height: 0.0625rem;
  background: linear-gradient(90deg, transparent, var(--primary-cyan), transparent);
  margin: 0.5rem 0;
}
.hud-button {
  width: 100%;
  height: 100%;
  aspect-ratio: 1 / 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  margin: 0;
  background: rgba(0, 243, 255, 0.05);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.15rem;
  color: var(--primary-cyan);
  font-size: 1.3rem;
  text-shadow: 0 0 0.5rem var(--primary-cyan);
  cursor: pointer;
  transition: all 0.3s ease;
  box-sizing: border-box;
}
.hud-button:hover {
  background: rgba(0, 243, 255, 0.2);
  box-shadow: 0 0 1rem rgba(0, 243, 255, 0.6);
  transform: scale(1.05);
}
.detection-status {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem;
  background: rgba(0, 0, 0, 0.5);
  border-radius: 0.25rem;
  margin-bottom: 0.5rem;
}
.detection-indicator {
  width: 1rem;
  height: 1rem;
  border-radius: 50%;
  background: var(--text-secondary);
  box-shadow: 0 0 0.5rem var(--text-secondary);
}
.detection-indicator.scanning {
  background: var(--primary-cyan);
  box-shadow: 0 0 1rem var(--primary-cyan);
  animation: pulse 1.5s ease-in-out infinite;
}
.detection-indicator.detected {
  background: var(--red);
  box-shadow: 0 0 1.5rem var(--red);
  animation: alert-pulse 0.5s ease-in-out infinite;
}
@keyframes alert-pulse {
  0%, 100% { transform: scale(1); }
  50% { transform: scale(1.3); }
}
.detection-text {
  font-size: 0.75rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  color: var(--text-secondary);
}
.detection-text.detected {
  color: var(--red);
  text-shadow: 0 0 0.5rem var(--red);
  animation: blink 0.5s ease-in-out infinite;
}
.hud-footer {
  position: absolute;
  bottom: 1.5rem;
  left: 50%;
  transform: translateX(-50%);
  width: 90%;
  max-width: 70rem;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 1rem 2rem;
  background: var(--bg-panel);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  box-shadow: 0 0 1.5rem rgba(0, 243, 255, 0.3);
}
.footer-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  width: 100%;
}
.scanner-line {
  width: 100%;
  max-width: 30rem;
  height: 0.1875rem;
  background: rgba(255, 51, 51, 0.1);
  position: relative;
  overflow: hidden;
  border-radius: 0.0625rem;
}
.scanner-line::after {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  width: 25%;
  height: 100%;
  background: linear-gradient(90deg, transparent, var(--primary-cyan), transparent);
  box-shadow: 0 0 1rem var(--primary-cyan), 0 0 2rem var(--primary-cyan);
  animation: scanLeftToRight 3s ease-in-out infinite;
  opacity: 1;
  transition: opacity 1s ease-out;
}
.scanner-line.fast-scan::after {
  animation: scanLeftToRight 0.3s linear infinite;
  opacity: 0.2;
}
.scanner-line.slowing-1::after {
  animation: scanLeftToRight 0.6s ease-in-out infinite;
  opacity: 0.3;
}
.scanner-line.slowing-2::after {
  animation: scanLeftToRight 1s ease-in-out infinite;
  opacity: 0.5;
}
.scanner-line.slowing-3::after {
  animation: scanLeftToRight 1.5s ease-in-out infinite;
  opacity: 0.7;
}
.scanner-line.slowing-4::after {
  animation: scanLeftToRight 2s ease-in-out infinite;
  opacity: 0.9;
}
@keyframes scanLeftToRight {
  0% { 
    left: -25%; 
    opacity: 0;
  }
  5% {
    opacity: 1;
  }
  45% {
    opacity: 1;
  }
  50% { 
    left: 100%; 
    opacity: 0;
  }
  55% {
    opacity: 1;
  }
  95% {
    opacity: 1;
  }
  100% { 
    left: -25%; 
    opacity: 0;
  }
}
.copyright {
  font-size: 0.625rem;
  color: var(--text-secondary);
  letter-spacing: 0.1em;
}
.notification-container {
  position: absolute;
  top: 6rem;
  right: 1.5rem;
  width: 20rem;
  max-width: 90vw;
  z-index: 1000;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}
.notification {
  background: var(--bg-panel);
  border: 0.0625rem solid var(--primary-cyan);
  border-left: 0.25rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  padding: 1rem 1.25rem;
  box-shadow: 0 0 1.5rem rgba(0, 243, 255, 0.4);
  animation: slideIn 0.3s ease-out, fadeOut 0.3s ease-in 2.7s;
  opacity: 0;
  transform: translateX(100%);
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
}
.notification.show {
  animation: slideIn 0.3s ease-out forwards;
  opacity: 1;
  transform: translateX(0);
}
.notification.hide {
  animation: slideOut 0.3s ease-in forwards;
}
@keyframes slideIn {
  from {
    transform: translateX(100%);
    opacity: 0;
  }
  to {
    transform: translateX(0);
    opacity: 1;
  }
}
@keyframes slideOut {
  from {
    transform: translateX(0);
    opacity: 1;
  }
  to {
    transform: translateX(100%);
    opacity: 0;
  }
}
.notification-content {
  flex: 1;
}
.notification-title {
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--primary-cyan);
  margin-bottom: 0.25rem;
  letter-spacing: 0.05em;
}
.notification-message {
  font-size: 0.75rem;
  color: var(--text-secondary);
  line-height: 1.6;
  white-space: pre-line;
}
.notification-close {
  background: none;
  border: none;
  color: var(--text-secondary);
  font-size: 1.25rem;
  cursor: pointer;
  padding: 0;
  line-height: 1;
  transition: color 0.2s ease;
  min-width: 1.25rem;
}
.notification-close:hover {
  color: var(--primary-cyan);
}
.notification.success {
  border-left-color: var(--green);
}
.notification.warning {
  border-left-color: var(--yellow);
}
.notification.error {
  border-left-color: var(--red);
}
.notification.info {
  border-left-color: var(--secondary-cyan);
}
.view-log-btn {
  position: absolute;
  top: 6.5rem;
  left: 1.5rem;
  padding: 0.5rem 1rem;
  background: rgba(255, 51, 51, 0.1);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  color: var(--primary-cyan);
  font-family: 'Orbitron', monospace;
  font-size: 0.6rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 0 1rem rgba(255, 51, 51, 0.3);
  text-shadow: 0 0 0.5rem var(--primary-cyan);
}
.view-log-btn:hover {
  background: rgba(255, 51, 51, 0.2);
  box-shadow: 0 0 2rem rgba(255, 51, 51, 0.6);
  transform: scale(1.05);
}
.log-popup {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%) scale(0);
  width: 90%;
  max-width: 35rem;
  max-height: 80vh;
  background: rgba(10, 5, 0, 0.95);
  border: 0.125rem solid var(--primary-cyan);
  border-radius: 0.5rem;
  box-shadow: 0 0 3rem rgba(255, 51, 51, 0.5);
  opacity: 0;
  transition: all 0.3s ease;
  z-index: 1000;
  overflow: hidden;
}
.log-popup.show {
  transform: translate(-50%, -50%) scale(1);
  opacity: 1;
}
.log-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1.25rem 1.5rem;
  background: rgba(255, 51, 51, 0.1);
  border-bottom: 0.0625rem solid var(--primary-cyan);
}
.log-header h2 {
  font-size: 1.125rem;
  font-weight: 700;
  letter-spacing: 0.15em;
  color: var(--primary-cyan);
  text-shadow: 0 0 1rem var(--primary-cyan);
  margin: 0;
}
.log-close {
  background: none;
  border: none;
  color: var(--text-secondary);
  font-size: 2rem;
  cursor: pointer;
  padding: 0;
  line-height: 1;
  transition: all 0.2s ease;
  width: 2rem;
  height: 2rem;
  display: flex;
  align-items: center;
  justify-content: center;
}
.log-close:hover {
  color: var(--primary-cyan);
  transform: scale(1.2);
}
.log-content {
  padding: 0 1.5rem 1.5rem 1.5rem;
  max-height: calc(80vh - 5rem);
  overflow-y: auto;
}
.log-table {
  width: 100%;
  border-collapse: collapse;
  font-family: 'Orbitron', monospace;
  margin-top: 0;
}
.log-table thead {
  background: #0a0500;
  position: sticky;
  top: 0;
  z-index: 10;
  border-bottom: 0.125rem solid var(--primary-cyan);
  box-shadow: 0 0.25rem 0.5rem rgba(0, 0, 0, 0.5);
}
.log-table th {
  padding: 1rem 0.75rem;
  text-align: left;
  font-size: 0.75rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  color: var(--primary-cyan);
  background: #0a0500;
}
.log-table td {
  padding: 0.75rem;
  font-size: 0.75rem;
  color: var(--text-secondary);
  border-bottom: 0.0625rem solid rgba(255, 51, 51, 0.1);
}
.log-table tbody tr:hover {
  background: rgba(255, 51, 51, 0.05);
}
.log-empty {
  text-align: center;
  padding: 3rem;
  color: var(--text-secondary);
  font-size: 0.875rem;
  display: none;
}
.log-table tbody:empty ~ .log-empty {
  display: block;
}
.reset-btn {
  position: absolute;
  bottom: 6.5rem;
  left: 1.5rem;
  padding: 0.5rem 1rem;
  background: rgba(255, 51, 51, 0.1);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  color: var(--primary-cyan);
  font-family: 'Orbitron', monospace;
  font-size: 0.6rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 0 1rem rgba(255, 51, 51, 0.3);
  text-shadow: 0 0 0.5rem var(--primary-cyan);
}
.reset-btn:hover {
  background: rgba(255, 51, 51, 0.2);
  box-shadow: 0 0 2rem rgba(255, 51, 51, 0.6);
  transform: scale(1.05);
}
.test-button {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  padding: 0.75rem 1.5rem;
  background: rgba(0, 243, 255, 0.1);
  border: 0.0625rem solid var(--primary-cyan);
  border-radius: 0.25rem;
  color: var(--primary-cyan);
  font-family: 'Orbitron', monospace;
  font-size: 0.75rem;
  font-weight: 600;
  letter-spacing: 0.15em;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 0 1rem rgba(0, 243, 255, 0.3);
  text-shadow: 0 0 0.5rem var(--primary-cyan);
}
.test-button:hover {
  background: rgba(0, 243, 255, 0.2);
  box-shadow: 0 0 2rem rgba(0, 243, 255, 0.6);
  transform: translate(-50%, -50%) scale(1.1);
}
.scan-lines {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: repeating-linear-gradient(
    0deg,
    transparent,
    transparent 0.125rem,
    rgba(0, 243, 255, 0.03) 0.125rem,
    rgba(0, 243, 255, 0.03) 0.25rem
  );
  pointer-events: none;
  animation: scan-move 10s linear infinite;
}
@keyframes scan-move {
  0% { transform: translateY(0); }
  100% { transform: translateY(0.5rem); }
}
/* Mobile-specific styles */
@media (max-width: 48rem) {
  /* Hide corners on mobile */
  .corner {
    display: none;
  }
  
  /* Simplify header for mobile */
  .hud-header {
    top: 0.5rem;
    left: 50%;
    transform: translateX(-50%);
    width: 95%;
    padding: 0.5rem 0.75rem;
    flex-wrap: wrap;
    gap: 0.5rem;
  }
  
  .header-left {
    flex: 1;
    min-width: 100%;
    justify-content: center;
    order: 1;
  }
  
  .header-center {
    flex: 1;
    justify-content: center;
    order: 2;
  }
  
  .header-right {
    flex: 1;
    justify-content: center;
    order: 3;
  }
  
  .system-name {
    font-size: 1.25rem;
  }
  
  .system-subtitle {
    font-size: 0.5rem;
  }
  
  .system-status {
    font-size: 0.625rem;
    padding: 0.2rem 0.5rem;
  }
  
  .time {
    font-size: 1rem;
  }
  
  .system-label {
    font-size: 0.75rem;
  }
  
  /* Show left panel on mobile with better layout */
  .hud-left {
    display: block;
    position: fixed;
    top: auto;
    bottom: 5rem;
    left: 0.5rem;
    width: calc(50% - 0.75rem);
    max-width: 12rem;
    height: auto;
    max-height: calc(100vh - 12rem);
    overflow-y: auto;
    padding: 0.5rem;
    font-size: 0.7rem;
  }
  
  .hud-panel.hud-right {
    position: fixed !important;
    right: 0.5rem !important;
    bottom: 5rem !important;
    top: auto !important;
    left: auto !important;
    width: auto !important;
    height: auto !important;
    display: flex !important;
    flex-direction: column;
    gap: 0.5rem;
    z-index: 1000;
    background: transparent !important;
    border: none !important;
    box-shadow: none !important;
  }
  .hud-right .button-container {
    width: 2.5rem;
    height: 2.5rem;
  }
  .hud-right .hud-button {
    font-size: 1rem;
  }
  
  .panel-title {
    font-size: 0.6rem;
    margin-bottom: 0.3rem;
  }
  
  .data-line {
    font-size: 0.55rem;
    padding: 0.2rem 0;
  }
  
  .data-label, .data-value {
    font-size: 0.55rem;
  }
  
  /* Make buttons larger and easier to tap */
  /* Touch-friendly buttons */
  .view-log-btn, .reset-btn, .test-button, .hud-button {
    -webkit-tap-highlight-color: rgba(255, 51, 51, 0.3);
    touch-action: manipulation;
    cursor: pointer;
  }
  
  .view-log-btn, .reset-btn {
    padding: 0.75rem 1rem;
    font-size: 0.7rem;
    min-height: 2.5rem;
    min-width: 5rem;
  }
  
  .view-log-btn {
    top: auto;
    bottom: 2rem;
    left: 0.5rem;
    width: calc(50% - 0.75rem);
    max-width: 12rem;
  }
  
  .reset-btn {
    bottom: 2rem;
    left: auto;
    right: 0.5rem;
    width: calc(50% - 0.75rem);
    max-width: 12rem;
  }
  
  .test-button {
    padding: 1rem 1.5rem;
    font-size: 0.875rem;
    min-height: 3rem;
    min-width: 8rem;
  }
  
  /* Footer adjustments */
  .hud-footer {
    bottom: 0.5rem;
    width: 95%;
    padding: 0.5rem 0.75rem;
  }
  
  .copyright {
    font-size: 0.5rem;
  }
  
  .scanner-line {
    max-width: 100%;
    height: 0.125rem;
  }
  
  /* Detection status mobile */
  .detection-status {
    padding: 0.5rem;
    gap: 0.5rem;
  }
  
  .detection-indicator {
    width: 0.75rem;
    height: 0.75rem;
  }
  
  .detection-text {
    font-size: 0.65rem;
  }
  
  /* Notifications mobile */
  .notification-container {
    top: 4rem;
    right: 0.5rem;
    width: calc(100vw - 1rem);
    max-width: 20rem;
  }
  
  .notification {
    padding: 0.75rem 1rem;
  }
  
  .notification-title {
    font-size: 0.75rem;
  }
  
  .notification-message {
    font-size: 0.65rem;
  }
  
  /* Log popup mobile - make it fullscreen-like */
  .log-popup {
    width: 95%;
    max-width: 95vw;
    max-height: 90vh;
    top: 50%;
    left: 50%;
  }
  
  .log-header {
    padding: 1rem;
  }
  
  .log-header h2 {
    font-size: 1rem;
  }
  
  .log-content {
    padding: 0 1rem 1rem 1rem;
    max-height: calc(90vh - 4rem);
  }
  
  .log-table th,
  .log-table td {
    padding: 0.5rem 0.25rem;
    font-size: 0.65rem;
  }
  
  /* Video overlay lighter on mobile */
  .video-overlay {
    background: radial-gradient(circle at center, transparent 0%, rgba(0, 0, 0, 0.05) 100%);
  }
  
  /* Video stream mobile optimization */
  .video-background img {
    object-fit: contain;
    filter: brightness(1.0);
  }
  
  /* Panel divider */
  .panel-divider {
    margin: 0.3rem 0;
  }
  
  /* Prevent text selection on mobile for better UX */
  .hud-header, .hud-footer, .hud-panel, button {
    -webkit-user-select: none;
    user-select: none;
  }
  
  /* Better scrolling on mobile */
  .hud-left {
    -webkit-overflow-scrolling: touch;
  }
  
  .log-content {
    -webkit-overflow-scrolling: touch;
  }
}

/* Landscape mobile orientation */
@media (max-width: 48rem) and (orientation: landscape) {
  .hud-header {
    padding: 0.4rem 0.5rem;
    top: 0.25rem;
  }
  
  .hud-left {
    bottom: 3rem;
    max-height: calc(100vh - 8rem);
  }
  
  .view-log-btn, .reset-btn {
    bottom: 0.5rem;
    padding: 0.5rem 0.75rem;
    font-size: 0.65rem;
  }
  
  .hud-footer {
    bottom: 0.25rem;
    padding: 0.4rem 0.5rem;
  }
  
  .test-button {
    padding: 0.75rem 1.25rem;
    font-size: 0.8rem;
  }
}

/* Tablet styles */
@media (min-width: 48.1rem) and (max-width: 64rem) {
  .hud-header {
    width: 95%;
    padding: 0.75rem 1.5rem;
  }
  
  .hud-left {
    display: block;
    width: 15rem;
    max-width: 15rem;
  }
  
  .view-log-btn, .reset-btn {
    font-size: 0.65rem;
    padding: 0.6rem 0.9rem;
  }
}
    </style>
</head>
<body>
    <div class="video-background">
        <img id="videoStream" src="/stream" alt="ESP32-CAM Stream">
        <div class="video-overlay"></div>
        <div class="gold-flash" id="goldFlash"></div>
    </div>
    <div class="gems-hud">
        <div class="corner corner-tl"></div>
        <div class="corner corner-tr"></div>
        <div class="corner corner-bl"></div>
        <div class="corner corner-br"></div>
        <button class="view-log-btn" id="viewLogBtn">VIEW LOG</button>
        <button class="reset-btn" id="resetBtn">RESET</button>
        <div class="hud-header">
            <div class="header-left">
                <span class="system-name">G.E.M.S.</span>
                <span class="system-subtitle">Metal Detection Robot</span>
                <span class="system-status">ONLINE</span>
            </div>
            <div class="header-center">
                <span class="time" id="currentTime">00:00:00</span>
            </div>
            <div class="header-right">
                <span class="system-label">v1.0</span>
                <div class="status-indicator active"></div>
            </div>
        </div>
        <div class="hud-panel hud-left">
            <div class="panel-title">SYSTEM STATUS</div>
            <div class="panel-content">
                <div class="data-line">
                    <span class="data-label">CONNECTION</span>
                    <span class="data-value green">STABLE</span>
                </div>
                <div class="data-line">
                    <span class="data-label">STREAM</span>
                    <span class="data-value green">ACTIVE</span>
                </div>
                <div class="data-line">
                    <span class="data-label">FPS</span>
                    <span class="data-value" id="fpsValue">30</span>
                </div>
                <div class="data-line">
                    <span class="data-label">RESOLUTION</span>
                    <span class="data-value" id="resValue">640x480</span>
                </div>
            </div>
            <div class="panel-divider"></div>
            <div class="panel-title">DIAGNOSTICS</div>
            <div class="panel-content">
                <div class="data-line">
                    <span class="data-label">POWER</span>
                    <span class="data-value green">OPTIMAL</span>
                </div>
                <div class="data-line">
                    <span class="data-label">SIGNAL</span>
                    <span class="data-value green">98%</span>
                </div>
                <div class="data-line">
                    <span class="data-label">LATENCY</span>
                    <span class="data-value" id="latency">24ms</span>
                </div>
            </div>
            <div class="panel-divider"></div>
            <div class="panel-title">METAL DETECTION</div>
            <div class="panel-content">
                <div class="detection-status" id="detectionStatus">
                    <div class="detection-indicator" id="detectionIndicator"></div>
                    <span class="detection-text" id="detectionText">SCANNING...</span>
                </div>
                <div class="data-line">
                    <span class="data-label">DETECTED AT</span>
                    <span class="data-value" id="detectionTime">--:--:-- --</span>
                </div>
                <div class="data-line">
                    <span class="data-label">CURRENT TIME</span>
                    <span class="data-value" id="currentTime">--:--:-- --</span>
                </div>
                <div class="data-line">
                    <span class="data-label">CONFIDENCE</span>
                    <span class="data-value" id="confidence">--</span>
                </div>
            </div>
        </div>
        <div class="hud-panel hud-right">
            <div class="button-container">
                <button class="hud-button find-me-btn" id="findMeBtn" title="FIND ROBOT">🔊</button>
            </div>
        </div>
        <div class="hud-footer">
            <div class="footer-content">
                <span class="copyright">G.E.M.S. INTERFACE v1.0 | METAL DETECTION ROBOT</span>
                <div class="scanner-line"></div>
            </div>
        </div>
        <div class="scan-lines"></div>
        <div class="notification-container" id="notificationContainer"></div>
        <button class="test-button" id="testButton">TEST</button>
        <div class="log-popup" id="logPopup">
            <div class="log-header">
                <h2>METAL DETECTION LOG</h2>
                <button class="log-close" id="logCloseBtn">×</button>
            </div>
            <div class="log-content">
                <table class="log-table">
                    <thead>
                        <tr>
                            <th>#</th>
                            <th>DETECTED AT</th>
                            <th>CONFIDENCE</th>
                        </tr>
                    </thead>
                    <tbody id="logTableBody"></tbody>
                </table>
                <div class="log-empty" id="logEmpty">No detections recorded</div>
            </div>
        </div>
    </div>
    <script>
class GemsInterface {
    constructor() {
        this.elements = {
            time: document.getElementById('currentTime'),
            fps: document.getElementById('fpsValue'),
            resolution: document.getElementById('resValue'),
            latency: document.getElementById('latency'),
            videoStream: document.getElementById('videoStream'),
            detectionTime: document.getElementById('detectionTime'),
            currentTimeDisplay: document.getElementById('currentTime'),
            confidence: document.getElementById('confidence'),
            detectionStatus: document.getElementById('detectionStatus'),
            detectionIndicator: document.getElementById('detectionIndicator'),
            detectionText: document.getElementById('detectionText'),
            scanMode: document.getElementById('scanMode'),
            detectCount: document.getElementById('detectCount'),
            scanBtn: document.getElementById('scanBtn'),
            simulateBtn: document.getElementById('simulateBtn'),
            clearBtn: document.getElementById('clearBtn')
        };
        this.isScanning = false;
        this.metalDetectionCount = 0;
        this.detectionLog = [];
        this.scanTimeouts = [];
         this.lastDetectionCount = 0;
         this.pollingInterval = null;
         this._lastDetectedState = false;
         this._lastDetectedNotify = 0;
         this._lastDebugLogTime = 0;
        this.addInitialReferencePoint();
        this.init();
    }
    addInitialReferencePoint() {
        const timestamp = new Date().toLocaleTimeString();
        this.detectionLog.push({
            count: 0,
            time: timestamp,
            x: 0,
            y: 0
        });
        console.log('[INITIAL REFERENCE] Origin point set at X: 0, Y: 0');
    }
    init() {
        console.log('G.E.M.S. System Initializing...');
        this.startClock();
        this.startMetricsUpdate();
        this.setupButtonHandlers();
        this.setupDetectionHandlers();
        this.setupTestButton();
        this.setupLogButton();
        this.setupResetButton();
        this.initializeStream();
         this.startMetalDetectionPolling();
        setTimeout(() => {
            console.log('G.E.M.S. Online - Metal Detection Ready');
            this.showNotification('System initialized successfully', 'success', 'SYSTEM READY');
        }, 1000);
    }
    startClock() {
        const updateTime = () => {
            const now = new Date();
            const hours = String(now.getHours()).padStart(2, '0');
            const minutes = String(now.getMinutes()).padStart(2, '0');
            const seconds = String(now.getSeconds()).padStart(2, '0');
            if (this.elements.time) {
                this.elements.time.textContent = `${hours}:${minutes}:${seconds}`;
            }
        };
        updateTime();
        setInterval(updateTime, 1000);
    }
    startMetricsUpdate() {
        setInterval(() => {
            const fps = Math.floor(Math.random() * 5) + 28;
            if (this.elements.fps) {
                this.elements.fps.textContent = fps;
            }
            const latency = Math.floor(Math.random() * 11) + 20;
            if (this.elements.latency) {
                this.elements.latency.textContent = `${latency}ms`;
            }
        }, 1500);
    }
    initializeStream() {
        if (this.elements.videoStream) {
            this.elements.videoStream.addEventListener('load', () => {
                console.log('Video stream loaded successfully');
                const width = this.elements.videoStream.naturalWidth;
                const height = this.elements.videoStream.naturalHeight;
                if (this.elements.resolution && width && height) {
                    this.elements.resolution.textContent = `${width}x${height}`;
                }
            });
            this.elements.videoStream.addEventListener('error', (e) => {
                console.error('Video stream error:', e);
            });
            const streamUrl = "/stream?t=" + Date.now();
            console.log('Setting video stream source to:', streamUrl);
            this.elements.videoStream.src = streamUrl;
        } else {
            console.error('Video stream element not found!');
        }
    }
    setupButtonHandlers() {
        const buttons = document.querySelectorAll('.hud-button');
        buttons.forEach((btn) => {
            btn.addEventListener('click', () => {
                const action = btn.getAttribute('title');
                this.handleButtonClick(action);
            });
        });
    }
    handleButtonClick(action) {
        console.log(`[G.E.M.S.] Action: ${action}`);
        switch(action) {
            case 'CAPTURE':
                this.showNotification('Image captured successfully', 'success', 'CAPTURE');
                break;
            case 'RECORD':
                this.showNotification('Recording started', 'info', 'RECORD');
                break;
        }
    }
    showNotification(message, type = 'info', title = 'G.E.M.S.') {
        const container = document.getElementById('notificationContainer');
        if (!container) return;
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.innerHTML = `
            <div class="notification-content">
                <div class="notification-title">${title}</div>
                <div class="notification-message">${message}</div>
            </div>
            <button class="notification-close" onclick="this.parentElement.remove()">×</button>
        `;
        container.appendChild(notification);
        setTimeout(() => notification.classList.add('show'), 10);
        setTimeout(() => {
            notification.classList.add('hide');
            setTimeout(() => notification.remove(), 300);
        }, 3000);
    }
    setupDetectionHandlers() {
        if (this.elements.scanBtn) {
            this.elements.scanBtn.addEventListener('click', () => {
                this.toggleScanning();
            });
        }
        if (this.elements.simulateBtn) {
            this.elements.simulateBtn.addEventListener('click', () => {
                this.simulateMetalDetection();
            });
        }
        if (this.elements.clearBtn) {
            this.elements.clearBtn.addEventListener('click', () => {
                this.clearDetectionData();
            });
        }
    }
    toggleScanning() {
        this.isScanning = !this.isScanning;
        if (this.isScanning) {
            if (this.elements.scanMode) this.elements.scanMode.textContent = 'ACTIVE';
            if (this.elements.detectionIndicator) this.elements.detectionIndicator.classList.add('scanning');
            if (this.elements.detectionText) this.elements.detectionText.textContent = 'SCANNING...';
            this.showNotification('Metal detection scan started', 'info', 'SCAN ACTIVE');
        } else {
            if (this.elements.scanMode) this.elements.scanMode.textContent = 'STANDBY';
            if (this.elements.detectionIndicator) this.elements.detectionIndicator.classList.remove('scanning');
            if (this.elements.detectionText) this.elements.detectionText.textContent = 'STANDBY';
            this.showNotification('Metal detection scan stopped', 'info', 'SCAN STOPPED');
        }
    }
    simulateMetalDetection() {
        const detection = {
            x: 97,
            y: -12,
            confidence: 94.7
        };
        this.displayDetection(detection);
    }
    displayDetection(detection) {
        if (this.elements.detectionTime) this.elements.detectionTime.textContent = detection.time || '--:--';
        if (this.elements.confidence) this.elements.confidence.textContent = `${detection.confidence}%`;
        if (this.elements.detectionIndicator) {
            this.elements.detectionIndicator.classList.remove('scanning');
            this.elements.detectionIndicator.classList.add('detected');
        }
        if (this.elements.detectionText) {
            this.elements.detectionText.textContent = 'METAL DETECTED!';
            this.elements.detectionText.classList.add('detected');
        }
        this.showNotification(
            `Metal detected at ${detection.time || '--:--'} | Confidence: ${detection.confidence}%`,
            'error',
            'METAL DETECTED!'
        );
        setTimeout(() => {
            if (this.isScanning) {
                if (this.elements.detectionIndicator) {
                    this.elements.detectionIndicator.classList.remove('detected');
                    this.elements.detectionIndicator.classList.add('scanning');
                }
                if (this.elements.detectionText) {
                    this.elements.detectionText.textContent = 'SCANNING...';
                    this.elements.detectionText.classList.remove('detected');
                }
            } else {
                if (this.elements.detectionIndicator) this.elements.detectionIndicator.classList.remove('detected');
                if (this.elements.detectionText) {
                    this.elements.detectionText.textContent = 'STANDBY';
                    this.elements.detectionText.classList.remove('detected');
                }
            }
        }, 3000);
    }
    clearDetectionData() {
        if (this.elements.detectionTime) this.elements.detectionTime.textContent = '--:--:-- --';
        if (this.elements.currentTimeDisplay) this.elements.currentTimeDisplay.textContent = '--:--:-- --';
        if (this.elements.confidence) this.elements.confidence.textContent = '--';
        this.showNotification('Detection data cleared', 'info', 'DATA CLEARED');
    }
    updateLogTable() {
        const tbody = document.getElementById('logTableBody');
        const emptyMsg = document.getElementById('logEmpty');
        if (!tbody) return;
        
        // Clear existing rows
        tbody.innerHTML = '';
        
        // Hide/show empty message
        if (emptyMsg) {
            emptyMsg.style.display = this.detectionLog.length === 0 ? 'block' : 'none';
        }
        
        // Add rows for each detection (newest first)
        const logs = [...this.detectionLog].reverse();
        logs.forEach(log => {
            if (log.count === 0) return; // Skip initial reference point
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${log.count}</td>
                <td>${log.time}</td>
                <td>${log.confidence ? log.confidence.toFixed(1) + '%' : '--'}</td>
            `;
            tbody.appendChild(row);
        });
    }
    setupTestButton() {
        const testBtn = document.getElementById('testButton');
        if (testBtn) {
            testBtn.addEventListener('click', () => {
                const x = Math.floor(Math.random() * 200) - 100;
                const y = Math.floor(Math.random() * 200) - 100;
                this.detectMetal(x, y);
            });
        }
    }
    detectMetal(x, y) {
        this.metalDetectionCount++;
        const timestamp = new Date().toLocaleTimeString();
        this.detectionLog.push({
            count: this.metalDetectionCount,
            time: timestamp,
            x: x,
            y: y
        });
        this.triggerMetalDetectedFlash();
        this.triggerFastScan();
        this.showNotification(
            `Metal detected at X: ${x}, Y: ${y}\nDetected Metal Count: ${this.metalDetectionCount}`,
            'error',
            'METAL DETECTED!'
        );
        console.log(`[DETECTION #${this.metalDetectionCount}] X: ${x}, Y: ${y} at ${timestamp}`);
    }
    triggerFastScan() {
        const scannerLine = document.querySelector('.scanner-line');
        if (!scannerLine) return;
        this.scanTimeouts.forEach(timeout => clearTimeout(timeout));
        this.scanTimeouts = [];
        scannerLine.className = 'scanner-line';
        scannerLine.classList.add('fast-scan');
        this.scanTimeouts.push(setTimeout(() => {
            scannerLine.classList.remove('fast-scan');
            scannerLine.classList.add('slowing-1');
        }, 800));
        this.scanTimeouts.push(setTimeout(() => {
            scannerLine.classList.remove('slowing-1');
            scannerLine.classList.add('slowing-2');
        }, 2000));
        this.scanTimeouts.push(setTimeout(() => {
            scannerLine.classList.remove('slowing-2');
            scannerLine.classList.add('slowing-3');
        }, 4000));
        this.scanTimeouts.push(setTimeout(() => {
            scannerLine.classList.remove('slowing-3');
            scannerLine.classList.add('slowing-4');
        }, 6000));
        this.scanTimeouts.push(setTimeout(() => {
            scannerLine.classList.remove('slowing-4');
            this.scanTimeouts = [];
        }, 8000));
    }
    setupLogButton() {
        const logBtn = document.getElementById('viewLogBtn');
        const logPopup = document.getElementById('logPopup');
        const closeBtn = document.getElementById('logCloseBtn');
        if (logBtn && logPopup) {
            logBtn.addEventListener('click', () => {
                this.showLogPopup();
            });
        }
        if (closeBtn && logPopup) {
            closeBtn.addEventListener('click', () => {
                logPopup.classList.remove('show');
            });
        }
        if (logPopup) {
            logPopup.addEventListener('click', (e) => {
                if (e.target === logPopup) {
                    logPopup.classList.remove('show');
                }
            });
        }
    }
    showLogPopup() {
        const logPopup = document.getElementById('logPopup');
        const tableBody = document.getElementById('logTableBody');
        const logEmpty = document.getElementById('logEmpty');
        const logTable = document.querySelector('.log-table');
        if (!logPopup || !tableBody) return;
        tableBody.innerHTML = '';
        if (this.detectionLog.length > 0) {
            if (logTable) logTable.style.display = 'table';
            if (logEmpty) logEmpty.style.display = 'none';
            this.detectionLog.forEach(log => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${log.count}</td>
                    <td>${log.time}</td>
                    <td>${log.x}</td>
                    <td>${log.y}</td>
                `;
                tableBody.appendChild(row);
            });
        } else {
            if (logTable) logTable.style.display = 'none';
            if (logEmpty) logEmpty.style.display = 'block';
        }
        logPopup.classList.add('show');
    }
    setupResetButton() {
        const resetBtn = document.getElementById('resetBtn');
        if (resetBtn) {
            resetBtn.addEventListener('click', () => {
                this.resetDetectionLog();
            });
        }
    }
    resetDetectionLog() {
        this.metalDetectionCount = 0;
         this.lastDetectionCount = 0;
        this.detectionLog = [];
        this.addInitialReferencePoint();
         
         // Reset UI display
         if (this.elements.detectionTime) this.elements.detectionTime.textContent = '--:--:-- --';
         if (this.elements.currentTimeDisplay) this.elements.currentTimeDisplay.textContent = '--:--:-- --';
         if (this.elements.confidence) this.elements.confidence.textContent = '--';
         
        this.showNotification(
            'Detection log has been cleared.',
            'info',
            'SYSTEM RESET'
        );
        console.log('[RESET] Detection log cleared');
    }
    triggerMetalDetectedFlash() {
        const goldFlash = document.getElementById('goldFlash');
        if (goldFlash) {
            goldFlash.classList.add('active');
            setTimeout(() => {
                goldFlash.classList.remove('active');
            }, 50);
        }
    }
     async startMetalDetectionPolling() {
         console.log('[G.E.M.S.] Starting metal detection polling...');
         
         // Helper function to fetch with timeout - VERY SIMPLE
         const fetchWithTimeout = (url, timeout = 8000) => {
             return new Promise((resolve, reject) => {
                 const timer = setTimeout(() => reject(new Error('Timeout')), timeout);
                 fetch(url)
                     .then(response => {
                         clearTimeout(timer);
                         resolve(response);
                     })
                     .catch(error => {
                         clearTimeout(timer);
                         reject(error);
                     });
             });
         };
         
         // First, sync with ESP32's current count to avoid missing detections after page refresh
         // NOTE: Even if this fails, we'll still start polling - it will catch up
         try {
             console.log('[G.E.M.S.] Fetching initial metal status...');
             const url = '/metal-status?t=' + Date.now();
             console.log('[G.E.M.S.] URL:', url);
             
             const response = await fetchWithTimeout(url, 8000);  // Longer timeout for initial sync
             console.log('[G.E.M.S.] ✅ Got response! Status:', response.status);
             
             if (response.ok) {
                 const data = await response.json();
                 console.log('[G.E.M.S.] ✅ Received data:', JSON.stringify(data));
                 
                 this.lastDetectionCount = data.count;
                 console.log(`[G.E.M.S.] ✅ Synced with ESP32 - current detection count: ${data.count}`);
                 
                // Update UI with current Philippine time
                if (this.elements.currentTimeDisplay) this.elements.currentTimeDisplay.textContent = data.currentTime || '--:--:-- --';
                
                if (data.detected) {
                    // Display real detection time from ESP32
                    if (this.elements.detectionTime) this.elements.detectionTime.textContent = data.detectionTime || '--:--:-- --';
                    if (this.elements.confidence) this.elements.confidence.textContent = `${data.confidence.toFixed(1)}%`;
                    
                    if (this.elements.detectionIndicator) {
                        this.elements.detectionIndicator.classList.remove('scanning');
                        this.elements.detectionIndicator.classList.add('detected');
                    }
                    if (this.elements.detectionText) {
                        this.elements.detectionText.textContent = 'METAL DETECTED!';
                        this.elements.detectionText.classList.add('detected');
                    }
                    console.log('[G.E.M.S.] ✅ UI updated with current detection state');
                }
             } else {
                 console.error(`[G.E.M.S.] ❌ HTTP Error: ${response.status} ${response.statusText}`);
                 const errorText = await response.text();
                 console.error('[G.E.M.S.] Error response:', errorText);
             }
         } catch (error) {
             console.warn('[G.E.M.S.] ⚠️ Initial sync failed (will retry with polling):', error.message);
             console.log('[G.E.M.S.] Starting polling anyway - it will catch up...');
             // Don't show error notification - polling will handle it
             this.lastDetectionCount = 0;  // Start fresh
         }
         
         // Poll ESP32 for metal detection status every 1000ms (1 second)
         // ESP32 needs time between requests since it's also streaming video
         this.pollingInterval = setInterval(() => {
             this.checkMetalDetection();
         }, 1000);  // Poll once per second - gives ESP32 time to handle video stream too
         console.log('[G.E.M.S.] ✅ Metal detection polling started (1000ms interval)');
     }
     async checkMetalDetection() {
         try {
             // Simple fetch with shorter timeout for polling (should be fast)
             const url = '/metal-status?t=' + Date.now();
             const response = await fetch(url);  // Simple fetch - no timeout wrapper for speed
             if (!response.ok) {
                 throw new Error(`Network response was not ok: ${response.status} ${response.statusText}`);
             }
             const data = await response.json();
             
             // DEBUG: Log every successful poll
             console.log(`[POLL] detected=${data.detected}, count=${data.count}, lastCount=${this.lastDetectionCount}`);
             
            // Always update current Philippine time
            if (this.elements.currentTimeDisplay) this.elements.currentTimeDisplay.textContent = data.currentTime || '--:--:-- --';
            
            // PRIMARY: Check if count increased (NEW DETECTION EVENT)
            if (data.count > this.lastDetectionCount) {
                // NEW DETECTION! Count increased
                const newDetections = data.count - this.lastDetectionCount;
                console.log(`🚨🚨🚨 [DETECTION] Count increased from ${this.lastDetectionCount} to ${data.count} (+${newDetections}) 🚨🚨🚨`);
                
                // Update tracking
                this.lastDetectionCount = data.count;
                this.metalDetectionCount = data.count;
                
                // Get real Philippine time from ESP32
                const detectionTimeStr = data.detectionTime || '--:--:-- --';
                
                // ===== UPDATE WHOLE PAGE WITH NEW STATE =====
                if (this.elements.detectionTime) this.elements.detectionTime.textContent = detectionTimeStr;
                if (this.elements.confidence) this.elements.confidence.textContent = `${data.confidence.toFixed(1)}%`;
                
                // Update detection indicator (visual state)
                if (this.elements.detectionIndicator) {
                    this.elements.detectionIndicator.classList.remove('scanning');
                    this.elements.detectionIndicator.classList.add('detected');
                }
                if (this.elements.detectionText) {
                    this.elements.detectionText.textContent = 'METAL DETECTED!';
                    this.elements.detectionText.classList.add('detected');
                }
                
                // Update detection status container
                if (this.elements.detectionStatus) {
                    this.elements.detectionStatus.classList.remove('scanning');
                    this.elements.detectionStatus.classList.add('detected');
                }
                
                // Log the detection with REAL TIME
                this.detectionLog.push({
                    count: data.count,
                    time: detectionTimeStr,
                    confidence: data.confidence
                });
                
                // Update log table
                this.updateLogTable();
                
                // Update system status to show detection state
                const statusIndicator = document.querySelector('.status-indicator');
                if (statusIndicator) {
                    statusIndicator.style.background = 'var(--red)';
                    statusIndicator.style.boxShadow = '0 0 1rem var(--red)';
                }
                
                // Trigger ALL visual effects
                this.triggerMetalDetectedFlash();
                this.triggerFastScan();
                
                // Show notification with REAL TIME
                console.log('[NOTIFICATION] About to show notification popup...');
                this.showNotification(
                    `Metal detected at ${detectionTimeStr}\nConfidence: ${data.confidence.toFixed(1)}% | Detection #${data.count}`,
                    'error',
                    '🚨 METAL DETECTED!'
                );
                console.log('[NOTIFICATION] showNotification() called!');
                
                console.log(`[PAGE UPDATED] Detection #${data.count} at ${detectionTimeStr} - All UI elements updated`);
            }
            
            // SECONDARY: Update indicator status based on current detected state
            if (data.detected) {
                // Metal currently detected - ensure UI shows "METAL DETECTED"
                if (this.elements.detectionIndicator && !this.elements.detectionIndicator.classList.contains('detected')) {
                    this.elements.detectionIndicator.classList.remove('scanning');
                    this.elements.detectionIndicator.classList.add('detected');
                }
                if (this.elements.detectionText && !this.elements.detectionText.classList.contains('detected')) {
                    this.elements.detectionText.textContent = 'METAL DETECTED!';
                    this.elements.detectionText.classList.add('detected');
                }
                
                // Update status indicator
                const statusIndicator = document.querySelector('.status-indicator');
                if (statusIndicator && !statusIndicator.classList.contains('alert')) {
                    statusIndicator.style.background = 'var(--red)';
                    statusIndicator.style.boxShadow = '0 0 1rem var(--red)';
                }
            } else {
                // Metal no longer detected - revert to "SCANNING..."
                if (this.elements.detectionIndicator && this.elements.detectionIndicator.classList.contains('detected')) {
                    this.elements.detectionIndicator.classList.remove('detected');
                    this.elements.detectionIndicator.classList.add('scanning');
                }
                if (this.elements.detectionText && this.elements.detectionText.classList.contains('detected')) {
                    this.elements.detectionText.textContent = 'SCANNING...';
                    this.elements.detectionText.classList.remove('detected');
                }
                
                // Reset status indicator to normal
                const statusIndicator = document.querySelector('.status-indicator');
                if (statusIndicator) {
                    statusIndicator.style.background = 'var(--green)';
                    statusIndicator.style.boxShadow = '0 0 1rem var(--green)';
                }
             }
         } catch (error) {
             console.error('[G.E.M.S.] ❌ Error polling metal detection:', error);
             console.error('[G.E.M.S.] Error details:', error.message);
             
             // Log error occasionally to avoid spam
             if (!this._lastErrorLog || Date.now() - this._lastErrorLog > 5000) {
                 this._lastErrorLog = Date.now();
                 console.error('[G.E.M.S.] ⚠️ Polling error - check ESP32 connection and /metal-status endpoint');
             }
         }
     }
}
document.addEventListener('DOMContentLoaded', () => {
    console.log('═══════════════════════════════════════');
    console.log('       G.E.M.S. SYSTEM BOOT            ');
    console.log('    Metal Detection System v1.0        ');
    console.log('═══════════════════════════════════════');
    const gems = new GemsInterface();
    window.GEMS = gems;
    
    // Find Me button handler
    const findMeBtn = document.getElementById('findMeBtn');
    let findMeActive = false;
    if (findMeBtn) {
        findMeBtn.addEventListener('click', async () => {
            try {
                const response = await fetch('/find-me');
                const data = await response.json();
                findMeActive = data.active;
                findMeBtn.style.background = findMeActive ? 'var(--red)' : '';
                findMeBtn.style.boxShadow = findMeActive ? '0 0 2rem var(--red)' : '';
                gems.showNotification(
                    findMeActive ? 'Buzzer activated! Follow the sound.' : 'Buzzer deactivated.',
                    findMeActive ? 'error' : 'info',
                    findMeActive ? '🔊 FIND ME ACTIVE' : 'FIND ME OFF'
                );
                console.log('[FIND ME]', findMeActive ? 'ACTIVATED' : 'Deactivated');
            } catch (error) {
                console.error('[FIND ME] Error:', error);
                gems.showNotification('Failed to toggle buzzer', 'error', 'ERROR');
            }
        });
        console.log('[G.E.M.S.] Find Me button ready');
    }
    
    // Keyboard shortcut: F for Find Me
    document.addEventListener('keydown', (e) => {
        if (e.key.toLowerCase() === 'f') {
            if (findMeBtn) findMeBtn.click();
        }
    });
    
    console.log('[G.E.M.S.] Keyboard shortcut: F = Find Me');
    console.log('[G.E.M.S.] Metal Detection System ready');
});
    </script>
</body>
</html>
)rawliteral";

// Initialize camera
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAME_SIZE;
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  
  // Camera sensor settings
  sensor_t * s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Camera sensor not found");
    return false;
  }
  
  // Adjust sensor settings for better image quality
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);
  s->set_special_effect(s, 0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_ae_level(s, 0);
  s->set_aec_value(s, 300);
  s->set_gain_ctrl(s, 1);
  s->set_agc_gain(s, 0);
  s->set_gainceiling(s, (gainceiling_t)0);
  s->set_bpc(s, 0);
  s->set_wpc(s, 1);
  s->set_raw_gma(s, 1);
  s->set_lenc(s, 1);
  s->set_hmirror(s, 0);
  s->set_vflip(s, 0);
  s->set_dcw(s, 1);
  s->set_colorbar(s, 0);
  
  Serial.println("Camera initialized successfully");
  return true;
}

// Handle root page
void handleRoot() {
  Serial.println("Client connected to root page");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "text/html", index_html);
}

// Handle MJPEG stream
void handleStream() {
  Serial.println("Stream requested");
  
  WiFiClient client = server.client();
  
  // MJPEG response header with CORS
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
  response += "Access-Control-Allow-Origin: *\r\n";
  response += "Access-Control-Allow-Methods: GET\r\n\r\n";
  server.sendContent(response);
  
  // Stream frames
  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(10);
      // CRITICAL: Allow server to handle other requests even during camera failure
      server.handleClient();
      yield();
      continue;
    }
    
    // Send MJPEG frame
    client.printf("--frame\r\n");
    client.printf("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.printf("\r\n");
    
    esp_camera_fb_return(fb);
    
    // CRITICAL: Process metal detection INSIDE the stream loop!
    // Without this, metal detection never runs while streaming video
    processMetalDetection();
    
    // Handle other server requests (like /metal-status)
    server.handleClient();
    yield();
    
    // Frame rate control
    delay(50);
  }
  
  Serial.println("Stream client disconnected");
}

// Handle CORS preflight
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "text/plain", "");
 }
 
// Helper function to get current Philippine time as string
void getCurrentTimeStr(char* buffer, int bufferSize) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    snprintf(buffer, bufferSize, "--:--:--");
    return;
  }
  // Format as 12-hour time with AM/PM (e.g., "11:06:30 PM")
  strftime(buffer, bufferSize, "%I:%M:%S %p", &timeinfo);
}

// Handle metal detection status API
void handleMetalStatus() {
  Serial.println("[API] /metal-status called!");  // Immediate log
  
  // Get current Philippine time
  char currentTimeStr[15];
  getCurrentTimeStr(currentTimeStr, sizeof(currentTimeStr));
  
  // Build JSON response with REAL TIME
  String json = "{";
  json += "\"detected\":" + String(metalDetector.detected ? "true" : "false") + ",";
  json += "\"count\":" + String(metalDetector.detectionCount) + ",";
  json += "\"currentTime\":\"" + String(currentTimeStr) + "\",";
  json += "\"detectionTime\":\"" + String(metalDetector.detectionTimeStr) + "\",";
  json += "\"confidence\":" + String(metalDetector.confidence, 1);
  json += "}";
  
  Serial.printf("[API] Sending JSON: %s\n", json.c_str());
  
  // Send response immediately
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
  
  Serial.println("[API] /metal-status response sent!");
}
 
// ===================== TIME TRACKING =====================
// Initialize NTP for real Philippine time
void initTimeTracking() {
  if (robotStartTime == 0) {
    robotStartTime = millis();
  }
  
  // Sync with NTP server if not already done
  if (!ntpSynced) {
    Serial.println("[TIME] Syncing with NTP server (Philippine Time)...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time to sync (up to 5 seconds)
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
      delay(500);
      attempts++;
      Serial.print(".");
    }
    
    if (getLocalTime(&timeinfo)) {
      ntpSynced = true;
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", &timeinfo);
      Serial.printf("\n[TIME] ✅ Philippine Time synced: %s\n", timeStr);
    } else {
      Serial.println("\n[TIME] ⚠️ NTP sync failed - using fallback time");
    }
  }
}

// Handle 404
void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// ============================================================
// CRITICAL: Metal detection processing - called from loop() AND handleStream()
// This ensures metal detection works even during video streaming!
// ============================================================
void processMetalDetection() {
  static unsigned long lastCheck = 0;
  static int highStreak = 0;
  static bool detectionLatched = false;
  const int HIGH_STREAK_NEEDED = 1;  // SENSITIVE: Trigger on first HIGH reading!
  
  unsigned long currentTime = millis();
  
  // Check every 20ms for faster response to small metals
  if (currentTime - lastCheck < 20) return;
  lastCheck = currentTime;
  
  // Read metal detector pin
  bool metalDetected = digitalRead(METAL_IN_PIN) == HIGH;
  
  if (metalDetected) {
    // Pin is HIGH - increment streak (max 20 for confidence calculation)
    if (highStreak < 20) highStreak++;
    
    // Calculate REAL confidence based on consecutive readings
    // More consecutive HIGH readings = higher confidence
    // 1 reading = 50%, 5 readings = 70%, 10 readings = 85%, 20+ readings = 99%
    float calculatedConfidence = 50.0 + (highStreak * 2.5);
    if (calculatedConfidence > 99.0) calculatedConfidence = 99.0;
    
    // Check if we have enough consecutive HIGH readings AND haven't already triggered
    if (highStreak >= HIGH_STREAK_NEEDED && !detectionLatched) {
      // NEW DETECTION EVENT!
      detectionLatched = true;
      
      metalDetector.detectionCount++;
      metalDetector.detectionStartTime = currentTime;
      metalDetector.lastDetectionTime = currentTime;
      
      // Record REAL PHILIPPINE TIME of detection
      getCurrentTimeStr(metalDetector.detectionTimeStr, sizeof(metalDetector.detectionTimeStr));
      
      // Set initial confidence (will increase as signal stays strong)
      metalDetector.confidence = calculatedConfidence;
      metalDetector.detected = true;
      metalDetector.detectedFlagHoldUntil = currentTime + 3000;
      
      Serial.printf("[METAL DETECTED #%d] Time: %s, Confidence: %.1f%%\n", 
                    metalDetector.detectionCount, metalDetector.detectionTimeStr, metalDetector.confidence);
    }
    
    // Update confidence as signal stays strong (increases over time)
    metalDetector.confidence = calculatedConfidence;
    
    // Keep detected flag true while metal is present
    metalDetector.detected = true;
    metalDetector.detectedFlagHoldUntil = currentTime + 3000;
    
  } else {
    // Pin is LOW - reset streak and latch
    if (detectionLatched) {
      Serial.printf("[METAL LOST] count=%d\n", metalDetector.detectionCount);
    }
    highStreak = 0;
    detectionLatched = false;
  }
  
  metalDetector.previousState = metalDetected;
  
  // Clear detected flag after hold period expires
  if (!metalDetected && currentTime >= metalDetector.detectedFlagHoldUntil) {
    metalDetector.detected = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(METAL_IN_PIN, INPUT_PULLDOWN);
  pinMode(FIND_ME_PIN, OUTPUT);
  digitalWrite(FIND_ME_PIN, LOW);
  Serial.println("Metal input on GPIO14 ready (INPUT_PULLDOWN mode).");
  Serial.println("Find Me output on GPIO13 ready.");
   
   // Initialize metal detector state
   metalDetector.detected = false;
   metalDetector.previousState = false;
   metalDetector.detectionCount = 0;
   metalDetector.detectedFlagHoldUntil = 0;
   Serial.println("Metal detection ready - waiting for signal from Arduino...");
  Serial.println("\n\nESP32-CAM Direct Web Server");
  Serial.println("============================");
  
  // Initialize camera
  Serial.println("Initializing camera...");
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    Serial.println("Please check your camera module and connections");
    while(1) { delay(1000); }
  }
  
  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Please check your SSID and password");
    Serial.println("Make sure your laptop hotspot is active");
    while(1) { delay(1000); }
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/stream", HTTP_OPTIONS, handleOptions);
   server.on("/metal-status", HTTP_GET, handleMetalStatus);
   server.on("/metal-status", HTTP_OPTIONS, handleOptions);  // Add CORS preflight support
   
   // Test endpoint to verify server is responding
   server.on("/test", HTTP_GET, []() {
     server.sendHeader("Access-Control-Allow-Origin", "*");
     server.send(200, "text/plain", "ESP32 server is working!");
   });
   
   // Find Me buzzer control
   server.on("/find-me", HTTP_GET, []() {
     findMeActive = !findMeActive;  // Toggle
     digitalWrite(FIND_ME_PIN, findMeActive ? HIGH : LOW);
     Serial.printf("[FIND ME] %s\n", findMeActive ? "ACTIVATED - Buzzer ON!" : "Deactivated");
     server.sendHeader("Access-Control-Allow-Origin", "*");
     String json = "{\"active\":" + String(findMeActive ? "true" : "false") + "}";
     server.send(200, "application/json", json);
   });
   server.on("/find-me", HTTP_OPTIONS, handleOptions);
   
   
   server.on("/raw-metal", HTTP_GET, []() {
     int raw = digitalRead(METAL_IN_PIN);
     server.sendHeader("Access-Control-Allow-Origin", "*");
     server.send(200, "text/plain", String(raw));
   });
   server.on("/raw-metal-live", HTTP_GET, []() {
     // Auto-refreshing HTML page for real-time pin monitoring
     String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
     html += "<meta http-equiv='refresh' content='0.1'>";  // Refresh every 100ms
     html += "<title>GPIO14 Live Monitor</title>";
     html += "<style>body{font-family:monospace;background:#000;color:#0f0;padding:20px;}";
     html += ".high{color:#f00;font-size:48px;font-weight:bold;}";
     html += ".low{color:#0f0;font-size:48px;}";
     html += ".info{color:#0ff;margin-top:20px;}</style></head><body>";
     html += "<h1>GPIO14 Live Monitor</h1>";
     int raw = digitalRead(METAL_IN_PIN);
     html += "<div class='" + String(raw ? "high" : "low") + "'>";
     html += "GPIO14: " + String(raw ? "HIGH (METAL DETECTED!)" : "LOW (No Metal)");
     html += "</div>";
     html += "<div class='info'>";
     html += "Detection Count: " + String(metalDetector.detectionCount) + "<br>";
     html += "Detected Flag: " + String(metalDetector.detected ? "TRUE" : "FALSE") + "<br>";
     html += "Last Update: " + String(millis()) + " ms<br>";
     html += "<small>Page auto-refreshes every 100ms</small>";
     html += "</div></body></html>";
     server.sendHeader("Access-Control-Allow-Origin", "*");
     server.send(200, "text/html", html);
   });
  server.on("/debug", HTTP_GET, []() {
    unsigned long elapsed = robotStartTime > 0 ? (millis() - robotStartTime) / 1000 : 0;
    int mins = elapsed / 60;
    int secs = elapsed % 60;
    
    String debug = "=== ESP32 METAL DETECTION DEBUG ===\n\n";
    debug += "GPIO14 (Raw Pin State): " + String(digitalRead(METAL_IN_PIN) ? "HIGH" : "LOW") + "\n";
    debug += "Detected Flag: " + String(metalDetector.detected ? "TRUE" : "FALSE") + "\n";
    debug += "Detection Count: " + String(metalDetector.detectionCount) + "\n";
    debug += "Confidence: " + String(metalDetector.confidence, 1) + "%\n";
    debug += "Last Detection Time: " + String(metalDetector.lastDetectionTime) + " ms\n\n";
    debug += "=== TIME ===\n";
    debug += "Elapsed: " + String(mins) + ":" + String(secs < 10 ? "0" : "") + String(secs) + "\n";
    debug += "Last Detection At: " + String(metalDetector.detectionTimeStr) + "\n";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", debug);
  });
  server.onNotFound(handleNotFound);
  
  // Start server
  server.begin();
  Serial.println("\nWeb server started!");
  Serial.println("============================");
  Serial.print("Open browser and go to: http://");
  Serial.println(WiFi.localIP());
  Serial.println("============================\n");
}

void loop() {
  // Handle server requests
  server.handleClient();
  
  // Initialize time tracking on first loop
  initTimeTracking();
  
  // Process metal detection (shared function, also called from handleStream)
  processMetalDetection();
  
  // Log elapsed time periodically for debugging
  static unsigned long lastLogTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastLogTime > 10000) {
    lastLogTime = currentTime;
    if (robotStartTime > 0) {
      unsigned long elapsed = (currentTime - robotStartTime) / 1000;
      int mins = elapsed / 60;
      int secs = elapsed % 60;
      Serial.printf("[TIME] Elapsed: %02d:%02d | Detections: %d\n", 
                   mins, secs, metalDetector.detectionCount);
    }
  }
}
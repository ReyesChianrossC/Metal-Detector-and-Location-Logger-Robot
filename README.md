# Metal Detection and Location Logger Robot

Welcome to the **Metal Detection and Location Logger Robot** repository! This project integrates an ESP32, an ESP32-CAM module, a metal detector, and a location logging mechanism into a unified robotic system. 

The robot is capable of detecting metals in its path, logging the geographical location of the detection, and streaming live video to a custom web interface.

## Repository Structure

This repository is divided into discrete modules, depending on the microcontroller and the specific task:

- **`Arduino Code/`**: Contains various Arduino sketches used for calibration and testing components (e.g., motor drivers, sensors).
- **`ESP32Code/`**: The main operational code for the primary ESP32 microcontroller, responsible for movement, metal detection logic, and overall coordination.
- **`ESP32 Cam Web App/`**: The code, documentation, and web interface elements specifically for the ESP32-CAM. This module runs a standalone web server serving an HTML/CSS UI and an MJPEG video stream.

## Getting Started

### 1. ESP32 Main Controller setup
Open the `ESP32Code.ino` using the Arduino IDE. 
*Note: Before compiling, be sure to open the file and replace the `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` placeholders with your actual hotspot or router credentials.*

### 2. ESP32-CAM Web Server Setup
The camera module serves as the vision system for the robot. 
Detailed setup instructions for the camera module can be found in `ESP32 Cam Web App/README.md`. 
*Note: Similar to the main controller, remember to replace the placeholder WiFi credentials in `ESP32_CAM_WebServer.ino` before uploading to the ESP32-CAM.*

## Hardware Stack
- ESP32 Microcontroller
- ESP32-CAM Module (AI-Thinker)
- Metal Detector Sensor (connected via GPIO)
- Motor Driver & DC Motors
- Power Supply / Batteries

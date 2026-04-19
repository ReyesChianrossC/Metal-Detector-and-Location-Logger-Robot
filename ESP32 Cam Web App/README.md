# ESP32-CAM Direct Web Server

A simple video streaming system where the ESP32-CAM serves HTML/CSS directly and streams video via MJPEG. No external server needed!

## System Architecture

```
[ESP32-CAM Web Server] ←WiFi→ [Your Browser]
```

The ESP32-CAM runs its own web server, serves the HTML interface, and streams video directly to your browser. Everything runs on the ESP32-CAM itself!

## Features

- ✅ **Direct web server** - No Node.js or laptop server needed
- ✅ **Embedded HTML/CSS** - Interface served from ESP32-CAM
- ✅ **MJPEG video streaming** - Simple and reliable
- ✅ **Local network operation** - Works with laptop hotspot
- ✅ **Multiple simultaneous viewers** - Multiple browsers can connect
- ✅ **Customizable interface** - Modify HTML/CSS in Arduino code
- ✅ **Montserrat font** - Modern typography
- ✅ **Responsive design** - Using relative units (rem, em, %)

## Hardware Requirements

- **ESP32-CAM Module** (AI-Thinker)
- **OV2640 Camera** (usually included with ESP32-CAM)
- **FTDI Programmer** or **ESP32-CAM-MB** (for uploading code)
- **5V Power Supply** (at least 500mA recommended)
- **Laptop with WiFi hotspot capability** (or any WiFi router)

## Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) (1.8.19 or later)
- ESP32 Board Support Package
- No external libraries required! (Uses built-in ESP32 libraries)

## Installation & Setup

### Step 1: Install Arduino IDE and ESP32 Support

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software)

2. Add ESP32 board support:
   - Open Arduino IDE
   - Go to `File` → `Preferences`
   - In "Additional Board Manager URLs", add:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to `Tools` → `Board` → `Boards Manager`
   - Search for "esp32" and install "esp32 by Espressif Systems"

### Step 2: Configure ESP32-CAM Code

1. Open `ESP32_CAM_WebServer/ESP32_CAM_WebServer.ino` in Arduino IDE

2. Update WiFi credentials (lines 17-18):
   ```cpp
   const char* ssid = "YourLaptopHotspotName";
   const char* password = "YourHotspotPassword";
   ```

3. Select board settings:
   - `Tools` → `Board` → `ESP32 Arduino` → `AI Thinker ESP32-CAM`
   - `Tools` → `Port` → Select your FTDI/USB port
   - `Tools` → `Upload Speed` → `115200`
   - `Tools` → `Partition Scheme` → `Huge APP (3MB No OTA/1MB SPIFFS)`

4. Upload the code:
   - Connect GPIO0 to GND (put ESP32-CAM in programming mode)
   - Click Upload button
   - Wait for "Hard resetting via RTS pin..." message
   - Disconnect GPIO0 from GND
   - Press RESET button on ESP32-CAM

## Usage

### 1. Start Your Laptop Hotspot

**Windows:**
- Go to `Settings` → `Network & Internet` → `Mobile hotspot`
- Turn on "Share my Internet connection"
- Note the network name and password

**macOS:**
- Go to `System Preferences` → `Sharing`
- Enable "Internet Sharing"

**Linux:**
- Use Network Manager or command line to create a hotspot

### 2. Power On ESP32-CAM

- Connect 5V power to ESP32-CAM (use proper power supply, not FTDI)
- Open Serial Monitor (115200 baud) to see the IP address
- The ESP32-CAM will automatically connect to your hotspot

You should see output like:
```
ESP32-CAM Direct Web Server
============================
Initializing camera...
Camera initialized successfully
Connecting to WiFi: YourHotspot
....
WiFi connected!
IP address: 192.168.137.45
Signal strength (RSSI): -45 dBm

Web server started!
============================
Open browser and go to: http://192.168.137.45
============================
```

### 3. View the Stream

Open your web browser and go to the IP address shown in Serial Monitor:
```
http://192.168.137.45
```
(Your IP address will be different)

You should see:
- Live video stream from ESP32-CAM
- Clean, minimal interface
- Streaming status indicator

## Customizing the HTML/CSS Interface

Since you mentioned you'll build the UI later, here's how to customize it:

### Where to Edit

Open `ESP32_CAM_WebServer.ino` and find the HTML section (around line 30):

```cpp
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <!-- Your custom HTML here -->
  <style>
    /* Your custom CSS here */
  </style>
</html>
)rawliteral";
```

### Key Points for Customization

1. **Video stream element**: The `<img>` tag with `src="/stream"` displays the video
   ```html
   <img src="/stream" alt="Live Stream">
   ```

2. **CSS is embedded**: All styles go in the `<style>` tag

3. **Montserrat font**: Already linked in the template

4. **Relative units**: Template uses rem, em, % throughout

5. **Stream endpoint**: The video always comes from `/stream` (don't change this)

### Example: Minimal Custom Interface

```cpp
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>My Custom ESP32-CAM</title>
    <style>
        body { 
            margin: 0; 
            background: #000; 
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        img { 
            max-width: 100%; 
            height: auto; 
        }
    </style>
</head>
<body>
    <img src="/stream">
</body>
</html>
)rawliteral";
```

## Troubleshooting

### ESP32-CAM Won't Connect to WiFi

1. **Check credentials**: Verify SSID and password are correct
2. **Check hotspot**: Make sure laptop hotspot is ON and visible
3. **Check distance**: ESP32-CAM should be close to laptop
4. **Serial Monitor**: Open at 115200 baud to see error messages
5. **Power supply**: Use adequate 5V power (not USB from FTDI)

### Can't See IP Address

1. **Open Serial Monitor**: Set to 115200 baud
2. **Press RESET**: Reset ESP32-CAM and watch Serial Monitor
3. **Check USB connection**: Make sure FTDI/USB is connected properly

### No Video in Browser

1. **Check IP address**: Make sure you're using the correct IP from Serial Monitor
2. **Check same network**: Browser must be on same WiFi/hotspot as ESP32-CAM
3. **Try /stream directly**: Go to `http://YOUR_IP/stream` to see raw stream
4. **Check camera**: Serial Monitor should show "Camera initialized successfully"
5. **Try different browser**: Chrome/Edge recommended

### Poor Video Quality or Low FPS

1. **Adjust JPEG quality**: Change `JPEG_QUALITY` in line 23 (lower = better quality)
   ```cpp
   #define JPEG_QUALITY 10  // Try values from 5-20
   ```

2. **Change resolution**: Modify `FRAME_SIZE` in line 22
   ```cpp
   FRAMESIZE_QVGA   // 320x240 - Fastest
   FRAMESIZE_VGA    // 640x480 - Balanced (default)
   FRAMESIZE_SVGA   // 800x600
   FRAMESIZE_XGA    // 1024x768 - Best quality
   ```

3. **Adjust frame rate**: Change delay in `handleStream()` function (line 316)
   ```cpp
   delay(30);  // Lower = faster FPS (try 10-100)
   ```

4. **Check WiFi signal**: Move ESP32-CAM closer to laptop
5. **Power supply**: Ensure stable 5V supply with adequate current

### Camera Initialization Failed

1. **Check camera module**: Ensure OV2640 is properly connected
2. **Check ribbon cable**: Make sure it's fully inserted and not damaged
3. **Try another module**: Camera module might be defective
4. **Check PSRAM**: Some modules may not have PSRAM

### "Brownout detector was triggered"

This means insufficient power:
- Use external 5V power supply (at least 500mA)
- Don't power from FTDI USB
- Use good quality power adapter
- Add capacitor (100-220µF) across power pins

## Configuration Options

### Camera Settings

Edit `ESP32_CAM_WebServer.ino`:

```cpp
// Resolution (line 22)
FRAMESIZE_QVGA   // 320x240 - Fastest
FRAMESIZE_VGA    // 640x480 - Balanced (default)
FRAMESIZE_SVGA   // 800x600
FRAMESIZE_XGA    // 1024x768 - Best quality

// JPEG Quality (line 23)
// Range: 0-63, lower = better quality, larger files
#define JPEG_QUALITY 10

// Frame rate (line 316 in handleStream function)
delay(30);  // Lower = faster FPS
```

### WiFi Settings

```cpp
const char* ssid = "YourNetwork";
const char* password = "YourPassword";
```

### Web Server Port

Default is port 80 (HTTP standard). To change:

```cpp
WebServer server(8080);  // Use port 8080 instead
```

Then access with: `http://192.168.137.45:8080`

## Project Structure

```
THESES/
├── ESP32_CAM_WebServer/
│   ├── ESP32_CAM_WebServer.ino    # Main Arduino sketch (includes HTML/CSS)
│   └── camera_pins.h              # Pin definitions for AI-Thinker
│
└── README.md                      # This file
```

## Technical Details

### How It Works

1. **ESP32-CAM boots** and connects to WiFi
2. **Web server starts** on port 80
3. **Browser requests** root page (`/`)
4. **ESP32 serves** HTML from program memory
5. **HTML loads** and requests video stream (`/stream`)
6. **ESP32 captures** frames and sends as MJPEG
7. **Browser displays** continuous video stream

### Endpoints

- `http://IP_ADDRESS/` - Main HTML interface
- `http://IP_ADDRESS/stream` - Raw MJPEG video stream

### MJPEG Format

- **Content-Type**: `multipart/x-mixed-replace; boundary=frame`
- Each frame is a complete JPEG image
- Frames are separated by boundary markers
- Simple and widely supported by browsers

### Memory Usage

- HTML/CSS stored in **PROGMEM** (flash memory, not RAM)
- Camera buffers use **PSRAM** (if available)
- Minimal RAM usage for web server
- No large string operations in RAM

## Performance Tips

1. **WiFi Signal**: Keep ESP32-CAM close to laptop/router for best performance
2. **Power Supply**: Use quality 5V power supply (500mA+ recommended)
3. **Resolution**: Lower resolution = higher frame rate
4. **Quality**: Adjust JPEG quality for balance between size and quality
5. **Frame Rate**: Modify delay to control FPS vs stability
6. **Multiple Viewers**: ESP32 can handle 2-4 simultaneous streams (depends on settings)

## Advantages of This Approach

✅ **Simple** - Everything runs on ESP32-CAM  
✅ **No server needed** - No laptop/cloud server required  
✅ **Portable** - Just power and WiFi needed  
✅ **Low latency** - Direct connection to camera  
✅ **Easy to customize** - Edit HTML/CSS in Arduino code  
✅ **No dependencies** - Uses built-in ESP32 libraries  

## Next Steps - Building Your UI

When you're ready to build your custom UI:

1. **Design your interface** in HTML/CSS
2. **Test locally** in a browser
3. **Copy into Arduino code** (the `index_html` section)
4. **Keep the stream element**: `<img src="/stream">`
5. **Upload and test** on ESP32-CAM

Tips:
- Keep HTML small (limited flash memory)
- Inline CSS and JavaScript
- Use CDN for fonts/libraries
- Test on mobile devices too

## Credits & License

This project demonstrates ESP32-CAM direct web serving for educational purposes.

**Hardware**: AI-Thinker ESP32-CAM with OV2640  
**Software**: Arduino ESP32, built-in WebServer library  
**License**: MIT - Feel free to use and modify for your projects

---

**Happy Streaming! 📹**

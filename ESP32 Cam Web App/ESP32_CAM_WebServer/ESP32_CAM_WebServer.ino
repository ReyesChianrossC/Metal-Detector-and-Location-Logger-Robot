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
#include "esp_camera.h"
#include "camera_pins.h"

// ======================== CONFIGURATION ========================
// WiFi credentials - Change these to your laptop hotspot settings
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Camera settings
#define CAMERA_MODEL_AI_THINKER
#define FRAME_SIZE FRAMESIZE_VGA  // Options: QVGA, VGA, SVGA, XGA
#define JPEG_QUALITY 10            // 0-63, lower means higher quality
// ===============================================================

WebServer server(80);

// Forward declarations
void handleRoot();
void handleStream();
void handleNotFound();

// HTML page with embedded CSS
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-CAM Live Stream</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Montserrat:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary-color: #2c3e50;
            --secondary-color: #3498db;
            --success-color: #27ae60;
            --bg-color: #ecf0f1;
            --card-bg: #ffffff;
            --text-primary: #2c3e50;
            --text-secondary: #7f8c8d;
            --shadow: 0 0.125rem 0.5rem rgba(0, 0, 0, 0.1);
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Montserrat', sans-serif;
            background-color: var(--bg-color);
            color: var(--text-primary);
            line-height: 1.6;
            font-size: 1rem;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            padding: 2rem 1rem;
        }

        .container {
            width: 90%;
            max-width: 75rem;
            margin: 0 auto;
        }

        .header {
            text-align: center;
            margin-bottom: 2rem;
        }

        .header h1 {
            font-size: 2.5rem;
            font-weight: 700;
            color: var(--primary-color);
            margin-bottom: 0.5rem;
        }

        .subtitle {
            font-size: 1.125rem;
            font-weight: 400;
            color: var(--text-secondary);
        }

        .main-content {
            background: var(--card-bg);
            border-radius: 1rem;
            padding: 2rem;
            box-shadow: var(--shadow);
        }

        .status-bar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1rem;
            background: var(--bg-color);
            border-radius: 0.5rem;
            margin-bottom: 1.5rem;
            flex-wrap: wrap;
            gap: 1rem;
        }

        .status-indicator {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .status-dot {
            width: 0.75rem;
            height: 0.75rem;
            border-radius: 50%;
            background: var(--success-color);
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .status-text {
            font-weight: 500;
            font-size: 1rem;
        }

        .stream-container {
            position: relative;
            width: 100%;
            background: #000;
            border-radius: 0.5rem;
            overflow: hidden;
            min-height: 30rem;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        #videoStream {
            width: 100%;
            height: auto;
            display: block;
            object-fit: contain;
        }

        .info-panel {
            background: var(--bg-color);
            border-radius: 0.5rem;
            padding: 1.5rem;
            margin-top: 1.5rem;
        }

        .info-panel h3 {
            font-size: 1.25rem;
            font-weight: 600;
            margin-bottom: 1rem;
            color: var(--primary-color);
        }

        .info-text {
            color: var(--text-secondary);
            font-size: 0.875rem;
            line-height: 1.8;
        }

        .footer {
            text-align: center;
            margin-top: 2rem;
            padding: 1rem;
            color: var(--text-secondary);
            font-size: 0.875rem;
        }

        @media (max-width: 48rem) {
            .header h1 { font-size: 2rem; }
            .container { width: 95%; }
            .main-content { padding: 1.5rem; }
            .stream-container { min-height: 20rem; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header class="header">
            <h1>ESP32-CAM Live Stream</h1>
            <p class="subtitle">Direct MJPEG Video Streaming</p>
        </header>

        <main class="main-content">
            <div class="status-bar">
                <div class="status-indicator">
                    <span class="status-dot"></span>
                    <span class="status-text">Streaming Active</span>
                </div>
                <div class="status-text" id="ipAddress">ESP32-CAM</div>
            </div>

            <div class="stream-container">
                <img id="videoStream" src="/stream" alt="ESP32-CAM Live Stream">
            </div>

            <div class="info-panel">
                <h3>Customize Your UI</h3>
                <p class="info-text">
                    This is a minimal web interface served directly from the ESP32-CAM.
                    You can customize the HTML and CSS in the Arduino code to build your own UI.
                    The video stream is served via MJPEG at /stream endpoint.
                </p>
            </div>
        </main>

        <footer class="footer">
            <p>ESP32-CAM Direct Web Server | Build your custom interface here</p>
        </footer>
    </div>
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
  server.send_P(200, "text/html", index_html);
}

// Handle MJPEG stream
void handleStream() {
  Serial.println("Stream requested");
  
  WiFiClient client = server.client();
  
  // MJPEG response header
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  // Stream frames
  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(100);
      continue;
    }
    
    // Send MJPEG frame
    client.printf("--frame\r\n");
    client.printf("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.printf("\r\n");
    
    esp_camera_fb_return(fb);
    
    // Small delay to control frame rate
    delay(30); // ~33 FPS max
  }
  
  Serial.println("Stream client disconnected");
}

// Handle 404
void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);
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
  server.handleClient();
}


/*
 * G.E.M.S. Interface JavaScript
 * Metal Detection System - Real-time updates and animations
 */

class GemsInterface {
    constructor() {
        this.elements = {
            time: document.getElementById('currentTime'),
            fps: document.getElementById('fpsValue'),
            resolution: document.getElementById('resValue'),
            latency: document.getElementById('latency'),
            videoStream: document.getElementById('videoStream'),
            xCoord: document.getElementById('xCoord'),
            yCoord: document.getElementById('yCoord'),
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
        this.lastDetectionCount = 0;  // Track ESP32's detection count
        this.pollingInterval = null;  // Store polling interval ID
        
        // Add initial reference point at origin
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
        this.startMetalDetectionPolling();  // Start polling ESP32
        
        // Boot sequence
        setTimeout(() => {
            console.log('G.E.M.S. Online - Metal Detection Ready');
            this.showNotification('System initialized successfully', 'success', 'SYSTEM READY');
        }, 1000);
    }

    // Real-time clock
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

    // Simulate metrics updates
    startMetricsUpdate() {
        setInterval(() => {
            // Simulate FPS (28-32)
            const fps = Math.floor(Math.random() * 5) + 28;
            if (this.elements.fps) {
                this.elements.fps.textContent = fps;
            }

            // Simulate latency (20-30ms)
            const latency = Math.floor(Math.random() * 11) + 20;
            if (this.elements.latency) {
                this.elements.latency.textContent = `${latency}ms`;
            }
        }, 1500);
    }

    // Initialize video stream
    initializeStream() {
        if (this.elements.videoStream) {
            this.elements.videoStream.addEventListener('load', () => {
                const width = this.elements.videoStream.naturalWidth;
                const height = this.elements.videoStream.naturalHeight;
                if (this.elements.resolution && width && height) {
                    this.elements.resolution.textContent = `${width}x${height}`;
                }
            });

            // For ESP32: Change src to "/stream"
            // this.elements.videoStream.src = "http://10.151.89.49/stream";
        }
    }

    // Setup button handlers
    setupButtonHandlers() {
        const buttons = document.querySelectorAll('.hud-button');
        buttons.forEach((btn, index) => {
            btn.addEventListener('click', () => {
                this.handleButtonClick(btn.querySelector('.btn-text').textContent);
            });
        });
    }

    handleButtonClick(action) {
        console.log(`[G.E.M.S.] Action: ${action}`);
        
        // Add your ESP32 API calls here
        switch(action) {
            case 'CAPTURE':
                console.log('Capturing image...');
                // fetch('/api/capture').then(...)
                this.showNotification('Image captured successfully', 'success', 'CAPTURE');
                break;
            case 'RECORD':
                console.log('Recording toggled');
                // fetch('/api/record').then(...)
                this.showNotification('Recording started', 'info', 'RECORD');
                break;
            case 'FLASH':
                console.log('Flash toggled');
                // fetch('/api/flash').then(...)
                this.showNotification('Flash activated', 'warning', 'FLASH');
                break;
            case 'ZOOM':
                console.log('Zoom activated');
                // fetch('/api/zoom').then(...)
                this.showNotification('Zoom enabled', 'info', 'ZOOM');
                break;
        }
    }

    showNotification(message, type = 'info', title = 'G.E.M.S.') {
        console.log(`[NOTIFICATION] ${message}`);
        
        const container = document.getElementById('notificationContainer');
        if (!container) return;

        // Create notification element
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        
        notification.innerHTML = `
            <div class="notification-content">
                <div class="notification-title">${title}</div>
                <div class="notification-message">${message}</div>
            </div>
            <button class="notification-close" onclick="this.parentElement.remove()">×</button>
        `;
        
        // Add to container
        container.appendChild(notification);
        
        // Trigger animation
        setTimeout(() => notification.classList.add('show'), 10);
        
        // Auto remove after 3 seconds
        setTimeout(() => {
            notification.classList.add('hide');
            setTimeout(() => notification.remove(), 300);
        }, 3000);
    }

    // API communication with ESP32
    async fetchAPI(endpoint, method = 'GET', data = null) {
        try {
            const options = {
                method,
                headers: { 'Content-Type': 'application/json' }
            };
            
            if (data) {
                options.body = JSON.stringify(data);
            }
            
            const response = await fetch(endpoint, options);
            return await response.json();
        } catch (error) {
            console.error('API Error:', error);
            return null;
        }
    }

    // Detection handlers
    setupDetectionHandlers() {
        // Scan button
        if (this.elements.scanBtn) {
            this.elements.scanBtn.addEventListener('click', () => {
                this.toggleScanning();
            });
        }

        // Simulate detection button
        if (this.elements.simulateBtn) {
            this.elements.simulateBtn.addEventListener('click', () => {
                this.simulateMetalDetection();
            });
        }

        // Clear data button
        if (this.elements.clearBtn) {
            this.elements.clearBtn.addEventListener('click', () => {
                this.clearDetectionData();
            });
        }
    }

    toggleScanning() {
        this.isScanning = !this.isScanning;
        
        if (this.isScanning) {
            // Start scanning
            this.elements.scanMode.textContent = 'ACTIVE';
            this.elements.scanMode.classList.add('green');
            this.elements.detectionIndicator.classList.add('scanning');
            this.elements.detectionText.textContent = 'SCANNING...';
            this.elements.scanBtn.querySelector('.btn-text').textContent = 'STOP SCAN';
            this.showNotification('Metal detection scan started', 'info', 'SCAN ACTIVE');
        } else {
            // Stop scanning
            this.elements.scanMode.textContent = 'STANDBY';
            this.elements.scanMode.classList.remove('green');
            this.elements.detectionIndicator.classList.remove('scanning');
            this.elements.detectionText.textContent = 'STANDBY';
            this.elements.scanBtn.querySelector('.btn-text').textContent = 'START SCAN';
            this.showNotification('Metal detection scan stopped', 'info', 'SCAN STOPPED');
        }
    }

    simulateMetalDetection() {
        // Dummy data: Metal detected at x: 97, y: -12
        const detection = {
            x: 97,
            y: -12,
            confidence: 94.7
        };

        this.displayDetection(detection);
    }

    displayDetection(detection) {
        // Update detection count
        this.detectionCount++;
        this.elements.detectCount.textContent = this.detectionCount;

        // Update coordinates
        this.elements.xCoord.textContent = detection.x;
        this.elements.yCoord.textContent = detection.y;
        this.elements.confidence.textContent = `${detection.confidence}%`;

        // Update status
        this.elements.detectionIndicator.classList.remove('scanning');
        this.elements.detectionIndicator.classList.add('detected');
        this.elements.detectionText.textContent = 'METAL DETECTED!';
        this.elements.detectionText.classList.add('detected');

        // Show notification
        this.showNotification(
            `Metal detected at X: ${detection.x}, Y: ${detection.y} | Confidence: ${detection.confidence}%`,
            'error',
            'METAL DETECTED!'
        );

        // Log to console
        console.log(`[DETECTION] X: ${detection.x}, Y: ${detection.y}, Confidence: ${detection.confidence}%`);

        // Reset status after 3 seconds
        setTimeout(() => {
            if (this.isScanning) {
                this.elements.detectionIndicator.classList.remove('detected');
                this.elements.detectionIndicator.classList.add('scanning');
                this.elements.detectionText.textContent = 'SCANNING...';
                this.elements.detectionText.classList.remove('detected');
            } else {
                this.elements.detectionIndicator.classList.remove('detected');
                this.elements.detectionText.textContent = 'STANDBY';
                this.elements.detectionText.classList.remove('detected');
            }
        }, 3000);
    }

    clearDetectionData() {
        this.detectionCount = 0;
        this.elements.detectCount.textContent = '0';
        this.elements.xCoord.textContent = '--';
        this.elements.yCoord.textContent = '--';
        this.elements.confidence.textContent = '--';
        this.showNotification('Detection data cleared', 'info', 'DATA CLEARED');
    }

    setupTestButton() {
        const testBtn = document.getElementById('testButton');
        if (testBtn) {
            testBtn.addEventListener('click', () => {
                // Generate random coordinates
                const x = Math.floor(Math.random() * 200) - 100; // -100 to 99
                const y = Math.floor(Math.random() * 200) - 100; // -100 to 99
                
                this.detectMetal(x, y);
            });
        }
    }

    detectMetal(x, y) {
        // Increment detection count
        this.metalDetectionCount++;
        
        // Add to log
        const timestamp = new Date().toLocaleTimeString();
        this.detectionLog.push({
            count: this.metalDetectionCount,
            time: timestamp,
            x: x,
            y: y
        });
        
        // Trigger gold flash effect
        this.triggerMetalDetectedFlash();
        
        // Trigger fast scanning animation
        this.triggerFastScan();
        
        // Trigger metal detection notification
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
        
        // Clear any existing timeouts to restart the animation
        this.scanTimeouts.forEach(timeout => clearTimeout(timeout));
        this.scanTimeouts = [];
        
        // Remove any existing classes
        scannerLine.className = 'scanner-line';
        
        // Start fast scan
        scannerLine.classList.add('fast-scan');
        
        // Gradually slow down over 8 seconds
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
            // Returns to normal speed (3s)
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
        
        // Close on backdrop click
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
        
        // Clear table
        tableBody.innerHTML = '';
        
        // Populate table with detection log
        if (this.detectionLog.length > 0) {
            // Show table, hide empty message
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
            // Hide table, show empty message
            if (logTable) logTable.style.display = 'none';
            if (logEmpty) logEmpty.style.display = 'block';
        }
        
        // Show popup
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
        // Reset detection count
        this.metalDetectionCount = 0;
        
        // Clear detection log
        this.detectionLog = [];
        
        // Re-add initial reference point
        this.addInitialReferencePoint();
        
        // Show notification
        this.showNotification(
            'Detection log has been cleared.\nOrigin point reset to (0, 0)',
            'info',
            'SYSTEM RESET'
        );
        
        console.log('[RESET] Detection log cleared and reset to origin');
    }

    triggerMetalDetectedFlash() {
        const goldFlash = document.getElementById('goldFlash');
        if (goldFlash) {
            // Instantly show gold flash
            goldFlash.classList.add('active');
            
            // Remove active class to trigger smooth fade out
            setTimeout(() => {
                goldFlash.classList.remove('active');
            }, 50);
        }
    }
    
    // ================= METAL DETECTION POLLING =================
    // Poll ESP32 /metal-status endpoint every 150ms (within 100-150ms range)
    async startMetalDetectionPolling() {
        // First, sync with ESP32's current count to avoid missing detections after page refresh
        try {
            const response = await fetch('/metal-status?t=' + Date.now());
            if (response.ok) {
                const data = await response.json();
                this.lastDetectionCount = data.count;
                console.log(`[G.E.M.S.] Synced with ESP32 - current detection count: ${data.count}`);
                
                // Update UI with current detection data if metal is currently detected
                if (data.detected) {
                    this.updateDetectionUI(data);
                } else {
                    this.updateScanningUI();
                }
            }
        } catch (error) {
            console.error('[G.E.M.S.] Error syncing with ESP32:', error);
        }
        
        // Poll ESP32 for metal detection status every 150ms
        this.pollingInterval = setInterval(() => {
            this.checkMetalDetection();
        }, 150);
        console.log('[G.E.M.S.] Metal detection polling started (150ms interval)');
    }
    
    // Check for new metal detections (primary trigger: count increase)
    async checkMetalDetection() {
        try {
            const response = await fetch('/metal-status?t=' + Date.now());
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            const data = await response.json();
            
            // PRIMARY TRIGGER: Check if count increased (most reliable indicator)
            if (data.count > this.lastDetectionCount) {
                // NEW DETECTION EVENT - Count increased
                const newDetections = data.count - this.lastDetectionCount;
                console.log(`[DETECTION] Count increased from ${this.lastDetectionCount} to ${data.count} (+${newDetections})`);
                
                // Update tracking
                this.lastDetectionCount = data.count;
                this.metalDetectionCount = data.count;
                
                // Update UI fields (X, Y, Confidence)
                this.updateDetectionUI(data);
                
                // Log the detection
                const timestamp = new Date().toLocaleTimeString();
                this.detectionLog.push({
                    count: data.count,
                    time: timestamp,
                    x: data.x,
                    y: data.y
                });
                
                // Trigger visual effects
                this.triggerMetalDetectedFlash();
                this.triggerFastScan();
                
                // Show notification (slide-in)
                this.showNotification(
                    `Metal detected at X: ${data.x.toFixed(2)}, Y: ${data.y.toFixed(2)}\nConfidence: ${data.confidence.toFixed(1)}% | Detection #${data.count}`,
                    'error',
                    '🚨 METAL DETECTED!'
                );
                
                console.log(`[NOTIFICATION SHOWN] Detection #${data.count} at (${data.x.toFixed(2)}, ${data.y.toFixed(2)})`);
            }
            
            // SECONDARY: Update indicator state based on detected flag (for visual feedback)
            // This keeps UI in sync but doesn't trigger notifications
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
            } else {
                // Metal no longer detected - revert to "SCANNING..."
                this.updateScanningUI();
            }
        } catch (error) {
            console.error('[G.E.M.S.] Error polling metal detection:', error);
        }
    }
    
    // Helper: Update UI to show detection state
    updateDetectionUI(data) {
        // Update X, Y, Confidence fields
        if (this.elements.xCoord) this.elements.xCoord.textContent = data.x.toFixed(2);
        if (this.elements.yCoord) this.elements.yCoord.textContent = data.y.toFixed(2);
        if (this.elements.confidence) this.elements.confidence.textContent = `${data.confidence.toFixed(1)}%`;
        
        // Update visual indicator to "METAL DETECTED"
        if (this.elements.detectionIndicator) {
            this.elements.detectionIndicator.classList.remove('scanning');
            this.elements.detectionIndicator.classList.add('detected');
        }
        if (this.elements.detectionText) {
            this.elements.detectionText.textContent = 'METAL DETECTED!';
            this.elements.detectionText.classList.add('detected');
        }
    }
    
    // Helper: Update UI to show scanning state
    updateScanningUI() {
        // Revert to "SCANNING..." when detected === false
        if (this.elements.detectionIndicator && this.elements.detectionIndicator.classList.contains('detected')) {
            this.elements.detectionIndicator.classList.remove('detected');
            this.elements.detectionIndicator.classList.add('scanning');
        }
        if (this.elements.detectionText && this.elements.detectionText.classList.contains('detected')) {
            this.elements.detectionText.textContent = 'SCANNING...';
            this.elements.detectionText.classList.remove('detected');
        }
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    console.log('═══════════════════════════════════════');
    console.log('       G.E.M.S. SYSTEM BOOT            ');
    console.log('    Metal Detection System v1.0        ');
    console.log('═══════════════════════════════════════');
    
    const gems = new GemsInterface();
    
    // Make globally accessible for debugging
    window.GEMS = gems;
    
    // Keyboard shortcuts
    document.addEventListener('keydown', (e) => {
        switch(e.key.toLowerCase()) {
            case 'c':
                gems.handleButtonClick('CAPTURE');
                break;
            case 'r':
                gems.handleButtonClick('RECORD');
                break;
            case 'f':
                gems.handleButtonClick('FLASH');
                break;
            case 'z':
                gems.handleButtonClick('ZOOM');
                break;
        }
    });
    
    console.log('[G.E.M.S.] Keyboard shortcuts enabled: C, R, F, Z');
    console.log('[G.E.M.S.] Metal Detection System ready');
});

// Sound effects (optional - uncomment to enable)
/*
function playSound(frequency, duration) {
    const audioContext = new (window.AudioContext || window.webkitAudioContext)();
    const oscillator = audioContext.createOscillator();
    const gainNode = audioContext.createGain();
    
    oscillator.connect(gainNode);
    gainNode.connect(audioContext.destination);
    
    oscillator.frequency.value = frequency;
    oscillator.type = 'sine';
    
    gainNode.gain.setValueAtTime(0.3, audioContext.currentTime);
    gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + duration);
    
    oscillator.start(audioContext.currentTime);
    oscillator.stop(audioContext.currentTime + duration);
}
*/

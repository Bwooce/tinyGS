import serial
import time
import sys

PORT = '/dev/ttyACM0'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    
    print(f"Opening {PORT} at {BAUD}...")
    
    # Reset procedure for ESP32
    ser.dtr = False
    ser.rts = False
    time.sleep(0.1)
    
    print("Resetting device...")
    ser.dtr = False
    ser.rts = True  # RTS low (True in pyserial logic for active low lines often inverted, but let's try standard)
    # Standard ESP32 Reset: RTS low (True), DTR high (False) -> Reset
    # Bootloader: RTS low (True), DTR low (True) -> GPIO0 low
    
    # Let's try the standard esptool sequence:
    # dtr=False (High), rts=True (Low) -> Reset active
    ser.dtr = False 
    ser.rts = True
    time.sleep(0.1)
    
    # dtr=False (High), rts=False (High) -> Reset release
    ser.rts = False
    
    print("Monitoring... (Press Ctrl+C to stop, auto-stop in 20s)")
    
    start_time = time.time()
    while time.time() - start_time < 20:
        if ser.in_waiting:
            try:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                print(line)
            except Exception as e:
                print(f"Error reading line: {e}")
        time.sleep(0.01)

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()

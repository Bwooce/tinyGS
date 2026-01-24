import serial
import time
import sys

# Get port and baud from arguments if provided
PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyACM0'
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    
    print(f"Opening {PORT} at {BAUD}...")
    
    # Reset procedure for ESP32
    ser.dtr = False
    ser.rts = False
    time.sleep(0.1)
    
    print("Resetting device...")
    # Standard ESP32 Reset logic for many serial adapters:
    # EN (Reset) connected to RTS
    # GPIO0 connected to DTR
    ser.dtr = False 
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    
    print("Monitoring... (Press Ctrl+C to stop, auto-stop in 40s)")
    
    start_time = time.time()
    while time.time() - start_time < 40:
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
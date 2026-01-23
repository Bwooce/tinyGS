import serial
import time
import sys

PORT = '/dev/ttyACM0'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    
    print(f"Opening {PORT} at {BAUD}...")
    # No reset this time, just listen
    
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

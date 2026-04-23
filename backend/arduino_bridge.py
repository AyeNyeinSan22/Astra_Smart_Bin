import argparse
import threading
import time
from typing import Optional

from flask import Flask, jsonify, request
import serial
from serial.tools import list_ports


app = Flask(__name__)

serial_lock = threading.Lock()
levels_lock = threading.Lock()

serial_conn: Optional[serial.Serial] = None
latest_levels = {"plastic": 0, "paper": 0, "metal": 0, "raw": ""}

# Mapping from AI labels/bin names to Arduino commands
BIN_MAP = {
    "plastic": "P",
    "paper": "S",
    "metal": "M"
}


def _find_default_port() -> Optional[str]:
    ports = list(list_ports.comports())
    if not ports:
        return None
    return ports[0].device


def _open_serial(port: str, baudrate: int) -> serial.Serial:
    print(f"--- Attempting to open Serial Port: {port} at {baudrate} baud ---")
    return serial.Serial(port=port, baudrate=baudrate, timeout=1)


def serial_reader(port: str, baudrate: int) -> None:
    global serial_conn

    while True:
        try:
            with serial_lock:
                if serial_conn is None or not serial_conn.is_open:
                    serial_conn = _open_serial(port, baudrate)
                    print(">>> SUCCESS: Arduino Connected! <<<")

            line = serial_conn.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            
            print(f"Serial Input: {line}") # Debug print

            with levels_lock:
                latest_levels["raw"] = line

            if line.startswith("DATA|"):
                parts = line.split("|")
                with levels_lock:
                    try:
                        # Dynamically update levels based on how many parts the Arduino sends
                        if len(parts) >= 2:
                            latest_levels["plastic"] = int(parts[1])
                        if len(parts) >= 3:
                            latest_levels["paper"] = int(parts[2])
                        if len(parts) >= 4:
                            latest_levels["metal"] = int(parts[3])
                    except (ValueError, IndexError):
                        continue
        except Exception as e:
            print(f"!!! SERIAL ERROR: {e} !!!")
            with serial_lock:
                if serial_conn is not None and serial_conn.is_open:
                    serial_conn.close()
                serial_conn = None
            time.sleep(2)


@app.get("/api/bin-status")
def get_bin_status():
    with levels_lock:
        return jsonify(dict(latest_levels))


@app.post("/api/command")
def post_command():
    payload = request.get_json(silent=True) or {}
    command = payload.get("command")

    if command not in {"P", "S", "M"}:
        return jsonify({"error": "Invalid command. Use one of: P, S, M"}), 400

    success = _send_to_arduino(command)
    if not success:
        return jsonify({"error": "Arduino serial connection is not available"}), 503

    return jsonify({"ok": True, "command": command})


@app.post("/open_bin")
def open_bin():
    """Endpoint specifically for the AI Vision Detector"""
    payload = request.get_json(silent=True) or {}
    bin_name = payload.get("bin", "").lower()
    
    # Extract just the keyword if label is complex (e.g., 'plastic_bottle')
    command = None
    for key, val in BIN_MAP.items():
        if key in bin_name:
            command = val
            break
    
    if not command:
        return jsonify({"error": f"Unknown bin type: {bin_name}", "message": "Failed"}), 400

    if _send_to_arduino(command):
        return jsonify({"ok": True, "message": f"Opening {bin_name} bin"})
    else:
        return jsonify({"error": "Arduino unavailable", "message": "Failed"}), 503


def _send_to_arduino(command: str) -> bool:
    with serial_lock:
        if serial_conn is None or not serial_conn.is_open:
            return False
        serial_conn.write(command.encode("utf-8"))
        return True


def main():
    parser = argparse.ArgumentParser(description="Arduino USB to Wi-Fi bridge")
    parser.add_argument("--port", default=None, help="Arduino serial port (e.g. COM3, /dev/ttyACM0)")
    parser.add_argument("--baudrate", type=int, default=9600, help="Serial baud rate")
    parser.add_argument("--host", default="0.0.0.0", help="Flask bind host")
    parser.add_argument("--api-port", type=int, default=5000, help="Flask API port")
    args = parser.parse_args()

    serial_port = args.port or _find_default_port()
    if not serial_port:
        raise SystemExit("No serial ports detected. Please provide --port.")

    threading.Thread(
        target=serial_reader,
        args=(serial_port, args.baudrate),
        daemon=True,
    ).start()

    app.run(host=args.host, port=args.api_port)


if __name__ == "__main__":
    main()

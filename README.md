# Astra Smart Bin

This project detects recyclable waste using a mobile camera and TensorFlow Lite, controlling physical bins via Arduino.

## Project Structure

- `arduino/`: Arduino source code for bin control.
- `backend/`: Python bridge and server logic.
- `vision/`: AI detection scripts and models.
- `mobile/`: Flutter application code.
- `docs/`: Documentation and assets.
- `legacy/`: Backup of old project versions.

## Getting Started

### 1. Requirements
Install Python dependencies:
```bash
pip install -r requirements.txt
```

### 2. Run Backend & Vision
```bash
cd vision
python vision_detector.py
```

### 3. Run Mobile App
```bash
cd mobile
flutter pub get
flutter run
```

### 4. Setup Arduino
Upload the code in `arduino/arduino.ino` to your Arduino board.
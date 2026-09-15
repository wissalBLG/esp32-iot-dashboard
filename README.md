# esp32-iot-dashboard — Meadow Watch

A pet-care monitoring project: an ESP32-S3 reads a water level sensor and a
PIR motion sensor, logs readings to Supabase, and a web dashboard
(deployed on Vercel) shows live status and lets you send feed / refill
commands back to the device.

## Structure

```
firmware/     ESP32-S3 Arduino sketch
index.html    Web dashboard (deployed on Vercel)
```

## Firmware setup

1. Open `firmware/meadow_watch.ino` in the Arduino IDE.
2. Fill in your WiFi credentials and Supabase URL/key at the top of the sketch.
3. Install the required libraries: `RTClib`, `ArduinoJson`.
4. Upload to the ESP32-S3.

## Dashboard

The dashboard (`index.html`) reads the latest sensor reading from Supabase
and lets you send `feed` / `refill_water` commands, which the device picks
up on its next command-check cycle.

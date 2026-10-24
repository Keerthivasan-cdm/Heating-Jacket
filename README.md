# Heat Jacket: Temperature-Controlled Therapeutic Heating Jacket

An ESP32-based wearable heating jacket for targeted back-pain relief and posture support. Four PTC heating pads are regulated by closed-loop temperature control using NTC thermistors. Heat level and vibration are set remotely over Wi-Fi.

Built as a client project with **Sairam Incubation Foundation** (Mar–Jun 2025).

## Features
- **4-channel temperature sensing:** NTC thermistors in voltage dividers, converted with the Beta equation (Beta = 4100, R25 = 18 kΩ).
- **Closed-loop bang-bang control:** each heater relay switches on below the set-point and off at or above it.
- **Safety cutoff:** both relays switch off if any channel reads 60 °C or higher, or reads an invalid value (sensor fault).
- **Independent left/right zones:** two relay channels, each driving a pair of PTC heaters.
- **4 vibration motors** for massage and posture cues, also split into left and right.
- **Wi-Fi remote control:** the ESP32 polls an HTTPS API, sends the average left/right temperatures, and receives JSON commands.

## Hardware
| Part | Details |
|---|---|
| MCU | ESP32 Dev board |
| Temperature sensors | 4 × NTC thermistor (18 kΩ @ 25 °C), 9.8 kΩ series resistor |
| Heaters | PTC heating elements |
| Relays | 2-channel, active-LOW relay module |
| Vibration | 4 × coin vibration motors |

### Pin map
| Signal | GPIO |
|---|---|
| NTC 1–4 (ADC) | 34, 35, 32, 33 |
| Relay 1 (left) / Relay 2 (right) | 4, 5 |
| Vibration motors 1–4 | 22, 18, 23, 27 |

## Command protocol
The device calls `GET <serverUrl>/api?avgLeft=..&avgRight=..` and expects:
```json
{ "status": "success", "command": "tl", "degree": 45 }
```
| Command | Action |
|---|---|
| `tl` / `tr` / `tb` | Heat left / right / both, up to the requested temperature |
| `TL` / `TR` / `TB` | Heat off for left / right / both |
| `vl` / `vr` / `vb` | Vibration on for left / right / both |
| `VL` / `VR` / `VB` | Vibration off for left / right / both |

`degree` sets the target temperature. The firmware adds a +5 °C offset to compensate for sensor placement.

## Build and flash
1. Install the Arduino IDE with the **ESP32 board package**, and the **ArduinoJson** library.
2. Open `ntc_calibirated_ptc_controller.ino`.
3. Set `ssid`, `password` and `serverUrl` at the top of the file.
4. Select your ESP32 board and upload. Serial monitor runs at 115200 baud.

## Testing done
- Calibrated sensors against a reference thermometer. The fixed −46.42 offset in the code comes from this calibration.
- Thermal testing of the heater pads within the 45–60 °C range.
- Demonstrated the working prototype at Sairam Incubation Foundation.

## Author
**Keerthivasan J**, B.E. EEE, Sri Sairam Engineering College ([GitHub](https://github.com/Keerthivasan-cdm) · [LinkedIn](https://linkedin.com/in/keerthi-vasan-j-))

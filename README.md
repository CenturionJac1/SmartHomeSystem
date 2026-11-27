# SmartHomeSystem
Code for a Smart Home System that communicates over HTTP. Controls an outle,t a light fixture, a humidifier, and a plant moisture detector.

# Firmware

ESP32 firmware for the smart lighting, soil moisture sensor, humidifier, and smart outlet. Each firmware module handles its own sensor readings or device outputs. The central controller connects the device nodes to the team dashboard.

## Firmware Modules

| Module | Function | Verification |
| --- | --- | --- |
| Smart lighting | Turns the light on when the PIR sensor detects motion and turns it off after 10 seconds without motion in automatic mode. | Built and flashed to an ESP32-S3. |
| Soil moisture sensor | Reads an analog soil moisture sensor and exposes the raw ADC value through `GET /moisture`. | The endpoint returned JSON in a local test. |
| Humidifier | Separate firmware target for humidifier control. | Build completed. Hardware operation and dashboard integration are pending. |
| Smart outlet | Supports HTTP on/off control, state reporting, and a timed shutoff. | Firmware code is written. Build, hardware testing, and dashboard integration are pending. |

## Smart Lighting

The lighting firmware reads a PIR motion sensor and drives a relay output. In automatic mode, motion turns the light on and resets the inactivity timer. The light turns off after 10 seconds without motion.

| Setting | Default |
| --- | --- |
| PIR sensor output | GPIO 14 |
| Relay control output | GPIO 5 |
| Inactivity timeout | 10 seconds |

## Soil Moisture Sensor

`GET /moisture` returns the raw ADC reading from the sensor.

```json
{"raw":2371}
```

To test the endpoint, use the sensor board's IP address from its serial monitor.

```bash
MOISTURE_IP=172.20.10.2
curl --fail --max-time 5 "http://$MOISTURE_IP/moisture"
```

The dashboard expects `soil.moisture` or `soil.percent` in the range 0–100. Measure dry and wet reference readings for the sensor before converting the raw ADC value to a percentage for the dashboard.

## Humidifier

The humidifier firmware builds as a separate target. Hardware testing and confirmation of the node's control API are pending. The dashboard is prepared to send on/off commands through the central controller and read the result from `humidifier.on` in `/status`.

## Smart Outlet

The outlet ESP32 controls a GPIO output and runs its shutoff timer locally. The firmware sets the output to OFF during initialization. The default pin is **GPIO 5, active high**. Update `OUTLET_GPIO` and `OUTLET_ACTIVE_LEVEL` in `smart-outlet/include/config.h` to match the hardware. If the outlet and lighting run on the same board, assign them separate output pins. Use a low-voltage LED as the demo load.

### Outlet Node API

| Method | Path | Behavior |
| --- | --- | --- |
| `GET` | `/outlet` | Read the output state and remaining timer duration. |
| `POST` | `/outlet?state=on` | Turn on and clear any active timer. |
| `POST` | `/outlet?state=off` | Turn off immediately and clear any active timer. |
| `POST` | `/outlet?state=on&seconds=10` | Turn on and set a 10-second shutoff timer. |

POST requests require the `X-Outlet-Control: 1` header and an empty body. The optional `seconds` parameter accepts whole numbers from 1 to 3,600 for `state=on`.

```json
{"on":true,"remaining_seconds":10}
```

`on` reports the firmware's commanded output state. `remaining_seconds` is `0` when no timer is active.

### Build, Flash, and Test

Activate ESP-IDF and run these commands from the `smart-outlet` directory.

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Set the Wi-Fi SSID and password under **Example Connection Configuration**. Save the configuration, then build and flash.

```bash
idf.py build
idf.py -p /dev/cu.usbserial-10 flash monitor
```

Replace the serial port with the one for your board. Use the IP printed in the serial monitor to test from a computer on the same network.

```bash
OUTLET_IP=192.168.1.104
BASE="http://$OUTLET_IP"

curl --fail "$BASE/outlet"
curl --fail -X POST -H 'X-Outlet-Control: 1' \
  "$BASE/outlet?state=on&seconds=10"
sleep 11
curl --fail "$BASE/outlet"
```

The LED should turn on and then turn off automatically. The final request should return `{"on":false,"remaining_seconds":0}`.

## Central Controller and Dashboard Integration

The dashboard sends `GET /status` and `POST /control` to the same origin that serves the dashboard page. That origin needs to serve those endpoints or forward them to the central controller.

| Owner | Integration responsibility |
| --- | --- |
| Device firmware | Flash and test each node. Provide the central controller team with the node IP address, endpoint paths, and example responses. |
| Central controller | Translate dashboard commands into node requests and combine node states into the dashboard's `/status` response. |

For the smart outlet, the controller needs these mappings.

| Dashboard request | Controller action |
| --- | --- |
| `POST /control` with `{"device":"outlet","state":"on"}` or `"off"` | Forward to the outlet node's `POST /outlet?state=on` or `off` with `X-Outlet-Control: 1`. Return `{"success":true}` to the dashboard after the node accepts the command. |
| `GET /status` | Read the outlet node's `GET /outlet` and include its `on` value as `outlet.on`. Include valid `light.mode` and `light.output` fields so the dashboard can enable the device controls. |

An example dashboard status response is shown below.

```json
{
  "light": {"mode":"auto","output":false},
  "outlet": {"on":true}
}
```

The supplied controller currently returns a `nodes` array from `/status` and accepts `node`, `light`, and `state` JSON fields on `/control`. The dashboard integration still needs the mappings above, along with support for the dashboard's form-encoded `cmd` field for lighting commands. The current dashboard has outlet ON/OFF buttons. The timed shutoff can be tested directly through the outlet node API.

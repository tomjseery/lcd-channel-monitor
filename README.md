# LCD Channel Monitor

An embedded channel-monitoring interface for an Arduino-compatible board and an Adafruit RGB LCD Shield. It receives named channels over serial, displays their live values on a 16×2 LCD and highlights readings that fall outside configured limits.

## Features

- Up to 26 letter-addressed channels (`A`–`Z`)
- Fifteen-character channel descriptions
- Live values in the range 0–255
- Per-channel minimum and maximum thresholds
- Alphabetical display ordering and button navigation
- Separate views for readings below or above their limits
- RGB backlight status indication
- Recent-value history stored as linked lists
- Channel metadata persisted in EEPROM
- Scrolling descriptions, custom arrow characters and a free-memory diagnostic

## Hardware and libraries

- Arduino-compatible board with sufficient SRAM and EEPROM
- Adafruit RGB LCD Shield with a 16×2 display and directional buttons
- `Adafruit_RGBLCDShield`
- `Adafruit_MCP23017`
- Arduino `Wire` and `EEPROM` libraries

## Serial protocol

The sketch communicates at 9,600 baud. Configure the sender to terminate commands with a newline.

At startup the device repeatedly sends `Q`. Reply with a single `X` to complete the handshake. It then reports the implemented capability set.

| Command | Meaning | Example |
| --- | --- | --- |
| `C<channel><description>` | Create/update a channel description | `CATemperature` |
| `V<channel><value>` | Submit a channel value | `VA128` |
| `N<channel><value>` | Set the minimum threshold | `NA20` |
| `X<channel><value>` | Set the maximum threshold | `XA220` |

Descriptions longer than 15 characters are truncated. Invalid commands are returned over serial with an `ERROR` prefix.

## Controls

- **Up/Down:** move through the displayed channel list.
- **Left/Right:** switch between below-minimum, normal and above-maximum views.
- **Select:** access the diagnostic display when held.

## Uploading

1. Install the Adafruit RGB LCD Shield library in the Arduino IDE.
2. Open `EmbeddedSystemsProject.ino`.
3. Select the connected board and serial port.
4. Upload the sketch and connect at 9,600 baud.

## Status

This is a self-contained embedded-systems coursework project. It intentionally uses a single sketch and manual memory management to demonstrate serial protocols, state machines, EEPROM persistence and constrained-device data structures.

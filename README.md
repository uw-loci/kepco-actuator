# Kepco Actuator

Arduino Uno firmware that controls the Kepco and Glassman servo actuators from
one pushbutton.

## Button behavior

The pushbutton uses the Uno's internal pullup resistor, so a press connects D2
to ground. A servo is selected only after the button is released:

- A press shorter than 5 seconds runs the Kepco servo sequence.
- A press of 5 seconds or longer runs the Glassman servo sequence.
- Both press and release transitions must remain stable for 50 ms before they
  are accepted.
- Only one complete servo sequence can run at a time. A release is ignored if
  either servo is in its active-hold or rest-hold phase.

The Kepco sequence moves from 90 degrees to 150 degrees, while the Glassman
sequence moves from 0 degrees to 100 degrees. Each servo holds its active
position for 1 second, returns to its rest position, and remains in its
rest-hold phase for 1 second before the sequence becomes idle.

## Wiring

| Connection | Arduino Uno pin |
| --- | --- |
| Kepco servo signal | D10 |
| Glassman SG90 signal | D9 |
| Pushbutton | D2 and GND |

Power the servos from an appropriately rated external supply and connect the
external supply ground to Arduino ground. Do not power both servos directly
from the Uno's 5 V pin. The manufacturer specifies an external supply for the
[Tower Pro SG90](https://towerpro.com.tw/product/sg90-7/).

## Servo calibration

Calibration and sequence values are defined near the top of `src/main.cpp`.
Kepco and Glassman have separate constants so they can be tested and adjusted
independently.

| Initial value | Kepco | Glassman SG90 |
| --- | ---: | ---: |
| Minimum pulse | 500 us | 1000 us |
| Maximum pulse | 2500 us | 2000 us |
| Configured range | 270 degrees | 150 degrees |
| Rest position | 90 degrees | 0 degrees |
| Active position | 150 degrees | 100 degrees |
| Active hold | 1000 ms | 1000 ms |
| Rest hold | 1000 ms | 1000 ms |

The Glassman pulse limits are conservative starting values and should be
calibrated with the final mechanical installation.

## Build

This is a PlatformIO project targeting an Arduino Uno:

```text
pio run
```

To build and upload to a connected board:

```text
pio run --target upload
```

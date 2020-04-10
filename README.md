# SLEEPINO

[![Build Status](https://jenkins.martinenhome.com/buildStatus/icon?job=sleepino/master)](https://jenkins.martinenhome.com/job/sleepino/job/master/)

This is a Proof Of Concept for power consumption using `main4ino` and `mod4ino` libraries on ESP modules (esp8266 and esp32).


# 3. Contribute

## Hardware

For information, the Board used is [NODEMCU / ESP-01](http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family).

- ESP8266 ESP-12E (see a full [list of variants here](https://www.esp8266.com/wiki/doku.php?id=esp8266-module-family))


## Software

To prepare your development environment first do:

```
./pull_dependencies -l -p
```

The project is a `platformio` project.

### Heads up

When contributing, keep always in mind the best practices: 

- Try not to overuse the heap (only 4K!): prefer static memory allocation rather than dynamic one
- Reuse instances as much as possible

## Upload

To upload the firmware just do: 

```
# upload via USB serial port:

 ./upload -n esp32 -p profiles/generic.prof -f
 ./upload -n esp8266 -p profiles/generic.prof -f

To see the logs:
```
# using minicom:
 sudo minicom -D /dev/ttyUSB0 -b 128000 -z -L -l # + Ctrl+a u  (and Ctrl+a q to exit)

# using platformio serial monitor:
 ./serial_monitor 0

```

## Test

```
./launch_tests
```


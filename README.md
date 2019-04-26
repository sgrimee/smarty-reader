# smarty-reader

Read smart Luxembourgish electricity and gaz counter values with ESP8266.

# Overview

<img src="http://weigu.lu/microcontroller/smartyreader/png/pcb_wemos3.png" alt="drawing" width="200"/>

In Luxembourg, electricty and gas counters are being replaced with smart counters that allow the provider to remotely measure consumption values. A special port is provided to allow the end-user to capture the data, for example for integration with home automation systems.

This work is a fork of the [awesome project by Guy WEILER](http://weigu.lu/microcontroller/smartyreader/) who did all the hard work on both the software and hardware side. Please review [Guy's website](http://weigu.lu/microcontroller/smartyreader/) for technical details on the solution, PCB layout, etc.

# Features

- publish all values from the electricity counter to MQTT in their own topic
- units are published as well in a separate topic for easy of interpretation
- if a gas counter is connected to the electricity counter, gas volume is published as well (implemented but not tested yet)
- possibility to work without a counter from a data capture to make quick tests when adding new features
- extensive logging available on the second UART port (the first one is used by the counter)


# Requirements

- A 'smarty' electricity meter
- Your personal decryption key, that you need to ask from the provider Creos, giving them your counter number
- A Wimo D1 mini pro micro-controller and adaptation board. See [Guy's website](http://weigu.lu/microcontroller/smartyreader/)

# Sample output in MQTT

```
root@54681c2ae65d:/# mosquitto_sub -t 'smarty/#' -v
smarty/current_l1/unit A
smarty/current_l1/value 002
smarty/current_l2/unit A
smarty/current_l2/value 000
smarty/current_l3/unit A
smarty/current_l3/value 002
smarty/electricity_failures/value 00040
smarty/electricity_sags_l1/value 00003
smarty/electricity_sags_l2/value 00003
smarty/electricity_sags_l3/value 00003
smarty/electricity_swells_l1/value 00000
smarty/electricity_swells_l2/value 00000
smarty/electricity_swells_l3/value 00000
smarty/electricity_switch_position/value 1
smarty/electricity_threshold/unit kVA
smarty/electricity_threshold/value 77.376
smarty/energy_delivered_tariff1/unit kWh
smarty/energy_delivered_tariff1/value 001377.072
smarty/energy_returned_tariff1/unit kWh
smarty/energy_returned_tariff1/value 000000.240
smarty/equipment_id/value SAG103xxxxxxxxxx
smarty/gas_index/unit m3
smarty/gas_index/value (null)
smarty/message_short/value (null)
smarty/message2_long/value (null)
smarty/message3_long/value (null)
smarty/message4_long/value (null)
smarty/message5_long/value (null)
smarty/p1_version/value 42
smarty/power_delivered/unit kW
smarty/power_delivered/value 00.930
smarty/power_returned/unit kW
smarty/power_returned/value 00.000
smarty/reactive_energy_delivered_tariff1/unit kVArh
smarty/reactive_energy_delivered_tariff1/value 000002.272
smarty/reactive_energy_returned_tariff1/unit kVArh
smarty/reactive_energy_returned_tariff1/value 001010.152
smarty/reactive_power_delivered/unit kVAr
smarty/reactive_power_delivered/value 00.000
smarty/reactive_power_returned/unit kVAr
smarty/reactive_power_returned/value 00.463
smarty/status connected
smarty/timestamp/value 190426173549S
```

# Usage

- Download and install [PlatformIO](https://platformio.org/). I did not test it with the Arduino IDE.
- Open the project in PlatformIO
- Copy the file `include/smarty_user_config_sample.h` to `include/smarty_user_config.h` and adjust it to your needs (WiFi settings, decription key, MQTT server).
- Build and program the Wimo


# smarty-reader

Read smart Luxembourgish electricity and gaz counter values with ESP8266.

# Overview

<img src="http://weigu.lu/microcontroller/smartyreader/png/smartyreader_1_800.png"  width="150"/>

In Luxembourg, electricty and gas counters are being replaced with smart counters that allow the provider to remotely measure consumption values. A special port is provided to allow the end-user to capture the data, for example for integration with home automation systems.

This work is a fork of the [awesome project by Guy WEILER](http://www.weigu.lu/microcontroller/smartyReader_P1/index.html) who did all the hard work on both the software and hardware side. Please review [Guy's website](http://www.weigu.lu/microcontroller/smartyReader_P1/index.html) for technical details on the solution, PCB layout, etc.

# Features

- publish all values from the electricity counter to MQTT in their own topic
- units are published as well in a separate topic for easy of interpretation
- if a gas counter is connected to the electricity counter, gas volume is published as well (implemented but not tested yet)
- possibility to work without a counter from a data capture to make quick tests when adding new features
- extensive logging available on the second UART port (the first one is used by the counter)
- supports the smarty firmware as of April 2020 (more data points available, see below)


# Requirements

- A 'smarty' electricity meter
- Your personal decryption key, that you need to ask from the provider Creos, giving them your counter number
- A Wimo D1 mini pro micro-controller and adaptation board. See [Guy's website](http://www.weigu.lu/microcontroller/smartyReader_P1/index.html)

# Sample output in MQTT

```
root@54681c2ae65d:/# mosquitto_sub -t 'smarty/#' -v
smarty/act_pwr_p_minus_l1/value 00.000
smarty/act_pwr_p_minus_l2/value 00.000
smarty/act_pwr_p_minus_l3/value 00.000
smarty/act_pwr_p_plus_l1/value 00.384
smarty/act_pwr_p_plus_l2/value 00.123
smarty/act_pwr_p_plus_l3/value 00.434
smarty/apparent_export_pwr/value 00.000
smarty/apparent_import_pwr/value 01.162
smarty/broker_ctrl_state_1/value 0
smarty/broker_ctrl_state_2/value 0
smarty/elec_failures/value 00340
smarty/elec_sags_l1/value 00009
smarty/elec_sags_l2/value 00009
smarty/elec_sags_l3/value 00009
smarty/elec_swells_l1/value 00000
smarty/elec_swells_l2/value 00000
smarty/elec_swells_l3/value 00000
smarty/elec_switch_postn/value 1
smarty/elec_threshold/value 027.6
smarty/energy_delivered_tariff1/value 011634.750
smarty/energy_returned_tariff1/value 000000.240
smarty/equipment_id/value SAG1030123456789
smarty/gas_index/value (null)
smarty/limiter_curr_monitor/value 040
smarty/msg_short/value (null)
smarty/msg2_long/value (null)
smarty/msg3_long/value (null)
smarty/msg4_long/value (null)
smarty/msg5_long/value (null)
smarty/p1_version/value 42
smarty/phase_curr_l1/value 002
smarty/phase_curr_l2/value 000
smarty/phase_curr_l3/value 002
smarty/phase_volt_l1/value 231.0
smarty/phase_volt_l2/value 230.0
smarty/phase_volt_l3/value 229.0
smarty/pwr_delivered/value 00.942
smarty/pwr_returned/value 00.000
smarty/react_energy_delivered_tariff1/value 000012.757
smarty/react_energy_returned_tariff1/value 006148.356
smarty/react_pwr_delivered/value 00.000
smarty/react_pwr_q_minus_l1/value 00.000
smarty/react_pwr_q_minus_l2/value 00.000
smarty/react_pwr_q_minus_l3/value 00.000
smarty/react_pwr_q_plus_l1/value 00.000
smarty/react_pwr_q_plus_l2/value 00.000
smarty/react_pwr_q_plus_l3/value 00.000
smarty/react_pwr_returned/value 00.000
smarty/timestamp/value 200423122938S
```

# Usage

- Download and install [PlatformIO](https://platformio.org/). I did not test it with the Arduino IDE.
- Open the project in PlatformIO
- Copy the file `include/smarty_user_config_sample.h` to `include/smarty_user_config.h` and adjust it to your needs (WiFi settings, decription key, MQTT server).
- Build and program the Wimo


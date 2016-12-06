# Motion aware Radiator Thermostat (TRV) Project

[![Video Demo Here](https://img.youtube.com/vi/8pM-4RSM31o/0.jpg)](https://www.youtube.com/watch?v=8pM-4RSM31o)

The project aims are to add intelligence to how we use radiator valves to heat rooms. Whilst Thermostatic Radiator Valves (TRVs) are mandated on all radiators by EU law, traditional designs rely on wax heating up and cooling down to open and close the valve. This is 2016 - we can do better! http://worrydream.com/ClimateChange/#consumption

Whilst there are more complex approaches to connect all TRVs to a LAN that has a fine degree of control, I am testing a different approach which relies on Bluetooth Low Energy. The basic outline is that a motion sensor, combined with a temperature sensor measures whether an *occupied* room is too cold and if so, sends a boost signal to the radiator to warm up the room. This approach doesn't require fancy control systems and would likely improve on not heating rooms that are not used. By setting appropriate values for time lag (leaving the room for 5 minutes shouldn't switch off the boost for example) this could prove to be easy to configure and produce effective heat savings.

This project has three main components

## The EQ3 Bluetooth Radiator Thermostatic Valve (TRV)

This particular model responds to Bluetooth commands which are usually sent by the manufacturer's phone app.
Available here: http://www.conrad-electronic.co.uk/ce/en/product/1364875/Wireless-thermostat-head-eQ-3-CC-RT-BLE
Also from Amazon UK

The low level instructions for interacting with this device were found in the following projects

https://github.com/mpex/EQ3-Thermostat/blob/master/eq3_control.py (Python)
https://github.com/maxnowack/homebridge-eq3ble/blob/master/src/thermostat.js (Node/Javascript)

## The Sensor Rig: Arduino Uno, HM10 Bluetooth Shield, PIR sensor and Temperature sensor

Arduino kit: https://www.amazon.co.uk/Arduino-Starter-Kit-UNO-Board/dp/B009UKZV0A/ref=sr_1_1?srs=1779758031&ie=UTF8&qid=1480979880&sr=8-1&keywords=arduino+starter+kit

(this includes Arduino Uno, Temperature sensor, breadboard and wires)

HC-SR501 PIR Motion Sensor (as part of kit): http://kumantech.com/kuman-16-in-1-modules-sensor-kit-project-super-starter-kits-for-arduino-uno-r3-mega2560-mega328-nano-raspberry-pi-3-2-model-b-k62_p0021.html

HM10 Bluetooth shield: https://www.amazon.co.uk/Bluetooth-Shield-Arduino-Master-iBeacon/dp/B017QPITRM/ref=sr_1_1?s=electronics&ie=UTF8&qid=1480980140&sr=1-1&keywords=bluetooth+shield+arduino

(Not recommended for reasons below)

# The Server: Raspberry Pi 2 w/bluetooth dongle running Node application

Bluetooth Dongle: https://www.amazon.co.uk/gp/product/B00KNPTHS8/ref=oh_aui_detailpage_o01_s00?ie=UTF8&psc=1

## Setup

The HM10 Bluetooth shield was a real problem for this project. I wouldn't buy another one.

Theoretically, the Raspberry Pi server in this setup is redundant. It should be possible for the Arduino to monitor the environment and then trigger the boost command directly on the TRV directly when needed. I spent a considerable amount of time trying to achieve this - to explain why it didn't work requires a brief diversion into Bluetooth Low Energy.

### The Trouble with HM10's Bluetooth

Bluetooth LE devices come in three main types:

* central (master)
* peripheral (slave)
* iBeacon (broadcast only)

In the app/TRV setup intended by the manufacturer, the app is the "central" device which sends Bluetooth messages to the "peripheral" (the EQ3 valve)

The communication process goes roughly as follows:

* central device A scans for peripherals - finds device B
* A requests a connection from B
* B responds with a success message including a connection handle
* This handle establishes a session between the devices and is passed in all future messages

* Device B has "characteristics" (think API endpoints) that have different "handles" (think HTTP verbs like GET, POST, PUT, etc.)
* These are grouped into "services" (think subfolders in a URL)

To take a concrete example, the boost command on the EQ3 takes the following format

    gatttool -b "00:1A:22:08:99:8C" --char-write-req  -a "0x0411" -n "4501"

where

* `00:1A:22:08:99:8C` is the MAC address of the EQ3 device
* `--char-write-req` represents the "write without response" "attribute handle" (represented as `0x12`)
* `0x0411` - this is an arbitrary identifier set by the device that represents the "boost mode" characteristic
* `4501` - again arbitrary, this is the value to be written to turn boost on

It took a lot of work to figure this stuff out, but even getting to this point I found that using the HM10 in central mode wasn't working. I read quite a lot of the BLE spec and still couldn't work out the right message formats so I ended up doing a packet trace first on OS X using "Packet Logger" and later on the Raspberry Pi using `btmon`.

To setup the HM10 in central mode required sending various AT commands via a serial connection:

```
  // Excerpt of HM10 setup in central mode
  mySerial.print("AT");
  delay(100);
  mySerial.print("AT+ROLE1"); // become "central" device
  delay(100);
  mySerial.print("AT+NOTI1"); // Receive notifications
  delay(100);  
  mySerial.print("AT+MODE2"); // Allow access to pins
  delay(1000);
  mySerial.print("AT+IMME1"); // IMME0 means to start running central commands "immediately" rather than waiting for AT commands
  delay(100);
  mySerial.print("AT+CON001A2208998C"); // Connect to the EQ3 via MAC address - found with AT+DISC? command
  delay(5000);
```

After establishing this connection all subsequent communications should be forwarded onto the peripheral device (EQ3 in this case).

From the packet trace on the RPi I could see that the connection was established, the connection handle exchanged and it all looked good. However, *on sending any information to the peripheral* the data was sent using the "Write Response Attribute" handle (`0x13`). This made no sense - it effectively means that the HM10 was treating any request as if it had received a write request and was responding with some data (inputted by me from the serial connection). There was no way to change this handle which meant there was no way to effectively use peripherals directly. The firmware is broken and this was a frustrating waste of time to say the least...

### A different approach

As I had a deadline looming (this was a University project) I didn't have time to try another device. Instead I had to look at what the HM10 *could* do - one of those things was to act as an iBeacon. This is a new(ish) proprietary variation on Bluetooth where devices broadcast messages and master devices (e.g. iPhones) can detect these and calculate their proximity based on the signal strength.

This format doesn't allow for arbitrary values, but it does allow for the version numbers of the broadcasting devices to be changed. As a bit of a hack, I opted to make the sensors emit a binary choice - if the room is warm enough and it is occupied it emits one device version, if it is too cold and occupied it emits another. I then hacked together the accompanying node script to listen for these values and detect changes.

Because the Raspberry Pi can correctly act as a central device, it is able to trigger the boost mode on the EQ3 without a problem.

In writing the node script I did run into lots of issues where the Bluetooth dongle would stop working if too many processes tried to access it at once. This resulted in lots of debugging and the defensive code you see as a result.

## Conclusions

Whilst the HM10 was for the most part a giant waste of time, the iBeacon approach that I ended up with does have some advantages. Firstly it is very simple - it is easy to imagine a system where (too cold + motion detected) could be generalised into a useful data beacon. Secondly it is extremely power efficient - iBeacon devices are designed to last for years on a single button battery. With a view to a finished product, this freedom would be attractive from a consumer perspective. Finally, allowing for a generic and more simple sensor desgin means that it can be decoupled from particular implementations - this is an issue that other smart TRV initiatives have run into - so there's no reason why the RPi couldn't talk to a Nest Thermostat over Wifi for example.

The downsides - temperature is not currently adjustable without recompiling the Arduino code(!). The Raspberry Pi is arguably redundant - having the sensor be able to trigger the boost would be more elegant. More elegant still would be motion detection built into the EQ3 itself, but the positioning of valves often means that this would be undesirable  and wouldn't pick up the motion effectively from the room.

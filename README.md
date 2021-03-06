
# Algorythm

Free / Libre open source IoT product based on ESP32, using CoAP for communication, powering a RGB LED band with changing colors at 60 Hz based on audio frequency and changing color intensity based on amplitude, using a microphone with audio sampling at 44.1 kHz

[![Algorythm Project Presentation](https://img.youtube.com/vi/iIuo6Bbvzmk/0.jpg)](https://www.youtube.com/watch?v=iIuo6Bbvzmk)

You can download the android app's source code [here](https://codeberg.org/btrc4t/algorythm-app)

## Installing
![System Diagram](system_diagram.png "Algorythm System Diagram")
### Software Prerequisites

* ESP-IDF environment: See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) for full steps to configure and use ESP-IDF to build projects.

### Hardware Prerequisites
* If you're plugging this into a home socket, you will need an AC-DC 12V converter.
* 12V-5V DC-DC converter (to power the esp32).
* LED driver: See [this schematic](https://github.com/idumzaes/ESP-M3-3ch-LED-Controller-Schematics/blob/master/Schematic_ESP%203-ch%20LED%20Driver.pdf) for setting up a LED driver. Specifically look at the LED Out section of the schematic. From that section eliminate the 1K resistors, those aren't needed.
* Microphone with amp: [Electret Microphone Amplifier - MAX9814 with Auto Gain Control](https://www.adafruit.com/product/1713) or similar. (Auto Gain not necessary)
* 12V RGB Led band.

### Functioning modes

  * MANUAL: Has one color set manually
  * AUDIO_INTENSITY: Color intensity varies with audio amplitude
  * AUDIO_FREQ: Color changes with audio frequencies
  * AUDIO: Color changes with audio frequencies and intensity varies with audio amplitude
  * OFF: Leds will not light up
  * AUDIO_HOLD: Color changes with audio frequencies and intensity varies with audio amplitude.  When amplitude is under the minimum, the last color set is shown.

### Configure the project

```
idf.py menuconfig
```

Algorythm Project Configuration  --->
 * If PSK enabled, Set CoAP Preshared Key to use in the connection to the server
 * Set WiFi SSID
 * Set WiFi Password
 * Set the mDNS hostname for the board to use in the internal WiFi network
 * Set the mDNS instance name for the board to use in the internal WiFi network (this is best left as it is, the android application uses this string to identify nodes)

Component config > CoAP Configuration  --->
  
  * Set encryption method definition, PSK (default) or PKI
  * Enable CoAP debugging if required

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py build
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## CoAP Endpoints

Currently both DTLS (port 5684) and insecure connections (port 5683) are available,
however once development reaches release status the server will be changed
to DTLS only.

These are the CoAP endpoints that the server makes available for clients:
 * /room (Fetch / change a room name to identify where you've placed the device)
 * /rgb (Fetch / change the RGB color in manual mode)
 * /mode (Fetch mode / set the device to function in one of multiple modes)
 * /prefs (Fetch configuration / set device configuration)

 If you're using [npm](https://docs.npmjs.com/cli/v7/configuring-npm/install) there is a 
command line tool which makes debugging CoAP easy: [coap-cli](https://www.npmjs.com/package/coap-cli)

For example to change the room name using the CoAP endpoint:
```
$ echo -n 'desk' | coap put coap://10.0.0.120/room
(2.04)
```
To ensure the endpoint stores the new value:
```
$ coap get coap://10.0.0.120/room
(2.05)  desk
```
The IP ``10.0.0.120`` might not be the one your device was assigned, check the troubleshooting section below.


NOTE: Client sessions trying to use coaps+tcp:// are not currently supported, even though both
libcoap and MbedTLS support it.

The Constrained Application Protocol (CoAP) is a specialized web transfer protocol for use with
constrained nodes and constrained networks in the Internet of Things.   
The protocol is designed for machine-to-machine (M2M) applications such as smart energy and
building automation.

Please refer to [RFC7252](https://www.rfc-editor.org/rfc/pdfrfc/rfc7252.txt.pdf) for more details.

## libcoap Documentation
This can be found at https://libcoap.net/doc/reference/4.2.0/

## Troubleshooting
* You should be able to find the IP of the esp32 board by watching the serial output with:
```
idf.py -p PORT monitor
```
* You can also find the IP of the esp32 board using dig:
```
$ dig @224.0.0.251 -p 5353 -t ptr +short _http._tcp.local
LEDSINSTANCEMDNS._http._tcp.local.
0 0 80 LEDSHOST-A0B000.local.
"prefs=Undefined" "mode=Undefined" "rgb=Undefined" "room=Undefined"
```

```
$ dig @224.0.0.251 -p 5353 +short LEDSHOST-A0B000.local
10.0.0.120
```


* Please make sure CoAP client fetches or puts data under path: `/room` or
fetches `/.well-known/core`

* CoAP logging can be enabled by running 'idf.py menuconfig -> Component config -> CoAP Configuration' and setting appropriate log level

* You can debug the microphone input by changing ``DEBUG_MIC_INPUT`` to 1

## TODO
* Setup CoAP DTLS only
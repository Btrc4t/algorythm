
# Algorythm

IOT appliance based on the CoAP protocol with rgb leds reacting to sound input.

### Configure the project

```
idf.py menuconfig
```

Algorythm Project Configuration  --->
 * If PSK enabled, Set CoAP Preshared Key to use in the connection to the server
 * Set WiFi SSID under Example Configuration
 * Set WiFi Password under Example Configuration
 * Set the mDNS hostname for the board to use in the internal WiFi network
 * Set the mDNS instance name for the board to use in the internal WiFi network

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

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) for full steps to configure and use ESP-IDF to build projects.

## CoAP Endpoints

Currently both DTLS (port 5684) and insecure connections (port 5683) are available,
however once development reaches release status the server will be changed
to DTLS only.

These are the CoAP endpoints that the server makes available for clients:
 * /room (Fetch / change a room name to identify where you've placed the device)
 * /rgb (Fetch / change the RGB color in manual mode)
 * /mode (Fetch mode / set the device to function in one of multiple modes)
 * /prefs (Fetch configuration / set device configuration)

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
* Please make sure CoAP client fetches or puts data under path: `/room` or
fetches `/.well-known/core`

* CoAP logging can be enabled by running 'idf.py menuconfig -> Component config -> CoAP Configuration' and setting appropriate log level

## TODO
* Complete README

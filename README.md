# Apple MIDI Demo

## Scope

This demo shows how to run an Apple MIDI Service

You should be able to connect to the WIFI Interface with your computer.

E.g. MacOS: in Audio/MIDI Config, MIDI Window:
  * press Network Configuration button
  * announce the IP of your ESP32 device (we use port number 5004 by default)
  * push the "Connect" button
  
The demo will just loopback incoming MIDI messages.

E.g. you could pick up a MIDI port to which a MIDI keyboard is connected under "Live Routings", send some notes
and observe the loopbacked events on a MIDI monitor.

Re-usable component is located under components/applemidi - please see the README.md for programmers there.


## Preparation

WIFI credentials have to be configured the first time you are launching this application.

Open a terminal connected to the UART of your device, you could enter "help" to get a list of available commands.

For WIFI configuration, enter:
  * wifi_join &lt;ssid&gt; &lt;password&gt;
  * wifi_store
  
The next time the device will be started, the stored credentials will be taken automatically and a connection
should take place immediately.


## Special Console Features

  * use "applemidi_info" to display some details about the connections.
    Up to 4 independent connections are supported
    
  * use "applemidi_debug on" to send more debug messages.
    Note that higher verbosity might result into packet lost since the printf() messages delay processing!
    
  * use "applemidi_start_session &lt;ip&gt;" to initiate an own session.
    A different port can be specified with --port=<port>, 5004 is used by default. 
  

## Important

Please optimize the app configuration with "idf.py menuconfig":

* Compiler Options->Optimization Level: set to -Os (Release)
* Component Config->ESP32 Specific: set Minimum Supported ESP32 Revision to 1 (if you have a newer device...)
* Component Config->ESP32 Specific: set CPU frequency to 240 MHz
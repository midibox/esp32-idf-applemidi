# Apple MIDI Driver

## Scope

This is just another Apple MIDI protocol implementation targeting ESP32 and similar cores which support LWIP

Although very generic solutions already exist, such as the object oriented Arduino AppleMIDI Library, or the
Cross-platform unified MIDI library "midikit", I decided to create an own "lightweighted" implementation with
the intention to use it with MIDIbox applications (and maybe also integrate it into MIOS32) in future.

Design targets:
   * easy to read (reduced number of files and dependencies)
   * minimum resource allocation
   * fast processing
   * separate packet parser from TCP/IP stack to allow adaptions to other stacks and OS aside from LWIP/FreeRTOS
   * consider possibility to forward MIDI messages to other interfaces, such as UART, USB MIDI, BLE MIDI, SPI MIDI (MIOS32 specific)
   

## References

### Overview & Documents
   * https://en.wikipedia.org/wiki/RTP-MIDI#AppleMIDI
   * https://datatracker.ietf.org/doc/rfc4695/
   * https://datatracker.ietf.org/doc/rfc4696/
   * https://datatracker.ietf.org/doc/rfc6295/

### Driver Inspirations   
   * https://github.com/lathoub/Arduino-AppleMidi-Library
   * https://github.com/jpommerening/midikit

## Usage

See also demo under ../../main


## Limitations
   * very limited documentation available yet (it's work-in-progress ;-)
   * no support for incoming/outgoing journals
   * no support for delta timestamps in buffered outgoing MIDI messages   
   
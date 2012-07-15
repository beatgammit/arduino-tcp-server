Intro
=====

This is a basic example of using a TCP server to control a servo. The TCP server follows the WebSocket protocol (without handshake), so you must supply a header frame before sending data. The data is just JSON.

For example, to move the servo to 80 degrees, send a TCP packet like so:

* first byte is 0x81 (means a text packet, not fragmented)
* second byte is 0x80 + the size of your JSON (should be less than 125 bytes)
* the rest is your json (`{"angle": 80}`)

For a sample implementation, see the included client.go. The included client reads an angle on stdin and displays the result on stdout. It will stay up until you kill it (Ctrl+C).

Note, this relies on [aJson](http://interactive-matter.eu/how-to/ajson-arduino-json-library/).

Client
------

The client is written in Go. For more information and installation instructions, see [golang.org](http://golang.org).

Info
----

By default, the server listens on 192.168.254.177:5000, and the servo is expected to be on pin 9. The servo is moved 1 degree at a time with a small delay between each one.

Issues
======

Currently, JSON content can be up to 0xffff, but this isn't tested. Sizes above 0xffff will be ignored and an error will be printed to Serial along with the data of the packet. This shouldn't be a problem though.

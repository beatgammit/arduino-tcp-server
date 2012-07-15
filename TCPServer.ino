/*
  Simple TCP server that communicates using the WebSocket protocol.
  The major difference in this implementation versus the actual
  protocol is that there is no handshaking.
 */

#include <SPI.h>
#include <Ethernet.h>
#include <aJSON.h>
#include <Servo.h> 

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x32, 0x10 };
IPAddress ip(192, 168, 254, 177);

// Initialize the Ethernet server library
EthernetServer server(5000);

// create the servo object
Servo servo;

// servo position
int pos = 0;

void setup() {
  // initialize the serial connection
  Serial.begin(9600);
  
  // attach our servo to pin 9
  servo.attach(9);
  
  Serial.println("Initializing ethernet connection");
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
}

void moveServo(int angle) {
  Serial.print("Current position: ");
  Serial.println(pos);
  Serial.print("Moving to: ");
  Serial.println(angle);
  
  if (pos < angle) {
    for (pos++; pos < angle; pos++) {  
      servo.write(pos);
      
      delay(15);
    }
  } else if (pos > angle) {
    for (pos--; pos > angle; pos--) {  
      servo.write(pos);
      
      delay(15);
    }
  }
  
  pos = angle;
}

// parses useful data out of the request
void parseRequest(char* data, long length) {
  aJsonObject* jsonObject = aJson.parse(data);
  aJsonObject* angle = aJson.getObjectItem(jsonObject, "angle");

  int degs = angle->valueint;

  aJson.deleteItem(jsonObject);
  
  moveServo(degs);
}

void parseStream(EthernetClient client) {
  aJsonObject* jsonObject = aJson.parse((FILE*)&client);
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    char c;
    long length;
    char* data;
    
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        // based on WebSocket protocol, but implemented in raw TCP
        // this implementation does not allow fragments
        // details of the spec were found from these resources:
        // http://stackoverflow.com/questions/8125507/how-can-i-send-and-receive-websocket-messages-on-the-server-side
        // http://datatracker.ietf.org/doc/rfc6455/?include_text=1
        
         // first byte: must be 10000001 (final packet & no extensions)
         c = client.read();
         if ((c & 0xff) != 0x81) {
           // protocol violation...
           Serial.println("Protocol violation. Invalid first byte. Ignoring...");
           Serial.println(c & 0xff, HEX);
         }
         
         // second byte: must have msb set, the rest is the length
         c = client.read();
         if ((c & 0x80) != 0x80) {
           // protocol violation...
           Serial.println("Protocol violation. Mask bit not set. Assuming it is...");
           Serial.println((int)c, HEX);
         }
         length = c & 0x7f;
         // if the length is 126, the next two bytes represent the length,
         // if the length is 127, the next eight bytes represent the length
         // length is unsigned
         if (length == 126) {
           // read the next two bytes as the length
           length = client.read() << 8;
           length |= client.read();
         } else if (length == 127) {
           length = 0;
           for (int i = 8; i >= 0; i--) {
             length |= client.read() << i * 8;
           }
           
           // unsupported length, we only have 2k ram...
           Serial.print("Length too large:");
           Serial.println(length, DEC);
           for (int i = 0; i < length; i++) {
             Serial.write(client.read());
           }
           
           return;
         }
         
         // allocate enough memory
         data = (char*)malloc(length + 1);
         
         // grab all of our data
         for (int i = 0; i < length; i++) {
           data[i] = client.read();
         }
         // null terminate
         data[length] = 0;
         
         // do something with the data
         parseRequest(data, length);
         
         // free our memory
         free(data);
         
         String reply = String("{\"message\": \"Angle updated\"}");
         Serial.println("Sending reply:");
         
         // short delay before we send data
         delay(10);
         
         client.write(0x81);
         client.write(0x80 | reply.length());
         for (int i = 0; i < reply.length(); i++) {
           client.write(reply[i]);
         }
         
         // give the client time to receive the data
         delay(10);
         
         Serial.println("Done sending reply");
      }
    }
    
    // close the connection:
    Serial.println("Closing connection");
    client.stop();
  }
}

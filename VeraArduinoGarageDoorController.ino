/*
 Garage Controller
 Written by Aram Perez
 Licensed under GPLv2, available at http://www.gnu.org/licenses/gpl-2.0.txt
 */

//#define LOG_SERIAL

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>

#define NO_PORTA_PINCHANGES
#define NO_PORTC_PINCHANGES
#include <PinChangeInt.h>

#define IOPORT 23  //Normal telnet port
#define NBR_OF_RELAYS 4

// Garage door sensors & pushbutton
#define GARAGE_CLOSED_SENSOR 2 //Connect to NC terminal, active high
#define GARAGE_PARTIALLY_OPEN_SENSOR 3   //Connect to NO terminal, active high

#define RELAY0 4
#define GARAGE_RELAY RELAY0  //Relay for garage door button
#define RELAY1 5
#define RELAY2 6
#define RELAY3 7

#define CR ((char)13)
#define LF ((char)10)

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
static byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
static IPAddress ip(192, 168, 1, 170);
static IPAddress gateway(192, 168, 1, 1);
static IPAddress subnet(255, 255, 255, 0);

static EthernetServer server(IOPORT);
static EthernetClient client;

static char relayState[NBR_OF_RELAYS];

class GarageDoor
{
  bool closedState, partiallyOpenState;
public:
  GarageDoor();
  void Init();
  void SetClosedState(bool st){
    closedState = st;
  }
  void SetPartiallyOpenState(bool st){
    partiallyOpenState = st;
  }
  char State() const;
  void PushButton();
};

static GarageDoor garageDoor;

//This should be a private function in the GarageDoor class
//i.e. GarageDoor::StateChangedISR(void),
//but the compiler gives an error if it is :-(
static void StateChangedISR(void)
{
  if( PCintPort::arduinoPin == GARAGE_CLOSED_SENSOR ){
    garageDoor.SetClosedState(PCintPort::pinState);
  }
  else{
    //Must have been the GARAGE_PARTIALLY_OPEN_SENSOR:
    garageDoor.SetPartiallyOpenState(PCintPort::pinState);
  }
}

GarageDoor::GarageDoor()
{
}

void GarageDoor::Init()
{
  pinMode(GARAGE_CLOSED_SENSOR, INPUT_PULLUP);
  PCintPort::attachInterrupt(GARAGE_CLOSED_SENSOR, &StateChangedISR, CHANGE);
  pinMode(GARAGE_PARTIALLY_OPEN_SENSOR, INPUT_PULLUP);
  PCintPort::attachInterrupt(GARAGE_PARTIALLY_OPEN_SENSOR, &StateChangedISR, CHANGE);
  closedState = digitalRead(GARAGE_CLOSED_SENSOR);
  partiallyOpenState = digitalRead(GARAGE_PARTIALLY_OPEN_SENSOR);
}

void GarageDoor::PushButton()
{
  digitalWrite(GARAGE_RELAY, LOW);
  delay(400);  //Delay .4 secs
  digitalWrite(GARAGE_RELAY, HIGH);
}

char GarageDoor::State() const
{
  if( closedState ) return 'c';
  return partiallyOpenState ? 'p' : 'o';
}


void setup() {
#ifdef LOG_SERIAL
  Serial.begin(56700);
#endif
  // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);
  // start listening for clients
  server.begin();
  garageDoor.Init();
  for( int i = 0; i < NBR_OF_RELAYS; i++ ){
    pinMode(RELAY0+i, OUTPUT);  //Zone 1
    digitalWrite(RELAY0+i, HIGH); //Relays use inverted logic, HIGH = Off
    relayState[i] = '0';  //Use normal logic
  }
  if( client.connected() ){
    client.flush();
  }
#ifdef LOG_SERIAL
  Serial.println("\r\nOK");
#endif
}

char ReadNext()
{
  char ch = client.read();
#ifdef LOG_SERIAL
  Serial.print(ch);
#endif
  return ch;
}

//
//Commands:
//  g? - return current garage door state
//          c - door is closed
//          o - door is fully open
//          p - door is partially open
//  gb - "push" garage door button
//  rx? - return relay x state
//  rxy - set relay x to y (0 or 1)
//
void loop() {
  static char lastGarageDoorState = 'c';
  char ch, rAsc;
  if( !client.connected() ){
    // If client is not connected, wait for a new client:
    client = server.available();
  }
  if( client.available() > 0 ){
    int rNdx;
    bool err = false;
    while( client.available() > 0 ){
      switch ( ReadNext() ) {
      case 'g':
        switch ( ReadNext() ) {
        case '?':
          ch = garageDoor.State();
          client.print('g');
          client.println(ch);
#ifdef LOG_SERIAL
          Serial.print(">g");
          Serial.println(ch);
#endif
          break;
        case 'b':
          garageDoor.PushButton();
          break;
        default:
          err = true;
        }
        break;
      case 'r':
        ch = ReadNext();
        switch( ch ){
        case '1':
        case '2':
        case '3':
          rAsc = ch;
          rNdx = ch - '1';
          ch = ReadNext();
          switch( ch ){
          case '?':
            ch = relayState[rNdx];
            break;
          case '0':
            digitalWrite(RELAY1 + rNdx, HIGH);  //Inverted logic
            relayState[rNdx] = ch;
            break;
          case '1':
            digitalWrite(RELAY1 + rNdx, LOW);  //Inverted logic
            relayState[rNdx] = ch;
            break;
          default:
            err = true;
          }
          if( !err ){
            client.print('r');
            client.print(rAsc);
            client.println(ch);
#ifdef LOG_SERIAL
            Serial.print('>');
            Serial.println(ch);
#endif
          }
          break;
        default:
          err = true;
        }
        break;
      case CR:
      case LF:
        break;    //Ignore CR & LF
      default:
        err = true;
      }
    }
    if( err ){
      client.println('?');
#ifdef LOG_SERIAL
      Serial.println(">Say what?");
#endif
    }
  }
  ch = garageDoor.State();
  if( ch != lastGarageDoorState ){
    lastGarageDoorState = ch;
    client.print('g');
    client.println(ch);
#ifdef LOG_SERIAL
    Serial.print(">g");
    Serial.println(ch);
#endif
  }
}

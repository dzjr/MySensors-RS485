/*
 *  Tilte:      RS-485 Relais node 
 *  Date:       27-12-2018
 *  Author:     MySensors community (Thanks @Woeka) & dzjr
 *  Libaries :  MySensors 2.3.1
 *  Description:MySensors RS-485 Relay node with 8 relays on a Arduino Pro Mini
 *              Relais node voor in de meterkast om relais in de verdeler te schakelen.
 *              
 * 
 */

// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable RS485 transport layer
#define MY_RS485

// Define this to enables DE-pin management on defined pin
#define MY_RS485_DE_PIN 11

// Set RS485 baud rate to use
#define MY_RS485_BAUD_RATE 9600

//eerst wat administratie en dergelijk
#define SN "MK Relais node" //Software Naam
#define SV "1.0"      //Software versie
#define MY_NODE_ID 10 //Node nummer
#define MY_TRANSPORT_WAIT_READY_MS 3000    //wachten tot de gateway is opgestart

#include <MySensors.h>

#define CHILD_ID 10
#define WACHTTijd 3000 //wachttijd om eerst de gateway opgestrat te hebben.

//Relay settings
const int PINS[] = {14, 15, 2, 3, 4, 5, 6, 7};
#define NUMBER_OF_RELAYS 8 // Total number of attached relays
#define RELAY_ON 0  // GPIO value to write to turn on attached relay
#define RELAY_OFF 1 // GPIO value to write to turn off attached relay
char* RELAY_NAMES[] = {"1e relais","2e relais","3e relais","4e relais","5e relais","6e relais","7e relais","8e relais"};


void before()
{
  wait (WACHTTijd);
	for (int sensor=1; sensor<=NUMBER_OF_RELAYS; sensor++) {
		// Then set relay pins in output mode
		pinMode(PINS[sensor-1], OUTPUT);
		// Set relay to last known state (using eeprom storage)
		digitalWrite(PINS[sensor-1], loadState(sensor)?RELAY_ON:RELAY_OFF);
	}
}

void setup()
{

}

void presentation()
{
	// Send the sketch version information to the gateway and Controller
	sendSketchInfo(SN, SV);

	for (int sensor=1; sensor<=NUMBER_OF_RELAYS; sensor++) {
		// Register all sensors to gw (they will be created as child devices)
		present(sensor, S_BINARY, RELAY_NAMES[sensor-1]);
	}
}


void loop()
{

}

void receive(const MyMessage &message)
{
	// We only expect one type of message from controller. But we better check anyway.
	if (message.type==V_STATUS) {
		// Change relay state
		digitalWrite(PINS[message.sensor-1], message.getBool()?RELAY_ON:RELAY_OFF);
		// Store state in eeprom
		saveState(message.sensor, message.getBool());
		// Write some debug info
		Serial.print("Incoming change for sensor:");
		Serial.print(message.sensor);
		Serial.print(", New status: ");
		Serial.println(message.getBool());
	}
}


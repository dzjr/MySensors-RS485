
/******************************************************************
 Created with PROGRAMINO IDE for Arduino - 28.12.2018 12:45:52
 Project     :MySensors Watermeter Node RS485
 Libraries   :MySensors, Dallas temp etc
 Author      :MySensors community & dzjr
 Description :Mysensors Watermeter node & leiding temperatuur 
              met lekkage detectie en optie voor waterdruk
******************************************************************/

#define MY_DEBUG

//RS 485 transport zaken
#define MY_RS485                  //RS-485 transport
#define MY_RS485_DE_PIN 11        //De DE pin benoemen, normaal is dit PIN-3
#define MY_RS485_BAUD_RATE 9600
//#define MY_RS485_MAX_MESSAGE_LENGTH 40
//#define MY_RS485_SOH_COUNT 3 // gebruiken bij Collision op de bus

//eerst wat administratie en dergelijk
#define SN "PWN_Node" //Software Naam
#define SV "1.01"      //Software versie
#define MY_NODE_ID 20 //Node nummer
#define MY_TRANSPORT_WAIT_READY_MS 3000    //wachten tot de gateway is opgestart


//libaries activeren
#include <MySensors.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Bounce2.h>

//Dallas settings 
#define COMPARE_TEMP 0 // 1= zenden alleen bij verandering 0= direct zenden
#define ONE_WIRE_BUS 7
#define TEMPERATURE_PRECISION 10 // 10 = 0,25°C 
#define MAX_ATTACHED_DS18B20 4
unsigned long SLEEP_TIME = 3000; //slaaptijd tussen twee metingen in ms
OneWire oneWire(ONE_WIRE_BUS); //Een oneWire-exemplaar instellen om te communiceren met alle OneWire-apparaten
DallasTemperature sensors(&oneWire); //OneWire naar Dallas
float lastTemperature[MAX_ATTACHED_DS18B20];
int numSensors=0;
bool receivedConfig = false;
bool metric = true;

DeviceAddress Probe03 = { 0x28, 0x80, 0x43, 0x77, 0x91, 0x0F, 0x02, 0x5A  }; // leiding temp
DeviceAddress Probe02 = { 0x28, 0xFF, 0x64, 0x1D, 0xF9, 0x9D, 0xDC, 0x5E  }; // op print1
DeviceAddress Probe01 = { 0x28, 0xFF, 0x64, 0x1D, 0xF8, 0x4F, 0x3A, 0x08  }; // op print2


MyMessage msgTemp(0, V_TEMP);

#define CHILD_ID_PWN 11                 //Child ID Sensor watermeter
#define CHILD_ID_LEKKAGE 21             //Child ID Lekkage sensor
#define CHILD_ID_DRUK 22                //Child ID Water druk sensor
#define PWN_SENSOR 2                    //Pin waar sensor watermeter op is aangesloten
#define LEKKAGE_PIN A0                  //Pin waarop de lekkage sensor op is aangesloten
#define WATERDRUK_PIN A1                //Pin waarom de druksensor is aangesloten
#define SENSOR_INTERRUPT PWN_SENSOR-2   //Usually the interrupt = pin -2 
#define PULSE_FACTOR 1000               //Pulsen per m³ water 1000=één rotatie per 10 liter
#define MAX_FLOW 60                     //flitert meetfouten eruit (40 van @RBisschops en 100 van @Rejoe)
#define wachtTime 5000

MyMessage flowmsg (CHILD_ID_PWN, V_FLOW);
MyMessage volumemsg (CHILD_ID_PWN, V_VOLUME);
MyMessage lastcountermsg (CHILD_ID_PWN, V_VAR1);
MyMessage lekmsg (CHILD_ID_LEKKAGE, V_TEXT);
MyMessage drukmsg (CHILD_ID_DRUK, V_PRESSURE);


unsigned long SEND_FREQUENCY = 30000; // Minimum time between send (in milliseconds). We don't want to spam the gateway.
double ppl = ((double)PULSE_FACTOR) / 1000;      // Pulses per liter
volatile unsigned long pulseCount = 0;
volatile unsigned long lastBlink = 0;
volatile double flow = 0;
boolean pcReceived = false;
unsigned long oldPulseCount = 0;
unsigned long newBlink = 0;
double oldflow = 0;
double volume = 0;
double oldvolume = 0;
unsigned long lastSend = 0;
unsigned long lastPulse = 0;
#define SLEEP_MODE false        // Watt-value can only be reported when sleep mode is false.
unsigned long maxTimeDryRun = 30000;
unsigned long startTimeDryRun = 0;
volatile unsigned long dryPulseCount = 0;

int lastlekValue = 0;
int lastdrukValue = 0;

// lowest and highest sensor readings:
const int sensorMin = 0;     // sensor minimum
const int sensorMax = 600;  // sensor maximum


void before()
{
  wait(wachtTime);
sensors.begin();
}
void setup()
{

  sensors.setWaitForConversion(false);
  
  
  
  pinMode(PWN_SENSOR, INPUT_PULLUP);
  
  
  
  sensors.setResolution(TEMPERATURE_PRECISION);

  pulseCount = oldPulseCount = 0;

  attachInterrupt(SENSOR_INTERRUPT, onPulse , FALLING );
  
}

void presentation()
{
  sendSketchInfo(SN,SV);

   numSensors = sensors.getDeviceCount();
  
  for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) 

  {
  metric = getControllerConfig().isMetric;
  // Registreer alle sensors naar de Gateway
  present(i+0, S_TEMP, "temp sensors");
  //present(i+1, S_TEMP, "MK Node temp");
  //present(i+2, S_TEMP, "PWN Leiding temp");
  present(CHILD_ID_PWN, S_WATER, "PWN_Meter");
  request(CHILD_ID_PWN, V_VAR1, "Req PWN data" );
  present(CHILD_ID_LEKKAGE, S_INFO, "Lekkage MK");
  present(CHILD_ID_DRUK, S_BARO, "PWN Waterdruk");
  
  
  }
}
void loop()
{
unsigned long currentTime = millis();  
      
      //Watermeter sensor
      //WaterPulse counter
      // Only send values at a maximum frequency or woken up from sleep
    if (currentTime - lastSend > SEND_FREQUENCY)
    {
    lastSend = currentTime;
    
  {  if (flow != oldflow) {
        oldflow = flow;
    #ifdef MY_DEBUG_LOCAL
        Serial.print("l/min:");
        Serial.println(flow);
    #endif
        // Check that we dont get unresonable large flow value.
        // could hapen when long wraps or false interrupt triggered
        if (flow < ((unsigned long)MAX_FLOW)) {
          send(flowmsg.set(flow, 2));                   // Send flow value to gw
        }
      }

      // No Pulse count received in 2min
      if (currentTime - lastPulse > 120000) {
        flow = 0;
      }

      // Pulse count has changed
      if (pulseCount != oldPulseCount) {
        oldPulseCount = pulseCount;
    #ifdef MY_DEBUG
          Serial.print(F("pulsecnt:"));
          Serial.println(pulseCount);
    #endif
          if (!pcReceived) {
          request(CHILD_ID_PWN, V_VAR1);
        }
          send(flowmsg.set(pulseCount));                  // Send  pulsecount value to gw in VAR1

        double volume = ((double)pulseCount / ((double)PULSE_FACTOR));
        if (volume != oldvolume) {
          oldvolume = volume;
    #ifdef MY_DEBUG
            Serial.print(F("vol:"));
            Serial.println(volume, 3);
    #endif
            send(volumemsg.set(volume, 3));               // Send volume value to gw
          }
        }
  

    {
 //Waterdruk meten 
      int Waterdruk = (analogRead(WATERDRUK_PIN))*10;
      if (Waterdruk != lastdrukValue) {
      send (drukmsg.set(Waterdruk));
      lastdrukValue = Waterdruk;}

  
  //waterlekkage sensor 
     int lekLevel = map(analogRead(LEKKAGE_PIN), sensorMin, sensorMax, 0, 3);
 
      switch (lekLevel) {
        case 0:    // Sensor is droog
        send(lekmsg.set("Geen lekkage"));
        break;
        case 1:    // Sensor raakt nat
        send(lekmsg.set("Vochtig"));
        break;
        case 2:   // Sensor is volledig bedekt met water
        send(lekmsg.set("Lekkage"));
        break;
      }
}

}
{
    //Dallas temperature sensoren
    // Fetch temperatures from Dallas sensors 
    sensors.requestTemperatures();
    // query conversion time and sleep until conversion completed
    int16_t conversionTime = sensors.millisToWaitForConversion(sensors.getResolution());
    
 
    wait(500);

    // Read temperatures and send them to controller 
    for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {
 
      // Fetch and round temperature to one decimal
     //float temperature = static_cast<float>(static_cast<int>((getControllerConfig().isMetric?sensors.getTempCByIndex(i):sensors.getTempFByIndex(i)) * 10.)) / 10.;
    float temperature;
      // voor geadreseerde sensoren
      switch (i)  {
        case 0:
          temperature = sensors.getTempC(Probe01);
          break;
        case 1:
          temperature = sensors.getTempC(Probe02);
          break;
        case 2:
          temperature = sensors.getTempC(Probe03);
          break;
        default:
          temperature = sensors.getTempCByIndex(Probe03);
          break;
      }
   
 
      // Only send data if temperature has changed and no error
      #if COMPARE_TEMP == 1
      if (lastTemperature[i] != temperature && temperature != -127.00 && temperature != 85.00) {
      #else
      if (temperature != -127.00 && temperature != 85.00) {
      #endif
 
        // Send in the new temperature
        send(msgTemp.setSensor(i).set(temperature,1));
        // Save new temperatures for next compare
        lastTemperature[i]=temperature;
      }
      }
  }
}
}

void onPulse()
{
  if (!SLEEP_MODE)
  {
    unsigned long newBlink = micros();
    unsigned long interval = newBlink - lastBlink;

    if (interval != 0)
    {
      lastPulse = millis();
      if (interval < 1000000L) {
        // Sometimes we get interrupt on RISING,  1000000 = 1sek debounce ( max 60 l/min)
        return;
      }
      flow = (60000000.0 / interval) / ppl;
    }
    lastBlink = newBlink;
  }
  pulseCount++;
  dryPulseCount++;
}


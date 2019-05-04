// argv: d_hot, d_cold
// output format: yyyy-mm-ddThh:mm:ss d_hot d_cold

// wiringPi: digitalRead, wiringPiISR, pullUpDnControl, wiringPiSetup
#include <wiringPi.h>

// std
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// sleep
#include <unistd.h>

// dirname
#include <libgen.h>

#include "isr.h"
#include "debounce.h"
#include "rest.h"
#include "cJSON/cJSON.h"

#define EVENING_FROM 20 /* hours */
#define EVENING_UPTO 2  /* hours, must be >= 0 */
#define DURATION 20     /* minutes, how long to be light since latest movement */

// globalCounter:
//	Global variable to count interrupts
//	Should be declared volatile to make sure the compiler doesn't cache it.

//static volatile int globalCounter [8] ;

static unsigned int kitchPirS = 15; // wiringpi id; phisical: 8
static unsigned int kitchRelay = 3; // wiringpi id; phisical: 15

int lastMovingTime = 0; // sec
bool isLightOn = false;
bool prevMoving = false;
unsigned long startedAt = null; // sec, since 1970 aka epoch
const unsigned HOUR = 24 * 60;  // sec
const unsigned MIN = 60;        // sec

// // get time in seconds
// unsigned getSunset()
// {
//   fprintf(stderr, "json...\n");
//   const char *headers = "X-RapidAPI-Host: sun.p.rapidapi.com\nX-RapidAPI-Key: eb17b3b315msh1feb8f4a6f34475p117f34jsnf8487dd7ab50";
//   http_get("sun.p.rapidapi.com", "/api/sun/?latitude=55.797447&longitude=37.607969&date=2019-05-27", headers);

//   // printf("json: %s\n", buffer);
//   //cJSON *json = cJSON_Parse(buffer);
//   //printf("%s", json);
//   return 42;
// }

void print_debug(const char *str)
{
  fprintf(stderr, str);
}

bool hasMoving(void)
{
  return digitalRead(kitchPirS);
}

time_t seconds()
{
  return time(NULL);
}

bool toggleLight(bool isOn)
{
  if (isLightOn == isOn)
    return isOn;
  fprintf(stderr, "effective toggle light. current: %d / request: %d\n", isLightOn, isOn);
  system("mpg321 ./beep.mp3");
  digitalWrite(kitchRelay, isOn);
  isLightOn = isOn;
  return isLightOn;
}

bool getEveningTime()
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  const unsigned char hour = lt->tm_hour + 3;
  const bool yes = hour >= EVENING_FROM || hour <= EVENING_UPTO;
  print_debug("hour: ");
  fprintf(stderr, "%d\n", hour); // print_debug

  return yes;
}
void onMove(void)
{
  print_debug("> moving <\n");
  toggleLight((bool)getEveningTime());

  if (!getEveningTime())
    print_debug("\nNot the evening time --> No light\n");

  lastMovingTime = seconds();
}
void checkDelay(void)
{
  bool shouldBeLight = seconds() - lastMovingTime <= DURATION * MIN;
  fprintf(stderr, "check: seconds: %ld / diff: %ld\n", seconds(), seconds() - lastMovingTime);
  if (!shouldBeLight)
    print_debug("moving timeout --> turn light off\n");
  toggleLight(getEveningTime() && shouldBeLight);
}

void setupPins()
{
  //pinMode(kitchPirS, INPUT);
  //pinMode(kitchRelay, OUTPUT);
  //pullUpDnControl(kitchPirS, PUD_DOWN); // out

  print_debug("wiringPiSetup\n");
  wiringPiSetup();

  print_debug("wiringPiISR...\n");
  wiringPiISR(kitchPirS, INT_EDGE_RISING, &onMove); // in

  isLightOn = digitalRead(kitchRelay);
  print_debug(isLightOn ? "init: light is on\n" : "init: light is off\n");
}

// // @returns epoch secs
// unsigned long getTimestamp () {
//   return startedAt + seconds();
// }

/*
 *********************************************************************************
 * main
 *********************************************************************************
 */

int main(int argc, char *argv[])
{
  setbuf(stdout, NULL); // disable buffering. write logs immediately for best reliability
  setbuf(stderr, NULL); // disable buffering. write logs immediately for best reliability

  setupPins();

  //printf (" Int on pin %d: Counter: %5d\n", pin, globalCounter [pin]) ;
  print_debug("waiting...\n");

  // nope. keep working. look to wiringPiISR that doing actual irq listening work
  for (;;)
  {
    checkDelay();
    sleep(15); // seconds
  }

  return 0;
}

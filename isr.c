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

#define EVENING_FROM 16 /* hours */
#define EVENING_UPTO 1  /* hours, must be >= 0 */
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
unsigned long startedAt = null;
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

void int_str(int i, char *s)
{
  sprintf(s, "%d", i);
}

void date_time_str(char *result_str)
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  const unsigned int hour = lt->tm_hour + 3, min = lt->tm_min;
  char hour_s[10] = "", min_s[10] = "";
  int_str(hour, hour_s), int_str(min, min_s);
  strcat(result_str, hour_s), strcat(result_str, ":"), strcat(result_str, min_s);
}

void print_debug(const char *str)
{
  char buf[2048] = "";
  char date_time[40] = "";
  date_time_str(date_time);

  strcat(buf, date_time);
  strcat(buf, ": ");
  strcat(buf, str);
  fprintf(stderr, buf);
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
  lastMovingTime = seconds();

  print_debug("> moving <\n");
  toggleLight((bool)getEveningTime());

  if (!getEveningTime())
    print_debug("Not the evening time --> No light\n");
}
void checkDelay(void)
{
  bool shouldBeLight = seconds() - lastMovingTime <= DURATION * MIN;
  fprintf(stderr, "check: seconds: %ld / diff: %ld\n", seconds(), seconds() - lastMovingTime);
  /*
   * don't turn-on by the timer.
   * usecase: light might be turnel off via switch button or api
  */
  if (!shouldBeLight) {
    print_debug("moving timeout --> turn light off\n");
    toggleLight(getEveningTime() && shouldBeLight);
  }
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

/*
 *********************************************************************************
 * main
 *********************************************************************************
 */

int main(int argc, char *argv[])
{
  setbuf(stdout, NULL); // disable buffering. write logs immediately for best reliability
  setbuf(stderr, NULL); // disable buffering. write logs immediately for best reliability

  startedAt = seconds();

  setupPins();

  //printf (" Int on pin %d: Counter: %5d\n", pin, globalCounter [pin]) ;
  print_debug("waiting...\n");

  if (getEveningTime())
    lastMovingTime = seconds();

  for (;;)
  {
    // Не начинать проверки сразу после старта
    if (seconds() - startedAt >= DURATION * 60) checkDelay();

    sleep(15); // seconds
  }

  return 0;
}

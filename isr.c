// compilation & running:
// sudo make NO_ACTIVE_TIME_LIMIT=1 LIGHT_PIN=8 PIR_S_PIN=0 CORR_TIME=0 DURATION=20

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
// #include "debounce.h"
// #include "rest.h"
// #include "cJSON/cJSON.h"

#define EVENING_FROM (20) /* hours */
#define EVENING_UPTO (2)  /* hours, must be >= 0 */
#ifndef DURATION          /* might be defined through Makefile */
#define DURATION (20)     /* minutes, how long to be light since latest movement */
#endif
// #define CORR_TIME (3)
// #define NO_ACTIVE_TIME_LIMIT

// wiringpi numbers; look at gpio readall for reference
#ifndef PIR_S_PIN /* might be defined through Makefile */
#define PIR_S_PIN (15)
#define LIGHT_PIN (3)
#endif

int lastMovingTime = null; // sec
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

void int_str(int i, char *s)
{
  sprintf(s, "%d", i);
}

void date_time_str(char *result_str)
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  const unsigned int hour = lt->tm_hour + CORR_TIME, min = lt->tm_min;
  char hour_s[4] = "", min_s[4] = "";
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
  return digitalRead(PIR_S_PIN);
}

time_t seconds()
{
  return time(NULL);
}

bool toggleLight(bool isOnNext)
{
  // Кстати вызов нельзя кешировать глобально, т.к. свет может быть переключен снаружи
  const bool isLightOn = digitalRead(LIGHT_PIN);
  if (isLightOn == isOnNext)
    return isOnNext;
  fprintf(stderr, "effective toggle light. current: %d / request: %d\n", isLightOn, isOnNext);
  system("mpg321 ./beep.mp3");
  digitalWrite(LIGHT_PIN, isOnNext);
  return isOnNext;
}

// evening time
bool getActiveTime()
{
#if NO_ACTIVE_TIME_LIMIT != 1
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  const unsigned char hour = lt->tm_hour + CORR_TIME;
  const bool yes = hour >= EVENING_FROM || hour <= EVENING_UPTO;
  print_debug("hour: ");
  fprintf(stderr, "%d\n", hour); // print_debug

  return yes;
#else
  return true;
#endif
}
void onMove(void)
{
  print_debug("> moving <\n");
  toggleLight((bool)getActiveTime());

  if (!getActiveTime())
    print_debug("Not the evening time --> No light\n");

  lastMovingTime = seconds();
}
void checkDelay(void)
{
  bool shouldBeLight = seconds() - lastMovingTime <= DURATION * MIN;
  fprintf(stderr, "check: seconds: %ld / diff: %ld\n", seconds(), seconds() - lastMovingTime);
  if (!shouldBeLight)
    print_debug("moving timeout --> turn light off\n");
  if (!lastMovingTime)
    return;
  toggleLight(getActiveTime() && shouldBeLight);
}

void setupPins()
{
  //pinMode(PIR_S_PIN, INPUT);
  //pinMode(LIGHT_PIN, OUTPUT);
  //pullUpDnControl(PIR_S_PIN, PUD_DOWN); // out

  print_debug("wiringPiSetup\n");
  wiringPiSetup();

  print_debug("wiringPiISR...\n");
  wiringPiISR(PIR_S_PIN, INT_EDGE_RISING, &onMove); // in

  const bool isLightOn = digitalRead(LIGHT_PIN);
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

  print_debug("waiting...\n");

  // nope. keep working. look to wiringPiISR that doing actual irq listening work
  for (;;)
  {
    checkDelay();
    sleep(15); // seconds
  }

  return 0;
}

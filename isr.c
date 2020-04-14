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

// #include "signal.h" -- пытались получать через SIGUSR сингалы с homekit

int SUNSET_HOURS[] = {16, 17, 18, 19, 20, 21, 21, 20, 19, 18, 17, 16}; // by the monts

//define ACTIVE_SINCE; /* hours */ -- use get_sunset_hour instead
#define ACTIVE_UPTO 1  /* hours, must be >= 0 */
#define DURATION 20     /* minutes, how long to be light since latest movement */
#define NIGHTY_DURATION 4
#define DAYTIME_DURATION 8

// globalCounter:
//	Global variable to count interrupts
//	Should be declared volatile to make sure the compiler doesn't cache it.

//static volatile int globalCounter [8] ;

static unsigned int kitchPirS = 15; // wiringpi id; phisical: 8
static unsigned int kitchSw = 10; // wiringpi id; phisical: 24
static unsigned int kitchRelay = 3; // wiringpi id; phisical: 15

unsigned long lastOnTime = 0; // sec; when light turned on recently
unsigned long lastSwToggleMs = 0 ; // millisec
unsigned long startedAt = null;

bool prevMoving = false;
bool prevEveningTime = null;
bool forceOn = false; // treat moving at night/day
bool forceOff = false; // don't treat moving at evening
bool isLightOn = false;
bool isExternalToggleProcessing = false;

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

int get_sunset_hour()
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  int month = lt->tm_mon; // 0..11
  return SUNSET_HOURS[month];
}

unsigned get_hour()
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);

  return lt->tm_hour + 3 >= 24 ? lt->tm_hour + 3 - 24 : lt->tm_hour + 3;
}

void date_time_str(char *result_str)
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  const unsigned int hour = get_hour();
  const unsigned int min = lt->tm_min;
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

bool getLightOn(void)
{
  return digitalRead(kitchRelay);
}

time_t seconds()
{
  return time(NULL);
}

bool toggleLight(bool isOn)
{
  if (getLightOn() == isOn)
    return isOn;
  fprintf(stderr, "effective toggle light. current: %d / request: %d\n", isLightOn, isOn);
  digitalWrite(kitchRelay, isOn);
  return isLightOn = isOn;
}

bool getEveningTime()
{
  const unsigned hour = get_hour();
  const bool yes = hour >= get_sunset_hour() || hour <= ACTIVE_UPTO;
  // print_debug("hour: ");
  // fprintf(stderr, "%d\n", hour); // print_debug

  return yes;
}
bool getCanBeLight()
{
  return forceOn || (getEveningTime() && !forceOff);
}

bool onExternalOn()
{
  if (isExternalToggleProcessing)
  {
    print_debug("skip onExternalOn\n");
    return isLightOn;
  }
  isExternalToggleProcessing = true;

  fprintf(stderr, "on external: cached: %d / actual: %d\n", isLightOn, getLightOn());
  print_debug("external on\n");

  lastOnTime = seconds();	
  if (getEveningTime() && forceOff) // undo
  {
    print_debug("undo force off\n");
    forceOff = false;
  }
  else if (!getEveningTime())
  {
    print_debug("force on\n");
    forceOn = true;
  }

  isExternalToggleProcessing = false;
  return true;
}
bool onExternalOff()
{
  if (isExternalToggleProcessing)
  {
    print_debug("skip onExternalOff\n");
    return isLightOn;
  }
  isExternalToggleProcessing = true;

  fprintf(stderr, "on external: cached: %d / actual: %d\n", isLightOn, getLightOn());
  print_debug("external off\n");

  // выключить до конча периода
  if (!getEveningTime() && forceOn) // undo
  {
    print_debug("undo force on\n");
    forceOn = false;
  }
  else if (getEveningTime())
  {
    print_debug("force off\n");
    forceOff = true;
  }

  isExternalToggleProcessing = false;
  return false;
}

void checkProcessExternalToggle(bool shouldLightOn)
{
  // У нас нет подписки на внешние события включения/выключения
  // если такое переключение произошло, закешированное тут состояние устарело
  // нужно его обновить, а за одно возможно выключить автоматику
  if (isLightOn != shouldLightOn)
    isLightOn = shouldLightOn ? onExternalOn() : onExternalOff();
}

void onMove(void)
{
  lastOnTime = seconds();

  bool shouldLightOn = getLightOn();
  // проверяем, не переключили ли недавно свет из вне
  checkProcessExternalToggle(shouldLightOn);
  if (isLightOn != shouldLightOn)
    fprintf(stderr, "(move) after external-external processed: isLightOn: %d\n", isLightOn);

  print_debug("> moving <\n");
  if ((bool)getCanBeLight()) toggleLight(true);

  if (forceOn)
    print_debug("Light forced on\n");
  if (!getEveningTime())
    print_debug("Not the evening time --> Should no light\n");
  if (forceOff)
    print_debug("Force off --> Should no light\n");
}

void onSwToggle(void)
{
  if (millis() - lastSwToggleMs < 233)
  {
     print_debug("debounce: skip sw toggle\n");
     return;
  }

  fprintf(stderr, "button toggle light. current: %d\n", isLightOn);
  checkProcessExternalToggle(!isLightOn);
  fprintf(stderr, "(button) after external processed: isLightOn: %d\n", isLightOn);
  digitalWrite(kitchRelay, isLightOn);

  lastSwToggleMs = millis();
}

unsigned get_duration(unsigned hour)
{
  // when user forced light on
  if (hour >= get_sunset_hour() + 2) return NIGHTY_DURATION;
  if (hour < get_sunset_hour()) return DAYTIME_DURATION;
  // usual evening routine
  return DURATION;
}

void check_delay()
{
  const unsigned hour = get_hour();
  const unsigned duration = get_duration(hour);
  const bool shouldBeLight = seconds() - lastOnTime <= duration * MIN;
  fprintf(stderr, "check: seconds: %ld / diff: %ld\n", seconds(), seconds() - lastOnTime);
  /*
   * don't turn-on by the timer.
   * usecase: light might be turned off via switch button or api
  */
  if (!shouldBeLight) {
    print_debug("moving timeout --> turn light off\n");
    toggleLight(getCanBeLight() && shouldBeLight);
  }

  if ((forceOn || forceOff) && getEveningTime() != prevEveningTime)
  {
    print_debug("period changed --> force mode finished");
    forceOn = false;
    forceOff = false;
  }

  prevEveningTime = getEveningTime();
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
  wiringPiISR(kitchSw, INT_EDGE_BOTH, &onSwToggle); // in

  isLightOn = getLightOn();

  print_debug(isLightOn ? "init: light is on\n" : "init: light is off\n");
}

void setupExternalSignals()
{
  // Хотели получать сигналы включения/выключения из вне, но программа падает от второго подряд сигнала
  // signal(SIGUSR1, &onExternalOn);
  // signal(SIGUSR2, &onExternalOff);
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

  // setupExternalSignals();

  //printf (" Int on pin %d: Counter: %5d\n", pin, globalCounter [pin]) ;
  print_debug("waiting...\n");

  if (getEveningTime())
    lastOnTime = seconds();

  for (;;)
  {
    // У нас нет подписки на внешние события включения/выключения
    // если такое переключение произошло, закешированное тут состояние устарело
    // нужно его обновить, а за одно возможно выключить автоматику
    if (isLightOn != getLightOn() && seconds() - startedAt >= 30)
    {
      isLightOn = getLightOn() ? onExternalOn() : onExternalOff();
      fprintf(stderr, "(cycle) after external-external processed: isLightOn: %d\n", isLightOn);
    }

    // Не начинать проверки сразу после старта
    const unsigned hour = get_hour();
    const unsigned duration = get_duration(hour);
    if (seconds() - startedAt >= duration * MIN) check_delay();

    fprintf(stderr, "debug: cached: %d, actual: %d, forceOn: %d, forceOff: %d\n", isLightOn, digitalRead(kitchRelay), forceOn, forceOff);

    sleep(30); // seconds
  }

  return 0;
}

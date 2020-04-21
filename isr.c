// compilation & running:
// sudo make ACTIVE_TIME_LIMIT=0 NIGHT_LIGHT_R_PIN=8 PIR_S_PIN=0 CORR_TIME=0 DURATION=20

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

// #include "mqtt.h"

#ifndef ACTIVE_TIME_LIMIT
#define ACTIVE_TIME_LIMIT 0
#endif

#define ACTIVE_SINCE (8) /* hours */
#define SILENT_SINCE (23)  /* hours, must be >= 0 */
#define ACTIVE_UPTO (0)  /* hours, must be >= 0 */

#define SILENT_FAN_DURATION (3) /* min */

#define CHECK_DELAY 25 /* sec */

#ifndef DURATION      /* might be defined through Makefile */
#define DURATION (0) /* minutes, how long to be light since latest movement */
#endif

#ifndef CORR_TIME
#define CORR_TIME 0
#endif

#define HOUR (24 * 60) /* sec */
#define MIN (60)

// as a mqtt client this app will be use mqtt signals instead local sensor pin
#ifndef MQTT_CLIENT
#define MQTT_CLIENT 0
#define MQTT_TOPIC "home/light"
#define MQTT_BROKER_HOST "localhost"
#endif

// wiringpi numbers; look at gpio readall for reference
#ifndef PIR_S_PIN /* might be defined through Makefile */
#define PIR_S_PIN (0)
#define DOOR_S_PIN (0)

#define NIGHT_LIGHT_R_PIN (0)
#define MAIN_LIGHT_R_PIN (0)
#define FAN_R_PIN (0)

#define LIGHT_SW_PIN  (0)
#endif

int lastMovingTime = null; // sec
int fanStartedAt = null; // sec
unsigned long startedAt = null; // sec, since 1970 aka epoch
unsigned long startMovingAt = null; 
unsigned long doorClosedAt = null; 
bool hasMovAfterDoorClosed = null;

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
  const unsigned int mon = lt->tm_mon + 1, day = lt->tm_mday, hour = lt->tm_hour + CORR_TIME, min = lt->tm_min;
  char mon_s[4] = "", day_s[4] = "", hour_s[4] = "", min_s[4] = "";
  int_str(mon, mon_s), int_str(day, day_s), int_str(hour, hour_s), int_str(min, min_s);
  strcat(result_str, mon_s), strcat(result_str, "/"), strcat(result_str, day_s);
  strcat(result_str, " "), strcat(result_str, hour_s), strcat(result_str, ":"), strcat(result_str, min_s);
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
unsigned char getHour()
{
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  unsigned char h = lt->tm_hour + CORR_TIME;

  return h >= 24 ? h - 24 : h;
}

bool checkMainLightTime()
{
  unsigned char hour = getHour();
  unsigned char upto = ACTIVE_UPTO == 0 ? 24 : ACTIVE_UPTO;

  bool upto_pm = upto > ACTIVE_SINCE;
  return upto_pm
    ? (hour >= ACTIVE_SINCE && hour < upto)
    : (hour >= ACTIVE_SINCE || hour < upto);
}

bool checkAnyLightOn(void)
{
  return digitalRead(NIGHT_LIGHT_R_PIN) || digitalRead(MAIN_LIGHT_R_PIN);
}

void toggleAnyLight(bool shouldBeOn)
{
  digitalWrite(MAIN_LIGHT_R_PIN, checkMainLightTime() && shouldBeOn);
  digitalWrite(NIGHT_LIGHT_R_PIN, !checkMainLightTime() && shouldBeOn);
}


// Переключает свет и вытяжку. isFanOn === !isLightOn
bool toggleLightFan(bool isOnNext)
{
  // Кстати вызов нельзя кешировать глобально, т.к. свет может быть переключен снаружи
  const bool isLightOn = checkAnyLightOn();
  const bool shouldFanOn = !isOnNext;

  if (shouldFanOn && !fanStartedAt) fanStartedAt = seconds();
  if (isLightOn != isOnNext)
  {
    char date_time[40] = "";
    date_time_str(date_time);

    fprintf(stderr, "%s: effective toggle light. current: %d / request: %d ~~ fan: %d, started: %d\n", date_time, isLightOn, isOnNext, shouldFanOn, fanStartedAt);
    // if (!isLightOn) system("mpg321 ./beep.mp3");

    toggleAnyLight(isOnNext);
    digitalWrite(FAN_R_PIN, shouldFanOn);
  }
  return isOnNext;
}

void onMove(void)
{
  print_debug("> moving <\n");
  toggleLightFan(true);

  lastMovingTime = seconds();
  if (!checkAnyLightOn()) startMovingAt = seconds();
#if MQTT_CLIENT == 0
  // mqtt_send("mov", MQTT_TOPIC);
#endif
}
void onDoorChanged(void)
{
  bool isDoorOpen = digitalRead(DOOR_S_PIN);
  print_debug(isDoorOpen ? "// door opened\n" : "// door closed\n");
  if (isDoorOpen)
  {
    lastMovingTime = seconds();
    startMovingAt = seconds();
    toggleLightFan(true);
  }
  else
  {
    doorClosedAt = seconds();
  }
}

void checkDelay(void)
{
  bool isLightOn = checkAnyLightOn();
  bool shouldBeLight = seconds() - lastMovingTime <= DURATION * MIN;
  if (!lastMovingTime)
    return;

  //
  // LIGHT
  //

  if (isLightOn)
  {
    fprintf(stderr, "check: diff: %ld, light is on ~~ fan started at: %d, fan on: %d // last_mov-door_closed: %ld\n", seconds() - lastMovingTime, fanStartedAt, digitalRead(FAN_R_PIN), lastMovingTime - doorClosedAt);
  }

  if (!shouldBeLight && isLightOn)
  {
    print_debug("moving timeout --> turn light off\n");
    toggleLightFan(false);
  }

  //
  // FAN
  //

  // fanStartedAt определяется после toggleLightFan
  unsigned char hour = getHour();
  bool isSilentHour = hour >= SILENT_SINCE || hour < ACTIVE_SINCE;
  bool isLastMovingTooShort = (lastMovingTime - startMovingAt) <= 1 * MIN;
  unsigned fanDuration = (isSilentHour || isLastMovingTooShort ? SILENT_FAN_DURATION : DURATION) * MIN;
  bool shouldFanOn = seconds() - fanStartedAt <= fanDuration;
  if (!shouldFanOn && fanStartedAt) {
    fprintf(stderr, "fan is over --> turn off / now: %ld, start: %d, diff: %ld, duration: %ldsec, silent: %d, movShort: %d, duration: %dmin, silent (real): %d, last moving duration: %ldsec, 1min: %d\n", seconds(), fanStartedAt, seconds() - fanStartedAt, fanDuration, isSilentHour, isLastMovingTooShort, isSilentHour || isLastMovingTooShort ? SILENT_FAN_DURATION : DURATION, hour >= SILENT_SINCE || hour < ACTIVE_SINCE, lastMovingTime - startMovingAt, MIN);
    digitalWrite(FAN_R_PIN, 0);
    fanStartedAt = null;
  }

  //
  // DOOR
  //

  if ((long)lastMovingTime - (long)doorClosedAt <= 5L && isLightOn)
  {
    if (seconds() - doorClosedAt >= 1 * MIN) {
      fprintf(stderr, "no hasMovAfterDoorClosed && 1 min gone\n");
      toggleLightFan(false);
    } else {
      fprintf(stderr, "// wait 1min until light off //\n");
    }
  }
}

void setupPins()
{
  print_debug("wiringPiSetup\n");
  wiringPiSetup();

  print_debug("wiringPiISR...\n");
  wiringPiISR(PIR_S_PIN, INT_EDGE_RISING, &onMove); // input mode; on high trigger
  wiringPiISR(DOOR_S_PIN, INT_EDGE_BOTH, &onDoorChanged); // input mode; on low trigger

  const bool isLightOn = false;
  digitalWrite(NIGHT_LIGHT_R_PIN, isLightOn);
  digitalWrite(MAIN_LIGHT_R_PIN, isLightOn);
  digitalWrite(FAN_R_PIN, false);
  print_debug("light & fan setted up to false\n");
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

  // mqtt_setup(MQTT_BROKER_HOST);

  print_debug("waiting...\n");

  // nope. keep working. look to wiringPiISR that doing actual irq listening work
  for (;;)
  {
    checkDelay();
    sleep(CHECK_DELAY); // seconds
  }

  return 0;
}

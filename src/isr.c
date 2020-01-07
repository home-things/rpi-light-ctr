// compilation & running:
// sudo make ACTIVE_TIME_LIMIT=0 LIGHT_PIN=8 PIR_S_PIN=0 CORR_TIME=0 DURATION=20

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

#define EVENING_FROM (20) /* hours */
#define EVENING_UPTO (2)  /* hours, must be >= 0 */

#ifndef DURATION      /* might be defined through Makefile */
#define DURATION (20) /* minutes, how long to be light since latest movement */
#endif

#ifndef CORR_TIME
#define CORR_TIME 0
#endif

#ifndef ACTIVE_TIME_LIMIT
#define ACTIVE_TIME_LIMIT 0
#endif

// as a mqtt subscriber this app will be use mqtt signals instead local sensor pin
#ifndef MQTT_SUBSCRIBE
#define MQTT_SUBSCRIBE 0
#define MQTT_TOPIC "home/light"
#define MQTT_BROKER_HOST "localhost"
#endif

// wiringpi numbers; look at gpio readall for reference
#ifndef PIR_S_PIN /* might be defined through Makefile */
#define PIR_S_PIN (15)
#define LIGHT_PIN (3)
#endif

int last_moving_time = null; // sec
bool prev_moving = false;
unsigned long started_at = null; // sec, since 1970 aka epoch
const unsigned HOUR = 24 * 60;   // sec
const unsigned MIN = 60;         // sec

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
  unsigned char h = lt->tm_hour + CORR_TIME;
  const unsigned int mon = lt->tm_mon, day = lt->tm_mday, hour = h >= 24 ? h - 24 : h, min = lt->tm_min;
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

bool has_moving(void)
{
  return digitalRead(PIR_S_PIN);
}

time_t seconds()
{
  return time(NULL);
}

bool toggle_tight(bool isOnNext)
{
  // Кстати вызов нельзя кешировать глобально, т.к. свет может быть переключен снаружи
  const bool isLightOn = digitalRead(LIGHT_PIN);
  digitalWrite(LIGHT_PIN, isOnNext);
  if (isLightOn != isOnNext)
  {
    char date_time[40] = "";
    date_time_str(date_time);

    fprintf(stderr, "%s: effective toggle light. current: %d / request: %d\n", date_time, isLightOn, isOnNext);
    if (!isLightOn)
      system("mpg321 ./beep.mp3");
  }
  return isOnNext;
}

// evening time
bool get_active_time()
{
#if ACTIVE_TIME_LIMIT == 1
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  unsigned char h = lt->tm_hour + CORR_TIME;
  unsigned char hour = h >= 24 ? h - 24 : h;

  return hour >= EVENING_FROM || hour <= EVENING_UPTO;
#else
  return true;
#endif
}
void on_move(void)
{
  last_moving_time = seconds();

  print_debug("> moving <\n");
  toggle_tight((bool)get_active_time());

  if (!get_active_time())
    print_debug("Not the evening time --> No light\n");
  // #if MQTT_SUBSCRIBE == 0
  //   // as mqtt publisher
  //   mqtt_send("mov", MQTT_TOPIC);
  // #endif
}
void check_delay(void)
{
  bool shouldBeLight = seconds() - last_moving_time <= DURATION * MIN;
  if (!last_moving_time)
    return;
  fprintf(stderr, "check: seconds: %ld / diff: %ld\n", seconds(), seconds() - last_moving_time);
  /*
   * don't turn-on by the timer.
   * usecase: light might be turnel off via switch button or api
  */
  if (!shouldBeLight)
  {
    print_debug("moving timeout --> turn light off\n");
    toggle_tight(get_active_time() && shouldBeLight);
  }
}

void setup_pins()
{
  //pinMode(PIR_S_PIN, INPUT);
  //pinMode(LIGHT_PIN, OUTPUT);
  //pullUpDnControl(PIR_S_PIN, PUD_DOWN); // out

  print_debug("wiringPiSetup\n");
  wiringPiSetup();

  print_debug("wiringPiISR...\n");

  // #if MQTT_SUBSCRIBE != 0
  // as mqtt subscriber
  wiringPiISR(PIR_S_PIN, INT_EDGE_RISING, &on_move); // in
                                                     // #else
                                                     //   mqtt_subscribe(&on_move);
                                                     // #endif
  const bool isLightOn = digitalRead(LIGHT_PIN);
  print_debug(isLightOn ? "init: light is on\n" : "init: light is off\n");
}

// // @returns epoch secs
// unsigned long getTimestamp () {
//   return started_at + seconds();
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

  setup_pins();

  // mqtt_setup(MQTT_BROKER_HOST);

  print_debug("waiting...\n");

  // nope. keep working. look to wiringPiISR that doing actual irq listening work
  for (;;)
  {
    check_delay();
    sleep(15); // seconds
  }

  return 0;
}

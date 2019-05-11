// std
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// mqtt
#include <mosquitto.h>

#include "mqtt.h"

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
  /* Pring all log messages regardless of level. */

  switch (level)
  {
  //case MOSQ_LOG_DEBUG:
  //case MOSQ_LOG_INFO:
  //case MOSQ_LOG_NOTICE:
  case MOSQ_LOG_WARNING:
  case MOSQ_LOG_ERR:
  {
    fprintf(stderr, "%i:%s\n", level, str);
  }
  }
}

struct mosquitto *mosq = NULL;
void mqtt_setup(char *broker_host)
{

  int port = 1883;
  int keepalive = 60;
  bool clean_session = true;

  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, clean_session, NULL);
  if (!mosq)
  {
    fprintf(stderr, "Error: Out of memory.\n");
    exit(1);
  }

  mosquitto_log_callback_set(mosq, mosq_log_callback);

  if (mosquitto_connect(mosq, broker_host, port, keepalive))
  {
    fprintf(stderr, "Unable to connect.\n");
    exit(1);
  }
  int loop = mosquitto_loop_start(mosq);
  if (loop != MOSQ_ERR_SUCCESS)
  {
    fprintf(stderr, "Unable to start loop: %i\n", loop);
    exit(1);
  }
}

int mqtt_send(char *msg, char *topic)
{
  return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, 0);
}

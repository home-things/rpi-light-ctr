// std
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// mqtt
#include <mosquitto.h>

#include "mqtt.h"

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
  /* Pring all log messages regardless of level. */

  fprintf(stderr, "log: %i:%s\n", level, str);

  switch (level)
  {
  case MOSQ_LOG_DEBUG:
  case MOSQ_LOG_INFO:
  case MOSQ_LOG_NOTICE:

  case MOSQ_LOG_WARNING:
  case MOSQ_LOG_ERR:
  {
    fprintf(stderr, "%i:%s\n", level, str);
  }
  }
}


static int run = 1;

void handle_signal(int s)
{
	run = 0;
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	printf("connect callback, rc=%d\n", result);
}

// example
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	bool match = 0;
	printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	mosquitto_topic_matches_sub("/home/#", message->topic, &match);
	if (match) {
		printf("got message for home topic\n");
	}
}

struct mosquitto *mosq = NULL;







void mqtt_setup(char *broker_host)
{

  int port = 1883;
  int keepalive = 60;
  bool clean_session = true;


	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);


  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, clean_session, NULL);
  if (!mosq)
  {
    fprintf(stderr, "MQTT Error: Out of memory.\n");
    exit(1);
  }


  mosquitto_log_callback_set(mosq, mosq_log_callback);
  mosquitto_connect_callback_set(mosq, connect_callback);
  mosquitto_message_callback_set(mosq, message_callback);

  if (mosquitto_connect(mosq, broker_host, port, keepalive))
  {
    fprintf(stderr, "MQTT Error: Unable to connect.\n");
    exit(1);
  }
  int loop = mosquitto_loop_start(mosq);
  if (loop != MOSQ_ERR_SUCCESS)
  {
    fprintf(stderr, "MQTT Error: Unable to start loop: %i\n", loop);
    exit(1);
  }

  fprintf(stderr, "MQTT: Success\n");

}

int mqtt_send(char *msg, char *topic)
{
  /*
    https://mosquitto.org/api/files/mosquitto-h.html#mosquitto_publish
    int mosquitto_publish(
      mosquitto 	*	mosq,
      int 	          *	mid,
      const 	char 	*	topic,
      int 		        payloadlen,
      const 	void 	*	payload,
      int 		        qos,
      bool 		        retain
    )

    @returns
    MOSQ_ERR_SUCCESS	on success.
    MOSQ_ERR_INVAL	if the input parameters were invalid.
    MOSQ_ERR_NOMEM	if an out of memory condition occurred.
    MOSQ_ERR_NO_CONN	if the client isn’t connected to a broker.
    MOSQ_ERR_PROTOCOL	if there is a protocol error communicating with the broker.
    MOSQ_ERR_PAYLOAD_SIZE	if payloadlen is too large.
    MOSQ_ERR_MALFORMED_UTF8	if the topic is not valid UTF-8
  */
  return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, 0);
}

// int mqtt_subscribe(char *cb, char *topic)
// {
//   /*
//     int mosquitto_subscribe(
//       mosquitto 	  *	mosq,
//       int 	        *	mid,
//       const 	char 	*	sub,
//       int 		      qos
//     )
//   */
//   return mosquitto_subscribe(mosq, NULL, topic, strlen(msg), msg, 0, 0);
// }

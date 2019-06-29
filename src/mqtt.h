void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
void mqtt_setup(char *broker_host);
int mqtt_send(char *msg, char *topic);

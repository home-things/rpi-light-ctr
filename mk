#!/usr/bin/env sh
cd ~/services/light-isr/
sudo make ACTIVE_TIME_LIMIT=1 LIGHT_PIN=3 PIR_S_PIN=15 CORR_TIME=3 DURATION=20 MQTT_SUBSCRIBE=1 MQTT_TOPIC=home/light MQTT_BROKER_HOST='"192.168.0.144"' # orpik.local

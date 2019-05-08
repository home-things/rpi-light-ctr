# rpi-light-ctr
Yet another light controller for raspberry pi. ISR Interruptons. C 

Hub repo: [home-things/rpi-tg-bot](https://github.com/home-things/rpi-tg-bot)

### compile & run
```
sudo make NO_ACTIVE_TIME_LIMIT=0 LIGHT_PIN=8 PIR_S_PIN=0 CORR_TIME=0 DURATION=20
```

### crontab
```
@reboot gpio mode 8 out # light (addition; ceiling)
@reboot gpio mode 0 in  # pir_s
@reboot cd /home/pi/services/light-ctr; ./isr 2>>./log
```

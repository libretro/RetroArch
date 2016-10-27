export WIILOAD=tcp:10.42.0.170
wiiload retroarch_wiiu.elf
netcat -p 4405 -l 10.42.0.170

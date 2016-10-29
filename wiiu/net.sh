export WIILOAD=tcp:$1
powerpc-eabi-strip retroarch_wiiu.elf -o retroarch_wiiu_stripped.elf
wiiload retroarch_wiiu_stripped.elf
netcat -p 4405 -l $1

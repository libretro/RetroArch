export WIILOAD=tcp:$1
rm $2.stripped -rf
powerpc-eabi-strip $2 -o $2.stripped
wiiload $2.stripped
#netcat -p 4405 -l $1

# calling netcat directly after wiiload is unreliable, better use something like :
# for i in {1..20}; do echo; echo == $i ==; echo; netcat -p 4405 -l <wiiu ip>; done
# from a different terminal to continuously listen for incoming connections

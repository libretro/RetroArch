/* public domain */
/* gcc -o udptest udp-test.c */

/*
   will send "RETROPAD RIGHT" indefinely to player 1
   to send to player 2 change port to 55401 and so on
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER "127.0.0.1"
#define PORT 55400

void die(char *s)
{
    perror(s);
    exit(1);
}

int main(void)
{
   struct sockaddr_in si_other;
   int s, i, slen=sizeof(si_other);

   if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
      die("socket");

   memset((char *) &si_other, 0, sizeof(si_other));
   si_other.sin_family = AF_INET;
   si_other.sin_port = htons(PORT);

   if (inet_aton(SERVER , &si_other.sin_addr) == 0)
   {
      fprintf(stderr, "inet_aton() failed\n");
      exit(1);
   }

   for (;;)
   {
      char message[10]="128";
      /* send the message */
      if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
         die("sendto()");

      /* sleep for 1 frame (60hz) */
      usleep(16*1000);
   }

   close(s);
   return 0;
}

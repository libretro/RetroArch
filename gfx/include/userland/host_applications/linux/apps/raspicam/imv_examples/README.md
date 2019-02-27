Post-processing example programs to play with the imv buffer:
=============================================================

Compile:
--------
gcc imv2pgm.c -o imv2pgm -lm

gcc imv2txt.c -o imv2txt

Record and split buffer:
------------------------
raspivid -x test.imv -o test.h264

We need to split the buffer first using split:

split -a 4 -d -b $(((120+1)\*68\*4)) test.imv frame-

Play:
-----
Now we can transform the velocity magnitues to a pgm image

./imv2pgm frame-0001 120 68 frame-0001.pgm

Or loop over all frames and create a movie

for i in frame-????; do ./imv2pgm $i 120 68 $i.pgm;convert $i.pgm $i.png; rm $i.pgm; done

avconv -i frame-%04d.png motion.avi

And we can create a text file for easy plotting

./imv2txt frame-0001 120 68 frame-0001.dat

This textfile has in each line the center position x y of the macro block and
the velocities u and v and the sum of differences sad.

These can be plot with xmgrace

xmgrace -autoscale none -settype xyvmap frame-0001.dat -param plot.par

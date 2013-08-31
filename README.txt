
Baoclone is a utility for programming Baofeng radios via a serial or USB
programming cable.  Supported radios:

 * Baofeng UV-5R
 * Baofeng UV-B5
 * Baofeng BF-888S


=== Usage ===

Save device binary image to file 'device.img', and text configuration
to 'device.conf':

    baoclone [-v] port

Write image to device.

    baoclone -w [-v] port file.img

Configure device from text file.
Previous device image saved to 'backup.img':

    baoclone -c [-v] port file.conf

Show configuration from image file:

    baoclone file.img

Option -v enables tracing of a serial protocol to the radio:


=== Sources ===

Sources are distributed freely under the terms of MIT license.
You can download sources via SVN:

    git clone https://code.google.com/p/baoclone/
___
Regards,
Serge Vakulenko
KK6ABQ

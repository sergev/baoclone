
Baoclone is a utility for programming handheld radios via a serial or USB
programming cable.  Supported radios:

 * Baofeng UV-5R
 * Baofeng UV-B5
 * Baofeng BF-888S

Web site of the project: https://github.com/sergev/baoclone/wiki


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


=== Example ===

For example:
    C:\> baoclone.exe COM5
    Connect to COM5.
    Detected Baofeng UV-5R.
    Read device: ################################################## done.
    Radio: Baofeng UV-5R
    Firmware: Ver  BFB291
    Serial: 120801NB5R0001
    Close device.
    Write image to file 'device.img'.
    Print configuration to file 'device.conf'.


=== Configurations ===

You can use these files as examples or templates:
 * uv-5r-factory.conf     - Factory configuration for UV-5R.
 * uv-b5-factory.conf     - Factory configuration for UV-B5.
 * bf-888s-factory.conf   - Factory configuration for BF-888S.
 * bf-t1-factory.conf     - Factory configuration for BF-T1.
 * uv-5r-sunnyvale.conf,
   uv-b5-sunnyvale.conf   - Configurations of my personal handhelds
                            for south SF Bay Area, CA.
 * bf-t1-gmrs.conf,
   bf-888s-gmrs.conf      - Configurations for [GMRS service](https://en.wikipedia.org/wiki/General_Mobile_Radio_Service)


=== Sources ===

Sources are distributed freely under the terms of MIT license.
You can download sources like this:

    git clone https://github.com/sergev/baoclone

To build on Linux or Mac OS X, run:
    make
    make install

To build on Windows using MINGW compiler, run:
    gmake -f make-mingw

___
Regards,
Serge Vakulenko
KK6ABQ

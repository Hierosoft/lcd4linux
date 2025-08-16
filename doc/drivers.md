# Drivers

How to write new display drivers for lcd4linux

If you plan to write a new display driver for lcd4linux, you should follow
this guidelines:

* Use Skeleton.c as a start point.
  You might also have a look at Text.c
* Create a new sourcefile `<drivername>.c` and add it to the bottom of
  Makefile.am
* Add an entry to configure.in
* There's no need for a `<drivername>.h`
* Create one (or more) unique display names (your driver will be selected by
  this name in the 'Display'-line of lcd4linux.conf).
* Include "display.h" in your driver, to get the LCD structure and various
  BAR_ definitions
* Include "cfg.h" if you need to access settings in the config file.
* Create a LCD table at the bottom of your driver, and fill it with the
  appropriate values. Take care that you specify the correct bar capabilities
  of your display or driver:
  * BAR_L:  horizontal bars headed left
  * BAR_R:  horizontal bars headed right
  * BAR_H2: driver supports horizontal dual-bars
  * BAR_U:  vertical bars bottom-up
  * BAR_D:  vertical bars top-down
  * BAR_V2: driver supports vertical dual-bars
* Edit display.c and create a reference to your LCD table:
  * external LCD YourDriver[];
* Extend the FAMILY table in display.c with your driver:
```C
     FAMILY Driver[] = {
       { "Skeleton",      Skeleton },
       { "MatrixOrbital", MatrixOrbital },
       { "YourFamily",    YourDriver },
       { "" }
     };
```
* Write the corresponding init(), clear(), put(), bar(), quit() and
  flush()-functions. There's no need to use a framebuffer and display its
  contents with the flush()- call (as in MatrixOrbital.c), you can directly
  write to the display in the put()- and bar()-functions, and use an empty
  flush()-function. But if you have a limited number of user-defined
  characters, and therefore you have to do some sort of 'character reduction'
  or similar stuff, you will have to use a framebuffer and the flush()-call.

#$Id: README.Drivers,v 1.4 2001/03/09 13:08:11 ltoetsch Exp $

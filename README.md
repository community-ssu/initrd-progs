initrd-progs
======================================
This package initrd-progs contains initfs utilities
for Maemo 5: text2creen, cal-tool and key_pressed.

text2screen was taken from fb_text2screen project
and cal-tool from opendsme project.

Dependencies
------------

Build time only:

  - CMake >= 2.6

Both build and runtime:

 -  libcal (from Maemo 5 SDK)
 -  popt (tested with 1.16)

You can read fb_text2screen and opendsme README
files below.

Authors
-------
 -  Marat Radchenko <marat@slonopotamus.org>
 -  Pali Rohár <pali.rohar@gmail.com>


fb_text2screen - opensource tool for simple text drawing on framebuffer devices
======================================
Latest fb_text2screen sources & docs are available
at <http://github.com/slonopotamus/fb_text2screen>

Dependencies
------------
Build time only:

 -  CMake >= 2.6 (tested with 2.8.1)

Both build and runtime:

 -  popt (tested with 1.16)
 
Compiling
---------
1. Create an empty directory and `cd` into it
2. Run `cmake /path/to/fb_text2screen/sources/dir/`
That'll generate a makefile and various cmake helper things.
3. Run `ccmake .` and configure build options
4. Now, run `make` to compile fb_text2screen sources and `make install` to install

**Attention**, CMake doesn't support `make uninstall`

Documentation
-------------
See `--help` output of `fb_text2screen` program

License
-------
fb_text2screen is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Contact
-------
If you have ideas about fb_text2screen improvement, feel
free to send a message to <marat@slonopotamus.org>


opendsme - opensource dsme replacement
======================================
Latest opendsme sources & docs are available
at <http://github.com/slonopotamus/opendsme>

Dependencies
------------
Build time only:

 -  CMake >= 2.6 (tested with 2.8.1)

Both build and runtime:

 -  popt (tested with 1.16)

Compiling
---------
1. Create an empty directory and `cd` into it
2. Run `cmake /path/to/opendsme/sources/dir/`
That'll generate a makefile and various cmake helper things.
3. Run `ccmake .` and configure build options
4. Now, run `make` to compile opendsme sources and `make install` to install

**Attention**, CMake doesn't support `make uninstall`

Documentation
-------------
See -h output of programs contained in opendsme package

License
-------
opendsme is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Authors
-------
 -  Marat Radchenko <marat@slonopotamus.org>

Contact
-------
If you have ideas about opendsme improvement, feel
free to send a message to <marat@slonopotamus.org>

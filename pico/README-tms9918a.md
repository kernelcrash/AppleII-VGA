Use AppleII-VGA as a base to emulate the TMS9918A VDP
=====================================================

The current code is designed to work with my breadboard MSX1 computer,
so takes some shortcuts compared to what you would need to do if you
were attaching this to an MSX1 computer, or some other proper computer.

- For my MSX1 breadboard computer, I need to wire the AppleII VGA like so

  - A15 to A8 are not connected
  - A7 to A1 are not connected
  - A0 is connected from the MSX1 computer to the A0 pin on the U2 74LVC245
  - D7 to D0 on the MSX1 computer go to the U3 74LVC245
  - On the U4 74LVC245
    - U4 p18 (RW) goes to the MSX1 _WR pin
    - U4 p17 (DEVSEL) goes to the active low IO chip select for 0x98/0x99 (this is a pin on one of the 74LS138's in the breadboard computer)
    - U4 p16 (PHI0) goes to the MSX1 _IORQ pin
    - U4 p15 is just tied to a pullup
  - GND is obviously connected between the MSX1 computer and the AppleII VGA. +5 can be connected or you could power the pico off USB

- For a normal MSX1 computer (or some other TMS9918/28/29 based computer) you would need to adapt
the code in abus.c to read the full address bus, decode the address etc. There's a comment in abus.c
about how you might do this ... or alternately you could just try the v9938 branch. The 9918 emulation
is obviously a subset of v9938 emulation and it works pretty well too .. and importantly the code was
written to work with a real MSX2 computer.

Works quite well. You quite often see some pixels flickering ... which might be easy to fix. Interestingly the 9918 emulation 
that is part of the V9938 branch probably works better. I don't know why.

Compilation is

```
git clone https://github.com/markadev/AppleII-VGA.git ~/AppleII-VGA
cd ~/AppleII-VGA/pico
mkdir build
cd build
cmake -DCOMPUTER_MODEL=TMS9918A -DCMAKE_BUILD_TYPE=Release ..
make
$ ls *uf2
...
applevga.uf2
```

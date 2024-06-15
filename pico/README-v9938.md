Use AppleII-VGA as a base to emulate the v9938 video chip (as used in MSX2)
===========================================================================

WARNING: This does not work properly. It emulates a lot of 9918 modes OK (and
hence a lot of MSX1 games will look fine), but there are major issues with the 
higher modes offered by the V9938. This is left here for 'interest' only.

- Wiring is almost identical to the Apple II implementation. Unlike the tms9918 branch,
  the wiring below is for a real MSX2 computer:
  - A15 to A8 on the MSX cartridge slot go to the U1 74LVC245
  - A7 to A0 on the MSX cartridge slot go to the U2 74LVC245
  - D7 to D0 on the MSX cartridge slot got to the U3 74LVC245
  - On the U4 74LVC245
    - U4 p18 (RW) goes to MSX cartridge slot p13 _WR
    - U4 p17 (DEVSEL) goes to MSX cartridge slot p9 _M1
    - U4 p16 (PHI0) goes to MSX cartridge slot p11 _IORQ
    - U4 p15 is just tied to a pullup

Not everything is implemented. This is just a Proof of Concept
- Graphic 7 is not implemented at all
- 9918 sprites render OK, but the V9938 extended sprite modes get the colours wrong
- Lots of graphical errors when you have 8 sprites per horizontal line in the extended sprite modes. Show's
up as flickering black horizontal lines and the lines below the sprites are pushed down. Basically the
ARM CPU cannot render the entire horizontal line fast enough.
- All the blitter like operations of the V9938 are very flakey. In some games they appear to mostly work,
with minor graphical errors. In terms of how the AppleII-VGA setup works, its amazing they appear to work at
all. This is probably very hard to fix.

Compilation is

git clone https://github.com/markadev/AppleII-VGA.git ~/AppleII-VGA
cd ~/AppleII-VGA/pico
mkdir build
cd build
cmake -DCOMPUTER_MODEL=V9938 -DCMAKE_BUILD_TYPE=Release ..
make
$ ls *uf2
...
applevga.uf2


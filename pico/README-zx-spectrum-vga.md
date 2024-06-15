What is this?
=============

- Take the Apple II VGA project code
- Adapt it to generate VGA output from a ZX Spectrum
- Essentially the same circuit diagram as the Apple II VGA version
  with the Z80 data bus connecting to the U3 74LVC245, the
  Z80 lower address bus A0 to A7 connecting to the U2 74LVC245,
  and the Z80 upper address bus A8 to A15 connecting to the U1
  74LVC245.

  The control pins are different though:

  - wire Z80 _WR to the U4 74LVC245 pin 16 which goes to 
    GPIO pin 26 where the PHI0 would have gone. That means we pick up every write cycle
  - wire Z80 _MREQ to the U4 74LVC245 pin 17 (which was ~DEVSEL) which goes to GPIO pin 8
  - wire Z80 _IORQ to the U4 74LVC245 pin 18 (which was ~RW) which goes to GPIO pin 9


- Note the different way to build it

git clone https://github.com/markadev/AppleII-VGA.git ~/AppleII-VGA
cd ~/AppleII-VGA/pico
mkdir build
cd build
cmake -DCOMPUTER_MODEL=ZXSPECTRUM -DCMAKE_BUILD_TYPE=Release ..
make
$ ls *uf2
...
applevga.uf2


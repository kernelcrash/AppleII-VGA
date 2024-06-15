Use AppleII-VGA as a base to emulate the 6847/6883 combo in the Coco and Dragon computers
=========================================================================================

- Wiring is almost identical to the Apple II implementation.
  - A15 to A8 on the coco/dragon cartridge slot go to the U1 74LVC245
  - A7 to A0 on the coco/dragon cartridge slot go to the U2 74LVC245
  - D7 to D0 on the coco/dragon cartridge slot go to the U3 74LVC245
  - On the U4 74LVC245
    - U4 p18 (RW) goes to coco/dragon cartridge slot p18 R!W
    - U4 p17 is not connected
    - U4 p16 (PHI0) goes to coco/dragon cartridge slot p6 E
    - U4 p15 is just tied to a pullup

Not all modes are implemented. Artefact mode is a 'hack' and only has one color set hardcoded.

Games that are tightly sync'd to the beam are likely to have a lot of screen weirdness 
(Dragonfire is an example).

Compilation is

```
git clone https://github.com/markadev/AppleII-VGA.git ~/AppleII-VGA
cd ~/AppleII-VGA/pico
mkdir build
cd build
cmake -DCOMPUTER_MODEL=COCO -DCMAKE_BUILD_TYPE=Release ..
make
$ ls *uf2
...
applevga.uf2
```


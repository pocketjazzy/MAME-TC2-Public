// license:BSD-3-Clause
// copyright-holders:R. Belmont, Ryan Holtz, Phil Stroffolino, Olivier Galibert
/*
    Namco System 22.5 and (Super) System 23 (Evolution 2)
    Preliminary driver by R. Belmont and Ryan Holtz, thanks to Phil Stroffolino & Olivier Galibert

    Hardware: * R4650 (MIPS III with IDT special instructions) main CPU.
                133 MHz for Gorgon, 166 MHz for System 23 and Super System 23, and
                200 MHz for Super System 23 Evolution 2.
              * H8/3002 MCU for sound/inputs
              * Custom polygon hardware
              * 1 text tilemap

    All games use a JVS I/O board connected to the H8/3002's serial port #0 and requires an I/O board to
    get past the subcpu check. It's similar to System 22 where one 37702 reads the I/O and communicates
    serially with the second 37702 which is the traditional "subcpu". Several I/O boards are accepted
    including TSS-I/O, FCA, ASCA3, ASCA5 and the common JVS I/O boards manufactured by Sega.

    NOTES:
    - First 128k of main program ROM is the BIOS, and after that is a 64-bit MIPS ELF image.
    - Text layer is identical to System 22 & Super System 22.

    TODO:
    - There are currently no differences seen between System 23 (Time Crisis 2) and
      Super System 23 (500GP, Final Furlong 2). These will presumably appear when
      the 3D hardware is emulated.

    - Serial number data is at offset 0x201 in the BIOS.  Until the games are running
      and displaying it I'm not going to meddle with it though.  Some newer Namco SS22 games
      have similar data there.

    - Improve GMEN hookups/comms.

    - Super System 23 tests irqs in the post.  timecrs2v4a's code can
      potentially test 7 sources, but only actually test 5.  With each
      source there is code to clear the interrupt and code to raise it.
      Levels 0 and 1 are not connected to anything according to the code.

      VBlank (level 2):
        clear: ad00000a.h = 0
        raise: just wait for it

      C361   (level 3):
        clear: a6820008.h = 1ff
               a100005c.w = 0
               a100005c.w = 1
               a4c3ff04.w = 0
        raise: a6820008.h = c8
               a100005c.w = 1

      Subcpu (level 3, same as C361):
        clear: same as C361
        raise: a4405002.h = 3170

      C435   (level 4):
        clear: a200000e.h = 1
               a200000e.h = 0
        raise: a2000000.h = 4f02 (c435 pio, state_set)
               a2000000.h = 1    (          interrupt)
               a2000000.h = 1    (          raise)

      C422   (level 5):
        clear: a6400002.h = f
               ad000008.h = 0
        raise: a640000e.h = 0
               a6400006.h = 1
               a640000a.h = 1
               a6400006.h = fffb
               a6400006.h = 0

      RS232  (level 6, not tested by timecrs2v4a):
        clear: nothing
        raise: nothing

      Timer  (level 7, not tested by timecrs2v4a):
        clear: c0.Compare = 10d880
        raise: c0.Count   = 10c8e0
               c0.Compare = 10d880

   Downhill bikers irq ack on level 3:
        check ad000000 & 0400
          if not, a4c3ff04 = 0
        check ad000000 & 0800
          if not, read a682000a, wait until it stops changing (?)
        return


c8000000:
  8011e384:
   if ((a2000000.w & 0xfff0 != 0x0080) (c417_r, 808e or 008e)
     +10.w = 2
     +16.w = 42
     +16.w = 23c0
     +10.w = 3
     801deaf0.w *0x28 -> +12.w (fixed)




':maincpu' (801142FC): unmapped program memory write to 0C800010 = 00020000 & FFFF0000
':maincpu' (801143A8): unmapped program memory write to 0C800010 = 00020000 & FFFF0000
':maincpu' (801143B4): unmapped program memory write to 0C800014 = 00000042 & 0000FFFF
':maincpu' (801143C0): unmapped program memory write to 0C800014 = 000023C0 & 0000FFFF
':maincpu' (801143CC): unmapped program memory write to 0C800010 = 00030000 & FFFF0000
':maincpu' (801143E0): unmapped program memory write to 0C800010 = 00000000 & 0000FFFF
':maincpu' (801143E0): unmapped program memory write to 0C800010 = 00000000 & 0000FFFF
':maincpu' (801143E0): unmapped program memory write to 0C800010 = 00000000 & 0000FFFF
':maincpu' (801143E0): unmapped program memory write to 0C800010 = 00000000 & 0000FFFF

    Game status:
        all games:          Glitchy graphics.
        downhill/u          Extremely glitchy graphics.
        panicprk/j/j2       Some mini-games are not playable due to Z-sorting issues.
        gunwars/a           Extremely glitchy graphics.
        raceon              Locks up after POST.
        aking               Inputs unresponsive.
        500gp               Occasional lockups.
        crszone(all)        Input issues.

****************************************************************************

Namco System 23 and Super System 23 Hardware Overview (last updated 7th April 2013 at 12.49am)
Namco, 1997 - 2000

Note! This document is a Work-In-Progress and will be updated from time to time as more information becomes available.

This document covers all the known Namco Gorgon / System 23 / Super System 23 games, including....
Final Furlong     Namco, 1997    System 22.5/Gorgon
Rapid River       Namco, 1997    System 22.5/Gorgon
Motocross Go!     Namco, 1997    System 23
Time Crisis II    Namco, 1997    System 23 and Super System 23
Downhill Bikers   Namco, 1997    System 23
Panic Park        Namco, 1998    System 23
Angler King       Namco, 1998    Super System 23
Gunmen Wars       Namco, 1998    Super System 23
Race On!          Namco, 1998    Super System 23
500 GP            Namco, 1998    Super System 23
Final Furlong 2   Namco, 1998    Super System 23
*Guitar Jam       Namco, 1999    Super System 23
Crisis Zone       Namco, 1999    System 23 Evolution 2

* - Guitar Jam is not dumped yet and the hardware type is not confirmed.
    According to Bandai Namco's website it is indeed SS23, and includes an extra sound board with Zoom Corp. DSP.

A System 23 unit is comprised of some of the following pieces....
- SYSTEM23 POWER(A) PCB            Small PCB bolted to the metal box only consisting of power in and network in/out. Only Motocross Go! used this as its
                                   video and sound connectors are mounted on the main board.
- V185B EMI PCB                    Small PCB bolted to the metal box with several connectors including power in, video out, network in/out, sound out
                                   (to AMP PCB) used with most of the S23/SS23 games that use a single main PCB and no other control PCBs.
- V198 EMI PCB                     Small PCB bolted to the metal box with several connectors (power/video/sound etc) plus a couple of extra
                                   ones for the CCD camera and feedback board. It is connected to the main and GMEN boards on the other side via
                                   2 multi-pin connectors (used only with games that use the GMEN board)
- BASS AMP PCB                     Power AMP PCB for general sounds and bass
- SYSTEM23 MAIN PCB                Main PCB for System 23               \
  or SystemSuper23 MAIN(1) PCB     Main PCB for Super System 23         / Note the 3 main boards are similar, but not exactly the same.
  or System23Evolution2 PCB        Main PCB for System 23 Evolution 2  /
- MSPM(FR*) PCB                    A small plug-in daughterboard that holds FLASHROMs containing Main CPU and Sound CPU programs
- FCA PCB                          Controls & I/O interface board used with a few Super System 23 games only. Contains mostly transistors,
                                   caps, resistors, several connectors, an MCU and a PIC16F84.
                                   The PIC is different for EACH game and the FCA PCBs are not interchangeable between different games.
                                   If the FCA PCB is not connected, the game will not advance past the 3rd screen shown below.
- ASCA-3A PCB / ASCA-4A PCB        This is the standard I/O board used with most of the S23/SS23 games with support for digital and
                                   analog controls (buttons/joysticks/pots etc).
- V183 AMC PCB                     I/O board only in Motocross Go that controls handlebar and seat force feedback. It's connected as a slave
                                   I/O board. Half of the board recycles a v145 motor board used in Rave Racer, Ace Driver and Dirt Dash.
- V185 I/O PCB                     Gun I/O board used with Time Crisis II
- V221 MIU PCB                     Gun I/O board used with Crisis Zone (System 23 Evolution 2) and Time Crisis 3 (on System 246)
- SYSTEM23 MEM(M) PCB              Holds mask ROMs for GFX/Sound and associated logic
                                   Note that in Super System23, the MEM(M) PCB is re-used from System23.
                                   On Super System23, there is a sticker over the System23 part labelled 'SystemSuper23' and one
                                   PAL is not populated.
- GMEN PCB                         A large board that sits on top of the main board containing a SH2 CPU, some RAM and a few CPLDs/FPGAs.
                                   This controls the video overlay from the CCD camera in Gunmen Wars, Race On! and Final Furlong 2.
                                   The ROM board plugs in on top of this board.
- V194 STR PCB                     Used with Race On! to control the steering feed-back. An identical re-labelled PCB (V257) with
                                   different SOP44 ROMs is used with Wangan Midnight (Chihiro) and Ridge Racer V (System 246)

The metal box housing these PCB's is approximately the same size as Super System 22. However, the box is mostly
empty. All of the CPU/Video/DSP hardware is located on the main PCB which is the same size as the
Super System 22 CPU board. The ROM PCB is half the size of the Super System22 ROM PCB. The ROM positions on it
can be configured for either 32MBit or 64MBit SOP44 mask ROMs with a maximum capacity of 1664MBits.
The system also uses a dual pipeline graphics bus similar to Super System 22 and has two copies of the graphics ROMs
on the PCB.
The System 23 hardware is the first NAMCO system to require an external 3.3V power source. Previously the 3.3V
was derived from a 5V to 3.3V regulator on systems such as System10/11/12 etc.
The KEYCUS chip is the familiar MACH211 PLCC44 IC as used on System12. The sound system is also taken from System12.

On bootup, the following happens (on 500GP)...

1st screen - Grey screen with white text
                               "SYSTEM 23 BOOTING     "
                               "SDRAM CHECKING A0xx000" (xx = slowly counts up to 3F, from 00), then OK ('CHECKING' is in yellow text, 'OK' is in green text)
   As the SDRAM is being checked, the LEDS 1 to 8 turn off in sequence from 8 to 1.

2nd screen - Grey screen with white text
                               "S.S.23 POWER ON TEST      xxxx"  (xxxx = numbers count up rapidly from 0000)
                               "(C) NAMCO                     "
                               "                     VER. 1.16"
   As these checks happen, the LEDs 1 to 8 flash on/off

3rd screen - Grey screen with white text
                               "S.S.23 POWER ON TEST      xxxx"  (xxxx = numbers count up rapidly from 0000)
                               "SUBCPU INITIALIZING ....      "
                               "SUBCPU PROGRAM Ver. 0211      "

   and a PACMAN eating dots along the bottom of the screen from left to right.
   As these checks happen, the LEDs 1 to 8 simultaneously flash on/off in various patterns.
   The Sub CPU will initialize before the Pacman reaches half-way across the screen.

When the SUB-CPU connects there are numerous POST screens that test almost all of the main components.

On System23, the bootup sequence is shorter. The screen remains blank while the SDRAM is being checked (i.e. 1st screen mentioned above is not shown).
LEDS 1-8 turn off in sequence 8-1. The bank of 8 LEDs on the main board cycles from left to right to left repeatedly (almost all S23/SS23 games seem
to do this in fact). After that, the bootup sequence is mostly the same as SS23.
When the game is running some games just cycle the 8 LEDs left/right/left etc. Others cycle the LEDs in pairs just one LED position
left/right/left etc. Those crazy Namco guys *really* like LEDs.

To skip the (long) POST, set DIP switch #2 of the 8-position DIP switch block to ON. As soon as the SUB-CPU connects the game will boot.
However this only works if the program supports it. Most games just ignore the DIPs.


PCB Layouts
-----------
Rev 1
SYSTEM23 MAIN PCB 8660961103 (8660971103) This rev has J7/J8/J9/J10/J11 populated
Rev 2
SYSTEM23 MAIN PCB 8660961105 (8660971105)
|----------------------------------------------------------------------------|
|       J5       J7  J8       3V_BATT       J9   J10                J11      |
|                     LED1-8         *R4543     *MAX734          ADM485JR    |
|  |-------| LED10-11         LC35256                 CXA1779P  *3414 *3414  |
|  |H8/3002|          *2061ASC-1                                             |
|  |       |       SW4                                *LM358                 |
|  |-------|                    DS8921                *MB88347  *MB87078     |
|J18                                                  *LC78832  *LC78832     |
|              SW3  14.7456MHz                  |----|   |----|  CXD1178Q    |
|             |------| |----| |----||---------| |C435|   |C435|              |
|    N341256  | C416 | |C422| |IDT ||         | |----|   |----|              |
|             |      | |----| |7200||   C403  |                              |
|    N341256  |      |        |----||         |                 |---------|  |
|             |------|        |----||         | PAL(2)  N341256 |         |  |
|   |----| *PST575            |IDT ||---------|                 |  C417   |  |
|   |C352|           CY7C182  |7200|                            |         |  |
|   |----| LED9               |----|                    N341256 |         |  |
|                            J12                                |---------|  |
|     KM416S1020            |-------|   PAL(3)  M5M4V4265                    |
|                           |XILINX |                                        |
|J16                        |XC95108|                                     J17|
|     KM416S1020            |-------|       |---------|  |---------| N341256 |
|                                           |         |  |         |         |
|                                           |   C421  |  |   C404  | N341256 |
|       |---------|              N341256    |         |  |         |         |
|       |         |                         |         |  |         | N341256 |
|       |   C413  |              N341256    |---------|  |---------|         |
|       |         |                                                          |
|       |         |                           M5M4V4265                      |
|       |---------| SW2    LC321664                                          |
|               *PST575                                                      |
|                                                             *KM681000      |
|       |----------|    |---------|                        |-------------|   |
|       |NKK       |    |         |                        |             |   |
|       |NR4650-13B|    |   C361  |           CY2291       |    C412     |   |
|J14    |          |    |         |                        |             |J15|
|       |          |    |         |           14.31818MHz  |             |   |
|       |----------|    |---------|  PAL(1)                |-------------|   |
|                                                             *KM681000      |
|                                                       HM5216165  HM5216165 |
|----------------------------------------------------------------------------|
Notes:
      * - These parts are underneath the PCB.

      Main Parts List:

      CPU
      ---
          NKK NR4650 - R4600-based 64bit RISC CPU (Main CPU, QFP208, clock input source = CY2291)
          H8/3002    - Hitachi H8/3002 HD6413002F17 (Sound CPU, QFP100, running at 14.7456MHz)

      RAM
      ---
          N341256    - NKK 32k x8 SRAM (x9, SOJ28)
          LC35256    - Sanyo 32k x8 SRAM (SOP28)
          KM416S1020 - Samsung 16MBit SDRAM (x2, TSSOP50)
          M5M4V4265  - Mitsubishi 256k x16 DRAM (x2, TSOP40/44)
          LC321664   - Sanyo 64k x16 EDO DRAM (SOJ40)
          HM5216165  - Hitachi 16MBit SDRAM (x2, TSSOP50)
          KM681000   - Samsung 128k x8 SRAM (x2, SOP32)
          CY7C182    - Cypress 8k x9 SRAM (SOJ28)

      Namco Customs
      -------------
                    C352 (QFP100)
                    C361 (QFP120)
                    C403 (QFP136)
                    C404 (QFP208)
                    C412 (QFP256)
                    C413 (QFP208)
                    C416 (QFP176)
                    C417 (QFP208)
                    C421 (QFP208)
                    C422 (QFP64)
                    C435 (x2, QFP144)

      Other ICs
      ---------
               XC95108  - Xilinx XC95108 In-System Programmable CPLD (QFP100)
                            - labelled 'S23MA9' on Rev 1
                            - labelled 'S23MA9B' on Rev 2
               DS8921   - National RS422/423 Differential Line Driver and Receiver Pair (SOIC8)
               CXD1178Q - SONY CXD1178Q  8-bit RGB 3-channel D/A converter (QFP48)
               PAL(1)   - PALCE16V8H (PLCC20, stamped 'PAD23')
               PAL(2)   - PALCE22V10H (PLCC28, stamped 'S23MA5')
               PAL(3)   - PALCE22V10H (PLCC28, stamped 'SS23MA6B')
               MAX734   - MAX734 +12V 120mA Flash Memory Programming Supply Switching Regulator (SOIC8)
               PST575   - PST575 System Reset IC (SOIC4)
               3414     - NJM3414 70mA Dual Op Amp (x2, SOIC8)
               LM358    - National LM358 Low Power Dual Operational Amplifier (SOIC8)
               MB87078  - Fujitsu MB87078 Electronic Volume Control IC (SOIC24)
               MB88347  - Fujitsu MB88347 8bit 8 channel D/A converter with OP AMP output buffers (SOIC16)
               ADM485   - Analog Devices Low Power EIA RS485 transceiver (SOIC8)
               CXA1779P - SONY CXA1779P TV/Video circuit RGB Pre-Driver (DIP28)
               CY2291   - Cypress CY2291 Three-PLL General Purpose EPROM Programmable Clock Generator (SOIC20)
               2061ASC-1- IC Designs 2061ASC-1 clock Generator IC (SOIC16, also found on Namco System 11 PCBs)
               R4543    - EPSON Real Time Clock Module (SOIC14)
               IDT7200  - Integrated Devices Technology IDT7200 256 x9 CMOS Asynchronous FIFO

      Misc
      ----
          J5    - Connector for EMI PCB
          J7/J8 - 15-pin VGA output connectors                        -\
          J9/J10- Red/White Stereo Audio Output RCA connectors          \
          J11   - Standard USB connector (used on Motocross Go!)       -/  Not populated on most PCBs
          J12   - 6-pin connector for In-System Programming of the XC95108 IC
          J14 \
          J15 |
          J16 |
          J17 \ - Connectors for MEM(M) PCB
          J18   - Connector for MSPM(FRA) PCB
          SW2   - 2 position DIP Switch
          SW3   - 2 position DIP Switch
          SW4   - 8 position DIP Switch


SystemSuper23 MAIN(1) PCB 8672960904 8672960104 (8672970104)
|----------------------------------------------------------------------------|
|       J5       J7  J8       3V_BATT       J9   J10                J11      |
|                     LED1-8         *R4543     *MAX734          ADM485JR    |
|  |-------| LED10-11         LC35256                 CXA1779P   3414  3414  |
|  |H8/3002|          *2061ASC-1                                             |
|  |       |       SW4                                *LM358                 |
|  |-------|                    DS8921                *MB88347  *MB87078     |
|J18                                                  *LC78832  *LC78832     |
|               SW3  14.7456MHz                |----|    |----|  CXD1178Q    |
|              |------|  |----|   |---------|  |C435|    |C435|              |
|    N341256   | C416 |  |C422|   |         |  |----|    |----|              |
|              |      |  |----|   |   C444  |                                |
|    N341256   |      |           |         |                   |---------|  |
|              |------|           |         |  PAL(2) CY7C1399  |         |  |
|   |----| *PST575                |---------|                   |  C417   |  |
|   |C352|           CY7C182                                    |         |  |
|   |----| LED9                                       CY7C1399  |         |  |
|                                                               |---------|  |
|     KM416S1020           EPM7064      PAL(3)  KM416V2540                   |
|                                                                            |
|J16                                                                      J17|
|     KM416S1020                            |---------|  |---------| N341256 |
|                                CY7C1399   |         |  |         |         |
|                                           |   C421  |  |   C404  | N341256 |
|       |---------|              CY7C1399   |         |  |         |         |
|       |         |                         |         |  |         | N341256 |
|       |   C413  |                         |---------|  |---------|         |
|       |         |                                                          |
|       |         |                             KM416V2540                   |
|       |---------| SW2    LC321664                                          |
|               *PST575                                                      |
|                                                           *KM416S1020      |
|       |----------|    |---------|                        |-------------|   |
|       |NKK       |    |         |                        |             |   |
|       |NR4650-167|    |   C361  |           CY2291       |    C447     |   |
|J14    |          |    |         |                        |             |J15|
|       |          |    |         |           14.31818MHz  |             |   |
|       |----------|    |---------|  PAL(1)                |-------------|   |
|                                                           *KM416S1020      |
|                                                          71V124   71V124   |
|----------------------------------------------------------------------------|
Notes:
      * - These parts are underneath the PCB.

      Main Parts List:

      CPU
      ---
          NKK NR4650 - R4600-based 64bit RISC CPU (Main CPU, QFP208, clock input source = CY2291)
          H8/3002    - Hitachi H8/3002 HD6413002F17 (Sound CPU, QFP100, running at 14.7456MHz)

      RAM
      ---
          N341256    - NKK 32k x8 SRAM (x5, SOJ28)
          LC35256    - Sanyo 32k x8 SRAM (SOP28)
          KM416S1020 - Samsung 16MBit SDRAM (x4, TSSOP50)
          KM416V2540 - Samsung 256k x16 EDO DRAM (x2, TSOP40/44)
          LC321664   - Sanyo 64k x16 EDO DRAM (SOJ40)
          71V124     - IDT 128k x8 SRAM (x2, SOJ32)
          CY7C1399   - Cypress 32k x8 SRAM (x4, SOJ28)
          CY7C182    - Cypress 8k x9 SRAM (SOJ28)
          M5M4V4265  - Mitsubishi 256k x16 DRAM (x2, TSOP40/44)

      Namco Customs
      -------------
                    C352 (QFP100)
                    C361 (QFP120)
                    C404 (QFP208)
                    C413 (QFP208)
                    C416 (QFP176)
                    C417 (QFP208)
                    C421 (QFP208)
                    C422 (QFP64)
                    C435 (x2, QFP144)
                    C444 (QFP136) This replaces C403 from System 23 but likely integrates the 2x IDT7200 chips used on System 23 inside this custom chip.
                    C447 (QFP256)

      Other ICs
      ---------
               EPM7064  - Altera MAX EPM7064STC100-10 CPLD (TQFP100, labelled 'SS23MA4A')
               DS8921   - National RS422/423 Differential Line Driver and Receiver Pair (SOIC8)
               CXD1178Q - SONY CXD1178Q  8-bit RGB 3-channel D/A converter (QFP48)
               PAL(1)   - PALCE16V8H (PLCC20, stamped 'SS23MA1B')
               PAL(2)   - PALCE22V10H (PLCC28, stamped 'SS23MA2A')
               PAL(3)   - PALCE22V10H (PLCC28, stamped 'SS23MA3A')
               MAX734   - MAX734 +12V 120mA Flash Memory Programming Supply Switching Regulator (SOIC8)
               PST575   - PST575 System Reset IC (SOIC4)
               3414     - NJM3414 70mA Dual Op Amp (x2, SOIC8)
               LM358    - National LM358 Low Power Dual Operational Amplifier (SOIC8)
               MB87078  - Fujitsu MB87078 Electronic Volume Control IC (SOIC24)
               MB88347  - Fujitsu MB88347 8bit 8 channel D/A converter with OP AMP output buffers (SOIC16)
               ADM485   - Analog Devices Low Power EIA RS485 transceiver (SOIC8)
               CXA1779P - SONY CXA1779P TV/Video circuit RGB Pre-Driver (DIP28)
               CY2291   - Cypress CY2291 Three-PLL General Purpose EPROM Programmable Clock Generator (SOIC20)
               2061ASC-1- IC Designs 2061ASC-1 clock generator IC (SOIC16, also found on Namco System 11 PCBs)
               R4543    - EPSON Real Time Clock Module (SOIC14)

      Misc
      ----
          J5    - Connector for EMI PCB
          J7/J8 - 15-pin VGA output connectors                 -\
          J9/J10- Red/White Stereo Audio Output RCA connectors   \
          J11   - Standard USB connector                        -/  Not populated on most PCBs
          J14 \
          J15 |
          J16 |
          J17 \ - Connectors for MEM(M) PCB
          J18   - Connector for MSPM(FRA) PCB
          SW2   - 2 position DIP Switch
          SW3   - 2 position DIP Switch
          SW4   - 8 position DIP Switch


System23Evolution2 MAIN PCB 8902960103 (8902970103)
|----------------------------------------------------------------------------|
|       J5          J7                           3V_BATT                     |
|                     LED1-8     ADM485               *R4543                 |
|  |-------| LED10-11                                  LC35256   CXD1178Q    |
|  |H8/3002|          *2061ASC-1                                             |
|  |       |       SW4                  UDA1320 UDA1320                      |
|  |-------|                    DS8921                                       |
|J18                                                                         |
|               SW3  14.7456MHz                |----|    |----|              |
|              |------|  |----|   |---------|  |C435|    |C435|              |
|    N341256   | C452 |  |C422|   |         |  |----|    |----|              |
|              |      |  |----|   |   C444  |                                |
|    N341256   |      |           |         | XILINX    XILINX  |---------|  |
|              |------|           |         | XC9572XL  XC9536XL|         |  |
|   |----| *PST575                |---------|                   |  C451   |  |
|   |C352|           CY7C182                                    |         |  |
|   |----| LED9                                                 |         |  |
|                                                               |---------|  |
|     KM432S2030                                KM416V2540                   |
|                                                                            |
|J16                                                                      J17|
|     KM432S2030                            |---------|  |---------| N341256 |
|                                IS61LV256  |         |  |         |         |
|                                           |   C421  |  |   C404  | N341256 |
|       |---------|              IS61LV256  |         |  |         |         |
|       |         |                         |         |  |         | N341256 |
|       |   C450  |                         |---------|  |---------|         |
|       |         |                                                          |
|       |         |                             KM416V2540                   |
|       |---------| SW2    LC321664                                          |
|               *PST575                                                      |
|                                                           *KM416S1120      |
|       |----------|    |---------|                        |-------------|   |
|       |IDT       |    |         |                        |             |   |
|       |NR4650-200|    |   C361  |           CY2291       |    C447     |   |
|J14    |          |    |         |                        |             |J15|
|       |          |    |         |           14.31818MHz  |             |   |
|       |----------|    |---------|  PAL                   |-------------|   |
|                                                           *KM416S1120      |
|                                                          71V124   71V124   |
|----------------------------------------------------------------------------|
Notes:
      * - These parts are underneath the PCB.

      Main Parts List:

      CPU
      ---
          NKK NR4650 - R4600-based 64bit RISC CPU (Main CPU, QFP208, clock input source = CY2291)
          H8/3002    - Hitachi H8/3002 HD6413002F17 (Sound CPU, QFP100, running at 14.7456MHz)

      RAM
      ---
          N341256    - NKK 32k x8 SRAM (x5, SOJ28)
          LC35256    - Sanyo 32k x8 SRAM (SOP28)
          KM432S2030 - Samsung 32MBit SDRAM (x2, TSSOP86)
          KM416S1120 - Samsung 16MBit SDRAM (x2, TSSOP50)
          KM416V2540 - Samsung 256k x16 EDO DRAM (x2, TSOP40/44)
          LC321664   - Sanyo 64k x16 EDO DRAM (SOJ40)
          71V124     - IDT 128k x8 SRAM (x2, SOJ32)
          ISS61LV256 - ISSI 32k x8 SRAM (x2, SOJ28)
          CY7C182    - Cypress 8k x9 SRAM (SOJ28)
          M5M4V4265  - Mitsubishi 256k x16 DRAM (x2, TSOP40/44)

      Namco Customs
      -------------
                    C352 (QFP100)
                    C361 (QFP120)
                    C404 (QFP208)
                    C421 (QFP208)
                    C422 (QFP64)
                    C435 (x2, QFP144)
                    C444 (QFP136) This replaces C403 from System 23 but likely integrates the 2x IDT7200 chips used on System 23 inside this custom chip.
                    C447 (QFP256)
                    C450 (BGAxxx)
                    C451 (QFP208)
                    C452 (QFP176)

      Other ICs
      ---------
               XC9536   - XILINX XC9536XL CPLD (TQFP64, labelled 'S23EV2.1')
               XC9572   - XILINX XC9536XL CPLD (TQFP100, labelled 'S23EV2.2')
               DS8921   - National RS422/423 Differential Line Driver and Receiver Pair (SOIC8)
               CXD1178Q - SONY CXD1178Q  8-bit RGB 3-channel D/A converter (QFP48)
               PAL      - PALCE16V8H (PLCC20, stamped 'PAD23')
               PST575   - PST575 System Reset IC (SOIC4)
               ADM485   - Analog Devices Low Power EIA RS485 transceiver (SOIC8)
               CY2291   - Cypress CY2291 Three-PLL General Purpose EPROM Programmable Clock Generator (SOIC20)
               2061ASC-1- IC Designs 2061ASC-1 clock generator IC (SOIC16, also found on Namco System 11 PCBs)
               R4543    - EPSON Real Time Clock Module (SOIC14)
               UDA1320  - Philips UDA1320 Low-cost stereo filter DAC (SOIC16)

      Misc
      ----
          J5    - Connector for EMI PCB
          J7    - 15-pin VGA output connector
          J14 \
          J15 |
          J16 |
          J17 \ - Connectors for MEM(M) PCB
          J18   - Connector for MSPM(FRC) PCB
          SW2   - 2 position DIP Switch
          SW3   - 2 position DIP Switch
          SW4   - 8 position DIP Switch


SystemSuper23 Gmen PCB 8672960502 (8672970502)
Sticker: Gmen (GTR) PCB
                         |---------------------------------------------------|
                         |        J8              LED4    J6        J5       |
                         |          20.25MHz          82AF          LT1635   |
                         |     VXP3220A            611_803        24.576MHz  |
                         |                                      |-----|      |
                         |                                      |FUJI |      |
                         |                           |-----|    |MD8402A     |
                         |                           |FUJI |    |-----|      |
                         |                           |MD8412A                |
                         |                           |-----|                 |
                         |                                                   |
                         |                                                   |
                         |                                                   |
                         |                       TSOP48                      |
|------------------------|                                                   |
|  N341256 N341256                                             N341024       |
|                     HM5118165 HM5118165                                    |
|                                                                            |
|     |---------|                  |---(1)--|                                |
|J10  |         |                  |ALTERA  |                  N341024    J17|
|     |  C446   |      |--------|  |MAX     |                                |
|     |         |      |ALTERA  |  |EPM7064 |                                |
|     |         |      |FLEX    |  |--------|                                |
|     |---------|      |EPM8282 | LED3                     |----------|      |
|                      |--------|                          |ALTERA    |      |
|                                                          |FLEX      |      |
|                                                          |EPM8452   |      |
|                                                          |          |      |
|                         |-------|                        |----------|      |
|                         | SH2   |                           LED2           |
|                         |       |                                          |
|              28.7MHz    |-------|                                          |
|J11                3771         KM416S1020                    N341024    J16|
|    HD63B50                                                                 |
|              40MHz                                                         |
|                                KM416S1020          SW3                     |
| |--(2)--|                                                    N341024       |
| |ALTERA |                                                                  |
| |EPM7064|                                                                  |
| |-------|    LT1180                               LED1    LED5-12          |
|----------------------------------------------------------------------------|
Notes:
      This board controls the video overlay from the CCD camera in Gunmen Wars, Race ON! and Final Furlong 2.
      The main board does it's usual POST then the board fires up, LED1 lights red and the LEDs 5-12 go crazy pulsing left to right from
      the middle outwards. The main board uploads a loader program, then the Main SH2 program. After completion LED 3 lights green and then
      LED 2 lights orange for a second then extinguishes then the main board resets :-/
      It doesn't boot up for unknown reasons but likely another PCB is missing from somewhere in the cabinet or the GMEN PCB is faulty.

      SH2        - Hitachi HD6417604 SH2 CPU (QFP144, clock input 28.7MHz)
      N341024    - NKK 128k x8 SRAM (x4, SOP32)
      N341256    - NKK 32k x8 SRAM (x2, SOP28)
      KM416S1020 - Samsung 16MBit SDRAM (x2, TSSOP50). Also compatible with MB811181622
      HM5118165  - Hitachi 16MBit SDRAM (x2, TSSOP44/50)
      C446       - Namco Custom IC (QFP160)
      TSOP48     - Not-populated position for a 29F400 512k x8 TSOP48 flash ROM
      3771       - Fujitsu MB3771 System Reset IC (SOIC8)
      MD8402     - Fuji MD8402A IEEE 1394 'Firewire' Physical Channel Interface IC (TQFP100)
      MD8412     - Fuji MD8412A IEEE 1394 'Firewire' Link Layer Controller IC (TQFP100)
      VPX3220A   - Micronas Intermetall VPX3220A Video Pixel Decoder IC (PLCC44)
      HD63B50    - Hitachi HD63B50 Asynchronous Communications Interface Adapter IC (SOP24)
      LT1180     - Linear Technology LT1180A Low Power 5V RS232 Dual Driver/Receiver Pair with 0.1mF Capacitors
      EPM7064(1) - Altera MAX EPM7064 CPLD labelled 'SS23GM1A' (TQFP100)
      EPM7064(2) - Altera EPM7064 CPLD labelled 'SS23GM2A' (TQFP44)
      EPM8452    - Altera FLEX EPM8452 CPLD (no label, QFP160)
      EPM8282    - Altera FLEX EPM8282 CPLD (no label, TQFP100)
      82AF       - National 82AF General Purpose EMI Reduction IC (SOIC8)
      LT1635     - Linear Technology LT1635 Micropower Rail-to-Rail Op Amp and Reference Buffer (SOIC8)
      611_803    - ? (SOIC8)
      J5/J6      - IEEE 1394 'Firewire' connectors
      J8         - Connector for EMI PCB
      J10        - Labelled 'WAVE'       \
      J11        - Labelled 'SYS23BUS'   | Connectors for MEM(M) PCB
      J16        - Labelled 'TEXTURE'    | i.e. The ROM board sits on top of this board
      J17        - Labelled 'POINT'      /
      SW3        - 8 position DIP Switch


Namco NamCam
------------

/------------------------\
|   NAMCO          /---\ |
|                  | O | |
|  NAMCAM          \---/ |
\------------------------/

This is a CCD camera used on Gunmen Wars and Race-On and is
mounted into the upper bezel above the monitor in front of the
player. Both games run on Namco Super System 23 hardware.
The camera has a 5 pin connector. That connector is wired to
a 6 pin connector J6 on the V198 EMI PCB on the outside of the metal
box housing the Super System 23 game boards.
The pinout of J6 is...
1 - +5V
2 - GND
3 - VIDEO (color composite video)
4 - GND
5 - NO CONNECT
6 - SCREEN (just tied to GND on the PCB)
Connector J6 eventually connects to the GMEN board to the VPX-3220 IC.

When the game is started the game takes a snapshot of the
player's face which is added on top of the game graphics by
the GMEN PCB.
If the NamCam is not connected the game will hang near the end of
the SH2 initialisation/loading and will not go in-game. If the
camera works enough to get the game booted but doesn't work in-game
the board will hang on the 'taking the photo' screen then eventually
reboot. The camera PCB is made by Toshiba.
The PCB is mounted to the underside of the NamCam.

PCB Top Side
------------

TOSHIBA
PB7408
70177778A
TW10794V-0
767 13604 (sticker)
|--------------------------------|
|        |-------|19.069MHz      |
|        |TOSHIBA|       CCD_CAM |
|        |TC90A22AF              |
|P001    |-------|  93C45A       |
|--------------------------------|
Notes:
TC90A22AF - Toshiba TA90A22AF CCD Camera Digital Signal Processing IC (TQFP80)
   93C45A - 93C45A (TSSOP8) EEPROM, connected to TC90A22AF on pins 59-64.
            pin 58 is NTPA and is grounded so the camera is set to NTSC mode.
            The eeprom holds camera setting data, brightness, contrast etc.
  CCD_CAM - This is the actual CCD sensor and lens
     P001 - 5-pin connector that joins to the V198 EMI PCB


PCB bottom Side
---------------
                    Note: This area has many tiny diodes, inductors, transistors,
TOSHIBA             capacitors etc, most can't be identified to period-correct
PB7408              manufacturers due to undocumented SMD codes.
70177778A            /----/\----\
TW10794V-0          /            \
|--------------------------------|
|             5W01  1R33  GY     |
|        TA1283FN        3800    |
| D16510             00DM   LY   |
|              X2 LY        800mA|
|--------------------------------|
Notes:
  D16510 - NEC uPD16510 Vertical Driver for CCD Sensor (TSSOP20)
TA1283FN - ? Made by Toshiba (TSSOP24)
    3800 - ? possibly an op-amp made by New Japan Radio Co. (i.e. JRC3800?) (TSSOP8)
    1R33 - Sharp PQ1R33 Voltage Regulator (SOIC6)
      X2 - ROHM EMX2 Dual General-Purpose NPN Transistor
      LY - Probably a transistor (SOT23)
      GY - Probably a transistor (SOT89)
    00DM - ? (SOT523)
    5W01 - ?. Possibly ST Microelectronics ST25W01 128 x8-bit Serial EEPROM (connected to TA1283FN) (TSSOP8)


Program ROM PCB
---------------
Type 1:
MSPM(FRA) PCB 8699017500 (8699017400)
|--------------------------|
|            J1            |
|                          |
|  IC3               IC1   |
|                          |
|                          |
|                    IC2   |
|--------------------------|
Notes:
      J1 -  Connector to plug into Main PCB
      IC1 \
      IC2 / Main Program  (Fujitsu 29F016 16MBit FlashROM, TSOP48)
      IC3 - Sound Program (Fujitsu 29F400T 4MBit FlashROM, TSOP48)

      Games that use this PCB include...

      Game             Code and revision
      ----------------------------------
      Time Crisis 2    TSS2 Ver.B (for System 23)
      Time Crisis 2    TSS3 Ver.B (for System 23)
      Gunmen Wars      GM1  Ver.A (for Super System 23)
      Downhill Bikers  DH3  Ver.A (for System 23)
      Motocross Go!    MG3  Ver.A (for System 23)
      Panic Park       PNP2 Ver.A (for System 23)
      Race On!         RO2  Ver.A (for Super System 23)

Type 2:
MSPM(FRA) PCB 8699017501 (8699017401)
|--------------------------|
|            J1            |
|                          |
|  IC2               IC3   |
|                          |
|                          |
|  IC1                     |
|--------------------------|
Notes:
      J1 -  Connector to plug into Main PCB
      IC1 \
      IC2 / Main Program  (Fujitsu 29F016 16MBit FlashROM, TSOP48)
      IC3 - Sound Program (ST M29F400T 4MBit FlashROM, TSOP48)

      Games that use this PCB include...

      Game             Code and revision
      ----------------------------------
      Angler King      AG1  Ver.A (for Super System 23)
      500GP            5GP3 Ver.C (for Super System 23)
      Time Crisis 2    TSS4 Ver.A (for Super System 23)
      Final Furlong 2  FFS1 Ver.A (for Super System 23)
      Final Furlong 2  FFS2 Ver.? (for Super System 23)

Type 3:
MSPM(FRC) PCB 8699019800 (8699019700)
|--------------------------|
|            J1            |
|                          |
|  IC1         IC2   IC4   |
|                          |
|                          |
|                 IC3      |
|--------------------------|
Notes:
      J1 -  Connector to plug into Main PCB
      IC1 - Sound Program (Fujitsu 29F400T 4MBit FlashROM, TSOP48)
      IC3 - a *very tiny* transistor marked 'H5'
      IC2 \
      IC4 / Main Program  (Intel DA28F640J5 64MBit FlashROM, SSOP56)

      Games that use this PCB include...

      Game             Code and revision    Notes
      -----------------------------------------------------------------------
      Crisis Zone      CSZO4 Ver.B          Serialsed ROMs, IC2 not populated
      Crisis Zone      CSZO3 Ver.B          Serialsed ROMs, IC2 not populated
      Crisis Zone      CSZO2 Ver.A          Serialsed ROMs, IC2 not populated


ROM PCB
-------

Printed on the PCB        - 8660960601 (8660970601) SYSTEM23 MEM(M) PCB
Sticker (500GP)           - 8672961100
Sticker (Time Crisis 2)   - 8660962302
Sticker (Crisis Zone)     - 8672961100 .... same as 500GP
Sticker (Race On!)        - 8672961100 .... same as 500GP
Sticker (Angler King)     - 8672961100 .... same as 500GP

|----------------------------------------------------------------------------|
| KEYCUS    MTBH.2M      CGLL.4M        CGLL.5M         CCRL.7M       PAL(3) |
|                                                                            |
|                                                                            |
|J1         MTAH.2J      CGLM.4K        CGLM.5K         CCRH.7K            J4|
|                                                                            |
|   PAL(4)                                            JP5                    |
|                        CGUM.4J        CGUM.5J       JP4                    |
|           MTAL.2H                                   JP3                    |
|                                                     JP2                    |
|                        CGUU.4F        CGUU.5F                              |
|                                                       CCRL.7F              |
|           MTBL.2F                                                          |
|                            PAL(1)    PAL(2)                                |
|                                                       CCRH.7E              |
|                                                                            |
|         JP1                                                                |
|                                                                            |
|       WAVEL.2C      PT3L.3C      PT2L.4C      PT1L.5C      PT0L.7C         |
|J2                                                                        J3|
|                                                                            |
|       WAVEH.2A      PT3H.3A      PT2H.4A      PT1H.5A      PT0H.7A         |
|                                                                            |
|                                                                            |
|----------------------------------------------------------------------------|
Notes:
      J1   \
      J2   |
      J3   |
      J4   \   - Connectors to main PCB
      JP1      - ROM size configuration jumper for WAVE ROMs. Set to 64M, alt. setting 32M
      JP2      - ROM size configuration jumper for CG ROMs. Set to 64M, alt. setting 32M
      JP3      - ROM size configuration jumper for CG ROMs. Set to 64M, alt. setting 32M
      JP4      - ROM size configuration jumper for CG ROMs. Set to 64M, alt. setting 32M
      JP5      - ROM size configuration jumper for CG ROMs. Set to 64M, alt. setting 32M
                 Other ROMs
                           CCRL - size fixed at 32M
                           CCRH - size fixed at 16M
                           PT*  - size fixed at 32M
                           MT*  - size fixed at 64M

      KEYCUS   - Mach211 CPLD (PLCC44)
      PAL(1)   - PALCE20V8H  (PLCC28, stamped 'SS22M2')  \ Both identical
      PAL(2)   - PALCE20V8H  (PLCC28, stamped 'SS22M2')  /
      PAL(3)   - PALCE16V8H  (PLCC20, stamped 'SS22M1')
      PAL(4)   - PALCE16V8H  (PLCC20, stamped 'SS23MM1')
                 Note this PAL is not populated when used on Super System 23

      All ROMs are SOP44 MaskROMs
      Note: ROMs at locations 7M, 7K, 5M, 5K, 5J & 5F are copies of identical ROMs on the PCB at locations 7F, 7E, 4M, 4K, 4J, 4F
            Each ROM is stamped with the Namco Game Code, then the ROM-use code (such as CCRL, CCRH, PT* or MT*).

                            Game
            Game            Code     Keycus    Notes
            -----------------------------------------------------------------------
            500GP           5GP1     KC029     -
            Angler King     AG1      KC028     -
            Crisis Zone     CSZ1     KC039     -
            Downhill Bikers DH1      KC016     3A, 3C, 2M and 2F not populated.
            Final Furlong 2 FFS1     KC???     -
            Gunmen Wars     GM1      KC018     3A, 3C, 4A, 4C, 2A, 2F and 2M not populated.
            Motocross Go!   MG1      KC009     3A, 3C, 4A, 4C, 4F and 7F not populated.
            Panic Park      PNP1     KC015     3A, 3C, 4A, 4C, 2M and 2F not populated.
            Race On!        RO1      KC017     2M and 2F not populated.
            Time Crisis 2   TSS1     KC010     3A and 3C not populated.

I/O PCBs
--------

V194 STR PCB
2487960102 (2487970102)
|----------------------------------------------------------|
|         RO1_STR-0A.IC16      TRANSFORMER        J105     |
| DIP42                                                    |
|    LED  N341256                  FUSE                    |
|    LED                           FUSE            BF150G8E|
|         N341256                                          |
|                                                          |
|RESET_SW      32MHz             7815                 K2682|
|   MB3771                                                 |
|J101                   DSW2(4, all off)                   |
|            MB90242A                                      |
|                       LED  MB3773    HP3150              |
|                       LED                           K2682|
|                                      HP3150              |
|            EPM7064                                       |
|J104  MAX232                          LM393               |
|       LED   JP1 O O-O                                    |
|       LED                            HP3150         K2682|
|                                                          |
|                                      HP3150              |
|                                                          |
|J103                                                      |
|                UPC358  LM393   UPC358               K2682|
|            J102                            J106          |
|----------------------------------------------------------|
Notes:
      This board is used with Race On! (and Wangan Midnight on Chihiro and Ridge Racer V on System 246) to control the
      steering feed-back motor. It may be used with other System 23/Super System 23 driving/racing games too but no
      other games are confirmed at the moment.

      RO1_STR-0A.IC16 - Fujitsu MB29F400TC 512k x8 flash ROM (SOP44)
                        - Labelled 'RO1 STR-0A' for Race On!
                        - Labelled 'RR3 STR-0A' for Ridge Racer V (on System 246)
      EPM7064         - Altera EPM7064 CPLD labelled 'STR-DR1' (PLCC44)
      N341256         - NKK 32k x8 SRAM (SOP28)
      K2682           - 2SK2682 N-Channel Silicon MOSFET
      BF150G8E        - Large power transistor(?) connected to the transformer
      UPC358          - NEC uPC358 Dual operational amplifier (SOIC8)
      LM393           - National LM393 Low Power Low Offset Voltage Dual Comparator (SOIC8)
      MAX232          - Maxim MAX232 dual serial to TTL logic level driver/receiver (SOIC16)
      HP3150          - ? (DIP8)
      MB3773          - Fujitsu MB3773 Power Supply Monitor with Watch Dog Timer and Reset (SOIC8)
      MB3771          - Fujitsu MB3771 System Reset IC (SOIC8)
      DIP42           - Unpopulated DIP42 socket for 27C4096 EPROM
      MB90242A        - Fujitsu MB90242A 16-Bit CISC ROM-less F2MC-16F Family Microcontroller optimized for mechatronics control applications (TQFP80)
      J101            - 8 pin connector (purpose unknown)
      J102            - 3 pin connector input from potentiometer connected to the steering wheel mechanism
      J103            - Power input connector (5v/GND/12v)
      J104            - 6 pin connector joined with a cable to J6 on the V198 EMI PCB. This cable is the I/O connection to/from the main board.
      J105            - 110VAC power input
      J106            - DC variable power output to feed-back motor


********************************************************************************************

Namco System 22.5 GORgON-based games
Hardware info by Guru
---------------------

Games on this system include....
Rapid River   (Namco, 1997)
Final Furlong (Namco, 1997)

These games run on hardware called "GORgON". This is half-way hardware after Namco Super System 22 and
before System 23. It has similar capabilities to System 23 but like Super System 22 it has sprites also, whereas
System 23 is a full 3D system and doesn't have sprites. The PCBs are about two times larger than System 23.

For Final Furlong (the first game on this system) the system comprises TWO Main PCBs each with ROM PCB on top.
There are 2x GORgON AV PCBs plugged directly into each main board providing connectors for power, audio,
video and comms with 3 separate power supplies for 5V, 12V and 3.3V all located inside a metal box. There is a
separate external I/O PCB ASCA-1A. This connects to the filter board via RS485 using the USB connector.
Main input power is 115VAC.
Final Furlong is a horse racing game.
To control the horse you rock it forwards and backwards continually (it's very tiring to play this game).
This activates one 5K-ohm potentiometer inside the horse body. Essentially the pot just moves from one extreme
to the other. Just like a real horse you need to control the speed so your horse lasts the entire race.
If you rock too much, a message on screen says 'Too Fast'. To steer the horse turn the head sideways using the reins.
There is another 5K-ohm potentiometer in the head to activate the turning direction. The head of the horse also has
two buttons for left and right. This is used to select items and activate the whip. When riding the horse, pressing either
button activates the whip (i.e. they both do the same thing). To select a different track or different horse turn the head.

For Rapid River there is only one main board and ROM board (linking/networking machines is not possible). The I/O
board is ASCA-2A and is different to the I/O board used with Final Furlong and plugs directly into the main board.
This board has the same connectors as the Final Furlong GORgON AV PCB but also has an audio amp and a standard 8 pin
JVS power connector. The I/O board is located inside the metal box. There are two flat cables on the I/O board joining
it to the filter board which is bolted to the outside of the metal box.
Rapid River is controlled by rotating a paddle (for thrust) and turning it sideways (moves left/right).
The rotation action is done with a 5K-ohm potentiometer whereby the thrust is achieved by moving the pot from full left to
full right continuously. The left/right turning movement is just another 5K-ohm potentiometer connected to the column of the paddle
center shaft. There are also some buttons just for test mode, including SELECT, UP & DOWN. The player's seat has movement
controlled by a compressor and several potentiometers. On bootup, the system tests the seat movement and displays a warning
if it's not working. Pressing START allows the game to continue and function normally without the seat movement.


Main PCB
--------

8664960102 (8664970102) GORGON MAIN PCB
|------------------------------------------------------------------------------------------------------|
|                                   J4                       J5                         J6             |
|                              |---------|           |---------| |------| |---------|                  |
|         |---------| |------| |         |           |         | |C401  | |         |HM534251 HM534251 |
| CXD1178Q|         | |C381  | |  C374   |  |------| |  C417   | |      | |  C304   |HM534251 HM534251 |
|         |  C404   | |      | |         |  |C435  | |         | |------| |         |HM534251 HM534251 |
|         |         | |------| |         |  |      | |         | |------| |         |                  |
|         |         |          |---------|  |------| |---------| |C400  | |---------|                  |
|         |---------|     |---------|       |------|             |      | |---------|                  |
|                         |         |       |C435  |    341256   |------| |         |HM534251 HM534251 |
|                         |  C397   |       |      |             |------| |  C304   |HM534251 HM534251 |
|  341256 341256  341256  |         |       |------|    341256   |C401  | |         |HM534251 HM534251 |
|  M5M51008       341256  |         |     |---------|            |      | |         |                  |
|                         |---------|     |         | |------|   |------| |---------|                  |
|  M5M51008       341256         |------| |  C403   | |C406  |   |------| |---------|                  |
|ADM485              |---------| |C379  | |         | |      |   |C400  | |         |HM534251 HM534251 |
|                    |         | |      | |         | |------|   |      | |  C304   |HM534251 HM534251 |
|    M5M51008        |  C300   | |------| |---------|            |------| |         |HM534251 HM534251 |
|                    |         | LH540204  LH540204              |------| |         |                  |
|    M5M51008        |         |341256                 |------|  |C401  | |---------|                  |
|J1   HCPL0611       |---------|341256                 |C407  |  |      | |---------|                  |
|         DS8921                  PST575  PST575       |      |  |------| |         |                  |
|  DS8921                                              |------|  |------| |  C304   |HM534251 HM534251 |
|                 M5M51008                                       |C400  | |         |HM534251 HM534251 |
|       CY7C128             CY2291S                              |      | |         |                  |
|         |------|M5M51008  14.31818MHz                          |------| |---------|                  |
|         |C422  |          J9           M5M5256                 |------| |---------|         3V_BATT  |
|         |      |341256                                         |C400  | |         |                  |
|         |------|341256                                         |      | |  C399   |341256  LEDS(8)   |
|                                   |------|      |--------|     |------| |         |341256            |
|                                   |C352  |      |ALTERA  |     |------| |         |                  |
|  ADM485        DSW1(2)   |------| |      |      |EPM7128 |     |C401  | |---------| DSW3(2)   DSW5(8)|
|    2061ASC               |C416  | |------|      |        |     |      |     |---------| |---------|  |
|      14.7456MHz          |      |               |--------|     |------|     |         | |NKK      |  |
|PAL             |-----|   |------|    |------|                   D4516161    |  C413   | |NR4650   |  |
|                |H8/  |               |C361  |                   D4516161    |         | |LQF-13B  |  |
|                |3002 |               |      |                               |         | |         |  |
|  J10           |-----|      LC321664 |------|    J8                         |---------| |---------|  |
|------------------------------------------------------------------------------------------------------|
Notes:
       NR4650 - NKK NR4650 R4600-based 64-bit RISC CPU (Main CPU, QFP208). Clock input source = CY2291S pin 10
                R4650 master clock input on pin 185 is 33.3333MHz [66.6666/2; CY2291S output = 66.6666]
      H8/3002 - Hitachi H8/3002 HD6413002F17 (Sound CPU, QFP100). Clock input 16.74115MHz. Clock source = ASC2061 MCLKOUT/2
      EPM7128 - Altera EPM7128 CPLD labelled 'GOR-M1' (PLCC84). This chip controls *MANY* chip-enable and clock signals.
          PAL - PALCE16V8H stamped 'GOR-M3' (PLCC20)
     HM534251 - Hitachi HM534251 256kB x4 Dynamic Video RAM (SOJ28)
      N341256 - NKK 32kB x8 SRAM (SOJ28)
      M5M5256 - Mitsubishi 32kB x8 SRAM (SOP28)
     D4516161 - NEC uPD4516161AG5-A80 1M x16 (16MBit) SDRAM (SSOP50)
     LC321664 - Sanyo 64kB x16 EDO DRAM (SOJ40)
     M5M51008 - Mitsubishi 128kB x8 SRAM (SOP32)
      CY7C128 - Cypress 2kB x8 SRAM (SOJ28)
     LH540204 - Sharp CMOS 4096 x 9 Asynchronous FIFO (PLCC32)
    2061ASC-1 - IC Designs 2061ASC-1 Programmable Clock Generator (SOIC16). XTAL input = 14.7456MHz
                This chip uses some internal ROM tables and formulas and is programmed with several registers to generate 2 output clocks.
                The measured values below can vary depending on the input clock frequency and accuracy tolerance.
                Measured Outputs: VCLKOUT - 25.9282MHz, MCLKOUT - 33.4823MHz
      CY2291S - General Purpose EPROM Programmable Clock Generator. Clock input 14.31818MHz
                Full part number is CY2291SC-221. SC="Special Customer". This is custom-programmed at the factory per Namco specifications.
                This chip uses some internal ROM tables to generate 6 output clocks. Not all of the outputs are actually used on the PCB.
                The measured values below can vary depending on the input clock frequency and accuracy tolerance.
                Measured Outputs: CPUCLK - 66.6666MHz, CLKB - 51.200MHz, CLKA - 20.000MHz , CLKF - none, CLKD - none, CLKC - 40.000MHz
                The outputs CLKA/C/D/F are not connected.
       DS8921 - Dallas Semiconductor DS8921 RS-422/423 Differential Line Driver and Receiver Pair (SOIC8)
     HCPL0611 - Fairchild HCPL0611 High Speed 10MBits/sec Logic Gate Optocoupler (SOIC8)
       ADM485 - Analog Devices ADM485 5V Low Power EIA RS-485 Transceiver (SOIC8)
                This is used for I/O PCB communication via H8/3002 signals PB0, P90 and P92.
       PST575 - System Reset IC (SOIC8)
     CXD1178Q - Sony CXD1178Q 8-bit RGB 3-channel D/A converter (QFP48). R,G,B Clock inputs 12.800MHz. Source clock is CY2291S CLKB [51.200/4]
           J1 - 64 pin connector for connection of I/O board
     J4/J5/J6 \
        J8/J9 / Custom NAMCO connectors for connection of MEM(M1) PCB
          J10 - Custom NAMCO connector for MSPM(FR) PCB


     Namco Custom ICs
     ----------------
         C300 (QFP160) - Sprite-related functions
     C304 (x4, QFP120) - Texture-related functions. Grouped with a C400 and C401 for each chip (4 sets).
         C352 (QFP100) - 32-Voice 4-channel 8-bit PCM Sound. Clock input 25.9282MHz (source = 2061ASC-1 pin 9)
         C361 (QFP120) - Text / Character Generator + HSync / VSync Generator
         C374 (QFP160) - Sprite-related functions / Sprite Zoom
          C379 (QFP64) - Sprite-related functions
         C381 (QFP144) - Sprite-related functions
         C397 (QFP160) - Sprite-related functions
         C399 (QFP160) \ This chip ties all the texture outputs from C304, C400 & C401 together and probably does CPU <> 3D System Communication.
     C400 (x4, QFP100) | Texture-related functions (these run burning hot then fail.... result = 3D objects all white and no textures ;-)
      C401 (x4, QFP64) /
         C403 (QFP136) - Polygon-related functions + FIFO data supply source
         C404 (QFP208) - GAMMA, Palette, Pixel Mixer, 24-bit RGB output directly to CXD1178Q (8-bits per color)
         C406 (QFP120) - Polygon-related functions
          C407 (QFP64) - Polygon-related functions
         C413 (QFP208) - Memory Controller
         C416 (QFP176) - CPU <> CPU Communication (R4650 <> H8/3002)
         C417 (QFP208) - Polygon Generator
          C422 (QFP64) - RS422 Networking (Twin Cabinet) Communication Controller
    C435 (x2, TQFP144) - Polygon-related functions


Program ROM PCB
---------------

MSPM(FR) PCB 8699015200 (8699015100)
|--------------------------|
|            J1            |
|                          |
|  IC3               IC1   |
|                          |
|                          |
|                    IC2   |
|--------------------------|
Notes:
      J1 - Connector to plug into Main PCB
     IC1 \
     IC2 / Main Program  (Fujitsu 29F016 16MBit FlashROM, TSOP48)
     IC3 - Sound Program (Fujitsu 29F400T 4MBit FlashROM, TSOP48)

     Games that use this PCB include...

     Game           Code and revision
     --------------------------------
     Rapid River    RD2 Ver.C
     Rapid River    RD3 Ver.C
     Final Furlong  FF2 Ver.A


ROM PCB
-------

MEM(M1) PCB
8664960202 (8664970202)
|--------------------------------------------------------|
|    J2(TEXTURE)        J3(POINT)           J5(SPRITE)   |
| PAL1                                                   |
|                                                        |
|                                                        |
|                                                        |
| CCRL.11A                                               |
|      CCRL.11E  PT3L.12J PT3H.12L  SPRLL.12P SPRLL.12T  |
| CCRH.11B                                               |
|      CCRH.11F                                          |
|                PT2L.11J PT2H.11L  SPRLM.11P SPRLM.11T  |
|                                                        |
|                                                        |
|                PT1L.10J PT1H.10L  SPRUM.10P SPRUM.10T  |
|   PAL2        PAL3                                     |
|                                                        |
|                PT0L.9J  PT0H.9L   SPRUU.9P  SPRUU.9T   |
|                                   JP7       JP9        |
|                                   JP6       JP8        |
| CGLL.8B     CGLL.8F                                    |
|                                                        |
|                                                        |
| CGLM.7B     CGLM.7F                                    |
|      JP2    JP4                                        |
|      JP1    JP3                                        |
| CGUM.6B     CGUM.6F                                    |
|                                          J1(WAVE)      |
|                                                        |
| CGUU.5B     CGUU.5F                      WAVEH.3S      |
|                                                        |
|                    MTBH.5J               WAVEL.2S      |
|                    MTAH.3J                      JP5    |
|                    MTBL.2J                             |
|                    MTAL.1J    KEYCUS                   |
|                                                        |
|                    J4(MOTION)                          |
|--------------------------------------------------------|
Notes:
        PAL1 - PALCE16V8H stamped 'SS22M1' (PLCC20)
        PAL2 - PALCE20V8H stamped 'SS22M2' (PLCC32)
        PAL3 - PALCE20V8H stamped 'SS22M2' (PLCC32)
      KEYCUS - for Rapid River: MACH211 CPLD stamped 'KC012' (PLCC44)
      KEYCUS - for Final Furlong: MACH211 CPLD stamped 'KC011' (PLCC44)
      J1->J5 - Custom NAMCO connectors for joining ROM PCB to Main PCB
     JP1/JP2 \
     JP3/JP4 |
     JP5     | Jumpers to set ROM sizes (32M/64M)
     JP6/JP7 |
     JP8/JP9 /

     ROMs
     ----
           PT* - Point ROMs, sizes configurable to either 16M or 32M (SOP44)
           MT* - Motion ROMs, sizes configurable to either 32M or 64M (SOP44)
           CG* - Texture ROMs, sizes configurable to either 32M or 64M (SOP44)
          CCR* - Texture Tilemap ROMs, sizes fixed at 16M (SOP44)
          SPR* - Sprite ROMs, sizes configurable to either 32M or 64M (SOP44)
          WAVE*- Wave ROMs, sizes configurable to either 32M or 64M (SOP44)

Other Boards
------------

GORgON AV PCB
8664960301 (8664970301)
|------------------------------------|
|J2   J3     J4        J5       J6   |
|            BD-8                    |
|  PC410  74AC244 NJM2100*           |
|    74AC00  LC78815       NJM2100*  |
|             J1    LC78815          |
|------------------------------------|
Notes: (* = these parts on bottom side of PCB)
     J1 - 64 pin connector for connection to Main PCB
     J2 - 10 pin connector
     J3 - 15 pin HD15 DSUB connector
     J4 - Dual Red/White RCA Jacks (Twin Stereo Audio)
     J5 - USB-A connector
     J6 - 6 pin power input connector (GND, GND, GND, 5V, 5V, 12V)
   BD-8 - TDK ZBDS5101-8 Ferrite Bead SMD Array
  PC410 - Sharp PC410 Photocoupler
 74AC00 - 74AC00 Quad 2-Input NAND Gate
74AC244 - 74AC244 Octal Buffer/Line Driver with Tri-state Outputs
NJM2100 - New Japan Radio Co. NJM2100 Dual Operational Amplifier
LC78815 - Sanyo LC78815 2-Channel 16-Bit D/A Converter

This board plugs into the mainboard used for Final Furlong and connects to ASCA-1A I/O PCB.


V187 ASCA-2B PCB
2477960201 (2477970201)
|-----------------------------------------------|
|                                               |
|  J207      J204     J206       J205     J203  |
|                                               |
|                                               |
|                                               |
|          J202*                    J201*       |
|-----------------------------------------------|
Notes: (* = these parts on bottom side of PCB)
      J201 - 34 pin flat cable connector for connection to ASCA-2A I/O PCB
      J202 - 50 pin flat cable connector for connection to ASCA-2A I/O PCB
      J203 - 9 pin connector. Pinout: 1 RED, 2 GREEN, 3 BLUE, 4 GND, 5 CSYNC, 6 SPK L+, 7 SPK R-, 8 SPK R+, 9 SPK L-
      J204 - 15 pin connector. Pinout: 1 GND, 2 12V, 3 GND, 4 5K-POT, 5 5K-POT, 6 SELECT, 7 NC, 8 NC, 9 UP, 10 5V, 11 NC, 12 NC, 13 LAMP, 14 START, 15 DOWN
             When wired to Final Furlong changes are: 11 RIGHT, 14 LEFT
      J205 - 6 pin connector. Pinout: 1 SERVICE, 2 TEST, 3 COIN, 4 GND, 5 NC, 6 NC
      J206 - 12 pin connector
      J207 - 12 pin connector. Pinout: 1 SOL FR, 2 SOL FL, 3 SOL RR, 4 SOL RL, 5 12V, 6 NC, 7 5K POT RL, 8 GND, 9 5K POT FR, 10 5K POT FL, 11 5K POT RR, 12 5V
             FR/FL/RR/RL means Front Left, Front Right, Rear Right, Rear Left. SOL means Solenoid.

This is the filter board bolted to the outside of the metal box for Rapid River. It plugs into V187 ASCA-2A I/O PCB with 2 flat cables.
It can also be used with Final Furlong when wired correctly.


*/

#include "emu.h"
#include "bus/jvs/namcoio.h"
#include "bus/rs232/rs232.h"
#include "cpu/h8/h83002.h"
#include "cpu/mips/mips3.h"
#include "cpu/sh/sh7604.h"
#include "machine/6850acia.h"
#include "machine/clock.h"
#include "machine/nvram.h"
#include "machine/rtc4543.h"
#include "sound/c352.h"
#include "video/poly.h"
#include "video/rgbutil.h"
#include "emupal.h"
#include "screen.h"
#include "speaker.h"

#include "md8412b_s23.h"
#include "namco_c139.h"
#include "namco_settings.h"
#include "vpx3220a.h"

#include <cfloat>
#include <cstdlib>  // P001/P002/P003: std::getenv for the NAMCOS23_PATCH_* experiment gates
#include <cstring>  // P059: std::memcpy for the u32->float reinterpret of the +0xCC/+0x80 world base
#include <cmath>    // P059: std::isfinite / std::fabs for the correction/drift magnitude guards

#define LOG_CLIP_DATA       (1ULL << 1)
#define LOG_3D_STATE_ERR    (1ULL << 2)
#define LOG_3D_STATE_UNK    (1ULL << 3)
#define LOG_MATRIX_ERR      (1ULL << 4)
#define LOG_MATRIX_UNK      (1ULL << 5)
#define LOG_VEC_ERR         (1ULL << 6)
#define LOG_VEC_UNK         (1ULL << 7)
#define LOG_RENDER_ERR      (1ULL << 8)
#define LOG_RENDER_INFO     (1ULL << 9)
#define LOG_MODEL_ERR       (1ULL << 10)
#define LOG_MODEL_INFO      (1ULL << 11)
#define LOG_MODELS          (1ULL << 12)
#define LOG_C435_PIO_UNK    (1ULL << 13)
#define LOG_C435_UNK        (1ULL << 14)
#define LOG_C417_UNK        (1ULL << 15)
#define LOG_C417_ACK        (1ULL << 16)
#define LOG_C412_UNK        (1ULL << 17)
#define LOG_C421_UNK        (1ULL << 18)
#define LOG_C422_IRQ        (1ULL << 19)
#define LOG_C422_UNK        (1ULL << 20)
#define LOG_C361_UNK        (1ULL << 21)
#define LOG_CTL_UNK         (1ULL << 22)
#define LOG_MCU             (1ULL << 23)
#define LOG_SH2             (1ULL << 24)
#define LOG_SUBIRQ          (1ULL << 25)
#define LOG_SPRITES         (1ULL << 26)
#define LOG_IOMCU           (1ULL << 27)
#define LOG_ADC_RD          (1ULL << 28)
#define LOG_ADC_WR          (1ULL << 29)
#define LOG_C417_IRQ        (1ULL << 30)
#define LOG_C361_IRQ        (1ULL << 31)
#define LOG_MATRIX_INFO     (1ULL << 32)
#define LOG_VEC_INFO        (1ULL << 33)
#define LOG_CTL_REG         (1ULL << 34)
#define LOG_C435_REG        (1ULL << 35)
#define LOG_C361_REG        (1ULL << 36)
#define LOG_C417_REG        (1ULL << 37)
#define LOG_C412_RAM        (1ULL << 38)
#define LOG_C421_RAM        (1ULL << 39)
#define LOG_C404_REGS       (1ULL << 40)
#define LOG_C404_RAM        (1ULL << 41)
#define LOG_GMEN            (1ULL << 42)
#define LOG_MCU_PORTS       (1ULL << 43)
#define LOG_RS232           (1ULL << 44)
#define LOG_IRQ_STATUS      (1ULL << 45)
#define LOG_C451            (1ULL << 46)
#define LOG_SH2_VPX         (1ULL << 47)
#define LOG_DIRECT          (1ULL << 48)
#define LOG_ALL ( LOG_CLIP_DATA | LOG_3D_STATE_ERR | LOG_3D_STATE_UNK | LOG_VEC_ERR | LOG_VEC_UNK | LOG_RENDER_ERR | LOG_RENDER_INFO | LOG_MODEL_ERR | \
				LOG_MODEL_INFO | LOG_MODELS | LOG_C435_PIO_UNK | LOG_C435_UNK | LOG_C417_UNK | LOG_C417_ACK | LOG_C412_UNK | LOG_C421_UNK | \
				LOG_C422_IRQ | LOG_C422_UNK | LOG_C361_UNK | LOG_CTL_UNK | LOG_C417_IRQ | LOG_C361_IRQ | LOG_MATRIX_INFO | LOG_VEC_INFO | \
				LOG_CTL_REG | LOG_C435_REG | LOG_C361_REG | LOG_C417_REG | LOG_C412_RAM | LOG_C421_RAM | LOG_C404_REGS | LOG_C404_RAM | LOG_GMEN | \
				LOG_GENERAL | LOG_RS232 | LOG_IRQ_STATUS | LOG_C451 | LOG_MATRIX_UNK | LOG_VEC_UNK | LOG_MCU_PORTS | LOG_DIRECT )

#define VERBOSE ( LOG_C422_IRQ | LOG_C422_UNK | LOG_IRQ_STATUS )
#include "logmacro.h"

namespace
{

#define JVSCLOCK    (XTAL(14'745'600))

#define H8CLOCK     (16934400)      /* based on research (superctr) */
#define BUSCLOCK    (16934400*2)
#define C352CLOCK   (25401600)
#define C352DIV     (288)

#define PIXEL_CLOCK (51.2_MHz_XTAL/4*2)

#define HTOTAL      (814)
#define HBEND       (0)
#define HBSTART     (640)

#define VTOTAL      (525)
#define VBEND       (0)
#define VBSTART     (480)

#define MAIN_VBLANK_IRQ 0x01
#define MAIN_C361_IRQ   0x02
#define MAIN_SUBCPU_IRQ 0x04
#define MAIN_C435_IRQ   0x08
#define MAIN_C422_IRQ   0x10
#define MAIN_C450_IRQ   0x20
#define MAIN_C451_IRQ   0x40
#define MAIN_RS232_IRQ  0x100

#define DUMP_MODELS 0

static s32 u32_to_s24(u32 v);
static s32 u32_to_s10(u32 v);

// P067 EXPERIMENT (branch patch/defaults-on): the ADOPTED patch recipe becomes
// the BUILT-IN DEFAULT - a plain launch (no env vars at all) runs the full
// adopted link-play recipe.  Uniform resolution for every ADOPTED patch env
// var (the same helper is duplicated in namco_c139.cpp for the device-side
// reads; keep the two copies identical):
//   env UNSET (or empty) -> the baked-in adopted default value (defval);
//                           the one-shot banner reports source "(default)"
//   env == literal "0"   -> DISABLED, the kill switch: the SAME inert code
//                           path that "unset" selected before P067
//   env == anything else -> that value, parsed exactly as before;
//                           the banner reports source "(env=<value>)"
// Returns the EFFECTIVE value string (defval or the env value), or nullptr
// when killed via "0".  from_env reports where the value came from.
// NON-adopted vars (LINK_WAIT, TCP_NODELAY, NEWEST_WINS, HB_PHASE_AWARE,
// TX_COMPLETE_IRQ, CORR_SMOOTH+knobs, every retired patch, and ALL
// NAMCOS23_TRACE_*) keep the old "armed only when set" idiom - do NOT route
// them through this helper.  MODEL PROVENANCE: Fable 5.
static char const *patch_env_or_default(char const *name, char const *defval, bool &from_env)
{
	char const *const env = std::getenv(name);
	from_env = env && env[0] != '\0';
	if (!from_env)
		return defval;                              // unset/empty -> adopted default
	if (env[0] == '0' && env[1] == '\0')
		return nullptr;                             // literal "0" -> kill switch
	return env;                                     // env override, parsed as before
}

// Banner source tag for the P067 defaults: "(default)" or "(env=<value>)".
static std::string patch_env_src(bool from_env, char const *effective)
{
	if (!from_env)
		return std::string("(default)");
	return std::string("(env=") + (effective ? effective : "0") + ")";
}

enum
{
	MODEL,
	IMMEDIATE,
	DIRECT,
	SPRITE
};

enum
{
	RENDER_MAX_ENTRIES = 4000,
	POLY_MAX_ENTRIES = 40000
};

struct c404_mixer_regs_t
{
	u8 poly_fade_r;
	u8 poly_fade_g;
	u8 poly_fade_b;
	u8 fog_r;
	u8 fog_g;
	u8 fog_b;
	u8 bgcolor_r;
	u8 bgcolor_g;
	u8 bgcolor_b;
	u16 spot_factor;
	u8 poly_alpha_color;
	u8 poly_alpha_pen;
	u8 poly_alpha;
	u8 alpha_check12;
	u8 alpha_check13;
	u8 alpha_mask;
	u8 alpha_factor;
	u8 screen_fade_r;
	u8 screen_fade_g;
	u8 screen_fade_b;
	u8 screen_fade_factor;
	u8 fade_flags;
	u16 palbase;
	u8 layer_flags;
};

struct namcos23_model_data
{
	u16 model;
	u16 model2;
	s16 m[9];
	s32 v[3];
	float scaling;
	bool transpose;
	s32 light_vector[3];
};

struct namcos23_direct_data
{
	u16 d[28];
};

struct namcos23_immediate_data
{
	u32 type;
	u32 h;
	u32 pal;
	u32 zbias;
	u32 i[4];
	u32 u[4];
	u32 v[4];
	u32 x[4];
	u32 y[4];
	u32 z[4];
};

struct namcos23_sprite_data
{
	s16 xpos;
	s16 ypos;
	s16 xsize;
	s16 ysize;
	u8 rows;
	u8 cols;
	u8 linktype;
	u8 alpha;
	u16 code;
	u8 xflip;
	u8 yflip;
	u8 cz;
	u8 prioverchar;
	u32 zcoord;
	u8 color;
	u8 fade_enabled;
};

struct namcos23_render_entry
{
	int type;
	u16 absolute_priority;
	u16 tx;
	u16 ty;
	u16 model_blend_factor;
	u16 light_power;
	u16 light_ambient;
	bool mirror_x;
	u8 poly_fade_r;
	u8 poly_fade_g;
	u8 poly_fade_b;
	u8 poly_alpha_color;
	u8 poly_alpha_pen;
	u8 poly_alpha;
	u8 screen_fade_r;
	u8 screen_fade_g;
	u8 screen_fade_b;
	u8 screen_fade_factor;
	u8 fade_flags;
	s16 vp_size_x;
	s16 vp_size_y;
	s16 vp_offset_x;
	s16 vp_offset_y;
	float vp_fov;

	union
	{
		namcos23_model_data model;
		namcos23_direct_data direct;
		namcos23_immediate_data immediate;
		namcos23_sprite_data sprite;
	};
};

struct namcos23_render_data
{
	u32 rgb;
	const pen_t *pens;
	bitmap_rgb32 *bitmap;
	bitmap_ind8 *primap;
	u32 flags;
	s32 polycolor_r;
	s32 polycolor_g;
	s32 polycolor_b;
	u16 model;
	bool direct;
	bool immediate;
	bool sprite;
	int tbase;
	u8 cmode;
	u16 cz_value;
	u8 cz_type;
	const u8 *sprite_source;
	u32 h;
	u32 type;
	u8 sprite_line_modulo;
	u8 sprite_xflip;
	u8 sprite_yflip;
	u8 fogfactor;
	bool pfade_enabled;
	s32 fadefactor;
	s32 fadefactor_inv;
	bool shade_enabled;
	bool alpha_enabled;
	bool blend_enabled;
	u8 alphafactor;
	s32 alpha;
	s32 alpha_inv;
	u8 poly_alpha_pen;
	u8 prioverchar;
	s16 vp_size_x;
	s16 vp_size_y;
	s16 vp_offset_x;
	s16 vp_offset_y;

	s32 fadecolor_r;
	s32 fadecolor_g;
	s32 fadecolor_b;
	bool stencil_enabled;
};

class namcos23_state;

class namcos23_renderer : public poly_manager<float, namcos23_render_data, 4>
{
public:
	namcos23_renderer(namcos23_state &state, const u16 *tmlrom, const u8 *tmhrom, const u8 *texrom, const u16 *texram,
		const u32 tileid_mask, const u32 tile_mask);
	void render_flush(screen_device &screen, bitmap_rgb32 &bitmap);
	void render_sprite_scanline(s32 scanline, const extent_t& extent, const namcos23_render_data& object, int threadid);

	template <bool Stencil, bool Shade, bool PolyFade, bool ColorFade, bool Blend, bool PolyAlpha>
	void render_scanline(s32 scanline, const extent_t& extent, const namcos23_render_data& object, int threadid);

private:
	bool stencil_lookup(u32 x, u32 y);
	u32 texture_lookup(const pen_t *pens, int penshift, int penmask, u32 x, u32 y, u8 &pen);

	namcos23_state& m_state;
	std::unique_ptr<u32[]> m_tmrom_decoded;
	std::unique_ptr<u8[]> m_texattr_decoded;
	const u8 *m_texrom;
	const u16 *m_texram;
	u32 m_tileid_mask;
	u32 m_tile_mask;
};

typedef namcos23_renderer::vertex_t poly_vertex;

struct namcos23_poly_entry
{
	namcos23_render_data rd;
	int vertex_count;
	int zkey;
	int index;
	poly_vertex pv[16];
};


struct c417_t
{
	u16 ram[0x10000];
	u16 adr;
	u32 pointrom_adr;
	bool test_mode;
};

struct c412_t
{
	u16 sdram_a[0x100000]; // Framebuffers, probably
	u16 sdram_b[0x100000];
	u16 sram[0x20000];     // Ram-based tiles for alpha-cutout drawing
	u16 pczram[0x200];     // PCZ Convert RAM
	u32 adr;
	u16 status_c;
};

struct c421_t
{
	u16 dram_a[0x40000];
	u16 dram_b[0x40000];
	u16 sram[0x8000];
	u32 adr;
};

struct c422_t
{
	s16 regs[0x10];
};

struct c361_t
{
	emu_timer *timer;
	int scanline;
};

struct c404_t
{
	u8 poly_fade_r;
	u8 poly_fade_g;
	u8 poly_fade_b;
	u8 fog_r;
	u8 fog_g;
	u8 fog_b;
	u8 bgcolor_r;
	u8 bgcolor_g;
	u8 bgcolor_b;
	u16 spot_factor;
	u8 poly_alpha_color;
	u8 poly_alpha_pen;
	u8 poly_alpha;
	u8 alpha_check12;
	u8 alpha_check13;
	u8 alpha_mask;
	u8 alpha_factor;
	u8 screen_fade_r;
	u8 screen_fade_g;
	u8 screen_fade_b;
	u8 screen_fade_factor;
	u8 fade_flags;
	u16 palbase;
	u8 layer_flags;
	u16 ram[0x400];
	u16 spritedata_idx;
	u64 rowscroll_frame;
	u16 rowscroll[480];
	u16 linexscroll[1024];
	u16 lastrow;
	u16 xscroll;
	u16 yscroll;
	struct
	{
		u16 d[4];
	} sprites[0x280];
};

struct c435_t
{
	u32 address;
	u32 size;
	u16 buffer[256];
	int buffer_pos;
	u16 direct_buf[28];
	u16 direct_vertex[6];
	u16 direct_vert_pos;
	u16 direct_buf_pos;
	bool direct_buf_nonempty;
	bool direct_buf_open;
	u16 pio_mode;
	u16 sprite_target;
	u16 spritedata[0x10000];
};

struct render_t
{
	std::unique_ptr<namcos23_renderer> polymgr;
	int cur;
	int poly_count;
	int count[2];
	namcos23_render_entry entries[2][RENDER_MAX_ENTRIES];
	namcos23_poly_entry polys[POLY_MAX_ENTRIES];
	namcos23_poly_entry *poly_order[POLY_MAX_ENTRIES];
};

class namcos23_state : public driver_device
{
public:
	namcos23_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_subcpu(*this, "subcpu"),
		m_adc(*this, "subcpu:adc"),
		m_jvs(*this, "jvs"),
		m_rtc(*this, "rtc"),
		m_settings(*this, "namco_settings"),
		m_c139(*this, "c139"),
		m_mainram(*this, "mainram"),
		m_shared_ram(*this, "shared_ram", 0x10000, ENDIANNESS_BIG),
		m_charram(*this, "charram"),
		m_textram(*this, "textram"),
		m_czattr(*this, "czattr", 0x10, ENDIANNESS_BIG),
		m_lightx(*this, "LIGHTX"),
		m_lighty(*this, "LIGHTY"),
		m_p1(*this, "P1"),
		m_p2(*this, "P2"),
		m_screen(*this, "screen"),
		m_palette(*this, "palette"),
		m_generic_paletteram_32(*this, "paletteram"),
		m_tmlrom(nullptr),
		m_tmhrom(nullptr),
		m_texrom(nullptr),
		m_texram(nullptr),
		m_tileid_mask(0),
		m_tile_mask(0),
		m_jvs_sense(jvs_port_device::sense::None),
		m_main_irqcause(0),
		m_ctl_vbl_active(false),
		m_ctl_led(0),
		m_subcpu_running(false),
		m_ptrom(nullptr),
		m_ptrom_limit(0),
		m_subcpu_scanline_on_timer(nullptr),
		m_subcpu_scanline_off_timer(nullptr),
		m_absolute_priority(0),
		m_tx(0),
		m_ty(0),
		m_model_blend_factor(0x4000),
		m_light_power(0),
		m_light_ambient(0),
		m_clip_data_line(0),
		m_vp_size_x(320),
		m_vp_size_y(240),
		m_vp_offset_x(0),
		m_vp_offset_y(0),
		m_scaling(0x4000),
		m_c361_irqnum(0),
		m_c422_irqnum(0),
		m_c435_irqnum(0),
		m_vbl_irqnum(0),
		m_sub_irqnum(0),
		m_rs232_irqnum(0),
		m_sub_port8(0),
		m_sub_porta(0),
		m_sub_portb(0),
		m_lamps(*this, "lamp%u", 0U)
	{ }

	void s23(machine_config &config);
	void timecrs2(machine_config &config);
	void downhill(machine_config &config);
	void panicprk(machine_config &config);

	render_t m_render;
	const u8 *m_sprrom;

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	virtual void video_start() override ATTR_COLD;

	void mips_base_map(address_map &map) ATTR_COLD;
	void mips_map(address_map &map) ATTR_COLD;
	void s23h8rwmap(address_map &map) ATTR_COLD;

	void c361_map(address_map &map, const u32 addr);
	void c404_map(address_map &map, const u32 addr);

	virtual void configure_jvs(device_jvs_interface &io);

	void irq_update_common(u32 cause);
	virtual void irq_update(u32 cause);
	void subcpu_irq1_update(int state);

	void paletteram_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	void sprites_idx_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void sprites_data_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	u16 c404_ram_r(offs_t offset);
	void c404_ram_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void c404_poly_fade_red_w(offs_t offset, u16 data);
	void c404_poly_fade_green_w(offs_t offset, u16 data);
	void c404_poly_fade_blue_w(offs_t offset, u16 data);
	void c404_fog_red_w(offs_t offset, u16 data);
	void c404_fog_green_w(offs_t offset, u16 data);
	void c404_fog_blue_w(offs_t offset, u16 data);
	void c404_bg_red_w(offs_t offset, u16 data);
	void c404_bg_green_w(offs_t offset, u16 data);
	void c404_bg_blue_w(offs_t offset, u16 data);
	void c404_spot_lsb_w(offs_t offset, u16 data);
	void c404_spot_msb_w(offs_t offset, u16 data);
	void c404_poly_alpha_color_w(offs_t offset, u16 data);
	void c404_poly_alpha_pen_w(offs_t offset, u16 data);
	void c404_poly_alpha_w(offs_t offset, u16 data);
	void c404_alpha_check12_w(offs_t offset, u16 data);
	void c404_alpha_check13_w(offs_t offset, u16 data);
	void c404_text_alpha_mask_w(offs_t offset, u16 data);
	void c404_text_alpha_factor_w(offs_t offset, u16 data);
	void c404_screen_fade_red_w(offs_t offset, u16 data);
	void c404_screen_fade_green_w(offs_t offset, u16 data);
	void c404_screen_fade_blue_w(offs_t offset, u16 data);
	void c404_screen_fade_factor_w(offs_t offset, u16 data);
	void c404_fade_flags_w(offs_t offset, u16 data);
	void c404_palette_base_w(offs_t offset, u16 data);
	void c404_layer_flags_w(offs_t offset, u16 data);

	virtual u16 c417_status_r();
	u16 c417_addr_r();
	void c417_addr_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void c417_ptrom_addr_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 c417_test_done_r();
	void c417_ptrom_addr_clear_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 c417_ram_r();
	void c417_ram_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 c417_ptrom_msw_r();
	u16 c417_ptrom_lsw_r();
	void c417_irq_ack_w(offs_t offset, u16 data);

	u16 c412_flags_r(); // offset 0x06
	u16 c412_addr_lsw_r(); // offset 0x10
	u16 c412_addr_msw_r(); // offset 0x12
	u16 c412_ram_r(); // offset 0x14
	u16 c412_status_r(); // offset 0x18
	void c412_flags_w(offs_t offset, u16 data, u16 mem_mask = ~0); // offset 0x04
	void c412_addr_lsw_w(offs_t offset, u16 data, u16 mem_mask = ~0); // offset 0x10
	void c412_addr_msw_w(offs_t offset, u16 data, u16 mem_mask = ~0); // offset 0x12
	void c412_ram_w(offs_t offset, u16 data, u16 mem_mask = ~0); // offset 0x14

	u16 c421_ram_r();
	void c421_ram_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 c421_addr_msw_r();
	void c421_addr_msw_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 c421_addr_lsw_r();
	void c421_addr_lsw_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	void direct_buf_start_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void direct_buf_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	void ctl_leds_w(offs_t offset, u16 data);
	u16 ctl_status_r();
	u16 ctl_input1_r();
	void ctl_input1_w(offs_t offset, u16 data);
	u16 ctl_input2_r();
	void ctl_input2_w(offs_t offset, u16 data);
	void ctl_vbl_ack_w(offs_t offset, u16 data);
	void ctl_direct_poly_w(offs_t offset, u16 data);

	void c361_xscroll_w(offs_t offset, u16 data);
	void c361_yscroll_w(offs_t offset, u16 data);
	void c361_irq_scanline_w(offs_t offset, u16 data);
	u16 c361_vpos_r();
	u16 c361_vblank_r();
	TIMER_CALLBACK_MEMBER(c361_timer_cb);

	u16 c422_r(offs_t offset);
	void c422_irq_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	void c422_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	// Bridge from namco_c139_device's IRQ output line back into the
	// existing irq_update() / MAIN_C422_IRQ infrastructure.
	void c139_irq_w(int state);

	void mcuen_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	u32 c435_busy_flag_r();
	void c435_dma_addr_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	void c435_dma_size_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	void c435_dma_start_w(address_space &space, offs_t offset, u32 data, u32 mem_mask = ~0);
	void c435_clear_bufpos_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	void c435_state_pio_w(u16 data);
	void c435_state_reset_w(u16 data);
	void c435_pio_w(offs_t offset, u16 data);
	void c435_sprite_w(u16 data);

	void c435_dma(address_space &space, u32 adr, u32 size);

	s32 *c435_getv(u16 id);
	s16 *c435_getm(u16 id);

	void c435_state_set_interrupt(const u16 *param);
	void c435_state_set_viewport_data(const u16 *param);
	void c435_state_set_clip_data_line(const u16 *param);
	void c435_state_set(u16 type, const u16 *param);
	int c435_get_state_entry_size(u16 type);

	void c435_matrix_matrix_mul();
	void c435_matrix_vector_mul();
	void c435_matrix_matrix_immed_mul();
	void c435_matrix_vector_immed_mul();
	void c435_matrix_set();
	void c435_vector_set();
	void c435_state_set();
	void c435_unk_set0();
	void c435_absolute_priority_set();
	void c435_tx_set();
	void c435_ty_set();
	void c435_scaling_set();
	void c435_model_blend_factor_set();
	void c435_unk_set6();
	void c435_light_power_set();
	void c435_light_ambient_set();
	void c435_render();
	void c435_flush();

	void sharedram_sub_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 sharedram_sub_r(offs_t offset);

	void sub_interrupt_main_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 sub_comm_status_r();
	u16 sub_comm_data_r();
	void sub_comm_data_w(offs_t offset, u8 data);

	u8 mcu_p8_r();
	void mcu_p8_w(u8 data);
	u8 mcu_pa_r();
	void mcu_pa_w(u8 data);
	u8 mcu_pb_r();
	void mcu_pb_w(u8 data);
	u8 mcu_p6_r();
	void mcu_p6_w(u8 data);

	virtual u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	TIMER_CALLBACK_MEMBER(subcpu_scanline_on_tick);
	TIMER_CALLBACK_MEMBER(subcpu_scanline_off_tick);
	virtual void vblank(int state);

	u8 nthbyte(const u32 *pSource, int offs);
	float f24_to_f32(u32 v);

	void render_apply_transform(s32 xi, s32 yi, s32 zi, const namcos23_render_entry *re, float &x, float &y, float &z);
	void render_apply_matrot(s32 xi, s32 yi, s32 zi, const namcos23_render_entry *re, float &x, float &y, float &z);
	void render_project(poly_vertex &v, const s16 vp_size_x, const s16 vp_size_y, const float vp_fov);
	void render_model(const namcos23_render_entry *re);
	void render_direct_poly(const namcos23_render_entry *re);
	void render_immediate(const namcos23_render_entry *re);
	virtual void dispatch_render_entry(const namcos23_render_entry *re);
	virtual void render_run(screen_device &screen, bitmap_rgb32 &bitmap);

	void update_text_rowscroll();
	void apply_text_scroll();
	void draw_text_layer(screen_device &screen);
	void mix_text_layer(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect, int prival);

	required_device<mips3_device> m_maincpu;
	required_device<h83002_device> m_subcpu;
	required_device<h8_adc_device> m_adc;
	required_device<jvs_port_device> m_jvs;
	required_device<rtc4543_device> m_rtc;
	required_device<namco_settings_device> m_settings;
	optional_device<namco_c139_device> m_c139;  // Twin-cabinet link controller (silkscreened "C422" on System 23 PCBs)
	required_shared_ptr<u32> m_mainram;
	memory_share_creator<u32> m_shared_ram;
	required_shared_ptr<u32> m_charram;
	required_shared_ptr<u32> m_textram;
	memory_share_creator<u32> m_czattr;
	optional_ioport m_lightx;
	optional_ioport m_lighty;
	required_ioport m_p1;
	required_ioport m_p2;
	required_device<screen_device> m_screen;
	required_device<palette_device> m_palette;
	required_shared_ptr<u32> m_generic_paletteram_32;

	c404_t m_c404;
	c361_t m_c361;
	c417_t m_c417;
	c412_t m_c412;
	c421_t m_c421;
	c422_t m_c422;
	c435_t m_c435;
	std::unique_ptr<bitmap_ind16> m_mix_bitmap;

	const u16 *m_tmlrom;
	const u8 *m_tmhrom;
	const u8 *m_texrom;
	const u16 *m_texram;
	u32 m_tileid_mask;
	u32 m_tile_mask;

	u8 m_jvs_sense;

	u32 m_main_irqcause;

	bool m_ctl_vbl_active;
	u8 m_ctl_led;
	u16 m_ctl_inp_buffer[2];

	bool m_subcpu_running;

	// P001 EXPERIMENT (branch patch/keepalive-floor, cherry-picked onto
	// patch/continuous-arm): env-gated in-game link keepalive floor, read
	// once in machine_start(), applied in vblank().  P008 enhancement: the
	// env var's numeric value is the floor value (plain "1" = exact P001
	// semantics; "2" floors at 2 so the in-frame service decrement cannot
	// reach 0 and fire the COM-fallback).  Experiment branch only - never
	// merge to milestone.
	bool m_patch_keepalive_floor;
	u32 m_keepalive_floor_value;

	// P002 EXPERIMENT (branch patch/vblank-lockstep): env-gated 1/s status
	// tap of the ROM's frame-drift word 0x802F3504 and keepalive 0x802F3FD8,
	// plus the lockstep token/stall counters from the C139 device.  Gate read
	// once in machine_start(), sampled in vblank().  The barrier itself lives
	// in namco_c139.  Experiment branch only - never merge to milestone.
	bool m_patch_vblank_lockstep;
	u16 m_lockstep_tap_max_drift;
	u32 m_lockstep_tap_min_keepalive;
	u32 m_lockstep_tap_mode2_samples;

	// P003 EXPERIMENT (branch patch/round-start-arm): env-gated MAME-side
	// re-arm of the partner record's ingest bit on each keepalive refill
	// edge, plus read-only observability of the record flag words.  Gate
	// read once in machine_start(), applied in vblank().  Independent of
	// the P002 gate.  Experiment branch only - never merge to milestone.
	bool m_patch_round_start_arm;
	bool m_rsa_have_prev;
	u32 m_rsa_prev_keepalive;
	u32 m_rsa_prev_rec0;
	u32 m_rsa_prev_rec1;
	u32 m_rsa_low24_changes;

	// P008 EXPERIMENT (branch patch/continuous-arm): env-gated CONTINUOUS
	// MAME-side re-arm of the partner record's ingest bit - every vblank
	// while mode == 2 and the keepalive is alive, not just on refill edges
	// (the P003 edge trigger can never fire alongside the P001 floor, and
	// its single firing lost the within-frame COM-fallback race).  Gate read
	// once in machine_start(), applied in vblank() AFTER the P001 floor
	// write.  Independent of the P001/P002/P003 gates.  Experiment branch
	// only - never merge to milestone.
	bool m_patch_continuous_arm;
	u32 m_ca_rearm_count;

	// P009 EXPERIMENT (branch patch/qual-trace): READ-ONLY qualification
	// event tap - detects, at vblank granularity, the ROM-side events that
	// mark a delivered RX frame as QUALIFYING (drift-word reset / remote-seq
	// mirror change via 0x8000B8F0) or FAILED (checksum counter gp+0x75C8
	// increment), plus the RX ring head/tail so deliveries can be tracked
	// stage by stage.  Gate read once in machine_start(), sampled in
	// vblank().  Independent of all patch gates.  Experiment branch only -
	// never merge to milestone.
	bool m_trace_qual;
	bool m_qt_have_prev;
	u16 m_qt_prev_drift;
	u16 m_qt_prev_succ;
	u16 m_qt_prev_rseq;
	u16 m_qt_prev_head;
	u16 m_qt_prev_tail;
	u32 m_qt_win_quals;
	u32 m_qt_win_chkfails;
	u32 m_qt_win_enq;
	u32 m_qt_win_drain;

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): READ-ONLY
	// adoption / render-gate / cutscene-timer trace.  P014 proved the >255hw
	// bulk frame reassembles, crosses the link, and BLUE DMA-ingests the 718B
	// op-0x17/0x71/0xfe actor/scene batch (its cutscene advances on delivery)
	// but then FREEZES at shot 0x118003: blue renders the environment but never
	// the characters, bit 0x2000 ("render gate" / remote-controlled, aka bit13)
	// never sets, and the 0x0080d0 render-window low-24 signature is absent on
	// both cabs.  So the data ARRIVES but is not ADOPTED into the actor/render
	// path.  This block instruments enough to decide between (a) wrong-slot
	// ingest, (b) render-gate gated on a ready-bit that never sets, (c) a
	// missing 2nd link direction.  Every tap is READ-ONLY (no write to game RAM
	// or device state).  Gate read once in machine_start(), sampled in vblank().
	// Independent of all other gates.  Experiment branch only - never merge to
	// milestone.
	//
	// Addresses (verified from gold/full.txt, see 2026-06-16-p015 agent-log):
	//  - partner/team record family: base 0x802F4060, stride 0x474; +0x370 flags
	//    word rec0=0x802F43D0 rec1=0x802F4844.  op-0x17 batch consumer 0x800AC0E8
	//    writes the SAME record family (record base 0x802F43C0/0x802F4834) at
	//    sub-fields +0xA8/+0xAC/+0xB0 (s32 position X/Y/Z) and +0xE0 (hw coord);
	//    => rec0 pos words 0x802F4468/6C/70, rec1 0x802F48DC/E0/E4 (= base+0xA8..).
	//  - actor-spawn array 0x802F4DB0, guard 0x802F4DC8, stride 0x2B8; element
	//    +0x6C active, +0x1F4 coord0.  Slot 0 active word = 0x802F4DB0+0x6C.
	//  - bit-0x2000 ARM SITE: ONLY op-55 RX -> 0x80013D44 sw $a2,0x370 (0x80013E10)
	//    copies the peer's 24-bit wire flags (incl 0x2000) verbatim; round-start
	//    sets only 0x8000/0x8002, dispatcher never sets 0x2000.  So bit 0x2000 is
	//    on iff the PEER sends an op-55 frame carrying it.  We cannot see the wire
	//    flags from here, but we observe whether bit 0x2000 ever appears (and the
	//    low-24 0x0080d0 render composite) on either record around the freeze.
	//  - gp+0x7074==2 derivation 0x80016BCC: partner +0x370 & 0x6000 == 0x6000
	//    (read 0x80016BB4); op-70 cutscene-timer adoption gate 0x80016E58 (local
	//    +0x370 bit 0x2000) / 0x80016E64 (gp+0x7074==2) / store gp+0x7054 0x80016E80.
	//  - gp = 0x802C7820: gp+0x7054 (cutscene timer) = 0x802CE874, gp+0x7074
	//    (==2 full-link gate) = 0x802CE894.  localidx gp+0x7608 = 0x802CEE28.
	bool m_trace_adopt;
	bool m_adopt_have_prev;
	u32 m_adopt_prev_rec0;          // rec0 +0x370 last vblank
	u32 m_adopt_prev_rec1;          // rec1 +0x370 last vblank
	u32 m_adopt_prev_t7054;         // gp+0x7054 cutscene timer last vblank
	u32 m_adopt_prev_t7074;         // gp+0x7074 ==2 gate last vblank
	u32 m_adopt_prev_a0_active;     // actor-array slot 0 +0x6C (spawn landing)
	u32 m_adopt_prev_word01c6;      // link-chip RX RAM at the 0x01c6 ingest landing
	u32 m_adopt_max_t7054;          // 1/s window max cutscene timer
	u32 m_adopt_t7054_advances;     // 1/s window count of gp+0x7054 increases
	u32 m_adopt_p_pos_changes;      // 1/s window count of partner +0x370 low-24 changes (op02 ingest)
	u32 m_adopt_actor_changes;      // 1/s window count of actor slot 0 +0x6C changes (spawn ingest)
	u32 m_adopt_word01c6_changes;   // 1/s window count of link-RAM 0x01c6 changes (bulk landing)

	// P016 PHASE A EXPERIMENT (branch patch/op70-cutscene-timer-arm): READ-ONLY
	// op-70 cutscene-timer accept/reject proof.  P015 found gp+0x7054 = 0 the
	// entire run (op-70 timer channel dead) because op-70 adoption (RX handler
	// 0x80016E28, reached via op-table 0x8023F4D0[0x70]=0x800B2544) is DOUBLE-
	// gated on (LOCAL +0x370 bit 0x2000) AND (gp+0x7074 == 2), neither set on the
	// linked rig.  This tap proves, with evidence, whether op-70 frames ARRIVE at
	// blue during its 0x118002 cutscene freeze and are REJECTED at that gate, and
	// pins EXACTLY which predicate is false.  We watch gp+0x7054 for a change
	// (= the ROM store at 0x80016E80 fired = ACCEPT) and, every vblank + on every
	// op-70-relevant edge, sample the two gate operands the ROM checks:
	//   - LOCAL record +0x370 & 0x2000  (0x80016E50 reads 0x802F43D0 + localidx
	//     *0x474; localidx 0 -> rec0 0x802F43D0, 1 -> rec1 0x802F4844 - the cab's
	//     OWN record, NOT the partner) - gate operand 1 (0x80016E58 beqz)
	//   - gp+0x7074 == 2 (0x802CE894) - gate operand 2 (0x80016E64 bne)
	// Pair the DEVICE-side OP70: rx (arrival + peer value) with this driver-side
	// OP70: gate / accept / reject by t=.  Accept = gp+0x7054 moved toward the
	// peer value reported by the device line; reject = gp+0x7054 unchanged, with
	// the failing predicate named.  All reads READ-ONLY (m_mainram, no writes).
	// Addresses verified from full.txt (see 2026-06-16-p016 agent-log).
	// Experiment branch only - never merge to milestone.
	bool m_trace_op70;
	bool m_op70_have_prev;
	u32 m_op70_prev_t7054;          // gp+0x7054 last vblank (accept = it moved)
	u32 m_op70_t7054_changes;       // 1/s window count of gp+0x7054 changes (op-70 accepts)
	u32 m_op70_gate_local2000_set;  // 1/s window count of vblanks with LOCAL +0x370 bit 0x2000 set
	u32 m_op70_gate_7074eq2;        // 1/s window count of vblanks with gp+0x7074 == 2

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm, off
	// patch/op70-real-detector): READ-ONLY "linkbits" trace, env-gated by
	// NAMCOS23_TRACE_LINKBITS (inert when unset).  P018 proved (A) source-dead:
	// the op-70 emitter (0x80016DCC) AND the RX adopter (0x80016E64) are both
	// gated on gp+0x7074==2, which becomes 2 ONLY when the PARTNER record +0x370
	// has BOTH 0x6000 bits set (mask check 0x80016BB4-BCC reads
	// 0x802F43D0 + (1-localidx)*0x474 = the partner record), and partner6000=0
	// all run on both cabs.  STEP 1 localizes WHERE that 0x6000 chain breaks:
	//   (X) the LOCAL cab never even SETS its own +0x370 0x6000 bits, or
	//   (Y) the local 0x6000 bits are set but not on the cross-cab TX payload, or
	//   (Z) they are on the wire but the peer's op55 ingest doesn't land them.
	// This driver-side half answers (X) and pins the gate: it samples the LOCAL
	// record rec0 +0x370 (0x802F43D0) and the PARTNER record rec1 +0x370
	// (0x802F4844) 0x6000 bits every vblank, logs transitions, and emits a 1/s
	// LINKBITS: status pairing local0x6000 / partner0x6000 / gp+0x7074 and the
	// gate's WOULD-BE value (2 iff partner 0x6000 both set, else 1) so the whole
	// local-set -> TX -> ingest -> gate chain is one grep-able line.  The
	// device-side half (namco_c139 LINKBITS: txflags / rxflags) decodes the op55
	// 24-bit wire flags so (Y)/(Z) can be read off the wire.
	//
	// ROM provenance (full.txt, gp=0x802C7820):
	//  - LOCAL +0x370 bit 0x4000 set/clear: 0x80016DB8-DC0 (`|= 0x4000` when
	//    gp+0x705C(0x802CE87C)==0, else `&= ~0x4000`), $s0 = local record.
	//  - LOCAL +0x370 bit 0x2000 set: 0x800166B0-BC (`|= 0x2000` & `&= ~0x4000`),
	//    role-gated at 0x80016698-AC ((+0x370>>31 xor 1) == (gp+0x7608 != 0)).
	//  - PARTNER +0x370 written verbatim by op55 RX: 0x80013D44 -> sw 0x80013E10
	//    `$a2 = wire24flags | 0x40000000 | 0x80000000` (0x800B0718-38), record
	//    $t1 = 0x802F4060 + 1*0x474 = rec1 0x802F4844 (call $a0=1 at 0x800B0730);
	//    the partner 0x6000 bits therefore come straight from the op55 24-bit
	//    wire flags ($s2, parsed 0x800B05EC-0604 = operand bytes 4/5/6 = cell
	//    stream cells[op55+5..+7]); 0x6000 lives in cell[op55+6] & 0x60.
	//  - GATE: gp+0x7074(0x802CE894) = 2 iff partner +0x370 & 0x6000 == 0x6000
	//    (0x80016BB4-BCC, partner = 0x802F43D0 + (1-gp+0x7608)*0x474).
	// All reads READ-ONLY (m_mainram, no writes).  Experiment branch only -
	// never merge to milestone.
	bool m_trace_linkbits;
	bool m_lb_have_prev;
	u32 m_lb_prev_local;            // LOCAL rec +0x370 last vblank
	u32 m_lb_prev_partner;          // PARTNER rec +0x370 last vblank
	u32 m_lb_prev_t7074;            // gp+0x7074 last vblank
	u32 m_lb_prev_t705c;            // gp+0x705C (local-0x4000 gate predicate) last vblank
	u32 m_lb_local6000_set;         // 1/s window count of vblanks with LOCAL +0x370 0x6000 == 0x6000
	u32 m_lb_partner6000_set;       // 1/s window count of vblanks with PARTNER +0x370 0x6000 == 0x6000

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace, off
	// patch/op55-carrier-repeat): READ-ONLY trace of the op6F PLAY-CLOCK and the
	// bit-0x0004 CO-OP/HUMAN gate, env-gated by NAMCOS23_TRACE_PLAYCLOCK (inert
	// when unset).  The Fable-5 ROM-claims review retired the 0x6000/op-70 line
	// (death/continue subsystem) and named the REAL linked-play surface; this
	// trace instruments its two leads WITHOUT any RAM write.
	//
	// ROM provenance (full.txt, this pass; gp=0x802C7820):
	// (1) op6F PLAY-CLOCK.  Gate-4 table 0x8023F4D0[0x6F] (word @0x8023F68C) =
	//     RX handler 0x800B2448: parses TWO sign-extended 24-bit BE operands and,
	//     ONLY when role byte [0x802F3FFC] bit7 is CLEAR (bnez 0x800B24D0 skips
	//     the store), writes them to the receiver's OWN record +0x390/+0x394
	//     (0x800B2504-08, rec = 0x802F4060 + gp+0x7608*0x474 -> 0x802F43F0/F4) -
	//     an unconditional master->slave clock ADOPTION (no compare).  TX emitter
	//     0x800B22CC (sole caller 0x800B27CC inside the per-frame in-game TX
	//     cycle 0x800B27A8, right after the op02 emit): emits opcode 0x6F +
	//     local rec +0x390 low24 + +0x394 low24, gated on (a) lbu gp+0x74F3==0 =
	//     frame counter gp+0x74F0 low byte == 0 -> ONCE PER 256 FRAMES (~4.3 s),
	//     (b) keepalive 0x802F3FD8 > 0 (blez 0x800B22E0), (c) role byte
	//     [0x802F3FFC] bit7 SET (beqz 0x800B22F4).  So bit7 splits the cabs:
	//     master (bit7=1) emits, slave (bit7=0) adopts; if BOTH cabs are master
	//     (the default - every screen-mode init 0x8001E16C/0x8001E39C/0x8001E6C0
	//     ORs 0x80 in while hard-clearing the keepalive) NOBODY adopts, and if
	//     both are slave nobody emits.  Bit7 is CLEARED only by the staging call
	//     0x800B2F20 with request a0==2 (mode=2, [8]=0x10, andi 0x7F @0x800B2FD4;
	//     callers with li $a0,2: 0x8002075C/0x80020A38/0x80021080) and SET by
	//     a0==0/1 requests (ori 0x80 @0x800B2F9C/0x800B2FB8) - hash/role-sum
	//     failure downgrades any request to a0=0 = master.  The +0x390/+0x394
	//     pair itself: seeded -1 by the record initializer 0x80013D44
	//     (0x80013E30-34, $a0 clobbered li -1 @0x80013DBC), incremented +1/frame
	//     on the LOCAL record only in 0x80014CC0 (0x80014D3C-78; clamp 0x57E3F =
	//     359,999 frames; gates: gp+0x7570==0 (lh 0x80014CCC), +0x370 bit29
	//     CLEAR (0x80014D04-10), obj [rec+0x358] byte+9 != 0 && word+0x54 == -1);
	//     +0x394 alone is zeroed at 0x80104980 (segment clock); consumers: HUD
	//     time 0x800C90C0+ and the 0x80104xxx result cluster; exported to
	//     0x802F402C/30 when +0x370 bit1 set (0x80104420-44).
	// (2) bit 0x0004 CO-OP/HUMAN gate.  Consumer: 0x800136C4-D8 ORs BOTH records'
	//     +0x370 (0x802F43D0 | 0x802F4844) and tests 0x0004; set -> script-entity
	//     (6,1,0x1C) dispatch @0x80013720-2C (jal 0x80009530) = the co-op/human
	//     HUD element (H7).  PAIRED SETTER *AND* CLEARER = ONE script-VM opcode
	//     handler 0x800BE200 (table word @0x8023F910): reads a halfword operand
	//     from the script stream [ctx+0xC]; operand!=0 -> ORs 0x0004 into BOTH
	//     records (0x800BE21C-234, rec0 +0x370 AND rec0+0x7E4 = rec1 +0x370);
	//     operand==0 -> ANDs ~4 on BOTH (0x800BE238-254, li -5 @0x800BE210).  So
	//     set/clear is SCRIPT-driven (scene script), not link-state-driven.
	//     Other 0x04 clear paths: (i) whole-word record re-init 0x80013D44
	//     (sw $a2 @0x80013E10) - the round-start family passes 0x8000-constants
	//     WITHOUT 0x04, op55-RX passes wire-flags (0x04 survives only if on the
	//     wire, sourced from entity +0x102 & 0x4000 per gold TX builders
	//     0x800B37C8/0x800B3894); (ii) single-record reset helper 0x80013C54
	//     (andi ~4 @0x80013C70-78, also zeroes +0x384/+0x398; sole caller
	//     0x800A1080 passing an actor rec); (iii) the per-frame op02 status
	//     mirror REWRITES each record's low24 cross-cab (fieldA->rec1
	//     @0x800AB438, fieldB->rec0; top byte incl. b26/bit30/31 PRESERVED, low24
	//     incl. 0x04/0x2000/0x4000 REPLACED) - so a one-cab drop propagates to
	//     the peer within a frame, and bit04 stability requires BOTH cabs' 04
	//     set.  The COM-fallback 0x800B2A58-98 mask 0xBFFFDFFF does NOT touch
	//     0x04.  Edge-line fingerprints: BOTH records flip same vblank = script
	//     handler (the only paired site); whole-word discontinuity = re-init;
	//     single-record low24-only flip = op02 mirror (correlate with the peer
	//     log) or 0x80013C54.
	// All reads READ-ONLY (m_mainram + device counters).  Experiment branch only
	// - never merge to milestone.
	bool m_trace_playclock;
	bool m_pc_have_prev;
	u32 m_pc_prev_rec0_390;         // rec0 +0x390 (0x802F43F0) last vblank
	u32 m_pc_prev_rec0_394;         // rec0 +0x394 (0x802F43F4) last vblank
	u32 m_pc_prev_rec1_390;         // rec1 +0x390 (0x802F4864) last vblank
	u32 m_pc_prev_rec1_394;         // rec1 +0x394 (0x802F4868) last vblank
	u32 m_pc_prev_rec0_flags;       // rec0 +0x370 last vblank (bit04 edge detect)
	u32 m_pc_prev_rec1_flags;       // rec1 +0x370 last vblank (bit04 edge detect)
	u8  m_pc_prev_role3ffc;         // role/flags byte 0x802F3FFC last vblank (bit7 = op6F master)
	bool m_pc_prev_rec0_ticking;    // rec0 +0x390 advanced (+1/+2) last vblank (tick-run state)
	u32 m_pc_ticks390;              // 1/s window count of vblanks where rec0 +0x390 advanced
	u32 m_pc_bit04_both;            // 1/s window count of vblanks with BOTH records' bit 0x0004 set
	u32 m_pc_prev_op6f_tx;          // device op6f-tx running count at last 1/s status (delta basis)
	u32 m_pc_prev_op6f_rx;          // device op6f-rx running count at last 1/s status (delta basis)

	// P026 PART 2 EXPERIMENT (branch patch/reasm-chunk-passthru): READ-ONLY
	// phase9-VALIDATOR trace, env-gated by NAMCOS23_TRACE_PHASE9VAL (inert when
	// unset).  STEP-1 STATIC RESULT (full.txt, this pass; gp=0x802C7820) - the
	// semantics the project flip-flopped on, now nailed:
	//
	//   gp+0x75C8 (0x802CEDE8, the phase-9d tap's "succ") is a CHECKSUM-FAIL
	//   COUNTER, not a success counter.  Its ONLY writers: the increment at
	//   0x8000C004-0C (lhu/addiu 1/sh) inside the drain validator 0x8000BF38,
	//   reached ONLY when the byte-sum of a drained RX slot is non-zero - the
	//   compare is 0x8000BFF8 `andi $v0,$a3,0x00ff` + 0x8000BFFC `beqzl
	//   $v0,0x8000c05c` which JUMPS AROUND the increment to the dispatch `jal
	//   0x8000b8f0` when the sum is zero - and the zeroing at 0x8000C390 inside
	//   the link-layer reset 0x8000C368 (which also zeroes the C139 TX regs,
	//   both ring nibbles and the 75C2/C4/C6 counters).  Sole reader 0x8008DCFC
	//   just prints it on the link diagnostics screen.  There is NO success
	//   counter anywhere; "succ=0000 all run" therefore means ZERO CHECKSUM
	//   FAILURES (healthy or silent - disambiguated by the drift word).
	//
	//   gp+0x75BC (0x802CEDDC, "link") is a TRI-STATE last-outcome word:
	//   0 = last drained frame VALIDATED (sh $0 @0x8000C064, right after the
	//   jal 0x8000B8F0 dispatch), 1 = last drained frame CHKFAILED (sh $t1
	//   @0x8000C010, $t1=1 preloaded @0x8000BF6C), 2 = link TIMEOUT declared
	//   (sh @0x8000C050 in the drain's ring-empty tail and @0x8000BB44 in the
	//   standalone freshness tick 0x8000BB28-BB64; both fire when the drift
	//   word 0x802F3504 >= 0x11 and both also clear the freshness byte
	//   0x802F3502 - "gate3502").  Readers 0x8000C5D8 / 0x8000D714-D734 draw
	//   the three link-status strings on the test/diag screens.
	//
	//   The validated-frame path: scanner-enqueued ring entries {len@+0,
	//   offset@+2} of 6 bytes at 0x802F30A0[nibble] (write nibble gp+0x75BE,
	//   drain stop gp+0x75BA; drain walks newest (75BE-1)&0xF down to 75BA,
	//   0x8000BF44-54); per entry: len in [4..0x400] (0x8000BF88-94), cell 0 ==
	//   0x5A (0x8000BFA0-A8), then len-3 cells (low bytes of RX-window
	//   halfwords at 0xA6202000+2*off, source index wrapped & 0xFFF) are copied
	//   to the dispatch buffer 0x802F3510++ while byte-summed (0x8000BFCC-F4).
	//   Sum==0 -> jal 0x8000B8F0 (a0 = len-3): remote counter, body bytes ->
	//   0x802F3502/03, gp+0x77C8/D0/D4 link-state nibbles, drift 0x802F3504 =
	//   gp+0x75B8 - remote counter (0x8000BB10-1C).
	//
	//   CONSEQUENCE for the P024/P025 barrier reading: the checksum layer
	//   validates EVERY scanner-enqueued frame generically - it is NOT a
	//   session validator waiting on one specific exchange, and "link=0000
	//   succ=0000 all run" is the HEALTHY signature (no chkfail, last drain
	//   validated).  The arms-outstretched barrier's release must therefore be
	//   carried INSIDE validated traffic (a gate-4 VM op / op55 flag / record
	//   field) one layer up - which is what this tap makes visible: it logs
	//   the dispatch buffer head (the validator's INPUT bytes, rewritten per
	//   drained slot BEFORE the sum test, so failed drains show too) so a run
	//   with blue's session alive names exactly which ops flow - and which
	//   never arrive - while the barrier holds.
	//
	// Events (all change-driven, m_mainram reads only, no RAM writes):
	// 75BC/75C8 transitions, dispatch-buffer-head change (= a drain happened;
	// chkfail delta in the same vblank attributes pass/fail), + 1/s status.
	// Multiple drains within one vblank collapse into one event line
	// (documented limitation, same as the QUAL tap).
	bool m_trace_phase9val;
	bool m_p9v_have_prev;
	u16 m_p9v_prev_link;            // gp+0x75BC last vblank
	u16 m_p9v_prev_chkfail;         // gp+0x75C8 last vblank
	u32 m_p9v_prev_buf0;            // dispatch buffer 0x802F3510 word 0 last vblank
	u32 m_p9v_prev_buf1;            // dispatch buffer 0x802F3514 word 1 last vblank
	u32 m_p9v_win_drains;           // 1/s window count of buffer-head changes (drains observed)
	u32 m_p9v_win_chkfails;         // 1/s window sum of 75C8 deltas

	// P033 EXPERIMENT (branch patch/compose-gate-trace, off patch/announce-latch):
	// READ-ONLY trace of the ROM's boundary BULK-COMPOSE scheduler and its gates,
	// env-gated by NAMCOS23_TRACE_COMPOSEGATE (inert when unset).  P031 proved the
	// residual late area-skip is compose-ABSENT (zero bulk announces on EITHER cab
	// for one boundary, in a clean-health window) - upstream of the C139 device.
	// STEP-1 STATIC (full.txt, this pass; gp=0x802C7820) located the scheduler:
	//
	//   The ONLY op4B/4C bulk-snapshot TX emitter is 0x800AF84C (opcode 0x4C +
	//   0x158-byte entity copy + 0x18-byte ext block when flags&0x0E00, else
	//   opcode 0x4B + 0x158 bytes; cursor gp+0x77BC), and its ONLY caller is
	//   0x800150D8 inside the LINK PHASE MACHINE's phase-1 handler 0x80014F3C.
	//   The phase machine (jalr 0x8022A750[gp+0x7024=0x802CE844]) is run per
	//   frame by the boundary TRANSFER SCREENS: screen state gp+0x756C
	//   (0x802CED8C; P034 rider fixed a C<->E digit-swap typo here and in the
	//   sampler read) = 0x13 one-shot 0x8001541C -> 0x14 per-frame 0x80015550
	//   (phase machine ALWAYS), or 0x15 one-shot 0x800155EC -> 0x16 per-frame
	//   0x80015710 (phase machine ONLY when bypass gp+0x7036=0x802CE856 == 0,
	//   beqz 0x80015718; 0x7036 = $a0 of the 0x15-armer 0x80015B80: in-game
	//   caller 0x80012F5C passes 0, menu caller 0x800238E4 passes 1).  Screen
	//   0x13 is PRE-ARMED every running frame by the round state machine
	//   0x80016B3C (caller 0x800134A0) at 0x80016CC0.
	//
	//   Phase 0 (0x80014E50): HOLDs while role byte 0x802F3FFC bit5 (0x20) set
	//   @0x80014E7C-80 and countdown gp+0x7028 (0x802CE848, 0xF0 frames) alive;
	//   then ADVANCES to phase 1 @0x80014F08 - but iff mode 0x802F3FD0 != 2
	//   (@0x80014EA8) OR keepalive 0x802F3FD8 == 0 (@0x80014EB4) it FIRST tears
	//   the session down (mode<-1 @0x80014EC4, ka<-0, role bit0 clear, done
	//   latch gp+0x7014=0x802CE834 <- 1 @0x80014EE4) so phase 1 exits dead
	//   every frame (@0x80014F4C) and NOTHING is composed [candidate (a)].
	//   Phase 1 (0x80014F3C, THE BULK-COMPOSE SCHEDULER): serves ONE op4B/4C
	//   snapshot per frame for the entity id REQUESTED by the peer's op-0x1A
	//   record: RX 0x800AC8D0 -> 0x80015198 latches gp+0x7020=0x802CE840
	//   @0x800151D0 (requests ORIGINATE from the peer's per-frame TX cycle
	//   0x800B27A8 @0x800B289C-A4, emitted only for entities whose parallel
	//   word 0x802F4DC8[i*0x2B8]+0 has bit31 SET [candidate (c) ownership]).
	//   Per-frame gates: G1 done gp+0x7014!=0 @0x80014F4C -> dead frame (not
	//   even the TX cycle runs); G2 gp+0x7020==-1 @0x80014F88 -> no compose,
	//   idle streak gp+0x701C=0x802CE83C ++; G3 gp+0x7010=0x802CE830 ==0
	//   @0x80014F94 -> re-emit last snapshot (peer 0x1A echoed the outstanding
	//   id 0x802DFB42); G4 id lookup fails @0x80014FB0 / G5 entity +0xF2&0xE0
	//   @0x80014FE4 -> skip; serve marks entity |=0x40000000 @0x800150C8 and
	//   EMITS @0x800150D8.  TEARDOWN @0x8001511C-40 when watchdog gp+0x7018=
	//   0x802CE838 (300 frames) underflows @0x80015104 OR idle streak >= 0x20
	//   @0x80015110-14: mode<-1, ka<-0, role bit0 clear, gp+0x7014<-1 - so
	//   EVERY transfer ends with keepalive ZERO and mode 1; op55 must
	//   re-establish mode 2 before the next boundary [candidates (a)/(b)].
	//   The composed frame length is stored at 0x802F3910 by the finalizer
	//   0x800AA76C every frame (gate byte at 0x802F3D12: 4 = gameplay table
	//   when mode==2, 3 otherwise) - length > 0xFF = a bulk-class frame (the
	//   device-side expected_hw is this length + L2 overhead), the direct
	//   upstream analog of the C139 TXOFFSET bulk announce.  Area boundary
	//   marker = rec0/rec1 +0x370 (0x802F43D0/0x802F4844) low bits 0x40
	//   (marker, set @0x800131E8) / 0x20 (engaged, set @0x80013214/44, clear
	//   @0x8001324C) driven by the segment-phase machine [candidate (b)].
	// All reads READ-ONLY (m_mainram only, no writes to RAM/ROM/game state).
	// Experiment branch only - never merge to milestone.
	bool m_trace_composegate;
	bool m_cg_have_prev;
	u32 m_cg_prev_rec0_370;         // rec0 +0x370 (0x802F43D0) last vblank
	u32 m_cg_prev_rec1_370;         // rec1 +0x370 (0x802F4844) last vblank
	u32 m_cg_prev_screen;           // gp+0x756C screen state last vblank
	u32 m_cg_prev_phase;            // gp+0x7024 phase index last vblank
	u32 m_cg_prev_done;             // gp+0x7014 phase-machine done/abort latch last vblank
	s16 m_cg_prev_reqid;            // gp+0x7020 snapshot request id last vblank
	u16 m_cg_prev_len;              // 0x802F3910 composed VM frame length last vblank
	u32 m_cg_prev_mode;             // 0x802F3FD0 mode last vblank
	u32 m_cg_prev_ka;               // 0x802F3FD8 keepalive last vblank
	u8  m_cg_prev_role;             // role byte 0x802F3FFC last vblank
	u16 m_cg_prev_bypass;           // gp+0x7036 transfer-bypass selector last vblank
	u32 m_cg_win_bulkframes;        // 1/s window count of frames with len3910 > 0xFF
	u32 m_cg_win_reqframes;         // 1/s window count of frames with request id != -1
	u32 m_cg_win_phase1;            // 1/s window count of serving-capable frames (screen 0x14/0x16, phase 1, done==0)
	u16 m_cg_win_maxlen;            // 1/s window max of len3910

	// P034 EXPERIMENT (branch patch/roundend-trace, off patch/compose-gate-trace):
	// READ-ONLY trace of the ROUND-END trigger chain + join-armer preconditions,
	// env-gated by NAMCOS23_TRACE_ROUNDEND (inert when unset).  P033's run proved
	// the transfer screens / link phase machine NEVER executed (304 s, both cabs)
	// because red's round machine never left state 0 while blue walked 0->1->4.
	// STEP-1 STATIC (full.txt, this pass; gp=0x802C7820) named the whole chain:
	//
	//   round703c(gp+0x703C) 0->1 writer = 0x80017074 in the STATE-0 handler
	//   0x80017024 (dispatch table 0x8022A78C confirmed: [0]=0x80017024,
	//   [1]=0x800166D4, [2]=0x8001685C, [3]=0x80016938, [4]=0x80017090,
	//   [5]=0x800170BC) - UNCONDITIONAL once state 0 runs: it inits
	//   timer7054=0x4EB, 7058=1, gate705C=1, 7060=-1, zeroes 7048/704C/7050/
	//   7068/706C/7070/7078, increments 703C and tail-calls state 1.  The round
	//   machine 0x80016B3C (sole caller: per-frame dispatcher 0x800134A0
	//   @0x80013530) runs ONLY while LOCAL record +0x370 bit 0x2000 is set
	//   (test @0x80013520-28; LOCAL record = 0x802F4060 + gp+0x7608*0x474).
	//   Bit 0x2000's SOLE setter = 0x800166B0-BC (fn 0x80016698): +0x370 =
	//   (old|0x2000) & ~0x4000, and 703C/7040/7044 <- 0.  Its gate @0x800166AC
	//   admits only the record with bit31 == (gp+0x7608 != 0); gp+0x7608's only
	//   writers are sw $0 (6 re-init sites) + a 1-idx TOGGLE 0x8002A6B8-C8 on a
	//   menu input while keepalive==0 - in our sessions BOTH cabs stay idx 0,
	//   so only locally-owned RECORD 0 can enter round-end mode.  0x80016698's
	//   sole caller = the ROUND-END CHECKER 0x80026E00 (@0x80026EC4), gates:
	//   A +0x370 & 0x40010800 == 0 @0x80026E20; B +0x5C bit0 @0x80026E30
	//   (spawn-template flag, desc[+0x14] via 0x80031EE8/0x80032294 - no
	//   dynamic bit0 writer); C one-shot +0x37A bit0 clear @0x80026E40 (set
	//   @0x80026E48 the first passing call - a gate-E failure BURNS it until
	//   0x80014928 clears it); D the bit31/idx gate @0x80026E60; E countdown
	//   +0x3A0 pre-decrement <= 0 @0x80026EB8 (init byte gp+0x75E4 at record
	//   init 0x80013E4C / round result-1 0x80016C54; forced =1 by wrapper
	//   0x800D1668).  Checker callers: script triggers 0x800D5AC4/0x800DF324
	//   (float/position-gated), 0x800793CC/0x8007943C (+0x370 0x0840==0x40,
	//   both records), 0x80079ABC/0x80079BE8 + 0x800E9F34/0x800E9FC4 (0x40-
	//   gated, two hardcode record 1), wrappers 0x800D1624/0x800D1668, and the
	//   per-frame WATCHDOG 0x80014BA8 (called @0x80014D8C unless +0x370 0x2000
	//   already set): gates +0x5C bit0 && gp+0x75F8 bit2 clear && +0x3A2-- == 0
	//   (area TIME LIMIT: re-armed @0x80014DEC to desc-byte[9]*60 frames,
	//   default 0x960 = 40 s) && +0x37A bit1 clear (bit1 set by 0x80014B98
	//   from round result @0x80016C4C/0x80016CAC, cleared @0x80014DA4).
	//   Blue's 258.3 s entry timing fits the 40-s watchdog after boss intro.
	//   LOCAL 0x6000 advertise = 0x2000 (entry, above) + 0x4000 set by the
	//   round-machine tail 0x80016DB8-C0 when gate gp+0x705C == 0 (state-0
	//   sets 705C=1 @0x80017044; state-1 clears it @0x80016724 after counter
	//   gp+0x7068 exceeds 0x46 = 71 frames = 1.18 s - matches blue 258.31 ->
	//   259.9 advertise onset).  gp+0x7074 = 2 iff mode==2 && ka>0 && PARTNER
	//   record (1-idx) +0x370 & 0x6000 == 0x6000, derived at round-machine
	//   entry 0x80016B5C-BCC - it stays 0 while the round machine never runs.
	//   JOIN armer 0x80015B80 ($a0==0 in-game path): 7034==0 @0x80015B9C,
	//   role 0x802F3FFC bit1 @0x80015BAC-B0 (beqz -> EXIT) - role-byte writer
	//   census: 0x800130B4 andi 0xF7, 0x80014EE0 ori 0x01, 0x8001513C andi
	//   0xFE, 0x8001E174/0x8001E3A4/0x8001E6C8 ori 0x80 - NO PATH EVER SETS
	//   BIT1, so the in-game armer is statically dead (G10 named); then co-op
	//   (rec0|rec1 +0x370) & 0x04 @0x80015BC8-D0, entered gp+0x7034 <- 1, and
	//   gp+0x7624 != 0 gates only the final screen-0x15 arm @0x80015C10-14.
	//   SEGMENT-CLOCK RE-BASE: entry is 0x80104958 (0x80104980 is MID-function
	//   - that is why "jal 0x80104980" = 0 hits); it zeroes rec0+0x394/+0x388
	//   AND rec1+0x394/+0x388 (stores +0x394/+0x808/+0x7FC/+0x388 off the
	//   record-array base 0x802F4060) - all four zeroed in one frame is its
	//   fingerprint; callers 0x80104B8C/0x80104C34 (twin object handlers
	//   0x80104B14/0x80104BBC), pause-gated, firing once when the object's
	//   +0x18 word has bit27 (0x08000000), marking obj+8 |= 0x8000.
	//   op6F emitter 0x800B22CC (sole call site 0x800B27CC in the per-frame TX
	//   cycle 0x800B27A8, UNguarded): emit iff gp+0x74F3 == 0 (low byte of the
	//   global frame counter gp+0x74F0, incremented @0x800005EC -> once per
	//   256 frames) && keepalive 0x802F3FD8 > 0 && role bit7; payload = local
	//   record +0x390/+0x394 clock pair (3 bytes each) - no other gate term
	//   exists; the cadence byte IS the undocumented "extra" term.
	// All reads READ-ONLY (m_mainram only, no writes to RAM/ROM/game state).
	// Experiment branch only - never merge to milestone.
	bool m_trace_roundend;
	bool m_re_have_prev;
	u32 m_re_prev_p370[2];          // rec0/rec1 +0x370 (0x802F43D0 / 0x802F4844) last vblank
	u32 m_re_prev_5c[2];            // rec0/rec1 +0x5C (0x802F40BC / 0x802F4530) last vblank
	u16 m_re_prev_37a[2];           // rec0/rec1 +0x37A one-shot halfword last vblank
	s16 m_re_prev_3a0[2];           // rec0/rec1 +0x3A0 checker countdown last vblank
	s16 m_re_prev_3a2[2];           // rec0/rec1 +0x3A2 area time-limit watchdog last vblank
	u32 m_re_prev_394[2];           // rec0/rec1 +0x394 segment clock last vblank
	u32 m_re_prev_703c;             // gp+0x703C round state last vblank
	u32 m_re_prev_7040;             // gp+0x7040 round sub-state last vblank
	u32 m_re_prev_705c;             // gp+0x705C 0x4000-advertise gate last vblank
	u32 m_re_prev_7074;             // gp+0x7074 link status code last vblank
	u16 m_re_prev_7034;             // gp+0x7034 join entered latch last vblank
	u32 m_re_prev_7624;             // gp+0x7624 join arm-enable last vblank
	u8  m_re_prev_role;             // role byte 0x802F3FFC last vblank
	u32 m_re_prev_7608;             // gp+0x7608 local player index last vblank
	u32 m_re_prev_75f8;             // gp+0x75F8 global flags (bit2 watchdog block) last vblank
	u16 m_re_prev_6ff4;             // gp+0x6FF4 join-event countdown last vblank
	u8  m_re_prev_coop;             // (rec0|rec1 +0x370) bit2 co-op flag last vblank
	u8  m_re_prev_74f0lo;           // gp+0x74F0 low byte (op6F cadence) last vblank

	// P036 EXPERIMENT (branch patch/round-arm, off patch/latch-v2-snapshot):
	// ROUND-ARM ASSIST - the project's FIRST functional write to game work RAM
	// beyond the P001 keepalive floor.  Env-gated by NAMCOS23_PATCH_ROUNDARM
	// (inert unset).  P034 run 1 NAMED the round-end blocker: the checker
	// 0x80026E00 runs and passes gates A-D at real positional triggers on BOTH
	// cabs and fails ONLY gate E - the +0x3A0 trigger countdown (init byte
	// gp+0x75E4 = 4) that the desynced per-cab scripts never drain (red reached
	// 1, blue 2, at different areas, in 321.5 s).  Everything downstream is
	// proven healthy (blue's P033 walk 0->4, the 71-frame 0x6000 auto-arm, the
	// 1-frame one-shot self-heal), and BOTH cabs pass through the score/result
	// record state within ~0.5 s of each other (red rec0 05018040 @263.07 ->
	// 01008000 @263.82; blue 06118052-class @262.57) and BOTH make natural
	// checker calls within ~0.5 s of that state.  THE ASSIST: at the score
	// window exit, plant the ROM's OWN fire-now idiom - wrapper 0x800D1668
	// forces +0x3A0 <- 1 @0x800D16A8 before calling the checker - so the next
	// NATURAL checker call pre-decrements 1 -> 0, passes gate E, and 100% stock
	// ROM runs everything downstream (0x800166B0 entry -> round machine walk ->
	// 0x6000 advertise -> mutual 7074==2 -> transfer screens -> op4B/4C serves
	// -> segment re-base on both cabs).  NOT the P019-P024 forgery: no link or
	// round state is written - one countdown halfword is released.
	//   SIGNATURE (the score/result window; derived from the P034 run-1
	//   record-state walk, not hardcoded to the two observed words):
	//     OWN  rec0+0x370 (0x802F43D0): (w & 0xC48168E0) == 0x04010040
	//       required SET   0x04010040 = bit26 result-class modifier (the 05/06
	//         top-byte family vs plain 01/02 states) + bit16 (the result term
	//         whose clear edge is the window exit; also a checker gate-A bit)
	//         + 0x40 boundary marker;
	//       required CLEAR 0xC08068A0 = 0xC0000000 mirror marks (own-record
	//         sanity + gate-A bit30) + 0x00800000 bit23 strobe class
	//         (05808040/01808020) + 0x6000 advertise/round-end mode (never arm
	//         on a record already in/past round-end) + 0x800 yielded (gate A)
	//         + 0x80 trigger class (010080c0/050080d0/060080d2) + 0x20 engaged
	//         class (01008020/02008022).
	//     MIRROR rec1+0x370 (0x802F4844): (w & 0x048168E0) == 0x04010040 -
	//       same terms with the 0xC0000000 ingest marks ignored (mirrors carry
	//       them by design).  The MIRROR term is what makes the mask robust:
	//       blue's walking state 06018042 is a strict SUBSET of its score word
	//       06118052 (score = walking | 0x00100010), so NO own-record-only
	//       mask can separate them - but mid-stage the mirror shows the
	//       partner's fight states (c1008020-class), never the result class,
	//       so the MUTUAL signature only forms at a real stage boundary (both
	//       cabs hit the result state within ~0.5 s; mirrors track in real
	//       time per P034 run-1 section D).
	//     plus mode 0x802F3FD0 == 2 && keepalive 0x802F3FD8 > 0 &&
	//     gp+0x7608 == 0 (the record-0 ownership model everything rests on).
	//   FIRE on the rec0 bit16-clear edge (leaving the score/result state -
	//   bit16 is a checker gate-A term, so the checker can only pass AFTER
	//   this edge), iff ALL of: round703c gp+0x703C == 0, mode still 2, ka
	//   still > 0, one-shot rec0+0x37A == 0x0000 (bit0 burnt would eat the
	//   next checker call at gate C; bit1 blocks the watchdog backstop),
	//   post-edge word clean ((w & 0xC0006800) == 0, gate-A-shaped: mirror
	//   marks / 0x6000 / 0x800), and cnt3a0 in [2,15] (==1: already at the
	//   fire-now value, no write needed; <=0: checker already firing; >15:
	//   not a believable countdown state).
	//   THE WRITE (the only one, ever): rec0+0x3A0 <- 1 = hi halfword of the
	//   u32 at 0x802F4400; RMW preserves the +0x3A2 area watchdog in the lo
	//   halfword bit-exactly.  No other address is written on any path.
	//   RAILS: ONE-SHOT per score window - after fire/skip the window is
	//   consumed and re-arms only after the full signature has been ABSENT
	//   >= 60 consecutive frames and then re-enters (a fresh stage boundary;
	//   cnt3a0 legitimately re-inits to cfg75e4 at round result-1 0x80016C54,
	//   so every boundary needs its own assist).  Armed windows that never
	//   present the bit16-clear edge disarm after 300 frames (edge-timeout;
	//   protects against an unforeseen bit16-held exit path turning a stale
	//   arm into a mid-stage fire).  NON-RETRY: after a fire, if cnt3a0 rises
	//   above 1 while round703c is still 0, the ROM overwrote the write
	//   (record re-init) - logged ONCE, NEVER rewritten (no P025-style
	//   fight).  Logging: ROUNDARM: patch armed banner + per-event
	//   armed/fired/skipped/disarmed/entered/overwritten lines + 1/s
	//   ROUNDARM: status while mode == 2 only.
	// Experiment branch only - never merge to milestone.
	bool m_patch_roundarm;
	u32 m_ra_prev_p370;             // rec0 +0x370 last vblank (bit16 edge detection)
	bool m_ra_prev_own_sig;         // own-record signature last vblank (unarmed-window diagnostic)
	u32 m_ra_prev_703c;             // gp+0x703C last vblank (round-entry edge)
	bool m_ra_armed;                // signature latched, waiting for the bit16-clear edge
	u32 m_ra_armed_frame;           // frame the arm latched (edge-timeout base)
	bool m_ra_wait_reentry;         // window consumed - re-arm needs 60 sig-absent frames + re-entry
	u32 m_ra_gap_frames;            // consecutive full-signature-absent frames while waiting
	bool m_ra_fire_pending;         // fired, watching entered-vs-overwritten (non-retry rail)
	u32 m_ra_fire_frame;            // frame of the (last) fire
	u32 m_ra_cnt_armed;             // total window arms
	u32 m_ra_cnt_fired;             // total rec0+0x3A0 <- 1 writes
	u32 m_ra_cnt_entered;           // total round703c 0->nonzero entries seen (any source)
	u32 m_ra_cnt_skipped;           // total skips (fence fail / no-write-needed / unarmed window / disarm / pending-clear)
	u32 m_ra_cnt_overwritten;       // total post-fire overwrites detected (never retried)

	// P020 EXPERIMENT (branch patch/linked-gate-supply, off
	// patch/op70-linked-gate-arm): FORCING supply of the LOCAL "fully-linked"
	// 0x6000 bits, env-gated by NAMCOS23_PATCH_LINKED_GATE (inert when unset ->
	// behaviour byte-identical to before).  P019 proved (X) LOCAL-NEVER-SETS,
	// symmetric on both cabs: neither cab ever sets its own +0x370 0x6000
	// (local6000=0 all run), so has6000 is never on the wire, partner6000 stays
	// 0, gp+0x7074 never reaches 2, and op-70 (cutscene-timer sync) never runs.
	// The 0x4000 micro-gate (gp+0x705C==0, 0x80016DBC) is ALREADY satisfied yet
	// 0x4000 never sets => the set-code tail (0x80016DA0) is simply never
	// reached; the real root is one level up at the gp+0x7040 link-state that
	// gates entry to the "advertise-fully-linked + emit-op70" tail.
	//
	// This patch SUPPLIES the bit the ROM should set but never does: each vblank
	// while linked gameplay is staged (mode word 0x802F3FD0 == 2 - the SAME gate
	// the KEEPALIVE_FLOOR / CONTINUOUS_ARM blocks use), OR 0x6000 into the LOCAL
	// record +0x370 word (read-modify-write, write back only when it changes).
	// It is a SUPPLY, NOT a gate/partner fake: it does NOT write gp+0x7074, does
	// NOT touch the partner record, and does NOT synthesize op-70.  The whole
	// point is that the ROM's own op55 builder then serializes this LOCAL +0x370
	// onto the wire (the low24 of this word already mirrors cross-cab, proven by
	// P019 flags24=8001bc/800211), the PEER's op55 RX ingests it into its partner
	// record (0x80013E10), the peer's gate (0x80016BB4-BCC) sees partner 0x6000
	// and sets gp+0x7074==2, and op-70 flows NATURALLY off this one supplied bit
	// - a test that the real downstream chain works.  Runs on BOTH cabs (same
	// binary, env set on both); each cab supplies its OWN 0x6000.  The LOCAL
	// record base is resolved EXACTLY as the LINKBITS trace / KEEPALIVE_FLOOR
	// blocks do (localidx = gp+0x7608 = 0x802CEE28; local = 0x802F43D0 +
	// localidx*0x474), so it stays correct if localidx is ever != 0.
	//
	// SAFETY: OR-only (never clears bits), env-gated (unset == stock), bounded
	// to mode==2; no device/timing changes.  CAVEAT: this is a forcing SHORTCUT -
	// if the ROM's op55 builder only serializes +0x370 under conditions that ALSO
	// require the gp+0x7040 link-state, the supplied bit may not reach the wire
	// (first thing to check: device LINKBITS: txflags has6000).  Experiment
	// branch only - never merge to milestone.
	bool m_patch_linked_gate;
	u32 m_lg_set_count;             // total LOCAL +0x370 0x6000 supply writes (0->set transitions)

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off
	// patch/linked-gate-supply): WIRE-ONLY 0x6000 advertise, env-gated by
	// NAMCOS23_PATCH_LINKED_GATE_TXONLY (inert when unset; INDEPENDENT of the P020
	// NAMCOS23_PATCH_LINKED_GATE gate - the user runs with TXONLY set and
	// LINKED_GATE UNSET).  P020 proved the FORCING supply drives the full handshake
	// to gp+0x7074==2 on the PEER (genuine cross-cab sync, 0x118002 freeze broken)
	// but ALSO poisons this cab's LIVE +0x370 record: the gun-actor (0x80013644) /
	// own-record tick (0x80014CC0) read bit 0x2000 on the SAME word and treat the
	// local player as remote-driven -> the synchronized continue/death derail.
	// The +0x370 word is DOUBLE-DUTY (the LOCAL live state the death/gun path reads
	// AND the value advertised to the peer that drives the peer's gate7074==2).
	// P021 separates them: instead of OR-ing 0x6000 into the live LOCAL record, it
	// advertises 0x6000 ON THE WIRE ONLY.  The driver cannot reach the outgoing
	// frame copy (that lives in the device), and the device cannot read the
	// staging mode word (0x802F3FD0) or the env var (only the driver maps
	// m_mainram) - so the driver computes (armed && mode==2) each vblank and pushes
	// it to the device (set_linked_gate_txonly), which ORs 0x6000 into the op55
	// 24-bit wire flags IN THE OUTGOING FRAME COPY (emit_tx_frame's payload, a copy
	// read out of the C422 link RAM - NOT game RAM).  INVARIANT: the LOCAL game RAM
	// +0x370 (0x802F43D0) is NEVER written by this patch, so the gun-actor never
	// sees 0x2000 -> no death derail -> local player stays human.  This driver does
	// NO RAM write at all here; it only reads the mode word and pushes the gate.
	// SAFETY: env-gated (unset == stock), bounded to mode==2 (the gate is pushed
	// false otherwise, so the device injects nothing), OR-only on the wire.
	// Experiment branch only - never merge to milestone.
	bool m_patch_linked_gate_txonly;

	// P024 EXPERIMENT (branch patch/op55-carrier-repeat, off
	// patch/linked-gate-tx-only): PARTNER-RECORD STICKY LATCH, env-gated by
	// NAMCOS23_PATCH_OP55_REPEAT (inert when unset; STACKS on top of - and is
	// INDEPENDENT of - the P021 TXONLY wire advertise, which stays armed).
	// P021 PROVED the wire-only op55 0x6000 advertise lands in the PARTNER record
	// (rec1 +0x370 = 0x802F4844 -> &0x6000) and lit the FIRST b26 (0x04000000)
	// partner render - but op55 is emitted only ~once per scene, so the advertise
	// (and the b26 render it lights) DOES NOT PERSIST: the partner +0x370 0x6000 is
	// set sparsely by op55 RX then cleared again between op55 frames (the per-frame
	// op02 coord ingest 0x800AB3B8 rewriting the word, or a ROM clear).  P024 makes
	// the advertise PERSIST: each vblank while linked gameplay is staged
	// (mode word 0x802F3FD0 == 2), OR 0x6000 into the PARTNER record +0x370 word so
	// the gate (0x80016BB4-BCC reads PARTNER 0x6000, mask 0x6000 needs BOTH bits)
	// sees persistent 0x6000 -> gp+0x7074 latches 1->2 and the b26 partner render
	// STAYS drawn (the concrete visible win: partner stays rendered as a 2P human
	// peer), and we can observe whether sustained gate7074==2 finally flows op-70.
	//
	// PARTNER-0x2000 SAFETY (the decisive pre-step, from gold disasm): the three
	// sites that derailed P020/P023a all read bit 0x2000 on the LOCAL record only:
	//   - gun-actor 0x80013644 (0x80013690 reads 0x802F4060 + gp+0x7608*0x474, the
	//     LOCAL index) -> 0x2000 set takes the "ready"/skip-local-input branch.
	//   - own-record visibility tick 0x80014CC0 (0x80014D7C reads the SAME
	//     gp+0x7608-indexed LOCAL record) -> 0x2000 "hidden" bit bails.
	//   - the per-actor dispatcher 0x800134A0 (step 6/9) reads rec[0x370] & 0x2000
	//     on $s1 = base + gp+0x7608*0x474 = the LOCAL record to route local-vs-link;
	//     it does NOT consult the partner (1-idx) record for that decision.
	// NONE of them reads 0x2000 on the PARTNER record (1-localidx).  On the PARTNER
	// record, 0x6000 = exactly the ROM-intended "remote networked partner is fully
	// linked" state: the op55 RX path (0x800B058C -> 0x80013D44 sw 0x80013E10)
	// deliberately writes the partner record with 0x4000|0x8000 from the wire, and
	// the only consumer of partner 0x6000 is the gate (0x80016BB4).  So OR-ing
	// 0x6000 (BOTH bits - the gate masks 0x6000 and needs both) into the PARTNER
	// record is SAFE and is what makes it render as a 2P human peer.
	//
	// INVARIANT: we write ONLY the PARTNER record (0x802F43D0 + (1-localidx)*0x474),
	// NEVER the LOCAL record (rec0 when localidx==0).  The local gun stays human; no
	// death/black-screen derail (the structural cause of the P020 derail - local
	// 0x2000 - is never created).  Mechanism (A) "partner sticky latch" was chosen
	// over (B) "op55 TX repeat" because the pre-step proved partner 0x2000 safe, so
	// the direct driver-side OR is the simplest, lowest-blast-radius path AND avoids
	// re-emitting/restamping op55 frames (the P009 replay-reject pitfall - never
	// touch op02 coordinates or the 0x5A cell-0 marker).  This block runs in vblank
	// AFTER the ROM has executed the frame (and after any within-frame op02 clear of
	// the word), so the OR persists into the next frame's gate read.  OR-only,
	// mode-2-bounded, idempotent (writes back only when 0x6000 not already both set).
	// Experiment branch only - never merge to milestone.
	bool m_patch_op55_repeat;
	u32 m_or_partner6000_count;     // total PARTNER +0x370 0x6000 latch writes (not-both -> both)

	// P040 EXPERIMENT (branch patch/op6f-394-clamp, off patch/latch-v3-dedupe):
	// op6F SEGMENT-CLOCK ADOPTION CLAMP - a driver-side CONDITIONAL STORE FILTER,
	// env-gated by NAMCOS23_PATCH_OP6F_NO394 (inert unset).  No instruction is
	// modified, no game code / game RAM is written: the patch only PREVENTS one
	// ROM store from landing.
	//
	// MECHANISM SUPPRESSED (P039 static RE + P038 run, confirmed live): the op6F
	// clock-adoption receiver 0x800B2448 (RX dispatch 0x8023F4D0[0x6F] @0x8023F68C)
	// parses the master's (390,394) clock pair and, ONLY when role byte 0x802F3FFC
	// bit7 is CLEAR (slave; the bnez @0x800B24D0 skips both stores on the master),
	// writes the LOCAL record: +0x390 <- sender's 390 @0x800B2504 (sw $a3,0x390($v0),
	// word 0xAC470390) and +0x394 <- sender's 394 @0x800B2508 (sw $a2,0x394($v0),
	// word 0xAC460394).  The master (red) NEVER re-bases in our linked sessions
	// (its round-end/handoff trigger chain stays dry), so its (390,394) pair is
	// EQUAL and stale - the first delivered op6F after the slave's 4-cell re-base
	// (0x80104958) overwrites the fresh segment clock with the stale pair
	// (P038: blue re-base @244.27 poisoned @253.388, rec0_394 +9286 landing
	// ==rec0_390 EXACT; P033 +10.9 s; P036 +11.5 s; P034 never in 57.75 s - a
	// once-per-256-frames emitter x lossy delivery lottery).  The play-clock half
	// (+0x390 @0x800B2504) is BENIGN (both cabs' play clocks are wall-synced by
	// the seeded tick + lockstep; P039 verified its consumers) and is NOT touched.
	//
	// THE FILTER: a memory-system write tap on the rec0 segment-clock word
	// +0x394 = virtual 0x802F43F4 -> PROGRAM-space address 0x002F43F4 (the space
	// applies map.global_mask(0xfffffff); the R4650 core passes raw kseg0
	// addresses through - see the accessor notes below).  When a 32-bit store to
	// that word arrives from a PC inside the adoption receiver [0x800B2448,
	// 0x800B2520] (function body actually ends @0x800B2510 jr+slot; the wider
	// task-specified range is harmless because no other instruction in it stores
	// to this word), the tap rewrites the store DATA to the word's current value
	// - the store still "lands" but changes nothing = dropped.  Every other
	// writer passes untouched: per-frame tick +1 (0x80014D64/78), re-base zero
	// (0x80104980), record-init -1 (0x80013E34), Handler-A saved-pair restore
	// (0x801046A0), session-sync ingest (0x800B08F4).
	//
	// PC-AT-STORE-TIME RELIABILITY (the core facts this rests on; all verified
	// against this tree's src/devices/cpu/mips/*):
	//  - namcos23 runs an R4650 (r4650be_device).  Its INTERPRETER overrides
	//    RBYTE..WDOUBLE and in kernel mode passes the RAW virtual address
	//    straight to m_program->write_dword() (mips3.cpp 1673-81) - it never
	//    consults the add_fastram() ranges (those compare PHYSICAL addresses and
	//    are only reached by the base-class TLB cores' accessors + the DRC
	//    accessor), so the write tap ALWAYS fires for kseg0 stores.
	//  - mips3 exports NO exact-instruction-address state: MIPS3_PC ==
	//    STATE_GENPC (mips3.h:61) and STATE_GENPCBASE ("CURPC", mips3.cpp:739)
	//    BOTH map to m_core->pc; m_ppc (the true instruction base) is not
	//    exported, so pc() and pcbase() always read the same variable.  The
	//    interpreter advances m_core->pc BEFORE executing the op body
	//    (execute_run: pc += 4, then dispatch), so at store time pc() reads
	//    store-PC+4 = 0x800B250C - the task-anticipated "pc() at store time is
	//    offset" case, verified statically here.  The store is NOT in a branch
	//    delay slot (predecessor @0x800B2504 is the 390 sw; the jr $ra follows
	//    @0x800B250C), so no branch-target aliasing is possible.  The filter
	//    therefore matches pc() against the containing-function RANGE
	//    [0x800B2448, 0x800B2520] (not a single PC) - which absorbs the +4
	//    offset; the blocked line logs the observed pc so run 1 empirically
	//    confirms the compare value (expect exactly 0x800B250C).
	//  - Under the DRC (MAME default -drc; this fork's launcher passes no
	//    -nodrc) the generated code updates m_core->pc only at block exits /
	//    exceptions / debugger hooks (mips3drc.cpp generate_sequence_instruction),
	//    so BOTH pc() and pcbase() are STALE inside a mid-block store - the
	//    filter would silently fail open.  A value-pattern fallback was
	//    considered and REJECTED (orchestrator call: prefer the PC range +
	//    document).  Resolution: when the env gate is armed, the timecrs2
	//    machine config calls m_maincpu->set_force_no_drc(true) (the same
	//    driver-side switch atvtrack/aristmk6 use), forcing the INTERPRETER,
	//    whose per-instruction pc step makes the tap-time pc deterministic
	//    (store-PC+4, absorbed by the range).  This is the one behavioral
	//    rider of the patch: an armed run executes the main CPU interpreted
	//    (slower; VBLANK_LOCKSTEP bounds cross-cab drift, and a sub-60fps run
	//    shows up immediately in the lockstep stall counters).  Unset = stock
	//    DRC, bit-exact stock behavior.
	//  - The tap writes NO memory (it only edits the in-flight data by
	//    reference) -> no recursion; it is installed once at machine_start and
	//    persists across soft reset and save-state load (taps live in the
	//    memory system, not in save data; the memory_passthrough_handler member
	//    only keeps the handle alive).  Debugger pokes are passed through
	//    untouched (side_effects_disabled() guard).
	//
	// SCOPE / LIMITS: rec0 only (0x802F43F4) - gp+0x7608 localidx is 0 on BOTH
	// cabs in every traced run (ROUNDEND static census: only sw $0 writers + a
	// dead menu toggle), matching every other patch/tap in this file; if a
	// future ROM path adopted into rec1 (+0x394 @0x802F4868) the filter would
	// not see it - the adopt-edge falsifier below would.  Applied to BOTH cabs;
	// structurally inert on the master (role bit7 set skips the stores at
	// 0x800B24D0, so nothing from the adoption PC range ever reaches the word -
	// expected blocked count on red = 0).
	//
	// THE FALSIFIER (read-only, armed by the same env): a per-vblank adopt-edge
	// trace - rec0_394 jumping by >2 between vblank samples AND landing within
	// +/-3 of rec0_390 is the adoption fingerprint (P038 would have fired it
	// exactly twice on blue: the +8 micro-adopt @95.27 and the +9286 poison
	// @253.388, and never on red).  With the filter armed it MUST stay 0; any
	// hit = a second adoption path the RE missed (or a Handler-A saved-pair
	// restore whose saved pair happens to sit within +/-3 of the running play
	// clock - result-screen windows only; cross-check ROUNDEND lines by t=).
	// Experiment branch only - never merge to milestone.
	bool m_patch_op6f_no394;
	memory_passthrough_handler m_op6f_no394_tap;
	u32 m_no394_blocked;            // total adoption stores dropped (expected: red 0; blue = every DELIVERED op6F, incl. former equal-pair no-ops -> doubles as the true delivery-rate instrument)
	u32 m_no394_blocked_win;        // 1/s window count of drops
	u32 m_no394_pass394;            // total non-adoption +0x394 stores passed untouched (tick/re-base/init/restore)
	u32 m_no394_pass394_win;        // 1/s window count of passes
	u32 m_no394_adopt_edges;        // total adopt-edge falsifier hits (MUST stay 0 while armed)
	bool m_no394_have_prev;
	u32 m_no394_prev_394;           // rec0 +0x394 last vblank (adopt-edge jump basis)

	// P040b EXPERIMENT (branch patch/op6f-394-clamp - a MECHANISM AMENDMENT of
	// P040 on the SAME branch, not a new experiment): the SAME op6F
	// 394-adoption suppression, swapped from the v1 conditional store filter
	// to a VERIFY-BEFORE-POKE RAM-CODE NOP that runs under the DRC at FULL
	// SPEED - env NAMCOS23_PATCH_OP6F_NO394B (inert unset; independent of the
	// v1 env above, which stays functional).
	//
	// WHY THE SWAP (P040 run 1, 2026-07-07): the filter is PROVEN at the
	// store level (blue blocked=44, every line pc=0x800B250C, red 0,
	// adopt-edge 0, tap alive) but its only sound implementation keys on
	// pc() inside a write tap, and mips3's pc is block-stale under the DRC -
	// so the armed config forced the INTERPRETER (set_force_no_drc), costing
	// 1.56x wall speed (38.3 fps).  The run therefore ended 17 s short of
	// P038's 253.4 s poison window and the end-to-end proof (a LIVE poison
	// blocked; blue's re-base surviving >30 s) was never reached.  NOPping
	// the store INSTRUCTION suppresses the same store with no per-store PC
	// check and no interpreter.
	//
	// THE POKE: the System 23 main program executes from main RAM (the boot
	// ROM copies it there in early boot; at machine_start the target word
	// still reads 0).  Each armed vblank until terminal, read the u32 at
	// virtual 0x800B2508 (program-space 0x000b2508 under global_mask) via
	// the CPU's program space:
	//   0x00000000 -> program not copied yet: keep waiting (a zero word is
	//                 already a NOP - the store cannot execute meanwhile);
	//   0xAC460394 -> the P039-named adoption store (sw $a2,0x394($v0), the
	//                 sole +0x394 store of receiver 0x800B2448): write
	//                 0x00000000 (MIPS NOP: sll $0,$0,0), one-shot banner;
	//   anything else -> ROM-REVISION GUARD: one-shot REFUSED banner, stay
	//                 inert (never NOP an unverified instruction).
	// The +0x390 play-clock adoption @0x800B2504 (word 0xAC470390) stays
	// untouched - same scope as v1.
	//
	// DRC INVALIDATION - VERIFIED against this tree, not assumed:
	//  - mips3's DRC has NO store-triggered invalidation: a RAM write (via
	//    the address space or otherwise) does NOT notify the DRC.  Compiled
	//    code drops only on a FULL cache flush (code_flush_cache, driven by
	//    m_drc_cache_dirty - set at device_start mips3.cpp:530 and
	//    device_reset mips3.cpp:1149 - plus compile aborts; the method is
	//    not driver-callable).
	//  - Block-entry checksum validation (generate_checksum_block,
	//    mips3drc.cpp:1143): namcos23 never calls mips3drc_set_options, so
	//    m_drcoptions == 0 = LOOSE verify - each compiled sequence validates
	//    ONLY its head opcode (+ that op's delay slot) at entry.  Our word
	//    is MID-sequence (its predecessor is the 390 sw; nothing branches to
	//    it), so an already-compiled block would NOT notice the poke.
	//  - WHY THE POKE IS SOUND ANYWAY - ORDERING: the DRC compiles lazily on
	//    first execution.  The receiver 0x800B2448 is reached only through
	//    the RX dispatch table 0x8023F4D0[0x6F] (jalr - dynamic, never
	//    statically followed into another block's describe; the preceding
	//    function ends in jr $ra, which terminates describe), and op6F
	//    frames flow only once the link session is up (~56 s+).  The poke
	//    lands in the first seconds of boot, as soon as the loader places
	//    the word - so the store's block is ALWAYS compiled from the
	//    already-NOPed RAM (describe + validation read through m_prptr host
	//    pointers into the live RAM share).  The interpreter (-nodrc)
	//    re-fetches every instruction from RAM and is trivially safe.
	//  - RESIDUAL WINDOWS, guarded not hand-waved: (a) a loader re-copy
	//    (incl. the soft-reset re-boot) re-materializes 0xAC460394 -> the
	//    1/s guard in the status block re-reads the word and RE-POKES on
	//    observation (logged + counted; soft reset also flushes the whole
	//    DRC cache via device_reset, so no stale block survives that path);
	//    (b) a block compiled inside the sub-second window between a
	//    re-copy and the guard's re-poke would embed the ORIGINAL store and
	//    loose verify would not catch the subsequent re-poke - the
	//    adopt-edge falsifier (kept VERBATIM from v1: read-only, pc-free,
	//    DRC-safe) is the backstop that exposes any executed adoption.
	//    Expected in practice: repokes = 0.
	//
	// Shares the v1 falsifier + 1/s status blocks (gate widened to either
	// env); under NO394B-only the tap counters legitimately read 0 (no tap
	// is installed) and the OP6F_NO394B status line's word2508 field is the
	// aliveness rail instead (expect 00000000 from the poke onward).  Both
	// cabs run the same env; structurally inert on the master (its role
	// gate skips the stores anyway - the NOP just makes that unconditional).
	// NO set_force_no_drc, NO write tap.  Counters diagnostics-only, not
	// save_item'd (any save/load state converges: word==original -> poke or
	// re-poke path; word==0 -> already the wanted NOP).  Experiment branch
	// only - never merge to milestone.
	bool m_patch_op6f_no394b;
	bool m_no394b_poked;            // one-shot: the NOP is in RAM
	bool m_no394b_refused;          // one-shot: ROM-revision guard tripped (unexpected nonzero word) - inert
	u32 m_no394b_repokes;           // guard re-pokes after re-materialization (expect 0)

	// P041 EXPERIMENT (branch patch/op6f-390-clamp, off patch/op6f-394-clamp):
	// NOP the op6F +0x390 PLAY-CLOCK adoption store too - the OTHER HALF of
	// the pair P040b left live - env NAMCOS23_PATCH_OP6F_NO390B (inert unset;
	// independent of both envs above).  Structurally identical to P040b: a
	// VERIFY-BEFORE-POKE RAM-CODE NOP under the full-speed DRC.
	//
	// WHY (P040b runs 1+2, 2026-07-07): with ONLY the 394 store NOPed,
	// delivered op6F frames still adopt red's +0x390 play clock on the slave
	// - blue's 390-394 skew sawtoothed (+1/-7/+1/-6/-8/+1/-9), six +3..+12
	// forward play-clock snaps + 11 op6F-grid tick-stops in run 2, while
	// red's pair never split and red never jittered.  That HALF-ADOPTION
	// PAIR-SPLIT is the measured owner of blue's new gameplay
	// jitter/rubberbanding: P039's consumer census (total-time scoring,
	// Handler-B draw, best-flags) reads the LOCAL pair, so pair CONSISTENCY
	// matters more than cross-cab re-sync.  Blocking BOTH halves restores
	// stock-shaped pair consistency (both cells tick-driven only); accepted
	// cost = bounded cross-cab play-clock skew from blue's ingest-pause tick
	// losses (measured +3..+12 frames per ~30-45 s ~= <1 s per 5 min; total-
	// time scoring bias <1 s per round - falsify at the score screen when
	// mutual round-end finally arms).
	//
	// THE POKE: word @virtual 0x800B2504 (program-space 0x000b2504 under
	// global_mask), VERIFIED in full.txt before hardcoding:
	//   800B2504: AC470390 sw $a3,0x390($v0)
	// = the sole +0x390 store of the op6F adoption receiver 0x800B2448, one
	// instruction BEFORE the P040b-NOPed 394 store @0x800B2508.  Both sit
	// behind the same role gate (bnez @0x800B24D0 - the master skips them
	// anyway, so the env is structurally inert on red), nothing branches to
	// either store (mid-sequence words), and the jr $ra @0x800B250C + its
	// delay slot (move $v0,$a1 @0x800B2510) are untouched - with both stores
	// NOPed the receiver still parses the two 3-byte clock values into
	// $a3/$a2 and returns the advanced pointer exactly as before; it just
	// stores nothing.  Same states as P040b: word 0 = boot loader has not
	// copied the program yet, keep waiting (a zero word is already a NOP);
	// 0xAC470390 = poke 0x00000000 (MIPS NOP), one-shot banner; any OTHER
	// nonzero = one-shot REFUSED (ROM-revision guard - never NOP an
	// unverified instruction).  The DRC soundness argument is P040b's,
	// verbatim (see above): no store-triggered invalidation + loose verify,
	// but the poke lands ~frame 11 and the receiver first executes at
	// link-up ~32-56 s, so its block is ALWAYS compiled from already-NOPed
	// RAM; the 1/s re-check/re-poke guard + the falsifier below cover the
	// re-copy windows.
	//
	// THE FALSIFIER (this block's own; the 394 rails above stay UNTOUCHED):
	// a per-vblank rec0+0x390 jump census - any POSITIVE jump > +2 is the
	// adoption fingerprint (measured landings +3..+12, always red's residue
	// value; normal ticks are +1/+2; boot/seed re-inits are -1-class or
	// negative; the segment re-base zeroes 394, NOT 390).  With both pokes
	// armed it MUST stay 0 on both cabs; a hit = the adoption leaked via a
	// path the NOP does not cover (or a Handler-A saved-pair restore -
	// result-screen windows only, cross-check ROUNDEND lines by t=).  The
	// recipe's NAMCOS23_TRACE_PLAYCLOCK "clock-jump rec0_390" line (delta
	// outside 0..2, negatives included) stays available as the independent
	// cross-check.  Counters diagnostics-only, not save_item'd (any
	// save/load state converges exactly as P040b's does).  Experiment branch
	// only - never merge to milestone.
	bool m_patch_op6f_no390b;
	bool m_no390b_poked;            // one-shot: the NOP is in RAM
	bool m_no390b_refused;          // one-shot: ROM-revision guard tripped (unexpected nonzero word) - inert
	u32 m_no390b_repokes;           // guard re-pokes after re-materialization (expect 0)
	u32 m_no390b_jump390s;          // falsifier: positive in-game rec0+0x390 jumps > +2 (MUST stay 0 while armed)
	bool m_no390b_have_prev;
	u32 m_no390b_prev_390;          // rec0 +0x390 last vblank (jump-census basis)

	// P043 EXPERIMENT (branch patch/wave-init-hold, off patch/op6f-390-clamp):
	// close the WAVE-INIT COMPLETION RACE that P042's static RE named end-to-
	// end - env NAMCOS23_PATCH_WAVEINIT_HOLD (inert unset; independent of every
	// env above).  TWO verify-before-poke RAM-code WORD patches (the proven
	// P040b/P041 mechanism - deferred to vblank, one-shot, ROM-revision REFUSE,
	// 1/s re-check/re-poke guard - but each poked word here is a changed
	// IMMEDIATE, not a NOP).
	//
	// THE RACE (P042, all PC-anchored in full.txt): the per-frame area-
	// completion check 0x80014074 (own records only, p370 bit30 gate) has a
	// condition B that looks up the segment's WAVE-ANCHOR entity id
	// (rec+0x364 = desc[0x14]) in the 4-slot (id,handle) ring @0x802D2030
	// (lookup leaf 0x800B5690, SOLE caller = the jal @0x8001418C): any
	// NEGATIVE verdict (-1 no-id / -2 never-registered / released handle -1)
	// sets p370 |= 0x110000 (bit16, NO bit17 - the logged skip signature
	// 01008020->01018020) and the area is COMPLETE.  During the walk (f5c =
	// rec+0x5C bit0 clear) the ROM holds condition B until the LAST 10 FRAMES
	// of the intro timeline - `slti $v0,$v0,0xB` @0x8001417C on
	// end-cursor = rec[0x6A]-rec[0x68] - but the wave only registers/activates
	// AT the timeline end (per-frame wave tick 0x800A113C -> activation
	// 0x800A0E5C sets +0x5C bit0 at cursor==end-1), so EVERY fight area runs a
	// 10-frame (0.167 s = the measured 0.17 s skip margin, exactly) window in
	// which "is the wave dead?" is asked before the wave exists.  The anchor's
	// ring slot is written ONLY by descriptor commits (mirror commits are
	// op02-ingest-paced), op4B/4C snapshot-ingest creates, and op-0x20 release
	// transactions - so at the 150 ms HB-floor exchange regime the seed-time
	// link-lifecycle burst (2-3 hops = +0.30-0.45 s) lands INSIDE the window
	// and the first allowed check completes the empty area (room-1 skips 7/7
	// on record); allocator sharp edges (find returns a released handle -1
	// without resurrecting @0x800B5710; ring-full = silent non-registration
	// @0x800B5830-40) supply the fight-phase instant-resolves (blue's boss
	// 14/14, stage-2) which are clock-independent - exactly why P040b's clean
	// clock did not move them.
	//
	// POKE 1 "window-narrow" @virtual 0x8001417C (program-space 0x0001417c),
	// VERIFIED in full.txt line 20576 before hardcoding:
	//   8001417C: 2842000B slti $v0,$v0,0xb
	// -> 0x28420001 (slti $v0,$v0,1): the walk-phase condition-B window
	// shrinks from the last 10 frames to end-cursor<1.  FIGHT descriptors
	// activate at cursor==end-1 (end-cursor==1, still gated) and flip bit0
	// FIRST - the pre-activation completion window is closed COMPLETELY (the
	// next condition-B run takes the bit0 branch with the wave loaded).
	// WALK-ONLY descriptors (rec+0x70==-1, never engage/activate) still
	// advance through the designed desc[0x14]==-1 -> lookup -1 path once the
	// cursor ticks past end (end-cursor<=0) - up to 10 frames (<=0.17 s) later
	// than stock per walk segment, benign.  Not a branch target, not in a
	// delay slot (beqz@80014168 targets 0x80014188; the slti feeds
	// beqz@80014180).
	//
	// POKE 2 "not-found clamp" @virtual 0x800B56E0 (program-space
	// 0x000b56e0), VERIFIED in full.txt line 185785 before hardcoding:
	//   800B56E0: 2402FFFE li $v0,-2
	// -> 0x24020000 (li $v0,0 = addiu $v0,$zero,0): the ring lookup's
	// not-found verdict becomes "alive".  Kills the beta/never-registered
	// flavor EVERYWHERE including the fight phase (bit0 set - which poke 1
	// does not cover).  Scope is exactly condition B: 0x800B5690's sole
	// caller is 0x8001418C (career grep re-verified this session; the
	// allocator 0x800B56EC has its own inline ring search).  The id==-1 -> -1
	// fast path (walk-segment advance) and the found-slot handle return
	// @0x800B56A4 are untouched; the word is the fall-through of the search
	// loop (bgez@800B56D8), not a branch target, not a delay slot.  Legit -2
	// completions lost: only an anchor released AFTER its slot was recycled
	// by a newer registration (one-frame ordering window; the 40 s area
	// watchdog +0x3A2 still backstops).
	//
	// MECHANICS (per poke, independent - one REFUSING must NOT block the
	// other): each armed vblank until that poke is terminal, read the u32 via
	// the CPU program space; 0 = boot loader has not copied the program yet,
	// keep waiting; the verified original word = poke the replacement,
	// one-shot banner; any OTHER nonzero = one-shot REFUSED banner for THAT
	// poke only (ROM-revision guard - never patch an unverified instruction;
	// the sibling poke and all rails continue).  DRC soundness = the P040b
	// ordering argument (mips3 has no store-triggered invalidation; loose
	// verify checks only sequence-head words; both words are mid-sequence):
	// the pokes land in early boot (P040b measured the loader placing this
	// image by frame ~11, t~0.18 s) and the completion check first executes
	// when gameplay first runs (attract demo, tens of seconds later at the
	// earliest), so its block is ALWAYS compiled from the already-poked RAM.
	// Residual re-copy windows FAIL TO STOCK, not to corruption (a block
	// compiled from the original words merely behaves as the unpatched ROM):
	// the 1/s combined guard re-reads both words, re-pokes on
	// re-materialization (logged + counted, expect 0), and the status line
	// carries both word rails (expect 28420001 / 24020000 from the pokes
	// onward).  Both cabs run the same env (the race is role-symmetric - each
	// cab completes its OWN record).  Counters diagnostics-only, not
	// save_item'd (any save/load state converges: original word -> poke or
	// re-poke path; poked word -> nothing to do).  Experiment branch only -
	// never merge to milestone.
	bool m_patch_waveinit_hold;
	bool m_wih_win_poked;           // one-shot: window-narrow word is in RAM
	bool m_wih_win_refused;         // one-shot: ROM-revision guard tripped @0x8001417C - this poke inert
	u32 m_wih_win_repokes;          // guard re-pokes after re-materialization (expect 0)
	bool m_wih_clamp_poked;         // one-shot: not-found-clamp word is in RAM
	bool m_wih_clamp_refused;       // one-shot: ROM-revision guard tripped @0x800B56E0 - this poke inert
	u32 m_wih_clamp_repokes;        // guard re-pokes after re-materialization (expect 0)

	// P043 trace: READ-ONLY wave-anchor / ring observability - env
	// NAMCOS23_TRACE_WAVEINIT (independent of the patch env; either works
	// alone).  All reads m_mainram only, per-event lines only:
	//  - "WAVEINIT: bit16" on every p370 bit16 0->1 edge (both records,
	//    rec= tags them; mirrors carry bit30 in the printed p370): dumps the
	//    f5c word (+0x5C, bit0 = wave active), the intro-timeline cursor/end
	//    (rec+0x68/rec+0x6A) and end-cursor, the anchor id (rec+0x364), the
	//    big-map view (rec+0x368), the FULL 4-slot ring (id:handle x4,
	//    0x802D2030..0x802D204C) + rotation cursor (gp+0x73E0 = 0x802CEC00),
	//    and an EMULATED ring lookup replicating 0x800B5690 exactly
	//    (id==-1 -> -1; else slots (cursor+a1)&3 for a1=3..0, first id match
	//    returns the slot handle lh; none -> -2) - enough to name the poison
	//    flavor per the P042 taxonomy, printed as flavor=: hookA-conditionA
	//    (bit17 set in the same store - the per-descriptor hook, ring
	//    irrelevant), gamma-rearm (anchor id changed since the engage rise),
	//    no-id-walkclass (anchor==-1, the designed walk-advance path),
	//    beta-never-registered (lookup -2), alpha-released-handle (found,
	//    handle<0), healthy-unexplained (found, handle>=0, no bit17 - MUST
	//    not appear: condition B cannot have fired on a healthy slot).
	//  - "WAVEINIT: engage-cycle" once per activation (f5c bit0 0->1): the
	//    cycle's engage t (p370 bit5 0x20 rise), the stock check-window entry
	//    t (end-cursor crossing <11 during the walk) and the narrowed-window
	//    entry t (crossing <1), the activation t, the anchor, and whether
	//    bit16 was ALREADY set (pre-latched = the skip signature).
	// Expected healthy choreography (P042 F3): engage -> window11 at
	// engage+0.35 -> activation at engage+0.50, bit16 never inside the walk
	// window.  Cycle trackers re-arm on each engage rise; save/boot states
	// converge via the have_prev guard.  Experiment branch only.
	bool m_trace_waveinit;
	bool m_wt_have_prev;
	u32 m_wt_prev_p370[2];          // last vblank p370 (bit16 + engage-bit edges)
	u32 m_wt_prev_5c[2];            // last vblank +0x5C (activation bit0 edge)
	s32 m_wt_prev_endcur[2];        // last vblank end-cursor (window-crossing edges)
	double m_wt_engage_t[2];        // t of the current cycle's engage rise (-1 = none yet)
	u32 m_wt_engage_anchor[2];      // rec+0x364 latched at the engage rise (gamma detection)
	double m_wt_win11_t[2];         // t end-cursor first crossed <11 in the walk this cycle (-1 = not yet)
	double m_wt_win1_t[2];          // t end-cursor first crossed <1 in the walk this cycle (-1 = not yet)

	// P044 EXPERIMENT (branch patch/anchor-lifecycle, off patch/wave-init-hold):
	// the ANCHOR LIFECYCLE fix - attack the named root of ALL area skips.  The
	// P043b measurement (402 s, second boss) proved the ENTIRE skip economy
	// (14/14 race skips, room-1 through the tank set-piece, both cabs) is
	// pre-activation WAVE-ANCHOR DEATH: the segment's anchor entity id
	// (rec+0x364 = desc[0x14]) sits in the 4-slot (id,handle) ring @0x802D2030
	// with handle -1 BEFORE the completion window opens (13/14 dead at
	// win11+0), flavor alpha-released-handle unanimous, beta ZERO in 402 s.
	// Two death sub-classes: (i) BORN-DEAD commits - the commit's
	// find-or-create (allocator 0x800B56EC) hits a stale DEAD ring slot and
	// returns its handle WITHOUT resurrecting (@0x800B5710: found -> lhu
	// handle, and the create/registration path is only reached on verdict -2 =
	// not-found via slti -1 @0x800B5758 - VERIFIED again this session), so no
	// fresh entity is ever made (the 80264ef8 cascade: ONE dead slot re-latched
	// across 4 successive commits); (ii) post-commit WALK-PHASE KILLS (room-1
	// class: anchor created at seed, dead within 1.33 s on BOTH cabs, killing
	// transaction unobserved - predicted: an op-0x20 release ingest echo of the
	// round-start cull/avatar burst, RX 0x800ACD48 -> release 0x800B5BC8 ->
	// ring handle=-1 @0x800B5C14-1C).  Window pokes cannot fix either class
	// (P043b: imm=2 rescues ~1/14); the fix must keep the ANCHOR alive
	// (shield) or make dead slots REUSABLE at the next commit (resurrect).
	//
	// RING FACTS VERIFIED in full.txt this session (the whole design rests on
	// them; ring-base reference census CLOSED at exactly these + the init):
	//  - init 0x800B5590: slots start (id=0, handle=-1); cursor gp+0x73E0=0.
	//  - lookup 0x800B5690 (sole caller = condition B @0x8001418C): id==-1
	//    fast-path returns -1 WITHOUT searching; loop first-match by id from
	//    (cursor+3)&3 down; not-found -2.  Caller tests only the SIGN.
	//  - allocator 0x800B56EC find: same id==-1 fast path + same loop; found
	//    -> return handle EVEN IF -1 (no create); ONLY verdict -2 (strictly
	//    < -1) reaches create 0x80031B34 + registration.
	//  - registration @0x800B5808-38: scans (cursor+k)&3 for the FIRST slot
	//    whose lh handle < 0 and overwrites BOTH fields (sw id / sh handle) -
	//    the slot ID is IRRELEVANT to reuse; all-live = silent non-register.
	//  - release ring-mark @0x800B5C90-5CC0: find-by-id, store sh -1 into
	//    slot+4 (id left in place); find-miss = no store.
	// COROLLARY: a dead slot whose ID is rewritten to 0xFFFFFFFF is INVISIBLE
	// to every find (both fast-path id==-1 before the loops, so 0xFFFFFFFF is
	// unmatchable) yet still a perfectly reusable free slot (handle stays -1);
	// and for condition B a scrubbed slot merely turns verdict -1 into -2 -
	// SAME negative sign = SAME completion behavior (the designed last-kill ->
	// complete mechanism is sign-only).  That corollary is gate 3.
	//
	// THREE INDEPENDENT ENV GATES (any subset; all inert unset; both cabs -
	// the race is role-symmetric):
	//
	// (1) NAMCOS23_TRACE_OP20 - READ-ONLY ring-transaction trace (grep "OP20:").
	//     Driver half (here): per-vblank diff of the 4 ring slots names every
	//     kill (handle live->dead), registration (id change + live handle),
	//     revive (falsifier - no stock path does this) and dead-slot rewrite,
	//     each with which record's rec+0x364 matches the id, that record's
	//     walk/fight phase (f5c bit0) and end-cursor; plus a commit-edge
	//     detector (rec+0x35C/+0x364 change) that classifies each commit's
	//     first post-commit ring state: born-dead (found dead - the class-i
	//     counter), unregistered (absent - beta fuel), healthy.  Device half
	//     (namco_c139.cpp, same env): the P018 cell-walk reports genuine
	//     op-0x20 release / op-0x1F despawn cells on RX and TX with their wire
	//     id16 - an rx-release adjacent (by t=) to a ring-kill NAMES the killer
	//     as an ingested release; a kill with no adjacent rx line = local
	//     release (cull/cascade/avatar).  Purpose: name the class-(ii) killer
	//     and measure sub-class shares.  Vblank granularity: a same-frame
	//     kill+re-register collapses to one edge (documented since P015).
	//
	// (2) NAMCOS23_PATCH_ANCHOR_SHIELD - walk-phase anchor-death repair (grep
	//     "ANCHOR_SHIELD:").  Own records only (p370 bit30 clear - the
	//     completion-check domain), anchor valid (!= -1), f5c bit0 CLEAR (wave
	//     not yet active - "no legitimate partner kill can target a wave that
	//     does not exist yet"): while walking, the shield CAPTURES the anchor's
	//     live ring handle each vblank; if the slot goes dead mid-walk
	//     (ingested op-0x20 echo or local cull), it RESTORES the captured
	//     handle (halfword lane at slot+4, low half preserved) - so condition
	//     B's window-open lookup sees a live anchor and the empty completion
	//     never latches.  The release itself is NOT undone (the entity keeps
	//     its released mark; only the ring VIEW is restored): whether the
	//     activation then plays the wave correctly is exactly what run 1
	//     measures.  NO re-kill is applied at activation - re-applying would
	//     recreate the skip at the activation instant, the outcome P043b
	//     already proved worthless (imm=2 verdict).  P025 log-and-yield: max 8
	//     repairs per walk cycle, then yield with a line - never fight a
	//     persistent ROM writer.  Born-dead slots (no live capture ever) are
	//     logged "missed" and left alone - gate 3's domain.  Repairs write
	//     m_mainram DATA (never code): no DRC concern (fastram loads/stores
	//     target the same host buffer; vblank runs on the emulation thread).
	//
	// (3) NAMCOS23_PATCH_ANCHOR_RESURRECT - the born-dead fix: dead-slot ID
	//     SCRUB (grep "ANCHOR_RESURRECT:").  Per vblank, any ring slot with
	//     handle < 0 (GUARD: never touch a live handle) and a real id (not 0 =
	//     init state, not 0xFFFFFFFF = already scrubbed) gets its ID word
	//     rewritten to 0xFFFFFFFF.  Effect: the NEXT commit of that anchor id
	//     find-MISSES (verdict -2) -> allocator creates a FRESH entity and the
	//     registration reuses the (still handle<0) slot -> the anchor is BORN
	//     LIVE - the no-resurrect edge is bypassed with a REAL entity, not a
	//     forged handle.  This is "find returns dead => behave as not-found"
	//     implemented on the DATA side: no code cave, no poke, one word of
	//     game data per dead slot, sign-neutral for condition B (see the
	//     corollary above; the current, already-doomed cycle still completes
	//     empty - the scrub fixes the NEXT commit, which is exactly the
	//     born-dead cascade class).  Runs AFTER the shield in the same vblank
	//     (a repaired slot is live again and is skipped).  INCOMPATIBLE with
	//     the retired NAMCOS23_PATCH_WAVEINIT_HOLD clamp poke (li -2 -> li 0
	//     would make scrubbed slots read ALIVE forever): machine_start REFUSES
	//     to arm RESURRECT if WAVEINIT_HOLD is set.
	//
	// Ordering per vblank: sample -> (1) trace diffs vs prev (ROM-caused
	// transactions only) -> (2) shield repairs -> (3) resurrect scrubs ->
	// prevs = POST-WRITE state (our own writes never masquerade as ROM
	// transactions in the next sample).  Counters diagnostics-only, not
	// save_item'd.  Experiment branch only - never merge to milestone.
	bool m_trace_op20;              // gate 1: READ-ONLY ring-transaction trace
	bool m_patch_anchor_shield;     // gate 2: walk-phase anchor-death repair
	bool m_patch_anchor_resurrect;  // gate 3: dead-slot id scrub
	bool m_anl_have_prev;           // shared sampler basis valid
	u32 m_anl_prev_ringid[4];       // last post-write ring slot ids
	s32 m_anl_prev_ringh[4];        // last post-write ring slot handles (s16-extended)
	u32 m_anl_prev_anchor[2];       // last rec+0x364 per record (commit-edge detection)
	u32 m_anl_prev_desc[2];         // last rec+0x35C per record (commit-edge detection)
	u32 m_o20_kills;                // trace: handle live->dead edges
	u32 m_o20_regs;                 // trace: fresh registrations (id change + live handle)
	u32 m_o20_revives;              // trace FALSIFIER: same-id dead->live without registration (no stock path; expect 0)
	u32 m_o20_rewrites;             // trace: dead-slot rewrites / other combos
	u32 m_o20_borndead;             // trace: commits landing on a stale dead slot (class i; RESURRECT armed => ~0)
	u32 m_o20_unreg;                // trace: commits whose anchor is absent from the ring (beta fuel)
	u32 m_o20_healthy;              // trace: commits whose anchor is ring-live at first sample
	s32 m_ash_live_handle[2];       // shield: captured live handle for the current walk anchor (-1 = none yet)
	u32 m_ash_live_id[2];           // shield: anchor id the capture belongs to
	u32 m_ash_repairs_cycle[2];     // shield: repairs this walk cycle (cap 8, then yield)
	bool m_ash_yielded[2];          // shield: yielded this cycle (persistent writer)
	bool m_ash_missed_logged[2];    // shield: born-dead "missed" line latched this cycle
	u32 m_ash_repairs;              // shield: total repairs
	u32 m_ash_yields;               // shield: total yields
	u32 m_ash_missed;               // shield: total missed (born-dead sightings from the shield's view)
	u32 m_ars_scrubs;               // resurrect: total dead-slot id scrubs

	// P055 EXPERIMENT (branch patch/boat-jitter-trace, off patch/single-burst-pump):
	// READ-ONLY per-vblank BLUE entity-ingest trace (grep "BOATJITTER:"), env-gated
	// by NAMCOS23_TRACE_BOATJITTER (inert when unset).  Diagnoses the linked-play
	// water/BOAT-level defect: enemy boats "shake in place" (snap backward then
	// forward) ONLY on blue (connector/follower), never solo, never on red - so the
	// artifact is in blue's ingest path and shows on fast entities.
	//
	// PHASE 1 (RE, see agent-log): the on-screen position of an actor-array entity
	// is a LOCAL per-frame recompute (renderer 0x80089C78 rebuilds the object's
	// +0x30/+0x34/+0x38 world position each frame from matrix*offset+base) of an
	// INGESTED transform: the actor records at 0x802F4DB0 + slot*0x2B8 hold the
	// coordinates that the WIRE opcodes write - op0x17/0x18/0x19 (full transform)
	// -> record +0xA8/+0xAC/+0xB0 (s32 packed position vector), and op0x21-0x29 /
	// 0x2A-0x33 (spawn/transform actor with 3 coords) -> record +0x1F4/+0x1F8/+0x1FC.
	// So the position is BOTH locally recomputed AND ingest-driven; whether blue
	// ALSO advances the boat with a local motion script (dual-writer, hyp B) vs
	// merely holds/re-applies stale ingested coords (stale-replay, hyp A) is the
	// empirical question this trace answers.  (op02 field A is a per-frame STATUS
	// mirror, NOT a coordinate - not sampled.)
	//
	// METHOD (same per-vblank memory-sample + diff idiom as the OP20 ring trace):
	// per vblank on the CONNECTOR/blue while staging mode 0x802F3FD0==2, scan the
	// actor array, gate to live slots (status word at base+0x18 = 0x802F4DC8 +
	// slot*0x2B8 < 0), and diff two candidate ingested position triples
	// (A=+0x1F4/+0x1F8/+0x1FC, B=+0xA8/+0xAC/+0xB0) vs last vblank.  A REVERSAL =
	// the frame-to-frame delta of a component flips sign vs the previous delta
	// (|deltas| >= BJ_MIN_MOVE, to reject 1-LSB noise) - the actual "shake" signal.
	// SOURCE tag is a per-vblank attribution: if the device rx-apply generation
	// advanced during this vblank window (an ingest landed), the move is tagged RX
	// (freshness = the last delivered frame's class, FRESH vs HEARTBEAT-REPLAY from
	// the device bridge), else LOCAL (a local-sim advance with no ingest this
	// window).  At vblank granularity a specific slot's write cannot be bound to a
	// specific frame, but over the boat era ingests are frequent, so the
	// REPLAY-vs-FRESH split ACROSS reversal frames is the A/B discriminator:
	// reversals riding REPLAY deliveries => hyp A; reversals alternating
	// LOCAL-forward / RX-FRESH-backward => hyp B.  Log-on-reversal + a 1/s status
	// rail keeps the boat-area capture in the low-MB range (see the agent-log).
	// MODEL PROVENANCE: Opus 4.8.  Experiment branch only - never merge to milestone.
	static constexpr int BJ_SLOTS = 48;   // actor-array slots scanned (liveness-gated; 48*0x2B8 stays in RAM)
	static constexpr s32 BJ_MIN_MOVE = 4; // min |delta| on a component to count as motion (reject LSB noise)
	static constexpr int BJ_MAX_LINES_FRAME = 24; // per-vblank reversal-line safety cap
	struct bj_slot
	{
		s32 v[6];       // last sampled components: 0..2 = A(+0x1F4/+0x1F8/+0x1FC), 3..5 = B(+0xA8/+0xAC/+0xB0)
		s32 d[6];       // last frame-to-frame delta per component (for sign-flip reversal detection)
		bool live;      // slot was live last vblank (delta basis valid)
	};
	bool m_bj_trace;                // env gate (driver side)
	bool m_bj_have_prev;            // at least one prior sample taken
	bj_slot m_bj_prev[BJ_SLOTS];    // per-slot previous sample
	u32 m_bj_prev_rxgen;            // device rx_apply_gen at last vblank (ingest-this-window detector)
	u32 m_bj_reversals;             // cumulative reversals (1/s status)
	u32 m_bj_rx_moves;              // cumulative reversal-moves attributed to an RX delivery
	u32 m_bj_local_moves;           // cumulative reversal-moves with no ingest this vblank

	// P056 EXPERIMENT (branch patch/boat-render-trace, off patch/boat-jitter-trace):
	// READ-ONLY per-vblank BLUE RENDERED-position + liveness trace (grep
	// "BOATRENDER:"), env-gated by NAMCOS23_TRACE_BOATRENDER (inert when unset).
	// REFINES P055: P055 watched the INGEST coordinate fields (+0xA8/+0x1F4) and
	// caught only 17 reversals in ~7 min, 14/17 on EMPTY id=0000 slots - it traced
	// the wire coords, not the position the eye sees.  P056 instead watches the
	// RENDERED world position +0x30/+0x34/+0x38 (rebuilt every frame by the object
	// renderer 0x80089C78 as matrix(+0x3C/40/44)*record_offset+base, then trunc.w.s
	// to integer world coords), and FILTERS OUT empty slots (real id only), so it
	// captures the actual visible shake.  Reuses the P055 device bridge (rx-apply
	// generation + FRESH/HEARTBEAT-REPLAY classifier) verbatim for source attribution
	// (the device arms that bridge on NAMCOS23_TRACE_BOATJITTER OR _BOATRENDER).
	//
	// PART 1 - rendered reversal (the visible shake): per vblank on the
	// CONNECTOR/blue while staging mode 0x802F3FD0==2, scan the actor array
	// 0x802F4DB0 + slot*0x2B8, gate to ACTIVE actors (status word +0x18 < 0 AND a
	// real id, i.e. skip id=0000/ffff empty/placeholder slots), and diff the rendered
	// world pos +0x30/+0x34/+0x38 vs last vblank.  A REVERSAL = a component's
	// frame-to-frame delta flips sign vs the previous delta (|deltas| >= BR_MIN_MOVE)
	// - the "snap backward then forward" the eye sees.  Each reversal is tagged RX
	// (device rx-apply generation advanced this vblank window = an ingest landed;
	// freshness = last delivered frame's class) vs LOCAL (no ingest this window), the
	// same A/B discriminator P055 used: reversals riding REPLAY => stale-replay
	// (hyp A); reversals splitting LOCAL-forward / RX-FRESH-backward => dual-writer
	// (hyp B).
	//
	// PART 2 - liveness timeline (the disappearing red player + 1P tag): whenever an
	// actor slot's +0x18 live flag TRANSITIONS (live->dead or dead->live) with a real
	// id on either side, log one line with the same RX/LOCAL + fresh/replay
	// attribution - revealing the slot whose liveness flickers on blue during the boat
	// scene and whether an RX-fresh vs LOCAL write drives it.
	//
	// UNCERTAINTY (see agent-log): renderer 0x80089C78 holds +0x30/34/38 on the
	// OBJECT INSTANCE whose +0x18 indexes the 0x802F4DB0 record table; the task's
	// known-RE treats these 0x2B8-stride records as also carrying the rendered world
	// pos at +0x30/34/38.  If the log shows large-magnitude coords that reverse often
	// in the boat scene this is the visible field; if tiny/static it is not and the
	// object-instance array must be found.  READ-ONLY.  MODEL PROVENANCE: Opus 4.8.
	static constexpr int BR_SLOTS = 48;    // actor-array slots scanned (liveness-gated; 48*0x2B8 stays in RAM)
	static constexpr s32 BR_MIN_MOVE = 4;  // min |delta| on a component to count as motion (reject LSB noise)
	static constexpr int BR_MAX_LINES_FRAME = 32; // per-vblank line cap (reversals + liveness combined)
	struct br_slot
	{
		s32 v[3];       // last rendered world pos +0x30/+0x34/+0x38 (X/Y/Z)
		s32 d[3];       // last frame-to-frame delta per component (sign-flip reversal basis)
		u32 id;         // last sampled id ((+0x1F0>>8)&0xffff) - for liveness id attribution
		bool live;      // +0x18 < 0 last vblank
		bool sampled;   // had a valid ACTIVE position sample last vblank (delta basis valid)
	};
	bool m_br_trace;                // env gate (driver side)
	bool m_br_have_prev;            // at least one prior vblank scanned (liveness arm)
	br_slot m_br_prev[BR_SLOTS];    // per-slot previous sample
	u32 m_br_prev_rxgen;            // device rx_apply_gen at last vblank (ingest-this-window detector)
	u32 m_br_reversals;             // cumulative rendered reversals (1/s status)
	u32 m_br_rx_moves;              // cumulative reversal-moves attributed to an RX delivery
	u32 m_br_local_moves;           // cumulative reversal-moves with no ingest this vblank
	u32 m_br_live_transitions;      // cumulative liveness transitions logged

	// P059 EXPERIMENT (branch patch/poscorr-trace, off patch/boat-render-trace):
	// READ-ONLY per-vblank BLUE CORRECTION-CADENCE + LOCAL-DRIFT trace on the
	// remote actor's world-pos BASE +0xCC (type-1) / +0x80 (type-2) - the FOUGHT
	// FIELD the local-position-writer RE pinned (grep "POSCORR: rom").  Env-gated
	// by NAMCOS23_TRACE_POSCORR (inert/byte-identical when unset), connector-gated
	// (blue only), staging-mode-2 gated.  Serves FIX FAMILY B (make the transport
	// deliver red's position corrections more often / in-order).  MODEL PROVENANCE:
	// Opus 4.8.  Experiment branch only - never merge to milestone.
	//
	// WHY SAMPLING, NOT A WRITE-TAP (task-mandated decision, documented):  the ideal
	// instrument is a memory write-tap on the actor array that logs the store PC to
	// bucket each +0xCC write as INGEST (op4B/4C 0x800AF8E0) vs LOCAL (transform tick
	// 0x80100014 / 0x8010C5B0 / lean 0x800FEB10).  It is IMPRACTICAL on this driver:
	// the whole main RAM is registered add_fastram (machine_start ~line 10388), so
	// under the DRC every kseg0 store bypasses the memory system entirely (no tap
	// fires) AND mips3's exported PC is block-stale mid-store (see the P039/P040
	// op6f-no394 tap notes above).  The only sound tap forces the INTERPRETER
	// (set_force_no_drc), which the P040 run measured at ~1.56x wall slowdown - and,
	// worse for a DIAGNOSTIC, forcing the interpreter perturbs the very CPU timing /
	// heartbeat cadence this trace is meant to measure (Heisenberg; establishment is
	// fragile - 16 ms broke it).  So per the task's fallback clause we SAMPLE the
	// base per vblank and KEY the INGEST/LOCAL attribution on the device rx-apply
	// generation bridge (the same bridge P055/P056 use): a base move in a vblank
	// where rx_apply_gen advanced (an op4B/4C snapshot was delivered) is a CORRECTION;
	// a base move with no ingest this window is LOCAL DRIFT.  Limitation (same as
	// P055): rx_apply_gen is GLOBAL not per-slot, so within one vblank a correction
	// to actor A and local drift on actor B both read "ingest window" - acceptable
	// because we aggregate over the still-crouch era where the answer is a RATE, not
	// a per-frame attribution.
	//
	// The base is read as an IEEE float (0x80100014 composes it via the FPU; the
	// renderer trunc.w.s's it into the integer +0x30 P056 sampled), guarded by
	// std::isfinite so a non-float word can never poison the magnitude.  Per remote
	// slot we track: the gap (frames) between consecutive INGEST corrections (a
	// histogram => steady ~HB vs bursty-with-long-gaps), and the LOCAL drift
	// magnitude that accumulates on +0xCC between corrections (how far blue's local
	// prediction wanders before the next snap corrects it) - the two numbers the
	// B verdict needs: current correction rate (Hz) and how much correction is owed.
	static constexpr int PC_SLOTS = 48;             // actor-array slots scanned (remote/live-gated; 48*0x2B8 in RAM)
	static constexpr float PC_MIN_MOVE = 0.125f;    // min |d base| (float world units) counted as motion (reject FPU LSB noise)
	static constexpr float PC_SANE_MAX = 1.0e9f;    // reject an absurd delta (word was not actually a base float this frame)
	static constexpr int PC_MAX_LINES_FRAME = 16;   // per-vblank correction-line cap
	// P061 RIDER (branch patch/tx-complete-irq): per-1/s-window cap on the
	// anim-while-static lines (the discriminator needs existence + rough rate,
	// not every event - the digest carries the counts).
	static constexpr int PC_ANIM_MAX_LINES_WIN = 2;
	struct pc_slot
	{
		float cc[3];        // +0xCC/+0xD0/+0xD4 last sample (type-1 world base, IEEE float)
		float w80[3];       // +0x80/+0x84/+0x88 last sample (type-2 world base, IEEE float)
		bool  sampled;      // had a valid remote sample last vblank (delta basis valid)
		bool  corr_seen;    // last_corr_frame valid (>=1 correction seen on this slot)
		u32   last_corr_frame; // frame of the last INGEST correction (gap basis)
		float drift_accum;  // sum |local d base| since the last correction (pre-snap drift)
		float drift_max;    // peak drift excursion reached since the last correction
		u32   anim;         // P061 RIDER: +0x3C anim/orientation matrix word last sample (raw)
		u16   kf;           // P061 RIDER: +0x1A keyframe counter last sample (low half of the +0x18 word)
		bool  anim_sampled; // P061 RIDER: anim/kf delta basis valid
	};
	bool m_pc_trace;                // env gate (driver side)
	bool m_pc_pos_have_prev;        // >=1 prior vblank sampled (POSCORR; m_pc_have_prev is the P044 PLAYCLOCK flag)
	pc_slot m_pc_prev[PC_SLOTS];    // per-slot previous sample + drift/gap state
	u32 m_pc_prev_rxgen;            // device rx_apply_gen at last vblank (ingest-this-window detector)
	u32 m_pc_corr_win;              // INGEST-attributed base corrections this 1/s window
	u32 m_pc_corr_fresh_win;        // of those, device-classified FRESH (new positions)
	u32 m_pc_corr_replay_win;       // of those, heartbeat REPLAY (stale re-apply)
	u32 m_pc_local_events_win;      // LOCAL drift events (base moved, no ingest) this window
	float m_pc_local_sum_win;       // sum of local drift magnitude this window
	float m_pc_drift_max_seen;      // largest single pre-snap drift excursion (cumulative)
	u32 m_pc_corr_tot;              // cumulative corrections
	float m_pc_snap_sum;            // sum of correction snap magnitude (cumulative, for a mean)
	u32 m_pc_gap_hist[6];           // inter-correction gap buckets: [1][2][3-4][5-8][9-16][17+] frames
	u32 m_pc_remote_live_last;      // remote/live slot count last vblank (status context)

	// P061 RIDER (branch patch/tx-complete-irq, audit D): STILL-SHUDDER
	// ANIM-LAYER DISCRIMINATOR - READ-ONLY, same NAMCOS23_TRACE_POSCORR gate
	// (no new env).  P059's crouch-still window showed the fought base +0xCC
	// utterly quiet while the user-reported still-shudder may live in the
	// ANIM/POSE layer (keyframe advancer 0x8008A5D8 -> object matrix; the
	// `animMatrix * component` term the +0xCC arc never measured).  Alongside
	// the +0xCC/+0x80 sampling, diff per remote slot per vblank: the anim/
	// orientation matrix word +0x3C (raw u32) and the keyframe counter +0x1A
	// (low halfword of the +0x18 word - big-endian layout).  A change in a
	// vblank where the BASE was static (no move > PC_MIN_MOVE on either base)
	// = pose-layer motion with the base pinned - logged (capped
	// PC_ANIM_MAX_LINES_WIN/s) + counted.  During the still-crouch repro:
	// shudder visible while anim/kf churn and +0xCC static => pose layer
	// CONFIRMED (its own fix track); anim/kf also quiet => the shudder is not
	// in these words either (widen the probe).  Digest: 1/s 'POSCORR:
	// anim-status' line.  MODEL PROVENANCE: Fable 5.
	u32 m_pc_anim_chg_win;          // window: remote-slot vblanks where +0x3C changed
	u32 m_pc_kf_chg_win;            // window: remote-slot vblanks where +0x1A changed
	u32 m_pc_anim_ccstatic_win;     // window: anim/kf changed while BOTH bases static (the discriminator signal)
	u32 m_pc_anim_lines_win;        // window: anim-while-static lines emitted (cap PC_ANIM_MAX_LINES_WIN)

	// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
	// driver-side arm for the PHASE-AWARE heartbeat token cadence - env
	// NAMCOS23_PATCH_HB_PHASE_AWARE=<fast_ms> (inert/byte-identical when unset).
	// The DEVICE (namco_c139) reads the SAME env in device_start, validates the
	// value and owns the cadence choice + debounce; the driver's only job is
	// the MODE SIGNAL: each vblank, read the staging mode word 0x802F3FD0
	// (READ-ONLY - only the driver maps m_mainram) and push (mode == 2) to the
	// C139 via set_ingame(), the same driver->device bridge idiom as
	// set_local_counter / set_linked_gate_txonly.  Armed on BOTH cabs: the
	// heartbeat is the pacing TOKEN that clears the PEER's reg5 stop-and-wait
	// (P050/P059), so blue's cadence releases RED's emits and vice versa - a
	// one-sided arm only speeds one direction.  MODEL PROVENANCE: Fable 5.
	// Experiment branch only - never merge to milestone.
	bool m_patch_hb_phase_aware;    // env NAMCOS23_PATCH_HB_PHASE_AWARE (driver push arm)

	// P061 EXPERIMENT (branch patch/tx-complete-irq, off patch/hb-phase-aware):
	// driver-side arm for the C422 TX-COMPLETE dispatch model - env
	// NAMCOS23_PATCH_TX_COMPLETE_IRQ (inert/byte-identical when unset).  The
	// DEVICE (namco_c139) reads the SAME env in device_start and owns the
	// mechanism (synchronous dispatch of a freshly staged standalone TX = the
	// chip's txsize_commit trigger; see the device member block); the driver's
	// only job is the SAME mode signal P060 built: each vblank, read the
	// staging mode word 0x802F3FD0 (READ-ONLY) and push (mode == 2) to the C139
	// via set_ingame() - the device debounces and activates the dispatch model
	// only in stable mode-2, so op55 establishment keeps the stock reg5
	// stop-and-wait untouched.  Armed on BOTH cabs (each cab's own emits are
	// released by its own TX completing - no peer dependence).
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	bool m_patch_tx_complete_irq;   // env NAMCOS23_PATCH_TX_COMPLETE_IRQ (driver push arm)

	// P063 EXPERIMENT (branch patch/tx-complete-v2, off patch/txstage-trace):
	// driver-side arm for the P062-measured TX-COMPLETE RELEASE v2 - env
	// NAMCOS23_PATCH_TX_COMPLETE_V2 (inert/byte-identical when unset).  The
	// DEVICE (namco_c139) reads the SAME env in device_start and owns the
	// mechanism (prev-hash-gated synchronous dispatch + modeled TX-busy +
	// heartbeat stale age-out; see the device member block); the driver's
	// only job is the SAME P060 mode signal: each vblank, read the staging
	// mode word 0x802F3FD0 (READ-ONLY) and push (mode == 2) to the C139 via
	// set_ingame() - the device debounces and activates only in stable
	// mode-2, so op55 establishment keeps the stock reg5 stop-and-wait
	// untouched.  Armed on BOTH cabs (each cab self-releases).
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	bool m_patch_tx_complete_v2;    // env NAMCOS23_PATCH_TX_COMPLETE_V2 (driver push arm)

	// P065 EXPERIMENT (branch patch/corr-smooth, off patch/tx-complete-v2):
	// VIEWER-SIDE INGEST-CORRECTION BLENDING for remote entities - env
	// NAMCOS23_PATCH_CORR_SMOOTH (inert/byte-identical when unset).  The P064
	// verdict: the transport is done (everything red composes is dispatched,
	// arrives and is delivered), but a remote-viewed boat gets ~0.17 position
	// corrections/s against the ROM's 60 fps LOCAL writer of the same field -
	// so each correction lands as a visible SNAP (hold-then-teleport).  A bare
	// pin (hold the base at the last ingested value) trades 60 Hz jitter for
	// hold-then-teleport every few seconds, so instead we KEEP the ROM's local
	// prediction and BLEND each ingested correction over ~100-200 ms: when a
	// correction of delta D lands, immediately rewrite the base back to
	// (new - D) + D/N and release the remainder in ~D/N steps over the next
	// N-1 vblanks (N = NAMCOS23_PATCH_CORR_SMOOTH_FRAMES, default 8 ~ 133 ms).
	// The local writer keeps advancing from the blended base each frame - that
	// is the DESIGN (local prediction + gradual error correction), the same
	// contract the real C139 link satisfied by correcting every frame.
	//
	// NUMERIC FORMAT (phase-1 verification, 2026-07-13, full.txt): the world-
	// pos base at record+0xCC/+0xD0/+0xD4 (type-1) and +0x80/+0x84/+0x88
	// (type-2) is a SIGNED 32-BIT INTEGER triple, NOT an IEEE float and NOT a
	// packed 16.16 pair.  Three independent disasm sites prove it:
	//   - renderer 0x80089DF8/0x80089E74/0x80089EF0: lwc1 f0,0/4/8($a1) then
	//     cvt.s.w f0,f0 - the raw word is CONVERTED word->single, i.e. the
	//     memory word is an integer ($a1 = record+0xCC type-1 / +0x80 type-2);
	//   - writers 0x8010C5B0 (@0x8010C6E0 trunc.w.s + swc1 ->+0x80/84/88) and
	//     0x80100014 (@0x80100454/4D0/54C trunc.w.s + swc1 ->+0xCC/D0/D4):
	//     both stores are float results TRUNCATED TO INTEGER words;
	//   - 0x800FEB10 (@0x800FEB48-58): lw/lw/subu on the position words - the
	//     ROM itself does raw s32 SUBTRACTION on this field (then cvt.s.w's
	//     the difference).  So s32 delta/divide/add blend math below is the
	//     exact arithmetic the ROM already performs.  This also explains the
	//     P059 caveat: reading these words as IEEE floats gave garbage
	//     magnitudes because integers like 0x30BD0000 decode as denormal-ish
	//     floats.  As a residual rail (the task's fallback), any slot whose
	//     base word exceeds CS_SANE_ABS in magnitude (implausible world
	//     coordinate; observed live values are ~0x2BAxxxxx..0x30Bxxxxx, well
	//     inside) is PASSED THROUGH unsmoothed and counted format_reject.
	//
	// MECHANISM (per-vblank, BOTH cabs - every cab is a remote-viewer of the
	// peer's entities; red sees blue-owned enemies rubberband too; placed
	// AFTER the P059 POSCORR scan so the trace samples the RAW ROM values and
	// its correction counts stay comparable - we then refresh POSCORR's
	// per-slot cache with what we wrote so it never attributes our own blend
	// steps as ROM movement).  Walk the actor array 0x802F4DB0 + slot*0x2B8
	// (CS_SLOTS), gate to REMOTE (+0x18 bit31 set) + valid (+0x08 halfword
	// bit 0x8000 clear) + type 1/2 (+0x0A halfword selects WHICH base triple
	// per the renderer's own dispatch).  Per slot track the triple as of the
	// end of last vblank (seen = post-blend), the un-released remainder
	// (rem), and frames_left.  DETECT a correction: the device rx-apply gen
	// advanced this vblank (an ingest was delivered - the same P055/P056/P059
	// bridge) AND the slot's watched base moved >= CS min-delta (word-compare
	// first, then s32 decode).  On detect: if any |D| component exceeds the
	// teleport threshold (NAMCOS23_PATCH_CORR_SMOOTH_MAXD) DO NOT BLEND -
	// teleports/respawns/scene changes must stay instant (snap_through);
	// otherwise (re)start the blend: rem = D, frames_left = N (a NEWER
	// correction mid-blend RETARGETS from the current displayed value - the
	// old remainder is subsumed because the ROM's new word already embodies
	// the full authoritative position).  Each vblank with frames_left > 0:
	// step = rem / frames_left (s32 division; the last frame applies the
	// exact remainder so the blend converges precisely on the ROM's current
	// word), write base = cur - rem + step.  Because the step is applied
	// RELATIVE to the freshly-read current word, a local-writer advance (or a
	// full re-derivation) between our writes is preserved, never forced back
	// - the "re-detect rather than force" rule.  SAFETY RAILS: position words
	// ONLY (+0xCC/+0xD0/+0xD4 or +0x80/+0x84/+0x88 per the type gate) - never
	// status/liveness words; <= 3 word-writes per slot per vblank (<= 3 *
	// CS_SLOTS total); ALWAYS fail toward stock snap behavior - slot-id
	// change (+0x9A, the op4B/4C lookup key = reuse), type change, remote/
	// valid-bit loss, staging-mode-2 loss, out-of-range current or computed
	// value each reset the slot to passive tracking (the ROM's value stands).
	// One-shot banner at machine_start; 1/s "CORR_SMOOTH: status" rail.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	static constexpr int CS_SLOTS = 48;             // actor-array slots scanned (same array/stride as POSCORR)
	static constexpr s32 CS_SANE_ABS = 0x40000000;  // |base word| beyond this = not a plausible world coord (format rail)
	struct cs_slot
	{
		bool active;        // tracked (remote+valid+type-sane) last vblank; seen[] is a valid delta basis
		u8   type;          // record type (1 -> +0xCC triple, 2 -> +0x80 triple) at last sample
		u16  id;            // actor id (+0x9A, the op4B/4C snapshot lookup key) at last sample - slot-reuse detector
		u8   frames_left;   // blend frames remaining (0 = no active blend)
		s32  seen[3];       // watched base triple as of the END of last vblank (post any blend write)
		s32  rem[3];        // un-released correction remainder (target - displayed); 0 when idle
	};
	bool m_cs_on;                   // env gate NAMCOS23_PATCH_CORR_SMOOTH
	u8  m_cs_frames;                // blend length N (env _FRAMES, default 8, clamped 3..15)
	s32 m_cs_maxd;                  // teleport threshold per component (env _MAXD, raw s32 units)
	s32 m_cs_mind;                  // detection epsilon per component (env _MIND, raw s32 units, default 1)
	cs_slot m_cs_slot[CS_SLOTS];    // per-slot blend state
	bool m_cs_have_prev;            // >=1 prior vblank sampled (rxgen delta basis valid)
	u32 m_cs_prev_rxgen;            // device rx_apply_gen at last vblank (ingest-this-window detector)
	bool m_cs_mode2_prev;           // staging mode was 2 last vblank (loss -> full state reset)
	u32 m_cs_blends_win;            // 1/s window: blends started (correction detected + accepted)
	u32 m_cs_steps_win;             // 1/s window: blend steps applied (slot-vblanks with a write pass)
	u32 m_cs_snapthru_win;          // 1/s window: corrections passed through as instant snaps (>MAXD / reuse / rail)
	u32 m_cs_fmtrej_win;            // 1/s window: slots skipped by the CS_SANE_ABS format rail
	u32 m_cs_blends_tot;            // cumulative blends started
	u32 m_cs_snapthru_tot;          // cumulative snap-throughs
	u32 m_cs_fmtrej_tot;            // cumulative format rejects

	// P048 EXPERIMENT (branch patch/reaper-patience, off patch/anchor-lifecycle):
	// widen the ROM's bit31 snapshot-timeout REAPER threshold - the ONE-WORD fix
	// for the remaining anchor-death skip classes (P047 static RE) - env
	// NAMCOS23_PATCH_REAPER_PATIENCE (inert unset; VALUE-CHECKED: only "1" and
	// "2" are defined, any other value refuses at arm time - see machine_start).
	//
	// THE REAPER (P047, every claim PC-anchored in full.txt): the per-entity
	// tick 0x800B6B28's reap tail releases any REMOTE-PLACEHOLDER entity (word0
	// bit31 SET = partner-owned, its script has never run) that receives no
	// partner state for ~17 consecutive frames.  Gate @0x800B7184-71A4:
	//   lw word0; bgez -> skip            (bit31 must be SET)
	//   lbu +0xF2; +1; sb; andi 0x1F      (wrap-masked unserved-tick count)
	//   sltiu $v0,$v0,0x11; bnez -> skip  (survive while count < 17)
	// then release via jal 0x800B5BC8 @0x800B71CC.  The +0xF2 budget is
	// refreshed ONLY by partner state ingest for that entity (the six sb 0xf2
	// stores in the RX-apply cluster 0x800ABE9C-0x800AC818).  At the round-start
	// exchange regime (in-game HB floor, 4-9 drains/s) a spawn-burst serve takes
	// 2-3 hops = 0.30-0.45 s > the 17-tick 0.28 s budget, so freshly-spawned
	// bit31 placeholders die UNSERVED on both cabs: slave-side anchors directly
	// (8025c0c8 class, reaped at the 17th tick = the measured +16 f), and
	// room-1's ten op-0x2E REMOTE-spawned sub-wave children - whose deaths let
	// the anchor's DESIGNED completion (op 0x76 "wait while any live entity's
	// +0x86 tracks my id16" -> FFFFFFFF -> script-end self-release 0x800B6D30)
	// fire pre-engage at commit+36/+51 f = the exact P044 fingerprints.
	//
	// THE POKE: word @virtual 0x800B71A0 (program-space 0x000b71a0 under
	// global_mask; m_mainram word 0x2DC68), VERIFIED in full.txt before
	// hardcoding:
	//   800B71A0: 2C420011 sltiu $v0,$v0,0x11
	// The replacement is CONSTRUCTED from the verified word by changing ONLY
	// the 16-bit immediate field ((word & 0xFFFF0000) | imm):
	//   mode 1 -> imm 0x001F = 0x2C42001F: patience 17 -> 31 ticks (~0.52 s),
	//     beyond the 3-hop HB-floor worst case (0.45 s) - round-start serves
	//     land before the reap;
	//   mode 2 -> imm 0x0020 = 0x2C420020: the &0x1F-masked count (max 31)
	//     never reaches 32 = sltiu always true = REAPER OFF (escalation
	//     reserve for run 2 if 31 ticks proves short; the 40 s area watchdog
	//     +0x3A2 and the partner's designed op-0x20/despawn echoes remain the
	//     orphan cleanup).
	// On any OTHER nonzero word: one-shot REFUSE (ROM-revision guard - never
	// patch an unverified instruction).
	//
	// WHY THIS CANNOT RE-CREATE THE P043/P044 WALK/STAGING STALL CLASS - the
	// discriminator is STRUCTURAL, the reaper's own first test @0x800B7188:
	// the patch touches ONLY entities with word0 bit31 SET, and bit31 SET means
	// the script VM never runs the entity's program (placeholder tick
	// 0x800B6A50 instead of the interpreter).  Every designed walk/staging
	// completion rides the script-END self-release of a bit31-CLEAR entity
	// (the program must RUN to reach FFFFFFFF) or the desc[0x14]==-1 no-anchor
	// path - both disjoint from the patched class.  A patched-class member
	// either gets served (bit31 clears, it becomes a normal entity and dies
	// only by design) or lingers script-inert under the watchdog.  No
	// fingerprinting, no PC-range detection, no phase heuristic.
	//
	// DRC soundness = the P040b ordering class: the reap tail first executes
	// at attract-mode gameplay, tens of frames after the boot loader places
	// the word (the poke lands within one vblank of that); mips3 has no
	// store-triggered invalidation and namcos23 runs loose verify, so a block
	// compiled inside a residual re-copy window would embed the STOCK 17-tick
	// immediate = FAIL TO STOCK only (no corruption; the 1/s word rail plus
	// the run's OP20 reg-to-kill offset census - the +[15,18] f ring-kill
	// class MUST collapse to 0 - expose it); soft reset flushes the whole DRC
	// cache (device_reset); the 1/s guard re-pokes on re-materialization
	// (logged + counted, expect 0).  Both cabs run the same env (bit31
	// carriers exist on both sides: every slave-side allocator create, plus
	// remote-designated op-0x2E children on either cab).  Counters
	// diagnostics-only, not save_item'd (any save/load state converges:
	// word == original -> poke/re-poke path; word == patched -> nothing to
	// do).  Experiment branch only - never merge to milestone.
	int m_patch_reaper_patience;    // 0 = inert; 1 = imm 0x11->0x1F (31 ticks); 2 = imm 0x11->0x20 (reaper OFF - reserve)
	bool m_rpp_poked;               // one-shot: the widened immediate is in RAM
	bool m_rpp_refused;             // one-shot: ROM-revision guard tripped (unexpected nonzero word) - inert
	u32 m_rpp_repokes;              // guard re-pokes after re-materialization (expect 0)

	// P050 EXPERIMENT (branch patch/single-burst-pump, off patch/reaper-patience):
	// the SINGLE-BURST PUMP - the ONE-WORD ROM half of the transport fix for the
	// remaining top defects (P049 static RE).  MODEL PROVENANCE: Opus 4.8.
	//
	// P049 F1 decoded the fight-era 19/s -> 5.5-11/s service collapse as
	// EMERGENT, not a scheduler: the ROM composes+sends a fresh link frame every
	// 60 Hz tick over a one-frame-in-flight STOP-AND-WAIT wire, and its OWN TX
	// pump 0x8000BB6C chops any frame > 255 hw into <=0xFF-hw bursts, so a
	// fight-era frame (2-4 bursts) costs 2-4 device round-trips = the measured
	// rate and the 0.9-2 s red->blue queueing.  The 255-hw ceiling is the pump's
	// burst QUANTUM, not the chip's frame limit: the receive-side drain validator
	// @0x8000BF90 accepts slot lengths in [4..0x400] (VERIFIED this session:
	//   8000BF8C: 2442FFFC addiu $v0,$v0,-4
	//   8000BF90: 2C4203FD sltiu $v0,$v0,0x3fd   ; (len-4) < 0x3FD  ->  len <= 0x400
	// ), and each C422 TX window slot IS 0x400 hw.  Our largest frame is bounded
	// at 0x380 payload bytes + header/trailer = 0x394 hw < 0x401.
	//
	// THE POKE: word @virtual 0x8000BC78 (program-space 0x0000bc78 under
	// global_mask; m_mainram word 0x2F1E), VERIFIED in full.txt before hardcoding
	// (the pump's frame-START burst-size selector):
	//   8000BC74: 00021403 sra   $v0,$v0,16       ; sign-extend the frame len
	//   8000BC78: 28420100 slti  $v0,$v0,0x100    ; len < 0x100 ?
	//   8000BC7C: 14400009 bnez  $v0,0x8000bca4   ; small -> WHOLE-frame path (reg5 = full len)
	//                                              ; else -> first-burst reg5 = 0xFF, txrem = len-0xFF
	// The replacement changes ONLY the 16-bit immediate ((word & 0xFFFF0000) |
	// 0x0401 = 0x28420401 = slti $v0,$v0,0x401): every frame <= 0x400 hw then
	// takes the WHOLE-frame branch = ONE burst, and the 0xFF-hw continuation
	// paths (@0x8000BB88-BBE8) go dead.  Imm 0x401 (not 0x400) so a frame of
	// exactly 0x400 hw - the RX validator's own ceiling - still goes single.
	//
	// DEVICE CHECKLIST (P049 F6.1; audited against namco_c139.cpp this session):
	// the CHUNK_PASSTHRU saturation heuristic keys on frame_size == EXACTLY 0xFF
	// hw AND expected_hw > 0xFF, so a single burst (frame_size == expected_hw,
	// != 0xFF) is NOT saturated and passes through as one complete frame; REASM
	// never engages (no 0xFF-hw head); deliver_rx_frames writes any size <= the
	// 0x4000-byte cap into the RX ring with the faithful 0x0FFF wrap; the
	// announce-latch snapshot (AL_SNAP_MAX_BYTES 0x2000) fits a 0x400-hw frame,
	// and a wiped large frame dispatches whole from the latch.  The device reads
	// this same env in device_start for two SMALL companions (latch consumed_ok
	// retirement + heartbeat-clock re-arm for >255-hw single bursts) - see the
	// namco_c139.h P050 block; both self-guard so a DRC-stale poke is inert.
	//
	// DRC soundness = the P040b/REAPER_PATIENCE ordering class: the pump's
	// frame-start path first executes at attract link-up (link frames flow only
	// once the TCP session is up, seconds/hundreds of frames after the early-boot
	// poke lands), so the DRC compiles the pump block from the already-poked RAM;
	// mips3 has no store-triggered invalidation and namcos23 runs loose verify,
	// so a block compiled inside a residual re-copy window would embed the STOCK
	// 0x100 quantum = FAIL TO STOCK only (no corruption; the run's own falsifier
	// is the tell - BURST_QUANTUM poked=1 but the device still logs 255-hw chunk
	// trains / single_burst_tx stays 0 in fights => the compiled block is stale).
	// Soft reset flushes the whole DRC cache (device_reset); the 1/s guard
	// re-pokes on re-materialization (logged + counted, expect 0).  Both cabs run
	// the same env (both compose+chunk).  On ANY other nonzero word: one-shot
	// REFUSE (ROM-revision guard).  Counters diagnostics-only, not save_item'd
	// (converges: word == original -> poke path; word == patched -> nothing).
	// Experiment branch only - never merge to milestone.
	bool m_patch_burst_quantum;    // env NAMCOS23_PATCH_BURST_QUANTUM (driver poke arm)
	bool m_bq_poked;               // one-shot: the widened immediate is in RAM
	bool m_bq_refused;             // one-shot: ROM-revision guard tripped (unexpected nonzero word) - inert
	u32 m_bq_repokes;              // guard re-pokes after re-materialization (expect 0)

	// P066 EXPERIMENT (branch patch/link-wait, off milestone/phase10-linkplay):
	// env-extendable PARTNER-SEARCH countdown - the ONE-WORD seed widen for the
	// boot "NETWORK CHECK" join window (two-PC WiFi sessions may need more than
	// the ROM's ~30 s to get both cabs up).  MODEL PROVENANCE: Fable 5.
	//
	// THE COUNTDOWN (static RE this session, every claim PC-anchored in
	// full.txt): boot clears the top-level screen state gp+0x756C
	// (@0x80000598), and the once-per-main-loop dispatcher 0x8000A370 (called
	// @0x80000650 inside the vblank-gated loop 0x800005E8..0x80000690) calls
	// table 0x8022A170[gp+0x756C]:
	//   state 0 = 0x80009A90: seeds the search countdown halfword gp+0x6F78
	//     and advances to state 1:
	//       80009AB8: 2402012C li $v0,0x12c   ; 300 - THE POKED WORD
	//       80009ABC: A7826F78 sh $v0,0x6f78($gp)
	//   state 1 = 0x80009AD8: decrements gp+0x6F78 once per dispatch and draws
	//     it (4-digit field, jal 0x8000173C @0x80009B18 - the on-screen "300");
	//     on expiry (<= 0) AND link board alive (gp+0x7790 != 0 - the A440
	//     comms-poll phase counter armed by the 0x7601 ID handshake in
	//     0x8009CF04) it resets and advances to state 2 = 0x8000A4CC, where the
	//     flow continues to the link-vs-solo decision from what discovery
	//     found.  The SAME gp+0x6F78 expiry is thus the sole trigger ending
	//     partner search - there is no separate solo-fallback timer on this
	//     screen - and one seed serves BOTH cabs' code path (same ROM, no role
	//     branch before the store).  gp+0x6F78 is referenced NOWHERE else
	//     (grepped full.txt: only 0x80009ABC seed / 0x80009AE0+AE8 tick /
	//     0x80009B08 draw reload); the other li 0x12C sites (0x80014EB8/EE8 ->
	//     gp+0x7018, 0x80018D60 -> gp+0x7098, 0x80020ABC -> gp+0x7110,
	//     0x800A0774 -> call arg) are DIFFERENT timers - no shared word, so the
	//     shared-seed REFUSE clause does not trigger.
	//
	// UNIT: one count per search-loop dispatch - nominally vblank-gated 60 Hz,
	// but the OBSERVED on-screen cadence is ~10 counts/s (the A440 comms poll
	// 0x8009CF04 busy-waits/strobes inside the same loop and slows it during
	// unanswered search), i.e. 300 counts ~= 30.0 s = tenths of seconds in
	// practice.  Conversion: NAMCOS23_PATCH_LINK_WAIT=<seconds> -> imm =
	// seconds*10 (value-checked 30..300 s -> imm 300..3000, always inside the
	// positive s16 range the signed lh/bgtz tick requires; anything else =
	// one-shot REFUSED banner at arm time; unset = inert/byte-identical).
	//
	// THE POKE: word @virtual 0x80009AB8 (program-space 0x00009ab8), VERIFIED
	// in full.txt before hardcoding:
	//   80009AB8: 2402012C li $v0,0x12c
	// The replacement is CONSTRUCTED from the verified word by changing ONLY
	// the 16-bit immediate field ((word & 0xFFFF0000) | units).  On any OTHER
	// nonzero word: one-shot REFUSE (ROM-revision guard).
	//
	// ESTABLISHMENT SAFETY (this runs pre-mode-2, the phase every experiment
	// protects): the poke changes ONLY the countdown seed VALUE - the tick /
	// expiry logic, the A440 handshake gate gp+0x7790 and the op55
	// establishment machinery are untouched.  BOTH cabs must run the SAME
	// value: each cab counts its own window, and a shorter-window cab exits
	// the search screen (state -> 2) earlier - a mismatch re-creates the very
	// early-fallback this patch exists to remove (called out in the banner).
	//
	// DRC soundness = the P040b/REAPER/BURST ordering class, with the family's
	// TIGHTEST margin: the seed first executes at the FIRST state-0 dispatch,
	// only the boot init chain (A440 resets + main-loop entry, a handful of
	// vblanks) after the loader copy - the early-boot vblank poke still
	// precedes it; mips3 has no store-triggered invalidation and namcos23 runs
	// loose verify, so a block compiled inside a residual re-copy window seeds
	// STOCK 300 = FAIL TO STOCK only (no corruption; the on-screen start value
	// IS the tell: 300 instead of units).  Soft reset flushes the whole DRC
	// cache (device_reset); the 1/s guard re-pokes on re-materialization.
	// Counters diagnostics-only, not save_item'd (converges: word == original
	// -> poke/re-poke path; word == patched -> nothing to do).  Experiment
	// branch only - never merge to milestone.
	int m_patch_link_wait_units;   // 0 = inert; else the widened seed in ROM units (= env seconds * 10)
	bool m_lw_poked;               // one-shot: the widened immediate is in RAM
	bool m_lw_refused;             // one-shot: ROM-revision guard tripped (unexpected nonzero word) - inert
	u32 m_lw_repokes;              // guard re-pokes after re-materialization (expect 0)

	const u32 *m_ptrom;
	u32 m_ptrom_limit;

	emu_timer *m_subcpu_scanline_on_timer;
	emu_timer *m_subcpu_scanline_off_timer;

	u16 m_absolute_priority;
	u16 m_tx;
	u16 m_ty;
	u16 m_model_blend_factor;
	u16 m_light_power;
	u16 m_light_ambient;
	float m_clip_data[8*3];
	u8 m_clip_data_line;
	s16 m_vp_size_x;
	s16 m_vp_size_y;
	s16 m_vp_offset_x;
	s16 m_vp_offset_y;

	// There may only be 128 matrix and vector slots.
	// At 0x1e bytes per slot, rounded up to 0x20, that's 0x1000 to 0x2000 bytes.
	// That fits pretty much anywhere, including inside an IC.
	// Unknown right now if it's directly CPU-accessible. Command packets via DMA are probably more efficient.
	s16 m_matrices[256][9]; // Matrices are stored in signed 2.14 fixed point
	s32 m_vectors[256][3];  // Vectors are stored in signed 10.14 fixed point
	s32 m_light_vector[3];
	u16 m_scaling;
	s32 m_spv[3];
	s16 m_spm[9];

	int m_c361_irqnum;
	int m_c422_irqnum;
	int m_c435_irqnum;
	int m_vbl_irqnum;
	int m_sub_irqnum;
	int m_rs232_irqnum;

	u8 m_sub_port8;
	u8 m_sub_porta;
	u8 m_sub_portb;
	output_finder<8> m_lamps;
};

class gorgon_state : public namcos23_state
{
public:
	gorgon_state(const machine_config &mconfig, device_type type, const char *tag) :
		namcos23_state(mconfig, type, tag),
		m_gfxdecode(*this, "gfxdecode")
	{
	}

	void gorgon(machine_config &config);
	void finfurl(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	virtual void video_start() override ATTR_COLD;

	void mips_map(address_map &map) ATTR_COLD;

	void nvram_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	u16 czattr_r(offs_t offset, u16 mem_mask = ~0);
	void czattr_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u16 czram_r(offs_t offset, u16 mem_mask = ~0);
	void czram_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	void c435_pio_mode_w(offs_t offset, u32 data, u32 mem_mask = ~0);

	void render_sprite(const namcos23_render_entry *re);
	void render_sprite_tile(u32 code_offset, const namcos23_render_entry *re, int row, int col);

	virtual u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect) override;
	virtual void dispatch_render_entry(const namcos23_render_entry *re) override;
	virtual void render_run(screen_device &screen, bitmap_rgb32 &bitmap) override;
	virtual void vblank(int state) override;

	void recalc_czram();

	required_device<gfxdecode_device> m_gfxdecode;

	std::unique_ptr<u16[]> m_banked_czram[4];
	std::unique_ptr<u8[]> m_recalc_czram[4];
	u32 m_cz_was_written[4];
};

class motoxgo_state : public namcos23_state
{
public:
	using namcos23_state::namcos23_state;

	void motoxgo(machine_config &config)
	{
		s23(config);
		m_jvs->set_default_option("namco_asca3a");
	}

protected:
	virtual void configure_jvs(device_jvs_interface &io) override
	{
		namcos23_state::configure_jvs(io);

		io.analog_input<4>().set_constant(0x200); // FREEZE/RELAY? < 0x200 = BANK ERR
		if (io.jvs())
		{
			io.jvs()->set_default_option("namco_amc");
			io.jvs()->set_option_machine_config("namco_amc", [](device_t *device)
			{
				auto &io = dynamic_cast<device_jvs_interface &>(*device);
				io.player<0>().set_constant(0);
				// motors?
				io.output().set_nop();
				io.analog_output<0>().set_nop();
				io.analog_output<1>().set_nop();
			});
		}
	}
};

class rapidrvr_state : public gorgon_state
{
public:
	rapidrvr_state(const machine_config &mconfig, device_type type, const char *tag) :
		gorgon_state(mconfig, type, tag),
		m_paddle(*this, "JVS_ANALOG_INPUT%u", 1U),
		m_output{},
		m_sensor{}
	{
	}

	void rapidrvr(machine_config &config)
	{
		gorgon(config);
		m_jvs->set_default_option("namco_asca1");
	}

protected:
	virtual void driver_start() override
	{
		gorgon_state::driver_start();

		m_sensor_timer = timer_alloc(FUNC(rapidrvr_state::sensor_timer_callback), this);
		m_sensor_timer->adjust(attotime::zero, 0, attotime::from_hz(200));

		save_item(NAME(m_sensor));
		save_item(NAME(m_output));
	}

	virtual void configure_jvs(device_jvs_interface &io) override
	{
		namcos23_state::configure_jvs(io);

		io.analog_input<0>().set(*this, FUNC(rapidrvr_state::paddle_r<0>)); // yaw
		io.analog_input<1>().set(*this, FUNC(rapidrvr_state::paddle_r<1>)); // pitch
		io.analog_input<2>().set(*this, FUNC(rapidrvr_state::sensor_r<0>)); // fr
		io.analog_input<3>().set(*this, FUNC(rapidrvr_state::sensor_r<1>)); // fl
		io.analog_input<4>().set(*this, FUNC(rapidrvr_state::sensor_r<2>)); // rr
		io.analog_input<5>().set(*this, FUNC(rapidrvr_state::sensor_r<3>)); // rl
		io.output().set(*this, FUNC(rapidrvr_state::output_w));
	}

	TIMER_CALLBACK_MEMBER(sensor_timer_callback)
	{
		for (int i = 0; i < 4; i++)
			if (BIT(m_output, 7 - i))
				m_sensor[i] = std::min(0x1100, m_sensor[i] + 0x280);
			else
				m_sensor[i] = std::max(0x40, m_sensor[i] - 0x280);
	}

	template<int N>
	uint16_t paddle_r()
	{
		return m_paddle[N]->read() - 0x8000;
	}

	template<int N>
	uint16_t sensor_r()
	{
		return (N == 1 || N == 2) ? -m_sensor[N] : m_sensor[N];
	}

	void output_w(offs_t offset, uint64_t data, uint64_t mem_mask)
	{
		COMBINE_DATA(&m_output);
	}

	emu_timer *m_sensor_timer;
	required_ioport_array<2> m_paddle;
	u64 m_output;
	u16 m_sensor[4];
};

class namcoss23_state : public namcos23_state
{
public:
	namcoss23_state(const machine_config &mconfig, device_type type, const char *tag) :
		namcos23_state(mconfig, type, tag)
	{ }

	void ss23(machine_config &config);
	void timecrs2v4a(machine_config &config);
	void _500gp(machine_config &config);
	void aking(machine_config &config);

protected:
	void mips_map(address_map &map) ATTR_COLD;

private:
	void c435_c361_ack_w(offs_t offset, u32 data);
};

class namcoss23_gmen_state : public namcoss23_state
{
public:
	namcoss23_gmen_state(const machine_config &mconfig, device_type type, const char *tag) :
		namcoss23_state(mconfig, type, tag),
		m_sh2(*this, "sh2"),
		m_vpx(*this, "vpx"),
		m_firewire(*this, "firewire"),
		m_sh2_shared(*this, "sh2_shared", 0x10000, ENDIANNESS_BIG),
		m_dsw(*this, "GMENDSW")
	{ }

	void gmen(machine_config &config);
	void gunwars(machine_config &config);
	void raceon(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	u32 sh2_trigger_r();
	u32 sh2_shared_r(offs_t offset, u32 mem_mask = ~0);
	void sh2_shared_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	u32 mips_to_sh2_shared_r(offs_t offset, u32 mem_mask = ~0);
	void mips_to_sh2_shared_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	u32 sh2_dsw_r(offs_t offset, u32 mem_mask = ~0);
	u32 mips_sh2_unk_r(offs_t offset, u32 mem_mask = ~0);
	u32 sh2_vpxstate_r(offs_t offset, u32 mem_mask = ~0);
	void sh2_vpxstate_w(offs_t offset, u32 data, u32 mem_mask = ~0);
	u32 sh2_unk6200000_r(offs_t offset, u32 mem_mask = ~0);
	u32 vpx_line_r(offs_t offset, u32 mem_mask = ~0);
	void vpx_i2c_sdao_w(int state);
	u8 vpx_i2c_r();
	void vpx_i2c_w(u8 data);

private:
	void mips_map(address_map &map) ATTR_COLD;
	void sh2_map(address_map &map) ATTR_COLD;

	required_device<sh7604_device> m_sh2;
	required_device<vpx3220a_device> m_vpx;
	required_device<md8412b_s23_device> m_firewire;
	memory_share_creator<u32> m_sh2_shared;
	required_ioport m_dsw;

	int m_vpx_sdao;
	u32 m_sh2_unk;
};

class finfurl2_state : public namcoss23_gmen_state
{
public:
	finfurl2_state(const machine_config &mconfig, device_type type, const char *tag) :
		namcoss23_gmen_state(mconfig, type, tag),
		m_handle(*this, "JVS_ANALOG_INPUT2")
	{
	}

	void finfurl2(machine_config &config)
	{
		gmen(config);
		m_jvs->set_default_option("namco_asca3a");
	}

protected:
	virtual void configure_jvs(device_jvs_interface &io) override
	{
		namcoss23_state::configure_jvs(io);
		io.analog_input<1>().set(*this, FUNC(finfurl2_state::handle_r));
	}

	uint16_t handle_r()
	{
		return m_handle->read() + 0xc000;
	}

	required_ioport m_handle;
};

class crszone_state : public namcoss23_state
{
public:
	crszone_state(const machine_config &mconfig, device_type type, const char *tag) :
		namcoss23_state(mconfig, type, tag),
		m_acia(*this, "acia")
	{ }

	void crszone(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;

	void mips_map(address_map &map) ATTR_COLD;

	virtual void irq_update(u32 cause) override;

	virtual u16 c417_status_r() override;

	void irq_vbl_ack_w(offs_t offset, u32 data);
	u32 irq_lv3_status_r();
	u32 irq_lv5_status_r();
	u32 irq_lv6_status_r();

	void acia_irq_w(int state);
	u8 acia_r(offs_t offset);
	void acia_w(offs_t offset, u8 data);

	u32 c450_irq_status_r(offs_t offset, u32 mem_mask);
	void c450_dma_addr_w(address_space &space, offs_t offset, u32 data, u32 mem_mask = ~0);
	void c450_dma_size_w(address_space &space, offs_t offset, u32 data, u32 mem_mask = ~0);

	required_device<acia6850_device> m_acia;

	int m_c450_irqnum;
	int m_c451_irqnum;
};

u8 namcos23_state::nthbyte(const u32 *pSource, int offs)
{
	pSource += offs/4;
	return (pSource[0]<<((offs&3)*8))>>24;
}



/***************************************************************************

  Video

***************************************************************************/

namcos23_renderer::namcos23_renderer(namcos23_state &state, const u16 *tmlrom, const u8 *tmhrom, const u8 *texrom, const u16 *texram,
		const u32 tileid_mask, const u32 tile_mask)
	: poly_manager<float, namcos23_render_data, 4>(state.machine()),
	m_state(state),
	m_tmrom_decoded(nullptr),
	m_texattr_decoded(nullptr),
	m_texrom(texrom),
	m_texram(texram),
	m_tileid_mask(tileid_mask),
	m_tile_mask(tile_mask)
{
	m_tmrom_decoded = std::make_unique<u32[]>((m_tileid_mask | 0xff) + 1);
	m_texattr_decoded = std::make_unique<u8[]>((m_tileid_mask | 0xff) + 1);
	for (u32 tileid = 0; tileid <= m_tileid_mask; tileid++)
	{
		u8 attr = tmhrom[tileid >> 1];
		if (tileid & 1)
			attr &= 15;
		else
			attr >>= 4;
		m_tmrom_decoded[tileid] = ((tmlrom[tileid] | ((attr & 1) << 16)) & m_tile_mask) << 8;
		m_texattr_decoded[tileid] = attr >> 1;
	}
}

// 3D hardware

s32 u32_to_s24(u32 v)
{
	return v & 0x800000 ? v | 0xff000000 : v & 0xffffff;
}

s32 u32_to_s10(u32 v)
{
	return v & 0x200 ? (0x3ff - (v & 0x3ff)) | 0xfffffe00 : v & 0x1ff;
}

float namcos23_state::f24_to_f32(u32 v)
{
	// 8 bits exponent, 16 mantissa
	// mantissa is 16-bits signed, 2-complement
	// value is m * 2**(e-46)
	// 1 is e=32, m=0x4000, -1 is e=31, m=0x8000

	// This code turns it into a standard float
	if (!(v & 0x0000ffff))
		return 0;

	u32 r = v & 0x8000 ? 0x80000000 : 0;
	u16 m = r ? -v : v;
	u8 e = (v >> 16) + 0x60;
	while(!(m & 0x8000))
	{
		m <<= 1;
		e--;
	}

	r = r | (e << 23) | ((m & 0x7fff) << 8);
	return *(float *)&r;
}

s32 *namcos23_state::c435_getv(u16 id)
{
	if (id == 0x8000)
		return m_light_vector;
	if (id >= 0x100)
	{
		memset(m_spv, 0, sizeof(m_spv));
		return m_spv;
	}
	return m_vectors[id];
}

s16 *namcos23_state::c435_getm(u16 id)
{
	if (id >= 0x100)
	{
		memset(m_spm, 0, sizeof(m_spm));
		return m_spm;
	}
	return m_matrices[id];
}

void namcos23_state::c435_state_set_interrupt(const u16 *param)
{
	if (param[0] & 1)
		irq_update(m_main_irqcause | MAIN_C435_IRQ);
	else
		irq_update(m_main_irqcause & ~MAIN_C435_IRQ);
}

void namcos23_state::c435_state_set_viewport_data(const u16 *param)
{
	const u32 vp_data_raw = ((u32)param[7] << 16) | param[8];
	const u16 vp_offset_x_raw = (u16)(vp_data_raw & 0x0fff);
	const u16 vp_offset_y_raw = (u16)((vp_data_raw >> 12) & 0x0fff);
	m_vp_offset_x = ((s16)(vp_offset_x_raw << 4)) >> 4;
	m_vp_offset_y = -(((s16)(vp_offset_y_raw << 4)) >> 4);
}

void namcos23_state::c435_state_set_clip_data_line(const u16 *param)
{
	// timecrs2:
	//   sx = 640/2, sy = 480/2, t = tan(fov/2) (fov=45 degrees)
	//   line 1: 1 0 -(sx-a)/(sx/t) 0 -1  0 -(sx+a)/(sx/t) 0
	//   line 2: 0 1 -(sy-b)/(sx/t) 0  0 -1 -(sy+b)/(sx/t) 0
	//   line 3: 0 0 -1             c  0  0              0 sx/t

	std::ostringstream buf;
	buf << "clip data line:";
	for (int i = 0; i < 8; i++)
	{
		util::stream_format(buf, " %f", f24_to_f32((param[2 * i + 1] << 16) | param[2 * i + 2]));
		m_clip_data[m_clip_data_line * 8 + i] = f24_to_f32((param[2 * i + 1] << 16) | param[2 * i + 2]);
	}
	m_clip_data_line = (m_clip_data_line + 1) % 3;
	buf << "\n";
	LOGMASKED(LOG_CLIP_DATA, "%s: %s", machine().describe_context(), std::move(buf).str());

	if (m_clip_data_line == 0 && m_clip_data[10] != 0.f)
	{
		m_vp_size_y = (s16)std::roundf(std::abs(m_clip_data[10]) * std::abs(m_clip_data[23]));
		m_vp_size_x = (s16)std::roundf((float)m_vp_size_y * std::abs(m_clip_data[2]) / std::abs(m_clip_data[10]));
	}

	std::ostringstream buf2;
	buf2 << "clip data line:";
	for (int i = 0; i < 8; i++)
	{
		util::stream_format(buf2, " %08x", (param[2 * i + 1] << 16) | param[2 * i + 2]);
	}
	buf2 << "\n";
	LOGMASKED(LOG_CLIP_DATA, "%s: %s", machine().describe_context(), std::move(buf2).str());
}

void namcos23_state::c435_state_reset_w(u16 data)
{
	m_c435.buffer_pos = 0;
}

void namcos23_state::c435_state_pio_w(u16 data)
{
	m_c435.buffer[m_c435.buffer_pos++] = data;
	int psize = c435_get_state_entry_size(m_c435.buffer[0]);
	if (m_c435.buffer_pos < psize+1)
		return;
	c435_state_set(m_c435.buffer[0], m_c435.buffer + 1);

	m_c435.buffer_pos = 0;
}

int namcos23_state::c435_get_state_entry_size(u16 type)
{
	constexpr int STATE_SIZES[0x100] =
	{
		43,  1, 53, -1, -1, -1, -1, -1, -1, 19, 47, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 41, -1, -1, -1, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		33, -1, -1, -1, -1, -1, 13, -1, 17, -1, -1, -1, -1, -1, -1, -1,
	};

	if (STATE_SIZES[type & 0xff] == -1)
	{
		LOGMASKED(LOG_3D_STATE_UNK, "%s: WARNING: Unknown size for state type %04x\n", machine().describe_context(), type);
	}
	return STATE_SIZES[type & 0xff];
}

static void transpose_matrix(s16 *m1)
{
	s16 m2[9];
	memcpy(m2, m1, sizeof(m2));
	m1[0] = m2[0];  m1[1] = m2[3];  m1[2] = m2[6];
	m1[3] = m2[1];  m1[4] = m2[4];  m1[5] = m2[7];
	m1[6] = m2[2];  m1[7] = m2[5];  m1[8] = m2[8];
}

void namcos23_state::c435_matrix_matrix_mul() // 0.0
{
	if ((m_c435.buffer[0] & 0xf) != 4)
	{
		LOGMASKED(LOG_MATRIX_ERR, "%s: WARNING: c435_matrix_matrix_mul with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}
	bool transpose = BIT(m_c435.buffer[0], 10);
	bool identity = BIT(m_c435.buffer[0], 8);
	if ((m_c435.buffer[0] & ~0x500) != 0x0004)
		LOGMASKED(LOG_MATRIX_UNK, "%s: WARNING: c435_matrix_matrix_mul header %04x\n", machine().describe_context(), m_c435.buffer[0]);
	if (m_c435.buffer[3] != 0xffff)
		LOGMASKED(LOG_MATRIX_UNK, "%s: WARNING: c435_matrix_matrix_mul with +2=%04x\n", machine().describe_context(), m_c435.buffer[3]);

	LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_matrix_mul (%04x): Matrix %d = Matrix %d * Matrix %d\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[4]);

	s16 *t = c435_getm(m_c435.buffer[1]);
	s16 m1[9];
	s16 m2[9];

	memcpy(m2, c435_getm(m_c435.buffer[2]), sizeof(s16) * 9);
	if (transpose)
		transpose_matrix(m2);
	if (identity)
	{
		memset(m1, 0, sizeof(s16) * 9);
		m1[0] = m1[4] = m1[8] = 0x4000;
	}
	else
	{
		memcpy(m1, c435_getm(m_c435.buffer[4]), sizeof(s16) * 9);
	}
	t[0] = s16((m1[0] * m2[0] + m1[1] * m2[1] + m1[2] * m2[2]) >> 14);
	t[1] = s16((m1[0] * m2[3] + m1[1] * m2[4] + m1[2] * m2[5]) >> 14);
	t[2] = s16((m1[0] * m2[6] + m1[1] * m2[7] + m1[2] * m2[8]) >> 14);
	t[3] = s16((m1[3] * m2[0] + m1[4] * m2[1] + m1[5] * m2[2]) >> 14);
	t[4] = s16((m1[3] * m2[3] + m1[4] * m2[4] + m1[5] * m2[5]) >> 14);
	t[5] = s16((m1[3] * m2[6] + m1[4] * m2[7] + m1[5] * m2[8]) >> 14);
	t[6] = s16((m1[6] * m2[0] + m1[7] * m2[1] + m1[8] * m2[2]) >> 14);
	t[7] = s16((m1[6] * m2[3] + m1[7] * m2[4] + m1[8] * m2[5]) >> 14);
	t[8] = s16((m1[6] * m2[6] + m1[7] * m2[7] + m1[8] * m2[8]) >> 14);

	LOGMASKED(LOG_MATRIX_INFO, "result: %04x    %04x    %04x\n", (u16)t[0], (u16)t[1], (u16)t[2]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n", (u16)t[3], (u16)t[4], (u16)t[5]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n", (u16)t[6], (u16)t[7], (u16)t[8]);
}

void namcos23_state::c435_matrix_vector_mul() // 0.1
{
	if ((m_c435.buffer[0] & 0xf) != 4)
	{
		LOGMASKED(LOG_VEC_ERR, "%s: WARNING: c435_matrix_vector_mul with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}

	bool extra_logging = false;
	if (m_c435.buffer[0] != 0x0814 && m_c435.buffer[0] != 0x1014 && m_c435.buffer[0] != 0x0414)
	{
		LOGMASKED(LOG_VEC_UNK, "%s: WARNING: c435_matrix_vector_mul header %04x %04x %04x %04x %04x\n", machine().describe_context(), m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[3], m_c435.buffer[4]);
		extra_logging = true;
	}

	s32 *t = c435_getv(m_c435.buffer[1]);
	s16 m[9];
	s32 v[3];
	memcpy(m, c435_getm(m_c435.buffer[2]), sizeof(s16) * 9);
	memcpy(v, c435_getv(m_c435.buffer[4]), sizeof(s32) * 3);

	if (BIT(m_c435.buffer[0], 10))
		transpose_matrix(m);

	if (m_c435.buffer[3] != 0xffff)
	{
		s32 vt[3];
		memcpy(vt, c435_getv(m_c435.buffer[3]), sizeof(s32) * 3);

		LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_vector_mul (%04x): Vector %d = Matrix %d * Vector %d + Vector %d\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[4], m_c435.buffer[3]);
		if (BIT(m_c435.buffer[0], 12))
		{
			t[0] = s32((m[0] * s64(v[0]) + m[1] * s64(v[1]) + m[2] * s64(v[2])) >> 14) - vt[0];
			t[1] = s32((m[3] * s64(v[0]) + m[4] * s64(v[1]) + m[5] * s64(v[2])) >> 14) - vt[1];
			t[2] = s32((m[6] * s64(v[0]) + m[7] * s64(v[1]) + m[8] * s64(v[2])) >> 14) - vt[2];
		}
		else
		{
			t[0] = s32((m[0] * s64(v[0]) + m[1] * s64(v[1]) + m[2] * s64(v[2])) >> 14) + vt[0];
			t[1] = s32((m[3] * s64(v[0]) + m[4] * s64(v[1]) + m[5] * s64(v[2])) >> 14) + vt[1];
			t[2] = s32((m[6] * s64(v[0]) + m[7] * s64(v[1]) + m[8] * s64(v[2])) >> 14) + vt[2];
		}
		if (extra_logging)
		{
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x   %08x\n", m[0], m[1], m[2], (u32)v[0], (u32)vt[0]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x * %08x + %08x\n", m[3], m[4], m[5], (u32)v[1], (u32)vt[1]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x   %08x\n", m[6], m[7], m[8], (u32)v[2], (u32)vt[2]);
		}
		LOGMASKED(LOG_MATRIX_INFO, "result: %08x %08x %08x\n", (u32)t[0], (u32)t[1], (u32)t[2]);
	}
	else
	{
		LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_vector_mul (%04x): Vector %d = Matrix %d * Vector %d\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[4]);

		t[0] = s32((m[0]*s64(v[0]) + m[1]*s64(v[1]) + m[2]*s64(v[2])) >> 14);
		t[1] = s32((m[3]*s64(v[0]) + m[4]*s64(v[1]) + m[5]*s64(v[2])) >> 14);
		t[2] = s32((m[6]*s64(v[0]) + m[7]*s64(v[1]) + m[8]*s64(v[2])) >> 14);

		if (extra_logging)
		{
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x\n", m[0], m[1], m[2], (u32)v[0]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x * %08x\n", m[3], m[4], m[5], (u32)v[1]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x\n", m[6], m[7], m[8], (u32)v[2]);
		}
		LOGMASKED(LOG_MATRIX_INFO, "result: %08x %08x %08x\n", (u32)t[0], (u32)t[1], (u32)t[2]);
	}
}

void namcos23_state::c435_matrix_matrix_immed_mul() // 0.2
{
	if ((m_c435.buffer[0] & 0xf) != 12)
	{
		LOGMASKED(LOG_MATRIX_ERR, "%s: WARNING: c435_matrix_matrix_immed_mul with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}
	bool transpose = BIT(m_c435.buffer[0], 10);
	if ((m_c435.buffer[0] & ~0x400) != 0x000c)
		LOGMASKED(LOG_MATRIX_UNK, "%s: WARNING: c435_matrix_matrix_immed_mul header %04x\n", machine().describe_context(), m_c435.buffer[0]);
	if (m_c435.buffer[3] != 0xffff)
		LOGMASKED(LOG_MATRIX_UNK, "%s: WARNING: c435_matrix_matrix_immed_mul with +2=%04x\n", machine().describe_context(), m_c435.buffer[3]);

	LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_matrix_immed_mul (%04x): Matrix %d = Immediate Matrix * Matrix %d\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2]);
	LOGMASKED(LOG_MATRIX_INFO, "    %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
		m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[3],
		m_c435.buffer[4], m_c435.buffer[5], m_c435.buffer[6],
		m_c435.buffer[7], m_c435.buffer[8], m_c435.buffer[9],
		m_c435.buffer[10], m_c435.buffer[11], m_c435.buffer[12]);

	s16 *t = c435_getm(m_c435.buffer[1]);
	s16 m1[9];
	s16 m2[9];

	memcpy(m1, &m_c435.buffer[4], sizeof(s16) * 9);
	memcpy(m2, c435_getm(m_c435.buffer[2]), sizeof(s16) * 9);
	LOGMASKED(LOG_MATRIX_INFO, "    m1: %04x    %04x    %04x\n",   (u16)m1[0], (u16)m1[1], (u16)m1[2]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n",   (u16)m1[3], (u16)m1[4], (u16)m1[5]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n\n", (u16)m1[6], (u16)m1[7], (u16)m1[8]);
	LOGMASKED(LOG_MATRIX_INFO, "    m2: %04x    %04x    %04x\n",   (u16)m2[0], (u16)m2[1], (u16)m2[2]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n",   (u16)m2[3], (u16)m2[4], (u16)m2[5]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n\n", (u16)m2[6], (u16)m2[7], (u16)m2[8]);

	if (transpose)
		transpose_matrix(m2);
	t[0] = s16((m1[0] * m2[0] + m1[1] * m2[3] + m1[2] * m2[6]) >> 14);
	t[1] = s16((m1[0] * m2[1] + m1[1] * m2[4] + m1[2] * m2[7]) >> 14);
	t[2] = s16((m1[0] * m2[2] + m1[1] * m2[5] + m1[2] * m2[8]) >> 14);

	t[3] = s16((m1[3] * m2[0] + m1[4] * m2[3] + m1[5] * m2[6]) >> 14);
	t[4] = s16((m1[3] * m2[1] + m1[4] * m2[4] + m1[5] * m2[7]) >> 14);
	t[5] = s16((m1[3] * m2[2] + m1[4] * m2[5] + m1[5] * m2[8]) >> 14);

	t[6] = s16((m1[6] * m2[0] + m1[7] * m2[3] + m1[8] * m2[6]) >> 14);
	t[7] = s16((m1[6] * m2[1] + m1[7] * m2[4] + m1[8] * m2[7]) >> 14);
	t[8] = s16((m1[6] * m2[2] + m1[7] * m2[5] + m1[8] * m2[8]) >> 14);

	LOGMASKED(LOG_MATRIX_INFO, "result: %04x    %04x    %04x\n", (u16)t[0], (u16)t[1], (u16)t[2]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n", (u16)t[3], (u16)t[4], (u16)t[5]);
	LOGMASKED(LOG_MATRIX_INFO, "        %04x    %04x    %04x\n", (u16)t[6], (u16)t[7], (u16)t[8]);
}

void namcos23_state::c435_matrix_vector_immed_mul() // 0.3
{
	if ((m_c435.buffer[0] & 0xf) != 9)
	{
		LOGMASKED(LOG_VEC_ERR, "%s: WARNING: c435_matrix_vector_immed_mul with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}

	bool extra_logging = false;
	if (m_c435.buffer[0] != 0x0839 && m_c435.buffer[0] != 0x1039 && m_c435.buffer[0] != 0x0439)
	{
		LOGMASKED(LOG_VEC_UNK, "%s: WARNING: c435_matrix_vector_immed_mul header %04x %04x %04x %04x\n", machine().describe_context(), m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[3]);
		extra_logging = true;
	}

	s32 *t = c435_getv(m_c435.buffer[1]);
	s16 m[9];
	s32 v[3];
	memcpy(m, c435_getm(m_c435.buffer[2]), sizeof(s16) * 9);
	v[0] = (s32(m_c435.buffer[4]) << 16) | u16(m_c435.buffer[5]);
	v[1] = (s32(m_c435.buffer[6]) << 16) | u16(m_c435.buffer[7]);
	v[2] = (s32(m_c435.buffer[8]) << 16) | u16(m_c435.buffer[9]);

	if (BIT(m_c435.buffer[0], 10))
		transpose_matrix(m);

	if (m_c435.buffer[3] != 0xffff)
	{
		s32 vt[3];
		memcpy(vt, c435_getv(m_c435.buffer[3]), sizeof(s32) * 3);

		LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_vector_immed_mul (%04x): Vector %d = Matrix %d * Immediate Vector + Vector %d\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[3]);
		if (BIT(m_c435.buffer[0], 12))
		{
			t[0] = s32((m[0] * s64(v[0]) + m[1] * s64(v[1]) + m[2] * s64(v[2])) >> 14) - vt[0];
			t[1] = s32((m[3] * s64(v[0]) + m[4] * s64(v[1]) + m[5] * s64(v[2])) >> 14) - vt[1];
			t[2] = s32((m[6] * s64(v[0]) + m[7] * s64(v[1]) + m[8] * s64(v[2])) >> 14) - vt[2];
		}
		else
		{
			t[0] = s32((m[0] * s64(v[0]) + m[1] * s64(v[1]) + m[2] * s64(v[2])) >> 14) + vt[0];
			t[1] = s32((m[3] * s64(v[0]) + m[4] * s64(v[1]) + m[5] * s64(v[2])) >> 14) + vt[1];
			t[2] = s32((m[6] * s64(v[0]) + m[7] * s64(v[1]) + m[8] * s64(v[2])) >> 14) + vt[2];
		}
		if (extra_logging)
		{
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x   %08x\n", m[0], m[1], m[2], (u32)v[0], (u32)vt[0]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x * %08x + %08x\n", m[3], m[4], m[5], (u32)v[1], (u32)vt[1]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x   %08x\n", m[6], m[7], m[8], (u32)v[2], (u32)vt[2]);
		}
		LOGMASKED(LOG_MATRIX_INFO, "result: %08x %08x %08x\n", (u32)t[0], (u32)t[1], (u32)t[2]);
	}
	else
	{
		LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_vector_immed_mul (%04x): Vector %d = Matrix %d * Immediate Vector\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2]);

		t[0] = s32((m[0] * s64(v[0]) + m[1] * s64(v[1]) + m[2] * s64(v[2])) >> 14);
		t[1] = s32((m[3] * s64(v[0]) + m[4] * s64(v[1]) + m[5] * s64(v[2])) >> 14);
		t[2] = s32((m[6] * s64(v[0]) + m[7] * s64(v[1]) + m[8] * s64(v[2])) >> 14);

		if (extra_logging)
		{
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x\n", m[0], m[1], m[2], (u32)v[0]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x * %08x\n", m[3], m[4], m[5], (u32)v[1]);
			LOGMASKED(LOG_MATRIX_UNK, "    %04x %04x %04x   %08x\n", m[6], m[7], m[8], (u32)v[2]);
		}
		LOGMASKED(LOG_MATRIX_INFO, "result: %08x %08x %08x\n", (u32)t[0], (u32)t[1], (u32)t[2]);
	}
}

void namcos23_state::c435_matrix_set() // 0.4
{
	if ((m_c435.buffer[0] & 0xf) != 10)
	{
		LOGMASKED(LOG_MATRIX_ERR, "%s: WARNING: c435_matrix_set with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}

	if (m_c435.buffer[0] != 0x004a)
		LOGMASKED(LOG_MATRIX_UNK, "%s: WARNING: c435_matrix_set header %04x\n", machine().describe_context(), m_c435.buffer[0]);

	s16 *t = c435_getm(m_c435.buffer[1]);
	for (int i = 0; i < 9; i++)
		t[i] = m_c435.buffer[i+2];
	LOGMASKED(LOG_MATRIX_INFO, "c435_matrix_set (%04x): Matrix %d:\n", m_c435.buffer[0], m_c435.buffer[1]);
	LOGMASKED(LOG_MATRIX_INFO, "    %04x %04x %04x\n", t[0], t[1], t[2]);
	LOGMASKED(LOG_MATRIX_INFO, "    %04x %04x %04x\n", t[3], t[4], t[5]);
	LOGMASKED(LOG_MATRIX_INFO, "    %04x %04x %04x\n", t[6], t[7], t[8]);
}

void namcos23_state::c435_vector_set() // 0.5
{
	if ((m_c435.buffer[0] & 0xf) != 7)
	{
		LOGMASKED(LOG_VEC_ERR, "%s: WARNING: c435_vector_set with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}
	if (m_c435.buffer[0] != 0x057)
		LOGMASKED(LOG_VEC_UNK, "%s: WARNING: c435_vector_set header %04x\n", machine().describe_context(), m_c435.buffer[0]);

	s32 *t = c435_getv(m_c435.buffer[1]);
	for (int i = 0; i < 3; i++)
		t[i] = u32_to_s24((m_c435.buffer[2 * i + 2] << 16) | m_c435.buffer[2 * i + 3]);

	LOGMASKED(LOG_VEC_INFO, "c435_vector_set (%04x): Vector %d = %08x %08x %08x\n", m_c435.buffer[0], m_c435.buffer[1], t[0], t[1], t[2]);
}

void namcos23_state::c435_state_set(u16 type, const u16 *param)
{
	LOGMASKED(LOG_3D_STATE_UNK, "%s: c435_state_set, type %04x, header %04x\n", machine().describe_context(), type, m_c435.buffer[0]);
	switch (type)
	{
	case 0x0000:
	{
		render_t &render = m_render;
		namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
		re->type = IMMEDIATE;
		re->poly_fade_r = m_c404.poly_fade_r;
		re->poly_fade_g = m_c404.poly_fade_g;
		re->poly_fade_b = m_c404.poly_fade_b;
		re->poly_alpha_color = m_c404.poly_alpha_color;
		re->poly_alpha_pen = m_c404.poly_alpha_pen;
		re->poly_alpha = m_c404.poly_alpha;
		re->screen_fade_r = m_c404.screen_fade_r;
		re->screen_fade_g = m_c404.screen_fade_g;
		re->screen_fade_b = m_c404.screen_fade_b;
		re->screen_fade_factor = m_c404.screen_fade_factor;
		re->fade_flags = m_c404.fade_flags;
		re->absolute_priority = m_absolute_priority;
		re->model_blend_factor = 0;
		re->tx = 0;
		re->ty = 0;
		re->light_power = m_light_power;
		re->light_ambient = m_light_ambient;
		re->vp_size_x = m_vp_size_x;
		re->vp_size_y = m_vp_size_y;
		re->vp_offset_x = m_vp_offset_x;
		re->vp_offset_y = m_vp_offset_y;
		re->vp_fov = m_clip_data[23];
		if (m_c435.buffer[0] == 0x4f38)
		{
			re->immediate.type  =  param[ 0];
			re->immediate.h     = (param[ 1] << 16) | param[ 2];
			re->immediate.pal   = (param[ 3] << 16) | param[ 4];
			re->immediate.zbias = (param[ 5] << 16) | param[ 6];
			re->immediate.i[0]  = (param[ 7] << 16) | param[ 8];
			re->immediate.i[1]  = (param[ 9] << 16) | param[10];
			re->immediate.i[2]  = (param[11] << 16) | param[12];
			re->immediate.i[3]  = (param[13] << 16) | param[14];
			re->immediate.u[0]  = (param[15] << 16) | param[16];
			re->immediate.v[0]  = (param[17] << 16) | param[18];
			re->immediate.u[1]  = (param[19] << 16) | param[20];
			re->immediate.v[1]  = (param[21] << 16) | param[22];
			re->immediate.u[2]  = (param[23] << 16) | param[24];
			re->immediate.v[2]  = (param[25] << 16) | param[26];
			re->immediate.u[3]  = (param[27] << 16) | param[28];
			re->immediate.v[3]  = (param[29] << 16) | param[30];
			re->immediate.x[0]  = (param[31] << 16) | param[32];
			re->immediate.y[0]  = (param[33] << 16) | param[34];
			re->immediate.z[0]  = (param[35] << 16) | param[36];
			re->immediate.x[1]  = (param[37] << 16) | param[38];
			re->immediate.y[1]  = (param[39] << 16) | param[40];
			re->immediate.z[1]  = (param[41] << 16) | param[42];
			re->immediate.x[2]  = (param[43] << 16) | param[44];
			re->immediate.y[2]  = (param[45] << 16) | param[46];
			re->immediate.z[2]  = (param[47] << 16) | param[48];
			re->immediate.x[3]  = (param[49] << 16) | param[50];
			re->immediate.y[3]  = (param[51] << 16) | param[52];
			re->immediate.z[3]  = (param[53] << 16) | param[54];
		}
		else
		{
			re->immediate.type  =  param[ 0];
			re->immediate.h     = (param[ 1] << 16) | param[ 2];
			re->immediate.pal   = (param[ 3] << 16) | param[ 4];
			re->immediate.zbias = (param[ 5] << 16) | param[ 6];
			re->immediate.i[0]  = (param[ 7] << 16) | param[ 8];
			re->immediate.i[1]  = (param[ 9] << 16) | param[10];
			re->immediate.i[2]  = (param[11] << 16) | param[12];
			re->immediate.u[0]  = (param[13] << 16) | param[14];
			re->immediate.v[0]  = (param[15] << 16) | param[16];
			re->immediate.u[1]  = (param[17] << 16) | param[18];
			re->immediate.v[1]  = (param[19] << 16) | param[20];
			re->immediate.u[2]  = (param[21] << 16) | param[22];
			re->immediate.v[2]  = (param[23] << 16) | param[24];
			re->immediate.x[0]  = (param[25] << 16) | param[26];
			re->immediate.y[0]  = (param[27] << 16) | param[28];
			re->immediate.z[0]  = (param[29] << 16) | param[30];
			re->immediate.x[1]  = (param[31] << 16) | param[32];
			re->immediate.y[1]  = (param[33] << 16) | param[34];
			re->immediate.z[1]  = (param[35] << 16) | param[36];
			re->immediate.x[2]  = (param[37] << 16) | param[38];
			re->immediate.y[2]  = (param[39] << 16) | param[40];
			re->immediate.z[2]  = (param[41] << 16) | param[42];
		}
		render.count[render.cur]++;
		break;
	}
	case 0x0002:
	{
		render_t &render = m_render;
		namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
		re->poly_fade_r = m_c404.poly_fade_r;
		re->poly_fade_g = m_c404.poly_fade_g;
		re->poly_fade_b = m_c404.poly_fade_b;
		re->poly_alpha_color = m_c404.poly_alpha_color;
		re->poly_alpha_pen = m_c404.poly_alpha_pen;
		re->poly_alpha = m_c404.poly_alpha;
		re->screen_fade_r = m_c404.screen_fade_r;
		re->screen_fade_g = m_c404.screen_fade_g;
		re->screen_fade_b = m_c404.screen_fade_b;
		re->screen_fade_factor = m_c404.screen_fade_factor;
		re->fade_flags = m_c404.fade_flags;
		re->type = IMMEDIATE;
		re->absolute_priority = m_absolute_priority;
		re->model_blend_factor = 0;
		re->tx = 0;
		re->ty = 0;
		re->light_power = m_light_power;
		re->light_ambient = m_light_ambient;
		re->vp_size_x = m_vp_size_x;
		re->vp_size_y = m_vp_size_y;
		re->vp_offset_x = m_vp_offset_x;
		re->vp_offset_y = m_vp_offset_y;
		re->vp_fov = m_clip_data[23];
		/*
		3-e0: 1110 0000, has shade+tex+pos
		3-a0: 1010 0000, has tex+pos
		[:] Word 00: 000a
		[:] Word 01: 34a0       type
		[:] Word 02: 00003242   h
		[:] Word 04: 0040ff00   pal?
		[:] Word 06: 00000000   zbias?
		[:] Word 08: 0000000f   u0
		[:] Word 0a: 0000060f   v0
		[:] Word 0c: 0000000f   u1
		[:] Word 0e: 00000600   v1
		[:] Word 10: 00000000   u2
		[:] Word 12: 00000600   v2
		[:] Word 14: 00000000   u3
		[:] Word 16: 0000060f   v3
		[:] Word 18: 000000c8   x0
		[:] Word 1a: ffffff38   y0
		[:] Word 1c: 00000500   z0
		[:] Word 1e: ffffff38   x1
		[:] Word 20: ffffff38   y1
		[:] Word 22: 00000500   z1
		[:] Word 24: ffffff38   x2
		[:] Word 26: 000000c8   y2
		[:] Word 28: 00000500   z2
		[:] Word 2a: 000000c8   x3
		[:] Word 2c: 000000c8   y3
		[:] Word 2e: 00000500   z3
		*/
		re->immediate.type  =  param[ 0];
		re->immediate.h     = (param[ 1] << 16) | param[ 2];
		re->immediate.pal   = (param[ 3] << 16) | param[ 4];
		re->immediate.zbias = 0;
		re->immediate.i[0]  = (param[ 5] << 16) | param[ 6];
		re->immediate.i[1]  = (param[ 7] << 16) | param[ 8];
		re->immediate.i[2]  = (param[ 9] << 16) | param[10];
		re->immediate.i[3]  = (param[11] << 16) | param[12];
		re->immediate.u[0]  = (param[13] << 16) | param[14];
		re->immediate.v[0]  = (param[15] << 16) | param[16];
		re->immediate.u[1]  = (param[17] << 16) | param[18];
		re->immediate.v[1]  = (param[19] << 16) | param[20];
		re->immediate.u[2]  = (param[21] << 16) | param[22];
		re->immediate.v[2]  = (param[23] << 16) | param[24];
		re->immediate.u[3]  = (param[25] << 16) | param[26];
		re->immediate.v[3]  = (param[27] << 16) | param[28];
		re->immediate.x[0]  = (param[29] << 16) | param[30];
		re->immediate.y[0]  = (param[31] << 16) | param[32];
		re->immediate.z[0]  = (param[33] << 16) | param[34];
		re->immediate.x[1]  = (param[35] << 16) | param[36];
		re->immediate.y[1]  = (param[37] << 16) | param[38];
		re->immediate.z[1]  = (param[39] << 16) | param[40];
		re->immediate.x[2]  = (param[41] << 16) | param[42];
		re->immediate.y[2]  = (param[43] << 16) | param[44];
		re->immediate.z[2]  = (param[45] << 16) | param[46];
		re->immediate.x[3]  = (param[47] << 16) | param[48];
		re->immediate.y[3]  = (param[49] << 16) | param[50];
		re->immediate.z[3]  = (param[51] << 16) | param[52];
		render.count[render.cur]++;
		break;
	}
	case 0x000a:
	{
		render_t &render = m_render;
		namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
		re->poly_fade_r = m_c404.poly_fade_r;
		re->poly_fade_g = m_c404.poly_fade_g;
		re->poly_fade_b = m_c404.poly_fade_b;
		re->poly_alpha_color = m_c404.poly_alpha_color;
		re->poly_alpha_pen = m_c404.poly_alpha_pen;
		re->poly_alpha = m_c404.poly_alpha;
		re->screen_fade_r = m_c404.screen_fade_r;
		re->screen_fade_g = m_c404.screen_fade_g;
		re->screen_fade_b = m_c404.screen_fade_b;
		re->screen_fade_factor = m_c404.screen_fade_factor;
		re->fade_flags = m_c404.fade_flags;
		re->type = IMMEDIATE;
		re->absolute_priority = m_absolute_priority;
		re->model_blend_factor = 0;
		re->tx = 0;
		re->ty = 0;
		re->light_power = m_light_power;
		re->light_ambient = m_light_ambient;
		re->vp_size_x = m_vp_size_x;
		re->vp_size_y = m_vp_size_y;
		re->vp_offset_x = m_vp_offset_x;
		re->vp_offset_y = m_vp_offset_y;
		re->vp_fov = m_clip_data[23];
		re->immediate.type  =  param[ 0];
		re->immediate.h     = (param[ 1] << 16) | param[ 2];
		re->immediate.pal   = (param[ 3] << 16) | param[ 4];
		re->immediate.zbias = (param[ 5] << 16) | param[ 6];
		re->immediate.u[0]  = (param[ 7] << 16) | param[ 8];
		re->immediate.v[0]  = (param[ 9] << 16) | param[10];
		re->immediate.u[1]  = (param[11] << 16) | param[12];
		re->immediate.v[1]  = (param[13] << 16) | param[14];
		re->immediate.u[2]  = (param[15] << 16) | param[16];
		re->immediate.v[2]  = (param[17] << 16) | param[18];
		re->immediate.u[3]  = (param[19] << 16) | param[20];
		re->immediate.v[3]  = (param[21] << 16) | param[22];
		re->immediate.x[0]  = (param[23] << 16) | param[24];
		re->immediate.y[0]  = (param[25] << 16) | param[26];
		re->immediate.z[0]  = (param[27] << 16) | param[28];
		re->immediate.x[1]  = (param[29] << 16) | param[30];
		re->immediate.y[1]  = (param[31] << 16) | param[32];
		re->immediate.z[1]  = (param[33] << 16) | param[34];
		re->immediate.x[2]  = (param[35] << 16) | param[36];
		re->immediate.y[2]  = (param[37] << 16) | param[38];
		re->immediate.z[2]  = (param[39] << 16) | param[40];
		re->immediate.x[3]  = (param[41] << 16) | param[42];
		re->immediate.y[3]  = (param[43] << 16) | param[44];
		re->immediate.z[3]  = (param[45] << 16) | param[46];
		render.count[render.cur]++;
		break;
	}
	case 0x0001:
		c435_state_set_interrupt(param);
		break;
	case 0x0036:
		LOGMASKED(LOG_3D_STATE_UNK, "%s: unknown state set (%04x)\n", machine().describe_context(), m_c435.buffer[0]);
		for (int i = 0; i < (m_c435.buffer[0] & 0xff); i++)
			LOGMASKED(LOG_3D_STATE_UNK, "%s: Word %02x: %04x\n", machine().describe_context(), i, m_c435.buffer[1 + i]);
		break;
	case 0x0046:
		c435_state_set_viewport_data(param);
		break;
	case 0x00c8:
		c435_state_set_clip_data_line(param);
		break;
	default:
		LOGMASKED(LOG_3D_STATE_UNK, "%s: unknown state type (%04x, %04x)\n", machine().describe_context(), m_c435.buffer[0], m_c435.buffer[1]);
		for (int i = 0; i < (m_c435.buffer[0] & 0xff); i++)
			LOGMASKED(LOG_3D_STATE_UNK, "%s: Word %02x: %04x\n", machine().describe_context(), i, m_c435.buffer[1 + i]);
		break;
	}
}

void namcos23_state::c435_state_set() // 4.f
{
	if ((m_c435.buffer[0] & 0xff) == 0)
	{
		LOGMASKED(LOG_3D_STATE_ERR, "%s: WARNING: c435_state_set with zero size\n", machine().describe_context());
		return;
	}
	int size = c435_get_state_entry_size(m_c435.buffer[1]);
	if (size != ((m_c435.buffer[0] & 0xff) - 1) && m_c435.buffer[0] != 0x4f38)
	{
		LOGMASKED(LOG_3D_STATE_ERR, "%s: WARNING: c435_state_set size disagreement (type=%04x, got %d, expected %d)\n", machine().describe_context(), m_c435.buffer[1], (m_c435.buffer[0] & 0xff) - 1, size);
		LOGMASKED(LOG_3D_STATE_ERR, "Header: %04x\n", m_c435.buffer[0]);
		for (int i = 0; i < (m_c435.buffer[0] & 0xff); i++)
		{
			LOGMASKED(LOG_3D_STATE_ERR, "Word %02x: %04x\n", i, m_c435.buffer[i+1]);
		}
		return;
	}

	c435_state_set(m_c435.buffer[1], m_c435.buffer + 2);
}

void namcos23_state::c435_unk_set0() // 4.0
{
	LOGMASKED(LOG_3D_STATE_UNK, "%s: Unknown state-set type 40xx: %04x %04x\n", machine().describe_context(), m_c435.buffer[0], m_c435.buffer[1]);
}

void namcos23_state::c435_absolute_priority_set() // 4.1
{
	m_absolute_priority = m_c435.buffer[1];
}

void namcos23_state::c435_tx_set() // 4.2
{
	m_tx = m_c435.buffer[1];
}

void namcos23_state::c435_ty_set() // 4.3
{
	m_ty = m_c435.buffer[1];
}

void namcos23_state::c435_scaling_set() // 4.4
{
	if ((m_c435.buffer[0] & 0xff) != 1)
	{
		LOGMASKED(LOG_VEC_ERR, "%s: WARNING: c435_scaling_set with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xff);
		return;
	}

	LOGMASKED(LOG_MATRIX_INFO, "c435_scaling_set (%04x): %04x\n", m_c435.buffer[0], m_c435.buffer[1]);
	m_scaling = m_c435.buffer[1];
}

void namcos23_state::c435_model_blend_factor_set() // 4.5
{
	m_model_blend_factor = m_c435.buffer[1];
}

void namcos23_state::c435_unk_set6() // 4.6
{
	LOGMASKED(LOG_3D_STATE_UNK, "%s: Unknown state-set type 46xx: %04x %04x\n", machine().describe_context(), m_c435.buffer[0], m_c435.buffer[1]);
}

void namcos23_state::c435_light_power_set() // 4.7
{
	m_light_power = m_c435.buffer[1];
}

void namcos23_state::c435_light_ambient_set() // 4.8
{
	m_light_ambient = m_c435.buffer[1];
}

void namcos23_state::c435_render() // 8
{
	const int size = m_c435.buffer[0] & 0xf;
	if (size != 3)
		LOGMASKED(LOG_RENDER_ERR, "%04x %04x %04x %04x %04x\n", m_c435.buffer[0], m_c435.buffer[1], m_c435.buffer[2], m_c435.buffer[3], m_c435.buffer[4]);

	if (m_c435.buffer[1] == 0)
	{
		irq_update(m_main_irqcause | MAIN_C435_IRQ);
		return;
	}

	render_t &render = m_render;
	const bool mirror_x = BIT(m_c435.buffer[0], 13);
	const bool scroll = BIT(m_c435.buffer[0], 9);
	const bool use_scaling = BIT(m_c435.buffer[0], 7);
	const bool transpose = BIT(m_c435.buffer[0], 6);

	if (render.count[render.cur] >= RENDER_MAX_ENTRIES)
	{
		LOGMASKED(LOG_RENDER_ERR, "%s: WARNING: render buffer full\n", machine().describe_context());
		return;
	}

	const s16 *m = c435_getm(m_c435.buffer[size - 1]);
	const s32 *v = c435_getv(m_c435.buffer[size]);

	namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
	re->type = MODEL;
	re->model.model = m_c435.buffer[1];
	re->model.model2 = (size == 4 ? m_c435.buffer[2] : 0);
	re->model.scaling = use_scaling ? m_scaling / 16384.0 : 1.0;
	re->model.transpose = transpose;
	re->absolute_priority = m_absolute_priority;
	re->model_blend_factor = m_model_blend_factor;
	re->mirror_x = mirror_x;
	re->tx = scroll ? m_tx : 0;
	re->ty = scroll ? m_ty : 0;
	re->light_power = m_light_power;
	re->light_ambient = m_light_ambient;
	re->vp_size_x = m_vp_size_x;
	re->vp_size_y = m_vp_size_y;
	re->vp_offset_x = m_vp_offset_x;
	re->vp_offset_y = m_vp_offset_y;
	re->vp_fov = m_clip_data[23];
	re->model.light_vector[0] = m_light_vector[0];
	re->model.light_vector[1] = m_light_vector[1];
	re->model.light_vector[2] = m_light_vector[2];
	re->poly_fade_r = m_c404.poly_fade_r;
	re->poly_fade_g = m_c404.poly_fade_g;
	re->poly_fade_b = m_c404.poly_fade_b;
	re->poly_alpha_color = m_c404.poly_alpha_color;
	re->poly_alpha_pen = m_c404.poly_alpha_pen;
	re->poly_alpha = m_c404.poly_alpha;
	re->screen_fade_r = m_c404.screen_fade_r;
	re->screen_fade_g = m_c404.screen_fade_g;
	re->screen_fade_b = m_c404.screen_fade_b;
	re->screen_fade_factor = m_c404.screen_fade_factor;
	re->fade_flags = m_c404.fade_flags;
	memcpy(re->model.m, m, sizeof(re->model.m));
	memcpy(re->model.v, v, sizeof(re->model.v));

	LOGMASKED(LOG_MODEL_INFO, "%s: Render %04x (%04x) (%04x) %f (%d)\n", machine().describe_context(),
			re->model.model, m_c435.buffer[size - 1], m_c435.buffer[size], re->model.scaling, render.count[render.cur] + 1);

	render.count[render.cur]++;
}

void namcos23_state::c435_flush() // c
{
	if ((m_c435.buffer[0] & 0xf) != 0)
	{
		LOGMASKED(LOG_RENDER_ERR, "%s: WARNING: c435_flush with size %d\n", machine().describe_context(), m_c435.buffer[0] & 0xf);
		return;
	}

	if (BIT(m_c435.buffer[0], 13))
		irq_update(m_main_irqcause | MAIN_C451_IRQ);
	else
		irq_update(m_main_irqcause & ~MAIN_C451_IRQ);
}

void namcos23_state::c435_sprite_w(u16 data)
{
	// Ignore the first two values
	if (m_c435.buffer_pos >= 2)
	{
		if (m_c435.buffer_pos < 4)
		{
			m_c435.sprite_target = data;
		}
		else
		{
			u16 buf_idx = m_c435.buffer_pos - 4;
			LOGMASKED(LOG_SPRITES, "c435_sprite_w: Sprite data area %04x write: %04x\n", m_c435.sprite_target + buf_idx, data);
			m_c435.spritedata[m_c435.sprite_target + buf_idx] = data;
		}
	}
	m_c435.buffer_pos++;
}

void namcos23_state::c435_pio_w(offs_t offset, u16 data)
{
	LOGMASKED(LOG_C435_PIO_UNK, "%s: C435 PIO: %x\n", machine().describe_context(), data);
	m_c435.buffer[m_c435.buffer_pos++] = data;
	u16 h = m_c435.buffer[0];
	int psize;
	if ((h & 0x4000) == 0x4000)
		psize = h & 0xff;
	else
		psize = h & 0xf;
	if (m_c435.buffer_pos < psize + 1)
		return;

	bool known = true;
	switch (h & 0xc000)
	{
	case 0x0000:
		switch (h & 0xf0)
		{
		case 0x00:
			c435_matrix_matrix_mul();
			break;
		case 0x10:
			c435_matrix_vector_mul();
			break;
		case 0x20:
			c435_matrix_matrix_immed_mul();
			break;
		case 0x30:
			c435_matrix_vector_immed_mul();
			break;
		case 0x40:
			c435_matrix_set();
			break;
		case 0x50:
			c435_vector_set();
			break;
		default:
			known = false;
			break;
		}
		break;

	case 0x4000:
		switch (h & 0x3f00)
		{
		case 0x0000:
			c435_unk_set0();
			break;
		case 0x0100:
			c435_absolute_priority_set();
			break;
		case 0x0200:
			c435_tx_set();
			break;
		case 0x0300:
			c435_ty_set();
			break;
		case 0x0400:
			c435_scaling_set();
			break;
		case 0x0500:
			c435_model_blend_factor_set();
			break;
		case 0x0600:
			c435_unk_set6();
			break;
		case 0x0700:
			c435_light_power_set();
			break;
		case 0x0800:
			c435_light_ambient_set();
			break;
		case 0x0f00:
			c435_state_set();
			break;
		default:
			known = false;
			break;
		}
		break;

	case 0x8000:
		c435_render();
		break;
	case 0xc000:
		c435_flush();
		break;
	}

	if (!known)
	{
		std::ostringstream buf;
		buf << "Unknown c435 -";
		for (int i = 0; i < m_c435.buffer_pos; i++)
			util::stream_format(buf, " %04x", m_c435.buffer[i]);
		buf << "\n";
		LOGMASKED(LOG_C435_PIO_UNK, "%s: %s", machine().describe_context(), std::move(buf).str());
	}

	m_c435.buffer_pos = 0;
}

void namcos23_state::c435_dma(address_space &space, u32 adr, u32 size)
{
	adr &= 0x1fffffff;

	if (BIT(m_c435.pio_mode, 0))
	{
		for (u32 pos = 0; pos < size; pos += 2)
			c435_sprite_w(space.read_word(adr + pos));
	}
	else
	{
		for (u32 pos = 0; pos < size; pos += 2)
			c435_pio_w(0, space.read_word(adr + pos));
	}
}

void gorgon_state::nvram_w(offs_t offset, u32 data, u32 mem_mask)
{
	// TODO
}

u16 gorgon_state::czattr_r(offs_t offset, u16 mem_mask)
{
	u16 *czattr = (u16 *)m_czattr.target();
	LOGMASKED(LOG_C435_REG, "%s: czattr[%d] read: %04x & %04x\n", machine().describe_context(), offset, czattr[offset], mem_mask);
	return czattr[offset];
}

void gorgon_state::czattr_w(offs_t offset, u16 data, u16 mem_mask)
{
	u16 *czattr = (u16 *)m_czattr.target();
	u16 prev = czattr[offset];
	LOGMASKED(LOG_C435_REG, "%s: czattr[%d] write: %04x & %04x\n", machine().describe_context(), offset, czattr[offset], mem_mask);
	COMBINE_DATA(&czattr[offset]);

	if (offset == 4)
	{
		// invalidate if compare function changed
		u16 changed = prev ^ czattr[offset];
		for (int bank = 0; bank < 4; bank++)
		{
			m_cz_was_written[bank] |= changed >> (bank * 4) & 2;
		}
	}
}

u16 gorgon_state::czram_r(offs_t offset, u16 mem_mask)
{
	u16 *czattr = (u16 *)m_czattr.target();
	int bank = czattr[5] & 3;
	return m_banked_czram[bank][offset & 0xff];
}

void gorgon_state::czram_w(offs_t offset, u16 data, u16 mem_mask)
{
	u16 *czattr = (u16 *)m_czattr.target();
	for (int bank = 0; bank < 4; bank++)
	{
		// write enable bit
		if (~czattr[4] >> (bank * 4) & 1)
		{
			const u16 old = m_banked_czram[bank][offset & 0xff];
			COMBINE_DATA(&m_banked_czram[bank][offset & 0xff]);
			m_cz_was_written[bank] |= (old ^ m_banked_czram[bank][offset & 0xff]);
		}
	}
}

void gorgon_state::recalc_czram()
{
	u16 *czattr = (u16 *)m_czattr.target();
	for (int bank = 0; bank < 4; bank++)
	{
		// gorgon czram is 'just' a big compare table
		// this is very slow when emulating, so let's recalculate it to a simpler lookup table
		if (m_cz_was_written[bank])
		{
			int reverse = (czattr[4] >> (bank * 4) & 2) ? 0xff : 0;
			int small_val = 0x2000;
			int small_offset = reverse;
			int large_val = 0;
			int large_offset = reverse ^ 0xff;
			int prev = 0;

			for (int i = 0; i < 0x100; i++)
			{
				int factor = i ^ reverse;
				int val = std::min<u16>(m_banked_czram[bank][factor], 0x2000);
				int start = prev;
				int end = val;

				if (i > 0)
				{
					// discard if compare function doesn't match
					if (start >= end)
						continue;

					// fill range
					for (int j = start; j < end; j++)
						m_recalc_czram[bank][j] = factor;
				}

				// remember largest/smallest for later
				if (val < small_val)
				{
					small_val = val;
					small_offset = factor;
				}
				if (val > large_val)
				{
					large_val = val;
					large_offset = factor;
				}

				prev = val;
			}

			// fill possible leftover ranges
			for (int j = 0; j < small_val; j++)
				m_recalc_czram[bank][j] = small_offset;
			for (int j = large_val; j < 0x2000; j++)
				m_recalc_czram[bank][j] = large_offset;

			m_cz_was_written[bank] = 0;
		}
	}
}

u32 namcos23_state::c435_busy_flag_r()
{
	LOGMASKED(LOG_C435_REG, "%s: c435 read busy flag: %08x\n", machine().describe_context(), 1);
	return 1;
}

void namcoss23_state::c435_c361_ack_w(offs_t offset, u32 data)
{
	irq_update(m_main_irqcause & ~MAIN_C361_IRQ);
}

void namcos23_state::c435_dma_addr_w(offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: c435 write address: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_c435.address);
}

void namcos23_state::c435_dma_size_w(offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: c435 write size: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_c435.size);
}

void namcos23_state::c435_dma_start_w(address_space &space, offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: c435 write DMA: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	if (data & 1)
		c435_dma(space, m_c435.address, m_c435.size);
}

void gorgon_state::c435_pio_mode_w(offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: PIO mode write: %08x & %08x (%s mode)\n", machine().describe_context(), data, mem_mask, BIT(data, 0) ? "sprite" : "command");
	COMBINE_DATA(&m_c435.pio_mode);
	m_c435.buffer_pos = 0;
}

void namcos23_state::c435_clear_bufpos_w(offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: clear buffer pos: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	m_c435.buffer_pos = 0;
}

bool namcos23_renderer::stencil_lookup(u32 x, u32 y)
{
	u32 bit = (x & 15) ^ 15;
	u32 offs = ((y << 6) | (x >> 4)) & 0x1ffff;
	return !BIT(m_texram[offs], bit);
}

u32 namcos23_renderer::texture_lookup(const pen_t *pens, int penshift, int penmask, u32 u, u32 v, u8 &pen)
{
	const u32 tileid = ((u >> 4) & 0xff) | ((v << 4) & m_tileid_mask);
	const u32 tile = m_tmrom_decoded[tileid];
	const u32 attr = m_texattr_decoded[tileid];
	if (BIT(attr, 0))
		v = ~v;
	if (BIT(attr, 1))
		u = ~u;
	if (BIT(attr, 2))
		std::swap(u, v);
	pen = m_texrom[tile | ((v << 4) & 0xf0) | (u & 0x0f)];
	return pens[(pen >> penshift) & penmask];
}

void namcos23_renderer::render_sprite_scanline(s32 scanline, const extent_t& extent, const namcos23_render_data& object, int threadid)
{
	const namcos23_render_data& rd = object;

	int y_index = extent.param[1].start - rd.sprite_yflip;
	float x_index = extent.param[0].start - rd.sprite_xflip;
	float dx = extent.param[0].dpdx;
	const pen_t *pal = rd.pens;
	u8 *const source = (u8 *)rd.sprite_source + y_index * rd.sprite_line_modulo;
	u32 *const dest = &object.bitmap->pix(scanline);
	u8 *primap = &object.primap->pix(scanline);
	int prioverchar = object.prioverchar;

	const s32 alphafactor = object.alpha;
	const s32 alphafactor_inv = object.alpha_inv;
	const bool alpha_enabled = object.alpha_enabled;
	const u8 alpha_pen = object.poly_alpha_pen;
	const s32 fadefactor = object.fadefactor;
	const s32 fadefactor_inv = object.fadefactor_inv;
	const s32 fadecolor_r = object.fadecolor_r;
	const s32 fadecolor_g = object.fadecolor_g;
	const s32 fadecolor_b = object.fadecolor_b;

	for (int x = extent.startx; x < extent.stopx; x++)
	{
		int pen = source[(int)x_index];
		if (pen != 0xff)
		{
			const u32 rgb = (u32)pal[pen];
			s32 r = s32((rgb >> 16) & 0xff);
			s32 g = s32((rgb >> 8) & 0xff);
			s32 b = s32(rgb & 0xff);

			if (fadefactor != 0xff)
			{
				r = ((r * fadefactor) + (fadecolor_r * fadefactor_inv)) >> 8;
				g = ((g * fadefactor) + (fadecolor_g * fadefactor_inv)) >> 8;
				b = ((b * fadefactor) + (fadecolor_b * fadefactor_inv)) >> 8;
			}

			if (alphafactor != 0xff && (alpha_enabled || pen == alpha_pen))
			{
				const u32 drgb = dest[x];
				const s32 dr = s32((drgb >> 16) & 0xff);
				const s32 dg = s32((drgb >> 8) & 0xff);
				const s32 db = s32(drgb & 0xff);
				r = ((r * alphafactor) + (dr * alphafactor_inv)) >> 8;
				g = ((g * alphafactor) + (dg * alphafactor_inv)) >> 8;
				b = ((b * alphafactor) + (db * alphafactor_inv)) >> 8;
			}

			dest[x] = 0xff000000 | (r << 16) | (g << 8) | b;
			primap[x] = (primap[x] & ~1) | prioverchar;
		}
		x_index += dx;
	}
}

template <bool Stencil, bool Shade, bool PolyFade, bool ColorFade, bool Blend, bool PolyAlpha>
void namcos23_renderer::render_scanline(s32 scanline, const extent_t& extent, const namcos23_render_data& object, int threadid)
{
	const namcos23_render_data& rd = object;

	float z = extent.param[0].start;
	float u = extent.param[1].start;
	float v = extent.param[2].start;
	float i = extent.param[3].start;
	const float dz = extent.param[0].dpdx;
	const float du = extent.param[1].dpdx;
	const float dv = extent.param[2].dpdx;
	const float di = extent.param[3].dpdx;

	const s32 fadefactor = rd.fadefactor;
	const s32 fadefactor_inv = rd.fadefactor_inv;
	const s32 alphafactor = rd.alpha;
	const s32 alphafactor_inv = rd.alpha_inv;
	const bool alpha_enabled = rd.alpha_enabled;
	const u8 alpha_pen = rd.poly_alpha_pen;
	const s32 fadecolor_r = rd.fadecolor_r;
	const s32 fadecolor_g = rd.fadecolor_g;
	const s32 fadecolor_b = rd.fadecolor_b;
	const s32 polycolor_r = rd.polycolor_r;
	const s32 polycolor_g = rd.polycolor_g;
	const s32 polycolor_b = rd.polycolor_b;
	const s32 clip_left = ((320 - rd.vp_size_x) + rd.vp_offset_x);
	const s32 clip_right = ((320 + rd.vp_size_x) + rd.vp_offset_x);
	const s32 clip_top = ((240 - rd.vp_size_y) - rd.vp_offset_y);
	const s32 clip_bottom = ((240 + rd.vp_size_y) - rd.vp_offset_y);

	const s32 offset_scanline = scanline + (s32)clip_top;
	if (offset_scanline < 0 || offset_scanline >= 480 || offset_scanline < clip_top || offset_scanline >= clip_bottom)
		return;

	u32 *dest = &rd.bitmap->pix(offset_scanline);
	u8 *primap = &rd.primap->pix(offset_scanline);

	const pen_t *pens = rd.pens;
	int prioverchar = rd.prioverchar;
	int penmask = 0xff;
	int penshift = 0;

	if (rd.cmode & 4)
	{
		pens += 0xec + ((rd.cmode & 8) << 1);
		penmask = 0x03;
		penshift = 2 * (~rd.cmode & 3);
	}
	else if (rd.cmode & 2)
	{
		pens += 0xe0 + ((rd.cmode & 8) << 1);
		penmask = 0x0f;
		penshift = 4 * (~rd.cmode & 1);
	}

	for (int x = extent.startx; x < extent.stopx; x++)
	{
		float ooz = 1.0f / z;
		u32 tx = u32(u * ooz);
		u32 ty = u32(v * ooz);
		u8 pen = 0;
		const u32 view_x = x + clip_left;
		if ((!Stencil || !stencil_lookup(tx, ty)) && view_x >= clip_left && view_x < clip_right)
		{
			ty += rd.tbase;
			u32 tex_rgb = texture_lookup(pens, penshift, penmask, tx, ty, pen);
			s32 r = s32((tex_rgb >> 16) & 0xff);
			s32 g = s32((tex_rgb >> 8) & 0xff);
			s32 b = s32(tex_rgb & 0xff);

			if (Shade)
			{
				const s32 shade = std::clamp<s32>(i * ooz, 0, 63);
				r = (r * shade) >> 6;
				g = (g * shade) >> 6;
				b = (b * shade) >> 6;
			}

			if (PolyFade)
			{
				r = (r * polycolor_r) >> 8;
				g = (g * polycolor_g) >> 8;
				b = (b * polycolor_b) >> 8;
			}

			if (ColorFade)
			{
				r = ((r * fadefactor) + (fadecolor_r * fadefactor_inv)) >> 8;
				g = ((g * fadefactor) + (fadecolor_g * fadefactor_inv)) >> 8;
				b = ((b * fadefactor) + (fadecolor_b * fadefactor_inv)) >> 8;
			}

			u32 drgb;
			s32 dr, dg, db;
			if (Blend || (PolyAlpha && (alpha_enabled || pen == alpha_pen)))
			{
				drgb = dest[view_x];
				dr = s32((drgb >> 16) & 0xff);
				dg = s32((drgb >> 8) & 0xff);
				db = s32(drgb & 0xff);
			}

			if (PolyAlpha && (alpha_enabled || pen == alpha_pen))
			{
				r = ((r * alphafactor) + (dr * alphafactor_inv)) >> 8;
				g = ((g * alphafactor) + (dg * alphafactor_inv)) >> 8;
				b = ((b * alphafactor) + (db * alphafactor_inv)) >> 8;
			}
			else if (Blend)
			{
				r = ((r * 0x80) + (dr * 0x80)) >> 8;
				g = ((g * 0x80) + (dg * 0x80)) >> 8;
				b = ((b * 0x80) + (db * 0x80)) >> 8;
			}

			dest[view_x] = 0xff000000 | (r << 16) | (g << 8) | b;
			primap[view_x] = (primap[view_x] & ~1) | prioverchar;
		}

		z += dz;
		u += du;
		v += dv;
		i += di;
	}
}

void namcos23_state::render_apply_transform(s32 xi, s32 yi, s32 zi, const namcos23_render_entry *re, float &x, float &y, float &z)
{
	s16 m[9];
	memcpy(m, re->model.m, sizeof(s16) * 9);
	if (re->model.transpose)
		transpose_matrix(m);
	x = (((m[0] * s64(xi) + m[1] * s64(yi) + m[2] * s64(zi)) >> 14) * re->model.scaling + re->model.v[0]) / 16384.f;
	y = (((m[3] * s64(xi) + m[4] * s64(yi) + m[5] * s64(zi)) >> 14) * re->model.scaling + re->model.v[1]) / 16384.f;
	z = (((m[6] * s64(xi) + m[7] * s64(yi) + m[8] * s64(zi)) >> 14) * re->model.scaling + re->model.v[2]) / 16384.f;
}

void namcos23_state::render_apply_matrot(s32 xi, s32 yi, s32 zi, const namcos23_render_entry *re, float &x, float &y, float &z)
{
	x = (re->model.m[0] * xi + re->model.m[1] * yi + re->model.m[2] * zi) / 4194304.f;
	y = (re->model.m[3] * xi + re->model.m[4] * yi + re->model.m[5] * zi) / 4194304.f;
	z = (re->model.m[6] * xi + re->model.m[7] * yi + re->model.m[8] * zi) / 4194304.f;
}

void namcos23_state::render_project(poly_vertex &pv, const s16 vp_size_x, const s16 vp_size_y, const float vp_fov)
{
	// 768 validated by the title screen size on tc2:
	// texture is 640x480, x range is 3.125, y range is 2.34375, z is 3.75
	// 640/(3.125/3.75) = 768
	// 480/(2.34375/3.75) = 768

	pv.x = (float)vp_size_x + vp_fov * pv.x;
	pv.y = (float)vp_size_y - vp_fov * pv.y;

	pv.p[0] = 1.0f / pv.p[0];
}

void namcos23_state::render_direct_poly(const namcos23_render_entry *re)
{
	render_t &render = m_render;

	u32 polyshift = ((re->direct.d[1] & 0x1ff) << 12) | (re->direct.d[0] & 0xfff);
	u32 cztype = re->direct.d[3] & 3;
	u32 flags = ((re->direct.d[3] << 6) & 0x1fff) | cztype;

	static const int indices[2][3] =
	{
		{ 0, 1, 2 },
		{ 0, 2, 3 }
	};
	for (int i = 0; i < 2; i++)
	{
		namcos23_poly_entry *p = render.polys + render.poly_count;
		p->vertex_count = 3;

		for (int j = 0; j < 3; j++)
		{
			int index = indices[i][j];
			u16 const *src = &re->direct.d[4 + index * 6];

			int mantissa = src[5];
			int exponent = src[4] & 0x3f;

			if (mantissa)
			{
				p->pv[j].p[0] = mantissa;
				while (exponent < 0x2e)
				{
					p->pv[j].p[0] /= 2.0f;
					exponent++;
				}
			}
			else
				p->pv[j].p[0] = 1.f;

			p->pv[j].p[1] = ((src[0] >> 4) + 0.5) * p->pv[j].p[0];
			p->pv[j].p[2] = ((src[1] >> 4) + 0.5) * p->pv[j].p[0];
			p->pv[j].p[3] = (src[4] >> 8) * p->pv[j].p[0];
			p->pv[j].x = ((s16)src[2] + 320);
			p->pv[j].y = ((s16)src[3] + 240);
		}

		int zsort = 0;
		if (zsort > 0x1fffff) zsort = 0x1fffff;

		int absolute_priority = re->absolute_priority & 7;
		if (BIT(polyshift, 21))
			zsort = polyshift & 0x1fffff;
		else
		{
			zsort += BIT(polyshift, 17) ? (polyshift | 0xfffc0000) : (polyshift & 0x0001ffff);
			absolute_priority += (polyshift & 0x1c0000) >> 18;
		}

		zsort = std::clamp(zsort, 0, 0x1fffff);
		zsort |= (absolute_priority << 21);
		p->zkey = zsort;
		p->index = render.poly_count;

		p->rd.type = re->type;
		p->rd.stencil_enabled = false;
		p->rd.pens = m_palette->pens() + (re->direct.d[2] & 0x7f00);
		p->rd.direct = true;
		p->rd.sprite = false;
		p->rd.immediate = false;
		p->rd.shade_enabled = true;
		p->rd.rgb = 0x00ffffff;
		p->rd.flags = flags;
		p->rd.tbase = ((re->direct.d[2] & 0x000f) << 12) | (BIT(re->direct.d[2], 7) << 16);
		p->rd.cmode = (re->direct.d[2] & 0x0070) >> 4;
		p->rd.cz_value = (re->direct.d[3] >> 2) & 0x1fff;
		p->rd.cz_type = re->direct.d[3] & 3;
		p->rd.prioverchar = ((p->rd.cmode & 7) == 1) ? 7 : 0;
		p->rd.vp_size_x = re->vp_size_x;
		p->rd.vp_size_y = re->vp_size_y;
		p->rd.vp_offset_x = re->vp_offset_x;
		p->rd.vp_offset_y = re->vp_offset_y;

		p->rd.fogfactor = 0;
		p->rd.fadefactor = 0xff;
		p->rd.fadefactor_inv = 0x01;
		p->rd.alphafactor = re->poly_alpha;

		// global fade
		if (re->fade_flags & 1)
		{
			p->rd.fadefactor = 0xff - re->screen_fade_factor;
			p->rd.fadefactor_inv = 0x100 - p->rd.fadefactor;
			p->rd.fadecolor_r = re->screen_fade_r;
			p->rd.fadecolor_g = re->screen_fade_g;
			p->rd.fadecolor_b = re->screen_fade_b;
		}

		// poly fade
		p->rd.pfade_enabled = re->poly_fade_r != 0 || re->poly_fade_g != 0 || re->poly_fade_b != 0;
		p->rd.polycolor_r = re->poly_fade_r;
		p->rd.polycolor_g = re->poly_fade_g;
		p->rd.polycolor_b = re->poly_fade_b;

		// alpha
		p->rd.alpha = 0xff - re->poly_alpha;
		p->rd.alpha_inv = 0x100 - p->rd.alpha;
		p->rd.alpha_enabled = ((re->direct.d[2] >> 8) & 0x7f) != re->poly_alpha_color;
		p->rd.poly_alpha_pen = re->poly_alpha_pen;

		// blend
		p->rd.blend_enabled = false;

		render.poly_count++;
	}
}

void gorgon_state::render_sprite(const namcos23_render_entry *re)
{
	int offset = 0;

	for (int row = 0; row < re->sprite.rows; row++)
	{
		for (int col = 0; col < re->sprite.cols; col++)
		{
			render_sprite_tile(offset, re, row, col);
			offset++;
		}
	}
}

void gorgon_state::render_sprite_tile(u32 code_offset, const namcos23_render_entry *re, int row, int col)
{
	const namcos23_sprite_data &sprite = re->sprite;
	render_t &render = m_render;

	u32 code = sprite.code + code_offset;

	gfx_element *gfx = m_gfxdecode->gfx(0);
	s32 sprite_screen_height = (((sprite.ysize << 16) >> 5) * gfx->height() + 0x8000) >> 16;
	s32 sprite_screen_width = (((sprite.xsize << 16) >> 5) * gfx->width() + 0x8000) >> 16;
	if (sprite_screen_width && sprite_screen_height)
	{
		float fsx = sprite.xpos + col * sprite.xsize;
		float fsy = sprite.ypos + row * sprite.ysize;
		float fwidth = gfx->width();
		float fheight = gfx->height();
		float fsw = sprite_screen_width;
		float fsh = sprite_screen_height;

		namcos23_poly_entry *p = render.polys + render.poly_count;
		p->vertex_count = 4;

		p->pv[0].x = fsx;
		p->pv[0].y = fsy;
		p->pv[0].p[0] = 0;
		p->pv[0].p[1] = 0;
		p->pv[1].x = fsx + fsw;
		p->pv[1].y = fsy;
		p->pv[1].p[0] = fwidth;
		p->pv[1].p[1] = 0;
		p->pv[2].x = fsx + fsw;
		p->pv[2].y = fsy + fsh;
		p->pv[2].p[0] = fwidth;
		p->pv[2].p[1] = fheight;
		p->pv[3].x = fsx;
		p->pv[3].y = fsy + fsh;
		p->pv[3].p[0] = 0;
		p->pv[3].p[1] = fheight;
		p->pv[0].p[2] = p->pv[1].p[2] = p->pv[2].p[2] = p->pv[3].p[2] = 1.f;
		p->pv[0].p[3] = p->pv[1].p[3] = p->pv[2].p[3] = p->pv[3].p[3] = 64.f;

		p->rd.pens = m_palette->pens() + gfx->granularity() * (sprite.color & 0x7f);
		p->zkey = sprite.zcoord;
		p->index = render.poly_count;

		p->rd.sprite = true;
		p->rd.immediate = false;
		p->rd.shade_enabled = false;
		p->rd.sprite_source = gfx->get_data(code % gfx->elements());
		p->rd.sprite_line_modulo = gfx->rowbytes();
		p->rd.sprite_xflip = sprite.xflip;
		p->rd.sprite_yflip = sprite.yflip;

		p->rd.fadefactor = 0xff;

		// global fade
		if (re->fade_flags & 2 || sprite.fade_enabled)
		{
			p->rd.fadefactor = 0xff - re->screen_fade_factor;
			p->rd.fadefactor_inv = 0x100 - p->rd.fadefactor;
			p->rd.fadecolor_r = re->screen_fade_r;
			p->rd.fadecolor_g = re->screen_fade_g;
			p->rd.fadecolor_b = re->screen_fade_b;
		}

		// sprite fog
		p->rd.fogfactor = 0;

		p->rd.alpha = 0xff - sprite.alpha;
		p->rd.alpha_inv = 0x100 - p->rd.alpha;
		p->rd.poly_alpha_pen = re->poly_alpha_pen;
		p->rd.alpha_enabled = (sprite.color & 0x7f) != re->poly_alpha_color;

		p->rd.blend_enabled = false;
		render.poly_count++;
	}
}

void namcos23_state::render_immediate(const namcos23_render_entry *re)
{
	render_t &render = m_render;
	poly_vertex pv[16];

	u32 type = re->immediate.type;
	u32 h    = re->immediate.h;
	u32 polyshift = re->immediate.zbias;
	u32 ne   = (type >> 8) & 0xf;
	bool stencil_enabled = BIT(h, 11);

	float minz = FLT_MAX;
	float maxz = FLT_MIN;

	for (int i = 0; i < ne; i++)
	{
		pv[i].x = s32(re->immediate.x[i]) / 16384.f;
		pv[i].y = s32(re->immediate.y[i]) / 16384.f;
		pv[i].p[0] = (s32)re->immediate.z[i] / 16384.f;
		pv[i].p[1] = (s32)re->immediate.u[i];
		pv[i].p[2] = (s32)re->immediate.v[i];
		pv[i].p[3] = (s32)re->immediate.i[i];
	}

	namcos23_poly_entry *p = render.polys + render.poly_count;

	// Should be unnecessary once frustum clipping happens correctly, but this will at least cull polys behind the camera
	p->vertex_count = render.polymgr->zclip_if_less<4>(ne, pv, p->pv, 0.0001f);

	// Project if you don't clip on the near plane
	if (p->vertex_count >= 3)
	{
		for (int i = 0; i < p->vertex_count; i++)
		{
			float z = p->pv[i].p[0] != 0 ? p->pv[i].p[0] : 1.f;
			p->pv[i].x /= z;
			p->pv[i].y /= z;

			z *= 16384.f;
			if (z > maxz)
				maxz = z;
			if (z < minz)
				minz = z;

			render_project(p->pv[i], re->vp_size_x, re->vp_size_y, re->vp_fov);

			float w = p->pv[i].p[0];
			p->pv[i].p[1] *= w;
			p->pv[i].p[2] *= w;
			p->pv[i].p[3] *= w;
		}

		// Compute an odd sorta'-Z thing that can situate the polygon wherever you want in Z-depth
		int zsort = 0.5f * (minz + maxz) + 0.5f;
		if (zsort > 0x1fffff) zsort = 0x1fffff;

		int absolute_priority = re->absolute_priority & 7;
		if (BIT(polyshift, 21))
			zsort = polyshift & 0x1fffff;
		else
		{
			zsort += BIT(polyshift, 16) ? (polyshift | 0xfffe0000) : (polyshift & 0x0000ffff);
			absolute_priority += (polyshift & 0x1c0000) >> 18;
		}

		p->zkey = zsort | (absolute_priority << 21);
		p->index = render.poly_count;

		p->rd.stencil_enabled = stencil_enabled;
		p->rd.pens = m_palette->pens() + (re->immediate.pal & 0x7f00);
		p->rd.rgb = 0x00ffffff;
		p->rd.direct = false;
		p->rd.sprite = false;
		p->rd.immediate = true;
		p->rd.shade_enabled = true;
		p->rd.h = h;
		p->rd.type = type;
		p->rd.vp_size_x = re->vp_size_x;
		p->rd.vp_size_y = re->vp_size_y;
		p->rd.vp_offset_x = re->vp_offset_x;
		p->rd.vp_offset_y = re->vp_offset_y;
		p->rd.tbase = 0;

		// global fade
		if (re->fade_flags & 1)
		{
			p->rd.fadefactor = 0xff - re->screen_fade_factor;
			p->rd.fadefactor_inv = 0x100 - p->rd.fadefactor;
			p->rd.fadecolor_r = re->screen_fade_r;
			p->rd.fadecolor_g = re->screen_fade_g;
			p->rd.fadecolor_b = re->screen_fade_b;
		}

		// poly fade
		p->rd.pfade_enabled = re->poly_fade_r != 0 || re->poly_fade_g != 0 || re->poly_fade_b != 0;
		p->rd.polycolor_r = re->poly_fade_r;
		p->rd.polycolor_g = re->poly_fade_g;
		p->rd.polycolor_b = re->poly_fade_b;

		// alpha
		p->rd.alpha = 0xff - re->poly_alpha;
		p->rd.alpha_inv = 0x100 - p->rd.alpha;
		p->rd.alpha_enabled = ((re->immediate.pal >> 8) & 0x7f) != re->poly_alpha_color;
		p->rd.poly_alpha_pen = re->poly_alpha_pen;
		p->rd.blend_enabled = BIT(h, 10);
		p->rd.type = re->type;

		render.poly_count++;
	}
}

void namcos23_state::render_model(const namcos23_render_entry *re)
{
	render_t &render = m_render;

	LOGMASKED(LOG_MODELS, "%s: Drawing model %04x\n", machine().describe_context(), re->model.model);

	const bool model_blend = (re->model.model2 != 0);
	u32 adr = m_ptrom[re->model.model];
	u32 adr2 = m_ptrom[re->model.model2];
	if (adr >= m_ptrom_limit || adr2 >= m_ptrom_limit)
	{
		LOGMASKED(LOG_MODEL_ERR, "%s: WARNING: model %04x base address %08x out-of-bounds - pointram?\n", machine().describe_context(), re->model.model, adr);
		return;
	}

	u32 offs = 0;
	const u32 *data = &m_ptrom[adr];
	const u32 *data2 = &m_ptrom[adr2];
	while ((adr + offs) < m_ptrom_limit && (adr2 + offs) < m_ptrom_limit)
	{
		poly_vertex pv[15];

		u32 type = data[offs++];
		u32 h    = data[offs++];

		u32 cmode = (type & 0x70000000) >> 28;
		u32 tbase = (((type & 0x0f000000) >> 24) << 12) | (BIT(type, 31) << 16);
		u8 color = (h >> 24) & 0x7f;
		int lmode = (type >> 19) & 3;
		int ne = (type >> 8) & 15;
		bool stencil_enabled = BIT(h, 11);

		u32 polyshift = 0;
		if (type & 0x00001000)
		{
			polyshift = data[offs++];
		}
		u8 alpha = 0xff;

		u32 light = 0;
		u32 extptr = 0;

		if (lmode == 3)
		{
			extptr = offs;
			offs += ne;
		}
		else
		{
			light = data[offs++];
		}

		for (int i = 0; i < ne; i++)
		{
			u32 v1 = data[offs + 0];
			u32 v2 = data[offs + 1];
			u32 v3 = data[offs + 2];

			float x, y, z;
			render_apply_transform(u32_to_s24(v1), u32_to_s24(v2), u32_to_s24(v3), re, x, y, z);

			float factor_b = re->model_blend_factor / 16384.f;
			float factor_a = 1.f - factor_b;
			if (model_blend)
			{
				u32 v12 = data2[offs + 0];
				u32 v22 = data2[offs + 1];
				u32 v32 = data2[offs + 2];

				float x2, y2, z2;
				render_apply_transform(u32_to_s24(v12), u32_to_s24(v22), u32_to_s24(v32), re, x2, y2, z2);

				x = x * factor_a + x2 * factor_b;
				y = y * factor_a + y2 * factor_b;
				z = z * factor_a + z2 * factor_b;
			}

			offs += 3;

			pv[i].x = x;
			pv[i].y = y;
			pv[i].p[0] = z;
			pv[i].p[1] = (((v1 >> 20) & 0xf00) | ((v2 >> 24) & 0xff)) + (stencil_enabled ? 0 : 0.5) + re->tx;
			pv[i].p[2] = (((v1 >> 16) & 0xf00) | ((v3 >> 24) & 0xff)) + (stencil_enabled ? 0 : 0.5) + re->ty;
			pv[i].p[3] = 64;

			static const u8 LIGHT_SHIFTS[4] = { 24, 16,  8,  0 };
			switch (lmode)
			{
			case 0:
				pv[i].p[3] = (float)((light >> LIGHT_SHIFTS[i]) & 0xff);
				break;
			case 1:
				pv[i].p[3] = (float)((light >> LIGHT_SHIFTS[i]) & 0xff);
				break;
			case 2:
				pv[i].p[3] = 64.f;
				break;
			case 3:
			{
				u32 norm = data[extptr];
				s32 nx = u32_to_s10((norm >> 20) & 0x3ff);
				s32 ny = u32_to_s10((norm >> 10) & 0x3ff);
				s32 nz = u32_to_s10(norm & 0x3ff);

				if (model_blend)
				{
					u32 norm2 = data2[extptr];
					s32 nx2 = u32_to_s10((norm2 >> 20) & 0x3ff);
					s32 ny2 = u32_to_s10((norm2 >> 10) & 0x3ff);
					s32 nz2 = u32_to_s10(norm2 & 0x3ff);

					nx = (s32)(nx * factor_a + nx2 * factor_b);
					ny = (s32)(ny * factor_a + ny2 * factor_b);
					nz = (s32)(nz * factor_a + nz2 * factor_b);
				}
				extptr++;

				float nrx, nry, nrz;
				render_apply_matrot(nx, ny, nz, re, nrx, nry, nrz);
				float length = sqrtf(nrx * nrx + nry * nry + nrz * nrz);
				if (length != 0.0f)
				{
					nrx /= length;
					nry /= length;
					nrz /= length;
				}

				float cx, cy, cz;
				render_apply_matrot(re->model.light_vector[0], re->model.light_vector[1], re->model.light_vector[2], re, cx, cy, cz);
				length = sqrtf(cx * cx + cy * cy + cz * cz);
				if (length != 0.0f)
				{
					cx /= length;
					cy /= length;
					cz /= length;
				}

				float lsi = nrx * cx + nry * cy + nrz * cz;
				if (lsi < 0)
					lsi = 0;

				pv[i].p[3] = std::clamp(re->light_ambient + re->light_power * lsi, 0.f, 64.f);
			}   break;
			}
		}
		namcos23_poly_entry *p = render.polys + render.poly_count;

		if (ne >= 3)
		{
			if (BIT(h, 5))
			{
				const float z0 = pv[0].p[0];
				const float z1 = pv[1].p[0];
				const float z2 = pv[2].p[0];
				const float z3 = pv[3].p[0];
				float c1 =
					(pv[2].x * (z0 * pv[1].y - pv[0].y * z1)) +
					(pv[2].y * (pv[0].x * z1 - z0 * pv[1].x)) +
					(z2 * (pv[0].y * pv[1].x - pv[0].x * pv[1].y));
				float c2 =
					(pv[0].x * (z2 * pv[3].y - pv[2].y * z3))+
					(pv[0].y * (pv[2].x * z3 - z2 * pv[3].x))+
					(z0 * (pv[2].y * pv[3].x - pv[2].x * pv[3].y));

				if (c1 >= 0.f && c2 >= 0.f)
				{
					if (type & 0x00010000)
						break;
					continue;
				}
			}

			if (re->mirror_x)
			{
				pv[0].x *= -1.0f;
				pv[1].x *= -1.0f;
				pv[2].x *= -1.0f;
				pv[3].x *= -1.0f;
			}
		}
		else
		{
			if (type & 0x00010000)
				break;
			continue;
		}

		float minz = FLT_MAX;
		float maxz = FLT_MIN;

		// Should be unnecessary once frustum clipping happens correctly, but this will at least cull polys behind the camera
		p->vertex_count = render.polymgr->zclip_if_less<4>(ne, pv, p->pv, 0.0001f);

		// Project if you don't clip on the near plane
		if (p->vertex_count >= 3)
		{
			// This is our bad excuse for a projection matrix
			for (int i = 0; i < p->vertex_count; i++)
			{
				float z = (p->pv[i].p[0] != 0) ? p->pv[i].p[0] : 1.f;
				p->pv[i].x /= z;
				p->pv[i].y /= z;

				z *= 16384.f;
				if (z > maxz)
					maxz = z;
				if (z < minz)
					minz = z;

				render_project(p->pv[i], re->vp_size_x, re->vp_size_y, re->vp_fov);

				float w = p->pv[i].p[0];
				p->pv[i].p[1] *= w;
				p->pv[i].p[2] *= w;
				p->pv[i].p[3] *= w;
			}

			if (maxz < 0)
			{
				if (type & 0x00010000)
					break;
				continue;
			}

			int zsort = 0;
			switch (h & 0x300)
			{
			case 0x000:
				zsort = minz + 0.5f;
				break;
			case 0x100:
				zsort = maxz + 0.5f;
				break;
			default:
				zsort = 0.5f * (minz + maxz) + 0.5f;
				break;
			}
			if (zsort > 0x1fffff) zsort = 0x1fffff;

			int absolute_priority = re->absolute_priority & 7;
			if (BIT(polyshift, 21))
				zsort = polyshift & 0x1fffff;
			else
			{
				zsort += BIT(polyshift, 17) ? (polyshift | 0xfffc0000) : (polyshift & 0x0001ffff);
				absolute_priority += (polyshift & 0x1c0000) >> 18;
			}

			zsort = std::clamp(zsort, 0, 0x1fffff);
			zsort |= (absolute_priority << 21);
			p->zkey = zsort;
			p->index = render.poly_count;

			p->rd.type = re->type;
			p->rd.stencil_enabled = stencil_enabled;
			p->rd.pens = m_palette->pens() + (color << 8);
			p->rd.rgb = (alpha << 24) | 0x00ffffff;
			p->rd.model = re->model.model;
			p->rd.direct = false;
			p->rd.sprite = false;
			p->rd.immediate = false;
			p->rd.shade_enabled = true;
			p->rd.h = h;
			p->rd.type = type;
			p->rd.tbase = tbase;
			p->rd.cmode = cmode;
			p->rd.cz_value = 0;
			p->rd.cz_type = 0;
			p->rd.vp_size_x = re->vp_size_x;
			p->rd.vp_size_y = re->vp_size_y;
			p->rd.vp_offset_x = re->vp_offset_x;
			p->rd.vp_offset_y = re->vp_offset_y;

			p->rd.fogfactor = 0;
			p->rd.fadefactor = 0xff;
			p->rd.alphafactor = re->poly_alpha;

			// global fade
			if (re->fade_flags & 1)
			{
				p->rd.fadefactor = 0xff - re->screen_fade_factor;
				p->rd.fadefactor_inv = 0x100 - p->rd.fadefactor;
				p->rd.fadecolor_r = re->screen_fade_r;
				p->rd.fadecolor_g = re->screen_fade_g;
				p->rd.fadecolor_b = re->screen_fade_b;
			}

			// poly fade
			p->rd.pfade_enabled = re->poly_fade_r != 0 || re->poly_fade_g != 0 || re->poly_fade_b != 0;
			p->rd.polycolor_r = re->poly_fade_r;
			p->rd.polycolor_g = re->poly_fade_g;
			p->rd.polycolor_b = re->poly_fade_b;

			// alpha
			p->rd.alpha = 0xff - re->poly_alpha;
			p->rd.alpha_inv = 0x100 - p->rd.alpha;
			p->rd.alpha_enabled = ((color & 0x7f) != re->poly_alpha_color) || BIT(type, 21);
			p->rd.poly_alpha_pen = re->poly_alpha_pen;
			p->rd.blend_enabled = BIT(h, 10);

			render.poly_count++;
		}

		if (type & 0x000010000)
			break;
	}
}

static int render_poly_compare(const void *i1, const void *i2)
{
	const namcos23_poly_entry *p1 = *(const namcos23_poly_entry **)i1;
	const namcos23_poly_entry *p2 = *(const namcos23_poly_entry **)i2;

	if (p1->zkey < p2->zkey)
		return 1;
	if (p1->zkey > p2->zkey)
		return -1;
	if (p1->index < p2->index)
		return -1;
	if (p1->index > p2->index)
		return 1;
	return 0;
}

#define RENDER_SCANLINE_ENTRY(stencil, shade, polyfade, colorfade, blend, polyalpha) \
	if (p->vertex_count == 3) \
		render_triangle<4>(scissor, render_delegate(&namcos23_renderer::render_scanline<stencil, shade, polyfade, colorfade, blend, polyalpha>, this), p->pv[0], p->pv[1], p->pv[2]); \
	else if (p->vertex_count == 4) \
		render_triangle_fan<4>(scissor, render_delegate(&namcos23_renderer::render_scanline<stencil, shade, polyfade, colorfade, blend, polyalpha>, this), 4, p->pv); \
	else if (p->vertex_count == 5) \
		render_triangle_fan<4>(scissor, render_delegate(&namcos23_renderer::render_scanline<stencil, shade, polyfade, colorfade, blend, polyalpha>, this), 5, p->pv); \
	else if (p->vertex_count == 6) \
		render_triangle_fan<4>(scissor, render_delegate(&namcos23_renderer::render_scanline<stencil, shade, polyfade, colorfade, blend, polyalpha>, this), 6, p->pv); \
	break;

void namcos23_renderer::render_flush(screen_device &screen, bitmap_rgb32 &bitmap)
{
	render_t &render = m_state.m_render;

	if (!render.poly_count)
		return;

	for (int i = 0; i < render.poly_count; i++)
		render.poly_order[i] = &render.polys[i];

	qsort(render.poly_order, render.poly_count, sizeof(namcos23_poly_entry *), render_poly_compare);

	const static rectangle scissor(0, 639, 0, 479);

	for (int i = 0; i < render.poly_count; i++)
	{
		const namcos23_poly_entry *p = render.poly_order[i];
		namcos23_render_data& extra = render.polymgr->object_data().next();
		extra = p->rd;
		extra.bitmap = &bitmap;
		extra.primap = &screen.priority();
		extra.prioverchar = 2;

		// We should probably split the polygons into triangles ourselves to ensure everything is being rendered properly
		if (p->rd.sprite)
		{
			render_triangle_fan<4>(scissor, render_delegate(&namcos23_renderer::render_sprite_scanline, this), 4, p->pv);
		}
		else
		{
			const u8 render_hash = u32(p->rd.stencil_enabled) << 5 |
				u32(p->rd.shade_enabled) << 4 |
				u32(p->rd.pfade_enabled) << 3 |
				u32(p->rd.fadefactor != 0xff) << 2 |
				u32(p->rd.blend_enabled) << 1 |
				u32(p->rd.alpha != 0xff);
			switch (render_hash)
			{
			case  0: RENDER_SCANLINE_ENTRY(false, false, false, false, false, false);
			case  1: RENDER_SCANLINE_ENTRY(false, false, false, false, false,  true);
			case  2: RENDER_SCANLINE_ENTRY(false, false, false, false,  true, false);
			case  3: RENDER_SCANLINE_ENTRY(false, false, false, false,  true,  true);
			case  4: RENDER_SCANLINE_ENTRY(false, false, false,  true, false, false);
			case  5: RENDER_SCANLINE_ENTRY(false, false, false,  true, false,  true);
			case  6: RENDER_SCANLINE_ENTRY(false, false, false,  true,  true, false);
			case  7: RENDER_SCANLINE_ENTRY(false, false, false,  true,  true,  true);
			case  8: RENDER_SCANLINE_ENTRY(false, false,  true, false, false, false);
			case  9: RENDER_SCANLINE_ENTRY(false, false,  true, false, false,  true);
			case 10: RENDER_SCANLINE_ENTRY(false, false,  true, false,  true, false);
			case 11: RENDER_SCANLINE_ENTRY(false, false,  true, false,  true,  true);
			case 12: RENDER_SCANLINE_ENTRY(false, false,  true,  true, false, false);
			case 13: RENDER_SCANLINE_ENTRY(false, false,  true,  true, false,  true);
			case 14: RENDER_SCANLINE_ENTRY(false, false,  true,  true,  true, false);
			case 15: RENDER_SCANLINE_ENTRY(false, false,  true,  true,  true,  true);
			case 16: RENDER_SCANLINE_ENTRY(false,  true, false, false, false, false);
			case 17: RENDER_SCANLINE_ENTRY(false,  true, false, false, false,  true);
			case 18: RENDER_SCANLINE_ENTRY(false,  true, false, false,  true, false);
			case 19: RENDER_SCANLINE_ENTRY(false,  true, false, false,  true,  true);
			case 20: RENDER_SCANLINE_ENTRY(false,  true, false,  true, false, false);
			case 21: RENDER_SCANLINE_ENTRY(false,  true, false,  true, false,  true);
			case 22: RENDER_SCANLINE_ENTRY(false,  true, false,  true,  true, false);
			case 23: RENDER_SCANLINE_ENTRY(false,  true, false,  true,  true,  true);
			case 24: RENDER_SCANLINE_ENTRY(false,  true,  true, false, false, false);
			case 25: RENDER_SCANLINE_ENTRY(false,  true,  true, false, false,  true);
			case 26: RENDER_SCANLINE_ENTRY(false,  true,  true, false,  true, false);
			case 27: RENDER_SCANLINE_ENTRY(false,  true,  true, false,  true,  true);
			case 28: RENDER_SCANLINE_ENTRY(false,  true,  true,  true, false, false);
			case 29: RENDER_SCANLINE_ENTRY(false,  true,  true,  true, false,  true);
			case 30: RENDER_SCANLINE_ENTRY(false,  true,  true,  true,  true, false);
			case 31: RENDER_SCANLINE_ENTRY(false,  true,  true,  true,  true,  true);
			case 32: RENDER_SCANLINE_ENTRY( true, false, false, false, false, false);
			case 33: RENDER_SCANLINE_ENTRY( true, false, false, false, false,  true);
			case 34: RENDER_SCANLINE_ENTRY( true, false, false, false,  true, false);
			case 35: RENDER_SCANLINE_ENTRY( true, false, false, false,  true,  true);
			case 36: RENDER_SCANLINE_ENTRY( true, false, false,  true, false, false);
			case 37: RENDER_SCANLINE_ENTRY( true, false, false,  true, false,  true);
			case 38: RENDER_SCANLINE_ENTRY( true, false, false,  true,  true, false);
			case 39: RENDER_SCANLINE_ENTRY( true, false, false,  true,  true,  true);
			case 40: RENDER_SCANLINE_ENTRY( true, false,  true, false, false, false);
			case 41: RENDER_SCANLINE_ENTRY( true, false,  true, false, false,  true);
			case 42: RENDER_SCANLINE_ENTRY( true, false,  true, false,  true, false);
			case 43: RENDER_SCANLINE_ENTRY( true, false,  true, false,  true,  true);
			case 44: RENDER_SCANLINE_ENTRY( true, false,  true,  true, false, false);
			case 45: RENDER_SCANLINE_ENTRY( true, false,  true,  true, false,  true);
			case 46: RENDER_SCANLINE_ENTRY( true, false,  true,  true,  true, false);
			case 47: RENDER_SCANLINE_ENTRY( true, false,  true,  true,  true,  true);
			case 48: RENDER_SCANLINE_ENTRY( true,  true, false, false, false, false);
			case 49: RENDER_SCANLINE_ENTRY( true,  true, false, false, false,  true);
			case 50: RENDER_SCANLINE_ENTRY( true,  true, false, false,  true, false);
			case 51: RENDER_SCANLINE_ENTRY( true,  true, false, false,  true,  true);
			case 52: RENDER_SCANLINE_ENTRY( true,  true, false,  true, false, false);
			case 53: RENDER_SCANLINE_ENTRY( true,  true, false,  true, false,  true);
			case 54: RENDER_SCANLINE_ENTRY( true,  true, false,  true,  true, false);
			case 55: RENDER_SCANLINE_ENTRY( true,  true, false,  true,  true,  true);
			case 56: RENDER_SCANLINE_ENTRY( true,  true,  true, false, false, false);
			case 57: RENDER_SCANLINE_ENTRY( true,  true,  true, false, false,  true);
			case 58: RENDER_SCANLINE_ENTRY( true,  true,  true, false,  true, false);
			case 59: RENDER_SCANLINE_ENTRY( true,  true,  true, false,  true,  true);
			case 60: RENDER_SCANLINE_ENTRY( true,  true,  true,  true, false, false);
			case 61: RENDER_SCANLINE_ENTRY( true,  true,  true,  true, false,  true);
			case 62: RENDER_SCANLINE_ENTRY( true,  true,  true,  true,  true, false);
			case 63: RENDER_SCANLINE_ENTRY( true,  true,  true,  true,  true,  true);
			}
		}
	}

	render.poly_count = 0;
}

void gorgon_state::render_run(screen_device &screen, bitmap_rgb32 &bitmap)
{
	render_t &render = m_render;

	if (BIT(m_c404.layer_flags, 1) && !BIT(m_c435.spritedata[0], 0))
	{
		bool y_lowres = !BIT(m_c435.spritedata[0], 2);
		s16 deltax = (s16)((s16)m_c435.spritedata[6] + (s16)m_c435.spritedata[10] + 0x50);
		s16 deltay = (s16)((s16)m_c435.spritedata[12] + (0x2a >> (y_lowres ? 1 : 0)));
		u16 base = m_c435.spritedata[2];
		u16 sprite_count = (m_c435.spritedata[4] - base) + 1;
		for (int i = 0; i < sprite_count; i++)
		{
			namcos23_render_entry *re = render.entries[!render.cur] + render.count[!render.cur];

			u16 *data = &m_c435.spritedata[0x4000 + i * 16];
			re->type = SPRITE;
			re->poly_fade_r = m_c404.poly_fade_r;
			re->poly_fade_g = m_c404.poly_fade_g;
			re->poly_fade_b = m_c404.poly_fade_b;
			re->poly_alpha_color = m_c404.poly_alpha_color;
			re->poly_alpha_pen = m_c404.poly_alpha_pen;
			re->poly_alpha = m_c404.poly_alpha;
			re->screen_fade_r = m_c404.screen_fade_r;
			re->screen_fade_g = m_c404.screen_fade_g;
			re->screen_fade_b = m_c404.screen_fade_b;
			re->screen_fade_factor = m_c404.screen_fade_factor;
			re->fade_flags = m_c404.fade_flags;
			re->vp_size_x = m_vp_size_x;
			re->vp_size_y = m_vp_size_y;
			re->vp_offset_x = m_vp_offset_x;
			re->vp_offset_y = m_vp_offset_y;
			re->vp_fov = m_clip_data[23];
			re->absolute_priority = m_absolute_priority;
			re->model_blend_factor = 0;
			re->tx = 0;
			re->ty = 0;
			re->sprite.zcoord = ((m_c404.sprites[i].d[0] << 16) | m_c404.sprites[i].d[1]) & 0x00ffffff;
			re->sprite.xpos = (s16)data[0] - deltax;
			re->sprite.ypos = (s16)data[2] - deltay;
			re->sprite.xsize = (s16)data[4];
			re->sprite.ysize = (s16)data[6];
			re->sprite.rows = data[10] & 7;
			re->sprite.cols = (data[10] >> 4) & 7;
			re->sprite.linktype = data[8] & 0xff;
			re->sprite.alpha = data[14] >> 8;
			re->sprite.code = data[12];
			re->sprite.xflip = BIT(data[10], 7);
			re->sprite.yflip = BIT(data[10], 3);
			re->sprite.color = m_c404.sprites[i].d[2] & 0xff;
			re->sprite.cz = m_c404.sprites[i].d[3] & 0xff;
			re->sprite.fade_enabled = BIT(m_c404.sprites[i].d[3], 15);
			re->sprite.prioverchar = (re->sprite.cz == 0xfe) ? 1 : 0;

			if (re->sprite.rows == 0)
				re->sprite.rows = 8;
			if (re->sprite.cols == 0)
				re->sprite.cols = 8;

			// right justify
			if (BIT(data[10], 9))
				re->sprite.xpos -= re->sprite.xsize * re->sprite.cols - 1;

			// bottom justify
			if (BIT(data[10], 8))
				re->sprite.ypos -= re->sprite.ysize * re->sprite.rows - 1;

			if (re->sprite.yflip)
			{
				re->sprite.ypos += re->sprite.ysize * re->sprite.rows - 1;
				re->sprite.ysize = -re->sprite.ysize;
			}

			if (re->sprite.xflip)
			{
				re->sprite.xpos += re->sprite.xsize * re->sprite.cols - 1;
				re->sprite.xsize = -re->sprite.xsize;
			}

			if (re->sprite.xsize && re->sprite.ysize)
			{
				render.count[!render.cur]++;
			}
		}
	}

	namcos23_state::render_run(screen, bitmap);
}

void gorgon_state::dispatch_render_entry(const namcos23_render_entry *re)
{
	switch (re->type)
	{
	case SPRITE:
		render_sprite(re);
		return;
	default:
		namcos23_state::dispatch_render_entry(re);
		return;
	}
}

void namcos23_state::dispatch_render_entry(const namcos23_render_entry *re)
{
	switch (re->type)
	{
	case MODEL:
		if (m_c404.layer_flags & 1)
			render_model(re);
		return;
	case DIRECT:
		if (m_c404.layer_flags & 1)
			render_direct_poly(re);
		return;
	case IMMEDIATE:
		if (m_c404.layer_flags & 1)
			render_immediate(re);
		return;
	}
}

void namcos23_state::render_run(screen_device &screen, bitmap_rgb32 &bitmap)
{
	render_t &render = m_render;
	const namcos23_render_entry *re = render.entries[!render.cur];

	render.poly_count = 0;
	for (int i = 0; i < render.count[!render.cur]; i++)
	{
		dispatch_render_entry(re);
		re++;
	}

	render.polymgr->render_flush(screen, bitmap);
	render.polymgr->wait();

	render.cur = !render.cur;
	render.count[render.cur] = 0;
}



// C404 (gamma/palette/sprites)

void namcos23_state::sprites_idx_w(offs_t offset, u16 data, u16 mem_mask)
{
	m_c404.spritedata_idx = data >> 1;
	LOGMASKED(LOG_SPRITES, "%s: sprites_idx_w: %d = %04x\n", machine().describe_context(), offset, data);
}

void namcos23_state::sprites_data_w(offs_t offset, u16 data, u16 mem_mask)
{
	m_c404.sprites[(m_c404.spritedata_idx >> 2) % 0x280].d[m_c404.spritedata_idx & 3] = data;
	m_c404.spritedata_idx++;
}

void namcos23_state::paletteram_w(offs_t offset, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_generic_paletteram_32[offset]);

	// each LONGWORD is 2 colors, each OFFSET is 2 colors
	for (int i = 0; i < 2; i++)
	{
		int which = (offset << 2 | i << 1) & 0xfffe;
		int r = nthbyte(m_generic_paletteram_32, which|0x00001);
		int g = nthbyte(m_generic_paletteram_32, which|0x10001);
		int b = nthbyte(m_generic_paletteram_32, which|0x20001);
		m_palette->set_pen_color(which/2, rgb_t(r,g,b));
	}
}



// C404 (mixing, gamma RAM)

u16 namcos23_state::c404_ram_r(offs_t offset)
{
	LOGMASKED(LOG_C404_RAM, "%s: c404_ram_r[%04x]: %04x\n", machine().describe_context(), offset, m_c404.ram[offset]);
	return m_c404.ram[offset];
}

void namcos23_state::c404_ram_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_C404_RAM, "%s: c404_ram_w[%04x] = %04x & %04x\n", machine().describe_context(), offset, data, mem_mask);
	COMBINE_DATA(&m_c404.ram[offset]);
}

void namcos23_state::c404_poly_fade_red_w(offs_t offset, u16 data) // 0
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_fade_red_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_fade_r = m_c404.ram[0x00] = data;
}

void namcos23_state::c404_poly_fade_green_w(offs_t offset, u16 data) // 1
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_fade_green_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_fade_g = m_c404.ram[0x01] = data;
}

void namcos23_state::c404_poly_fade_blue_w(offs_t offset, u16 data) // 2
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_fade_blue_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_fade_b = m_c404.ram[0x02] = data;
}

void namcos23_state::c404_fog_red_w(offs_t offset, u16 data) // 5
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_fog_red_w: %04x\n", machine().describe_context(), data);
	m_c404.fog_r = m_c404.ram[0x05] = data;
}

void namcos23_state::c404_fog_green_w(offs_t offset, u16 data) // 6
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_fog_green_w: %04x\n", machine().describe_context(), data);
	m_c404.fog_g = m_c404.ram[0x06] = data;
}

void namcos23_state::c404_fog_blue_w(offs_t offset, u16 data) // 7
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_fog_blue_w: %04x\n", machine().describe_context(), data);
	m_c404.fog_b = m_c404.ram[0x07] = data;
}

void namcos23_state::c404_bg_red_w(offs_t offset, u16 data) // 8
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_bg_red_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x08] = data;
	m_c404.bgcolor_r = data & 0x00ff;
}

void namcos23_state::c404_bg_green_w(offs_t offset, u16 data) // 9
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_bg_green_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x09] = data;
	m_c404.bgcolor_g = data & 0x00ff;
}

void namcos23_state::c404_bg_blue_w(offs_t offset, u16 data) // a
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_bg_blue_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x0a] = data;
	m_c404.bgcolor_b = data & 0x00ff;
}

void namcos23_state::c404_spot_lsb_w(offs_t offset, u16 data) // d
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_spot_lsb_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x0d] = data;
	m_c404.spot_factor &= 0xff00;
	m_c404.spot_factor |= data & 0x00ff;
}

void namcos23_state::c404_spot_msb_w(offs_t offset, u16 data) // e
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_spot_msb_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x0e] = data;
	m_c404.spot_factor &= 0x00ff;
	m_c404.spot_factor |= (data & 0x00ff) << 8;
}

void namcos23_state::c404_poly_alpha_color_w(offs_t offset, u16 data) // f
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_alpha_color_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_alpha_color = m_c404.ram[0x0f] = data;
}

void namcos23_state::c404_poly_alpha_pen_w(offs_t offset, u16 data) // 10
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_alpha_pen_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_alpha_pen = m_c404.ram[0x10] = data;
}

void namcos23_state::c404_poly_alpha_w(offs_t offset, u16 data) // 11
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_poly_alpha_w: %04x\n", machine().describe_context(), data);
	m_c404.poly_alpha = m_c404.ram[0x11] = data;
}

void namcos23_state::c404_alpha_check12_w(offs_t offset, u16 data) // 12
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_alpha_check12_w: %04x\n", machine().describe_context(), data);
	m_c404.alpha_check12 = m_c404.ram[0x12] = data;
}

void namcos23_state::c404_alpha_check13_w(offs_t offset, u16 data) // 13
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_alpha_check13_w: %04x\n", machine().describe_context(), data);
	m_c404.alpha_check13 = m_c404.ram[0x13] = data;
}

void namcos23_state::c404_text_alpha_mask_w(offs_t offset, u16 data) // 14
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_text_alpha_mask_w: %04x\n", machine().describe_context(), data);
	m_c404.alpha_mask = m_c404.ram[0x14] = data;
}

void namcos23_state::c404_text_alpha_factor_w(offs_t offset, u16 data) // 15
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_text_alpha_factor_w: %04x\n", machine().describe_context(), data);
	m_c404.alpha_factor = m_c404.ram[0x15] = data;
}

void namcos23_state::c404_screen_fade_red_w(offs_t offset, u16 data) // 16
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_screen_fade_red_w: %04x\n", machine().describe_context(), data);
	m_c404.screen_fade_r = m_c404.ram[0x16] = data;
}

void namcos23_state::c404_screen_fade_green_w(offs_t offset, u16 data) // 17
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_screen_fade_green_w: %04x\n", machine().describe_context(), data);
	m_c404.screen_fade_g = m_c404.ram[0x17] = data;
}

void namcos23_state::c404_screen_fade_blue_w(offs_t offset, u16 data) // 18
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_screen_fade_blue_w: %04x\n", machine().describe_context(), data);
	m_c404.screen_fade_b = m_c404.ram[0x18] = data;
}

void namcos23_state::c404_screen_fade_factor_w(offs_t offset, u16 data) // 19
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_screen_fade_factor_w: %04x\n", machine().describe_context(), data);
	m_c404.screen_fade_factor = m_c404.ram[0x19] = data;
}

void namcos23_state::c404_fade_flags_w(offs_t offset, u16 data) // 1a
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_fade_flags_w: %04x\n", machine().describe_context(), data);
	m_c404.fade_flags = m_c404.ram[0x1a] = data;
}

void namcos23_state::c404_palette_base_w(offs_t offset, u16 data) // 1b
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_palette_base_w: %04x\n", machine().describe_context(), data);
	m_c404.ram[0x1b] = data;
	m_c404.palbase = (data << 8) & 0x7f00;
}

void namcos23_state::c404_layer_flags_w(offs_t offset, u16 data) // 1f
{
	LOGMASKED(LOG_C404_REGS, "%s: c404_layer_flags_w: %04x\n", machine().describe_context(), data);
	m_c404.layer_flags = m_c404.ram[0x1f] = data;
}


// Video start/update callbacks

void namcos23_state::video_start()
{
	m_ptrom  = (const u32 *)memregion("pointrom")->base();
	m_tmlrom = (const u16 *)memregion("textilemapl")->base();
	m_tmhrom = memregion("textilemaph")->base();
	m_texrom = memregion("textile")->base();

	m_tileid_mask = (memregion("textilemapl")->bytes()/2 - 1) & ~0xff; // Used for y masking
	m_tile_mask = memregion("textile")->bytes()/256 - 1;

	m_mix_bitmap = std::make_unique<bitmap_ind16>(640, 480);
	m_render.polymgr = std::make_unique<namcos23_renderer>(*this, m_tmlrom, m_tmhrom, m_texrom, m_c412.sram, m_tileid_mask, m_tile_mask);

	m_ptrom_limit = memregion("pointrom")->bytes()/4;
}

void gorgon_state::video_start()
{
	namcos23_state::video_start();
	m_sprrom = memregion("sprites")->base();
}

void namcos23_state::draw_text_layer(screen_device &screen)
{
	// tile width: 16
	// tile height: 16
	// total elements: 0x3c0
	// bitplanes: 4
	// plane offsets: 0, 1, 2, 3
	// bit offset of each horizontal pixel:
	//     0,   4,   8,  12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  60
	// bit offset of each vertical pixel:
	//     0,  64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960
	// tile spacing (in bits): 1024

	// xxxx.----.----.---- palette select
	// ----.xx--.----.---- flip
	// ----.--xx.xxxx.xxxx code

	for (u32 tmy = 0; tmy < 30; tmy++)
	{
		for (u32 ty = 0; ty < 16; ty++)
		{
			const u32 y = (tmy << 4) | ty;
			u16 *dst = &m_mix_bitmap->pix(y);
			u8 *pri = &screen.priority().pix(y);
			const u32 scrolled_y = (y + m_c404.yscroll) & 0x03ff;
			const u32 adjusted_tmy = scrolled_y >> 4;
			const u32 adjusted_ty = scrolled_y & 0xf;
			for (u32 tmx = 0; tmx < 40; tmx++)
			{
				const u16 scroll_x = m_c404.linexscroll[scrolled_y];
				u16 tile_scroll = scroll_x >> 4;
				s16 pix_scroll = scroll_x & 0xf;
				u16 tile_data = util::big_endian_cast<const u16>(m_textram.target())[(adjusted_tmy << 6) | ((tmx + tile_scroll) & 0x3f)];
				u32 pal_select = BIT(tile_data, 12, 4) << 4;
				u32 tile_code = (tile_data & 0x03ff) << 5;
				u8 flipx_mask = BIT(tile_data, 10) ? 0x00 : 0x3c;
				u16 flipy_mask = BIT(tile_data, 11) ? 0x1e : 0x00;
				const u32 char_addr = tile_code | ((adjusted_ty << 1) ^ flipy_mask);
				u64 char_data = ((u64)m_charram[char_addr] << 32) | m_charram[char_addr | 1];
				for (u32 tx = 0; tx < 16; tx++)
				{
					const u8 val = BIT(char_data, ((tx + pix_scroll) << 2) ^ flipx_mask, 4);
					if (val != 0xf)
					{
						*dst = m_c404.palbase | pal_select | val;
						*pri = 4;
					}
					dst++;
					pri++;
					if (pix_scroll && (tx + pix_scroll) == 15)
					{
						tile_data = util::big_endian_cast<const u16>(m_textram.target())[(adjusted_tmy << 6) | ((tmx + tile_scroll + 1) & 0x3f)];
						pal_select = BIT(tile_data, 12, 4) << 4;
						tile_code = (tile_data & 0x03ff) << 5;
						flipx_mask = BIT(tile_data, 10) ? 0x00 : 0x3c;
						flipy_mask = BIT(tile_data, 11) ? 0x1e : 0x00;
						const u32 char_addr = tile_code | ((adjusted_ty << 1) ^ flipy_mask);
						char_data = ((u64)m_charram[char_addr] << 32) | m_charram[char_addr | 1];
						pix_scroll -= 16;
					}
				}
			}
		}
	}
}

void namcos23_state::mix_text_layer(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect, int prival)
{
	const pen_t *pens = m_palette->pens();
	u8 pen = 0;

	// prepare fader
	const bool fade_enabled = (m_c404.fade_flags & 2) && m_c404.screen_fade_factor;
	const s32 fadefactor = 0xff - m_c404.screen_fade_factor;
	const s32 fadefactor_inv = 0x100 - fadefactor;
	const s32 fadecolor_r = m_c404.screen_fade_r;
	const s32 fadecolor_g = m_c404.screen_fade_g;
	const s32 fadecolor_b = m_c404.screen_fade_b;
	const s32 alphafactor = 0xff - m_c404.alpha_factor;
	const s32 alphafactor_inv = 0x100 - alphafactor;

	// mix textlayer with poly/sprites
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		u16 const *const src = &m_mix_bitmap->pix(y);
		u8 const *const pri = &screen.priority().pix(y);
		u32 *dst = &bitmap.pix(y);
		for (int x = cliprect.min_x; x <= cliprect.max_x; x++, dst++)
		{
			// skip if transparent or under poly/sprite
			if (pri[x] == prival)
			{
				const u32 rgb = pens[src[x]];
				s32 r = (s32)((rgb >> 16) & 0xff);
				s32 g = (s32)((rgb >> 8) & 0xff);
				s32 b = (s32)(rgb & 0xff);
				pen = src[x];

				// apply fade
				if (fade_enabled)
				{
					r = ((r * fadefactor) + (fadecolor_r * fadefactor_inv)) >> 8;
					g = ((g * fadefactor) + (fadecolor_g * fadefactor_inv)) >> 8;
					b = ((b * fadefactor) + (fadecolor_b * fadefactor_inv)) >> 8;
				}

				// apply alpha
				if (m_c404.alpha_factor && ((pen & 0xf) == m_c404.alpha_mask || (pen >= m_c404.alpha_check12 && pen <= m_c404.alpha_check13)))
				{
					const u32 drgb = *dst;
					const s32 dr = (s32)((drgb >> 16) & 0xff);
					const s32 dg = (s32)((drgb >> 8) & 0xff);
					const s32 db = (s32)(drgb & 0xff);
					r = ((r * alphafactor) + (dr * alphafactor_inv)) >> 8;
					g = ((g * alphafactor) + (dg * alphafactor_inv)) >> 8;
					b = ((b * alphafactor) + (db * alphafactor_inv)) >> 8;
				}

				*dst = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
	}
}

u32 gorgon_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (m_c404.layer_flags & 2 && !machine().video().skip_this_frame())
		recalc_czram();

	return namcos23_state::screen_update(screen, bitmap, cliprect);
}

u32 namcos23_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (machine().video().skip_this_frame())
	{
		m_render.cur = !m_render.cur;
		m_render.count[m_render.cur] = 0;
		m_render.poly_count = 0;

		return UPDATE_HAS_NOT_CHANGED;
	}

	screen.priority().fill(0, cliprect);

	// background color
	u32 bgcolor = 0xff000000;
	s32 bg_r = (s32)m_c404.bgcolor_r;
	s32 bg_g = (s32)m_c404.bgcolor_g;
	s32 bg_b = (s32)m_c404.bgcolor_b;
	if (m_c404.fade_flags & 1 && m_c404.screen_fade_factor)
	{
		const s32 scale1 = (s32)(0xff - m_c404.screen_fade_factor);
		const s32 scale2 = 0x100 - scale1;
		bg_r = ((bg_r * scale1) + ((s32)m_c404.screen_fade_r * scale2)) >> 8;
		bg_g = ((bg_g * scale1) + ((s32)m_c404.screen_fade_g * scale2)) >> 8;
		bg_b = ((bg_b * scale1) + ((s32)m_c404.screen_fade_b * scale2)) >> 8;
	}
	bgcolor |= (bg_r << 16) | (bg_g << 8) | bg_b;
	bitmap.fill(bgcolor, cliprect);

	if (m_c404.layer_flags & 4)
	{
		apply_text_scroll();

		draw_text_layer(screen);
		mix_text_layer(screen, bitmap, cliprect, 4);
	}

	render_run(screen, bitmap);

	if (m_c404.layer_flags & 4)
	{
		mix_text_layer(screen, bitmap, cliprect, 6);
	}

	return 0;
}





/***************************************************************************

  Main CPU I/O + Memory Map
  (some cpu->video I/O handled above)

***************************************************************************/

// Interrupts

void namcos23_state::irq_update_common(u32 cause)
{
	const u32 old = m_main_irqcause;
	const u32 changed = cause ^ m_main_irqcause;
	m_main_irqcause = cause;

	// vblank
	if (changed & MAIN_VBLANK_IRQ)
	{
		if (m_main_irqcause & MAIN_VBLANK_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, main VBL\n", m_vbl_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_VBLANK_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, main VBL\n", m_vbl_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_vbl_irqnum, (cause & MAIN_VBLANK_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}

	// C422
	if (changed & MAIN_C422_IRQ)
	{
		if (m_main_irqcause & MAIN_C422_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C422\n", m_c422_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_C422_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, C422\n", m_c422_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c422_irqnum, (cause & MAIN_C422_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}
}

void namcos23_state::irq_update(u32 cause)
{
	const u32 old = m_main_irqcause;
	const u32 changed = cause ^ m_main_irqcause;

	irq_update_common(cause);

	// C361/subcpu
	if (changed & (MAIN_C361_IRQ | MAIN_SUBCPU_IRQ))
	{
		if (m_main_irqcause & (MAIN_C361_IRQ | MAIN_SUBCPU_IRQ))
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C361(%d) || SubCPU(%d)\n", m_c361_irqnum - MIPS3_IRQ0, (m_main_irqcause & MAIN_C361_IRQ) ? 1 : 0, (m_main_irqcause & MAIN_SUBCPU_IRQ) ? 1 : 0);
		else if (old & (MAIN_C361_IRQ | MAIN_SUBCPU_IRQ))
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d\n", m_c361_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c361_irqnum, (cause & (MAIN_C361_IRQ | MAIN_SUBCPU_IRQ)) ? ASSERT_LINE : CLEAR_LINE);
	}

	// C435
	if (changed & MAIN_C435_IRQ && m_c435_irqnum >= 0)
	{
		if (m_main_irqcause & MAIN_C435_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C435\n", m_c435_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_C435_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, C435\n", m_c435_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c435_irqnum, (cause & MAIN_C435_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}
}

void crszone_state::irq_update(u32 cause)
{
	const u32 old = m_main_irqcause;
	const u32 changed = cause ^ m_main_irqcause;

	irq_update_common(cause);

	// RS232
	if (changed & MAIN_RS232_IRQ)
	{
		if (m_main_irqcause & MAIN_RS232_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, RS232\n", m_rs232_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_RS232_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, RS232\n", m_rs232_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_rs232_irqnum, (cause & MAIN_RS232_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}

	// C361
	if (changed & MAIN_C361_IRQ)
	{
		if (m_main_irqcause & MAIN_C361_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C361\n", m_c361_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_C361_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, C361\n", m_c361_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c361_irqnum, (cause & MAIN_C361_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}

	// SubCPU
	if (changed & MAIN_SUBCPU_IRQ)
	{
		if (m_main_irqcause & MAIN_SUBCPU_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, SubCPU\n", m_sub_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_SUBCPU_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, SubCPU\n", m_sub_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_sub_irqnum, (cause & MAIN_SUBCPU_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}

	// C450
	if (changed & MAIN_C450_IRQ && m_c450_irqnum >= 0)
	{
		if (m_main_irqcause & MAIN_C450_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C450\n", m_c450_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_C450_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, C450\n", m_c450_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c450_irqnum, (cause & MAIN_C450_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}

	// C451
	if (changed & MAIN_C451_IRQ && m_c451_irqnum >= 0)
	{
		if (m_main_irqcause & MAIN_C451_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Raising IRQ%d, C451\n", m_c451_irqnum - MIPS3_IRQ0);
		else if (old & MAIN_C451_IRQ)
			LOGMASKED(LOG_IRQ_STATUS, "Lowering IRQ%d, C451\n", m_c451_irqnum - MIPS3_IRQ0);
		m_maincpu->set_input_line(m_c451_irqnum, (cause & MAIN_C451_IRQ) ? ASSERT_LINE : CLEAR_LINE);
	}
}

void namcos23_state::subcpu_irq1_update(int state)
{
	m_subcpu->set_input_line(INPUT_LINE_IRQ1, state ? ASSERT_LINE : CLEAR_LINE);
	m_sub_port8 = state ? (m_sub_port8 & ~0x02) : (m_sub_port8 | 0x02); // IRQ1 pin
}

void namcos23_state::vblank(int state)
{
	if (state)
	{
		// TEMP DEBUG (phase 9d): poll link-state vars + timeout counter.
		// add_fastram bypasses install_write_tap, so polling is the only way
		// to observe these writes. REMOVE BEFORE COMMIT.
		static u16 last_link = 0xffff;
		static u16 last_timeout = 0xffff;
		static u16 last_succ = 0xffff;
		static u32 frame_count = 0;
		++frame_count;
		u16 const cur_link    = u16(m_mainram[0x002CEDDC / 4] >> 16);
		u16 const cur_timeout = u16(m_mainram[0x002F3504 / 4] >> 16);
		u16 const cur_succ    = u16(m_mainram[0x002CEDE8 / 4] >> 16); // gp+0x75C8 validator success counter
		u16 const cur_counter = u16(m_mainram[0x002CEDD8 / 4] >> 16); // gp+0x75B8 state-machine call counter
		if (cur_link != last_link || cur_timeout != last_timeout || cur_succ != last_succ)
		{
			logerror("namcos23: phase9 frame=%u link=%04x timeout=%04x succ=%04x counter=%u\n",
					frame_count, cur_link, cur_timeout, cur_succ, cur_counter);
			last_link = cur_link;
			last_timeout = cur_timeout;
			last_succ = cur_succ;
		}

		// PHASE 9D: push our state counter to C139 every frame so the
		// heartbeat replay can stamp current bytes 0-1.  REMOVE BEFORE COMMIT.
		if (m_c139)
			m_c139->set_local_counter(cur_counter);

		// P001 EXPERIMENT (H1 "keepalive keystone", cherry-picked from
		// patch/keepalive-floor onto patch/continuous-arm): floor the in-game
		// link keepalive word 0x802F3FD8 while linked gameplay is staged
		// (mode word 0x802F3FD0 == 2).  ROM provenance (full.txt, all word
		// lw/sw): RX service 0x800B2968 gates on the mode word (lw/bne
		// 0x800B2990-98), refills the keepalive to 0x10 only while it is
		// still > 0 (bgtz 0x800B29C4, sw 0x800B29E8; sibling top-up
		// 0x80023840-58 likewise), and once it reads 0 the one-way
		// COM-fallback 0x800B2A48-98 clears partner record +0x370 bits
		// 0x40000000|0x2000; round-start arming samples it too (blez
		// 0x8001313C).  Flooring (raising to the floor value only when the
		// counter is below it, never lowering a refilled counter) re-enables
		// the ROM's own refill/arming paths.  Enhancement over P001: the
		// floor value comes from the env var's numeric value ("1" = exact
		// P001 semantics, only a 0 counter is touched; "2" keeps the
		// within-frame service decrement from ever reaching 0, so the
		// COM-fallback cannot fire at all).  Ordered BEFORE the other
		// experiment blocks below so a freshly floored keepalive reads as
		// alive in the same vblank pass.  Same m_mainram[phys / 4] word
		// conversion as the phase 9d taps above.  +0x370 words: record 0 =
		// 0x802F43D0, record 1 = 0x802F4844 (0x802F4060 + idx*0x474 + 0x370);
		// the partner is record 1 - [gp+0x7608], gp = 0x802C7820 (boot code
		// 0x80000384-88), so gp+0x7608 = 0x802CEE28.  Experiment branch only -
		// never merge to milestone.
		if (m_patch_keepalive_floor)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4]; // record 0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4]; // record 1 +0x370
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			if (staging_mode == 2 && keepalive < m_keepalive_floor_value)
			{
				m_mainram[0x002F3FD8 / 4] = m_keepalive_floor_value;
				logerror("KEEPALIVE_FLOOR: floored frame=%u t=%.3fs pre=%u post=%u rec0+0x370=%08x rec1+0x370=%08x localidx=%u\n",
						frame_count, machine().time().as_double(), keepalive, m_mainram[0x002F3FD8 / 4],
						rec0_flags, rec1_flags, local_idx);
			}
			if (frame_count % 60 == 0)
				logerror("KEEPALIVE_FLOOR: status frame=%u mode=%u keepalive=%u rec0+0x370=%08x rec1+0x370=%08x localidx=%u\n",
						frame_count, staging_mode, m_mainram[0x002F3FD8 / 4], rec0_flags, rec1_flags, local_idx);
		}

		// P002 EXPERIMENT (H2 "drift lockstep", branch patch/vblank-lockstep):
		// 1/s status tap.  Per vblank, accumulate the max of the ROM's
		// frame-drift word 0x802F3504 (= cur_timeout sampled by the phase 9d
		// tap above; >= 0x11 declares the link idle and clears the freshness
		// byte 0x802F3502) and the min of the keepalive word 0x802F3FD8 over
		// mode-2 samples (staging mode word 0x802F3FD0 == 2 = linked
		// gameplay); every 60 vblanks emit one status line including the
		// token/stall counters from the C139 barrier.  Same m_mainram[phys/4]
		// word idiom as the phase 9d tap and the P001 block.  Experiment
		// branch only - never merge to milestone.
		if (m_patch_vblank_lockstep)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8
			if (cur_timeout > m_lockstep_tap_max_drift)
				m_lockstep_tap_max_drift = cur_timeout;
			if (staging_mode == 2)
			{
				++m_lockstep_tap_mode2_samples;
				if (keepalive < m_lockstep_tap_min_keepalive)
					m_lockstep_tap_min_keepalive = keepalive;
			}
			if (frame_count % 60 == 0)
			{
				namco_c139_device::lockstep_stats const st =
						m_c139 ? m_c139->get_lockstep_stats() : namco_c139_device::lockstep_stats{};
				// min keepalive logged as -1 when no mode-2 vblank fell in this window
				int const minka = m_lockstep_tap_mode2_samples ? int(m_lockstep_tap_min_keepalive) : -1;
				logerror("VBLANK_LOCKSTEP: status frame=%u t=%.3fs mode=%u maxdrift=%04x minka=%d mode2=%u tok_tx=%u tok_rx=%u local=%u peer=%u stalls=%u timeouts=%u stall_ms=%u susp=%u\n",
						frame_count, machine().time().as_double(), staging_mode,
						m_lockstep_tap_max_drift, minka, m_lockstep_tap_mode2_samples,
						st.tokens_tx, st.tokens_rx, st.local_frame, st.peer_token,
						st.stall_events, st.stall_timeouts, unsigned(st.stall_us / 1000),
						st.suspended ? 1 : 0);
				m_lockstep_tap_max_drift = 0;
				m_lockstep_tap_min_keepalive = 0xffffffff;
				m_lockstep_tap_mode2_samples = 0;
			}
		}

		// P002 EXPERIMENT (branch patch/vblank-lockstep): per-vblank
		// frame-token send + bounded barrier stall.  Self-gated inside the
		// device on NAMCOS23_PATCH_VBLANK_LOCKSTEP - inert when unset.
		if (m_c139)
			m_c139->vblank_tick();

		// P021 EXPERIMENT (branch patch/linked-gate-tx-only): push the WIRE-ONLY
		// 0x6000 advertise gate to the device every vblank.  Only the driver can
		// read the staging mode word (0x802F3FD0) and the env arm; the device
		// performs the OR-only op55 wire-flag injection in emit_tx_frame.  This is
		// a READ-ONLY mode-word read + a push: it does NOT write the LOCAL +0x370
		// record (the whole point of P021 vs P020 - the live record stays clean so
		// the gun-actor 0x80013644 never sees 0x2000).  Pushed even when not armed
		// (the device stays inert), so the gate tracks mode==2 transitions exactly.
		// Experiment branch only - never merge to milestone.
		if (m_c139 && m_patch_linked_gate_txonly)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0 (READ-ONLY)
			m_c139->set_linked_gate_txonly(true, staging_mode == 2);
		}

		// P060 EXPERIMENT (branch patch/hb-phase-aware): push the staging-phase
		// signal to the device every vblank.  Only the driver can read the
		// staging mode word (0x802F3FD0, READ-ONLY here); the device debounces
		// (>= 60 consecutive mode-2 vblanks -> FAST heartbeat token cadence,
		// SAFE immediately on any non-mode-2 vblank) and its heartbeat re-arm
		// sites pick the cadence via hb_cadence_effective_ms().  Pushed on BOTH
		// cabs - the token that releases RED's emits is BLUE's heartbeat and
		// vice versa.  No RAM write, no regs touch - a read + a bool push.
		// Experiment branch only - never merge to milestone.
		// P061 (branch patch/tx-complete-irq): the same push also feeds the
		// TX-complete dispatch model's mode-2 gate (the device debounces once,
		// both consumers read the shared state) - so push when EITHER is armed.
		// P063 (branch patch/tx-complete-v2): third consumer, same shared
		// debounce - push when ANY of the three is armed.
		if (m_c139 && (m_patch_hb_phase_aware || m_patch_tx_complete_irq
				|| m_patch_tx_complete_v2))
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0 (READ-ONLY)
			m_c139->set_ingame(staging_mode == 2, staging_mode);
		}

		// P003 EXPERIMENT (H3 "round-start arming race", branch
		// patch/round-start-arm): MAME-side partner re-arm.  ROM provenance
		// (full.txt via the 2026-06-10 investigation log): round-start
		// 0x80013058 samples the keepalive word 0x802F3FD8 ONCE (blez
		// 0x8001313C) and only then ORs bit 0x40000000 ("ingest armed") into
		// the partner record's +0x370 word; the COM-fallback 0x800B2A58-98
		// clears bits 0x40000000|0x2000 whenever the keepalive drains, and
		// NO ROM path re-arms mid-stage (refill 0x800B29E8 / top-up
		// 0x80023840-58 touch only the counter; op55 / the 0x80020xxx
		// handshake never run mid-stage).  So: whenever the keepalive
		// transitions 0 -> positive between vblank samples while linked
		// gameplay is staged (mode word 0x802F3FD0 == 2), re-set bit
		// 0x40000000 on the partner record so op02 coordinate import (gate
		// 0x800AB3B8-C4) resumes.  Addresses (same as the P001 block):
		// +0x370 words rec0 = 0x802F43D0, rec1 = 0x802F4844 (0x802F4060 +
		// idx*0x474 + 0x370); partner = record 1 - [gp+0x7608], gp+0x7608 =
		// 0x802CEE28 (gp = 0x802C7820).  Read-only observability: bit 0x2000
		// ("remote-controlled / link-alive") transitions on either record,
		// and partner +0x370 low-24 changes (op02 ingest evidence, ledger
		// P003) counted into a 1/s status line.  Experiment branch only -
		// never merge to milestone.
		if (m_patch_round_start_arm)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4]; // record 0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4]; // record 1 +0x370
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			bool const partner_is_rec1 = (local_idx == 0);
			offs_t const partner_word = partner_is_rec1 ? (0x002F4844 / 4) : (0x002F43D0 / 4);

			if (m_rsa_have_prev)
			{
				// Keepalive refill edge (0 -> positive) in linked gameplay:
				// re-arm the partner's ingest bit.
				if (staging_mode == 2 && m_rsa_prev_keepalive == 0 && keepalive != 0)
				{
					u32 const pre = m_mainram[partner_word];
					if (!(pre & 0x40000000))
					{
						m_mainram[partner_word] = pre | 0x40000000;
						logerror("ROUND_START_ARM: re-armed frame=%u t=%.3fs keepalive %u->%u partner=rec%u pre=%08x post=%08x localidx=%u\n",
								frame_count, machine().time().as_double(),
								m_rsa_prev_keepalive, keepalive,
								partner_is_rec1 ? 1 : 0, pre, m_mainram[partner_word], local_idx);
					}
					else
					{
						logerror("ROUND_START_ARM: refill edge frame=%u keepalive %u->%u partner=rec%u already armed (%08x)\n",
								frame_count, m_rsa_prev_keepalive, keepalive,
								partner_is_rec1 ? 1 : 0, pre);
					}
				}

				// READ-ONLY: bit 0x2000 transitions on either record (the
				// gun-actor "remote-driven" bit - never observed set in the
				// P001 runs; any edge here is signal).
				if ((rec0_flags ^ m_rsa_prev_rec0) & 0x2000)
					logerror("ROUND_START_ARM: rec0 bit13 %u->%u frame=%u t=%.3fs rec0=%08x\n",
							(m_rsa_prev_rec0 >> 13) & 1, (rec0_flags >> 13) & 1,
							frame_count, machine().time().as_double(), rec0_flags);
				if ((rec1_flags ^ m_rsa_prev_rec1) & 0x2000)
					logerror("ROUND_START_ARM: rec1 bit13 %u->%u frame=%u t=%.3fs rec1=%08x\n",
							(m_rsa_prev_rec1 >> 13) & 1, (rec1_flags >> 13) & 1,
							frame_count, machine().time().as_double(), rec1_flags);

				// READ-ONLY: op02 ingest evidence - count vblanks on which
				// the partner +0x370 low-24 bits changed (op02 rewrites the
				// status portion of the record; ledger P003 watch item).
				u32 const partner_cur  = partner_is_rec1 ? rec1_flags : rec0_flags;
				u32 const partner_prev = partner_is_rec1 ? m_rsa_prev_rec1 : m_rsa_prev_rec0;
				if ((partner_cur ^ partner_prev) & 0x00ffffff)
					++m_rsa_low24_changes;
			}

			if (frame_count % 60 == 0)
			{
				logerror("ROUND_START_ARM: status frame=%u mode=%u keepalive=%u rec0+0x370=%08x rec1+0x370=%08x localidx=%u low24_changes=%u\n",
						frame_count, staging_mode, keepalive, rec0_flags, rec1_flags,
						local_idx, m_rsa_low24_changes);
				m_rsa_low24_changes = 0;
			}

			m_rsa_prev_keepalive = keepalive;
			m_rsa_prev_rec0 = rec0_flags;
			m_rsa_prev_rec1 = rec1_flags;
			m_rsa_have_prev = true;
		}

		// P008 EXPERIMENT (branch patch/continuous-arm): CONTINUOUS partner
		// re-arm.  Rationale (P002/P003 run-1 analysis, 2026-06-10): the ROM
		// arms the partner's ingest bit 0x40000000 exactly once at round
		// start (blez 0x8001313C) and the within-frame COM-fallback
		// 0x800B2A8C-98 clears it again on any keepalive drain; P003's
		// 0->positive edge trigger fired once and lost that race, and with
		// the P001 floor active the keepalive never reads 0 at vblank so the
		// edge cannot occur at all.  So instead: EVERY vblank, while linked
		// gameplay is staged (mode word 0x802F3FD0 == 2) and the keepalive
		// 0x802F3FD8 is alive (> 0), re-set bit 0x40000000 on the partner
		// record's +0x370 word if it is clear.  The worst the fallback can do
		// is hold the bit down for the remainder of one frame; every frame
		// with a live keepalive starts armed, so the op02 import gate
		// (0x800AB3B8-C4) sees an armed destination each frame.  Ordered
		// AFTER the P001 floor block above so a freshly floored keepalive
		// counts as alive in the same vblank pass.  Addresses identical to
		// the P001/P003 blocks.  Experiment branch only - never merge to
		// milestone.
		if (m_patch_continuous_arm)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8 (post-floor)
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			bool const partner_is_rec1 = (local_idx == 0);
			offs_t const partner_word = partner_is_rec1 ? (0x002F4844 / 4) : (0x002F43D0 / 4);
			u32 const pre = m_mainram[partner_word];

			if (staging_mode == 2 && keepalive != 0 && !(pre & 0x40000000))
			{
				m_mainram[partner_word] = pre | 0x40000000;
				++m_ca_rearm_count;
				logerror("CONTINUOUS_ARM: re-armed frame=%u t=%.3fs keepalive=%u partner=rec%u pre=%08x post=%08x localidx=%u count=%u\n",
						frame_count, machine().time().as_double(), keepalive,
						partner_is_rec1 ? 1 : 0, pre, m_mainram[partner_word],
						local_idx, m_ca_rearm_count);
			}

			if (frame_count % 60 == 0)
			{
				// rec words re-read so the status line reflects any re-arm
				// write made just above
				logerror("CONTINUOUS_ARM: status frame=%u mode=%u keepalive=%u rec0+0x370=%08x rec1+0x370=%08x localidx=%u rearm_total=%u\n",
						frame_count, staging_mode, keepalive,
						m_mainram[0x002F43D0 / 4], m_mainram[0x002F4844 / 4],
						local_idx, m_ca_rearm_count);
			}
		}

		// P009 EXPERIMENT (branch patch/qual-trace): READ-ONLY qualification
		// event tap.  ROM RX pipeline (gold/rom 30-netplay/40-gameplay-sync,
		// instruction-verified there):
		//   delivery (C139 IRQ) -> ring slot enqueue (head gp+0x75BE, 4-bit)
		//   -> drain validator 0x8000BF38 Phase A (tail gp+0x75BA; length
		//   [4..0x400] + marker + zero byte-checksum) -> on PASS jal
		//   0x8000B8F0: remote_seq -> 0x802F3510, body bytes -> 0x802F3502/03,
		//   payload len -> 0x802F3100, drift word 0x802F3504 = local_seq -
		//   remote_seq (the RESET we call "qualifying"); on FAIL gp+0x75C8++.
		//   With no pending slot the fallback bumps 0x802F3504 (saturating at
		//   0x11 = link idle).
		// Detection per vblank: remote-seq mirror change OR drift decrease =
		// at least one qualifying ingest since the last vblank; gp+0x75C8
		// delta = checksum failures; head/tail deltas (mod 16) = per-stage
		// flow.  Pair events with the device-side QUAL_TRACE rx fingerprints
		// by t= (deliveries ~17/s = one every 3-4 vblanks, so the pairing is
		// usually unambiguous; multiple ingests inside one vblank window
		// collapse into one event line - documented limitation).
		// gp = 0x802C7820: gp+0x75BA = 0x802CEDDA, gp+0x75BE = 0x802CEDDE.
		// Experiment branch only - never merge to milestone.
		if (m_trace_qual)
		{
			u16 const head    = u16(m_mainram[0x002CEDDC / 4]);       // gp+0x75BE ring head (4-bit)
			u16 const tail    = u16(m_mainram[0x002CEDD8 / 4]);       // gp+0x75BA drain tail
			u16 const rseq    = u16(m_mainram[0x002F3510 / 4] >> 16); // remote_seq mirror (B8F0)
			u32 const bodyw   = m_mainram[0x002F3500 / 4];            // 0x802F3502/03 body byte mirrors
			u8  const body0   = u8(bodyw >> 8);
			u8  const op      = u8(bodyw);
			u16 const plen    = u16(m_mainram[0x002F3100 / 4] >> 16); // payload length (B8F0)
			u32 const hdrw    = m_mainram[0x002F3FFC / 4];            // rolling header bytes FFC..FFF (H4 watches FFD bit0/bit2)
			u32 const mode    = m_mainram[0x002F3FD0 / 4];
			u32 const ka      = m_mainram[0x002F3FD8 / 4];
			// cur_timeout = drift word 0x802F3504, cur_succ = gp+0x75C8,
			// cur_link = gp+0x75BC, cur_counter = gp+0x75B8 (phase 9d tap)

			if (m_qt_have_prev)
			{
				if (rseq != m_qt_prev_rseq || cur_timeout < m_qt_prev_drift)
				{
					++m_qt_win_quals;
					logerror("QUAL_TRACE: qual frame=%u t=%.6f rseq %04x->%04x drift %04x->%04x body0=%02x op=%02x plen=%u hdr3FFC=%08x ka=%u\n",
							frame_count, machine().time().as_double(),
							m_qt_prev_rseq, rseq, m_qt_prev_drift, cur_timeout,
							body0, op, plen, hdrw, ka);
				}
				if (cur_succ != m_qt_prev_succ)
				{
					m_qt_win_chkfails += u16(cur_succ - m_qt_prev_succ);
					logerror("QUAL_TRACE: chkfail frame=%u t=%.6f succ75C8 %04x->%04x head=%u tail=%u drift=%04x rseq=%04x\n",
							frame_count, machine().time().as_double(),
							m_qt_prev_succ, cur_succ, head, tail, cur_timeout, rseq);
				}
				m_qt_win_enq   += u16(head - m_qt_prev_head) & 0xf;
				m_qt_win_drain += u16(tail - m_qt_prev_tail) & 0xf;
			}

			if (frame_count % 60 == 0)
			{
				logerror("QUAL_TRACE: status frame=%u t=%.6f mode=%u drift=%04x rseq=%04x head=%u tail=%u enq=%u drain=%u quals=%u chkfails=%u link75BC=%04x counter=%u plen=%u hdr3FFC=%08x ka=%u\n",
						frame_count, machine().time().as_double(), mode,
						cur_timeout, rseq, head, tail,
						m_qt_win_enq, m_qt_win_drain, m_qt_win_quals, m_qt_win_chkfails,
						cur_link, cur_counter, plen, hdrw, ka);
				m_qt_win_enq = 0;
				m_qt_win_drain = 0;
				m_qt_win_quals = 0;
				m_qt_win_chkfails = 0;
			}

			m_qt_prev_drift = cur_timeout;
			m_qt_prev_succ = cur_succ;
			m_qt_prev_rseq = rseq;
			m_qt_prev_head = head;
			m_qt_prev_tail = tail;
			m_qt_have_prev = true;
		}

		// P026 PART 2 EXPERIMENT (branch patch/reasm-chunk-passthru): READ-ONLY
		// phase9-validator trace.  Static provenance + resolved 75C8/75BC
		// semantics: see the member-block comment (gp+0x75C8 = CHECKSUM-FAIL
		// counter, increment 0x8000C004-0C on byte-sum!=0, beqzl 0x8000BFFC
		// skips it to the jal 0x8000B8F0 dispatch on sum==0; gp+0x75BC =
		// tri-state last-outcome 0/1/2 = validated/chkfail/timeout).  The
		// dispatch buffer 0x802F3510 receives the drained cells of EVERY slot
		// (written during summing, BEFORE the pass/fail branch), so a head
		// change = one-or-more drains this vblank; the same-vblank 75C8 delta
		// attributes pass vs fail.  cur_link/cur_succ/cur_timeout/cur_counter
		// come from the always-on phase-9d sample at the top of this function
		// (cur_succ IS gp+0x75C8 - kept under its historic name there; this
		// tap reports it as what it is, chkfail75C8).  READ-ONLY: m_mainram
		// reads only, no game-RAM writes.  Experiment branch only - never
		// merge to milestone.
		if (m_trace_phase9val)
		{
			u32 const buf0     = m_mainram[0x002F3510 / 4];            // dispatch buffer bytes 0-3 (remote ctr + body head)
			u32 const buf1     = m_mainram[0x002F3514 / 4];            // dispatch buffer bytes 4-7
			u32 const bodyw    = m_mainram[0x002F3500 / 4];            // bytes 3500-3503: plen mirror hi / fresh 3502 / op 3503
			u8  const fresh    = u8(bodyw >> 8);                       // freshness byte 0x802F3502 ("gate3502")
			u8  const op3503   = u8(bodyw);                            // body byte 0x802F3503
			u16 const plen     = u16(m_mainram[0x002F3100 / 4] >> 16); // payload length (B8F0)
			u16 const ring_wr  = u16(m_mainram[0x002CEDDC / 4]) & 0xf; // gp+0x75BE scanner enqueue nibble
			u16 const ring_rd  = u16(m_mainram[0x002CEDD8 / 4]) & 0xf; // gp+0x75BA drain stop nibble
			u32 const ka       = m_mainram[0x002F3FD8 / 4];
			u32 const mode     = m_mainram[0x002F3FD0 / 4];

			if (m_p9v_have_prev)
			{
				u16 const chkfail_delta = u16(cur_succ - m_p9v_prev_chkfail);
				if (buf0 != m_p9v_prev_buf0 || buf1 != m_p9v_prev_buf1)
				{
					++m_p9v_win_drains;
					logerror("PHASE9VAL: drain frame=%u t=%.6f buf3510=%08x%08x plen=%u drift=%04x link=%04x chkfail_delta=%u ring=%u/%u\n",
							frame_count, machine().time().as_double(), buf0, buf1,
							plen, cur_timeout, cur_link, chkfail_delta, ring_wr, ring_rd);
				}
				if (cur_succ != m_p9v_prev_chkfail)
				{
					m_p9v_win_chkfails += chkfail_delta;
					logerror("PHASE9VAL: chkfail frame=%u t=%.6f 75C8 %04x->%04x buf3510=%08x%08x plen=%u drift=%04x ring=%u/%u\n",
							frame_count, machine().time().as_double(),
							m_p9v_prev_chkfail, cur_succ, buf0, buf1, plen,
							cur_timeout, ring_wr, ring_rd);
				}
				if (cur_link != m_p9v_prev_link)
					logerror("PHASE9VAL: link frame=%u t=%.6f 75BC %04x->%04x (0=validated 1=chkfail 2=timeout) drift=%04x fresh3502=%02x chkfails75C8=%04x\n",
							frame_count, machine().time().as_double(),
							m_p9v_prev_link, cur_link, cur_timeout, fresh, cur_succ);
			}

			if (frame_count % 60 == 0)
			{
				logerror("PHASE9VAL: status frame=%u t=%.6f link=%04x chkfails75C8=%04x drains_win=%u chkfails_win=%u drift=%04x fresh3502=%02x op3503=%02x plen=%u rseq=%04x ring=%u/%u ctr75B8=%u ka=%u mode=%u\n",
						frame_count, machine().time().as_double(), cur_link, cur_succ,
						m_p9v_win_drains, m_p9v_win_chkfails, cur_timeout, fresh,
						op3503, plen, u16(buf0 >> 16), ring_wr, ring_rd,
						cur_counter, ka, mode);
				m_p9v_win_drains = 0;
				m_p9v_win_chkfails = 0;
			}

			m_p9v_prev_link = cur_link;
			m_p9v_prev_chkfail = cur_succ;
			m_p9v_prev_buf0 = buf0;
			m_p9v_prev_buf1 = buf1;
			m_p9v_have_prev = true;
		}

		// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace):
		// READ-ONLY adoption / render-gate / cutscene-timer trace.  P014 proved
		// the 718B op-0x17/0x71/0xfe actor/scene batch reassembles, crosses the
		// link, and BLUE DMA-ingests it (its cutscene advances to shot 0x118003
		// on delivery) but then FREEZES there, rendering environment but never
		// characters, with bit 0x2000 (render gate) dark and the 0x0080d0
		// render-window low-24 signature absent on both cabs.  This tap decides
		// between (a) wrong-slot ingest, (b) render-gate gated on a ready-bit
		// that never sets, (c) a missing 2nd link direction, by observing AFTER
		// each ingest:
		//   - the render-gate state: bit 0x2000 + the 0x0080d0 low-24 composite
		//     on both +0x370 records (rec0=0x802F43D0, rec1=0x802F4844).  The
		//     ONLY ROM writer of bit 0x2000 into +0x370 is op-55 RX -> 0x80013D44
		//     (sw 0x80013E10) copying the PEER's 24-bit wire flags verbatim; if
		//     bit 0x2000 never appears, blue is not receiving an op-55 frame that
		//     carries it -> a missing 2nd direction (c) OR an unset peer-side
		//     ready-bit (b).
		//   - the gp+0x7074==2 full-link gate (derived 0x80016BCC from partner
		//     +0x370 & 0x6000 == 0x6000) and the gp+0x7054 cutscene timer (op-70
		//     adoption store 0x80016E80, gated 0x80016E58 bit 0x2000 / 0x80016E64
		//     gp+0x7074==2): does the cutscene timer advance after each bulk
		//     delivery then STOP at the 0x118003 freeze, and is it pinned because
		//     the render gate (bit 0x2000) is dark (same bit gates both)?
		//   - actor-array slot 0 (spawn ops write 0x802F4DB0 + idx*0x2B8: +0x6C
		//     active = 0x802F4E1C, +0x1F4 coord0 = 0x802F4FA4) and the partner
		//     +0x370 record low-24 (op02 coordinate ingest evidence, per the
		//     2026-06-10 map): does the ingested batch reach the actor/partner
		//     records at all (else wrong-slot ingest (a))?
		// NOTE on op-0x17 landing: its handler 0x800AC0E8 writes a record
		// resolved at runtime by 0x8010FE84(id) (stores +0xA8/AC/B0 position,
		// +0xE0 coord), NOT a fixed address - so we watch the FIXED-address
		// signals above (the +0x370 partner record low-24, which op02 rewrites,
		// and the fixed-base actor-spawn array) rather than a guessed op-0x17
		// offset.  The device-side ADOPT: ingest line (namco_c139, paired by t=)
		// reports WHERE each delivered frame lands (P014's "to word=0x01c6").
		// All reads READ-ONLY (m_mainram / device adopt_peek_ram, no writes).
		// Addresses verified from gold/full.txt (see 2026-06-16-p015 agent-log).
		// Sampled per-vblank; events logged on change + a 1/s status line.
		// Experiment branch only - never merge to milestone.
		if (m_trace_adopt)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4]; // record 0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4]; // record 1 +0x370
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			u32 const t7054        = m_mainram[0x002CE874 / 4]; // gp+0x7054 cutscene/round timer
			u32 const t7074        = m_mainram[0x002CE894 / 4]; // gp+0x7074 ==2 full-link gate
			bool const partner_is_rec1 = (local_idx == 0);
			u32 const partner_flags = partner_is_rec1 ? rec1_flags : rec0_flags;
			// actor-spawn array slot 0 (fixed base 0x802F4DB0, stride 0x2B8):
			u32 const a0_active    = m_mainram[0x002F4E1C / 4]; // slot 0 +0x6C active (hw in upper16)
			u32 const a0_coord0    = m_mainram[0x002F4FA4 / 4]; // slot 0 +0x1F4 coord0
			// link-chip RX RAM at P014's bulk landing word (rx_base 0x1000 +
			// fifo_ptr 0x01c6 = m_ram word 0x11c6); read-only peek, no drain:
			u16 const word01c6     = m_c139 ? m_c139->adopt_peek_ram(0x11c6) : 0;

			// render-gate composite helpers
			bool const rg0  = (rec0_flags & 0x2000) != 0;
			bool const rg1  = (rec1_flags & 0x2000) != 0;
			bool const rw0  = (rec0_flags & 0x00ffffff) == 0x0080d0; // 0x0080d0 render-window low-24
			bool const rw1  = (rec1_flags & 0x00ffffff) == 0x0080d0;

			if (m_adopt_have_prev)
			{
				// EVENT: bit 0x2000 (render gate) edge on either record - the
				// keystone; never observed set in 6 prior runs.  Any edge here
				// names the moment (and which record) the gate opens.
				if ((rec0_flags ^ m_adopt_prev_rec0) & 0x2000)
					logerror("ADOPT: rendergate rec0 bit2000 %u->%u frame=%u t=%.3fs rec0=%08x t7074=%u\n",
							(m_adopt_prev_rec0 >> 13) & 1, rg0 ? 1 : 0, frame_count,
							machine().time().as_double(), rec0_flags, t7074);
				if ((rec1_flags ^ m_adopt_prev_rec1) & 0x2000)
					logerror("ADOPT: rendergate rec1 bit2000 %u->%u frame=%u t=%.3fs rec1=%08x t7074=%u\n",
							(m_adopt_prev_rec1 >> 13) & 1, rg1 ? 1 : 0, frame_count,
							machine().time().as_double(), rec1_flags, t7074);

				// EVENT: 0x0080d0 render-window low-24 composite appears/leaves
				// on either record (absent on both cabs in P014).
				bool const prw0 = (m_adopt_prev_rec0 & 0x00ffffff) == 0x0080d0;
				bool const prw1 = (m_adopt_prev_rec1 & 0x00ffffff) == 0x0080d0;
				if (rw0 != prw0)
					logerror("ADOPT: renderwindow rec0 0080d0 %u->%u frame=%u t=%.3fs rec0=%08x\n",
							prw0 ? 1 : 0, rw0 ? 1 : 0, frame_count, machine().time().as_double(), rec0_flags);
				if (rw1 != prw1)
					logerror("ADOPT: renderwindow rec1 0080d0 %u->%u frame=%u t=%.3fs rec1=%08x\n",
							prw1 ? 1 : 0, rw1 ? 1 : 0, frame_count, machine().time().as_double(), rec1_flags);

				// EVENT: gp+0x7074 (==2 full-link gate) transition.
				if (t7074 != m_adopt_prev_t7074)
					logerror("ADOPT: cutscene gate7074 %u->%u frame=%u t=%.3fs rec0=%08x rec1=%08x partner=rec%u\n",
							m_adopt_prev_t7074, t7074, frame_count, machine().time().as_double(),
							rec0_flags, rec1_flags, partner_is_rec1 ? 1 : 0);

				// 1/s window aggregates: cutscene-timer advances, partner +0x370
				// low-24 changes (op02 ingest), actor-slot changes (spawn ingest),
				// link-RAM 0x01c6 changes (bulk landing).
				if (t7054 > m_adopt_prev_t7054)
					++m_adopt_t7054_advances;
				if (t7054 > m_adopt_max_t7054)
					m_adopt_max_t7054 = t7054;
				if ((partner_flags ^ (partner_is_rec1 ? m_adopt_prev_rec1 : m_adopt_prev_rec0)) & 0x00ffffff)
					++m_adopt_p_pos_changes;
				if (a0_active != m_adopt_prev_a0_active)
					++m_adopt_actor_changes;
				if (u32(word01c6) != m_adopt_prev_word01c6)
					++m_adopt_word01c6_changes;
			}

			// 1/s status line: render-gate + cutscene-timer + ingest-landing in
			// one grep-decomposable record (ADOPT: status ...).
			if (frame_count % 60 == 0)
			{
				logerror("ADOPT: status frame=%u t=%.3fs mode=%u ka=%u rg2000=%u/%u rw0080d0=%u/%u rec0=%08x rec1=%08x partner=rec%u t7054=%u t7054max=%u t7074=%u t7054adv=%u plow24chg=%u actorchg=%u a0act=%04x a0c0=%08x word01c6=%04x w01c6chg=%u\n",
						frame_count, machine().time().as_double(), staging_mode, keepalive,
						rg0 ? 1 : 0, rg1 ? 1 : 0, rw0 ? 1 : 0, rw1 ? 1 : 0,
						rec0_flags, rec1_flags, partner_is_rec1 ? 1 : 0,
						t7054, m_adopt_max_t7054, t7074, m_adopt_t7054_advances, m_adopt_p_pos_changes,
						m_adopt_actor_changes, u16(a0_active >> 16), a0_coord0,
						word01c6, m_adopt_word01c6_changes);
				m_adopt_t7054_advances = 0;
				m_adopt_p_pos_changes = 0;
				m_adopt_actor_changes = 0;
				m_adopt_word01c6_changes = 0;
				m_adopt_max_t7054 = 0;
			}

			m_adopt_prev_rec0 = rec0_flags;
			m_adopt_prev_rec1 = rec1_flags;
			m_adopt_prev_t7054 = t7054;
			m_adopt_prev_t7074 = t7074;
			m_adopt_prev_a0_active = a0_active;
			m_adopt_prev_word01c6 = u32(word01c6);
			m_adopt_have_prev = true;
		}

		// P016 PHASE A EXPERIMENT (branch patch/op70-cutscene-timer-arm):
		// READ-ONLY op-70 cutscene-timer accept/reject proof.  See the member
		// block for rationale.  The DEVICE side (namco_c139 OP70: rx) proves an
		// op-70 frame ARRIVED and what peer cutscene-timer value it carries; this
		// driver-side half is the DECISIVE accept/reject: it watches gp+0x7054
		// (the ROM store target at 0x80016E80) for a change (= ACCEPT) and, every
		// vblank, samples the two gate operands the ROM tests so a REJECT names
		// which predicate failed.  The op-70 gate reads the LOCAL record's +0x370
		// (0x80016E50: 0x802F43D0 + localidx*0x474), NOT the partner record - so
		// we select rec0/rec1 by localidx exactly as the ROM does.  Independent of
		// all patch gates; read-only.  Experiment branch only - never merge.
		if (m_trace_op70)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4]; // 0x802F3FD8
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4]; // record 0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4]; // record 1 +0x370
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			u32 const t7054        = m_mainram[0x002CE874 / 4]; // gp+0x7054 cutscene/round timer (op-70 store target)
			u32 const t7074        = m_mainram[0x002CE894 / 4]; // gp+0x7074 ==2 full-link gate
			// LOCAL record (op-70 gate 0x80016E50 reads 0x802F43D0 + localidx*0x474):
			bool const local_is_rec0 = (local_idx == 0);
			u32 const local_flags  = local_is_rec0 ? rec0_flags : rec1_flags;
			u32 const partner_flags = local_is_rec0 ? rec1_flags : rec0_flags;
			// gate operand 1 (0x80016E58 beqz): LOCAL +0x370 & 0x2000
			bool const gate_local2000 = (local_flags & 0x2000) != 0;
			// gate operand 2 (0x80016E64 bne 2): gp+0x7074 == 2 (needs partner +0x370 & 0x6000 == 0x6000)
			bool const gate_7074eq2 = (t7074 == 2);
			bool const partner_6000 = (partner_flags & 0x6000) == 0x6000;

			if (m_op70_have_prev)
			{
				// ACCEPT detector: gp+0x7054 changed = the ROM store at
				// 0x80016E80 fired (op-70 adopted the peer's cutscene timer).
				// On the linked rig P015 saw this NEVER happen (perma-0); any
				// line here is decisive ACCEPT evidence.
				if (t7054 != m_op70_prev_t7054)
				{
					++m_op70_t7054_changes;
					logerror("OP70: accept frame=%u t=%.6f gp+0x7054 %u->%u (op-70 store 0x80016E80 fired) gate_local2000=%u gate_7074eq2=%u localidx=%u local=%08x partner=%08x\n",
							frame_count, machine().time().as_double(),
							m_op70_prev_t7054, t7054,
							gate_local2000 ? 1 : 0, gate_7074eq2 ? 1 : 0,
							local_idx, local_flags, partner_flags);
				}
			}

			// 1/s window aggregates: how often each gate operand was satisfied.
			if (gate_local2000)
				++m_op70_gate_local2000_set;
			if (gate_7074eq2)
				++m_op70_gate_7074eq2;

			// 1/s status line: the two gate operands + the current timer, so the
			// analyst can read, beside the device-side OP70: rx arrival lines,
			// WHETHER each op-70 frame would be accepted or rejected and by which
			// predicate.  When gp+0x7054 stays 0 while OP70: rx lines appear, the
			// gate columns below name the false predicate (gate_local2000=0 =>
			// LOCAL +0x370 bit 0x2000 clear; gate_7074eq2=0 => gp+0x7074 != 2,
			// which itself needs partner6000=1).
			if (frame_count % 60 == 0)
			{
				logerror("OP70: status frame=%u t=%.6f mode=%u ka=%u t7054=%u t7074=%u gate_local2000=%u gate_7074eq2=%u partner6000=%u localidx=%u local=%08x partner=%08x t7054chg=%u local2000set=%u/60 7074eq2=%u/60\n",
						frame_count, machine().time().as_double(), staging_mode, keepalive,
						t7054, t7074, gate_local2000 ? 1 : 0, gate_7074eq2 ? 1 : 0,
						partner_6000 ? 1 : 0, local_idx, local_flags, partner_flags,
						m_op70_t7054_changes, m_op70_gate_local2000_set, m_op70_gate_7074eq2);
				m_op70_t7054_changes = 0;
				m_op70_gate_local2000_set = 0;
				m_op70_gate_7074eq2 = 0;
			}

			m_op70_prev_t7054 = t7054;
			m_op70_have_prev = true;
		}

		// P020 EXPERIMENT (branch patch/linked-gate-supply): FORCING supply of
		// the LOCAL "fully-linked" 0x6000 bits.  See the member-block comment for
		// full rationale + ROM provenance.  Each vblank, while linked gameplay is
		// staged (mode word 0x802F3FD0 == 2 - the same gate the KEEPALIVE_FLOOR /
		// CONTINUOUS_ARM blocks use), OR 0x6000 into the LOCAL record +0x370 word
		// (RMW, write back only when it changes).  The LOCAL record base is
		// resolved EXACTLY as the LINKBITS trace / KEEPALIVE_FLOOR blocks do:
		// localidx = gp+0x7608 (0x802CEE28); local = 0x802F43D0 + localidx*0x474
		// (rec0 0x802F43D0 if localidx==0, rec1 0x802F4844 if 1).  SUPPLY only -
		// no write to gp+0x7074, no partner-record write, no synthetic op-70: the
		// ROM's own op55 builder/TX carries the supplied +0x370 0x6000 to the peer
		// so the downstream gate7074->2 / op-70 chain comes alive naturally.
		// OR-only + env-gated + mode==2-bounded (see safety note in the member
		// block).  Ordered BEFORE the LINKBITS trace below so the supplied bit is
		// reflected in the same vblank's LINKBITS: status line.  Experiment branch
		// only - never merge to milestone.
		if (m_patch_linked_gate)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			// LOCAL record +0x370 = 0x802F43D0 + localidx*0x474 (rec0 if
			// localidx==0, rec1 0x802F4844 if 1) - same resolution as LINKBITS.
			offs_t const local_word = (local_idx == 0) ? (0x002F43D0 / 4) : (0x002F4844 / 4);
			u32 const pre = m_mainram[local_word];

			if (staging_mode == 2 && (pre & 0x6000) != 0x6000)
			{
				m_mainram[local_word] = pre | 0x6000;
				++m_lg_set_count;
				logerror("LINKED_GATE: set frame=%u t=%.6f localidx=%u local+0x370 pre=%08x post=%08x count=%u\n",
						frame_count, machine().time().as_double(), local_idx,
						pre, m_mainram[local_word], m_lg_set_count);
			}

			if (frame_count % 60 == 0)
				logerror("LINKED_GATE: status frame=%u t=%.6f mode=%u localidx=%u local+0x370=%08x set_total=%u\n",
						frame_count, machine().time().as_double(), staging_mode,
						local_idx, m_mainram[local_word], m_lg_set_count);
		}

		// P024 EXPERIMENT (branch patch/op55-carrier-repeat, off
		// patch/linked-gate-tx-only): PARTNER-RECORD STICKY LATCH.  See the
		// member-block comment for full rationale + the partner-0x2000 safety
		// pre-step.  Each vblank, while linked gameplay is staged (mode word
		// 0x802F3FD0 == 2), OR 0x6000 into the PARTNER record +0x370 word so the
		// gate (0x80016BB4-BCC) sees PERSISTENT 0x6000 (P021's wire-only advertise
		// landed it only ~1/scene and it lapsed; this re-supplies it every frame).
		// PARTNER record = 0x802F43D0 + (1-localidx)*0x474 (rec1 0x802F4844 if
		// localidx==0, rec0 0x802F43D0 if 1) - resolved EXACTLY as the LINKBITS
		// trace / KEEPALIVE_FLOOR / CONTINUOUS_ARM blocks resolve the partner.
		// INVARIANT: the LOCAL record (0x802F43D0 + localidx*0x474) is NEVER
		// written - only the gun-actor-irrelevant PARTNER record carries 0x2000, so
		// the local gun stays human (no P020/P023a death derail).  OR-only +
		// env-gated + mode-2-bounded + idempotent (writes back only when 0x6000 is
		// not already both set).  Ordered BEFORE the LINKBITS trace below so the
		// latched partner bit is reflected in the same vblank's LINKBITS: status
		// line (partner6000).  Experiment branch only - never merge to milestone.
		if (m_patch_op55_repeat)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			// PARTNER record +0x370 = 0x802F43D0 + (1-localidx)*0x474 (rec1
			// 0x802F4844 if localidx==0, rec0 0x802F43D0 if 1) - the SAME resolution
			// the gate uses (0x80016BB4 reads 0x802F43D0 + (1-gp+0x7608)*0x474).
			offs_t const partner_word = (local_idx == 0) ? (0x002F4844 / 4) : (0x002F43D0 / 4);
			u32 const pre = m_mainram[partner_word];

			if (staging_mode == 2 && (pre & 0x6000) != 0x6000)
			{
				m_mainram[partner_word] = pre | 0x6000;
				++m_or_partner6000_count;
				logerror("OP55_REPEAT: latch frame=%u t=%.6f localidx=%u partner=rec%u +0x370 pre=%08x post=%08x count=%u\n",
						frame_count, machine().time().as_double(), local_idx,
						(local_idx == 0) ? 1u : 0u, pre, m_mainram[partner_word],
						m_or_partner6000_count);
			}

			if (frame_count % 60 == 0)
				logerror("OP55_REPEAT: status frame=%u t=%.6f mode=%u localidx=%u partner=rec%u partner+0x370=%08x partner6000=%u latch_total=%u\n",
						frame_count, machine().time().as_double(), staging_mode,
						local_idx, (local_idx == 0) ? 1u : 0u, m_mainram[partner_word],
						((m_mainram[partner_word] & 0x6000) == 0x6000) ? 1u : 0u,
						m_or_partner6000_count);
		}

		// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): READ-ONLY
		// "linkbits" trace.  See the member-block comment for full rationale +
		// ROM provenance.  This driver-side half samples the LOCAL and PARTNER
		// +0x370 0x6000 bits each vblank, logs transitions, and pairs them with
		// gp+0x7074 and the gate's would-be value so the chain
		// local-set -> TX -> ingest -> gate is visible in one LINKBITS: status
		// line.  Decision: local6000=0 all run => (X) local never declares
		// fully-linked (root is the local 0x4000/0x2000 set gate); local6000=1 +
		// (device LINKBITS: txflags 6000 absent) => (Y) TX framing drops it;
		// local6000=1 + tx 6000 present + (device LINKBITS: rxflags 6000 present)
		// but partner6000 still 0 => (Z) the peer's op55 ingest didn't land it.
		// All reads READ-ONLY (m_mainram).  Experiment branch only - never merge.
		if (m_trace_linkbits)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4]; // 0x802F3FD0
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4]; // record 0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4]; // record 1 +0x370
			u32 const local_idx    = m_mainram[0x002CEE28 / 4]; // gp+0x7608 local record index
			u32 const t7074        = m_mainram[0x002CE894 / 4]; // gp+0x7074 ==2 full-link gate
			u32 const t705c        = m_mainram[0x002CE87C / 4]; // gp+0x705C (local 0x4000 set gate predicate 0x80016DA0)
			// localidx selects the cab's OWN record (rec0 if localidx==0) and the
			// PARTNER is the other (exactly as the ROM: local = 0x802F43D0 +
			// localidx*0x474; partner = 0x802F43D0 + (1-localidx)*0x474).
			bool const local_is_rec0 = (local_idx == 0);
			u32 const local_flags   = local_is_rec0 ? rec0_flags : rec1_flags;
			u32 const partner_flags = local_is_rec0 ? rec1_flags : rec0_flags;
			bool const local_6000_both   = (local_flags   & 0x6000) == 0x6000;
			bool const partner_6000_both = (partner_flags & 0x6000) == 0x6000;
			// would-be gate value the ROM derives at 0x80016BC0-C8 from the PARTNER
			// 0x6000 (this is what gp+0x7074 SHOULD become): 2 iff partner 0x6000
			// both set, else 1.
			unsigned const gate_wouldbe = partner_6000_both ? 2u : 1u;

			if (m_lb_have_prev)
			{
				// EVENT: LOCAL +0x370 0x6000 bit edges (the (X) decider - has the
				// local cab EVER declared itself fully-linked?).  Log each of the
				// two bits separately so the 0x4000 (0x80016DBC) vs 0x2000
				// (0x800166B0) write sites can be told apart.
				if ((local_flags ^ m_lb_prev_local) & 0x2000)
					logerror("LINKBITS: local bit2000 %u->%u frame=%u t=%.6f local=%08x t705c=%u (set site 0x800166B0)\n",
							(m_lb_prev_local >> 13) & 1, (local_flags >> 13) & 1,
							frame_count, machine().time().as_double(), local_flags, t705c);
				if ((local_flags ^ m_lb_prev_local) & 0x4000)
					logerror("LINKBITS: local bit4000 %u->%u frame=%u t=%.6f local=%08x t705c=%u (set site 0x80016DBC gate gp+0x705C==0)\n",
							(m_lb_prev_local >> 14) & 1, (local_flags >> 14) & 1,
							frame_count, machine().time().as_double(), local_flags, t705c);

				// EVENT: PARTNER +0x370 0x6000 bit edges (the ingest decider - did
				// op55 RX land 0x6000 into rec1?).
				if ((partner_flags ^ m_lb_prev_partner) & 0x2000)
					logerror("LINKBITS: partner bit2000 %u->%u frame=%u t=%.6f partner=%08x (op55 RX -> 0x80013E10)\n",
							(m_lb_prev_partner >> 13) & 1, (partner_flags >> 13) & 1,
							frame_count, machine().time().as_double(), partner_flags);
				if ((partner_flags ^ m_lb_prev_partner) & 0x4000)
					logerror("LINKBITS: partner bit4000 %u->%u frame=%u t=%.6f partner=%08x (op55 RX -> 0x80013E10)\n",
							(m_lb_prev_partner >> 14) & 1, (partner_flags >> 14) & 1,
							frame_count, machine().time().as_double(), partner_flags);

				// EVENT: gate gp+0x7074 transition + its driving predicate gp+0x705C
				// (the local 0x4000 set gate at 0x80016DA0).
				if (t7074 != m_lb_prev_t7074)
					logerror("LINKBITS: gate7074 %u->%u frame=%u t=%.6f local=%08x partner=%08x wouldbe=%u\n",
							m_lb_prev_t7074, t7074, frame_count, machine().time().as_double(),
							local_flags, partner_flags, gate_wouldbe);
				if (t705c != m_lb_prev_t705c)
					logerror("LINKBITS: t705c %u->%u frame=%u t=%.6f (gates local bit4000 set at 0x80016DB8: ==0 -> set, !=0 -> clear)\n",
							m_lb_prev_t705c, t705c, frame_count, machine().time().as_double());
			}

			// 1/s window aggregates.
			if (local_6000_both)
				++m_lb_local6000_set;
			if (partner_6000_both)
				++m_lb_partner6000_set;

			// 1/s LINKBITS: status - the whole chain in one grep-decomposable line:
			// local0x6000 (X), partner0x6000 (ingest result), gp+0x7074 (gate), and
			// the would-be gate value.  Pair with the device-side LINKBITS: txflags
			// / rxflags (op55 24-bit wire flags) by t= to tell (Y) from (Z).
			if (frame_count % 60 == 0)
			{
				logerror("LINKBITS: status frame=%u t=%.6f mode=%u localidx=%u local=%08x partner=%08x local6000=%u partner6000=%u t7074=%u t705c=%u wouldbe=%u local6000set=%u/60 partner6000set=%u/60\n",
						frame_count, machine().time().as_double(), staging_mode, local_idx,
						local_flags, partner_flags,
						local_6000_both ? 1 : 0, partner_6000_both ? 1 : 0,
						t7074, t705c, gate_wouldbe,
						m_lb_local6000_set, m_lb_partner6000_set);
				m_lb_local6000_set = 0;
				m_lb_partner6000_set = 0;
			}

			m_lb_prev_local   = local_flags;
			m_lb_prev_partner = partner_flags;
			m_lb_prev_t7074   = t7074;
			m_lb_prev_t705c   = t705c;
			m_lb_have_prev    = true;
		}

		// P025 EXPERIMENT (branch patch/playclock-humangate-trace): READ-ONLY
		// per-vblank trace of (1) the op6F play-clock pair rec+0x390/+0x394 and
		// its emit/adopt gate predicates and (2) the bit-0x0004 co-op/human gate
		// on BOTH records with full attribution context on every set AND clear.
		// See the member-block comment for the full ROM provenance.  Volume:
		// edges + 1/s status only (normal +1/frame clock ticks are NOT logged
		// per-vblank - only JUMPS, i.e. adoption/re-init, and tick-run
		// start/stop edges).  All reads (m_mainram + device counters); no
		// writes.  Experiment branch only - never merge to milestone.
		if (m_trace_playclock)
		{
			u32 const staging_mode = m_mainram[0x002F3FD0 / 4];            // 0x802F3FD0 staging mode (2 = linked gameplay)
			u32 const keepalive    = m_mainram[0x002F3FD8 / 4];            // 0x802F3FD8 keepalive (op6F TX gate (b): must be > 0)
			u32 const local_idx    = m_mainram[0x002CEE28 / 4];            // gp+0x7608 local record index (0 on this ROM)
			u32 const rec0_flags   = m_mainram[0x002F43D0 / 4];            // rec0 +0x370
			u32 const rec1_flags   = m_mainram[0x002F4844 / 4];            // rec1 +0x370
			u32 const rec0_390     = m_mainram[0x002F43F0 / 4];            // rec0 +0x390 play-clock (ticks +1/frame in 0x80014CC0)
			u32 const rec0_394     = m_mainram[0x002F43F4 / 4];            // rec0 +0x394 segment clock
			u32 const rec1_390     = m_mainram[0x002F4864 / 4];            // rec1 +0x390 (expected static -1: tick is LOCAL-record only)
			u32 const rec1_394     = m_mainram[0x002F4868 / 4];            // rec1 +0x394
			u8  const role3ffc     = u8(m_mainram[0x002F3FFC / 4] >> 24);  // byte 0x802F3FFC (bit7: 1 = op6F MASTER/emit, 0 = SLAVE/adopt)
			u8  const gate3502     = u8(m_mainram[0x002F3500 / 4] >> 8);   // byte 0x802F3502 peer TX gate (must be 4 for gate-4 dispatch incl. op6F RX)
			u8  const framephase   = u8(m_mainram[0x002CED10 / 4]);        // gp+0x74F0 low byte = gp+0x74F3 (op6F TX gate (a): emits when == 0)
			u16 const pause7570    = u16(m_mainram[0x002CED90 / 4] >> 16); // gp+0x7570 (clock-tick hold: != 0 -> no tick, 0x80014CCC)

			bool const rec0_04 = (rec0_flags & 0x0004) != 0;
			bool const rec1_04 = (rec1_flags & 0x0004) != 0;

			if (m_pc_have_prev)
			{
				// EVENT: bit 0x0004 edge on EITHER record - one line carrying BOTH
				// records' pre/post words so the clear can be ATTRIBUTED by
				// fingerprint: paired= both records flipped this same vblank (the
				// script handler 0x800BE200 is the ONLY paired site); a whole-word
				// discontinuity = record re-init 0x80013D44 (round start / op55 RX);
				// a single-record low24-only change = op02 mirror (correlate with
				// the peer log by t=) or the reset helper 0x80013C54.
				bool const p0_04 = (m_pc_prev_rec0_flags & 0x0004) != 0;
				bool const p1_04 = (m_pc_prev_rec1_flags & 0x0004) != 0;
				if (p0_04 != rec0_04 || p1_04 != rec1_04)
					logerror("PLAYCLOCK: bit04 edge frame=%u t=%.6f rec0 %u->%u (%08x->%08x) rec1 %u->%u (%08x->%08x) paired=%u mode=%u ka=%u role3ffc=%02x (paired-flip=script 0x800BE200; whole-word=re-init 0x80013D44; low24-only=op02 mirror 0x800AB438/peer or 0x80013C54)\n",
							frame_count, machine().time().as_double(),
							p0_04 ? 1 : 0, rec0_04 ? 1 : 0, m_pc_prev_rec0_flags, rec0_flags,
							p1_04 ? 1 : 0, rec1_04 ? 1 : 0, m_pc_prev_rec1_flags, rec1_flags,
							((p0_04 != rec0_04) && (p1_04 != rec1_04)) ? 1 : 0,
							staging_mode, keepalive, role3ffc);

				// EVENT: op6F role byte change (bit7 = master/emit vs slave/adopt;
				// SET by screen-mode inits 0x8001E16C/39C/6C0 + staging a0==0/1,
				// CLEARED only by staging request a0==2 @0x800B2FD4).  The op6F
				// channel works ONLY when the two cabs' bit7 DIFFER.
				if (role3ffc != m_pc_prev_role3ffc)
					logerror("PLAYCLOCK: role3ffc %02x->%02x frame=%u t=%.6f op6fmaster %u->%u mode=%u (bit7 set=0x8001E16C/39C/6C0|0x800B2F9C a0<2; clear=0x800B2FD4 a0==2)\n",
							m_pc_prev_role3ffc, role3ffc, frame_count, machine().time().as_double(),
							(m_pc_prev_role3ffc >> 7) & 1, (role3ffc >> 7) & 1, staging_mode);

				// EVENT: rec0 play-clock JUMP (delta outside 0..2) = op6F slave
				// ADOPTION (0x800B2504-08) or re-init to -1 (0x80013E30-34).
				// Normal +1/frame ticks are never logged.
				s32 const d390 = s32(rec0_390 - m_pc_prev_rec0_390);
				s32 const d394 = s32(rec0_394 - m_pc_prev_rec0_394);
				if (d390 < 0 || d390 > 2)
					logerror("PLAYCLOCK: clock-jump rec0_390 %08x->%08x delta=%d frame=%u t=%.6f (adopt @0x800B2504 needs role3ffc bit7=0; reinit=-1 @0x80013E30) role3ffc=%02x\n",
							m_pc_prev_rec0_390, rec0_390, d390, frame_count, machine().time().as_double(), role3ffc);
				if (d394 < 0 || d394 > 2)
					logerror("PLAYCLOCK: clock-jump rec0_394 %08x->%08x delta=%d frame=%u t=%.6f (adopt @0x800B2508; segment-clock zero @0x80104980) role3ffc=%02x\n",
							m_pc_prev_rec0_394, rec0_394, d394, frame_count, machine().time().as_double(), role3ffc);

				// EVENT: rec1 clocks changing AT ALL (the tick 0x80014CC0 is
				// LOCAL-record only and op6F adopts into the OWN record, so any
				// rec1 movement would refute the static model - log every change).
				if (rec1_390 != m_pc_prev_rec1_390 || rec1_394 != m_pc_prev_rec1_394)
					logerror("PLAYCLOCK: rec1-clock change 390 %08x->%08x 394 %08x->%08x frame=%u t=%.6f (unexpected: tick+adopt are LOCAL-record only)\n",
							m_pc_prev_rec1_390, rec1_390, m_pc_prev_rec1_394, rec1_394,
							frame_count, machine().time().as_double());

				// EVENT: rec0 tick-run start/stop (ticking = +1/+2 per vblank).
				// A stop names the failing tick gate via the sampled predicates:
				// pause7570 != 0, or rec0 bit29 set, or the +0x358 obj condition.
				bool const ticking = (d390 >= 1 && d390 <= 2);
				if (ticking != m_pc_prev_rec0_ticking)
					logerror("PLAYCLOCK: tick-%s rec0_390=%08x frame=%u t=%.6f pause7570=%04x rec0bit29=%u mode=%u (tick 0x80014D3C gates: gp+0x7570==0 @0x80014CCC, +0x370 bit29 clear @0x80014D04, obj[+0x358]+9!=0 && +0x54==-1)\n",
							ticking ? "start" : "stop", rec0_390, frame_count, machine().time().as_double(),
							pause7570, (rec0_flags >> 29) & 1, staging_mode);
				m_pc_prev_rec0_ticking = ticking;

				// 1/s window aggregates.
				if (ticking)
					++m_pc_ticks390;
				if (rec0_04 && rec1_04)
					++m_pc_bit04_both;
			}

			// 1/s PLAYCLOCK: status - both play-clocks, both bit04s, and every
			// gate predicate the static map identified, in one grep-decomposable
			// line.  op6f_tx/op6f_rx are the device cell-walk running counts
			// (PLAYCLOCK: op6f-tx / op6f-rx lines), with the per-second delta.
			if (frame_count % 60 == 0)
			{
				u32 const op6f_tx = m_c139 ? m_c139->op6f_tx_count() : 0;
				u32 const op6f_rx = m_c139 ? m_c139->op6f_rx_count() : 0;
				logerror("PLAYCLOCK: status frame=%u t=%.6f mode=%u localidx=%u rec0_390=%08x rec0_394=%08x rec1_390=%08x rec1_394=%08x ticks390=%u/60 rec0=%08x rec1=%08x bit04_rec0=%u bit04_rec1=%u bit04both=%u/60 role3ffc=%02x op6fmaster=%u gate3502=%02x ka=%u pause7570=%04x framephase=%02x op6f_tx=%u(+%u) op6f_rx=%u(+%u)\n",
						frame_count, machine().time().as_double(), staging_mode, local_idx,
						rec0_390, rec0_394, rec1_390, rec1_394, m_pc_ticks390,
						rec0_flags, rec1_flags, rec0_04 ? 1 : 0, rec1_04 ? 1 : 0, m_pc_bit04_both,
						role3ffc, (role3ffc >> 7) & 1, gate3502, keepalive, pause7570, framephase,
						op6f_tx, op6f_tx - m_pc_prev_op6f_tx, op6f_rx, op6f_rx - m_pc_prev_op6f_rx);
				m_pc_ticks390 = 0;
				m_pc_bit04_both = 0;
				m_pc_prev_op6f_tx = op6f_tx;
				m_pc_prev_op6f_rx = op6f_rx;
			}

			m_pc_prev_rec0_390   = rec0_390;
			m_pc_prev_rec0_394   = rec0_394;
			m_pc_prev_rec1_390   = rec1_390;
			m_pc_prev_rec1_394   = rec1_394;
			m_pc_prev_rec0_flags = rec0_flags;
			m_pc_prev_rec1_flags = rec1_flags;
			m_pc_prev_role3ffc   = role3ffc;
			m_pc_have_prev       = true;
		}

		// P033: READ-ONLY bulk-compose scheduler gate trace (see the member-block
		// comment for the full static map; every address below is anchored there).
		// All reads m_mainram only - no write to RAM/ROM/game state anywhere.
		if (m_trace_composegate)
		{
			// Session gates
			u32 const mode      = m_mainram[0x002F3FD0 / 4];               // 0x802F3FD0 mode (2 = linked gameplay; phase-0 abort @0x80014EA8 if != 2)
			u32 const ka        = m_mainram[0x002F3FD8 / 4];               // 0x802F3FD8 keepalive (phase-0 abort @0x80014EB4 if 0; zeroed by EVERY teardown)
			u8  const role      = u8(m_mainram[0x002F3FFC / 4] >> 24);     // role byte (bit0 transfer-active, bit5 phase-0 hold, bit7 op6F master)
			// Boundary / segment-phase words
			u32 const rec0_370  = m_mainram[0x002F43D0 / 4];               // rec0 +0x370 (low bits: 0x40 marker / 0x20 engaged)
			u32 const rec1_370  = m_mainram[0x002F4844 / 4];               // rec1 +0x370
			u32 const rec0_390  = m_mainram[0x002F43F0 / 4];               // rec0 +0x390 play-clock (op6F pair)
			u32 const rec0_394  = m_mainram[0x002F43F4 / 4];               // rec0 +0x394 segment clock (op6F pair)
			// Transfer-screen / phase-machine words
			u32 const screen    = m_mainram[0x002CED8C / 4];               // gp+0x756C screen state (0x13/0x14 always-transfer, 0x15/0x16 conditional, 6 in-game) [P034 rider: was 0x002CCD8C, a C<->E digit swap - frozen garbage all P033 run]
			u32 const phase     = m_mainram[0x002CE844 / 4];               // gp+0x7024 phase index (0 handshake, 1 = bulk-compose scheduler)
			u32 const wait7028  = m_mainram[0x002CE848 / 4];               // gp+0x7028 phase-0 hold countdown (from 0xF0)
			u32 const trans702c = m_mainram[0x002CE84C / 4];               // gp+0x702C transfer-screen countdown (from 0xF0)
			u32 const serve7010 = m_mainram[0x002CE830 / 4];               // gp+0x7010 serve flag (0 = re-emit last snapshot @0x80014F94)
			u32 const done      = m_mainram[0x002CE834 / 4];               // gp+0x7014 done/abort latch (1 = phase 1 dead @0x80014F4C)
			u32 const wdog      = m_mainram[0x002CE838 / 4];               // gp+0x7018 phase-1 watchdog (300 frames)
			u32 const idle      = m_mainram[0x002CE83C / 4];               // gp+0x701C request-idle streak (>= 0x20 -> teardown)
			s16 const reqid     = s16(m_mainram[0x002CE840 / 4] >> 16);    // gp+0x7020 snapshot request id (-1 = none; latched @0x800151D0 from peer op-0x1A)
			u16 const entered   = u16(m_mainram[0x002CE854 / 4] >> 16);    // gp+0x7034 join-entered latch
			u16 const bypass    = u16(m_mainram[0x002CE854 / 4]);          // gp+0x7036 transfer bypass selector (!=0 -> screen 0x16 skips the phase machine)
			u32 const round703c = m_mainram[0x002CE85C / 4];               // gp+0x703C round state (table 0x8022A78C)
			u32 const sub7040   = m_mainram[0x002CE860 / 4];               // gp+0x7040 round sub-state
			u32 const timer7054 = m_mainram[0x002CE874 / 4];               // gp+0x7054 round/link timer (op-0x70 clamped)
			u32 const code7074  = m_mainram[0x002CE894 / 4];               // gp+0x7074 link status code (2 = fully linked, op55-fed)
			// Compose output + TX pump state
			u16 const len3910   = u16(m_mainram[0x002F3910 / 4] >> 16);    // 0x802F3910 composed VM frame length (finalize 0x800AA76C; > 0xFF = bulk class)
			u8  const gate3d12  = u8(m_mainram[0x002F3D10 / 4] >> 8);      // 0x802F3D12 finalize gate byte (4 = gameplay table when mode==2, 3 otherwise)
			u16 const txrem     = u16(m_mainram[0x002CEDE0 / 4] >> 16);    // gp+0x75C0 TX pump remaining-size (chunking in flight)
			u16 const txkick    = u16(m_mainram[0x002CEDE4 / 4]);          // gp+0x75C6 TX pump kick/budget counter (PATH C announce while > 0)
			u16 const servedid  = u16(m_mainram[0x002DFB40 / 4]);          // 0x802DFB42 outstanding served/echo id (-1 = none)
			u8  const gate3502  = u8(m_mainram[0x002F3500 / 4] >> 8);      // 0x802F3502 peer TX gate (existing anchor)
			u16 const drift3504 = u16(m_mainram[0x002F3504 / 4] >> 16);    // 0x802F3504 drift (existing anchor)

			bool const serving_capable = (screen == 0x14 || (screen == 0x16 && bypass == 0)) && phase == 1 && done == 0;

			if (m_cg_have_prev)
			{
				// EVENT: area-word 0x40 (marker) / 0x20 (engaged) transition on
				// EITHER record = the boundary marker.  One line dumping EVERY
				// identified gate word so a composed boundary and a skipped one
				// can be diffed field-by-field from single lines.
				if (((rec0_370 ^ m_cg_prev_rec0_370) & 0x60) || ((rec1_370 ^ m_cg_prev_rec1_370) & 0x60))
					logerror("COMPOSEGATE: boundary frame=%u t=%.6f rec0 %08x->%08x rec1 %08x->%08x mode=%u ka=%u role=%02x screen=%02x phase=%u done=%u wdog=%d idle=%u reqid=%d serve7010=%u bypass=%04x entered=%04x wait7028=%d trans702c=%d round703c=%u sub7040=%u code7074=%u timer7054=%08x clk390=%08x clk394=%08x len3910=%u gate3d12=%02x txkick=%u txrem=%u servedid=%04x gate3502=%02x drift=%04x\n",
							frame_count, machine().time().as_double(),
							m_cg_prev_rec0_370, rec0_370, m_cg_prev_rec1_370, rec1_370,
							mode, ka, role, screen, phase, done, s32(wdog), idle, reqid, serve7010,
							bypass, entered, s32(wait7028), s32(trans702c), round703c, sub7040, code7074,
							timer7054, rec0_390, rec0_394, len3910, gate3d12, txkick, txrem, servedid,
							gate3502, drift3504);

				// EVENT: transfer-screen / phase-machine transitions (screen 0x13/
				// 0x14/0x15/0x16 entry+exit, phase 0->1, done/abort latch edges).
				// done 0->1 while screen 0x14/0x16 = the phase-0 abort @0x80014EC0
				// or the phase-1 teardown @0x8001511C (wdog/idle tell which).
				if (screen != m_cg_prev_screen || phase != m_cg_prev_phase || done != m_cg_prev_done)
					logerror("COMPOSEGATE: phase frame=%u t=%.6f screen %02x->%02x phase %u->%u done %u->%u mode=%u ka=%u role=%02x wdog=%d idle=%u reqid=%d bypass=%04x wait7028=%d trans702c=%d serving_capable=%u\n",
							frame_count, machine().time().as_double(),
							m_cg_prev_screen, screen, m_cg_prev_phase, phase, m_cg_prev_done, done,
							mode, ka, role, s32(wdog), idle, reqid, bypass,
							s32(wait7028), s32(trans702c), serving_capable ? 1 : 0);

				// EVENT: snapshot request id change (peer op-0x1A latched a new id,
				// or phase-1 entry cleared it back to -1 with no request pending).
				if (reqid != m_cg_prev_reqid)
					logerror("COMPOSEGATE: request frame=%u t=%.6f reqid %d->%d idle=%u serve7010=%u servedid=%04x screen=%02x phase=%u done=%u len3910=%u\n",
							frame_count, machine().time().as_double(),
							m_cg_prev_reqid, reqid, idle, serve7010, servedid, screen, phase, done, len3910);

				// EVENT: a bulk-class VM frame was composed this frame (the direct
				// upstream analog of the device's >255hw TXOFFSET bulk announce).
				// Logged on value change only so a stalled length is not re-logged.
				if (len3910 > 0xFF && len3910 != m_cg_prev_len)
					logerror("COMPOSEGATE: bulkframe frame=%u t=%.6f len3910=%u gate3d12=%02x screen=%02x phase=%u done=%u reqid=%d idle=%u mode=%u ka=%u rec0=%08x rec1=%08x\n",
							frame_count, machine().time().as_double(),
							len3910, gate3d12, screen, phase, done, reqid, idle, mode, ka, rec0_370, rec1_370);

				// EVENT: session gate movement (mode change, keepalive zero-edge
				// either way, role-byte change).  ka 0-edges bracket every transfer
				// teardown (@0x80014EC8/0x80015128) and each op55 re-establishment.
				if (mode != m_cg_prev_mode || (ka == 0) != (m_cg_prev_ka == 0) || role != m_cg_prev_role || bypass != m_cg_prev_bypass)
					logerror("COMPOSEGATE: session frame=%u t=%.6f mode %u->%u ka %u->%u role %02x->%02x bypass %04x->%04x screen=%02x phase=%u done=%u\n",
							frame_count, machine().time().as_double(),
							m_cg_prev_mode, mode, m_cg_prev_ka, ka, m_cg_prev_role, role,
							m_cg_prev_bypass, bypass, screen, phase, done);

				// 1/s window aggregates.
				if (len3910 > 0xFF)
					++m_cg_win_bulkframes;
				if (reqid != -1)
					++m_cg_win_reqframes;
				if (serving_capable)
					++m_cg_win_phase1;
				if (len3910 > m_cg_win_maxlen)
					m_cg_win_maxlen = len3910;
			}

			// 1/s COMPOSEGATE: status - every gate word + the window counters, one
			// grep-decomposable line.
			if (frame_count % 60 == 0)
			{
				logerror("COMPOSEGATE: status frame=%u t=%.6f mode=%u ka=%u role=%02x screen=%02x phase=%u done=%u wdog=%d idle=%u reqid=%d serve7010=%u bypass=%04x entered=%04x round703c=%u sub7040=%u code7074=%u timer7054=%08x rec0=%08x rec1=%08x clk390=%08x clk394=%08x len3910=%u gate3d12=%02x txkick=%u txrem=%u servedid=%04x gate3502=%02x drift=%04x bulkframes=%u/60 reqframes=%u/60 phase1=%u/60 maxlen=%u\n",
						frame_count, machine().time().as_double(),
						mode, ka, role, screen, phase, done, s32(wdog), idle, reqid, serve7010,
						bypass, entered, round703c, sub7040, code7074, timer7054,
						rec0_370, rec1_370, rec0_390, rec0_394, len3910, gate3d12,
						txkick, txrem, servedid, gate3502, drift3504,
						m_cg_win_bulkframes, m_cg_win_reqframes, m_cg_win_phase1, m_cg_win_maxlen);
				m_cg_win_bulkframes = 0;
				m_cg_win_reqframes = 0;
				m_cg_win_phase1 = 0;
				m_cg_win_maxlen = 0;
			}

			m_cg_prev_rec0_370 = rec0_370;
			m_cg_prev_rec1_370 = rec1_370;
			m_cg_prev_screen   = screen;
			m_cg_prev_phase    = phase;
			m_cg_prev_done     = done;
			m_cg_prev_reqid    = reqid;
			m_cg_prev_len      = len3910;
			m_cg_prev_mode     = mode;
			m_cg_prev_ka       = ka;
			m_cg_prev_role     = role;
			m_cg_prev_bypass   = bypass;
			m_cg_have_prev     = true;
		}

		// P034: READ-ONLY round-end trigger / join-precondition trace (see the
		// member-block comment for the full static map; every address below is
		// anchored there).  All reads m_mainram only - no write anywhere.
		if (m_trace_roundend)
		{
			// Session / role words
			u32 const mode      = m_mainram[0x002F3FD0 / 4];               // 0x802F3FD0 mode (2 = linked)
			u32 const ka        = m_mainram[0x002F3FD8 / 4];               // 0x802F3FD8 keepalive
			u8  const role      = u8(m_mainram[0x002F3FFC / 4] >> 24);     // role byte (bit1 = join-armer gate, bit7 = op6F master)
			u32 const screen    = m_mainram[0x002CED8C / 4];               // gp+0x756C screen state (correct address)
			u32 const idx7608   = m_mainram[0x002CEE28 / 4];               // gp+0x7608 local player index (record selector)
			// Per-record round-end trigger words (rec0 = 0x802F4060, rec1 = +0x474)
			u32 const p370[2]   = { m_mainram[0x002F43D0 / 4], m_mainram[0x002F4844 / 4] };  // +0x370 flags (0x2000 round-end mode, 0x6000 advertise, 0x40010800 checker gate A, 0x04 co-op)
			u32 const f5c[2]    = { m_mainram[0x002F40BC / 4], m_mainram[0x002F4530 / 4] };  // +0x5C flags (bit0 = checker gate B)
			u16 const os37a[2]  = { u16(m_mainram[0x002F43D8 / 4]), u16(m_mainram[0x002F484C / 4]) };            // +0x37A one-shots (bit0 checker C, bit1 round-consumed)
			s16 const c3a0[2]   = { s16(m_mainram[0x002F4400 / 4] >> 16), s16(m_mainram[0x002F4874 / 4] >> 16) };// +0x3A0 checker gate-E countdown
			s16 const wd3a2[2]  = { s16(m_mainram[0x002F4400 / 4]), s16(m_mainram[0x002F4874 / 4]) };            // +0x3A2 area time-limit watchdog
			u32 const clk394[2] = { m_mainram[0x002F43F4 / 4], m_mainram[0x002F4868 / 4] };  // +0x394 segment clock (re-base target)
			u32 const clk388[2] = { m_mainram[0x002F43E8 / 4], m_mainram[0x002F485C / 4] };  // +0x388 (re-base fingerprint partner cell)
			// Round machine globals
			u32 const r703c     = m_mainram[0x002CE85C / 4];               // gp+0x703C round state
			u32 const r7040     = m_mainram[0x002CE860 / 4];               // gp+0x7040 round sub-state
			u32 const g705c     = m_mainram[0x002CE87C / 4];               // gp+0x705C 0x4000-advertise gate (0 -> tail sets 0x4000)
			u32 const t7054     = m_mainram[0x002CE874 / 4];               // gp+0x7054 round timer
			u32 const h7064     = m_mainram[0x002CE884 / 4];               // gp+0x7064 state-4/5 hold countdown
			u32 const c7068     = m_mainram[0x002CE888 / 4];               // gp+0x7068 state-1 frame counter (0x47 clears 705C)
			u32 const c7074     = m_mainram[0x002CE894 / 4];               // gp+0x7074 link status code
			// Join-armer precondition words
			u16 const ent7034   = u16(m_mainram[0x002CE854 / 4] >> 16);    // gp+0x7034 entered latch
			u16 const byp7036   = u16(m_mainram[0x002CE854 / 4]);          // gp+0x7036 bypass selector
			u32 const arm7624   = m_mainram[0x002CEE44 / 4];               // gp+0x7624 join arm-enable
			u32 const g75f8     = m_mainram[0x002CEE18 / 4];               // gp+0x75F8 global flags (bit2 blocks the watchdog path)
			u8  const cfg75e4   = u8(m_mainram[0x002CEE04 / 4] >> 24);     // gp+0x75E4 config byte (+0x3A0 init value)
			u16 const g6ff4     = u16(m_mainram[0x002CE814 / 4] >> 16);    // gp+0x6FF4 join-event countdown (0x80013EF8 sets 2)
			u8  const coop      = ((p370[0] | p370[1]) & 0x04) ? 1 : 0;    // join-armer co-op gate term
			u8  const ctr74f0lo = u8(m_mainram[0x002CED10 / 4]);           // gp+0x74F3 = frame counter low byte (op6F cadence gate)

			if (m_re_have_prev)
			{
				// EVENT: round machine walked (state/sub-state/advertise-gate/
				// link-code edges).  round703c 0->1 = state-0 handler ran =
				// LOCAL record entered round-end mode last frame.
				if (r703c != m_re_prev_703c || r7040 != m_re_prev_7040 || g705c != m_re_prev_705c || c7074 != m_re_prev_7074)
					logerror("ROUNDEND: round frame=%u t=%.6f state703c %u->%u sub7040 %u->%u gate705c %u->%u code7074 %u->%u cnt7068=%u timer7054=%08x hold7064=%d rec0_370=%08x rec1_370=%08x mode=%u ka=%u\n",
							frame_count, machine().time().as_double(),
							m_re_prev_703c, r703c, m_re_prev_7040, r7040, m_re_prev_705c, g705c,
							m_re_prev_7074, c7074, c7068, t7054, s32(h7064), p370[0], p370[1], mode, ka);

				for (int r = 0; r < 2; r++)
				{
					// EVENT: +0x370 trigger-relevant bit movement (round-end
					// mode 0x2000, advertise 0x6000, checker gate-A bits
					// 0x40010800, co-op 0x04, 2P 0x02, mirror marks
					// 0xC0000000).  Marker bits 0x60 stay COMPOSEGATE's job.
					if ((p370[r] ^ m_re_prev_p370[r]) & 0xC0016806)
						logerror("ROUNDEND: mode2000 frame=%u t=%.6f rec=%d p370 %08x->%08x adv6000 %u->%u mode2000 %u->%u gateA %u->%u coop04=%u round703c=%u\n",
								frame_count, machine().time().as_double(), r,
								m_re_prev_p370[r], p370[r],
								(m_re_prev_p370[r] & 0x6000) == 0x6000 ? 1 : 0, (p370[r] & 0x6000) == 0x6000 ? 1 : 0,
								(m_re_prev_p370[r] >> 13) & 1, (p370[r] >> 13) & 1,
								(m_re_prev_p370[r] & 0x40010800) ? 1 : 0, (p370[r] & 0x40010800) ? 1 : 0,
								coop, r703c);

					// EVENT: checker activity fingerprint - +0x3A0 changes on
					// every 0x80026E00 call that got past gate D, +0x37A edges
					// on one-shot latch/clear (0x80026E48 / 0x80014928 bit0,
					// 0x80014B98 / 0x80014DA4 bit1).  A bit0 0->1 WITHOUT a
					// same-frame round703c walk = the one-shot BURNED (gate E
					// failed) - the suspected red killer.
					if (c3a0[r] != m_re_prev_3a0[r] || os37a[r] != m_re_prev_37a[r])
						logerror("ROUNDEND: checker frame=%u t=%.6f rec=%d cnt3a0 %d->%d oneshot37a %04x->%04x wd3a2=%d flags5c=%08x p370=%08x round703c=%u\n",
								frame_count, machine().time().as_double(), r,
								s32(m_re_prev_3a0[r]), s32(c3a0[r]), m_re_prev_37a[r], os37a[r],
								s32(wd3a2[r]), f5c[r], p370[r], r703c);

					// EVENT: +0x5C gate-B-relevant bit movement (bit0 checker
					// gate B, bit3 from +0x370 bit16 @0x80014A20-48, bit16
					// toggle 0x80013C20-50).
					if ((f5c[r] ^ m_re_prev_5c[r]) & 0x00010009)
						logerror("ROUNDEND: flags5c frame=%u t=%.6f rec=%d f5c %08x->%08x bit0 %u->%u p370=%08x\n",
								frame_count, machine().time().as_double(), r,
								m_re_prev_5c[r], f5c[r], m_re_prev_5c[r] & 1, f5c[r] & 1, p370[r]);

					// EVENT: area time-limit watchdog expiry (the 0x80014BA8
					// path's gate) or re-arm (0x80014DEC, desc[9]*60 frames).
					if ((m_re_prev_3a2[r] > 0 && wd3a2[r] <= 0) || wd3a2[r] > m_re_prev_3a2[r])
						logerror("ROUNDEND: wd3a2 frame=%u t=%.6f rec=%d wd3a2 %d->%d flags5c=%08x oneshot37a=%04x p75f8=%08x round703c=%u\n",
								frame_count, machine().time().as_double(), r,
								s32(m_re_prev_3a2[r]), s32(wd3a2[r]), f5c[r], os37a[r], g75f8, r703c);

					// EVENT: segment-clock re-base - +0x394 zero-edge.  BOTH
					// records' +0x394 AND +0x388 zeroed in the same frame is
					// the 0x80104958 fingerprint (writer PC is not observable
					// from a RAM sampler; attribution is static).
					if (m_re_prev_394[r] != 0 && clk394[r] == 0)
						logerror("ROUNDEND: rebase frame=%u t=%.6f rec=%d clk394 %08x->0 rec0_394=%08x rec1_394=%08x rec0_388=%08x rec1_388=%08x round703c=%u sub7040=%u\n",
								frame_count, machine().time().as_double(), r,
								m_re_prev_394[r], clk394[0], clk394[1], clk388[0], clk388[1], r703c, r7040);
				}

				// EVENT: join-armer precondition movement (entered gp+0x7034,
				// arm-enable gp+0x7624 zero-edge, role-byte change - bit1 is
				// the statically-dead G10 gate - co-op bit04, event countdown
				// gp+0x6FF4).
				if (ent7034 != m_re_prev_7034 || (arm7624 == 0) != (m_re_prev_7624 == 0) || role != m_re_prev_role
						|| coop != m_re_prev_coop || g6ff4 != m_re_prev_6ff4)
					logerror("ROUNDEND: join frame=%u t=%.6f entered7034 %04x->%04x bypass7036=%04x arm7624 %08x->%08x role %02x->%02x bit1=%u bit7=%u coop04 %u->%u g6ff4 %04x->%04x screen=%02x\n",
							frame_count, machine().time().as_double(),
							m_re_prev_7034, ent7034, byp7036, m_re_prev_7624, arm7624,
							m_re_prev_role, role, (role >> 1) & 1, (role >> 7) & 1,
							m_re_prev_coop, coop, m_re_prev_6ff4, g6ff4, screen);

				// EVENT: op6F cadence window opened ((frame ctr & 0xFF) == 0).
				// Emit expected this frame iff ka>0 && role bit7 (0x800B22CC).
				if (ctr74f0lo == 0 && m_re_prev_74f0lo != 0)
					logerror("ROUNDEND: op6fwin frame=%u t=%.6f ka=%u rolebit7=%u round703c=%u clk390=%08x clk394=%08x\n",
							frame_count, machine().time().as_double(),
							ka, (role >> 7) & 1, r703c, m_mainram[0x002F43F0 / 4], clk394[0]);

				// EVENT: local player index changed (should never happen
				// in-session; would rewrite the whole record-ownership model).
				if (idx7608 != m_re_prev_7608)
					logerror("ROUNDEND: idx frame=%u t=%.6f idx7608 %u->%u role=%02x mode=%u ka=%u\n",
							frame_count, machine().time().as_double(),
							m_re_prev_7608, idx7608, role, mode, ka);
			}

			// 1/s ROUNDEND: status - the whole trigger chain in one line.
			if (frame_count % 60 == 0)
				logerror("ROUNDEND: status frame=%u t=%.6f idx=%u role=%02x(b1=%u b7=%u) mode=%u ka=%u screen=%02x round703c=%u sub7040=%u g705c=%u code7074=%u cnt7068=%u hold7064=%d timer7054=%08x entered7034=%04x bypass7036=%04x arm7624=%08x coop04=%u g6ff4=%04x cfg75e4=%02x p75f8=%08x ctr74f0lo=%02x rec0[370=%08x 5c=%08x 37a=%04x 3a0=%d 3a2=%d 394=%08x] rec1[370=%08x 5c=%08x 37a=%04x 3a0=%d 3a2=%d 394=%08x]\n",
						frame_count, machine().time().as_double(),
						idx7608, role, (role >> 1) & 1, (role >> 7) & 1, mode, ka, screen,
						r703c, r7040, g705c, c7074, c7068, s32(h7064), t7054,
						ent7034, byp7036, arm7624, coop, g6ff4, cfg75e4, g75f8, ctr74f0lo,
						p370[0], f5c[0], os37a[0], s32(c3a0[0]), s32(wd3a2[0]), clk394[0],
						p370[1], f5c[1], os37a[1], s32(c3a0[1]), s32(wd3a2[1]), clk394[1]);

			for (int r = 0; r < 2; r++)
			{
				m_re_prev_p370[r] = p370[r];
				m_re_prev_5c[r]   = f5c[r];
				m_re_prev_37a[r]  = os37a[r];
				m_re_prev_3a0[r]  = c3a0[r];
				m_re_prev_3a2[r]  = wd3a2[r];
				m_re_prev_394[r]  = clk394[r];
			}
			m_re_prev_703c   = r703c;
			m_re_prev_7040   = r7040;
			m_re_prev_705c   = g705c;
			m_re_prev_7074   = c7074;
			m_re_prev_7034   = ent7034;
			m_re_prev_7624   = arm7624;
			m_re_prev_role   = role;
			m_re_prev_7608   = idx7608;
			m_re_prev_75f8   = g75f8;
			m_re_prev_6ff4   = g6ff4;
			m_re_prev_coop   = coop;
			m_re_prev_74f0lo = ctr74f0lo;
			m_re_have_prev   = true;
		}

		// P036 EXPERIMENT (branch patch/round-arm, off patch/latch-v2-snapshot):
		// ROUND-ARM ASSIST - see the member-block comment for the full design
		// and safety rails.  The project's FIRST functional write to game work
		// RAM beyond the P001 keepalive floor, and by construction its
		// narrowest: ONE halfword (rec0+0x3A0 <- 1, the ROM's own fire-now
		// idiom, wrapper 0x800D1668 @0x800D16A8) at the score/result window
		// exit, one-shot per window, never retried, no other address written
		// on any path.  Ordered AFTER the P034 ROUNDEND tap so a stacked-trace
		// run samples the pristine pre-write state in the same frame (the tap
		// then reports our write as a cnt3a0 edge one frame later - join
		// ROUNDARM: fired to ROUNDEND: checker by frame=).  Inert unset.
		// Experiment branch only - never merge to milestone.
		if (m_patch_roundarm)
		{
			u32 const mode    = m_mainram[0x002F3FD0 / 4];            // 0x802F3FD0 staging mode (2 = linked)
			u32 const ka      = m_mainram[0x002F3FD8 / 4];            // 0x802F3FD8 keepalive
			u32 const idx7608 = m_mainram[0x002CEE28 / 4];            // gp+0x7608 local player index (record-0 model fence)
			u32 const p370_0  = m_mainram[0x002F43D0 / 4];            // rec0 +0x370 (OWN record while idx==0)
			u32 const p370_1  = m_mainram[0x002F4844 / 4];            // rec1 +0x370 (partner mirror, 0xC0000000-marked)
			u32 const r703c   = m_mainram[0x002CE85C / 4];            // gp+0x703C round state (0 = round machine idle)
			u16 const os37a   = u16(m_mainram[0x002F43D8 / 4]);       // rec0 +0x37A one-shot halfword (bit0 gate C, bit1 consumed)
			u32 const w3a0    = m_mainram[0x002F4400 / 4];            // rec0 +0x3A0 (hi lane) / +0x3A2 (lo lane) word
			s16 const c3a0    = s16(w3a0 >> 16);                      // +0x3A0 checker gate-E countdown (the ONE write target)
			s16 const wd3a2   = s16(w3a0);                            // +0x3A2 area watchdog (lo lane - NEVER touched)
			u8  const cfg75e4 = u8(m_mainram[0x002CEE04 / 4] >> 24);  // gp+0x75E4 countdown init value (log only)

			// The MUTUAL score/result signature (derivation in the member
			// comment).  OWN: result-class bit26 + bit16 + 0x40 marker set;
			// mirror-marks/bit23-strobe/0x6000/0x800/0x80/0x20 clear.
			// MIRROR: same with the 0xC0000000 ingest marks ignored.
			// Requiring BOTH records kills the 06018042 walking-state
			// collision (an own-only mask provably cannot).
			bool const own_sig  = (p370_0 & 0xC48168E0) == 0x04010040;
			bool const mir_sig  = (p370_1 & 0x048168E0) == 0x04010040;
			bool const full_sig = own_sig && mir_sig && mode == 2 && ka > 0 && idx7608 == 0;
			bool const b16_edge = (m_ra_prev_p370 & 0x00010000) != 0 && (p370_0 & 0x00010000) == 0;

			// Round-entry edge, any source.  Post-fire it closes the
			// non-retry watch (success); otherwise it documents a natural
			// entry.  Checked BEFORE the overwrite test: round result-1
			// (0x80016C54) legitimately re-inits cnt3a0 to cfg75e4 while the
			// round machine walks - that must never read as an overwrite.
			if (m_ra_prev_703c == 0 && r703c != 0)
			{
				++m_ra_cnt_entered;
				logerror("ROUNDARM: entered frame=%u t=%.6f source=%s round703c=%u dt_frames=%d p370=%08x cnt3a0=%d wd3a2=%d entered_total=%u\n",
						frame_count, machine().time().as_double(),
						m_ra_fire_pending ? "post-fire" : "natural", r703c,
						m_ra_fire_pending ? s32(frame_count - m_ra_fire_frame) : -1,
						p370_0, s32(c3a0), s32(wd3a2), m_ra_cnt_entered);
				m_ra_fire_pending = false;
			}
			// NON-RETRY rail: our planted 1 was overwritten (record re-init
			// raised the countdown) before any checker call consumed it.
			// Log once and stand down - NEVER rewrite (no P025-style fight).
			else if (m_ra_fire_pending && r703c == 0 && s32(c3a0) > 1)
			{
				++m_ra_cnt_overwritten;
				logerror("ROUNDARM: overwritten frame=%u t=%.6f cnt3a0=%d (planted 1 @frame %u, %u frames ago) wd3a2=%d p370=%08x round703c=%u overwritten_total=%u - write did not stick, NO retry\n",
						frame_count, machine().time().as_double(), s32(c3a0),
						m_ra_fire_frame, frame_count - m_ra_fire_frame,
						s32(wd3a2), p370_0, r703c, m_ra_cnt_overwritten);
				m_ra_fire_pending = false;
			}
			// Session ended (teardown/quit) with a fire still pending: clear
			// the watch so a later session's entry edge is not mislabeled.
			else if (m_ra_fire_pending && (mode != 2 || ka == 0))
			{
				++m_ra_cnt_skipped;
				logerror("ROUNDARM: skipped frame=%u t=%.6f reason=pending-cleared-session-end mode=%u ka=%u cnt3a0=%d round703c=%u skipped_total=%u\n",
						frame_count, machine().time().as_double(), mode, ka,
						s32(c3a0), r703c, m_ra_cnt_skipped);
				m_ra_fire_pending = false;
			}

			if (m_ra_wait_reentry)
			{
				// Window consumed: re-arm only after the FULL signature has
				// been absent >= 60 consecutive frames and then re-enters
				// (debounced window boundary - a bounce inside the same score
				// screen cannot re-fire; the next stage boundary, minutes
				// away, legitimately re-arms).
				if (full_sig)
					m_ra_gap_frames = 0;
				else if (++m_ra_gap_frames >= 60)
					m_ra_wait_reentry = false;
			}
			else if (!m_ra_armed)
			{
				if (full_sig)
				{
					m_ra_armed = true;
					m_ra_armed_frame = frame_count;
					++m_ra_cnt_armed;
					logerror("ROUNDARM: armed frame=%u t=%.6f rec0_370=%08x rec1_370=%08x cnt3a0=%d wd3a2=%d oneshot37a=%04x round703c=%u mode=%u ka=%u cfg75e4=%02x armed_total=%u\n",
							frame_count, machine().time().as_double(), p370_0, p370_1,
							s32(c3a0), s32(wd3a2), os37a, r703c, mode, ka, cfg75e4,
							m_ra_cnt_armed);
				}
				else if (b16_edge && m_ra_prev_own_sig)
				{
					// Diagnostic: the own-record window closed before the
					// mirror ever matched (the mutual signature never
					// formed).  No write - but consume the window so a late
					// bounce cannot arm on stale state.
					++m_ra_cnt_skipped;
					m_ra_wait_reentry = true;
					m_ra_gap_frames = 0;
					logerror("ROUNDARM: skipped frame=%u t=%.6f reason=unarmed-window p370 %08x->%08x rec1_370=%08x cnt3a0=%d wd3a2=%d oneshot37a=%04x round703c=%u mode=%u ka=%u skipped_total=%u\n",
							frame_count, machine().time().as_double(), m_ra_prev_p370,
							p370_0, p370_1, s32(c3a0), s32(wd3a2), os37a, r703c, mode,
							ka, m_ra_cnt_skipped);
				}
			}
			else // armed - waiting for the bit16-clear window exit
			{
				if (b16_edge)
				{
					// The window exit.  Evaluate the fire fences in order;
					// plant the ROM's fire-now value or skip (log-only).
					// The window is consumed either way.
					char const *reason = nullptr;
					if (idx7608 != 0)                    reason = "localidx-nonzero";
					else if (r703c != 0)                 reason = "round703c-nonzero";
					else if (mode != 2)                  reason = "mode-not-2";
					else if (ka == 0)                    reason = "ka-zero";
					else if (os37a != 0)                 reason = "oneshot37a-nonzero";
					else if ((p370_0 & 0xC0006800) != 0) reason = "p370-postedge-dirty";
					else if (s32(c3a0) == 1)             reason = "cnt3a0-already-1";
					else if (s32(c3a0) <= 0)             reason = "cnt3a0-lez-checker-firing";
					else if (s32(c3a0) > 15)             reason = "cnt3a0-insane";

					if (!reason)
					{
						// THE WRITE - the only game-RAM store this experiment
						// ever performs: rec0+0x3A0 <- 1 (hi halfword of the
						// word at 0x802F4400), the +0x3A2 watchdog lo
						// halfword preserved bit-exactly.
						u32 const post = (w3a0 & 0x0000FFFFu) | 0x00010000u;
						m_mainram[0x002F4400 / 4] = post;
						++m_ra_cnt_fired;
						m_ra_fire_pending = true;
						m_ra_fire_frame = frame_count;
						logerror("ROUNDARM: fired frame=%u t=%.6f p370 %08x->%08x cnt3a0 %d->1 word3a0 %08x->%08x wd3a2=%d oneshot37a=%04x round703c=%u mode=%u ka=%u fired_total=%u\n",
								frame_count, machine().time().as_double(), m_ra_prev_p370,
								p370_0, s32(c3a0), w3a0, post, s32(wd3a2), os37a, r703c,
								mode, ka, m_ra_cnt_fired);
					}
					else
					{
						++m_ra_cnt_skipped;
						logerror("ROUNDARM: skipped frame=%u t=%.6f reason=%s p370 %08x->%08x cnt3a0=%d wd3a2=%d oneshot37a=%04x round703c=%u mode=%u ka=%u skipped_total=%u\n",
								frame_count, machine().time().as_double(), reason,
								m_ra_prev_p370, p370_0, s32(c3a0), s32(wd3a2), os37a,
								r703c, mode, ka, m_ra_cnt_skipped);
					}
					m_ra_armed = false;
					m_ra_wait_reentry = true;
					m_ra_gap_frames = 0;
				}
				else if (frame_count - m_ra_armed_frame > 300)
				{
					// Edge-timeout: the armed window never presented the
					// bit16-clear exit within 5 s (real score flashes exit in
					// 0.6-1.9 s).  Disarm WITHOUT writing - protects against
					// an unforeseen bit16-held exit path turning a stale arm
					// into a mid-stage fire.
					++m_ra_cnt_skipped;
					m_ra_armed = false;
					m_ra_wait_reentry = true;
					m_ra_gap_frames = 0;
					logerror("ROUNDARM: disarmed frame=%u t=%.6f reason=edge-timeout held=%u rec0_370=%08x rec1_370=%08x cnt3a0=%d round703c=%u mode=%u ka=%u skipped_total=%u\n",
							frame_count, machine().time().as_double(),
							frame_count - m_ra_armed_frame, p370_0, p370_1,
							s32(c3a0), r703c, mode, ka, m_ra_cnt_skipped);
				}
			}

			// 1/s status - ONLY while linked gameplay is staged (mode == 2).
			if (mode == 2 && frame_count % 60 == 0)
				logerror("ROUNDARM: status frame=%u t=%.6f mode=%u ka=%u armed=%u wait_reentry=%u fire_pending=%u round703c=%u rec0_370=%08x rec1_370=%08x cnt3a0=%d wd3a2=%d oneshot37a=%04x cfg75e4=%02x totals[armed=%u fired=%u entered=%u skipped=%u overwritten=%u]\n",
						frame_count, machine().time().as_double(), mode, ka,
						m_ra_armed ? 1u : 0u, m_ra_wait_reentry ? 1u : 0u,
						m_ra_fire_pending ? 1u : 0u, r703c, p370_0, p370_1,
						s32(c3a0), s32(wd3a2), os37a, cfg75e4,
						m_ra_cnt_armed, m_ra_cnt_fired, m_ra_cnt_entered,
						m_ra_cnt_skipped, m_ra_cnt_overwritten);

			m_ra_prev_p370 = p370_0;
			m_ra_prev_own_sig = own_sig;
			m_ra_prev_703c = r703c;
		}

		// P040b: the VERIFY-BEFORE-POKE RAM-CODE NOP (mechanism amendment of
		// the P040 store filter - full rationale + the VERIFIED DRC
		// invalidation analysis live in the member-block comment).  Runs
		// every vblank until terminal: at machine_start the boot loader has
		// not yet copied the main program into RAM, so the word reads 0 for
		// the first frames (a zero word is already a NOP - the store cannot
		// execute while we wait).  One-shot both ways: poke on the verified
		// original word, REFUSE on any other nonzero word (ROM-revision
		// guard).  Poking here - within the first seconds of boot - precedes
		// the receiver's first execution (link-up, ~56 s+), so the DRC
		// compiles its block from the already-NOPed RAM and needs no
		// invalidation on this path; the 1/s guard in the status block below
		// covers re-materialization.
		if (m_patch_op6f_no394b && !m_no394b_poked && !m_no394b_refused)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word = prog.read_dword(0x000b2508);
			if (word == 0xac460394)
			{
				prog.write_dword(0x000b2508, 0x00000000);
				m_no394b_poked = true;
				logerror("OP6F_NO394B: poked @0x800B2508 old=AC460394 new=00000000 frame=%u t=%.6f (op6F 394-adoption store sw $a2,0x394($v0) NOPed in RAM before its block can be DRC-compiled - receiver 0x800B2448 first runs at link-up; +0x390 adoption @0x800B2504 untouched; 1/s guard re-checks the word)\n",
						frame_count, machine().time().as_double());
			}
			else if (word != 0)
			{
				m_no394b_refused = true;
				logerror("OP6F_NO394B: REFUSED @0x800B2508 found=%08x expected=AC460394 frame=%u t=%.6f - ROM-revision guard: staying inert (no poke ever; falsifier + status stay armed)\n",
						word, frame_count, machine().time().as_double());
			}
		}

		// P041: the SAME verify-before-poke RAM-code NOP for the +0x390
		// PLAY-CLOCK adoption store - the OTHER HALF of the pair (the P040b
		// half-block left blue's 390 still adopting, splitting the local
		// 390/394 pair = the measured owner of blue's gameplay jitter; full
		// rationale + the verified word live in the member-block comment).
		// Word VERIFIED in full.txt: 0xAC470390 = sw $a3,0x390($v0)
		// @0x800B2504, one instruction before the P040b-NOPed 394 store.
		// Same states, same ordering soundness; own falsifier + 1/s guard
		// below (the shared 394 rails stay untouched).
		if (m_patch_op6f_no390b && !m_no390b_poked && !m_no390b_refused)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word = prog.read_dword(0x000b2504);
			if (word == 0xac470390)
			{
				prog.write_dword(0x000b2504, 0x00000000);
				m_no390b_poked = true;
				logerror("OP6F_NO390B: poked @0x800B2504 old=AC470390 new=00000000 frame=%u t=%.6f (op6F 390-adoption store sw $a3,0x390($v0) NOPed in RAM before its block can be DRC-compiled - receiver 0x800B2448 first runs at link-up; pairs with the P040b 394 NOP @0x800B2508 = BOTH adoption halves blocked, local pair consistency restored; 1/s guard re-checks the word)\n",
						frame_count, machine().time().as_double());
			}
			else if (word != 0)
			{
				m_no390b_refused = true;
				logerror("OP6F_NO390B: REFUSED @0x800B2504 found=%08x expected=AC470390 frame=%u t=%.6f - ROM-revision guard: staying inert (no poke ever; falsifier + status stay armed)\n",
						word, frame_count, machine().time().as_double());
			}
		}

		// P043: the WAVE-INIT HOLD verify-before-poke pair (full rationale +
		// the P042 race map + both VERIFIED instruction words live in the
		// member-block comment).  Two INDEPENDENT one-shot pokes - a REFUSE on
		// one must not (and structurally cannot) block the other; each waits
		// for the boot loader (word reads 0 until the program image lands),
		// pokes on the verified original word, refuses on any other nonzero
		// word.  Poking in early boot precedes the completion check's first
		// execution (attract-demo gameplay, tens of seconds later), so the
		// DRC compiles its blocks from the already-poked RAM - and a residual
		// re-copy window fails to STOCK behavior only (the guard in the 1/s
		// status block below re-pokes on re-materialization).
		if (m_patch_waveinit_hold)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);

			// Poke 1: window-narrow - slti $v0,$v0,0xB -> slti $v0,$v0,1
			// (the walk-phase condition-B window closes until end-cursor<1;
			// fight areas activate at end-cursor==1 and can no longer
			// complete before their wave exists).
			if (!m_wih_win_poked && !m_wih_win_refused)
			{
				u32 const word = prog.read_dword(0x0001417c);
				if (word == 0x2842000b)
				{
					prog.write_dword(0x0001417c, 0x28420001);
					m_wih_win_poked = true;
					logerror("WAVEINIT_HOLD: poked window @0x8001417C old=2842000B new=28420001 frame=%u t=%.6f (slti $v0,$v0,0xB -> slti $v0,$v0,1: condition B of completion check 0x80014074 held until end-cursor<1 in the walk - the 10-frame ask-before-the-wave-exists window is closed; activation @0x800A0E5C fires at end-cursor==1, walk-only advance rides id==-1 at end-cursor<=0; 1/s guard re-checks the word)\n",
							frame_count, machine().time().as_double());
				}
				else if (word != 0)
				{
					m_wih_win_refused = true;
					logerror("WAVEINIT_HOLD: REFUSED window @0x8001417C found=%08x expected=2842000B frame=%u t=%.6f - ROM-revision guard: window poke stays inert (the clamp poke is INDEPENDENT and continues; status stays armed)\n",
							word, frame_count, machine().time().as_double());
				}
			}

			// Poke 2: not-found clamp - li $v0,-2 -> li $v0,0 (the ring
			// lookup's not-found verdict reads "alive"; kills the
			// never-registered flavor everywhere incl. the fight phase;
			// sole caller of 0x800B5690 is condition B's jal @0x8001418C).
			if (!m_wih_clamp_poked && !m_wih_clamp_refused)
			{
				u32 const word = prog.read_dword(0x000b56e0);
				if (word == 0x2402fffe)
				{
					prog.write_dword(0x000b56e0, 0x24020000);
					m_wih_clamp_poked = true;
					logerror("WAVEINIT_HOLD: poked clamp @0x800B56E0 old=2402FFFE new=24020000 frame=%u t=%.6f (li $v0,-2 -> li $v0,0: ring lookup 0x800B5690 not-found verdict clamped neutral - a never-registered anchor no longer completes its area; id==-1 -> -1 walk-advance path and found-slot handle return untouched; 1/s guard re-checks the word)\n",
							frame_count, machine().time().as_double());
				}
				else if (word != 0)
				{
					m_wih_clamp_refused = true;
					logerror("WAVEINIT_HOLD: REFUSED clamp @0x800B56E0 found=%08x expected=2402FFFE frame=%u t=%.6f - ROM-revision guard: clamp poke stays inert (the window poke is INDEPENDENT and continues; status stays armed)\n",
							word, frame_count, machine().time().as_double());
				}
			}
		}

		// P048: the REAPER-PATIENCE verify-before-poke (one word - full P047
		// rationale, the verified instruction and the structural bit31
		// discriminator live in the member-block comment).  Runs every vblank
		// until terminal: the word reads 0 until the boot loader copies the
		// program image (stock behavior meanwhile - fail-to-stock, never
		// fail-open).  One-shot both ways: poke on the verified original word
		// (the replacement is CONSTRUCTED from it by changing ONLY the 16-bit
		// immediate field), REFUSE on any other nonzero word (ROM-revision
		// guard).  Poking in early boot precedes the reap tail's first
		// execution (attract-mode entities, tens of frames later), so the DRC
		// compiles the tick's block from the already-widened RAM; the 1/s
		// guard below covers re-materialization.
		if (m_patch_reaper_patience != 0 && !m_rpp_poked && !m_rpp_refused)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word = prog.read_dword(0x000b71a0);
			if (word == 0x2c420011)
			{
				u32 const imm = (m_patch_reaper_patience == 2) ? 0x0020 : 0x001f;
				u32 const patched = (word & 0xffff0000) | imm; // still sltiu $v0,$v0,imm - opcode/rs/rt untouched
				prog.write_dword(0x000b71a0, patched);
				m_rpp_poked = true;
				logerror("REAPER_PATIENCE: poked @0x800B71A0 old=2C420011 new=%08X mode=%d frame=%u t=%.6f (bit31 snapshot-timeout reaper gate sltiu $v0,$v0,0x11 widened: %s; reap tail 0x800B71CC first runs at attract gameplay so the DRC compiles from the widened RAM; bit31-CLEAR entities and every designed script-END completion are structurally untouched; 1/s guard re-checks the word)\n",
						patched, m_patch_reaper_patience, frame_count, machine().time().as_double(),
						(m_patch_reaper_patience == 2)
								? "patience 17 ticks -> OFF (imm 0x20 unreachable by the &0x1F count - escalation reserve; 40 s watchdog + partner despawn echoes remain the orphan cleanup)"
								: "patience 17 -> 31 ticks (~0.28 s -> ~0.52 s, beyond the 0.45 s 3-hop HB-floor worst case)");
			}
			else if (word != 0)
			{
				m_rpp_refused = true;
				logerror("REAPER_PATIENCE: REFUSED @0x800B71A0 found=%08x expected=2C420011 frame=%u t=%.6f - ROM-revision guard: staying inert (no poke ever; status stays armed)\n",
						word, frame_count, machine().time().as_double());
			}
		}

		// P050: the SINGLE-BURST PUMP verify-before-poke (one word - full P049
		// rationale, the verified instruction and the RX-validator bound live in
		// the member-block comment).  Runs every vblank until terminal: the word
		// reads 0 until the boot loader copies the program (stock behavior
		// meanwhile - fail-to-stock).  One-shot both ways: poke on the verified
		// original word (the replacement is CONSTRUCTED from it by changing ONLY
		// the 16-bit immediate 0x0100 -> 0x0401), REFUSE on any other nonzero
		// word.  Poking in early boot precedes the pump's frame-start first
		// execution at attract link-up, so the DRC compiles the pump block from
		// the already-poked RAM; the 1/s guard below covers re-materialization.
		if (m_patch_burst_quantum && !m_bq_poked && !m_bq_refused)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word = prog.read_dword(0x0000bc78);
			if (word == 0x28420100)
			{
				u32 const patched = (word & 0xffff0000) | 0x0401; // still slti $v0,$v0,imm - opcode/rs/rt untouched
				prog.write_dword(0x0000bc78, patched);
				m_bq_poked = true;
				logerror("BURST_QUANTUM: poked @0x8000BC78 old=28420100 new=%08X frame=%u t=%.6f (TX pump burst quantum slti 0x100 -> slti 0x401: every VM frame <= 0x400 hw now emits as ONE burst instead of a <=0xFF-hw chunk train - the RX drain validator @0x8000BF90 accepts <= 0x400 hw and each C422 slot is 0x400 hw, our frames <= 0x394 hw; the 0xFF-hw continuation paths go dead; the pump's frame-start first runs at attract link-up so the DRC compiles from the poked RAM; 1/s guard re-checks the word; device companions arm on NAMCOS23_PATCH_BURST_QUANTUM in namco_c139 device_start)\n",
						patched, frame_count, machine().time().as_double());
			}
			else if (word != 0)
			{
				m_bq_refused = true;
				logerror("BURST_QUANTUM: REFUSED @0x8000BC78 found=%08x expected=28420100 frame=%u t=%.6f - ROM-revision guard: staying inert (no poke ever; status stays armed)\n",
						word, frame_count, machine().time().as_double());
			}
		}

		// P066: the LINK-WAIT partner-search-countdown seed verify-before-poke
		// (one word - the full RE rationale, the verified instruction, the
		// ~10 counts/s unit calibration and the both-cabs caveat live in the
		// member-block comment).  Runs every vblank until terminal: the word
		// reads 0 until the boot loader copies the program image (stock
		// behavior meanwhile - fail-to-stock, never fail-open).  One-shot both
		// ways: poke on the verified original word (the replacement is
		// CONSTRUCTED from it by changing ONLY the 16-bit immediate 0x012C ->
		// units), REFUSE on any other nonzero word (ROM-revision guard).
		// Poking in early boot precedes the state-0 seed's first execution
		// (the boot init chain runs first - the family's tightest but still
		// sufficient margin), so the DRC compiles the seed block from the
		// already-poked RAM; the 1/s guard below covers re-materialization.
		if (m_patch_link_wait_units != 0 && !m_lw_poked && !m_lw_refused)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word = prog.read_dword(0x00009ab8);
			if (word == 0x2402012c)
			{
				u32 const patched = (word & 0xffff0000) | u32(m_patch_link_wait_units); // still li $v0,imm - opcode/rs/rt untouched
				prog.write_dword(0x00009ab8, patched);
				m_lw_poked = true;
				logerror("LINK_WAIT: poked @0x80009AB8 old=2402012C new=%08X units=%d (~%d s at the observed ~10 counts/s) frame=%u t=%.6f (partner-search countdown seed li $v0,300 widened; state 0 0x80009A90 stores it to gp+0x6F78, the state-1 tick 0x80009AD8 counts it down on the NETWORK CHECK screen, and its expiry - the same timer - is the sole trigger advancing to the state-2 link-vs-solo flow; op55 machinery untouched; BOTH cabs must run the same value or the shorter cab leaves partner search early; the on-screen start value = units is the visible confirmation; 1/s guard re-checks the word)\n",
						patched, m_patch_link_wait_units, m_patch_link_wait_units / 10, frame_count, machine().time().as_double());
			}
			else if (word != 0)
			{
				m_lw_refused = true;
				logerror("LINK_WAIT: REFUSED @0x80009AB8 found=%08x expected=2402012C frame=%u t=%.6f - ROM-revision guard: staying inert (no poke ever; status stays armed)\n",
						word, frame_count, machine().time().as_double());
			}
		}

		// P040: the ADOPT-EDGE FALSIFIER (read-only; armed by EITHER
		// suppression env - the v1 filter NAMCOS23_PATCH_OP6F_NO394 or the
		// P040b NOP NAMCOS23_PATCH_OP6F_NO394B: the falsifier is pc-free and
		// DRC-safe, so the mechanism swap keeps it VERBATIM - see the
		// member-block comments).  The adoption fingerprint from the P038/P039
		// analyses: rec0+0x394 jumping by more than +/-2 between vblank samples
		// AND landing within +/-3 of rec0+0x390 (the master's never-re-based
		// EQUAL pair is wall-synced to the slave's own play clock, so an
		// adoption lands 394 == ~390; P038 run: +8 @95.269 and the +9286
		// poison @253.388, both landing ==390, red zero).  With the filter
		// armed this line MUST stay silent - a hit means a second adoption
		// path the static RE missed (or a Handler-A saved-pair restore whose
		// saved pair sits within +/-3 of the running play clock: result-screen
		// windows only, cross-check ROUNDEND lines by t=).  Legit writers
		// cannot fire it: tick jumps are +1/+2 by definition; the re-base
		// zero and init -1 land nowhere near the running 390; prints rec0_390
		// INLINE per the P038 test-plan addition.
		if (m_patch_op6f_no394 || m_patch_op6f_no394b)
		{
			u32 const rec0_390 = m_mainram[0x002F43F0 / 4]; // rec0 +0x390 play clock
			u32 const rec0_394 = m_mainram[0x002F43F4 / 4]; // rec0 +0x394 segment clock
			if (m_no394_have_prev)
			{
				s32 const jump = s32(rec0_394 - m_no394_prev_394);
				s32 const gap  = s32(rec0_394 - rec0_390);
				if ((jump < -2 || jump > 2) && gap >= -3 && gap <= 3)
				{
					++m_no394_adopt_edges;
					logerror("OP6F_NO394: adopt-edge frame=%u t=%.6f jump=%d new394=%08x cur390=%08x edges_total=%u (MUST stay 0 while the filter is armed - a hit = an adoption landed via a path the filter does not cover)\n",
							frame_count, machine().time().as_double(), jump,
							rec0_394, rec0_390, m_no394_adopt_edges);
				}
			}
			m_no394_prev_394 = rec0_394;
			m_no394_have_prev = true;

			// 1/s status: filter + falsifier counters and the live clock pair
			// (cheap: two RAM reads already done + one line).
			if (frame_count % 60 == 0)
			{
				logerror("OP6F_NO394: status frame=%u t=%.6f blocked=%u(+%u) pass394=%u(+%u) adoptedge=%u rec0_390=%08x rec0_394=%08x\n",
						frame_count, machine().time().as_double(),
						m_no394_blocked, m_no394_blocked_win,
						m_no394_pass394, m_no394_pass394_win,
						m_no394_adopt_edges, rec0_390, rec0_394);
				m_no394_blocked_win = 0;
				m_no394_pass394_win = 0;

				// P040b 1/s GUARD + status: re-read the poked word.  If the
				// ORIGINAL instruction re-materialized (loader re-copy /
				// soft-reset re-boot - the only ways RAM regains it), re-poke
				// immediately and count each observation.  The status line's
				// word2508 field (the OBSERVED, pre-re-poke value) is the
				// NO394B aliveness rail: expect 00000000 from the poke onward
				// - any nonzero value = the refused word or a
				// re-materialization instant.  Under NO394B-only the v1 tap
				// counters above legitimately read 0 (no tap installed).
				if (m_patch_op6f_no394b)
				{
					address_space &prog = m_maincpu->space(AS_PROGRAM);
					u32 const word2508 = prog.read_dword(0x000b2508);
					if (m_no394b_poked && word2508 == 0xac460394)
					{
						prog.write_dword(0x000b2508, 0x00000000);
						++m_no394b_repokes;
						logerror("OP6F_NO394B: re-poke @0x800B2508 old=AC460394 new=00000000 frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window would evade loose verify - adopt-edge is the backstop)\n",
								frame_count, machine().time().as_double(), m_no394b_repokes);
					}
					logerror("OP6F_NO394B: status frame=%u t=%.6f word2508=%08x poked=%u refused=%u repokes=%u\n",
							frame_count, machine().time().as_double(), word2508,
							m_no394b_poked ? 1u : 0u, m_no394b_refused ? 1u : 0u,
							m_no394b_repokes);
				}
			}
		}

		// P041: the 390-JUMP CENSUS FALSIFIER + the NO390B 1/s guard/status
		// (own block, own gate - the shared 394 rails above stay UNTOUCHED).
		// Any POSITIVE rec0+0x390 jump > +2 between vblank samples is the
		// adoption fingerprint (P040b run 2 measured landings +3..+12 on the
		// slave; normal ticks are +1/+2, boot/seed re-inits are -1-class or
		// negative, the segment re-base zeroes 394 only - none of those can
		// fire this).  With the poke armed it MUST stay 0 on both cabs; a
		// hit = a 390 adoption landed via a path the NOP does not cover (or
		// a Handler-A saved-pair restore - result-screen windows only,
		// cross-check ROUNDEND lines by t=).  The recipe's PLAYCLOCK
		// clock-jump rec0_390 census stays the independent cross-check.
		if (m_patch_op6f_no390b)
		{
			u32 const rec0_390 = m_mainram[0x002F43F0 / 4]; // rec0 +0x390 play clock
			if (m_no390b_have_prev)
			{
				s32 const jump = s32(rec0_390 - m_no390b_prev_390);
				if (jump > 2)
				{
					++m_no390b_jump390s;
					logerror("OP6F_NO390B: 390-jump frame=%u t=%.6f jump=%d new390=%08x jumps_total=%u (MUST stay 0 while the poke is armed - a positive jump >2 = a 390 adoption landed via a path the NOP does not cover; cross-check PLAYCLOCK clock-jump rec0_390 and ROUNDEND by t=)\n",
							frame_count, machine().time().as_double(), jump,
							rec0_390, m_no390b_jump390s);
				}
			}
			m_no390b_prev_390 = rec0_390;
			m_no390b_have_prev = true;

			// 1/s GUARD + status: re-read the poked word; if the ORIGINAL
			// instruction re-materialized (loader re-copy / soft-reset
			// re-boot - the only ways RAM regains it), re-poke immediately
			// and count each observation.  word2504 prints the OBSERVED,
			// pre-re-poke value = the NO390B aliveness rail: expect 00000000
			// from the poke onward.
			if (frame_count % 60 == 0)
			{
				address_space &prog = m_maincpu->space(AS_PROGRAM);
				u32 const word2504 = prog.read_dword(0x000b2504);
				if (m_no390b_poked && word2504 == 0xac470390)
				{
					prog.write_dword(0x000b2504, 0x00000000);
					++m_no390b_repokes;
					logerror("OP6F_NO390B: re-poke @0x800B2504 old=AC470390 new=00000000 frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window would evade loose verify - the 390-jump census is the backstop)\n",
							frame_count, machine().time().as_double(), m_no390b_repokes);
				}
				logerror("OP6F_NO390B: status frame=%u t=%.6f word2504=%08x poked=%u refused=%u repokes=%u jump390s=%u rec0_390=%08x\n",
						frame_count, machine().time().as_double(), word2504,
						m_no390b_poked ? 1u : 0u, m_no390b_refused ? 1u : 0u,
						m_no390b_repokes, m_no390b_jump390s, rec0_390);
			}
		}

		// P043: the COMBINED 1/s guard + status for the wave-init-hold pair.
		// Re-read both words; if an ORIGINAL instruction re-materialized
		// (loader re-copy / soft-reset re-boot - the only ways RAM regains
		// it), re-poke immediately and count each observation.  The status
		// line carries BOTH word rails (the OBSERVED, pre-re-poke values):
		// expect word1417c=28420001 and wordb56e0=24020000 from the pokes
		// onward - the ORIGINAL words (2842000B / 2402FFFE) on a status line
		// = a re-materialization instant; anything else = the refused word.
		if (m_patch_waveinit_hold && frame_count % 60 == 0)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word1417c = prog.read_dword(0x0001417c);
			u32 const wordb56e0 = prog.read_dword(0x000b56e0);
			if (m_wih_win_poked && word1417c == 0x2842000b)
			{
				prog.write_dword(0x0001417c, 0x28420001);
				++m_wih_win_repokes;
				logerror("WAVEINIT_HOLD: re-poke window @0x8001417C old=2842000B new=28420001 frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window merely runs the STOCK 10-frame check - fail-to-stock, the WAVEINIT trace exposes any resulting walk-window bit16)\n",
						frame_count, machine().time().as_double(), m_wih_win_repokes);
			}
			if (m_wih_clamp_poked && wordb56e0 == 0x2402fffe)
			{
				prog.write_dword(0x000b56e0, 0x24020000);
				++m_wih_clamp_repokes;
				logerror("WAVEINIT_HOLD: re-poke clamp @0x800B56E0 old=2402FFFE new=24020000 frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window merely returns the STOCK -2 verdict - fail-to-stock, the WAVEINIT trace exposes any resulting beta completion)\n",
						frame_count, machine().time().as_double(), m_wih_clamp_repokes);
			}
			logerror("WAVEINIT_HOLD: status frame=%u t=%.6f word1417c=%08x wordb56e0=%08x win[poked=%u refused=%u repokes=%u] clamp[poked=%u refused=%u repokes=%u]\n",
					frame_count, machine().time().as_double(), word1417c, wordb56e0,
					m_wih_win_poked ? 1u : 0u, m_wih_win_refused ? 1u : 0u, m_wih_win_repokes,
					m_wih_clamp_poked ? 1u : 0u, m_wih_clamp_refused ? 1u : 0u, m_wih_clamp_repokes);
		}

		// P048: 1/s guard + status for the reaper-patience poke.  Re-read the
		// word; if the ORIGINAL instruction re-materialized (loader re-copy /
		// soft-reset re-boot - the only ways RAM regains it), re-poke
		// immediately and count each observation.  wordb71a0 prints the
		// OBSERVED, pre-re-poke value = the aliveness rail: expect 2C42001F
		// (mode 1) / 2C420020 (mode 2) from the poke onward - 2C420011 on a
		// status line = a re-materialization instant; anything else = the
		// refused word.  A block compiled inside the sub-second re-copy window
		// keeps STOCK 17-tick patience only (fail-to-stock) - the run's OP20
		// reg-to-kill offset census (the +[15,18] f ring-kill class MUST be 0)
		// is the behavioral backstop.
		if (m_patch_reaper_patience != 0 && frame_count % 60 == 0)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const wordb71a0 = prog.read_dword(0x000b71a0);
			if (m_rpp_poked && wordb71a0 == 0x2c420011)
			{
				u32 const imm = (m_patch_reaper_patience == 2) ? 0x0020 : 0x001f;
				u32 const patched = (wordb71a0 & 0xffff0000) | imm;
				prog.write_dword(0x000b71a0, patched);
				++m_rpp_repokes;
				logerror("REAPER_PATIENCE: re-poke @0x800B71A0 old=2C420011 new=%08X frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window merely keeps STOCK 17-tick patience - fail-to-stock, the OP20 reg-to-kill census is the backstop)\n",
						patched, frame_count, machine().time().as_double(), m_rpp_repokes);
			}
			logerror("REAPER_PATIENCE: status frame=%u t=%.6f wordb71a0=%08x mode=%d poked=%u refused=%u repokes=%u\n",
					frame_count, machine().time().as_double(), wordb71a0,
					m_patch_reaper_patience,
					m_rpp_poked ? 1u : 0u, m_rpp_refused ? 1u : 0u, m_rpp_repokes);
		}

		// P050: the SINGLE-BURST PUMP 1/s re-check/re-poke guard + status.
		// wordbc78 prints the OBSERVED pre-re-poke value = the aliveness rail:
		// expect 28420401 from the poke onward; 28420100 on a status line = a
		// re-materialization instant (loader re-copy / soft-reset re-boot).  A
		// block compiled inside the sub-second re-copy window keeps the STOCK
		// 0x100 quantum only (fail-to-stock) - the device-side single_burst_tx
		// counter + the absence of 255-hw chunk-train logs in fights is the
		// behavioral backstop.
		if (m_patch_burst_quantum && frame_count % 60 == 0)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const wordbc78 = prog.read_dword(0x0000bc78);
			if (m_bq_poked && wordbc78 == 0x28420100)
			{
				u32 const patched = (wordbc78 & 0xffff0000) | 0x0401;
				prog.write_dword(0x0000bc78, patched);
				++m_bq_repokes;
				logerror("BURST_QUANTUM: re-poke @0x8000BC78 old=28420100 new=%08X frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a block compiled from the original word inside this sub-second window merely keeps the STOCK 0x100 burst quantum - fail-to-stock, the device single_burst_tx / chunk-train census is the backstop)\n",
						patched, frame_count, machine().time().as_double(), m_bq_repokes);
			}
			logerror("BURST_QUANTUM: status frame=%u t=%.6f wordbc78=%08x poked=%u refused=%u repokes=%u\n",
					frame_count, machine().time().as_double(), wordbc78,
					m_bq_poked ? 1u : 0u, m_bq_refused ? 1u : 0u, m_bq_repokes);
		}

		// P066: the LINK-WAIT seed 1/s re-check/re-poke guard + status.
		// word9ab8 prints the OBSERVED pre-re-poke value = the aliveness rail:
		// expect 2402xxxx with xxxx = units from the poke onward; 2402012C on
		// a status line = a re-materialization instant (loader re-copy /
		// soft-reset re-boot).  A seed executed inside such a window counted
		// STOCK 300 only (fail-to-stock) - the on-screen countdown start
		// value is the behavioral backstop.
		if (m_patch_link_wait_units != 0 && frame_count % 60 == 0)
		{
			address_space &prog = m_maincpu->space(AS_PROGRAM);
			u32 const word9ab8 = prog.read_dword(0x00009ab8);
			if (m_lw_poked && word9ab8 == 0x2402012c)
			{
				u32 const patched = (word9ab8 & 0xffff0000) | u32(m_patch_link_wait_units);
				prog.write_dword(0x00009ab8, patched);
				++m_lw_repokes;
				logerror("LINK_WAIT: re-poke @0x80009AB8 old=2402012C new=%08X frame=%u t=%.6f repokes_total=%u (word re-materialized - loader re-copy or soft-reset re-boot; a state-0 seed executed inside this window counted STOCK 300 - fail-to-stock, the on-screen start value is the tell)\n",
						patched, frame_count, machine().time().as_double(), m_lw_repokes);
			}
			logerror("LINK_WAIT: status frame=%u t=%.6f word9ab8=%08x units=%d poked=%u refused=%u repokes=%u\n",
					frame_count, machine().time().as_double(), word9ab8,
					m_patch_link_wait_units,
					m_lw_poked ? 1u : 0u, m_lw_refused ? 1u : 0u, m_lw_repokes);
		}

		// P043: READ-ONLY WAVEINIT ring/anchor trace (independent env; full
		// line spec in the member-block comment).  Per-event only: a line at
		// every p370 bit16 SET edge (with the emulated ring lookup naming the
		// poison flavor) and one line per engage cycle at its activation.
		// All reads m_mainram; the ring lookup emulation replicates
		// 0x800B5690 exactly (rotation (cursor+a1)&3 for a1=3..0, first id
		// match returns the slot's lh handle; id==-1 -> -1; none -> -2).
		if (m_trace_waveinit)
		{
			static u32 const rec_base[2] = { 0x002F4060, 0x002F44D4 }; // rec0 / rec1 (stride 0x474)
			for (int r = 0; r < 2; r++)
			{
				u32 const base   = rec_base[r];
				u32 const p370   = m_mainram[(base + 0x370) / 4];
				u32 const f5c    = m_mainram[(base + 0x5C) / 4];
				u32 const w68    = m_mainram[(base + 0x68) / 4];
				s32 const cursor = s16(w68 >> 16);                 // rec+0x68 intro/fight timeline cursor (lh)
				s32 const tend   = s16(w68);                       // rec+0x6A timeline end (lh)
				s32 const endcur = tend - cursor;
				u32 const anchor = m_mainram[(base + 0x364) / 4];  // wave-anchor entity id (desc[0x14])

				if (m_wt_have_prev)
				{
					// Engage rise (p370 bit5 0x20): re-arm the cycle trackers.
					if ((p370 & 0x20) && !(m_wt_prev_p370[r] & 0x20))
					{
						m_wt_engage_t[r] = machine().time().as_double();
						m_wt_engage_anchor[r] = anchor;
						m_wt_win11_t[r] = -1.0;
						m_wt_win1_t[r] = -1.0;
					}

					// Stock/narrowed check-window entries, walk phase only
					// (f5c bit0 clear; the fight phase re-sweeps the cursor).
					if (!(f5c & 1))
					{
						if (m_wt_win11_t[r] < 0.0 && endcur < 11 && m_wt_prev_endcur[r] >= 11)
							m_wt_win11_t[r] = machine().time().as_double();
						if (m_wt_win1_t[r] < 0.0 && endcur < 1 && m_wt_prev_endcur[r] >= 1)
							m_wt_win1_t[r] = machine().time().as_double();
					}

					// THE EDGE: p370 bit16 0->1 = an area completion latched.
					// Dump everything condition B read plus the flavor verdict.
					if ((p370 & 0x00010000) && !(m_wt_prev_p370[r] & 0x00010000))
					{
						u32 const map368  = m_mainram[(base + 0x368) / 4];   // big-map view of the anchor (0x800B7398 result, non-ring)
						u32 const ringcur = m_mainram[0x002CEC00 / 4];       // gp+0x73E0 ring rotation cursor
						u32 ringid[4];
						s32 ringh[4];
						for (int s = 0; s < 4; s++)
						{
							ringid[s] = m_mainram[(0x002D2030 + 8 * s) / 4];
							ringh[s]  = s16(m_mainram[(0x002D2034 + 8 * s) / 4] >> 16);
						}
						s32 lookup = -2;
						if (anchor == 0xffffffff)
							lookup = -1;
						else for (int a1 = 3; a1 >= 0; a1--)
						{
							int const s = (ringcur + a1) & 3;
							if (ringid[s] == anchor)
							{
								lookup = ringh[s];
								break;
							}
						}
						bool const rearm = (m_wt_engage_t[r] >= 0.0) && (anchor != m_wt_engage_anchor[r]);
						char const *flavor;
						if (p370 & 0x00020000)          flavor = "hookA-conditionA";
						else if (rearm)                 flavor = "gamma-rearm";
						else if (anchor == 0xffffffff)  flavor = "no-id-walkclass";
						else if (lookup == -2)          flavor = "beta-never-registered";
						else if (lookup < 0)            flavor = "alpha-released-handle";
						else                            flavor = "healthy-unexplained";
						logerror("WAVEINIT: bit16 frame=%u t=%.6f rec=%d p370 %08x->%08x bit17=%u f5c=%08x bit0=%u cursor=%d end=%d endcur=%d anchor=%08x anchor@engage=%08x map368=%08x ring[%08x:%d %08x:%d %08x:%d %08x:%d] ringcur=%u lookup=%d flavor=%s engage_t=%.6f win11_t=%.6f\n",
								frame_count, machine().time().as_double(), r,
								m_wt_prev_p370[r], p370, (p370 >> 17) & 1, f5c, f5c & 1,
								cursor, tend, endcur, anchor, m_wt_engage_anchor[r], map368,
								ringid[0], ringh[0], ringid[1], ringh[1],
								ringid[2], ringh[2], ringid[3], ringh[3],
								ringcur, lookup, flavor, m_wt_engage_t[r], m_wt_win11_t[r]);
					}

					// Activation (f5c bit0 0->1): the per-engage one-liner.
					// bit16_now=1 here = the completion PRE-LATCHED inside the
					// walk = the skip signature (expect 0 with the pokes armed).
					if ((f5c & 1) && !(m_wt_prev_5c[r] & 1))
						logerror("WAVEINIT: engage-cycle frame=%u t=%.6f rec=%d engage_t=%.6f win11_t=%.6f win1_t=%.6f act_t=%.6f anchor=%08x bit16_now=%u p370=%08x\n",
								frame_count, machine().time().as_double(), r,
								m_wt_engage_t[r], m_wt_win11_t[r], m_wt_win1_t[r],
								machine().time().as_double(), anchor,
								(p370 >> 16) & 1, p370);
				}

				m_wt_prev_p370[r] = p370;
				m_wt_prev_5c[r] = f5c;
				m_wt_prev_endcur[r] = endcur;
			}
			m_wt_have_prev = true;
		}

		// P044: the ANCHOR LIFECYCLE block - trace (OP20:), shield
		// (ANCHOR_SHIELD:) and resurrect scrub (ANCHOR_RESURRECT:).  Full
		// design, the verified ring facts and the ordering contract live in
		// the member-block comment.  One shared sample per vblank; the trace
		// diffs ROM-caused transactions BEFORE any of our writes; prevs are
		// updated to the POST-write state at the end.
		if (m_trace_op20 || m_patch_anchor_shield || m_patch_anchor_resurrect)
		{
			static u32 const anl_rec_base[2] = { 0x002F4060, 0x002F44D4 }; // rec0 / rec1 (stride 0x474)
			double const now = machine().time().as_double();

			// ---- shared sample (ring + both records) ----
			u32 ringid[4];
			s32 ringh[4];
			for (int s = 0; s < 4; s++)
			{
				ringid[s] = m_mainram[(0x002D2030 + 8 * s) / 4];
				ringh[s]  = s16(m_mainram[(0x002D2034 + 8 * s) / 4] >> 16); // lh lane = top half (BE)
			}
			u32 const ringcur = m_mainram[0x002CEC00 / 4]; // gp+0x73E0 rotation cursor
			u32 p370[2], f5c[2], anchor[2], desc[2];
			s32 endcur[2];
			for (int r = 0; r < 2; r++)
			{
				u32 const base = anl_rec_base[r];
				p370[r]   = m_mainram[(base + 0x370) / 4];
				f5c[r]    = m_mainram[(base + 0x5C) / 4];
				anchor[r] = m_mainram[(base + 0x364) / 4];
				desc[r]   = m_mainram[(base + 0x35C) / 4];
				u32 const w68 = m_mainram[(base + 0x68) / 4];
				endcur[r] = s32(s16(w68)) - s32(s16(w68 >> 16)); // end (lh +0x6A) - cursor (lh +0x68)
			}
			// Rotation-faithful ring find (replicates BOTH ROM find loops:
			// first id match from (cursor+3)&3 down; id -1 never searches).
			auto ring_find = [&ringid, ringcur](u32 id) -> int {
					if (id == 0xffffffff)
						return -1;
					for (int a1 = 3; a1 >= 0; a1--)
					{
						int const s = int((ringcur + u32(a1)) & 3);
						if (ringid[s] == id)
							return s;
					}
					return -1;
				};

			// ---- gate 1: READ-ONLY ring-transaction trace ----
			if (m_trace_op20 && m_anl_have_prev)
			{
				// Per-slot deltas since the last post-write sample = ROM-caused
				// transactions during the last frame.
				for (int s = 0; s < 4; s++)
				{
					bool const idch = ringid[s] != m_anl_prev_ringid[s];
					bool const hch  = ringh[s] != m_anl_prev_ringh[s];
					if (!idch && !hch)
						continue;
					u32 const id = ringid[s]; // post-transaction id (== pre for kills/revives)
					unsigned const m0 = (anchor[0] == id) ? 1 : 0;
					unsigned const m1 = (anchor[1] == id) ? 1 : 0;
					if (!idch && m_anl_prev_ringh[s] >= 0 && ringh[s] < 0)
					{
						++m_o20_kills;
						logerror("OP20: ring-kill frame=%u t=%.6f slot=%d id=%08x h %d->%d matchrec0=%u matchrec1=%u walk0=%u walk1=%u endcur0=%d endcur1=%d anchor0=%08x anchor1=%08x p370_0=%08x p370_1=%08x kills_total=%u (release marked this slot dead - an adjacent device 'OP20: rx-release' by t= NAMES it an INGESTED kill, none = local release/cull)\n",
								frame_count, now, s, id, m_anl_prev_ringh[s], ringh[s],
								m0, m1, (~f5c[0]) & 1, (~f5c[1]) & 1, endcur[0], endcur[1],
								anchor[0], anchor[1], p370[0], p370[1], m_o20_kills);
					}
					else if (idch && ringh[s] >= 0)
					{
						++m_o20_regs;
						logerror("OP20: ring-reg frame=%u t=%.6f slot=%d id %08x->%08x h %d->%d matchrec0=%u matchrec1=%u regs_total=%u (registration reused this slot for a fresh anchor)\n",
								frame_count, now, s, m_anl_prev_ringid[s], ringid[s],
								m_anl_prev_ringh[s], ringh[s], m0, m1, m_o20_regs);
					}
					else if (!idch && m_anl_prev_ringh[s] < 0 && ringh[s] >= 0)
					{
						++m_o20_revives;
						logerror("OP20: ring-revive frame=%u t=%.6f slot=%d id=%08x h %d->%d revives_total=%u (FALSIFIER: same-id dead->live without re-registration has NO stock path and shield repairs update the sampler basis - investigate if seen)\n",
								frame_count, now, s, id, m_anl_prev_ringh[s], ringh[s], m_o20_revives);
					}
					else
					{
						++m_o20_rewrites;
						logerror("OP20: ring-rewrite frame=%u t=%.6f slot=%d id %08x->%08x h %d->%d rewrites_total=%u (dead-slot rewrite / collapsed multi-transaction)\n",
								frame_count, now, s, m_anl_prev_ringid[s], ringid[s],
								m_anl_prev_ringh[s], ringh[s], m_o20_rewrites);
					}
				}

				// Commit-edge detector: rec+0x35C (desc) or rec+0x364 (anchor)
				// changed = a descriptor commit landed during the last frame.
				// Classify its anchor's FIRST post-commit ring state.  (A kill
				// landing in the same frame as the commit is counted born-dead
				// too - the device rx-release lines disambiguate by t=.)
				for (int r = 0; r < 2; r++)
				{
					if (desc[r] == m_anl_prev_desc[r] && anchor[r] == m_anl_prev_anchor[r])
						continue;
					if (anchor[r] == 0xffffffff)
						continue; // walk-only segment - no anchor, no ring interaction
					int const s = ring_find(anchor[r]);
					if (s >= 0 && ringh[s] < 0)
					{
						++m_o20_borndead;
						logerror("OP20: born-dead-commit frame=%u t=%.6f rec=%d anchor=%08x slot=%d h=%d desc35c %08x->%08x p370=%08x f5c=%08x borndead_total=%u (commit find-or-create hit a stale DEAD slot and did not resurrect - the 0x800B5710 edge, the cascade class; with ANCHOR_RESURRECT armed this MUST collapse toward 0)\n",
								frame_count, now, r, anchor[r], s, ringh[s],
								m_anl_prev_desc[r], desc[r], p370[r], f5c[r], m_o20_borndead);
					}
					else if (s < 0)
					{
						++m_o20_unreg;
						logerror("OP20: commit-unregistered frame=%u t=%.6f rec=%d anchor=%08x desc35c %08x->%08x ring[%08x:%d %08x:%d %08x:%d %08x:%d] unreg_total=%u (anchor absent from all 4 slots at first post-commit sample - ring-full silent drop / create-fail = beta fuel, or a same-frame scrub+commit collapse)\n",
								frame_count, now, r, anchor[r], m_anl_prev_desc[r], desc[r],
								ringid[0], ringh[0], ringid[1], ringh[1],
								ringid[2], ringh[2], ringid[3], ringh[3], m_o20_unreg);
					}
					else
					{
						++m_o20_healthy; // ring-live at first sample - counted, no line (the common case)
					}
				}
			}

			// ---- gate 2: the walk-phase ANCHOR SHIELD ----
			if (m_patch_anchor_shield)
			{
				for (int r = 0; r < 2; r++)
				{
					bool const own  = !(p370[r] & 0x40000000); // bit30 set = mirror, never self-completes
					bool const walk = !(f5c[r] & 1);           // bit0 set = wave active (fight phase)
					if (!own || !walk || anchor[r] == 0xffffffff)
					{
						// Disengaged (mirror record, activation reached, or a
						// no-anchor segment).  Summarize an eventful cycle once.
						if (m_ash_repairs_cycle[r] != 0 || m_ash_yielded[r] || m_ash_missed_logged[r])
							logerror("ANCHOR_SHIELD: cycle-end frame=%u t=%.6f rec=%d anchor=%08x repairs=%u yielded=%u missed=%u f5c=%08x p370=%08x bit16=%u (shield disengaged - bit16=0 here means the walk survived to activation)\n",
									frame_count, now, r, m_ash_live_id[r],
									m_ash_repairs_cycle[r], m_ash_yielded[r] ? 1u : 0u,
									m_ash_missed_logged[r] ? 1u : 0u, f5c[r], p370[r],
									(p370[r] >> 16) & 1);
						m_ash_live_handle[r] = -1;
						m_ash_live_id[r] = 0xffffffff;
						m_ash_repairs_cycle[r] = 0;
						m_ash_yielded[r] = false;
						m_ash_missed_logged[r] = false;
						continue;
					}
					// New walk cycle (fresh commit rewrote the anchor id).
					if (m_ash_live_id[r] != anchor[r])
					{
						m_ash_live_id[r] = anchor[r];
						m_ash_live_handle[r] = -1;
						m_ash_repairs_cycle[r] = 0;
						m_ash_yielded[r] = false;
						m_ash_missed_logged[r] = false;
					}
					int const s = ring_find(anchor[r]);
					if (s >= 0 && ringh[s] >= 0)
					{
						m_ash_live_handle[r] = ringh[s]; // capture/refresh the live handle
					}
					else if (m_ash_live_handle[r] >= 0 && !m_ash_yielded[r])
					{
						// The anchor WAS live this walk and is now dead (or its
						// slot got rewritten).  Restore the ring view - but only
						// into a slot that still carries OUR id (never clobber a
						// slot re-registered to another anchor).
						if (s >= 0 && ringh[s] < 0)
						{
							if (m_ash_repairs_cycle[r] < 8)
							{
								u32 const widx = (0x002D2034 + 8 * u32(s)) / 4;
								s32 const oldh = ringh[s];
								m_mainram[widx] = (u32(u16(m_ash_live_handle[r])) << 16) | (m_mainram[widx] & 0x0000ffff);
								ringh[s] = m_ash_live_handle[r]; // keep the local view post-write consistent
								++m_ash_repairs_cycle[r];
								++m_ash_repairs;
								logerror("ANCHOR_SHIELD: repair frame=%u t=%.6f rec=%d anchor=%08x slot=%d h %d->%d repairs_cycle=%u repairs_total=%u endcur=%d f5c=%08x p370=%08x (walk-phase ring view restored - the completion check can no longer empty-complete this segment; the entity's release mark is NOT undone: wave-activation outcome is run-1's measurement)\n",
										frame_count, now, r, anchor[r], s, oldh,
										m_ash_live_handle[r], m_ash_repairs_cycle[r], m_ash_repairs,
										endcur[r], f5c[r], p370[r]);
							}
							else
							{
								m_ash_yielded[r] = true;
								++m_ash_yields;
								logerror("ANCHOR_SHIELD: yield frame=%u t=%.6f rec=%d anchor=%08x slot=%d after=%u repairs (persistent ROM re-kill - P025 log-and-yield, shield stops fighting this cycle)\n",
										frame_count, now, r, anchor[r], s, m_ash_repairs_cycle[r]);
							}
						}
						else if (!m_ash_missed_logged[r])
						{
							// Slot no longer carries our id at all (recycled to
							// another anchor / scrubbed) - restoring would clobber
							// foreign state; log once and stand down this cycle.
							m_ash_missed_logged[r] = true;
							++m_ash_missed;
							logerror("ANCHOR_SHIELD: missed frame=%u t=%.6f rec=%d anchor=%08x lookup=-2 hadlive=%d (anchor's slot recycled/scrubbed after its death - cannot restore without clobbering; ANCHOR_RESURRECT fixes the NEXT commit)\n",
									frame_count, now, r, anchor[r], m_ash_live_handle[r]);
						}
					}
					else if (m_ash_live_handle[r] < 0 && !m_ash_missed_logged[r] && (s < 0 || ringh[s] < 0))
					{
						// Dead or absent with NO live capture this walk = the
						// anchor was BORN dead (class i) - nothing to restore.
						m_ash_missed_logged[r] = true;
						++m_ash_missed;
						logerror("ANCHOR_SHIELD: missed frame=%u t=%.6f rec=%d anchor=%08x lookup=%d hadlive=-1 (no live capture this walk = born-dead commit - shield cannot restore; ANCHOR_RESURRECT domain)\n",
								frame_count, now, r, anchor[r], (s >= 0) ? ringh[s] : -2);
					}
				}
			}

			// ---- gate 3: the ANCHOR RESURRECT dead-slot id scrub ----
			if (m_patch_anchor_resurrect)
			{
				for (int s = 0; s < 4; s++)
				{
					if (ringh[s] >= 0)
						continue; // GUARD: never touch a live handle
					if (ringid[s] == 0xffffffff || ringid[s] == 0)
						continue; // already scrubbed / ROM init state - nothing to do
					u32 const oldid = ringid[s];
					m_mainram[(0x002D2030 + 8 * s) / 4] = 0xffffffff;
					ringid[s] = 0xffffffff; // keep the local view post-write consistent
					++m_ars_scrubs;
					logerror("ANCHOR_RESURRECT: scrub frame=%u t=%.6f slot=%d id=%08x h=%d matchrec0=%u matchrec1=%u scrubs_total=%u (dead slot id blanked - the next find-or-create of this id MISSES and creates+registers a FRESH entity; verdict-neutral for the completion check: -2 and -1 are both negative)\n",
							frame_count, now, s, oldid, ringh[s],
							(anchor[0] == oldid) ? 1u : 0u, (anchor[1] == oldid) ? 1u : 0u,
							m_ars_scrubs);
				}
			}

			// ---- prevs = POST-write state; 1/s status rails ----
			for (int s = 0; s < 4; s++)
			{
				m_anl_prev_ringid[s] = ringid[s];
				m_anl_prev_ringh[s] = ringh[s];
			}
			for (int r = 0; r < 2; r++)
			{
				m_anl_prev_anchor[r] = anchor[r];
				m_anl_prev_desc[r] = desc[r];
			}
			m_anl_have_prev = true;

			if (frame_count % 60 == 0)
			{
				if (m_trace_op20)
					logerror("OP20: status frame=%u t=%.6f kills=%u regs=%u revives=%u rewrites=%u borndead=%u unreg=%u healthy=%u ring[%08x:%d %08x:%d %08x:%d %08x:%d] cur=%u anchor0=%08x anchor1=%08x\n",
							frame_count, now, m_o20_kills, m_o20_regs, m_o20_revives,
							m_o20_rewrites, m_o20_borndead, m_o20_unreg, m_o20_healthy,
							ringid[0], ringh[0], ringid[1], ringh[1],
							ringid[2], ringh[2], ringid[3], ringh[3],
							ringcur, anchor[0], anchor[1]);
				if (m_patch_anchor_shield)
					logerror("ANCHOR_SHIELD: status frame=%u t=%.6f repairs=%u yields=%u missed=%u live0=%d id0=%08x live1=%d id1=%08x\n",
							frame_count, now, m_ash_repairs, m_ash_yields, m_ash_missed,
							m_ash_live_handle[0], m_ash_live_id[0],
							m_ash_live_handle[1], m_ash_live_id[1]);
				if (m_patch_anchor_resurrect)
					logerror("ANCHOR_RESURRECT: status frame=%u t=%.6f scrubs=%u ring[%08x:%d %08x:%d %08x:%d %08x:%d]\n",
							frame_count, now, m_ars_scrubs,
							ringid[0], ringh[0], ringid[1], ringh[1],
							ringid[2], ringh[2], ringid[3], ringh[3]);
			}
		}

		// P055 EXPERIMENT (branch patch/boat-jitter-trace): READ-ONLY per-vblank
		// BLUE entity-ingest source/freshness/reversal trace.  Full design + the
		// Phase-1 finding live in the member-block comment.  Runs ONLY on the
		// connector/blue while staging mode 0x802F3FD0==2; diffs the two ingested
		// actor-position triples per live slot and tags each REVERSAL (delta
		// sign-flip = the shake) RX/LOCAL + FRESH/REPLAY from the device bridge -
		// the A-vs-B discriminator.
		if (m_bj_trace && m_c139 && m_c139->comm_is_connector()
				&& m_mainram[0x002F3FD0 / 4] == 2)
		{
			double const bj_now = machine().time().as_double();
			u32 const cabid = m_mainram[0x002F3FD4 / 4];        // cab-id landmark (context)
			u32 const rxgen = m_c139->rx_apply_gen();           // device delivered-complete-frame generation
			bool const ingest_this_vbl = m_bj_have_prev && (rxgen != m_bj_prev_rxgen);
			bool const last_replay = m_c139->last_delivered_was_replay();
			char const *const src = ingest_this_vbl ? "RX" : "LOCAL";
			char const *const fresh = ingest_this_vbl
					? (last_replay ? " replay=1 fresh=0" : " replay=0 fresh=1") : "";

			// Component -> actor-record byte offset: 0..2 = field A (op0x21-0x33
			// coord triple), 3..5 = field B (op0x17-0x19 packed transform).
			static u32 const bj_off[6]  = { 0x1F4, 0x1F8, 0x1FC, 0xA8, 0xAC, 0xB0 };
			static char const bj_axis[6] = { 'X', 'Y', 'Z', 'X', 'Y', 'Z' };
			static char const *const bj_fld[6] = { "A", "A", "A", "B", "B", "B" };

			int lines = 0;
			u32 live_count = 0;
			for (int s = 0; s < BJ_SLOTS; s++)
			{
				u32 const base = 0x002F4DB0 + u32(s) * 0x2B8;
				s32 const status = s32(m_mainram[(base + 0x18) / 4]); // +0x18 validity guard (<0 = live)
				bj_slot &p = m_bj_prev[s];
				if (status >= 0)
				{
					p.live = false;
					continue;
				}
				++live_count;
				s32 cur[6], d[6];
				for (int c = 0; c < 6; c++)
				{
					cur[c] = s32(m_mainram[(base + bj_off[c]) / 4]);
					d[c] = p.live ? (cur[c] - p.v[c]) : 0;
				}
				// Reversal = current delta flips sign vs the previous delta, both
				// above the noise floor (the actual "snap backward then forward").
				if (p.live && lines < BJ_MAX_LINES_FRAME)
				{
					for (int c = 0; c < 6; c++)
					{
						bool const rev = (d[c] > BJ_MIN_MOVE && p.d[c] < -BJ_MIN_MOVE)
								|| (d[c] < -BJ_MIN_MOVE && p.d[c] > BJ_MIN_MOVE);
						if (!rev)
							continue;
						++m_bj_reversals;
						if (ingest_this_vbl) ++m_bj_rx_moves; else ++m_bj_local_moves;
						u32 const idword = m_mainram[(base + 0x1F0) / 4]; // (id<<8)|sub (op0x2A-0x33 spawns)
						u32 const typ    = m_mainram[(base + 0x1EC) / 4]; // record-type tag
						logerror("BOATJITTER: reversal frame=%u t=%.6f slot=%d id=%04x typ=%08x fld=%s%03x comp=%c prev=%d cur=%d dprev=%d dcur=%d src=%s%s rxgen=%u revs=%u\n",
								frame_count, bj_now, s, unsigned((idword >> 8) & 0xffff), typ,
								bj_fld[c], bj_off[c], bj_axis[c], p.v[c], cur[c], p.d[c], d[c],
								src, fresh, rxgen, m_bj_reversals);
						if (++lines >= BJ_MAX_LINES_FRAME)
							break;
					}
				}
				for (int c = 0; c < 6; c++)
				{
					p.v[c] = cur[c];
					p.d[c] = d[c];
				}
				p.live = true;
			}

			m_bj_prev_rxgen = rxgen;
			m_bj_have_prev = true;

			if (frame_count % 60 == 0)
				logerror("BOATJITTER: status frame=%u t=%.6f live=%u reversals=%u rx_moves=%u local_moves=%u fresh=%u replay=%u rxgen=%u last_ingest=%s cabid=%08x\n",
						frame_count, bj_now, live_count, m_bj_reversals, m_bj_rx_moves,
						m_bj_local_moves, m_c139->rx_fresh_count(), m_c139->rx_replay_count(),
						rxgen, ingest_this_vbl ? (last_replay ? "REPLAY" : "FRESH") : "none", cabid);
		}

		// P056 EXPERIMENT (branch patch/boat-render-trace): READ-ONLY per-vblank
		// BLUE RENDERED-position + liveness trace.  Full design lives in the
		// member-block comment.  Watches the rendered world pos +0x30/34/38 (the
		// visible shake) on ACTIVE actors (live AND real id) and logs a REVERSAL on
		// any component's delta sign-flip, plus a liveness line on any real-actor
		// live-flag transition, each tagged RX/LOCAL + FRESH/REPLAY from the P055
		// device bridge.  Connector/blue + staging-mode-2 gated (arm on blue).
		if (m_br_trace && m_c139 && m_c139->comm_is_connector()
				&& m_mainram[0x002F3FD0 / 4] == 2)
		{
			double const br_now = machine().time().as_double();
			u32 const cabid = m_mainram[0x002F3FD4 / 4];        // cab-id landmark (context)
			u32 const rxgen = m_c139->rx_apply_gen();           // device delivered-complete-frame generation
			bool const ingest_this_vbl = m_br_have_prev && (rxgen != m_br_prev_rxgen);
			bool const last_replay = m_c139->last_delivered_was_replay();
			char const *const src = ingest_this_vbl ? "RX" : "LOCAL";
			char const *const fresh = ingest_this_vbl
					? (last_replay ? " replay=1 fresh=0" : " replay=0 fresh=1") : "";

			static u32 const br_off[3]  = { 0x30, 0x34, 0x38 }; // rendered world pos X/Y/Z (renderer 0x80089C78)
			static char const br_axis[3] = { 'X', 'Y', 'Z' };

			int lines = 0;
			u32 live_count = 0;     // slots with +0x18 < 0
			u32 active_count = 0;   // live AND real id (the id-filter's effect is visible in the rail)
			for (int s = 0; s < BR_SLOTS; s++)
			{
				u32 const base = 0x002F4DB0 + u32(s) * 0x2B8;
				s32 const status = s32(m_mainram[(base + 0x18) / 4]); // +0x18 validity guard (<0 = live)
				u32 const idword = m_mainram[(base + 0x1F0) / 4];     // (id<<8)|sub
				u32 const id     = unsigned((idword >> 8) & 0xffff);
				u32 const typ    = m_mainram[(base + 0x1EC) / 4];     // record-type tag
				bool const live  = status < 0;
				bool const real  = live && id != 0x0000 && id != 0xffff; // skip empty/placeholder slots
				br_slot &p = m_br_prev[s];

				bool const prev_live = p.live;
				bool const prev_real = p.live && p.id != 0x0000 && p.id != 0xffff;
				u32 const prev_id = p.id;

				if (live) ++live_count;
				if (real) ++active_count;

				// PART 2 - liveness transition (any real actor entering/leaving live).
				// Gated on m_br_have_prev so already-live actors at mode-2 entry don't
				// spuriously log.  id reported = the real side (prev_id when going dead).
				if (m_br_have_prev && live != prev_live && (real || prev_real)
						&& lines < BR_MAX_LINES_FRAME)
				{
					++m_br_live_transitions;
					logerror("BOATRENDER: live frame=%u t=%.6f slot=%d id=%04x typ=%08x old_live=%d new_live=%d src=%s%s rxgen=%u trans=%u\n",
							frame_count, br_now, s,
							unsigned(real ? id : prev_id), typ,
							prev_live ? 1 : 0, live ? 1 : 0, src, fresh, rxgen,
							m_br_live_transitions);
					++lines;
				}

				// PART 1 - rendered-position reversal (ACTIVE actors only).  Position
				// basis (p.v/p.d/p.sampled) is updated even when the log cap is hit, so
				// a busy vblank never corrupts the next frame's delta.
				if (real)
				{
					s32 cur[3], d[3];
					for (int c = 0; c < 3; c++)
					{
						cur[c] = s32(m_mainram[(base + br_off[c]) / 4]);
						d[c] = p.sampled ? (cur[c] - p.v[c]) : 0;
					}
					if (p.sampled && lines < BR_MAX_LINES_FRAME)
					{
						for (int c = 0; c < 3; c++)
						{
							bool const rev = (d[c] > BR_MIN_MOVE && p.d[c] < -BR_MIN_MOVE)
									|| (d[c] < -BR_MIN_MOVE && p.d[c] > BR_MIN_MOVE);
							if (!rev)
								continue;
							++m_br_reversals;
							if (ingest_this_vbl) ++m_br_rx_moves; else ++m_br_local_moves;
							logerror("BOATRENDER: reversal frame=%u t=%.6f slot=%d id=%04x typ=%08x comp=%c prev=%d cur=%d dprev=%d dcur=%d src=%s%s rxgen=%u revs=%u\n",
									frame_count, br_now, s, id, typ, br_axis[c],
									p.v[c], cur[c], p.d[c], d[c],
									src, fresh, rxgen, m_br_reversals);
							if (++lines >= BR_MAX_LINES_FRAME)
								break;
						}
					}
					for (int c = 0; c < 3; c++)
					{
						p.v[c] = cur[c];
						p.d[c] = d[c];
					}
					p.sampled = true;
				}
				else
				{
					p.sampled = false; // break the delta basis across a dead/empty gap
				}

				p.live = live;
				p.id = id;
			}

			m_br_prev_rxgen = rxgen;
			m_br_have_prev = true;

			if (frame_count % 60 == 0)
				logerror("BOATRENDER: status frame=%u t=%.6f active=%u live=%u reversals=%u rx_moves=%u local_moves=%u live_trans=%u fresh=%u replay=%u rxgen=%u last_ingest=%s cabid=%08x\n",
						frame_count, br_now, active_count, live_count, m_br_reversals,
						m_br_rx_moves, m_br_local_moves, m_br_live_transitions,
						m_c139->rx_fresh_count(), m_c139->rx_replay_count(),
						rxgen, ingest_this_vbl ? (last_replay ? "REPLAY" : "FRESH") : "none", cabid);
		}

		// P059 EXPERIMENT (branch patch/poscorr-trace): READ-ONLY per-vblank BLUE
		// correction-cadence + local-drift trace on the remote actor's world-pos
		// BASE +0xCC (type-1) / +0x80 (type-2).  Full design + the write-tap-vs-
		// sampling decision live in the member-block comment.  Runs ONLY on the
		// connector/blue while staging mode 0x802F3FD0==2.  A base move in a vblank
		// where the device rx-apply generation advanced (an op4B/4C snapshot landed)
		// is an INGEST CORRECTION; a base move with no ingest this window is LOCAL
		// DRIFT.  Reports: corrections/sec + the inter-correction gap histogram, and
		// the local drift magnitude accumulated on +0xCC between corrections - the
		// two numbers the FIX FAMILY B verdict needs (correction rate + drift owed).
		if (m_pc_trace && m_c139 && m_c139->comm_is_connector()
				&& m_mainram[0x002F3FD0 / 4] == 2)
		{
			double const pc_now = machine().time().as_double();
			u32 const cabid = m_mainram[0x002F3FD4 / 4];        // cab-id landmark (context)
			u32 const rxgen = m_c139->rx_apply_gen();           // device delivered-complete-frame generation
			bool const ingest_this_vbl = m_pc_pos_have_prev && (rxgen != m_pc_prev_rxgen);
			bool const last_replay = m_c139->last_delivered_was_replay();
			char const *const fresh = ingest_this_vbl ? (last_replay ? "replay" : "fresh") : "none";

			int lines = 0;
			u32 remote_live = 0;
			for (int s = 0; s < PC_SLOTS; s++)
			{
				u32 const base = 0x002F4DB0 + u32(s) * 0x2B8;
				s32 const status = s32(m_mainram[(base + 0x18) / 4]); // +0x18 bit31 set (<0) = remote/live
				pc_slot &p = m_pc_prev[s];
				if (status >= 0)
				{
					// not a remote actor this frame - break the delta basis and drop
					// the gap/drift state so a later re-use of the slot never carries
					// a bogus inter-correction gap across the dead gap.
					p.sampled = false;
					p.corr_seen = false;
					p.drift_accum = 0.f;
					p.drift_max = 0.f;
					p.anim_sampled = false;   // P061 RIDER: anim/kf basis breaks with the slot
					continue;
				}
				++remote_live;

				// Read BOTH world-pos bases as IEEE floats (the transform tick
				// 0x80100014 composes +0xCC and 0x8010C5B0 composes +0x80 via the
				// FPU).  Track both and attribute a move to whichever base moved
				// more - avoids a wrong type-1/type-2 decode killing the measurement.
				float cc[3], w80[3];
				for (int c = 0; c < 3; c++)
				{
					u32 const wc = m_mainram[(base + 0xCC + u32(c) * 4) / 4];
					u32 const w8 = m_mainram[(base + 0x80 + u32(c) * 4) / 4];
					std::memcpy(&cc[c], &wc, 4);
					std::memcpy(&w80[c], &w8, 4);
				}

				// P061 RIDER: did either base move on this slot this vblank?
				// (set below; the anim-layer discriminator keys on its negation)
				bool base_moved = false;

				if (p.sampled)
				{
					// Largest finite per-component delta on each base.
					float mag_cc = 0.f, mag_80 = 0.f;
					int comp_cc = -1, comp_80 = -1;
					for (int c = 0; c < 3; c++)
					{
						float const dcc = cc[c] - p.cc[c];
						if (std::isfinite(dcc) && std::fabs(dcc) < PC_SANE_MAX
								&& std::fabs(dcc) > mag_cc) { mag_cc = std::fabs(dcc); comp_cc = c; }
						float const d80 = w80[c] - p.w80[c];
						if (std::isfinite(d80) && std::fabs(d80) < PC_SANE_MAX
								&& std::fabs(d80) > mag_80) { mag_80 = std::fabs(d80); comp_80 = c; }
					}
					bool const use_cc = mag_cc >= mag_80;
					float const mag  = use_cc ? mag_cc : mag_80;
					int const comp   = use_cc ? comp_cc : comp_80;
					float const prevv = use_cc ? p.cc[comp >= 0 ? comp : 0] : p.w80[comp >= 0 ? comp : 0];
					float const curv  = use_cc ? cc[comp >= 0 ? comp : 0] : w80[comp >= 0 ? comp : 0];

					if (comp >= 0 && mag > PC_MIN_MOVE)
					{
						base_moved = true;   // P061 RIDER
						if (ingest_this_vbl)
						{
							// INGEST CORRECTION: the base snapped in a vblank an
							// op4B/4C snapshot was delivered.  Log the gap since the
							// last correction on THIS slot + the drift that had built
							// up (how far local prediction wandered before the snap).
							u32 const gap = p.corr_seen ? (frame_count - p.last_corr_frame) : 0;
							if (p.corr_seen)
							{
								int b = 0;
								if (gap <= 1) b = 0; else if (gap == 2) b = 1;
								else if (gap <= 4) b = 2; else if (gap <= 8) b = 3;
								else if (gap <= 16) b = 4; else b = 5;
								++m_pc_gap_hist[b];
							}
							++m_pc_corr_win;
							++m_pc_corr_tot;
							if (last_replay) ++m_pc_corr_replay_win; else ++m_pc_corr_fresh_win;
							m_pc_snap_sum += mag;
							if (lines < PC_MAX_LINES_FRAME)
							{
								logerror("POSCORR: corr frame=%u t=%.6f slot=%d base=+0x%s comp=%c snap=%.3f gap=%u drift_pre=%.3f src=%s prev=%.3f cur=%.3f raw=%08x rxgen=%u corr=%u\n",
										frame_count, pc_now, s, use_cc ? "CC" : "80",
										"XYZ"[comp], mag, gap, p.drift_accum, fresh,
										prevv, curv,
										unsigned(m_mainram[(base + (use_cc ? 0xCC : 0x80) + u32(comp) * 4) / 4]),
										rxgen, m_pc_corr_tot);
								++lines;
							}
							p.drift_accum = 0.f;
							p.drift_max = 0.f;
							p.last_corr_frame = frame_count;
							p.corr_seen = true;
						}
						else
						{
							// LOCAL DRIFT: base moved with no ingest this window =
							// blue's local transform tick re-deriving the base from
							// stale inputs.  Accumulate the excursion (no per-event
							// line - the aggregate goes in the 1/s rail).
							p.drift_accum += mag;
							if (p.drift_accum > p.drift_max) p.drift_max = p.drift_accum;
							if (p.drift_max > m_pc_drift_max_seen) m_pc_drift_max_seen = p.drift_max;
							++m_pc_local_events_win;
							m_pc_local_sum_win += mag;
						}
					}
				}

				// P061 RIDER (audit D): anim-layer discriminator.  Diff the anim/
				// orientation matrix word +0x3C (raw) and the keyframe counter
				// +0x1A (low halfword of the +0x18 status word already read above
				// - big-endian layout) against last vblank.  A change in a vblank
				// where BOTH world bases stayed static (base_moved false) is
				// pose-layer motion with the base pinned - the still-shudder
				// candidate the +0xCC arc never measured.  READ-ONLY; counts go
				// to the 1/s digest; per-event lines capped PC_ANIM_MAX_LINES_WIN
				// per window.
				u32 const anim_w = m_mainram[(base + 0x3C) / 4];
				u16 const kf_w = u16(u32(status) & 0xffff);
				if (p.anim_sampled)
				{
					bool const anim_chg = (anim_w != p.anim);
					bool const kf_chg = (kf_w != p.kf);
					if (anim_chg) ++m_pc_anim_chg_win;
					if (kf_chg) ++m_pc_kf_chg_win;
					if ((anim_chg || kf_chg) && p.sampled && !base_moved)
					{
						++m_pc_anim_ccstatic_win;
						if (m_pc_anim_lines_win < PC_ANIM_MAX_LINES_WIN)
						{
							++m_pc_anim_lines_win;
							logerror("POSCORR: anim-while-static frame=%u t=%.6f slot=%d anim=%08x->%08x kf=%04x->%04x src=%s (+0x3C anim matrix / +0x1A keyframe advanced in a vblank the +0xCC/+0x80 base was STATIC - pose-layer motion candidate for the still-shudder)\n",
									frame_count, pc_now, s, p.anim, anim_w,
									unsigned(p.kf), unsigned(kf_w), fresh);
						}
					}
				}
				p.anim = anim_w;
				p.kf = kf_w;
				p.anim_sampled = true;

				for (int c = 0; c < 3; c++) { p.cc[c] = cc[c]; p.w80[c] = w80[c]; }
				p.sampled = true;
			}

			m_pc_prev_rxgen = rxgen;
			m_pc_pos_have_prev = true;
			m_pc_remote_live_last = remote_live;

			if (frame_count % 60 == 0)
			{
				float const mean_snap = m_pc_corr_tot ? (m_pc_snap_sum / float(m_pc_corr_tot)) : 0.f;
				logerror("POSCORR: rom-status frame=%u t=%.6f remote_live=%u corr_win=%u corr_fresh=%u corr_replay=%u corr_tot=%u mean_snap=%.3f local_events=%u local_sum=%.3f drift_max=%.3f gaps[1=%u 2=%u 3-4=%u 5-8=%u 9-16=%u 17+=%u] dev_fresh=%u dev_replay=%u rxgen=%u cabid=%08x\n",
						frame_count, pc_now, m_pc_remote_live_last, m_pc_corr_win,
						m_pc_corr_fresh_win, m_pc_corr_replay_win, m_pc_corr_tot,
						mean_snap, m_pc_local_events_win, m_pc_local_sum_win,
						m_pc_drift_max_seen,
						m_pc_gap_hist[0], m_pc_gap_hist[1], m_pc_gap_hist[2],
						m_pc_gap_hist[3], m_pc_gap_hist[4], m_pc_gap_hist[5],
						m_c139->rx_fresh_count(), m_c139->rx_replay_count(),
						rxgen, cabid);
				m_pc_corr_win = 0;
				m_pc_corr_fresh_win = 0;
				m_pc_corr_replay_win = 0;
				m_pc_local_events_win = 0;
				m_pc_local_sum_win = 0.f;

				// P061 RIDER: 1/s anim-layer digest.  Read alongside the
				// rom-status line above: while_static>0 with corr_win/
				// local_events quiet during a user-called still-shudder window
				// = pose-layer motion CONFIRMED with the base pinned.
				logerror("POSCORR: anim-status frame=%u t=%.6f anim_chg=%u kf_chg=%u while_static=%u remote_live=%u (slot-vblanks this window where +0x3C anim matrix / +0x1A keyframe changed; while_static = changed while BOTH world bases were static that vblank - the pose-layer still-shudder discriminator)\n",
						frame_count, pc_now, m_pc_anim_chg_win, m_pc_kf_chg_win,
						m_pc_anim_ccstatic_win, m_pc_remote_live_last);
				m_pc_anim_chg_win = 0;
				m_pc_kf_chg_win = 0;
				m_pc_anim_ccstatic_win = 0;
				m_pc_anim_lines_win = 0;
			}
		}

		// P065 EXPERIMENT (branch patch/corr-smooth): VIEWER-SIDE CORRECTION
		// BLENDING on remote actors' world-pos base (s32 integer triple - the
		// phase-1 format verification and the full design live in the member-
		// block comment).  Runs on BOTH cabs (each cab is the remote-viewer of
		// the peer's entities) while staging mode 0x802F3FD0 == 2.  Placed
		// AFTER the P059 POSCORR scan so the trace samples the RAW ROM values
		// (its correction counts stay comparable to P059-P064); after each
		// blend write we refresh POSCORR's per-slot cache so it never
		// attributes our own steps as ROM movement.
		if (m_cs_on && m_c139)
		{
			bool const cs_mode2 = (m_mainram[0x002F3FD0 / 4] == 2);
			if (!cs_mode2)
			{
				// staging-mode-2 loss: fail toward stock - drop ALL blend and
				// tracking state so nothing stale carries across a scene/mode
				// transition (any in-flight remainder simply never lands; the
				// ROM's own values stand untouched).
				if (m_cs_mode2_prev)
				{
					for (int s = 0; s < CS_SLOTS; s++)
					{
						m_cs_slot[s].active = false;
						m_cs_slot[s].frames_left = 0;
					}
					m_cs_have_prev = false;
				}
				m_cs_mode2_prev = false;
			}
			else
			{
				u32 const rxgen = m_c139->rx_apply_gen();   // delivered-complete-frame generation (device bridge armed by this env too)
				bool const ingest_this_vbl = m_cs_have_prev && (rxgen != m_cs_prev_rxgen);
				u32 active_blends = 0;

				for (int s = 0; s < CS_SLOTS; s++)
				{
					u32 const base = 0x002F4DB0 + u32(s) * 0x2B8;
					cs_slot &p = m_cs_slot[s];

					// Gate exactly like the renderer's own dispatch (0x80089CD4-
					// 0x80089D68): REMOTE (+0x18 bit31 set), valid (+0x08
					// halfword bit 0x8000 clear = word bit31 of the big-endian
					// +0x08 word), record type 1 or 2 (+0x0A halfword = low
					// half) - the type selects WHICH base triple the renderer
					// adds (+0xCC type-1 / +0x80 type-2).
					s32 const w18 = s32(m_mainram[(base + 0x18) / 4]);
					u32 const w08 = m_mainram[(base + 0x08) / 4];
					u16 const rtype = u16(w08 & 0xffff);
					if (w18 >= 0 || (w08 & 0x80000000) != 0 || (rtype != 1 && rtype != 2))
					{
						// not a blendable remote actor - stock behavior.
						if (p.active && p.frames_left)
							{ ++m_cs_snapthru_win; ++m_cs_snapthru_tot; }
						p.active = false;
						p.frames_left = 0;
						continue;
					}

					u32 const off = (rtype == 1) ? 0xCC : 0x80;
					u16 const rid = u16(m_mainram[(base + 0x98) / 4] & 0xffff);  // +0x9A actor id (the op4B/4C snapshot lookup key)
					s32 cur[3];
					bool fmt_bad = false;
					for (int c = 0; c < 3; c++)
					{
						cur[c] = s32(m_mainram[(base + off + u32(c) * 4) / 4]);
						if (cur[c] > CS_SANE_ABS || cur[c] < -CS_SANE_ABS)
							fmt_bad = true;
					}
					if (fmt_bad)
					{
						// format rail (the task's fallback): not a plausible
						// world coordinate - PASS THROUGH unsmoothed.
						++m_cs_fmtrej_win; ++m_cs_fmtrej_tot;
						if (p.active && p.frames_left)
							{ ++m_cs_snapthru_win; ++m_cs_snapthru_tot; }
						p.active = false;
						p.frames_left = 0;
						continue;
					}

					if (!p.active || p.type != u8(rtype) || p.id != rid)
					{
						// first sight or slot REUSE (id/type changed) - stock
						// snap behavior; (re)acquire passive tracking only.
						if (p.active && p.frames_left)
							{ ++m_cs_snapthru_win; ++m_cs_snapthru_tot; }
						p.active = true;
						p.type = u8(rtype);
						p.id = rid;
						p.frames_left = 0;
						for (int c = 0; c < 3; c++) { p.seen[c] = cur[c]; p.rem[c] = 0; }
						continue;
					}

					// DETECT a correction: the device rx-apply gen advanced
					// this vblank AND the watched base moved beyond the epsilon
					// (word-compare first, then s32 decode - the P059-P064
					// detection).  rxgen is GLOBAL not per-slot (documented
					// P055 limitation), so a local-writer move in an ingest
					// vblank can also start a blend - bounded harm: the move is
					// re-released over N frames, and MIND is the tuning knob if
					// that ever reads as motion lag.
					if (ingest_this_vbl
							&& (cur[0] != p.seen[0] || cur[1] != p.seen[1] || cur[2] != p.seen[2]))
					{
						s64 d[3];
						s64 mag = 0;
						for (int c = 0; c < 3; c++)
						{
							d[c] = s64(cur[c]) - s64(p.seen[c]);
							s64 const a = (d[c] < 0) ? -d[c] : d[c];
							if (a > mag) mag = a;
						}
						if (mag >= s64(m_cs_mind))
						{
							if (mag > s64(m_cs_maxd))
							{
								// TELEPORT / respawn / scene change: DO NOT
								// blend - the snap lands instantly (stock).
								++m_cs_snapthru_win; ++m_cs_snapthru_tot;
								p.frames_left = 0;
								for (int c = 0; c < 3; c++) { p.seen[c] = cur[c]; p.rem[c] = 0; }
								continue;
							}
							// start / RETARGET the blend from the current
							// displayed value: the new remainder is (ROM's new
							// word - displayed); any old remainder is subsumed
							// because the ROM's word already embodies the full
							// authoritative position.
							for (int c = 0; c < 3; c++)
								p.rem[c] = s32(d[c]);
							p.frames_left = m_cs_frames;
							++m_cs_blends_win; ++m_cs_blends_tot;
						}
					}

					if (p.frames_left > 0)
					{
						// release one step: base = cur - rem + rem/frames_left.
						// Computed RELATIVE to the freshly-read word, so a
						// local-writer advance (or full re-derivation) between
						// our writes is preserved, never forced back ("re-
						// detect rather than force"); the final frame applies
						// the exact remainder (rem/1) so the blend converges
						// precisely on the ROM's current word.
						s32 step[3], neww[3];
						bool rail_bad = false;
						for (int c = 0; c < 3; c++)
						{
							step[c] = p.rem[c] / s32(p.frames_left);
							s64 const nw = s64(cur[c]) - s64(p.rem[c]) + s64(step[c]);
							if (nw > s64(CS_SANE_ABS) || nw < -s64(CS_SANE_ABS))
								rail_bad = true;
							neww[c] = s32(nw);
						}
						if (rail_bad)
						{
							// never write an implausible value - abort to stock.
							++m_cs_snapthru_win; ++m_cs_snapthru_tot;
							p.frames_left = 0;
							for (int c = 0; c < 3; c++) { p.seen[c] = cur[c]; p.rem[c] = 0; }
							continue;
						}
						for (int c = 0; c < 3; c++)
						{
							if (neww[c] != cur[c])
								m_mainram[(base + off + u32(c) * 4) / 4] = u32(neww[c]);
							p.rem[c] -= step[c];
							p.seen[c] = neww[c];
						}
						p.frames_left--;
						++m_cs_steps_win;
						if (p.frames_left > 0)
							++active_blends;

						// keep the P059 POSCORR trace blind to OUR writes:
						// refresh its per-slot cache (the float view of the
						// same words) so its next-vblank delta basis equals
						// RAM and its correction counts stay comparable.
						if (m_pc_trace && m_c139->comm_is_connector() && m_pc_prev[s].sampled)
						{
							for (int c = 0; c < 3; c++)
							{
								float f;
								u32 const w = u32(neww[c]);
								std::memcpy(&f, &w, 4);
								if (rtype == 1) m_pc_prev[s].cc[c] = f;
								else            m_pc_prev[s].w80[c] = f;
							}
						}
					}
					else
					{
						// no active blend: passive tracking.
						for (int c = 0; c < 3; c++) p.seen[c] = cur[c];
					}
				}

				m_cs_prev_rxgen = rxgen;
				m_cs_have_prev = true;
				m_cs_mode2_prev = true;

				if (frame_count % 60 == 0)
				{
					logerror("CORR_SMOOTH: status frame=%u t=%.6f blends=%u steps=%u snap_throughs=%u format_rejects=%u active_blends=%u tot[blends=%u snapthru=%u fmtrej=%u] N=%u maxd=%d mind=%d rxgen=%u\n",
							frame_count, machine().time().as_double(),
							m_cs_blends_win, m_cs_steps_win, m_cs_snapthru_win,
							m_cs_fmtrej_win, active_blends,
							m_cs_blends_tot, m_cs_snapthru_tot, m_cs_fmtrej_tot,
							unsigned(m_cs_frames), m_cs_maxd, m_cs_mind, rxgen);
					m_cs_blends_win = 0;
					m_cs_steps_win = 0;
					m_cs_snapthru_win = 0;
					m_cs_fmtrej_win = 0;
				}
			}
		}

		m_ctl_vbl_active = true;
		irq_update(m_main_irqcause | MAIN_VBLANK_IRQ);
	}
	else
	{
		irq_update(m_main_irqcause & ~MAIN_VBLANK_IRQ);
	}
	m_sub_portb = (m_sub_portb & 0x7f) | (state << 7);
	m_adc->adtrg_w(state);
}

void gorgon_state::vblank(int state)
{
	namcos23_state::vblank(state);
	subcpu_irq1_update(state);
}

TIMER_CALLBACK_MEMBER(namcos23_state::subcpu_scanline_on_tick)
{
	if (m_screen->vpos() < 72)
		subcpu_irq1_update(ASSERT_LINE);
}

TIMER_CALLBACK_MEMBER(namcos23_state::subcpu_scanline_off_tick)
{
	subcpu_irq1_update(CLEAR_LINE);
}


// C417

u16 namcos23_state::c417_status_r()
{
	/* According to timecrs2v4a, +0 is the status word with bits being:
	   15: test mode flag (huh?)
	   10: fifo data ready
	   9:  cmd ram data ready
	   8:  matrix busy
	   7:  output unit busy (inverted)
	   6:  hokan/tenso unit busy
	   5:  point unit busy
	   4:  access unit busy
	   3:  c403 busy, called c444 in 500gp (inverted)
	   2:  2nd c435 busy (inverted)
	   1:  1st c435 busy (inverted)
	   0:  xcpreq
	*/

	u16 data = 0x008e | (m_c417.test_mode << 15);
	LOGMASKED(LOG_C417_REG, "%s: c417_status_r: %04x\n", machine().describe_context(), data);
	return data;
}

u16 crszone_state::c417_status_r()
{
	u16 data = m_c417.test_mode << 15;
	LOGMASKED(LOG_C417_REG, "%s: c417_status_r: %04x\n", machine().describe_context(), data);
	return data;
}

u16 namcos23_state::c417_addr_r()
{
	LOGMASKED(LOG_C417_REG, "%s: c417_addr_r: %04x\n", machine().describe_context(), m_c417.adr);
	return m_c417.adr;
}

void namcos23_state::c417_addr_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_C417_REG, "%s: c417_addr_w: %04x & %04x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_c417.adr);
}

void namcos23_state::c417_ptrom_addr_w(offs_t offset, u16 data, u16 mem_mask)
{
	m_c417.pointrom_adr = (m_c417.pointrom_adr << 16) | data;
	LOGMASKED(LOG_C417_REG, "%s: c417_ptrom_addr_w: %04x & %04x (now %08x)\n", machine().describe_context(), data, mem_mask, m_c417.pointrom_adr);
}

u16 namcos23_state::c417_test_done_r()
{
	LOGMASKED(LOG_C417_REG, "%s: c417_test_done_r: %04x\n", machine().describe_context(), 0);
	m_c417.test_mode = false;
	return 0;
}

void namcos23_state::c417_ptrom_addr_clear_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_C417_REG, "%s: c417_ptrom_addr_clear_w: %04x & %04x\n", machine().describe_context(), data, mem_mask);
	m_c417.pointrom_adr = 0;
	m_c417.test_mode = true;
}

u16 namcos23_state::c417_ram_r()
{
	LOGMASKED(LOG_C417_REG, "%s: c417 ram[%04x] read: %04x\n", machine().describe_context(), m_c417.adr, m_c417.ram[m_c417.adr]);
	return m_c417.ram[m_c417.adr];
}

void namcos23_state::c417_ram_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (!m_c417.test_mode)
		LOGMASKED(LOG_C417_REG, "%s: c417_ram_w: %04x = %04x & %04x\n", machine().describe_context(), m_c417.adr, data, mem_mask);
	COMBINE_DATA(m_c417.ram + m_c417.adr);
}

u16 namcos23_state::c417_ptrom_msw_r()
{
	if (m_c417.pointrom_adr >= m_ptrom_limit)
	{
		LOGMASKED(LOG_C417_REG, "%s: c417 point rom (over-limit) msw read: %04x\n", machine().describe_context(), 0xffff);
		return 0xffff;
	}
	LOGMASKED(LOG_C417_REG, "%s: c417 point rom[%06x] msw read: %04x\n", machine().describe_context(), m_c417.pointrom_adr, m_ptrom[m_c417.pointrom_adr] >> 16);
	return m_ptrom[m_c417.pointrom_adr] >> 16;
}

u16 namcos23_state::c417_ptrom_lsw_r()
{
	if (m_c417.pointrom_adr >= m_ptrom_limit)
	{
		LOGMASKED(LOG_C417_REG, "%s: c417 point rom (over-limit) lsw read: %04x\n", machine().describe_context(), 0xffff);
		return 0xffff;
	}
	LOGMASKED(LOG_C417_REG, "%s: c417 point rom[%06x] lsw read: %04x\n", machine().describe_context(), m_c417.pointrom_adr, (u16)m_ptrom[m_c417.pointrom_adr]);
	if (machine().side_effects_disabled())
		return m_ptrom[m_c417.pointrom_adr];
	else
		return m_ptrom[m_c417.pointrom_adr++];
}

void namcos23_state::c417_irq_ack_w(offs_t offset, u16 data)
{
	LOGMASKED(LOG_C417_ACK, "%s: c417_w: ack IRQ 2 (%x)\n", machine().describe_context(), data);
	irq_update(m_main_irqcause & ~MAIN_C435_IRQ);
}


// C412

u32 crszone_state::c450_irq_status_r(offs_t offset, u32 mem_mask)
{
	return (m_main_irqcause & MAIN_C450_IRQ) ? 1 : 0;
}

void crszone_state::c450_dma_addr_w(address_space &space, offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: c450 write DMA address @%08x: %08x & %08x\n", machine().describe_context(), 0x10400000, data, mem_mask);
	COMBINE_DATA(&m_c435.address);
}

void crszone_state::c450_dma_size_w(address_space &space, offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_C435_REG, "%s: c450 write DMA size @%08x: %08x & %08x (kicking DMA from %08x)\n", machine().describe_context(), 0x1040000c, data, mem_mask, m_c435.address);
	COMBINE_DATA(&m_c435.size);
	c435_dma(space, m_c435.address, m_c435.size);
	irq_update(m_main_irqcause | MAIN_C450_IRQ);
}

// C412

// Offsets 0x00 and 0x02 are written to and read from, but it's not clear what they do.
// Simply returning the values written to them causes glitchy polygons instead of a background
// to appear in the gun-adjust screen of timecrs2v1b, as well as missing background graphics
// in some places in finfurl2/finfurl2j.

u16 namcos23_state::c412_flags_r() // offset 0x06
{
	LOGMASKED(LOG_C412_UNK, "%s: c412_flags_r: %04x\n", machine().describe_context(), 0x0002);
	return 0x0002; // 0001 = busy, 0002 = game uploads things
}

u16 namcos23_state::c412_addr_lsw_r() // offset 0x10
{
	LOGMASKED(LOG_C412_UNK, "%s: c412_addr_lsw_r: %04x\n", machine().describe_context(), (u16)m_c412.adr);
	return m_c412.adr;
}

u16 namcos23_state::c412_addr_msw_r() // offset 0x12
{
	LOGMASKED(LOG_C412_UNK, "%s: c412_addr_msw_r: %04x\n", machine().describe_context(), (u16)(m_c412.adr >> 16));
	return m_c412.adr >> 16;
}

u16 namcos23_state::c412_ram_r() // offset 0x14
{
	LOGMASKED(LOG_C412_UNK, "%s: c412_ram_r: %06x\n", machine().describe_context(), m_c412.adr);
	if (m_c412.adr < 0x100000)
		return m_c412.sdram_a[m_c412.adr & 0xfffff];
	else if (m_c412.adr < 0x200000)
		return m_c412.sdram_b[m_c412.adr & 0xfffff];
	else if (m_c412.adr < 0x220000)
		return m_c412.sram[m_c412.adr & 0x1ffff];
	else if (m_c412.adr < 0x220200)
		return m_c412.pczram[m_c412.adr & 0x1ff];

	return 0xffff;
}

u16 namcos23_state::c412_status_r() // offset 0x18
{
	// unknown status, 500gp reads it and waits for a transition
	// no other games use it?
	m_c412.status_c ^= 1;
	LOGMASKED(LOG_C412_UNK, "%s: c412_status_r: %06x\n", machine().describe_context(), m_c412.status_c);
	return m_c412.status_c;
}

void namcos23_state::c412_flags_w(offs_t offset, u16 data, u16 mem_mask) // offset 0x04
{
	// d0: cz on
	// other bits: no function?
	LOGMASKED(LOG_C412_UNK, "%s: c412 write %x = %04x & %04x\n", machine().describe_context(), offset, data, mem_mask);
}

void namcos23_state::c412_addr_lsw_w(offs_t offset, u16 data, u16 mem_mask) // offset 0x10
{
	m_c412.adr = (data & mem_mask) | (m_c412.adr & (0xffffffff ^ mem_mask));
}

void namcos23_state::c412_addr_msw_w(offs_t offset, u16 data, u16 mem_mask) // offset 0x12
{
	m_c412.adr = ((data & mem_mask) << 16) | (m_c412.adr & (0xffffffff ^ (mem_mask << 16)));
}

void namcos23_state::c412_ram_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (m_c412.adr < 0x100000)
	{
		//if (!m_c417.test_mode) LOGMASKED(LOG_C412_RAM, "C412 SDRAM A write: %08x = %04x & %04x\n", (m_c412.adr << 1), data, mem_mask);
		COMBINE_DATA(m_c412.sdram_a + (m_c412.adr & 0xfffff));
	}
	else if (m_c412.adr < 0x200000)
	{
		//if (!m_c417.test_mode) LOGMASKED(LOG_C412_RAM, "C412 SDRAM B write: %08x = %04x & %04x\n", (m_c412.adr << 1), data, mem_mask);
		COMBINE_DATA(m_c412.sdram_b + (m_c412.adr & 0xfffff));
	}
	else if (m_c412.adr < 0x220000)
	{
		if (!m_c417.test_mode) LOGMASKED(LOG_C412_RAM, "C412 SRAM write: %08x = %04x & %04x\n", (m_c412.adr << 1), data, mem_mask);
		COMBINE_DATA(m_c412.sram + (m_c412.adr & 0x1ffff));
	}
	else if (m_c412.adr < 0x220200)
	{
		if (!m_c417.test_mode) LOGMASKED(LOG_C412_RAM, "C412 PCZRAM write: %08x = %04x & %04x\n", (m_c412.adr << 1), data, mem_mask);
		COMBINE_DATA(m_c412.pczram + (m_c412.adr & 0x1ff));
	}
	else
	{
		if (!m_c417.test_mode) LOGMASKED(LOG_C412_RAM, "C412 Unknown RAM write: %08x = %04x & %04x\n", (m_c412.adr << 1), data, mem_mask);
	}
	m_c412.adr++;
}


// C421

u16 namcos23_state::c421_ram_r()
{
	offs_t offset = m_c421.adr & 0xfffff;
	LOGMASKED(LOG_C412_UNK, "%s: c421_ram_r: %06x\n", machine().describe_context(), offset);
	if (offset < 0x40000)
		return m_c421.dram_a[offset & 0x3ffff];
	else if (offset < 0x80000)
		return m_c421.dram_b[offset & 0x3ffff];
	else if (offset < 0x88000)
		return m_c421.sram  [offset & 0x07fff];

	return 0xffff;
}

void namcos23_state::c421_ram_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset = m_c421.adr & 0xfffff;
	if (offset < 0x40000)
		COMBINE_DATA(m_c421.dram_a + (offset & 0x3ffff));
	else if (offset < 0x80000)
		COMBINE_DATA(m_c421.dram_b + (offset & 0x3ffff));
	else if (offset < 0x88000)
		COMBINE_DATA(m_c421.sram   + (offset & 0x07fff));
	else
		LOGMASKED(LOG_C421_RAM, "C421 Unknown RAM write: %08x = %04x & %04x\n", (offset << 1), data, mem_mask);
	m_c421.adr += 2;
}

u16 namcos23_state::c421_addr_msw_r()
{
	LOGMASKED(LOG_C421_UNK, "%s: c421 RAM address MSW read: %04x\n", machine().describe_context(), (u16)(m_c421.adr >> 16));
	return (u16)(m_c421.adr >> 16);
}

void namcos23_state::c421_addr_msw_w(offs_t offset, u16 data, u16 mem_mask)
{
	m_c421.adr = ((data & mem_mask) << 16) | (m_c421.adr & ~(mem_mask << 16));
}

u16 namcos23_state::c421_addr_lsw_r()
{
	LOGMASKED(LOG_C421_UNK, "%s: c421 RAM address LSW read: %04x\n", machine().describe_context(), (u16)m_c421.adr);
	return (u16)m_c421.adr;
}

void namcos23_state::c421_addr_lsw_w(offs_t offset, u16 data, u16 mem_mask)
{
	m_c421.adr = (data & mem_mask) | (m_c421.adr & ~(u32)mem_mask);
}


// C422

u16 namcos23_state::c422_r(offs_t offset)
{
	LOGMASKED(LOG_C422_UNK, "%s: c422 reg[%d] read: %04x\n", machine().describe_context(), offset, m_c422.regs[offset]);
	return m_c422.regs[offset];
}

void namcos23_state::c422_irq_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_C422_IRQ, "%s: c422_irq_w: offset 1: %04x\n", machine().describe_context(), data);
	if (data == 0xfffb)
	{
		LOGMASKED(LOG_C422_IRQ, "%s: c422_irq_w: raise IRQ 3\n", machine().describe_context());
		irq_update(m_main_irqcause | MAIN_C422_IRQ);
	}
	else if (data == 0x000f)
	{
		LOGMASKED(LOG_C422_IRQ, "%s: c422_irq_w: ack IRQ 3\n", machine().describe_context());
		irq_update(m_main_irqcause & ~MAIN_C422_IRQ);
	}

	COMBINE_DATA(&m_c422.regs[1]);
}

void namcos23_state::c422_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_C422_UNK, "%s: c422 unknown write %x = %04x & %04x\n", machine().describe_context(), offset, data, mem_mask);
	COMBINE_DATA(&m_c422.regs[offset]);
}

void namcos23_state::c139_irq_w(int state)
{
	if (state)
		irq_update(m_main_irqcause | MAIN_C422_IRQ);
	else
		irq_update(m_main_irqcause & ~MAIN_C422_IRQ);
}



// C361 (text)

TIMER_CALLBACK_MEMBER(namcos23_state::c361_timer_cb)
{
	if (m_c361.scanline != 0x1ff)
	{
		LOGMASKED(LOG_C361_IRQ, "c361 timer callback, scanline %d\n", m_c361.scanline);
		// need to do a partial update here, but doesn't work properly yet
		irq_update(m_main_irqcause | MAIN_C361_IRQ);

		// TC2 indicates it's probably one-shot since it resets it each VBL...
		//c361.timer->adjust(m_screen->time_until_pos(c361.scanline));
	}
	else
	{
		LOGMASKED(LOG_C361_IRQ, "c361 timer callback, clearing irq\n", m_c361.scanline);
		irq_update(m_main_irqcause & ~MAIN_C361_IRQ);
	}
}

void namcos23_state::update_text_rowscroll()
{
	u64 frame = m_screen->frame_number();
	if (frame != m_c404.rowscroll_frame)
	{
		m_c404.rowscroll_frame = frame;
		m_c404.lastrow = 0;
	}

	int scroll_x = (m_c404.xscroll - 0x35c) & 0x3ff;
	int y = std::min(m_screen->vpos() + 4, 480); // +4 is a fudge factor, otherwise rowscrolls are 4 lines too early

	// save x scroll value until current scanline
	for (int i = m_c404.lastrow; i < y; i++)
		m_c404.rowscroll[i] = scroll_x;
	m_c404.lastrow = y;
}

void namcos23_state::apply_text_scroll()
{
	update_text_rowscroll();

	// apply current frame x scroll updates to tilemap
	for (int i = 0; i < 480; i++)
	{
		m_c404.linexscroll[(i + (m_c404.yscroll & 0x3ff) + 4) & 0x3ff] = m_c404.rowscroll[i];
	}
}

void namcos23_state::c361_xscroll_w(offs_t offset, u16 data)
{
	LOGMASKED(LOG_C361_REG, "%s: c361_xscroll_w: %04x (scanline %d)\n", machine().describe_context(), data, m_screen->vpos());
	update_text_rowscroll();
	m_c404.xscroll = data;
}

void namcos23_state::c361_yscroll_w(offs_t offset, u16 data)
{
	LOGMASKED(LOG_C361_REG, "%s: c361_yscroll_w: %04x (scanline %d)\n", machine().describe_context(), data, m_screen->vpos());
	m_c404.yscroll = data;
}

void namcos23_state::c361_irq_scanline_w(offs_t offset, u16 data)
{
	LOGMASKED(LOG_C361_IRQ, "%s: c361_irq_scanline_w: %04x\n", machine().describe_context(), data);
	m_c361.scanline = (data & 0x1ff);
	m_c361.timer->adjust(m_screen->time_until_pos(m_c361.scanline));
}

u16 namcos23_state::c361_vpos_r()
{
	irq_update(m_main_irqcause & ~MAIN_C361_IRQ);
	return (m_screen->vpos() * 2) | (m_screen->vblank() ? 1 : 0);
}

u16 namcos23_state::c361_vblank_r()
{
	const u16 data = ((m_main_irqcause & MAIN_C361_IRQ) || m_screen->vblank()) ? 1 : 0;
	irq_update(m_main_irqcause & ~MAIN_C361_IRQ);
	return data;
}


// C?? (control)

void namcos23_state::direct_buf_start_w(offs_t offset, u16 data, u16 mem_mask)
{
	LOGMASKED(LOG_DIRECT, "%s: direct_buf_start_w: %04x\n", machine().describe_context(), data);
	m_c435.direct_buf_open = (bool)data;
	m_c435.direct_buf_pos = 0;
	return;
}

void namcos23_state::direct_buf_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (!m_c435.direct_buf_open)
		return;

	LOGMASKED(LOG_DIRECT, "%s: direct_buf_w: %04x\n", machine().describe_context(), data);
	if (m_c435.direct_buf_pos == 0)
	{
		m_c435.direct_buf_pos++;
		return;
	}
	m_c435.direct_buf[m_c435.direct_buf_pos - 1] = data;
	m_c435.direct_buf_pos++;
	m_c435.direct_buf_nonempty = true;

	if (m_c435.direct_buf_pos >= 29)
	{
		render_t &render = m_render;
		namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
		re->type = DIRECT;
		re->poly_fade_r = m_c404.poly_fade_r;
		re->poly_fade_g = m_c404.poly_fade_g;
		re->poly_fade_b = m_c404.poly_fade_b;
		re->poly_alpha_color = m_c404.poly_alpha_color;
		re->poly_alpha_pen = m_c404.poly_alpha_pen;
		re->poly_alpha = m_c404.poly_alpha;
		re->screen_fade_r = m_c404.screen_fade_r;
		re->screen_fade_g = m_c404.screen_fade_g;
		re->screen_fade_b = m_c404.screen_fade_b;
		re->screen_fade_factor = m_c404.screen_fade_factor;
		re->fade_flags = m_c404.fade_flags;
		re->absolute_priority = m_absolute_priority;
		re->model_blend_factor = 0;
		re->tx = 0;
		re->ty = 0;
		re->vp_size_x = 320;
		re->vp_size_y = 240;
		re->vp_offset_x = 0;
		re->vp_offset_y = 0;
		re->vp_fov = 320.f;
		memcpy(re->direct.d, m_c435.direct_buf, sizeof(m_c435.direct_buf));
		render.count[render.cur]++;

		m_c435.direct_buf_nonempty = false;
		m_c435.direct_buf_pos = 0;
	}
}

void namcos23_state::ctl_leds_w(offs_t offset, u16 data)
{
	if (m_ctl_led != (data & 0xff))
	{
		m_ctl_led = data & 0xff;
		for (int i = 0; i < 8; i++)
			m_lamps[i] = BIT(data, 7 - i);
	}
}

u16 namcos23_state::ctl_status_r()
{
	// 0100 set freezes gorgon (polygon fifo flag)
	const u16 data = 0x0000 | ioport("DSW")->read() | ((m_main_irqcause & MAIN_C361_IRQ) ? 0x400 : 0);
	LOGMASKED(LOG_CTL_REG, "%s: ctl_status_r: %04x\n", machine().describe_context(), data);
	return data;
}

u16 namcos23_state::ctl_input1_r()
{
	const u16 data = m_ctl_inp_buffer[0] & 0x800 ? 0xffff : 0x0000;
	m_ctl_inp_buffer[0] = (m_ctl_inp_buffer[0] << 1) | 1;
	return data;
}

void namcos23_state::ctl_input1_w(offs_t offset, u16 data)
{
	// These may be coming from another CPU, in particular the I/O one
	m_ctl_inp_buffer[0] = m_p1->read();
}

u16 namcos23_state::ctl_input2_r()
{
	const u16 data = m_ctl_inp_buffer[1] & 0x800 ? 0xffff : 0x0000;
	m_ctl_inp_buffer[1] = (m_ctl_inp_buffer[1] << 1) | 1;
	return data;
}

void namcos23_state::ctl_input2_w(offs_t offset, u16 data)
{
	// These may be coming from another CPU, in particular the I/O one
	m_ctl_inp_buffer[1] = m_p2->read();
}

void namcos23_state::ctl_vbl_ack_w(offs_t offset, u16 data)
{
	if (m_ctl_vbl_active)
	{
		m_ctl_vbl_active = false;
		irq_update(m_main_irqcause & ~MAIN_VBLANK_IRQ);
	}
}

void namcos23_state::ctl_direct_poly_w(offs_t offset, u16 data)
{
	// gmen wars spams this heavily with 0 prior to starting the GMEN board test
	LOGMASKED(LOG_DIRECT, "%s: ctl_direct_poly_w: %04x\n", machine().describe_context(), data);
	m_c435.direct_buf[m_c435.direct_buf_pos++] = data;
	if (data)
		m_c435.direct_buf_nonempty = true;
	if (m_c435.direct_buf_pos >= 28)
	{
		if (m_c435.direct_buf_nonempty)
		{
			render_t &render = m_render;
			namcos23_render_entry *re = render.entries[render.cur] + render.count[render.cur];
			if (!BIT(m_c435.direct_buf[0], 15))
			{
				re->type = DIRECT;
				re->poly_fade_r = m_c404.poly_fade_r;
				re->poly_fade_g = m_c404.poly_fade_g;
				re->poly_fade_b = m_c404.poly_fade_b;
				re->poly_alpha_color = m_c404.poly_alpha_color;
				re->poly_alpha_pen = m_c404.poly_alpha_pen;
				re->poly_alpha = m_c404.poly_alpha;
				re->screen_fade_r = m_c404.screen_fade_r;
				re->screen_fade_g = m_c404.screen_fade_g;
				re->screen_fade_b = m_c404.screen_fade_b;
				re->screen_fade_factor = m_c404.screen_fade_factor;
				re->fade_flags = m_c404.fade_flags;
				re->absolute_priority = m_absolute_priority;
				re->model_blend_factor = 0;
				re->tx = 0;
				re->ty = 0;
				memcpy(re->direct.d, m_c435.direct_buf, sizeof(m_c435.direct_buf));
				render.count[render.cur]++;
			}
		}
		m_c435.direct_buf_nonempty = false;
		m_c435.direct_buf_pos = 0;
	}
}


// C?? (MCU enable)

void namcos23_state::mcuen_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (offset != 7)
	{
		LOGMASKED(LOG_MCU, "%s: mcuen_w: %08x = %04x & %04x\n", machine().describe_context(), offset, data, mem_mask);
	}
	switch (offset)
	{
	case 2:
		// subcpu irq ack
		irq_update(m_main_irqcause & ~MAIN_SUBCPU_IRQ);
		break;

	case 5:
		// boot/start the audio mcu
		if (data)
		{
			// Panic Park: writing 1 when it's already running means reboot?
			if (m_subcpu_running)
			{
				LOGMASKED(LOG_MCU, "%s: mcuen_w: resetting H8/3002\n", machine().describe_context());
				m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
			}

			m_subcpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
			m_subcpu_running = true;
		}
		else
		{
			LOGMASKED(LOG_MCU, "%s: mcuen_w: stopping H8/3002\n", machine().describe_context());
			m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
			m_subcpu_running = false;
		}
		break;

	default:
		// For some reason, the main program writes the high 16bits of the first 0x28000
		// 32-bits words of itself to offset 7
		//logerror("mcuen_w: mask %04x, data %04x @ %x\n", mem_mask, data, offset);
		break;
	}
}


// C?? (unknown comms)

// 6850 ACIA comms info:
// while getting the subcpu to be ready, panicprk sits in a tight loop waiting for status AND 0002 to be non-zero (at PC=BFC02F00)
// bit 1 tx fifo empty
// bit 0 rx fifo ready
// bit 4-6: error (reads data port and discards it) (PC=0xbfc03698)
// data ready is signalled thru MIPS irq 4
// 3f0fa8 (phys address) is where data pops up in TC2 & PP
// PC=0xbfc03838 bit 7 high => fail (data loaded must be parsed somehow)
u16 namcos23_state::sub_comm_status_r()
{
	m_maincpu->set_input_line(m_rs232_irqnum, CLEAR_LINE);
	LOGMASKED(LOG_MCU, "%s: sub_comm_status_r: %04x\n", machine().describe_context(), 3);
	return 1 | 2;
}

u16 namcos23_state::sub_comm_data_r()
{
	// data rx, TBD
	LOGMASKED(LOG_MCU, "%s: sub_comm_data_r data read: %04x\n", machine().describe_context(), 0);
	return 0;
}

void namcos23_state::sub_comm_data_w(offs_t offset, u8 data)
{
	LOGMASKED(LOG_MCU, "%s: sub_comm_data_w: %02x\n", machine().describe_context(), data);
	// data tx
	m_maincpu->set_input_line(m_rs232_irqnum, ASSERT_LINE);
}

void crszone_state::irq_vbl_ack_w(offs_t offset, u32 data)
{
	irq_update(m_main_irqcause & ~MAIN_VBLANK_IRQ);
}

u32 crszone_state::irq_lv3_status_r()
{
	u32 data = 0;
	if (m_main_irqcause & MAIN_SUBCPU_IRQ)
	{
		data |= 1;
		irq_update(m_main_irqcause & ~MAIN_SUBCPU_IRQ);
	}
	if (m_main_irqcause & MAIN_C422_IRQ)
	{
		data |= 2;
		irq_update(m_main_irqcause & ~MAIN_C422_IRQ);
	}
	LOGMASKED(LOG_IRQ_STATUS, "%s: LV3 IRQ status read: %08x\n", machine().describe_context(), data);
	return data;
}

u32 crszone_state::irq_lv5_status_r()
{
	u32 data = 0;
	if (m_main_irqcause & MAIN_C451_IRQ)
	{
		data |= 1;
		irq_update(m_main_irqcause & ~MAIN_C451_IRQ);
	}
	if (m_main_irqcause & MAIN_C361_IRQ)
	{
		data |= 2;
		irq_update(m_main_irqcause & ~MAIN_C361_IRQ);
	}
	LOGMASKED(LOG_IRQ_STATUS, "%s: LV5 IRQ status read: %08x\n", machine().describe_context(), data);
	return data;
}

u32 crszone_state::irq_lv6_status_r()
{
	u32 data = 0;
	if (m_main_irqcause & MAIN_C450_IRQ)
	{
		data |= 2;
		irq_update(m_main_irqcause & ~MAIN_C450_IRQ);
	}
	LOGMASKED(LOG_IRQ_STATUS, "%s: LV6 IRQ status read: %08x\n", machine().describe_context(), data);
	return data;
}



/***************************************************************************

  MIPS CPU Memory Maps

***************************************************************************/

void namcos23_state::mips_base_map(address_map &map)
{
	map.global_mask(0xfffffff);
	map(0x00000000, 0x00ffffff).ram().share("mainram");

	map(0x0100001c, 0x0100001f).w(FUNC(namcos23_state::c435_dma_addr_w));
	map(0x01000020, 0x01000023).w(FUNC(namcos23_state::c435_dma_size_w));
	map(0x01000024, 0x01000027).w(FUNC(namcos23_state::c435_dma_start_w));
	map(0x01000028, 0x0100002b).r(FUNC(namcos23_state::c435_busy_flag_r));
	map(0x01000060, 0x01000063).w(FUNC(namcos23_state::c435_clear_bufpos_w));

	map(0x02000000, 0x02000001).rw(FUNC(namcos23_state::c417_status_r), FUNC(namcos23_state::c435_pio_w));
	map(0x02000002, 0x02000003).rw(FUNC(namcos23_state::c417_addr_r), FUNC(namcos23_state::c417_addr_w));
	map(0x02000004, 0x02000005).w(FUNC(namcos23_state::c417_ptrom_addr_w));
	map(0x02000006, 0x02000007).rw(FUNC(namcos23_state::c417_test_done_r), FUNC(namcos23_state::c417_ptrom_addr_clear_w));
	map(0x02000008, 0x02000009).rw(FUNC(namcos23_state::c417_ram_r), FUNC(namcos23_state::c417_ram_w));
	map(0x0200000a, 0x0200000b).r(FUNC(namcos23_state::c417_ptrom_msw_r));
	map(0x0200000c, 0x0200000d).r(FUNC(namcos23_state::c417_ptrom_lsw_r));
	map(0x0200000e, 0x0200000f).w(FUNC(namcos23_state::c417_irq_ack_w));

	map(0x04400000, 0x0440ffff).ram().share("shared_ram"); // Communication RAM (C416)
	map(0x04c3ff00, 0x04c3ff0f).w(FUNC(namcos23_state::mcuen_w));

	map(0x0d000000, 0x0d000001).w(FUNC(namcos23_state::ctl_leds_w));
	map(0x0d000002, 0x0d000003).r(FUNC(namcos23_state::ctl_status_r));
	map(0x0d000004, 0x0d000005).rw(FUNC(namcos23_state::ctl_input1_r), FUNC(namcos23_state::ctl_input1_w));
	map(0x0d000006, 0x0d000007).rw(FUNC(namcos23_state::ctl_input2_r), FUNC(namcos23_state::ctl_input2_w));
	map(0x0d00000a, 0x0d00000b).w(FUNC(namcos23_state::ctl_vbl_ack_w));
	map(0x0d00000c, 0x0d00000d).w(FUNC(namcos23_state::ctl_direct_poly_w));

	map(0x0fc00000, 0x0fffffff).nopw().rom().region("user1", 0);
}

void gorgon_state::mips_map(address_map &map)
{
	namcos23_state::mips_base_map(map);
	map(0x0100005c, 0x0100005f).w(FUNC(gorgon_state::c435_pio_mode_w));

	map(0x06000000, 0x06000003).w(FUNC(gorgon_state::nvram_w));
	map(0x06080000, 0x0608000f).rw(FUNC(gorgon_state::czattr_r), FUNC(gorgon_state::czattr_w)); // CZ Attribute RAM
	map(0x06080200, 0x060803ff).rw(FUNC(gorgon_state::czram_r), FUNC(gorgon_state::czram_w));

	c404_map(map, 0x06108000);

	map(0x06110000, 0x0613ffff).ram().w(FUNC(gorgon_state::paletteram_w)).share("paletteram"); // Palette RAM (C404)
	map(0x06300000, 0x06300001).w(FUNC(gorgon_state::sprites_idx_w));
	map(0x06300002, 0x06300003).w(FUNC(gorgon_state::sprites_data_w));
	map(0x06400000, 0x0641dfff).ram().share("charram"); // Text CGRAM (C361)
	map(0x0641e000, 0x0641ffff).ram().share("textram"); // Text VRAM (C361)

	c361_map(map, 0x06420000);

	map(0x08000000, 0x087fffff).rom().region("data", 0x000000); // data ROMs

	map(0x0c000000, 0x0c00ffff).ram().share("nvram"); // Backup RAM

	map(0x0f000000, 0x0f000001).r(FUNC(gorgon_state::sub_comm_status_r));
	map(0x0f000002, 0x0f000003).rw(FUNC(gorgon_state::sub_comm_data_r), FUNC(gorgon_state::sub_comm_data_w));
	map(0x0f200000, 0x0f203fff).ram(); // C422 RAM
	map(0x0f300000, 0x0f30000f).rw(FUNC(gorgon_state::c422_r), FUNC(gorgon_state::c422_w)); // C422 registers
	map(0x0f300002, 0x0f300003).w(FUNC(gorgon_state::c422_irq_w));
}

// (Super) System 23
void namcos23_state::mips_map(address_map &map)
{
	namcos23_state::mips_base_map(map);

	// P056 (branch patch/boat-render-trace): silence the ~250-writes/frame unmapped-
	// write log spam at 0x0100005C (P055 run: 8.9M lines = ~94% of the log).
	// 0x0100005C is a REAL C435/C361 register on the sibling drivers - the C361
	// IRQ-acknowledge (namcoss23_state::c435_c361_ack_w, just irq_update(...&~MAIN_C361_IRQ))
	// / C435 PIO-mode (gorgon_state::c435_pio_mode_w) port - but on plain System 23
	// (which timecrs2 runs on) it is deliberately UNMAPPED, so the ROM's ~250/frame
	// writes of 0x00000000 (a C361-ack/PIO handshake the System 23 render path does
	// not need in MAME) hit the unmapped-write logger.  An unmapped write is already
	// discarded, so this writenop is BYTE-IDENTICAL behaviour - it only removes the
	// log line.  UNCONDITIONAL: always-on log hygiene benefits every run, and it
	// matches the crszone_state nopw of the same register family below.
	// namcoss23_state::mips_map chains this map then re-maps 0x0100005C to its real
	// ack handler, so Super System 23 is unaffected (its later .w() override wins).
	// MODEL PROVENANCE: Opus 4.8.
	map(0x0100005c, 0x0100005f).nopw();

	map(0x06000000, 0x0600ffff).ram().share("nvram"); // Backup RAM
	map(0x06200000, 0x06203fff).rw(m_c139, FUNC(namco_c139_device::ram_r), FUNC(namco_c139_device::ram_w)); // C422 (=C139) RAM
	map(0x06400000, 0x0640000f).m(m_c139, FUNC(namco_c139_device::regs_map));                              // C422 registers
	map(0x06800000, 0x0681dfff).ram().share("charram"); // Text CGRAM (C361)
	map(0x0681e000, 0x0681ffff).ram().share("textram"); // Text VRAM (C361)

	c361_map(map, 0x06820000);
	c404_map(map, 0x06a08000);

	map(0x06a10000, 0x06a3ffff).ram().w(FUNC(namcos23_state::paletteram_w)).share("paletteram"); // Palette RAM (C404)
	map(0x08000000, 0x08ffffff).rom().region("data", 0x0000000).mirror(0x1000000); // data ROMs
	map(0x0a000000, 0x0affffff).rom().region("data", 0x1000000).mirror(0x1000000);

	map(0x0c000004, 0x0c000005).w(FUNC(namcos23_state::c412_flags_w));
	map(0x0c000006, 0x0c000007).r(FUNC(namcos23_state::c412_flags_r));
	map(0x0c000010, 0x0c000011).rw(FUNC(namcos23_state::c412_addr_lsw_r), FUNC(namcos23_state::c412_addr_lsw_w));
	map(0x0c000012, 0x0c000013).rw(FUNC(namcos23_state::c412_addr_msw_r), FUNC(namcos23_state::c412_addr_msw_w));
	map(0x0c000014, 0x0c000015).rw(FUNC(namcos23_state::c412_ram_r), FUNC(namcos23_state::c412_ram_w));
	map(0x0c000018, 0x0c000019).r(FUNC(namcos23_state::c412_status_r));

	map(0x0c400000, 0x0c400001).rw(FUNC(namcos23_state::c421_ram_r), FUNC(namcos23_state::c421_ram_w));
	map(0x0c400004, 0x0c400005).rw(FUNC(namcos23_state::c421_addr_msw_r), FUNC(namcos23_state::c421_addr_msw_w));
	map(0x0c400006, 0x0c400007).rw(FUNC(namcos23_state::c421_addr_lsw_r), FUNC(namcos23_state::c421_addr_lsw_w));

	map(0x0c800010, 0x0c800011).w(FUNC(namcos23_state::c435_state_reset_w));
	map(0x0c800016, 0x0c800017).w(FUNC(namcos23_state::c435_state_pio_w));
	map(0x0cc00000, 0x0cc00001).w(FUNC(namcos23_state::direct_buf_w));
	map(0x0cc00002, 0x0cc00003).w(FUNC(namcos23_state::direct_buf_start_w));

	map(0x0e800000, 0x0e800001).r(FUNC(namcos23_state::sub_comm_status_r));
	map(0x0e800002, 0x0e800003).rw(FUNC(namcos23_state::sub_comm_data_r), FUNC(namcos23_state::sub_comm_data_w));
}

void crszone_state::mips_map(address_map &map)
{
	map.global_mask(0x1fffffff);
	map(0x00000000, 0x00ffffff).ram().share("mainram");

	map(0x10000058, 0x1000005b).nopw();
	map(0x10000080, 0x10000083).w(FUNC(crszone_state::irq_vbl_ack_w));
	map(0x100000a8, 0x100000ab).r(FUNC(crszone_state::irq_lv3_status_r));
	map(0x100000b0, 0x100000b3).r(FUNC(crszone_state::irq_lv5_status_r));
	map(0x100000b4, 0x100000b7).r(FUNC(crszone_state::irq_lv6_status_r));

	map(0x10400000, 0x10400003).w(FUNC(crszone_state::c450_dma_addr_w));
	map(0x1040000c, 0x1040000f).w(FUNC(crszone_state::c450_dma_size_w));
	map(0x1040001c, 0x1040001f).r(FUNC(crszone_state::c450_irq_status_r));

	map(0x12000000, 0x12000001).rw(FUNC(crszone_state::c417_status_r), FUNC(crszone_state::c435_pio_w));
	map(0x12000002, 0x12000003).rw(FUNC(crszone_state::c417_addr_r), FUNC(crszone_state::c417_addr_w));
	map(0x12000004, 0x12000005).w(FUNC(crszone_state::c417_ptrom_addr_w));
	map(0x12000006, 0x12000007).rw(FUNC(crszone_state::c417_test_done_r), FUNC(crszone_state::c417_ptrom_addr_clear_w));
	map(0x12000008, 0x12000009).rw(FUNC(crszone_state::c417_ram_r), FUNC(crszone_state::c417_ram_w));
	map(0x1200000a, 0x1200000b).r(FUNC(crszone_state::c417_ptrom_msw_r));
	map(0x1200000c, 0x1200000d).rw(FUNC(crszone_state::c417_ptrom_lsw_r), FUNC(crszone_state::c417_irq_ack_w));

	map(0x14400000, 0x1440ffff).ram().share("shared_ram"); // Communication RAM (C416)
	map(0x14c3ff00, 0x14c3ff0f).w(FUNC(crszone_state::mcuen_w));
	map(0x16000000, 0x1600ffff).ram().share("nvram"); // Backup RAM
	map(0x16200000, 0x16203fff).ram(); // C422 RAM
	map(0x16400000, 0x1640000f).rw(FUNC(crszone_state::c422_r), FUNC(crszone_state::c422_w)); // C422 registers
	map(0x16400002, 0x16400003).w(FUNC(crszone_state::c422_irq_w));
	map(0x16800000, 0x1681dfff).ram().share("charram"); // Text CGRAM (C361)
	map(0x1681e000, 0x1681ffff).ram().share("textram"); // Text VRAM (C361)

	c361_map(map, 0x16820000);
	c404_map(map, 0x16a08000);

	map(0x16a10000, 0x16a3ffff).ram().w(FUNC(crszone_state::paletteram_w)).share("paletteram"); // Palette RAM (C404)
	map(0x18000000, 0x18ffffff).rom().region("data", 0x0000000).mirror(0x1000000); // data ROMs
	map(0x1a000000, 0x1affffff).rom().region("data", 0x1000000).mirror(0x1000000);

	map(0x1c000004, 0x1c000005).w(FUNC(crszone_state::c412_flags_w));
	map(0x1c000006, 0x1c000007).r(FUNC(crszone_state::c412_flags_r));
	map(0x1c000010, 0x1c000011).rw(FUNC(crszone_state::c412_addr_lsw_r), FUNC(crszone_state::c412_addr_lsw_w));
	map(0x1c000012, 0x1c000013).rw(FUNC(crszone_state::c412_addr_msw_r), FUNC(crszone_state::c412_addr_msw_w));
	map(0x1c000014, 0x1c000015).rw(FUNC(crszone_state::c412_ram_r), FUNC(crszone_state::c412_ram_w));
	map(0x1c000018, 0x1c000019).r(FUNC(crszone_state::c412_status_r));

	map(0x1c400000, 0x1c400001).rw(FUNC(crszone_state::c421_ram_r), FUNC(crszone_state::c421_ram_w));
	map(0x1c400004, 0x1c400005).rw(FUNC(crszone_state::c421_addr_msw_r), FUNC(crszone_state::c421_addr_msw_w));
	map(0x1c400006, 0x1c400007).rw(FUNC(crszone_state::c421_addr_lsw_r), FUNC(crszone_state::c421_addr_lsw_w));

	map(0x1c800010, 0x1c800011).w(FUNC(crszone_state::c435_state_reset_w));
	map(0x1c800016, 0x1c800017).w(FUNC(crszone_state::c435_state_pio_w));
	// map(0x1cc00000, 0x1cc00003).w(FUNC(crszone_state::direct_buf_w)); // Currently commented out due to obstructing other geometry

	map(0x1d000000, 0x1d000001).w(FUNC(crszone_state::ctl_leds_w));
	map(0x1d000002, 0x1d000003).r(FUNC(crszone_state::ctl_status_r));
	map(0x1d000004, 0x1d000005).rw(FUNC(crszone_state::ctl_input1_r), FUNC(crszone_state::ctl_input1_w));
	map(0x1d000006, 0x1d000007).rw(FUNC(crszone_state::ctl_input2_r), FUNC(crszone_state::ctl_input2_w));
	map(0x1d00000a, 0x1d00000b).w(FUNC(crszone_state::ctl_vbl_ack_w));
	map(0x1d00000c, 0x1d00000d).w(FUNC(crszone_state::ctl_direct_poly_w));

	map(0x1f000000, 0x1f000003).rw(FUNC(crszone_state::acia_r), FUNC(crszone_state::acia_w));
	map(0x1f800000, 0x1fffffff).nopw().rom().region("user1", 0);
}

void namcos23_state::c361_map(address_map &map, const u32 addr)
{
	map(addr + 0x0, addr + 0x1).w(FUNC(namcos23_state::c361_xscroll_w));
	map(addr + 0x2, addr + 0x3).w(FUNC(namcos23_state::c361_yscroll_w));
	map(addr + 0x8, addr + 0x9).w(FUNC(namcos23_state::c361_irq_scanline_w));
	map(addr + 0xa, addr + 0xb).r(FUNC(namcos23_state::c361_vpos_r));
	map(addr + 0xc, addr + 0xd).r(FUNC(namcos23_state::c361_vblank_r));
}

void namcos23_state::c404_map(address_map &map, const u32 addr)
{
	map(addr + 0x000, addr + 0x7ff).rw(FUNC(namcos23_state::c404_ram_r), FUNC(namcos23_state::c404_ram_w));
	map(addr + 0x000, addr + 0x001).w(FUNC(namcos23_state::c404_poly_fade_red_w));
	map(addr + 0x002, addr + 0x003).w(FUNC(namcos23_state::c404_poly_fade_green_w));
	map(addr + 0x004, addr + 0x005).w(FUNC(namcos23_state::c404_poly_fade_blue_w));
	map(addr + 0x00a, addr + 0x00b).w(FUNC(namcos23_state::c404_fog_red_w));
	map(addr + 0x00c, addr + 0x00d).w(FUNC(namcos23_state::c404_fog_green_w));
	map(addr + 0x00e, addr + 0x00f).w(FUNC(namcos23_state::c404_fog_blue_w));
	map(addr + 0x010, addr + 0x011).w(FUNC(namcos23_state::c404_bg_red_w));
	map(addr + 0x012, addr + 0x013).w(FUNC(namcos23_state::c404_bg_green_w));
	map(addr + 0x014, addr + 0x015).w(FUNC(namcos23_state::c404_bg_blue_w));
	map(addr + 0x01a, addr + 0x01b).w(FUNC(namcos23_state::c404_spot_lsb_w));
	map(addr + 0x01c, addr + 0x01d).w(FUNC(namcos23_state::c404_spot_msb_w));
	map(addr + 0x01e, addr + 0x01f).w(FUNC(namcos23_state::c404_poly_alpha_color_w));
	map(addr + 0x020, addr + 0x021).w(FUNC(namcos23_state::c404_poly_alpha_pen_w));
	map(addr + 0x022, addr + 0x023).w(FUNC(namcos23_state::c404_poly_alpha_w));
	map(addr + 0x024, addr + 0x025).w(FUNC(namcos23_state::c404_alpha_check12_w));
	map(addr + 0x026, addr + 0x027).w(FUNC(namcos23_state::c404_alpha_check13_w));
	map(addr + 0x028, addr + 0x029).w(FUNC(namcos23_state::c404_text_alpha_mask_w));
	map(addr + 0x02a, addr + 0x02b).w(FUNC(namcos23_state::c404_text_alpha_factor_w));
	map(addr + 0x02c, addr + 0x02d).w(FUNC(namcos23_state::c404_screen_fade_red_w));
	map(addr + 0x02e, addr + 0x02f).w(FUNC(namcos23_state::c404_screen_fade_green_w));
	map(addr + 0x030, addr + 0x031).w(FUNC(namcos23_state::c404_screen_fade_blue_w));
	map(addr + 0x032, addr + 0x033).w(FUNC(namcos23_state::c404_screen_fade_factor_w));
	map(addr + 0x034, addr + 0x035).w(FUNC(namcos23_state::c404_fade_flags_w));
	map(addr + 0x036, addr + 0x037).w(FUNC(namcos23_state::c404_palette_base_w));
	map(addr + 0x03e, addr + 0x03f).w(FUNC(namcos23_state::c404_layer_flags_w));
}

void namcoss23_state::mips_map(address_map &map)
{
	namcos23_state::mips_map(map);
	map(0x0100005c, 0x0100005f).w(FUNC(namcoss23_state::c435_c361_ack_w));
}

// GMEN interface

u32 namcoss23_gmen_state::sh2_trigger_r()
{
	LOGMASKED(LOG_SH2, "%s: sh2_trigger_r: booting SH-2\n", machine().describe_context());
	m_sh2->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);

	return 0;
}

u32 namcoss23_gmen_state::sh2_shared_r(offs_t offset, u32 mem_mask)
{
	const u32 data = m_sh2_shared[offset];
	// Command responses go out at offset 0x0020
	return data;
}

void namcoss23_gmen_state::sh2_shared_w(offs_t offset, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_sh2_shared[offset]);
	// Commands come in at offset 0x0020
	if ((offset << 2) == 0x20 && (mem_mask & 0xffff0000) == 0xffff0000 && (data & 0xffff0000) == 0)
	{
		m_sh2->set_input_line(6, CLEAR_LINE);
	}
}

u32 namcoss23_gmen_state::mips_to_sh2_shared_r(offs_t offset, u32 mem_mask)
{
	return m_sh2_shared[offset];
}

void namcoss23_gmen_state::mips_to_sh2_shared_w(offs_t offset, u32 data, u32 mem_mask)
{
	COMBINE_DATA(&m_sh2_shared[offset]);
	if ((offset << 2) == 0x20 && (mem_mask & 0xffff0000) == 0xffff0000 && (data & 0xffff0000) < 0x01000000)
	{
		m_sh2->set_input_line(6, ASSERT_LINE);
	}
}

u32 namcoss23_gmen_state::sh2_dsw_r(offs_t offset, u32 mem_mask)
{
	return m_dsw->read() << 24;
}

u32 namcoss23_gmen_state::mips_sh2_unk_r(offs_t offset, u32 mem_mask)
{
	//logerror("%s: mips_sh2_unk_r: %08x & %08x\n", machine().describe_context().c_str(), 0x00000001, mem_mask);
	return 0x00000001;
}

u32 namcoss23_gmen_state::sh2_vpxstate_r(offs_t offset, u32 mem_mask)
{
	//const u32 data = 0x01000000 | (odd_frame ? 0x00000100 : 0x00000000);
	const u32 data = m_sh2_unk;
	LOGMASKED(LOG_SH2_VPX, "%s: sh2_vpxstate_r: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	return data;
}

void namcoss23_gmen_state::sh2_vpxstate_w(offs_t offset, u32 data, u32 mem_mask)
{
	LOGMASKED(LOG_SH2_VPX, "%s: sh2_vpxstate_w: %08x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_sh2_unk);
}

u32 namcoss23_gmen_state::sh2_unk6200000_r(offs_t offset, u32 mem_mask)
{
	return 0x04000000;
}

u32 namcoss23_gmen_state::vpx_line_r(offs_t offset, u32 mem_mask)
{
	return 0x7fff0000;
}

void namcoss23_gmen_state::vpx_i2c_sdao_w(int state)
{
	m_vpx_sdao = state;
}

u8 namcoss23_gmen_state::vpx_i2c_r()
{
	LOGMASKED(LOG_GMEN, "%s: vpx_i2c_r: %02x\n", machine().describe_context().c_str(), m_vpx_sdao);
	return m_vpx_sdao;
}

void namcoss23_gmen_state::vpx_i2c_w(u8 data)
{
	LOGMASKED(LOG_GMEN, "%s: vpx_i2c_w: %02x\n", machine().describe_context().c_str(), data);
	m_vpx->sda_write(BIT(data, 0));
	m_vpx->scl_write(BIT(data, 1));
}

void namcoss23_gmen_state::mips_map(address_map &map)
{
	namcoss23_state::mips_map(map);
	map(0x0e400000, 0x0e400003).r(FUNC(namcoss23_gmen_state::sh2_trigger_r));
	map(0x0e600000, 0x0e600003).r(FUNC(namcoss23_gmen_state::mips_sh2_unk_r));
	map(0x0e700000, 0x0e70ffff).rw(FUNC(namcoss23_gmen_state::mips_to_sh2_shared_r), FUNC(namcoss23_gmen_state::mips_to_sh2_shared_w));
}

void namcoss23_gmen_state::sh2_map(address_map &map)
{
	map(0x00000000, 0x0000ffff).mirror(0x01000000).rw(FUNC(namcoss23_gmen_state::sh2_shared_r), FUNC(namcoss23_gmen_state::sh2_shared_w));
	map(0x01400000, 0x014003ff).r(FUNC(namcoss23_gmen_state::vpx_line_r));
	map(0x01800000, 0x01bfffff).ram();
	map(0x02800000, 0x02800003).rw(FUNC(namcoss23_gmen_state::sh2_vpxstate_r), FUNC(namcoss23_gmen_state::sh2_vpxstate_w));
	map(0x03000000, 0x03000003).r(FUNC(namcoss23_gmen_state::sh2_dsw_r));
	map(0x04000000, 0x043fffff).ram(); // SH-2 main work RAM (SDRAM)
	map(0x06000000, 0x06000003).umask32(0xff000000).rw(FUNC(namcoss23_gmen_state::vpx_i2c_r), FUNC(namcoss23_gmen_state::vpx_i2c_w));
	map(0x06200000, 0x06200003).r(FUNC(namcoss23_gmen_state::sh2_unk6200000_r));
	map(0x06200000, 0x06200003).nopw();
	map(0x06600000, 0x06600003).nopw();
	map(0x06a00000, 0x06a00003).nopw();
	map(0x00c00000, 0x00c0006b).m(m_firewire, FUNC(md8412b_s23_device::map));
}





/***************************************************************************

  Sub CPU (H8/3002 MCU) I/O + Memory Map

***************************************************************************/

void namcos23_state::sharedram_sub_w(offs_t offset, u16 data, u16 mem_mask)
{
	u16 *shared16 = reinterpret_cast<u16 *>(m_shared_ram.target());
	COMBINE_DATA(&shared16[BYTE_XOR_BE(offset)]);
}

u16 namcos23_state::sharedram_sub_r(offs_t offset)
{
	u16 *shared16 = reinterpret_cast<u16 *>(m_shared_ram.target());
	return shared16[BYTE_XOR_BE(offset)];
}

void namcos23_state::sub_interrupt_main_w(offs_t offset, u16 data, u16 mem_mask)
{
	if ((mem_mask == 0xffff) && (data == 0x3170))
		irq_update(m_main_irqcause | MAIN_SUBCPU_IRQ);
	else
		LOGMASKED(LOG_SUBIRQ, "%s: Unknown write %x to sub_interrupt_main_w!\n", machine().describe_context(), data);
}

void crszone_state::acia_irq_w(int state)
{
	if (state)
		irq_update(m_main_irqcause | MAIN_RS232_IRQ);
	else
		irq_update(m_main_irqcause & ~MAIN_RS232_IRQ);
}

u8 crszone_state::acia_r(offs_t offset)
{
	u8 data = 0;
	if (offset == 1)
	{
		data = m_acia->status_r();
		LOGMASKED(LOG_RS232, "%s: ACIA S23 status read: %02x\n", machine().describe_context(), data);
		return data;
	}
	if (offset == 3)
	{
		data = m_acia->data_r();
		LOGMASKED(LOG_RS232, "%s: ACIA S23 data read: %02x\n", machine().describe_context(), data);
		return data;
	}
	LOGMASKED(LOG_RS232, "%s: ACIA S23 read %d: %02x\n", machine().describe_context(), offset, data);
	return data;
}

void crszone_state::acia_w(offs_t offset, u8 data)
{
	if (offset == 1)
	{
		LOGMASKED(LOG_RS232, "%s: ACIA control write = %02x\n", machine().describe_context(), data);
		m_acia->control_w(data);
		return;
	}
	LOGMASKED(LOG_RS232, "%s: ACIA S23 write %d = %02x\n", machine().describe_context(), offset, data);
}

// Port 6

u8 namcos23_state::mcu_p6_r()
{
	// bit 1 = JVS cable present sense (1 = I/O board plugged in)
	const u8 data = ((m_jvs_sense != jvs_port_device::sense::Initialized) << 1) | 0xfd;
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_p6_r: %02x\n", machine().describe_context(), data);
	return data;
}

void namcos23_state::mcu_p6_w(u8 data)
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_p6_w: %02x\n", machine().describe_context(), data);
	static u8 old_data = 0;
	if (data != old_data)
	{
		const u8 changed = data ^ old_data;
		for (int i = 0; i < 8; i++)
		{
			if (!BIT(changed, i))
				continue;
			LOGMASKED(LOG_MCU_PORTS, "%s:           Bit %d changed (now %d)\n", machine().describe_context(), i, BIT(data, i));
		}
		old_data = data;
	}
}



// Port 8

u8 namcos23_state::mcu_p8_r()
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_p8_r: %02x\n", machine().describe_context(), m_sub_port8);
	return m_sub_port8;
}

void namcos23_state::mcu_p8_w(u8 data)
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_p8_w: %02x\n", machine().describe_context(), data);
	static u8 old_data = 0;
	if (data != old_data)
	{
		const u8 changed = data ^ old_data;
		for (int i = 0; i < 8; i++)
		{
			if (!BIT(changed, i))
				continue;
			LOGMASKED(LOG_MCU_PORTS, "%s:           Bit %d changed (now %d)\n", machine().describe_context(), i, BIT(data, i));
		}
		old_data = data;
	}
	m_sub_port8 = (data & ~0x02) | (m_sub_port8 & 0x02);
}



// Port A

u8 namcos23_state::mcu_pa_r()
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_pa_r: %02x\n", machine().describe_context(), m_sub_porta);
	return m_sub_porta;
}

void namcos23_state::mcu_pa_w(u8 data)
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_pa_w: %02x\n", machine().describe_context(), data);
	static u8 old_data = 0;
	if (data != old_data)
	{
		const u8 changed = data ^ old_data;
		for (int i = 0; i < 8; i++)
		{
			if (!BIT(changed, i))
				continue;
			LOGMASKED(LOG_MCU_PORTS, "%s:           Bit %d changed (now %d)\n", machine().describe_context(), i, BIT(data, i));
		}
		old_data = data;
	}
	m_sub_porta = data;
	m_rtc->ce_w((m_sub_portb & 0x20) && (m_sub_porta & 1));
	m_settings->ce_w((m_sub_portb & 0x20) && !(m_sub_porta & 1));
}



// Port B

u8 namcos23_state::mcu_pb_r()
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_pb_r: %02x\n", machine().describe_context(), m_sub_portb);
	return m_sub_portb;
}

void namcos23_state::mcu_pb_w(u8 data)
{
	LOGMASKED(LOG_MCU_PORTS, "%s: mcu_pb_w: %02x\n", machine().describe_context(), data);
	static u8 old_data = 0;
	if (data != old_data)
	{
		const u8 changed = data ^ old_data;
		for (int i = 0; i < 8; i++)
		{
			if (!BIT(changed, i))
				continue;
			LOGMASKED(LOG_MCU_PORTS, "%s:           Bit %d changed (now %d)\n", machine().describe_context(), i, BIT(data, i));
		}
		old_data = data;
	}
	m_sub_portb = (m_sub_portb & 0xc0) | (data & 0x3f);
	m_rtc->ce_w((m_sub_portb & 0x20) && (m_sub_porta & 1));
	m_settings->ce_w((m_sub_portb & 0x20) && !(m_sub_porta & 1));
	m_jvs->rts(BIT(m_sub_portb, 0));
}


void namcos23_state::s23h8rwmap(address_map &map)
{
	map(0x000000, 0x07ffff).rom();
	map(0x080000, 0x08ffff).rw(FUNC(namcos23_state::sharedram_sub_r), FUNC(namcos23_state::sharedram_sub_w));
	map(0x280000, 0x287fff).rw("c352", FUNC(c352_device::read), FUNC(c352_device::write));
	map(0x300000, 0x300003).noprw(); // seems to be more inputs, maybe false leftover code from System 12?
	map(0x300010, 0x300011).noprw();
	map(0x300020, 0x300021).w(FUNC(namcos23_state::sub_interrupt_main_w));
	map(0x300030, 0x300031).nopw(); // timecrs2 writes this when writing to the sync shared ram location, motoxgo doesn't
}


/***************************************************************************

  Machine Drivers

***************************************************************************/

void namcos23_state::machine_start()
{
	save_item(NAME(m_c404.poly_fade_r));
	save_item(NAME(m_c404.poly_fade_g));
	save_item(NAME(m_c404.poly_fade_b));
	save_item(NAME(m_c404.fog_r));
	save_item(NAME(m_c404.fog_g));
	save_item(NAME(m_c404.fog_b));
	save_item(NAME(m_c404.bgcolor_r));
	save_item(NAME(m_c404.bgcolor_g));
	save_item(NAME(m_c404.bgcolor_b));
	save_item(NAME(m_c404.spot_factor));
	save_item(NAME(m_c404.poly_alpha_color));
	save_item(NAME(m_c404.poly_alpha_pen));
	save_item(NAME(m_c404.poly_alpha));
	save_item(NAME(m_c404.alpha_check12));
	save_item(NAME(m_c404.alpha_check13));
	save_item(NAME(m_c404.alpha_mask));
	save_item(NAME(m_c404.alpha_factor));
	save_item(NAME(m_c404.screen_fade_r));
	save_item(NAME(m_c404.screen_fade_g));
	save_item(NAME(m_c404.screen_fade_b));
	save_item(NAME(m_c404.screen_fade_factor));
	save_item(NAME(m_c404.fade_flags));
	save_item(NAME(m_c404.palbase));
	save_item(NAME(m_c404.layer_flags));
	save_item(NAME(m_c404.ram));
	save_item(NAME(m_c404.spritedata_idx));
	save_item(NAME(m_c404.rowscroll_frame));
	save_item(NAME(m_c404.rowscroll));
	save_item(NAME(m_c404.lastrow));
	save_item(NAME(m_c404.xscroll));
	save_item(NAME(m_c404.yscroll));
	save_item(STRUCT_MEMBER(m_c404.sprites, d));

	save_item(NAME(m_c361.scanline));

	save_item(NAME(m_c417.ram));
	save_item(NAME(m_c417.adr));
	save_item(NAME(m_c417.pointrom_adr));
	save_item(NAME(m_c417.test_mode));

	save_item(NAME(m_c412.sdram_a));
	save_item(NAME(m_c412.sdram_b));
	save_item(NAME(m_c412.sram));
	save_item(NAME(m_c412.pczram));
	save_item(NAME(m_c412.adr));
	save_item(NAME(m_c412.status_c));

	save_item(NAME(m_c421.dram_a));
	save_item(NAME(m_c421.dram_b));
	save_item(NAME(m_c421.sram));
	save_item(NAME(m_c421.adr));

	save_item(NAME(m_c422.regs));

	save_item(NAME(m_jvs_sense));
	save_item(NAME(m_main_irqcause));

	save_item(NAME(m_ctl_vbl_active));
	save_item(NAME(m_ctl_led));
	save_item(NAME(m_ctl_inp_buffer));

	save_item(NAME(m_subcpu_running));

	save_item(NAME(m_c435.address));
	save_item(NAME(m_c435.size));
	save_item(NAME(m_c435.buffer));
	save_item(NAME(m_c435.buffer_pos));
	save_item(NAME(m_c435.direct_buf));
	save_item(NAME(m_c435.direct_vertex));
	save_item(NAME(m_c435.direct_vert_pos));
	save_item(NAME(m_c435.direct_buf_pos));
	save_item(NAME(m_c435.direct_buf_nonempty));
	save_item(NAME(m_c435.direct_buf_open));
	save_item(NAME(m_c435.pio_mode));
	save_item(NAME(m_c435.sprite_target));
	save_item(NAME(m_c435.spritedata));

	save_item(NAME(m_ptrom_limit));
	save_item(NAME(m_clip_data));
	save_item(NAME(m_clip_data_line));

	save_item(NAME(m_absolute_priority));
	save_item(NAME(m_tx));
	save_item(NAME(m_ty));
	save_item(NAME(m_model_blend_factor));
	save_item(NAME(m_light_power));
	save_item(NAME(m_light_ambient));
	save_item(NAME(m_matrices));
	save_item(NAME(m_vectors));
	save_item(NAME(m_light_vector));
	save_item(NAME(m_scaling));
	save_item(NAME(m_spv));
	save_item(NAME(m_spm));

	save_item(NAME(m_sub_port8));
	save_item(NAME(m_sub_porta));
	save_item(NAME(m_sub_portb));

	m_lamps.resolve();

	m_c361.timer = timer_alloc(FUNC(namcos23_state::c361_timer_cb), this);
	m_c361.timer->adjust(attotime::never);

	m_maincpu->add_fastram(0, m_mainram.bytes()-1, false, reinterpret_cast<u32 *>(memshare("mainram")->ptr()));

	m_subcpu_scanline_on_timer = timer_alloc(FUNC(namcos23_state::subcpu_scanline_on_tick), this);
	m_subcpu_scanline_off_timer = timer_alloc(FUNC(namcos23_state::subcpu_scanline_off_tick), this);

	m_texram = m_c412.sram;

	m_rs232_irqnum = -1;
	m_vbl_irqnum = MIPS3_IRQ0;
	m_c361_irqnum = MIPS3_IRQ1;
	m_sub_irqnum = MIPS3_IRQ1;
	m_c435_irqnum = MIPS3_IRQ2;
	m_c422_irqnum = MIPS3_IRQ3;
	m_rs232_irqnum = MIPS3_IRQ5;

	// P001 EXPERIMENT (H1 "keepalive keystone", cherry-picked from
	// patch/keepalive-floor onto patch/continuous-arm): the in-game link
	// keepalive floor.  A numeric value 1..16 is taken as the floor value
	// ("1" = exact P001 semantics); anything non-numeric or out of range
	// falls back to 1 (unchanged pre-P067 parse).  Applied per-vblank in
	// vblank().  P067 (patch/defaults-on): ADOPTED - armed by DEFAULT with
	// floor 2 (the recipe value); NAMCOS23_PATCH_KEEPALIVE_FLOOR=0 is the
	// kill switch (fully inert, the pre-P067 unset path).
	bool keepalive_from_env = false;
	char const *const keepalive_val = patch_env_or_default("NAMCOS23_PATCH_KEEPALIVE_FLOOR", "2", keepalive_from_env);
	m_patch_keepalive_floor = keepalive_val != nullptr;
	m_keepalive_floor_value = 1;
	if (m_patch_keepalive_floor)
	{
		char *parse_end = nullptr;
		unsigned long const floor_v = std::strtoul(keepalive_val, &parse_end, 10);
		if (parse_end != keepalive_val && *parse_end == '\0' && floor_v >= 1 && floor_v <= 0x10)
			m_keepalive_floor_value = u32(floor_v);
		logerror("KEEPALIVE_FLOOR: armed (NAMCOS23_PATCH_KEEPALIVE_FLOOR=%s %s, floor=%u) - while staging mode word 0x802F3FD0 == 2, raise keepalive word 0x802F3FD8 to %u whenever it reads below that at vblank\n",
				keepalive_val, patch_env_src(keepalive_from_env, keepalive_val).c_str(),
				m_keepalive_floor_value, m_keepalive_floor_value);
	}
	else
		logerror("KEEPALIVE_FLOOR: DISABLED (NAMCOS23_PATCH_KEEPALIVE_FLOOR=0 kill switch - adopted default 2 overridden; stock keepalive behavior)\n");

	// P002 EXPERIMENT (H2 "drift lockstep", branch patch/vblank-lockstep):
	// the driver-side 1/s status tap.  The barrier in namco_c139 reads the
	// same variable in its own device_start() - keep the two resolutions
	// agreeing.  P067 (patch/defaults-on): ADOPTED - armed by DEFAULT;
	// NAMCOS23_PATCH_VBLANK_LOCKSTEP=0 is the kill switch (fully inert, the
	// pre-P067 unset path).
	bool lockstep_from_env = false;
	char const *const lockstep_val = patch_env_or_default("NAMCOS23_PATCH_VBLANK_LOCKSTEP", "1", lockstep_from_env);
	m_patch_vblank_lockstep = lockstep_val != nullptr;
	m_lockstep_tap_max_drift = 0;
	m_lockstep_tap_min_keepalive = 0xffffffff;
	m_lockstep_tap_mode2_samples = 0;
	if (m_patch_vblank_lockstep)
		logerror("VBLANK_LOCKSTEP: tap armed (NAMCOS23_PATCH_VBLANK_LOCKSTEP=%s %s) - 1/s status of max drift word 0x802F3504 / min keepalive 0x802F3FD8 (mode-2 samples) + C139 token/stall counters\n",
				lockstep_val, patch_env_src(lockstep_from_env, lockstep_val).c_str());
	else
		logerror("VBLANK_LOCKSTEP: driver tap DISABLED (NAMCOS23_PATCH_VBLANK_LOCKSTEP=0 kill switch - adopted default overridden)\n");

	// P003 EXPERIMENT (H3 "round-start arming race", branch
	// patch/round-start-arm): arm the partner re-arm from the environment,
	// same gate idiom as P001/P002 and independent of the P002 gate so the
	// user can toggle each separately.  Applied per-vblank in vblank().
	// Experiment branch only - never merge to milestone.
	char const *const rsa_env = std::getenv("NAMCOS23_PATCH_ROUND_START_ARM");
	m_patch_round_start_arm = rsa_env && rsa_env[0] != '\0'
			&& !(rsa_env[0] == '0' && rsa_env[1] == '\0');
	m_rsa_have_prev = false;
	m_rsa_prev_keepalive = 0;
	m_rsa_prev_rec0 = 0;
	m_rsa_prev_rec1 = 0;
	m_rsa_low24_changes = 0;
	if (m_patch_round_start_arm)
		logerror("ROUND_START_ARM: armed (NAMCOS23_PATCH_ROUND_START_ARM=%s) - on keepalive 0x802F3FD8 refill edge 0->positive while mode word 0x802F3FD0 == 2, OR bit 0x40000000 into the partner record +0x370 word; read-only bit13 + partner low-24 observability\n",
				rsa_env);

	// P008 EXPERIMENT (branch patch/continuous-arm): arm the continuous
	// partner re-arm from the environment, same gate idiom as P001/P002/P003
	// and independent of all other gates.  Applied per-vblank in vblank(),
	// after the P001 floor write.  Experiment branch only - never merge to
	// milestone.
	char const *const ca_env = std::getenv("NAMCOS23_PATCH_CONTINUOUS_ARM");
	m_patch_continuous_arm = ca_env && ca_env[0] != '\0'
			&& !(ca_env[0] == '0' && ca_env[1] == '\0');
	m_ca_rearm_count = 0;
	if (m_patch_continuous_arm)
		logerror("CONTINUOUS_ARM: armed (NAMCOS23_PATCH_CONTINUOUS_ARM=%s) - every vblank while mode word 0x802F3FD0 == 2 and keepalive 0x802F3FD8 > 0 (post-floor), OR bit 0x40000000 into the partner record +0x370 word if clear\n",
				ca_env);

	// P009 EXPERIMENT (branch patch/qual-trace): arm the read-only
	// qualification event tap from the environment, same gate idiom as the
	// others and independent of all patch gates.  The C139 device reads the
	// same variable in its own device_start() for the per-frame
	// fingerprints.  Sampled per-vblank in vblank().  Experiment branch
	// only - never merge to milestone.
	char const *const qual_env = std::getenv("NAMCOS23_TRACE_QUAL");
	m_trace_qual = qual_env && qual_env[0] != '\0'
			&& !(qual_env[0] == '0' && qual_env[1] == '\0');
	m_qt_have_prev = false;
	m_qt_prev_drift = 0;
	m_qt_prev_succ = 0;
	m_qt_prev_rseq = 0;
	m_qt_prev_head = 0;
	m_qt_prev_tail = 0;
	m_qt_win_quals = 0;
	m_qt_win_chkfails = 0;
	m_qt_win_enq = 0;
	m_qt_win_drain = 0;
	if (m_trace_qual)
		logerror("QUAL_TRACE: tap armed (NAMCOS23_TRACE_QUAL=%s) - per-vblank qual (rseq 0x802F3510 change / drift 0x802F3504 reset) + chkfail (gp+0x75C8) events, ring head/tail flow, 1/s status\n",
				qual_env);

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): arm the
	// READ-ONLY adoption / render-gate / cutscene-timer tap from the
	// environment, same gate idiom as the others and independent of all patch
	// gates.  The C139 device reads the same variable in its own device_start()
	// for the per-delivered-frame ADOPT: ingest line.  Sampled per-vblank in
	// vblank().  Experiment branch only - never merge to milestone.
	char const *const adopt_env = std::getenv("NAMCOS23_TRACE_ADOPT");
	m_trace_adopt = adopt_env && adopt_env[0] != '\0'
			&& !(adopt_env[0] == '0' && adopt_env[1] == '\0');
	m_adopt_have_prev = false;
	m_adopt_prev_rec0 = 0;
	m_adopt_prev_rec1 = 0;
	m_adopt_prev_t7054 = 0;
	m_adopt_prev_t7074 = 0;
	m_adopt_prev_a0_active = 0;
	m_adopt_prev_word01c6 = 0;
	m_adopt_max_t7054 = 0;
	m_adopt_t7054_advances = 0;
	m_adopt_p_pos_changes = 0;
	m_adopt_actor_changes = 0;
	m_adopt_word01c6_changes = 0;
	if (m_trace_adopt)
		logerror("ADOPT: tap armed (NAMCOS23_TRACE_ADOPT=%s) - READ-ONLY per-vblank render-gate (bit 0x2000 + 0x0080d0 low-24 on rec0=0x802F43D0/rec1=0x802F4844) + cutscene-timer (gp+0x7054=0x802CE874, gp+0x7074==2=0x802CE894, op-70 gate 0x80016E58/64/80) + actor-spawn slot0 (0x802F4E1C active / 0x802F4FA4 coord0) + link-RAM 0x01c6 (m_ram word 0x11c6, peek) ingest-landing trace; bit-0x2000 arm site = op-55 RX -> 0x80013D44 (sw 0x80013E10); event lines on change + 1/s ADOPT: status; pair with device ADOPT: ingest by t=\n",
				adopt_env);

	// P016 PHASE A EXPERIMENT (branch patch/op70-cutscene-timer-arm): arm the
	// READ-ONLY op-70 cutscene-timer accept/reject tap from the environment, same
	// gate idiom as the others and independent of all patch gates.  The C139
	// device reads the same env var in its own device_start() for the device-side
	// OP70: rx arrival line.  Sampled per-vblank in vblank().  Experiment branch
	// only - never merge to milestone.
	char const *const op70_env = std::getenv("NAMCOS23_TRACE_OP70");
	m_trace_op70 = op70_env && op70_env[0] != '\0'
			&& !(op70_env[0] == '0' && op70_env[1] == '\0');
	m_op70_have_prev = false;
	m_op70_prev_t7054 = 0;
	m_op70_t7054_changes = 0;
	m_op70_gate_local2000_set = 0;
	m_op70_gate_7074eq2 = 0;
	if (m_trace_op70)
		logerror("OP70: tap armed (NAMCOS23_TRACE_OP70=%s) - READ-ONLY per-vblank op-70 cutscene-timer accept/reject proof: gp+0x7054=0x802CE874 change watch (accept = ROM store 0x80016E80 fired) + the TWO gate operands the ROM tests (LOCAL +0x370 bit 0x2000 at 0x80016E50/E58 = 0x802F43D0+localidx*0x474; gp+0x7074==2 at 0x802CE894/0x80016E64, needs partner +0x370 & 0x6000); event lines on gp+0x7054 change + 1/s OP70: status; pair with device OP70: rx (arrival + peer value) by t=\n",
				op70_env);

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm, off
	// patch/op70-real-detector): arm the READ-ONLY linkbits trace from the
	// environment, same gate idiom as the others and independent of all patch
	// gates.  The C139 device reads the SAME env var in its own device_start()
	// for the device-side op55 wire-flag tap (LINKBITS: txflags / rxflags).
	// Sampled per-vblank in vblank().  Experiment branch only - never merge.
	char const *const linkbits_env = std::getenv("NAMCOS23_TRACE_LINKBITS");
	m_trace_linkbits = linkbits_env && linkbits_env[0] != '\0'
			&& !(linkbits_env[0] == '0' && linkbits_env[1] == '\0');
	m_lb_have_prev = false;
	m_lb_prev_local = 0;
	m_lb_prev_partner = 0;
	m_lb_prev_t7074 = 0;
	m_lb_prev_t705c = 0;
	m_lb_local6000_set = 0;
	m_lb_partner6000_set = 0;
	if (m_trace_linkbits)
		logerror("LINKBITS: tap armed (NAMCOS23_TRACE_LINKBITS=%s) - READ-ONLY per-vblank 0x6000 fully-linked-handshake trace: LOCAL rec +0x370 (0x802F43D0+localidx*0x474) 0x6000 set sites bit0x4000@0x80016DBC(gate gp+0x705C==0=0x802CE87C) bit0x2000@0x800166B0; PARTNER rec +0x370 (0x802F43D0+(1-localidx)*0x474) written by op55 RX 0x80013E10 from 24-bit wire flags; GATE gp+0x7074==2@0x802CE894 iff partner 0x6000 both (0x80016BB4-BCC); event lines on each bit edge + gp+0x7074/0x705C change + 1/s LINKBITS: status (local6000/partner6000/t7074/wouldbe); pair with device LINKBITS: txflags/rxflags (op55 wire 0x6000) by t= -> local6000=0 (X), tx-no-6000 (Y), rx-6000+partner6000=0 (Z)\n",
				linkbits_env);

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace, off
	// patch/op55-carrier-repeat): arm the READ-ONLY play-clock + human-gate
	// trace from the environment, same gate idiom as the others and independent
	// of all patch gates.  The C139 device reads the SAME env var in its own
	// device_start() for the op6F cell-walk (PLAYCLOCK: op6f-tx / op6f-rx).
	// Sampled per-vblank in vblank().  See the member-block comment for the full
	// ROM provenance.  Experiment branch only - never merge to milestone.
	char const *const playclock_env = std::getenv("NAMCOS23_TRACE_PLAYCLOCK");
	m_trace_playclock = playclock_env && playclock_env[0] != '\0'
			&& !(playclock_env[0] == '0' && playclock_env[1] == '\0');
	m_pc_have_prev = false;
	m_pc_prev_rec0_390 = 0;
	m_pc_prev_rec0_394 = 0;
	m_pc_prev_rec1_390 = 0;
	m_pc_prev_rec1_394 = 0;
	m_pc_prev_rec0_flags = 0;
	m_pc_prev_rec1_flags = 0;
	m_pc_prev_role3ffc = 0;
	m_pc_prev_rec0_ticking = false;
	m_pc_ticks390 = 0;
	m_pc_bit04_both = 0;
	m_pc_prev_op6f_tx = 0;
	m_pc_prev_op6f_rx = 0;
	if (m_trace_playclock)
		logerror("PLAYCLOCK: tap armed (NAMCOS23_TRACE_PLAYCLOCK=%s) - READ-ONLY per-vblank trace of (1) the op6F PLAY-CLOCK pair rec0 +0x390/+0x394 (0x802F43F0/F4; tick +1/frame @0x80014D3C-78 clamp 0x57E3F, gates gp+0x7570==0(0x802CED90)+bit29 clear; seeded -1 @0x80013E30; slave-ADOPTED from the wire @0x800B2504-08 when role byte 0x802F3FFC bit7 CLEAR; master EMITS @0x800B22CC once per 256 frames when bit7 SET + keepalive>0, sole caller 0x800B27CC) + rec1 +0x390/394 (0x802F4864/68, expected static) AND (2) the bit-0x0004 co-op/human gate on BOTH records' +0x370 (consumer 0x800136C4-D8 -> script-entity (6,1,0x1C); paired set/clear = script opcode 0x800BE200 operand!=0/==0; other clears: re-init 0x80013D44, helper 0x80013C54, op02 low24 mirror 0x800AB438); edge lines on bit04/role3ffc/clock-jump/rec1-clock/tick-run + 1/s PLAYCLOCK: status (clocks, ticks/60, bit04 both recs, role3ffc/op6fmaster, gate3502, ka, pause7570, framephase, op6f tx/rx counts); pair with device PLAYCLOCK: op6f-tx/rx by t=\n",
				playclock_env);

	// P026 PART 2 EXPERIMENT (branch patch/reasm-chunk-passthru): arm the
	// READ-ONLY phase9-validator trace from the environment, same gate idiom as
	// the others and independent of all patch gates.  Driver-only (the words it
	// watches are all MIPS main RAM); sampled per-vblank in vblank().
	// Experiment branch only - never merge to milestone.
	char const *const p9v_env = std::getenv("NAMCOS23_TRACE_PHASE9VAL");
	m_trace_phase9val = p9v_env && p9v_env[0] != '\0'
			&& !(p9v_env[0] == '0' && p9v_env[1] == '\0');
	m_p9v_have_prev = false;
	m_p9v_prev_link = 0;
	m_p9v_prev_chkfail = 0;
	m_p9v_prev_buf0 = 0;
	m_p9v_prev_buf1 = 0;
	m_p9v_win_drains = 0;
	m_p9v_win_chkfails = 0;
	if (m_trace_phase9val)
		logerror("PHASE9VAL: tap armed (NAMCOS23_TRACE_PHASE9VAL=%s) - READ-ONLY validator-layer trace; STEP-1 STATIC (full.txt): gp+0x75C8=0x802CEDE8 is a CHECKSUM-FAIL counter (sole increment 0x8000C004-0C, reached only when the drained slot's byte-sum!=0: compare andi 0x00ff @0x8000BFF8 + beqzl @0x8000BFFC jumps around it to jal 0x8000B8F0 on sum==0; zeroed only by link reset 0x8000C390; NO success counter exists - succ=0000 all run = ZERO chkfails, the phase-9d 'succ' label was wrong) and gp+0x75BC=0x802CEDDC is a TRI-STATE last-outcome word (0=validated @0x8000C064, 1=chkfail @0x8000C010, 2=timeout @0x8000C050/0x8000BB44 when drift 0x802F3504>=0x11, also clearing freshness 0x802F3502); events: 75BC/75C8 transitions + dispatch-buffer 0x802F3510 head change (= a drain; the validator's INPUT bytes, written pre-sum-test so failed drains show) + 1/s status; pair with CHUNK_PASSTHRU/KEEPALIVE_FLOOR/PLAYCLOCK by t=\n",
				p9v_env);

	// P033 EXPERIMENT (branch patch/compose-gate-trace, off patch/announce-latch):
	// arm the READ-ONLY bulk-compose-scheduler gate trace from the environment,
	// same gate idiom as the others and independent of all patch gates.
	// Driver-only (every watched word is MIPS main RAM); sampled per-vblank in
	// vblank().  Experiment branch only - never merge to milestone.
	char const *const cg_env = std::getenv("NAMCOS23_TRACE_COMPOSEGATE");
	m_trace_composegate = cg_env && cg_env[0] != '\0'
			&& !(cg_env[0] == '0' && cg_env[1] == '\0');
	m_cg_have_prev = false;
	m_cg_prev_rec0_370 = 0;
	m_cg_prev_rec1_370 = 0;
	m_cg_prev_screen = 0;
	m_cg_prev_phase = 0;
	m_cg_prev_done = 0;
	m_cg_prev_reqid = -1;
	m_cg_prev_len = 0;
	m_cg_prev_mode = 0;
	m_cg_prev_ka = 0;
	m_cg_prev_role = 0;
	m_cg_prev_bypass = 0;
	m_cg_win_bulkframes = 0;
	m_cg_win_reqframes = 0;
	m_cg_win_phase1 = 0;
	m_cg_win_maxlen = 0;
	if (m_trace_composegate)
		logerror("COMPOSEGATE: tap armed (NAMCOS23_TRACE_COMPOSEGATE=%s) - READ-ONLY bulk-compose scheduler gate trace; STEP-1 STATIC (full.txt, gp=0x802C7820): sole op4B/4C bulk-snapshot emitter 0x800AF84C, sole caller 0x800150D8 in phase-1 handler 0x80014F3C of the link phase machine (jalr 0x8022A750[gp+0x7024=0x802CE844]), run per-frame by the boundary TRANSFER SCREENS gp+0x756C=0x802CED8C: 0x13(0x8001541C)->0x14(0x80015550, phase machine ALWAYS) pre-armed @0x80016CC0 by round machine 0x80016B3C, or 0x15(0x800155EC)->0x16(0x80015710, phase machine IFF bypass gp+0x7036=0x802CE856==0 @0x80015718; armer 0x80015B80: in-game a0=0 @0x80012F5C, menu a0=1 @0x800238E4); phase 0 0x80014E50 holds on role 0x802F3FFC bit5 + countdown gp+0x7028=0x802CE848, ABORTS (mode<-1, ka<-0, done gp+0x7014=0x802CE834<-1) iff mode 0x802F3FD0!=2 @0x80014EA8 or keepalive 0x802F3FD8==0 @0x80014EB4 [cand (a)]; phase 1 serves peer op-0x1A requests gp+0x7020=0x802CE840 (latch 0x800151D0; requests emitted by peer 0x800B27A8 @0x800B289C for entities with 0x802F4DC8 word0 bit31 [cand (c)]), teardown @0x8001511C on watchdog gp+0x7018=0x802CE838<0 or idle gp+0x701C=0x802CE83C>=0x20 ending EVERY transfer with ka=0/mode=1 [cand (a)/(b)]; composed frame length 0x802F3910 (finalize 0x800AA76C, gate byte 0x802F3D12), >0xFF = bulk class (device expected_hw = len + L2 overhead); boundary marker = rec0/rec1 +0x370 low bits 0x40/0x20 (segment-phase machine 0x800131E8/0x80013214/0x8001324C [cand (b)]); events: COMPOSEGATE: boundary/phase/request/bulkframe/session + 1/s COMPOSEGATE: status; pair with device announce-latch/CHUNK_PASSTHRU + PLAYCLOCK/LINKBITS by t=\n",
				cg_env);

	// P034 EXPERIMENT (branch patch/roundend-trace, off patch/compose-gate-trace):
	// arm the READ-ONLY round-end trigger / join-precondition trace from the
	// environment, same gate idiom as the others and independent of all patch
	// gates.  Driver-only (every watched word is MIPS main RAM); sampled
	// per-vblank in vblank().  Experiment branch only - never merge to milestone.
	char const *const re_env = std::getenv("NAMCOS23_TRACE_ROUNDEND");
	m_trace_roundend = re_env && re_env[0] != '\0'
			&& !(re_env[0] == '0' && re_env[1] == '\0');
	m_re_have_prev = false;
	for (int i = 0; i < 2; i++)
	{
		m_re_prev_p370[i] = 0;
		m_re_prev_5c[i] = 0;
		m_re_prev_37a[i] = 0;
		m_re_prev_3a0[i] = 0;
		m_re_prev_3a2[i] = 0;
		m_re_prev_394[i] = 0;
	}
	m_re_prev_703c = 0;
	m_re_prev_7040 = 0;
	m_re_prev_705c = 0;
	m_re_prev_7074 = 0;
	m_re_prev_7034 = 0;
	m_re_prev_7624 = 0;
	m_re_prev_role = 0;
	m_re_prev_7608 = 0;
	m_re_prev_75f8 = 0;
	m_re_prev_6ff4 = 0;
	m_re_prev_coop = 0;
	m_re_prev_74f0lo = 0xff;
	if (m_trace_roundend)
		logerror("ROUNDEND: tap armed (NAMCOS23_TRACE_ROUNDEND=%s) - READ-ONLY round-end trigger trace; STEP-1 STATIC (full.txt, gp=0x802C7820): round703c 0->1 writer = 0x80017074 in state-0 handler 0x80017024 (table 0x8022A78C [0..5] = 0x80017024/0x800166D4/0x8001685C/0x80016938/0x80017090/0x800170BC), UNCONDITIONAL once the round machine 0x80016B3C runs - and that runs (sole caller 0x80013530 in dispatcher 0x800134A0) ONLY while LOCAL record +0x370 bit 0x2000 is set (LOCAL = 0x802F4060 + gp+0x7608*0x474; gp+0x7608 stays 0 on BOTH cabs - writers are sw $0 x6 + menu toggle 0x8002A6B8 - so only RECORD 0 can trigger); sole 0x2000 setter 0x800166B0-BC (fn 0x80016698, also 703C/7040/7044<-0), gate @0x800166AC bit31==(idx!=0); sole caller = ROUND-END CHECKER 0x80026E00 @0x80026EC4, gates A +0x370&0x40010800==0 @0x80026E20, B +0x5C bit0 @0x80026E30 (spawn-template flag), C one-shot +0x37A bit0 clear @0x80026E40 (SET @0x80026E48 on first passing call - gate-E failure BURNS it until 0x80014928 clears), D bit31/idx @0x80026E60, E +0x3A0 pre-dec <= 0 @0x80026EB8 (init byte gp+0x75E4; =1 fire-now wrapper 0x800D1668); callers = script triggers 0x800D5AC4/0x800DF324 (positional) + 0x800793CC/0x8007943C/0x80079ABC/0x80079BE8/0x800E9F34/0x800E9FC4 (0x40-marker-gated) + WATCHDOG 0x80014BA8 (@0x80014D8C per frame unless 0x2000 set): +0x5C bit0 && gp+0x75F8 bit2 clear && +0x3A2--==0 (area TIME LIMIT, re-arm @0x80014DEC desc[9]*60 frames, default 0x960=40 s) && +0x37A bit1 clear; LOCAL 0x6000 = 0x2000(entry) + 0x4000 (tail 0x80016DB8-C0 when gp+0x705C==0; state-0 sets 705C=1 @0x80017044, state-1 clears @0x80016724 after 7068 > 0x46 = 71 frames); 7074=2 iff mode==2 && ka>0 && PARTNER +0x370&0x6000==0x6000 @0x80016B5C-BCC (derived only when the round machine runs); JOIN armer 0x80015B80 in-game path: 7034==0, role bit1 @0x80015BAC (NO writer ever sets bit1 - 6-store census - statically dead = G10), coop (rec0|rec1)&0x04, 7624!=0 gates the 0x15 arm; SEGMENT RE-BASE entry 0x80104958 (0x80104980 is mid-function), zeroes rec0+394/388 AND rec1+394/388 same frame (fingerprint), callers 0x80104B8C/0x80104C34 on object +0x18 bit27; op6F 0x800B22CC (sole site 0x800B27CC, unguarded): emit iff gp+0x74F3==0 (frame ctr low byte - 1/256 frames) && ka>0 && role bit7; events: ROUNDEND: round/mode2000/checker/flags5c/wd3a2/rebase/join/op6fwin/idx + 1/s ROUNDEND: status; pair with COMPOSEGATE/PLAYCLOCK/LINKBITS by t=\n",
				re_env);

	// P036 EXPERIMENT (branch patch/round-arm, off patch/latch-v2-snapshot):
	// arm the ROUND-ARM assist from the environment, same gate idiom as the
	// others and independent of all trace/patch gates.  Driver-only; applied
	// per-vblank in vblank() AFTER the P034 ROUNDEND tap so a stacked-trace
	// run samples the pristine pre-write state first.  Experiment branch only
	// - never merge to milestone.
	char const *const ra_env = std::getenv("NAMCOS23_PATCH_ROUNDARM");
	m_patch_roundarm = ra_env && ra_env[0] != '\0'
			&& !(ra_env[0] == '0' && ra_env[1] == '\0');
	m_ra_prev_p370 = 0;
	m_ra_prev_own_sig = false;
	m_ra_prev_703c = 0;
	m_ra_armed = false;
	m_ra_armed_frame = 0;
	m_ra_wait_reentry = false;
	m_ra_gap_frames = 0;
	m_ra_fire_pending = false;
	m_ra_fire_frame = 0;
	m_ra_cnt_armed = 0;
	m_ra_cnt_fired = 0;
	m_ra_cnt_entered = 0;
	m_ra_cnt_skipped = 0;
	m_ra_cnt_overwritten = 0;
	if (m_patch_roundarm)
		logerror("ROUNDARM: patch armed (NAMCOS23_PATCH_ROUNDARM=%s) - round-arm assist, the FIRST functional game-RAM write beyond the P001 keepalive floor: ONE halfword rec0+0x3A0 (hi lane of 0x802F4400; +0x3A2 watchdog lo lane preserved bit-exactly) <- 1 = the ROM's own fire-now idiom (wrapper 0x800D1668 @0x800D16A8) planted at the score/result window EXIT so the next NATURAL checker call 0x80026E00 pre-decrements 1->0, passes gate E and enters round-end through 100%% stock paths (0x800166B0 entry -> round machine walk -> 0x6000 advertise @71 frames -> mutual 7074==2 -> transfer screens -> op4B/4C serves -> segment re-base); window signature = OWN rec0+0x370 (0x802F43D0)&0xC48168E0==0x04010040 (bit26 result-class + bit16 + 0x40 marker set; mirror-marks/bit23-strobe/0x6000/0x800/0x80-trigger/0x20-engaged clear) AND MIRROR rec1+0x370 (0x802F4844)&0x048168E0==0x04010040 (partner at result too - an own-only mask provably cannot separate blue score 06118052 from blue walking 06018042, its strict subset) while mode 0x802F3FD0==2 && ka 0x802F3FD8>0 && idx gp+0x7608==0; FIRE on the rec0 bit16-clear edge iff round703c 0x802CE85C==0 && mode==2 && ka>0 && oneshot rec0+0x37A==0000 && post-edge (p370&0xC0006800)==0 && cnt3a0 in [2,15] (==1 natural no-write, <=0 checker-firing, >15 insane); RAILS: one-shot per window (re-arm only after full-signature re-entry with >=60 absent frames), 300-frame edge-timeout disarm, NON-RETRY on overwrite (cnt3a0>1 while round703c==0 after a fire = logged once, never rewritten), no other address ever written; events ROUNDARM: armed/fired/skipped/disarmed/entered/overwritten + 1/s ROUNDARM: status while mode==2; pair with ROUNDEND:/COMPOSEGATE:/LINKBITS: by t=\n",
				ra_env);

	// P020 EXPERIMENT (branch patch/linked-gate-supply, off
	// patch/op70-linked-gate-arm): arm the FORCING 0x6000 supply from the
	// environment, same gate idiom as the others and independent of all patch
	// gates.  Applied per-vblank in vblank() (BEFORE the LINKBITS trace block).
	// Experiment branch only - never merge to milestone.
	char const *const linked_gate_env = std::getenv("NAMCOS23_PATCH_LINKED_GATE");
	m_patch_linked_gate = linked_gate_env && linked_gate_env[0] != '\0'
			&& !(linked_gate_env[0] == '0' && linked_gate_env[1] == '\0');
	m_lg_set_count = 0;
	if (m_patch_linked_gate)
		logerror("LINKED_GATE: armed (NAMCOS23_PATCH_LINKED_GATE=%s) - every vblank while mode word 0x802F3FD0 == 2, OR 0x6000 into the LOCAL record +0x370 word (0x802F43D0+localidx*0x474, localidx=gp+0x7608=0x802CEE28); SUPPLY only (no gp+0x7074/partner/op-70 write) so the ROM op55 builder carries it to the peer -> peer partner6000=1 -> gp+0x7074=2 -> op-70; OR-only, mode-2-bounded\n",
				linked_gate_env);

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off
	// patch/linked-gate-supply): arm the WIRE-ONLY 0x6000 advertise from the
	// environment.  INDEPENDENT of NAMCOS23_PATCH_LINKED_GATE (run TXONLY set +
	// LINKED_GATE unset).  Same "non-empty and not literal 0" gate idiom.  The
	// driver pushes (armed && staging_mode==2) to the device each vblank; the
	// device does the OR-only op55 wire-flag injection in emit_tx_frame.  No RAM
	// write here - the LOCAL +0x370 record is left untouched (the P021 invariant).
	// Experiment branch only - never merge to milestone.
	char const *const linked_gate_txonly_env = std::getenv("NAMCOS23_PATCH_LINKED_GATE_TXONLY");
	m_patch_linked_gate_txonly = linked_gate_txonly_env && linked_gate_txonly_env[0] != '\0'
			&& !(linked_gate_txonly_env[0] == '0' && linked_gate_txonly_env[1] == '\0');
	if (m_patch_linked_gate_txonly)
		logerror("LINKED_GATE_TXONLY: armed (NAMCOS23_PATCH_LINKED_GATE_TXONLY=%s) - every vblank while mode word 0x802F3FD0 == 2, the device ORs 0x6000 into the op55 24-bit WIRE flags (cells[op55+6] |= 0x60) IN THE OUTGOING FRAME COPY (NOT game RAM); the peer's op55 RX 0x800B058C lands it into ITS partner record +0x370 (0x80013E10) -> peer partner6000=1 -> gate7074 1->2, while this cab's LOCAL game RAM +0x370 (0x802F43D0) is NEVER modified (gun-actor 0x80013644 stays human, no death derail). INDEPENDENT of NAMCOS23_PATCH_LINKED_GATE. Pair LINKED_GATE_TXONLY: inject/status with device LINKBITS: txflags has6000\n",
				linked_gate_txonly_env);

	// P024 EXPERIMENT (branch patch/op55-carrier-repeat, off
	// patch/linked-gate-tx-only): arm the PARTNER-RECORD STICKY LATCH from the
	// environment.  STACKS on top of (and is INDEPENDENT of) the P021 TXONLY wire
	// advertise, which stays armed.  Same "non-empty and not literal 0" gate idiom.
	// P021 proved the wire advertise lands in the partner record but only ~1/scene;
	// this makes it PERSIST by re-OR-ing 0x6000 into the PARTNER record +0x370 every
	// mode-2 vblank.  Pre-step (gold disasm) proved partner 0x2000 is SAFE (the gun-
	// actor 0x80013644 / tick 0x80014CC0 / dispatcher 0x800134A0 read 0x2000 on the
	// LOCAL record only), so this writes ONLY the partner record - the LOCAL record
	// (and thus the local gun) is NEVER touched.  Experiment branch only.
	char const *const op55_repeat_env = std::getenv("NAMCOS23_PATCH_OP55_REPEAT");
	m_patch_op55_repeat = op55_repeat_env && op55_repeat_env[0] != '\0'
			&& !(op55_repeat_env[0] == '0' && op55_repeat_env[1] == '\0');
	m_or_partner6000_count = 0;
	if (m_patch_op55_repeat)
		logerror("OP55_REPEAT: armed (NAMCOS23_PATCH_OP55_REPEAT=%s) - every vblank while mode word 0x802F3FD0 == 2, OR 0x6000 into the PARTNER record +0x370 word (0x802F43D0+(1-localidx)*0x474, localidx=gp+0x7608=0x802CEE28) so the gate 0x80016BB4-BCC sees PERSISTENT partner 0x6000 -> gp+0x7074 latches 1->2 and the b26 partner render STAYS drawn. PARTNER record only: the LOCAL record (0x802F43D0+localidx*0x474) is NEVER written (gun-actor 0x80013644 stays human, no death derail; partner-0x2000 is the ROM-intended remote-networked state per op55 RX 0x80013E10). OR-only, mode-2-bounded, idempotent. STACKS on NAMCOS23_PATCH_LINKED_GATE_TXONLY. Pair OP55_REPEAT: latch/status with LINKBITS: status partner6000/gate7074 + OP70: status\n",
				op55_repeat_env);

	// P040 EXPERIMENT (branch patch/op6f-394-clamp, off patch/latch-v3-dedupe):
	// arm the op6F SEGMENT-CLOCK ADOPTION STORE FILTER from the environment.
	// Same "non-empty and not literal 0" gate idiom.  Full mechanism, PC
	// reliability analysis and scope limits live in the member-block comment;
	// the DRC resolution (set_force_no_drc) lives in timecrs2(machine_config&)
	// because mips3 latches m_isdrc in device_start(), before this runs.
	// NOTE on the phase-9d comment below: "add_fastram bypasses
	// install_write_tap" holds for the base-class TLB mips3 cores, whose
	// accessors translate to PHYSICAL and consult m_fastram first - but the
	// R4650 used here OVERRIDES the interpreter accessors (mips3.cpp 1541-1711)
	// and in kernel mode passes the raw virtual address straight to the
	// program space (never touching fastram), and the r4650 DRC accessor's
	// fastram compare runs on the raw kseg0 VIRTUAL address (0x802Fxxxx),
	// which can never match the PHYSICAL range (0, mainram.bytes()-1)
	// registered above - so on THIS driver the tap fires under both cores.
	char const *const op6f_no394_env = std::getenv("NAMCOS23_PATCH_OP6F_NO394");
	m_patch_op6f_no394 = op6f_no394_env && op6f_no394_env[0] != '\0'
			&& !(op6f_no394_env[0] == '0' && op6f_no394_env[1] == '\0');
	m_no394_blocked = 0;
	m_no394_blocked_win = 0;
	m_no394_pass394 = 0;
	m_no394_pass394_win = 0;
	m_no394_adopt_edges = 0;
	m_no394_have_prev = false;
	m_no394_prev_394 = 0;
	if (m_patch_op6f_no394)
	{
		// THE FILTER: write tap on the ONE word rec0+0x394.  Virtual
		// 0x802F43F4 -> program-space 0x002F43F4 (map.global_mask(0xfffffff)
		// strips the kseg bits at dispatch).  The tap edits only the in-flight
		// data (never writes memory -> no recursion), passes debugger pokes
		// untouched, and stays installed across soft reset / save-state load.
		m_op6f_no394_tap = m_maincpu->space(AS_PROGRAM).install_write_tap(
				0x002f43f4, 0x002f43f7, "op6f_no394",
				[this](offs_t offset, u32 &data, u32 mem_mask)
				{
					if (machine().side_effects_disabled())
						return;
					// On this core pc() and pcbase() BOTH read m_core->pc
					// (MIPS3_PC == STATE_GENPC and STATE_GENPCBASE both map
					// there; m_ppc is not exported), and the interpreter
					// advances it BEFORE executing the op - so at store
					// time it reads store-PC+4 = 0x800B250C (the sw is not
					// in a delay slot).  Compare against the containing-
					// function range of the adoption receiver 0x800B2448
					// (task-specified upper bound 0x800B2520; the body
					// actually ends @0x800B2510 jr+slot - the two extra
					// instructions belong to the next function and never
					// store to this word), which absorbs the +4 offset.
					u32 const curpc = u32(m_maincpu->pc());
					if (curpc >= 0x800b2448 && curpc <= 0x800b2520)
					{
						u32 const cur394 = m_mainram[0x002f43f4 / 4];
						u32 const cur390 = m_mainram[0x002f43f0 / 4];
						u32 const wouldbe = data;
						data = cur394; // store lands but changes nothing = DROPPED
						++m_no394_blocked;
						++m_no394_blocked_win;
						logerror("OP6F_NO394: blocked t=%.6f wouldbe=%08x cur394=%08x cur390=%08x pc=%08x mask=%08x total=%u (op6F 394-adoption sw@0x800B2508 dropped, expect pc=0x800B250C; +0x390 adoption NOT touched)\n",
								machine().time().as_double(), wouldbe, cur394, cur390,
								curpc, mem_mask, m_no394_blocked);
					}
					else
					{
						// tick +1 (0x80014D64/78) / re-base 0 (0x80104980) /
						// init -1 (0x80013E34) / Handler-A restore
						// (0x801046A0) / session-sync ingest (0x800B08F4):
						// pass untouched, count for the 1/s status line.
						++m_no394_pass394;
						++m_no394_pass394_win;
					}
				});
		logerror("OP6F_NO394: filter armed (NAMCOS23_PATCH_OP6F_NO394=%s) - write tap on rec0+0x394 segment clock (virtual 0x802F43F4 = program-space 0x002F43F4, 32-bit word 0x002F43F4-F7); DROP stores whose pc lies in [0x800B2448,0x800B2520] = the op6F clock-adoption receiver (sole 394 store = sw $a2,0x394($v0) word 0xAC460394 @0x800B2508; expect observed pc=0x800B250C = store-PC+4, interpreter pre-advances and GENPC==GENPCBASE==m_core->pc on mips3); all other +0x394 writers pass; +0x390 adoption @0x800B2504 untapped (benign, P039-verified consumers); MAIN CPU FORCED TO INTERPRETER at config time (set_force_no_drc - DRC pc is block-stale and would fail open); slave-only mechanism (master's bnez @0x800B24D0 skips the stores) -> expected blocked: master 0, slave >=1 per DELIVERED op6F (incl. former equal-pair no-op adoptions invisible to the jump census - the count doubles as the true delivery-rate instrument P039 lacked; the P038 run showed 2 by-effect); falsifier: OP6F_NO394 adopt-edge lines MUST stay 0; tap-aliveness rail: status pass394 ticks ~+60/s in-game (the per-frame tick writer 0x80014D64/78) - +0 while in-game = tap dead = filter fail-open\n",
				op6f_no394_env);
	}

	// P040b EXPERIMENT (same branch - MECHANISM AMENDMENT of P040): arm the
	// verify-before-poke RAM-code NOP from the environment.  Same "non-empty
	// and not literal 0" gate idiom.  The poke itself is DEFERRED to vblank:
	// at machine_start the boot loader has not yet copied the main program
	// into RAM, so the target word still reads 0.  NO set_force_no_drc and
	// NO write tap - the whole point of the amendment is full DRC speed (the
	// v1 filter env above stays functional and independent; arming both is
	// harmless but redundant).  Full mechanism + the verified DRC
	// invalidation analysis live in the member-block comment.
	// P067 (patch/defaults-on): ADOPTED - armed by DEFAULT;
	// NAMCOS23_PATCH_OP6F_NO394B=0 is the kill switch (fully inert, no poke
	// ever - the pre-P067 unset path).
	bool op6f_no394b_from_env = false;
	char const *const op6f_no394b_val = patch_env_or_default("NAMCOS23_PATCH_OP6F_NO394B", "1", op6f_no394b_from_env);
	m_patch_op6f_no394b = op6f_no394b_val != nullptr;
	m_no394b_poked = false;
	m_no394b_refused = false;
	m_no394b_repokes = 0;
	if (!m_patch_op6f_no394b)
		logerror("OP6F_NO394B: DISABLED (NAMCOS23_PATCH_OP6F_NO394B=0 kill switch - adopted default overridden; stock +0x394 adoption, no poke)\n");
	if (m_patch_op6f_no394b)
		logerror("OP6F_NO394B: armed (NAMCOS23_PATCH_OP6F_NO394B=%s %s) - verify-before-poke RAM-code NOP, FULL-SPEED DRC (no forced interpreter, no write tap): each vblank until applied, read u32 @0x800B2508 (program-space 0x000b2508 under global_mask; System 23 executes the main program from RAM - the boot loader copies it in early boot, the word reads 0 until then); on 0xAC460394 (sw $a2,0x394($v0) - the sole +0x394 store of the op6F adoption receiver 0x800B2448) poke 0x00000000 = MIPS NOP, one-shot; on any OTHER nonzero word REFUSE one-shot (ROM-revision guard, stay inert); +0x390 adoption @0x800B2504 untouched. DRC soundness (verified in mips3drc.cpp, see member comment): the receiver's block first executes at link-up ~56 s+ and is compiled lazily from the then-already-NOPed RAM; mips3 has NO store-triggered invalidation and default loose verify checks only sequence-head words, so the guards are the 1/s word re-check (re-poke + repokes counter, expect 0) and the shared adopt-edge falsifier (MUST stay 0). Expect: poked banner x1 in early boot; status word2508=00000000 thereafter; blocked/pass394 stay 0 under NO394B-only (no tap - word2508 is the aliveness rail); full ~60 fps\n",
				op6f_no394b_val, patch_env_src(op6f_no394b_from_env, op6f_no394b_val).c_str());

	// P041 EXPERIMENT (branch patch/op6f-390-clamp, off patch/op6f-394-clamp):
	// arm the +0x390 PLAY-CLOCK adoption NOP from the environment - the other
	// half of the P040b pair (full rationale, the verified instruction word
	// and the falsifier spec live in the member-block comment).  Same
	// "non-empty and not literal 0" gate idiom; the poke itself is DEFERRED
	// to vblank (at machine_start the boot loader has not yet copied the
	// program into RAM, so the word reads 0).  NO set_force_no_drc, NO write
	// tap - full DRC speed, exactly like P040b.
	// P067 (patch/defaults-on): ADOPTED - armed by DEFAULT;
	// NAMCOS23_PATCH_OP6F_NO390B=0 is the kill switch (fully inert, no poke
	// ever - the pre-P067 unset path).
	bool op6f_no390b_from_env = false;
	char const *const op6f_no390b_val = patch_env_or_default("NAMCOS23_PATCH_OP6F_NO390B", "1", op6f_no390b_from_env);
	m_patch_op6f_no390b = op6f_no390b_val != nullptr;
	m_no390b_poked = false;
	m_no390b_refused = false;
	m_no390b_repokes = 0;
	m_no390b_jump390s = 0;
	m_no390b_have_prev = false;
	m_no390b_prev_390 = 0;
	if (!m_patch_op6f_no390b)
		logerror("OP6F_NO390B: DISABLED (NAMCOS23_PATCH_OP6F_NO390B=0 kill switch - adopted default overridden; stock +0x390 adoption, no poke)\n");
	if (m_patch_op6f_no390b)
		logerror("OP6F_NO390B: armed (NAMCOS23_PATCH_OP6F_NO390B=%s %s) - verify-before-poke RAM-code NOP of the op6F +0x390 PLAY-CLOCK adoption store, FULL-SPEED DRC (no forced interpreter, no write tap): each vblank until terminal, read u32 @0x800B2504 (program-space 0x000b2504 under global_mask; the word reads 0 until the boot loader copies the program); on 0xAC470390 (sw $a3,0x390($v0) - the sole +0x390 store of the op6F adoption receiver 0x800B2448, VERIFIED in full.txt, one instruction before the P040b 394 store @0x800B2508) poke 0x00000000 = MIPS NOP, one-shot; on any OTHER nonzero word REFUSE one-shot (ROM-revision guard, stay inert). Blocks the OTHER HALF of the P040b half-adoption pair-split (blue's 390-394 skew sawtooth -9..+1, six +3..+12 play-clock snaps + 11 grid tick-stops = the measured gameplay-jitter owner; red's pair never splits) - both halves NOPed restores LOCAL pair consistency for P039's consumers (total-time scoring, Handler-B draw, best-flags); accepted cost = bounded cross-cab play-clock skew (<1 s per 5 min from the slave's ingest-pause tick losses). Guards: 1/s word re-check (re-poke + repokes counter, expect 0) and the OP6F_NO390B 390-jump census falsifier (any positive in-game rec0_390 jump >2 MUST stay 0; the PLAYCLOCK clock-jump rec0_390 line is the independent cross-check). The 394 rails (adopt-edge falsifier + NO394B status) are UNTOUCHED. Expect: poked banner x1 early boot; status word2504=00000000 jump390s=0 thereafter; structurally inert on the master (its role gate bnez @0x800B24D0 skips both stores anyway)\n",
				op6f_no390b_val, patch_env_src(op6f_no390b_from_env, op6f_no390b_val).c_str());

	// P043 EXPERIMENT (branch patch/wave-init-hold, off patch/op6f-390-clamp):
	// arm the WAVE-INIT HOLD poke pair from the environment - same "non-empty
	// and not literal 0" gate idiom, independent of every env above.  Both
	// pokes are DEFERRED to vblank (at machine_start the boot loader has not
	// yet copied the program into RAM, so both words read 0).  Full race map,
	// both VERIFIED instruction words and the fail-to-stock analysis live in
	// the member-block comment.
	char const *const waveinit_hold_env = std::getenv("NAMCOS23_PATCH_WAVEINIT_HOLD");
	m_patch_waveinit_hold = waveinit_hold_env && waveinit_hold_env[0] != '\0'
			&& !(waveinit_hold_env[0] == '0' && waveinit_hold_env[1] == '\0');
	m_wih_win_poked = false;
	m_wih_win_refused = false;
	m_wih_win_repokes = 0;
	m_wih_clamp_poked = false;
	m_wih_clamp_refused = false;
	m_wih_clamp_repokes = 0;
	if (m_patch_waveinit_hold)
		logerror("WAVEINIT_HOLD: armed (NAMCOS23_PATCH_WAVEINIT_HOLD=%s) - TWO verify-before-poke RAM-code word patches closing the P042 wave-init completion race (condition B of per-frame completion check 0x80014074: ring lookup 0x800B5690(rec+0x364) opened 10 frames BEFORE wave activation 0x800A0E5C = the measured 0.17 s skip margin). Each vblank until terminal, INDEPENDENT one-shots (a REFUSE on one does not block the other): (1) WINDOW @0x8001417C on 0x2842000B (slti $v0,$v0,0xB - VERIFIED in full.txt) poke 0x28420001 (slti $v0,$v0,1: walk-phase check held until end-cursor<1; fight areas activate at end-cursor==1 first, walk-only advance keeps its id==-1 path at end-cursor<=0, <=0.17 s later than stock); (2) CLAMP @0x800B56E0 on 0x2402FFFE (li $v0,-2 - VERIFIED in full.txt) poke 0x24020000 (li $v0,0: ring-lookup not-found verdict neutral - kills the beta/never-registered flavor incl. fight-phase boss/stage-2 instant-resolves; sole caller of 0x800B5690 = condition B @0x8001418C); on any OTHER nonzero word REFUSE that poke one-shot (ROM-revision guard). DRC soundness by P040b ordering (pokes land ~frame 11, the check first executes at attract gameplay); residual re-copy windows FAIL TO STOCK only - guards: 1/s combined re-check/re-poke + status with both word rails (expect word1417c=28420001 wordb56e0=24020000) + the NAMCOS23_TRACE_WAVEINIT flavor trace. Both cabs (the race is role-symmetric). Prediction: room-1 PLAYS in late-link-up sessions (0/7 on record), zero walk-window bit16, boss instant-resolves collapse; 40 s watchdog +0x3A2 still backstops legit-completion losses\n",
				waveinit_hold_env);

	// P043 trace: arm the READ-ONLY wave-anchor / ring observability trace
	// from the environment - independent of the patch env (either works
	// alone; the combined run arms both).  Driver-only (every watched word is
	// MIPS main RAM); sampled per-vblank in vblank(), per-event lines only.
	char const *const waveinit_trace_env = std::getenv("NAMCOS23_TRACE_WAVEINIT");
	m_trace_waveinit = waveinit_trace_env && waveinit_trace_env[0] != '\0'
			&& !(waveinit_trace_env[0] == '0' && waveinit_trace_env[1] == '\0');
	m_wt_have_prev = false;
	for (int i = 0; i < 2; i++)
	{
		m_wt_prev_p370[i] = 0;
		m_wt_prev_5c[i] = 0;
		m_wt_prev_endcur[i] = 0;
		m_wt_engage_t[i] = -1.0;
		m_wt_engage_anchor[i] = 0xffffffff;
		m_wt_win11_t[i] = -1.0;
		m_wt_win1_t[i] = -1.0;
	}
	if (m_trace_waveinit)
		logerror("WAVEINIT: trace armed (NAMCOS23_TRACE_WAVEINIT=%s) - READ-ONLY wave-anchor/ring trace, per-event lines only: 'WAVEINIT: bit16' on every p370 bit16 0->1 edge (both records; dumps f5c word, timeline cursor/end/end-cursor, anchor id rec+0x364, big-map view rec+0x368, the full 4-slot ring 0x802D2030 (id:handle x4) + rotation cursor gp+0x73E0, an EMULATED 0x800B5690 lookup ((cursor+a1)&3, a1=3..0, first match -> lh handle; id==-1 -> -1; none -> -2) and the P042 poison-flavor verdict: hookA-conditionA (bit17 same store) / gamma-rearm (anchor changed since engage) / no-id-walkclass (-1) / beta-never-registered (-2) / alpha-released-handle (found, handle<0) / healthy-unexplained (MUST not appear)); 'WAVEINIT: engage-cycle' once per activation f5c bit0 rise (engage_t = p370 bit5 rise, win11_t = stock window entry end-cursor<11 in walk, win1_t = narrowed entry <1, act_t, anchor, bit16_now - bit16_now=1 = completion PRE-LATCHED in the walk = the skip signature, expect 0 with the pokes armed). Healthy choreography: engage -> win11 +0.35 -> activation +0.50\n",
				waveinit_trace_env);

	// P044 EXPERIMENT (branch patch/anchor-lifecycle, off patch/wave-init-hold):
	// arm the three ANCHOR-LIFECYCLE gates from the environment - same
	// "non-empty and not literal 0" idiom, each independent of every env above
	// and of each other (any subset works; all inert unset).  Full design +
	// the verified ring facts live in the member-block comment.  The device
	// (namco_c139) reads NAMCOS23_TRACE_OP20 itself in device_start for the
	// wire half (op-0x20/op-0x1F cell detection).
	char const *const op20_trace_env = std::getenv("NAMCOS23_TRACE_OP20");
	m_trace_op20 = op20_trace_env && op20_trace_env[0] != '\0'
			&& !(op20_trace_env[0] == '0' && op20_trace_env[1] == '\0');
	char const *const anchor_shield_env = std::getenv("NAMCOS23_PATCH_ANCHOR_SHIELD");
	m_patch_anchor_shield = anchor_shield_env && anchor_shield_env[0] != '\0'
			&& !(anchor_shield_env[0] == '0' && anchor_shield_env[1] == '\0');
	// P067 (patch/defaults-on): ANCHOR_RESURRECT ADOPTED - armed by DEFAULT;
	// NAMCOS23_PATCH_ANCHOR_RESURRECT=0 is the kill switch (fully inert, no
	// scrub ever - the pre-P067 unset path).  ANCHOR_SHIELD stays opt-in
	// (retired).  The WAVEINIT_HOLD safety interlock below still applies to
	// the default-armed state (a user arming the retired WAVEINIT_HOLD gets
	// the loud REFUSE, resurrect disarmed).
	bool anchor_resurrect_from_env = false;
	char const *const anchor_resurrect_val = patch_env_or_default("NAMCOS23_PATCH_ANCHOR_RESURRECT", "1", anchor_resurrect_from_env);
	m_patch_anchor_resurrect = anchor_resurrect_val != nullptr;
	if (!m_patch_anchor_resurrect)
		logerror("ANCHOR_RESURRECT: DISABLED (NAMCOS23_PATCH_ANCHOR_RESURRECT=0 kill switch - adopted default overridden; born-dead ring slots left as stock)\n");
	m_anl_have_prev = false;
	for (int s = 0; s < 4; s++)
	{
		m_anl_prev_ringid[s] = 0;
		m_anl_prev_ringh[s] = -1;
	}
	for (int i = 0; i < 2; i++)
	{
		m_anl_prev_anchor[i] = 0xffffffff;
		m_anl_prev_desc[i] = 0;
		m_ash_live_handle[i] = -1;
		m_ash_live_id[i] = 0xffffffff;
		m_ash_repairs_cycle[i] = 0;
		m_ash_yielded[i] = false;
		m_ash_missed_logged[i] = false;
	}
	m_o20_kills = 0;
	m_o20_regs = 0;
	m_o20_revives = 0;
	m_o20_rewrites = 0;
	m_o20_borndead = 0;
	m_o20_unreg = 0;
	m_o20_healthy = 0;
	m_ash_repairs = 0;
	m_ash_yields = 0;
	m_ash_missed = 0;
	m_ars_scrubs = 0;
	// SAFETY INTERLOCK: the retired WAVEINIT_HOLD clamp poke rewrites the ring
	// lookup's not-found verdict -2 -> 0 ("alive") - under it a scrubbed slot
	// (id blanked -> lookup not-found) would read ALIVE forever and areas could
	// never complete by anchor death.  The scrub is only sign-neutral against
	// the STOCK lookup, so refuse to arm RESURRECT alongside WAVEINIT_HOLD.
	if (m_patch_anchor_resurrect && m_patch_waveinit_hold)
	{
		m_patch_anchor_resurrect = false;
		logerror("ANCHOR_RESURRECT: REFUSED (NAMCOS23_PATCH_WAVEINIT_HOLD is armed - its retired not-found clamp poke would make scrubbed slots read ALIVE forever; unarm WAVEINIT_HOLD to use the scrub - it is retired from the recipe anyway)\n");
	}
	if (m_trace_op20)
		logerror("OP20: driver armed (NAMCOS23_TRACE_OP20=%s) - READ-ONLY wave-anchor RING-TRANSACTION trace, per-event + 1/s status: per-vblank diff of the 4-slot ring @0x802D2030 -> 'OP20: ring-kill' (handle live->dead; matchrec0/1 = which record's rec+0x364 the id equals, walk0/1 = that record's f5c bit0 clear, endcur inline - join the device 'OP20: rx-release' lines by t= to name INGESTED kills vs local releases) / 'ring-reg' (fresh registration) / 'ring-revive' (FALSIFIER - no stock path, expect 0) / 'ring-rewrite'; plus the commit-edge classifier on any rec+0x35C/+0x364 change: 'born-dead-commit' (find hit a stale DEAD slot - the 0x800B5710 no-resurrect edge, P043b class i, the cascade), 'commit-unregistered' (absent from all slots - beta fuel), healthy commits counted silently. Purpose: NAME the class-(ii) walk-phase killer (predicted: op-0x20 ingest echo of the round-start cull/avatar burst) and measure sub-class shares for the shield/resurrect arms. Vblank granularity: same-frame kill+re-register collapses to one edge\n",
				op20_trace_env);
	if (m_patch_anchor_shield)
		logerror("ANCHOR_SHIELD: armed (NAMCOS23_PATCH_ANCHOR_SHIELD=%s) - walk-phase wave-anchor death repair (P043b class ii - room-1: anchor dead within 1.33 s of seed on BOTH cabs): own records only (p370 bit30 clear), anchor != -1, f5c bit0 CLEAR (wave not yet active = no legitimate kill can target it); captures the anchor's LIVE ring handle each vblank and, if the slot goes dead mid-walk (ingested op-0x20 echo / local cull), RESTORES the captured handle into slot+4 (halfword lane; only into a slot still carrying OUR id) so the completion check's window-open lookup sees a live anchor and the empty completion never latches; the entity's release mark is NOT undone and NO re-kill is applied at activation (re-applying = the imm=2 outcome P043b proved worthless). P025 log-and-yield: max 8 repairs per walk cycle then 'ANCHOR_SHIELD: yield'. Born-dead slots (never live this walk) are logged 'missed' and left to ANCHOR_RESURRECT. Lines: repair/yield/missed/cycle-end + 1/s status. Writes DATA only (ring halfword; never code - no DRC concern). Expect: repairs on race-window kills, cycle-end bit16=0 (walk survived), room-1 PLAYS\n",
				anchor_shield_env);
	if (m_patch_anchor_resurrect)
		logerror("ANCHOR_RESURRECT: armed (NAMCOS23_PATCH_ANCHOR_RESURRECT=%s %s) - the born-dead fix (P043b class i, the 80264ef8 cascade: ONE dead ring slot re-latched across 4 successive commits): per vblank, any ring slot with handle<0 (GUARD: live handles NEVER touched) and a real id (not 0 init / not ffffffff already-scrubbed) gets its ID word blanked to ffffffff = 'OP20: born-dead made impossible' - the next find-or-create of that anchor id MISSES (both ROM find loops fast-path id==-1 and so can never match ffffffff - VERIFIED @0x800B5690/0x800B56EC-5748) and CREATES+REGISTERS a fresh entity into the still-free slot (registration @0x800B5808-38 scans handle<0 only, id irrelevant - VERIFIED); verdict-neutral for the completion check (found-dead -1 and not-found -2 are both negative; the check is sign-only @0x80014194-98) so the designed last-kill->complete mechanism is untouched; the current already-doomed cycle still completes empty - the scrub fixes the NEXT commit (exactly the cascade class). Lines: 'ANCHOR_RESURRECT: scrub' per blanked slot + 1/s status; falsifier = 'OP20: born-dead-commit' MUST collapse toward 0. Writes DATA only. INCOMPATIBLE with the retired WAVEINIT_HOLD clamp (refused above if both set)\n",
				anchor_resurrect_val, patch_env_src(anchor_resurrect_from_env, anchor_resurrect_val).c_str());

	// P055 EXPERIMENT (branch patch/boat-jitter-trace): arm the READ-ONLY per-vblank
	// BLUE entity-ingest trace from the environment - same "non-empty and not
	// literal 0" idiom.  The device (namco_c139) reads the SAME env var in its
	// device_start for the FRESH/REPLAY classifier + rx-apply generation this trace
	// consumes.  Full design + the Phase-1 RE finding live in the member-block
	// comment.  MODEL PROVENANCE: Opus 4.8.
	char const *const bj_trace_env = std::getenv("NAMCOS23_TRACE_BOATJITTER");
	m_bj_trace = bj_trace_env && bj_trace_env[0] != '\0'
			&& !(bj_trace_env[0] == '0' && bj_trace_env[1] == '\0');
	m_bj_have_prev = false;
	m_bj_prev_rxgen = 0;
	m_bj_reversals = 0;
	m_bj_rx_moves = 0;
	m_bj_local_moves = 0;
	for (int s = 0; s < BJ_SLOTS; s++)
	{
		for (int c = 0; c < 6; c++)
		{
			m_bj_prev[s].v[c] = 0;
			m_bj_prev[s].d[c] = 0;
		}
		m_bj_prev[s].live = false;
	}
	if (m_bj_trace)
	{
		bool const is_conn = m_c139 && m_c139->comm_is_connector();
		logerror("BOATJITTER: driver armed (NAMCOS23_TRACE_BOATJITTER=%s) - READ-ONLY per-vblank BLUE entity-ingest source/freshness/reversal trace. Emits ONLY on the CONNECTOR/blue (this cab is %s) while staging mode 0x802F3FD0==2. Scans the actor array 0x802F4DB0+slot*0x2B8 (live = status word +0x18 < 0), diffs two ingested position triples A=+0x1F4/+0x1F8/+0x1FC (op0x21-0x33 coords) and B=+0xA8/+0xAC/+0xB0 (op0x17-0x19 transform) vs last vblank, and logs 'BOATJITTER: reversal' when a component's delta flips sign (|deltas|>=%d) - the shake signal. src=RX (device rx-apply gen advanced this vblank; fresh=/replay= from the FRESH/REPLAY bridge) vs src=LOCAL (no ingest this window). Read: reversals riding replay= => hyp A (stale-replay); reversals split LOCAL-forward / RX fresh= backward => hyp B (dual-writer). Per-vblank line cap %d + 1/s status. Expect only a few to a few tens of reversal lines/sec in the boat area (low-MB capture)\n",
				bj_trace_env, is_conn ? "the CONNECTOR/blue - trace LIVE" : "NOT the connector - trace stands down (arm on blue)",
				int(BJ_MIN_MOVE), int(BJ_MAX_LINES_FRAME));
	}

	// P056 EXPERIMENT (branch patch/boat-render-trace): arm the READ-ONLY per-vblank
	// BLUE rendered-position + liveness trace from the environment (same "non-empty
	// and not literal 0" gate).  The device (namco_c139) arms its FRESH/REPLAY +
	// rx-apply bridge on EITHER NAMCOS23_TRACE_BOATJITTER or NAMCOS23_TRACE_BOATRENDER,
	// so this trace reuses that bridge with no extra device wiring.  Full design +
	// the residual uncertainty live in the member-block comment.  MODEL PROVENANCE:
	// Opus 4.8.
	char const *const br_trace_env = std::getenv("NAMCOS23_TRACE_BOATRENDER");
	m_br_trace = br_trace_env && br_trace_env[0] != '\0'
			&& !(br_trace_env[0] == '0' && br_trace_env[1] == '\0');
	m_br_have_prev = false;
	m_br_prev_rxgen = 0;
	m_br_reversals = 0;
	m_br_rx_moves = 0;
	m_br_local_moves = 0;
	m_br_live_transitions = 0;
	for (int s = 0; s < BR_SLOTS; s++)
	{
		for (int c = 0; c < 3; c++)
		{
			m_br_prev[s].v[c] = 0;
			m_br_prev[s].d[c] = 0;
		}
		m_br_prev[s].id = 0;
		m_br_prev[s].live = false;
		m_br_prev[s].sampled = false;
	}
	if (m_br_trace)
	{
		bool const is_conn = m_c139 && m_c139->comm_is_connector();
		logerror("BOATRENDER: driver armed (NAMCOS23_TRACE_BOATRENDER=%s) - READ-ONLY per-vblank BLUE rendered-position + liveness trace. Emits ONLY on the CONNECTOR/blue (this cab is %s) while staging mode 0x802F3FD0==2. Scans actor array 0x802F4DB0+slot*0x2B8, gates to ACTIVE actors (live +0x18<0 AND real id, skipping id=0000/ffff empty/placeholder slots), and diffs the RENDERED world pos +0x30/+0x34/+0x38 (renderer 0x80089C78) vs last vblank. 'BOATRENDER: reversal' = a component's delta flips sign (|deltas|>=%d) = the visible shake; 'BOATRENDER: live' = a real actor's +0x18 live flag transitions (the disappearing red player). Each line tagged src=RX (device rx-apply gen advanced this vblank; fresh=/replay= from the P055 FRESH/REPLAY bridge) vs src=LOCAL. Read: dual-writer (hyp B) = reversals split LOCAL-forward / RX fresh= backward; stale-replay (hyp A) = reversals ride replay=. Per-vblank line cap %d + 1/s status rail (active/live/reversals/live_trans/fresh/replay/cabid). REFINES P055 (which watched ingest coords on mostly-empty slots and caught only 17 reversals)\n",
				br_trace_env, is_conn ? "the CONNECTOR/blue - trace LIVE" : "NOT the connector - trace stands down (arm on blue)",
				int(BR_MIN_MOVE), int(BR_MAX_LINES_FRAME));
	}

	// P059 EXPERIMENT (branch patch/poscorr-trace): arm the READ-ONLY per-vblank
	// BLUE correction-cadence + local-drift trace from the environment (same
	// "non-empty and not literal 0" gate).  The device (namco_c139) arms its
	// FRESH/REPLAY + rx-apply bridge on NAMCOS23_TRACE_POSCORR too, so this trace
	// reuses that bridge with no extra device wiring.  Full design + the write-tap-
	// vs-sampling decision live in the member-block comment.  MODEL PROVENANCE:
	// Opus 4.8.
	char const *const pc_trace_env = std::getenv("NAMCOS23_TRACE_POSCORR");
	m_pc_trace = pc_trace_env && pc_trace_env[0] != '\0'
			&& !(pc_trace_env[0] == '0' && pc_trace_env[1] == '\0');
	m_pc_pos_have_prev = false;
	m_pc_prev_rxgen = 0;
	m_pc_corr_win = 0;
	m_pc_corr_fresh_win = 0;
	m_pc_corr_replay_win = 0;
	m_pc_local_events_win = 0;
	m_pc_local_sum_win = 0.f;
	m_pc_drift_max_seen = 0.f;
	m_pc_corr_tot = 0;
	m_pc_snap_sum = 0.f;
	m_pc_remote_live_last = 0;
	// P061 RIDER: anim-layer discriminator window state.
	m_pc_anim_chg_win = 0;
	m_pc_kf_chg_win = 0;
	m_pc_anim_ccstatic_win = 0;
	m_pc_anim_lines_win = 0;
	for (int i = 0; i < 6; i++)
		m_pc_gap_hist[i] = 0;
	for (int s = 0; s < PC_SLOTS; s++)
	{
		for (int c = 0; c < 3; c++)
		{
			m_pc_prev[s].cc[c] = 0.f;
			m_pc_prev[s].w80[c] = 0.f;
		}
		m_pc_prev[s].sampled = false;
		m_pc_prev[s].corr_seen = false;
		m_pc_prev[s].last_corr_frame = 0;
		m_pc_prev[s].drift_accum = 0.f;
		m_pc_prev[s].drift_max = 0.f;
		m_pc_prev[s].anim = 0;            // P061 RIDER
		m_pc_prev[s].kf = 0;              // P061 RIDER
		m_pc_prev[s].anim_sampled = false; // P061 RIDER
	}
	if (m_pc_trace)
	{
		bool const is_conn = m_c139 && m_c139->comm_is_connector();
		logerror("POSCORR: driver armed (NAMCOS23_TRACE_POSCORR=%s) - READ-ONLY per-vblank BLUE correction-cadence + local-drift trace for FIX FAMILY B. Emits ONLY on the CONNECTOR/blue (this cab is %s) while staging mode 0x802F3FD0==2. Scans actor array 0x802F4DB0+slot*0x2B8, gates to REMOTE actors (+0x18<0 = bit31 set), and diffs the world-pos BASE +0xCC (type-1) and +0x80 (type-2) - read as IEEE floats - vs last vblank. A base move in a vblank the device rx-apply gen advanced (an op4B/4C snapshot delivered) = 'POSCORR: corr' (INGEST correction: logs slot, snap, gap-since-last-corr, drift_pre, fresh/replay); a base move with no ingest this window = LOCAL drift (accumulated, no per-event line). 1/s 'POSCORR: rom-status' rail = remote_live, corr_win, corr_fresh/replay, mean_snap, local_events/sum, drift_max, inter-correction gap histogram, dev fresh/replay. SAMPLING (not a write-tap): main RAM is add_fastram so under the DRC stores bypass any tap AND mips3 PC is block-stale - the only sound tap forces the interpreter (~1.56x, perturbs the measured cadence), so we key INGEST/LOCAL on the rx-apply bridge instead (see member comment). Per-vblank corr-line cap %d, min move %.3f. Pair by t= with the device 'POSCORR: dev' arrival/delivery/drop rail\n",
				pc_trace_env, is_conn ? "the CONNECTOR/blue - trace LIVE" : "NOT the connector - trace stands down (arm on blue)",
				int(PC_MAX_LINES_FRAME), double(PC_MIN_MOVE));
	}

	// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
	// arm the driver-side mode-signal push for the phase-aware heartbeat token
	// cadence.  Simple non-empty/non-"0" idiom HERE (arm bit only, gating the
	// per-vblank push in vblank()); the DEVICE reads the same env in its
	// device_start(), value-checks the fast_ms and refuses out-of-range values
	// there - a refused value leaves the device inert, so a stray driver push
	// is harmless (set_ingame early-returns unarmed).  Full rationale +
	// debounce in the member-block comment.  MODEL PROVENANCE: Fable 5.
	char const *const hb_phase_aware_env = std::getenv("NAMCOS23_PATCH_HB_PHASE_AWARE");
	m_patch_hb_phase_aware = hb_phase_aware_env && hb_phase_aware_env[0] != '\0'
			&& !(hb_phase_aware_env[0] == '0' && hb_phase_aware_env[1] == '\0');
	if (m_patch_hb_phase_aware)
		logerror("HB_PHASE_AWARE: driver push armed (NAMCOS23_PATCH_HB_PHASE_AWARE=%s) - staging mode word 0x802F3FD0 sampled READ-ONLY each vblank and pushed to the C139 (set_ingame); the device debounces (60 consecutive mode-2 vblanks -> FAST, SAFE immediately on loss) and picks the heartbeat cadence; see the device HB_PHASE_AWARE armed/REFUSED banner for the validated fast_ms\n",
				hb_phase_aware_env);

	// P061 EXPERIMENT (branch patch/tx-complete-irq, off patch/hb-phase-aware):
	// arm the driver-side mode-signal push for the C422 TX-COMPLETE dispatch
	// model.  Same non-empty/non-"0" idiom; the DEVICE reads the same env in
	// device_start and owns the mechanism + the mode-2 gate (the shared P060
	// set_ingame debounce) - the driver only guarantees the per-vblank mode
	// push happens when this env is armed even with HB_PHASE_AWARE unset.
	// Full rationale in the member-block comment.  MODEL PROVENANCE: Fable 5.
	char const *const tx_complete_irq_env = std::getenv("NAMCOS23_PATCH_TX_COMPLETE_IRQ");
	m_patch_tx_complete_irq = tx_complete_irq_env && tx_complete_irq_env[0] != '\0'
			&& !(tx_complete_irq_env[0] == '0' && tx_complete_irq_env[1] == '\0');
	if (m_patch_tx_complete_irq)
		logerror("TX_COMPLETE_IRQ: driver push armed (NAMCOS23_PATCH_TX_COMPLETE_IRQ=%s) - staging mode word 0x802F3FD0 sampled READ-ONLY each vblank and pushed to the C139 (set_ingame, shared P060 debounce); the device dispatches staged standalone TXs at their stage instant only in stable mode-2; see the device TX_COMPLETE_IRQ armed banner + 1/s status rail\n",
				tx_complete_irq_env);

	// P063 EXPERIMENT (branch patch/tx-complete-v2, off patch/txstage-trace):
	// arm the driver-side mode-signal push for the P062-measured TX-complete
	// release v2.  Same non-empty/non-"0" idiom; the DEVICE reads the same env
	// in device_start and owns the whole mechanism (it also refuses to arm if
	// the dead P061 env is set - a refused device leaves this push harmless,
	// set_ingame early-returns unarmed).  Full rationale in the member-block
	// comment.  MODEL PROVENANCE: Fable 5.
	// P067 (patch/defaults-on): ADOPTED - the driver push is armed by DEFAULT;
	// NAMCOS23_PATCH_TX_COMPLETE_V2=0 is the kill switch (fully inert, the
	// pre-P067 unset path).  The DEVICE resolves the same var (default-on
	// there too) - keep the two agreeing; a device REFUSE (dead P061 env also
	// set) leaves this push harmless, set_ingame early-returns unarmed.
	bool tx_complete_v2_from_env = false;
	char const *const tx_complete_v2_val = patch_env_or_default("NAMCOS23_PATCH_TX_COMPLETE_V2", "1", tx_complete_v2_from_env);
	m_patch_tx_complete_v2 = tx_complete_v2_val != nullptr;
	if (m_patch_tx_complete_v2)
		logerror("TX_COMPLETE_V2: driver push armed (NAMCOS23_PATCH_TX_COMPLETE_V2=%s %s) - staging mode word 0x802F3FD0 sampled READ-ONLY each vblank and pushed to the C139 (set_ingame, shared P060 debounce); the device dispatches fresh-content staged TXs at their stage instant (prev-hash gate), models a TX-busy interval on the busy-poll, and ages out stale heartbeat replays - only in stable mode-2; see the device TX_COMPLETE_V2 armed banner + 1/s status rail\n",
				tx_complete_v2_val, patch_env_src(tx_complete_v2_from_env, tx_complete_v2_val).c_str());
	else
		logerror("TX_COMPLETE_V2: driver push DISABLED (NAMCOS23_PATCH_TX_COMPLETE_V2=0 kill switch - adopted default overridden; stock reg5 stop-and-wait release)\n");

	// P065 EXPERIMENT (branch patch/corr-smooth, off patch/tx-complete-v2):
	// arm the VIEWER-SIDE INGEST-CORRECTION BLENDING from the environment.
	// Boolean gate = the usual non-empty/non-"0" idiom; the three tuning
	// knobs are VALUE-CHECKED (out-of-range values are refused LOUDLY and the
	// default stands - a typo can never arm an absurd blend).  The device
	// (namco_c139) arms its rx-apply-generation bridge on the SAME env so
	// rx_apply_gen() ticks even with the POSCORR/BOATRENDER traces unset.
	// Armed on BOTH cabs (every cab is the remote-viewer of the peer's
	// entities).  Full design + phase-1 s32 format verification in the
	// member-block comment.  MODEL PROVENANCE: Fable 5.
	char const *const cs_env = std::getenv("NAMCOS23_PATCH_CORR_SMOOTH");
	m_cs_on = cs_env && cs_env[0] != '\0' && !(cs_env[0] == '0' && cs_env[1] == '\0');
	m_cs_frames = 8;                // ~133 ms at 60 fps
	m_cs_maxd = 0x01000000;         // teleport threshold, raw s32 world units (live bases observed ~0x2BAxxxxx..0x30Bxxxxx, so 0x01000000 ~ a large playfield fraction; generous by design - only cross-scene teleports should exceed it)
	m_cs_mind = 1;                  // detection epsilon: any word change (raise if 60fps local-writer motion ever reads as blend-induced lag)
	char const *const cs_frames_env = std::getenv("NAMCOS23_PATCH_CORR_SMOOTH_FRAMES");
	if (cs_frames_env && cs_frames_env[0] != '\0')
	{
		long const v = std::strtol(cs_frames_env, nullptr, 0);
		if (v >= 3 && v <= 15)
			m_cs_frames = u8(v);
		else
			logerror("CORR_SMOOTH: REFUSED _FRAMES=%s (sane 3..15) - keeping default %u\n", cs_frames_env, unsigned(m_cs_frames));
	}
	char const *const cs_maxd_env = std::getenv("NAMCOS23_PATCH_CORR_SMOOTH_MAXD");
	if (cs_maxd_env && cs_maxd_env[0] != '\0')
	{
		long long const v = std::strtoll(cs_maxd_env, nullptr, 0);
		if (v >= 1 && v <= (long long)CS_SANE_ABS)
			m_cs_maxd = s32(v);
		else
			logerror("CORR_SMOOTH: REFUSED _MAXD=%s (sane 1..0x%x) - keeping default 0x%x\n", cs_maxd_env, CS_SANE_ABS, m_cs_maxd);
	}
	char const *const cs_mind_env = std::getenv("NAMCOS23_PATCH_CORR_SMOOTH_MIND");
	if (cs_mind_env && cs_mind_env[0] != '\0')
	{
		long long const v = std::strtoll(cs_mind_env, nullptr, 0);
		if (v >= 1 && v <= (long long)m_cs_maxd)
			m_cs_mind = s32(v);
		else
			logerror("CORR_SMOOTH: REFUSED _MIND=%s (sane 1..maxd=0x%x) - keeping default %d\n", cs_mind_env, m_cs_maxd, m_cs_mind);
	}
	m_cs_have_prev = false;
	m_cs_prev_rxgen = 0;
	m_cs_mode2_prev = false;
	m_cs_blends_win = 0;
	m_cs_steps_win = 0;
	m_cs_snapthru_win = 0;
	m_cs_fmtrej_win = 0;
	m_cs_blends_tot = 0;
	m_cs_snapthru_tot = 0;
	m_cs_fmtrej_tot = 0;
	for (int s = 0; s < CS_SLOTS; s++)
	{
		m_cs_slot[s].active = false;
		m_cs_slot[s].type = 0;
		m_cs_slot[s].id = 0;
		m_cs_slot[s].frames_left = 0;
		for (int c = 0; c < 3; c++)
		{
			m_cs_slot[s].seen[c] = 0;
			m_cs_slot[s].rem[c] = 0;
		}
	}
	if (m_cs_on)
		logerror("CORR_SMOOTH: armed (NAMCOS23_PATCH_CORR_SMOOTH=%s) - VIEWER-SIDE correction blending for remote entities, BOTH cabs, driver-side, per-vblank while staging mode 0x802F3FD0==2. Walks actor array 0x802F4DB0+slot*0x2B8 (%d slots), gates to REMOTE (+0x18 bit31) + valid (+0x08 hw bit15 clear) + type 1/2 (+0x0A hw), and blends INGEST corrections on the world-pos base +0xCC/+0xD0/+0xD4 (type-1) / +0x80/+0x84/+0x88 (type-2) - VERIFIED s32 INTEGER triples (renderer 0x80089DF8 lwc1+cvt.s.w; writers 0x8010C5B0/0x80100014 trunc.w.s+swc1; 0x800FEB10 raw lw/subu) - NOT IEEE floats (the P059 garbage-magnitude caveat). A correction (rx-apply gen advanced + base moved >= mind=%d) of max-component delta D is released over N=%u vblanks (~%u ms): base written back to (new-D)+D/N immediately, +~D/N per vblank after (integer-exact; retargets from current on a newer correction). SNAP-THROUGH (instant, stock): |D| > maxd=0x%x (teleports/respawns/scene changes), slot id/type change (reuse), remote/valid loss, mode-2 loss, |word| > 0x%x format rail (pass through unsmoothed, counted format_rejects). Local prediction is KEPT: steps are relative to the freshly-read word, never forced back. Position words ONLY; <=3 word-writes/slot/vblank. 1/s 'CORR_SMOOTH: status' rail = blends/steps/snap_throughs/format_rejects + active_blends. Expect on the P064 boat repro: blue's boat snaps soften (blends track corrections, snap_throughs only at scene changes); boats still drift BETWEEN corrections (~0.17 corr/s each - blending covers the snap, not the starvation)\n",
				cs_env, int(CS_SLOTS), m_cs_mind, unsigned(m_cs_frames),
				unsigned(u32(m_cs_frames) * 1000 / 60), m_cs_maxd, CS_SANE_ABS);

	// P048 EXPERIMENT (branch patch/reaper-patience, off patch/anchor-lifecycle):
	// arm the bit31 snapshot-timeout REAPER patience widening from the
	// environment.  VALUE-CHECKED gate, stricter than the boolean idiom: only
	// "1" (imm 0x11 -> 0x1F, patience 17 -> 31 ticks) and "2" (imm 0x11 ->
	// 0x20, reaper OFF - the escalation reserve) are defined; empty or literal
	// "0" = silently inert (stock); ANY other value = loud refuse-to-arm and
	// inert, so a typo can never poke an unintended immediate.  The poke
	// itself is DEFERRED to vblank (at machine_start the boot loader has not
	// yet copied the program into RAM, so the word reads 0).  Full P047
	// rationale, the verified instruction word and the structural bit31
	// safety discriminator live in the member-block comment.
	// P067 (patch/defaults-on): ADOPTED - default mode 1 (imm 0x11->0x1F,
	// patience 17->31 ticks); NAMCOS23_PATCH_REAPER_PATIENCE=0 is the kill
	// switch (fully inert, no poke ever - the pre-P067 unset path); the
	// strict value check is UNCHANGED: only 1 and 2 are defined, anything
	// else still refuses loudly and stays inert.
	bool reaper_patience_from_env = false;
	char const *const reaper_patience_val = patch_env_or_default("NAMCOS23_PATCH_REAPER_PATIENCE", "1", reaper_patience_from_env);
	m_patch_reaper_patience = 0;
	m_rpp_poked = false;
	m_rpp_refused = false;
	m_rpp_repokes = 0;
	if (reaper_patience_val)
	{
		if (reaper_patience_val[0] == '1' && reaper_patience_val[1] == '\0')
			m_patch_reaper_patience = 1;
		else if (reaper_patience_val[0] == '2' && reaper_patience_val[1] == '\0')
			m_patch_reaper_patience = 2;
		else
			logerror("REAPER_PATIENCE: REFUSED to arm (NAMCOS23_PATCH_REAPER_PATIENCE=%s %s) - only 1 (imm 0x11->0x1F: patience 17->31 ticks) and 2 (imm 0x11->0x20: reaper OFF, escalation reserve) are defined; staying inert (no poke ever)\n",
					reaper_patience_val, patch_env_src(reaper_patience_from_env, reaper_patience_val).c_str());
	}
	else
		logerror("REAPER_PATIENCE: DISABLED (NAMCOS23_PATCH_REAPER_PATIENCE=0 kill switch - adopted default 1 overridden; stock 17-tick reaper, no poke)\n");
	if (m_patch_reaper_patience != 0)
		logerror("REAPER_PATIENCE: armed (NAMCOS23_PATCH_REAPER_PATIENCE=%d %s) - verify-before-poke RAM-code word patch of the ROM's bit31 snapshot-timeout REAPER threshold (P047: gate @0x800B7184-71A4, release @0x800B71CC - kills any word0-bit31 remote-placeholder entity unserved for (count&0x1F) >= 0x11 = ~17 frames; the +0xF2 budget is refreshed ONLY by partner state ingest @0x800ABE9C-0x800AC818; at the round-start HB-floor regime a serve takes 2-3 hops = 0.30-0.45 s > the 0.28 s budget, so slave-side bit31 anchors (8025c0c8 +16 f class) and REMOTE op-0x2E sub-wave children (room-1: the emptied family lets the anchor's designed op-0x76 -> script-END completion fire pre-engage at +36/+51 f) evaporate exactly in the skip regime). Each vblank until terminal, read u32 @0x800B71A0 (program-space 0x000b71a0; the word reads 0 until the boot loader copies the program); on 0x2C420011 (sltiu $v0,$v0,0x11 - VERIFIED in full.txt) poke the SAME word with ONLY the immediate field changed: mode 1 -> 0x2C42001F (31 ticks ~0.52 s > the 3-hop worst case), mode 2 -> 0x2C420020 (unreachable by the &0x1F count = reaper OFF; the 40 s watchdog +0x3A2 + partner op-0x20/despawn echoes remain the orphan cleanup); on any OTHER nonzero word REFUSE one-shot (ROM-revision guard). SAFETY IS STRUCTURAL: only bit31 placeholders are touched, and designed walk/staging completions require a script to have RUN, which bit31 forbids (bit31 -> placeholder tick 0x800B6A50; the program never executes) - the P043/P044 stall class is unreachable. DRC: P040b ordering (the reap tail first runs at attract gameplay, long after the early-boot poke; residual re-copy windows fail to STOCK patience only), 1/s word re-check/re-poke guard + status rail. Both cabs, same env. Expect: poked banner x1 early boot; status wordb71a0=%s thereafter; the OP20 reg-to-kill +[15,18] f ring-kill class collapses to 0; room-1 and c0c8-class fight areas PLAY\n",
				m_patch_reaper_patience,
				patch_env_src(reaper_patience_from_env, reaper_patience_val).c_str(),
				(m_patch_reaper_patience == 2) ? "2C420020" : "2C42001F");

	// P050 EXPERIMENT (branch patch/single-burst-pump): arm the SINGLE-BURST
	// PUMP poke from the environment - same "non-empty and not literal 0" gate
	// idiom; the poke itself is DEFERRED to vblank (at machine_start the boot
	// loader has not yet copied the program into RAM, so the word reads 0).  The
	// device (namco_c139) reads the SAME env in device_start for its two
	// companions.  Full P049 rationale, the verified word, the RX-validator
	// bound and the device checklist live in the member-block comment.  MODEL
	// PROVENANCE: Opus 4.8.
	// P067 (patch/defaults-on): ADOPTED - armed by DEFAULT;
	// NAMCOS23_PATCH_BURST_QUANTUM=0 is the kill switch (fully inert, no poke
	// ever - the pre-P067 unset path).  The device reads the same var in its
	// device_start() for the two companions - keep the two agreeing.
	bool burst_quantum_from_env = false;
	char const *const burst_quantum_val = patch_env_or_default("NAMCOS23_PATCH_BURST_QUANTUM", "1", burst_quantum_from_env);
	m_patch_burst_quantum = burst_quantum_val != nullptr;
	m_bq_poked = false;
	m_bq_refused = false;
	m_bq_repokes = 0;
	if (!m_patch_burst_quantum)
		logerror("BURST_QUANTUM: DISABLED (NAMCOS23_PATCH_BURST_QUANTUM=0 kill switch - adopted default overridden; stock 0xFF-hw pump quantum, no poke)\n");
	if (m_patch_burst_quantum)
		logerror("BURST_QUANTUM: armed (NAMCOS23_PATCH_BURST_QUANTUM=%s %s) - verify-before-poke RAM-code word patch of the ROM TX pump burst quantum (P049 F1: the fight-era 19/s->5.5-11/s collapse is the stop-and-wait wire x a payload chopped into <=0xFF-hw bursts; the 255-hw ceiling is the pump's own quantum @0x8000BC78, NOT the chip limit - the RX drain validator @0x8000BF90 accepts <=0x400 hw and each C422 slot is 0x400 hw). Each vblank until terminal, read u32 @0x8000BC78 (program-space 0x0000bc78; the word reads 0 until the boot loader copies the program); on 0x28420100 (slti $v0,$v0,0x100 - VERIFIED in full.txt) poke the SAME word with ONLY the immediate changed to 0x28420401 (slti $v0,$v0,0x401): every VM frame <=0x400 hw emits as ONE burst (our frames <=0x394 hw), the 0xFF-hw continuation paths go dead; on any OTHER nonzero word REFUSE one-shot (ROM-revision guard). DRC: P040b/REAPER ordering (the pump's frame-start first runs at attract link-up, after the early-boot poke; residual re-copy windows fail to STOCK 0x100 quantum only), 1/s word re-check/re-poke guard + status rail. Both cabs, same env. Device companions (latch retire + heartbeat re-arm for >255-hw single bursts) arm on the same env in namco_c139 device_start. Expect: poked banner x1 early boot; status wordbc78=28420401 thereafter; device single_burst_tx climbs in fights + no 255-hw chunk trains; fight fps 5.5-11 -> 12-19/s\n",
				burst_quantum_val, patch_env_src(burst_quantum_from_env, burst_quantum_val).c_str());

	// P066 EXPERIMENT (branch patch/link-wait): arm the PARTNER-SEARCH
	// countdown seed widen from the environment.  VALUE-CHECKED:
	// NAMCOS23_PATCH_LINK_WAIT=<seconds>, integer 30..300 only (30 = stock
	// width, 300 = 5 min; converted to ROM units = seconds*10 per the observed
	// ~10 counts/s on-screen cadence); empty or literal "0" = silently inert
	// (stock, byte-identical); ANY other value = loud one-shot refuse-to-arm
	// and inert, so a typo can never poke an unintended immediate.  The poke
	// itself is DEFERRED to vblank (at machine_start the boot loader has not
	// yet copied the program into RAM, so the word reads 0).  Full RE
	// rationale, the verified word, the unit calibration and the both-cabs
	// caveat live in the member-block comment.  MODEL PROVENANCE: Fable 5.
	char const *const link_wait_env = std::getenv("NAMCOS23_PATCH_LINK_WAIT");
	m_patch_link_wait_units = 0;
	m_lw_poked = false;
	m_lw_refused = false;
	m_lw_repokes = 0;
	if (link_wait_env && link_wait_env[0] != '\0'
			&& !(link_wait_env[0] == '0' && link_wait_env[1] == '\0'))
	{
		char *end = nullptr;
		long const seconds = std::strtol(link_wait_env, &end, 10);
		if (end != link_wait_env && *end == '\0' && seconds >= 30 && seconds <= 300)
			m_patch_link_wait_units = int(seconds) * 10;
		else
			logerror("LINK_WAIT: REFUSED to arm (NAMCOS23_PATCH_LINK_WAIT=%s) - integer seconds 30..300 only (ROM stock = 30 s, on-screen seed 300); staying inert (no poke ever)\n",
					link_wait_env);
	}
	if (m_patch_link_wait_units != 0)
		logerror("LINK_WAIT: armed (NAMCOS23_PATCH_LINK_WAIT=%s -> units=%d) - verify-before-poke RAM-code word patch of the boot PARTNER-SEARCH countdown seed (P066 static RE: state 0 @0x80009A90 - table 0x8022A170[gp+0x756C] via dispatcher 0x8000A370 - seeds gp+0x6F78 with li $v0,300 @0x80009AB8; the state-1 tick 0x80009AD8 counts it down on the NETWORK CHECK screen at the observed ~10 counts/s, and its expiry - the SAME timer, there is no separate solo-fallback timer - advances to the state-2 link-vs-solo flow @0x8000A4CC once the A440 gate gp+0x7790 is up). Each vblank until terminal, read u32 @0x80009AB8 (program-space 0x00009ab8; reads 0 until the boot loader copies the program); on 0x2402012C (VERIFIED in full.txt; gp+0x6F78 has no other writer and no other timer shares the word) poke the SAME word with ONLY the immediate changed to units = seconds*10; on any OTHER nonzero word REFUSE one-shot (ROM-revision guard). ESTABLISHMENT SAFETY: only the seed VALUE changes - tick/expiry logic, the A440 handshake and the op55 machinery are untouched. BOTH CABS MUST RUN THE SAME VALUE: a shorter-window cab exits partner search (state -> 2) earlier - a mismatch re-creates the early solo-fallback this patch removes. DRC: P040b/REAPER/BURST ordering class at its tightest margin (the seed first runs at the first state-0 dispatch, after the boot init chain; a residual re-copy window seeds STOCK 300 = fail-to-stock, the on-screen start value is the tell), 1/s word re-check/re-poke guard + status rail. Expect: poked banner x1 early boot; the on-screen countdown starts at %d instead of 300\n",
				link_wait_env, m_patch_link_wait_units, m_patch_link_wait_units);

	// TEMP DEBUG (phase 9d): install_write_tap on link-state vars + timeout
	// counter is bypassed by add_fastram above. Polled per-frame in vblank()
	// instead. REMOVE BEFORE COMMIT.
}

void gorgon_state::machine_start()
{
	namcos23_state::machine_start();

	save_item(NAME(m_cz_was_written));

	for (int bank = 0; bank < 4; bank++)
	{
		m_banked_czram[bank] = make_unique_clear<u16[]>(0x100);
		m_recalc_czram[bank] = make_unique_clear<u8[]>(0x2000);
		m_cz_was_written[bank] = 1;

		save_pointer(NAME(m_banked_czram[bank]), 0x100, bank);
		save_pointer(NAME(m_recalc_czram[bank]), 0x2000, bank);
	}
}

void crszone_state::machine_start()
{
	namcos23_state::machine_start();

	m_rs232_irqnum = MIPS3_IRQ0;
	m_sub_irqnum = MIPS3_IRQ1;
	m_c422_irqnum = MIPS3_IRQ1;
	m_vbl_irqnum = MIPS3_IRQ2;
	m_c361_irqnum = MIPS3_IRQ3;
	m_c435_irqnum = -1;
	m_c451_irqnum = MIPS3_IRQ3;
	m_c450_irqnum = MIPS3_IRQ4;
}

void namcos23_state::machine_reset()
{
	m_absolute_priority = 0;
	m_tx = 0;
	m_ty = 0;
	m_model_blend_factor = 0x4000;
	m_light_power = 0;
	m_light_ambient = 0;
	memset(m_clip_data, 0, sizeof(m_clip_data));
	m_clip_data_line = 0;

	for (int i = 0; i < 256; i++)
	{
		memset(m_matrices[i], 0, sizeof(s16) * 9);
		memset(m_vectors[i], 0, sizeof(s32) * 3);
	}
	memset(m_light_vector, 0, sizeof(m_light_vector));
	m_scaling = 0x4000;
	memset(m_spv, 0, sizeof(m_spv));
	memset(m_spm, 0, sizeof(m_spm));

	memset(&m_c404, 0, sizeof(c404_t));
	m_c361.scanline = 0;
	memset(&m_c417, 0, sizeof(c417_t));
	memset(&m_c412, 0, sizeof(c412_t));
	memset(&m_c421, 0, sizeof(c421_t));
	memset(&m_c422, 0, sizeof(c422_t));
	memset(&m_c435, 0, sizeof(c435_t));

	m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	m_subcpu_running = false;

	m_subcpu_scanline_on_timer->adjust(attotime::zero, 0, m_screen->scan_period());
	m_subcpu_scanline_off_timer->adjust(m_screen->time_until_pos(0, 32), 0, m_screen->time_until_pos(0, 32) + m_screen->scan_period());

	m_jvs_sense = 1;
	m_main_irqcause = 0;
	m_ctl_vbl_active = false;
	m_ctl_led = 0;
	m_ctl_inp_buffer[0] = m_ctl_inp_buffer[1] = 0;
	m_sub_port8 = 0x02;
	m_sub_porta = 0;
	m_sub_portb = 0x50;
	m_subcpu_running = false;
	m_render.count[0] = m_render.count[1] = 0;
	m_render.cur = 0;
}

void gorgon_state::machine_reset()
{
	namcos23_state::machine_reset();

	m_subcpu_scanline_on_timer->adjust(attotime::never);
	m_subcpu_scanline_off_timer->adjust(attotime::never);
}

void namcoss23_gmen_state::machine_start()
{
	namcos23_state::machine_start();

	save_item(NAME(m_vpx_sdao));
}

void namcoss23_gmen_state::machine_reset()
{
	namcos23_state::machine_reset();

	// halt the SH-2 until we need it
	m_sh2->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);

	m_sh2_unk = 0;
	m_vpx_sdao = 0;
}

static GFXLAYOUT_RAW(gorgon_sprite_layout, 32, 32, 32*8, 32*32*8)

static GFXDECODE_START( gfx_gorgon )
	GFXDECODE_ENTRY( "sprites", 0, gorgon_sprite_layout, 0, 0x80 )
GFXDECODE_END

void gorgon_state::gorgon(machine_config &config)
{
	/* basic machine hardware */
	R4650BE(config, m_maincpu, BUSCLOCK*4);
	m_maincpu->set_icache_size(8192);   // VERIFIED
	m_maincpu->set_dcache_size(8192);   // VERIFIED
	m_maincpu->set_addrmap(AS_PROGRAM, &gorgon_state::mips_map);

	H83002(config, m_subcpu, H8CLOCK);
	m_subcpu->set_addrmap(AS_PROGRAM, &gorgon_state::s23h8rwmap);
	m_subcpu->read_adc<0>().set_constant(0);
	m_subcpu->read_adc<1>().set_constant(0);
	m_subcpu->read_adc<2>().set_constant(0);
	m_subcpu->read_adc<3>().set_constant(0);
	m_subcpu->read_port6().set(FUNC(gorgon_state::mcu_p6_r));
	m_subcpu->write_port6().set(FUNC(gorgon_state::mcu_p6_w));
	m_subcpu->read_port7().set_constant(0);
	m_subcpu->read_port8().set(FUNC(gorgon_state::mcu_p8_r));
	m_subcpu->write_port8().set(FUNC(gorgon_state::mcu_p8_w));
	m_subcpu->read_porta().set(FUNC(gorgon_state::mcu_pa_r));
	m_subcpu->write_porta().set(FUNC(gorgon_state::mcu_pa_w));
	m_subcpu->read_portb().set(FUNC(gorgon_state::mcu_pb_r));
	m_subcpu->write_portb().set(FUNC(gorgon_state::mcu_pb_w));

	// Timer at 115200*16 for the jvs serial clock
	m_subcpu->sci_set_external_clock_period(0, attotime::from_hz(JVSCLOCK/8));

	NAMCO_SETTINGS(config, m_settings, 0);

	RTC4543(config, m_rtc, XTAL(32'768));
	m_rtc->data_cb().set(m_subcpu, FUNC(h8_device::sci_rx_w<1>));

	m_subcpu->write_sci_tx<1>().set(m_settings, FUNC(namco_settings_device::data_w));
	m_subcpu->write_sci_clk<1>().set(m_rtc, FUNC(rtc4543_device::clk_w)).invert();
	m_subcpu->write_sci_clk<1>().append(m_settings, FUNC(namco_settings_device::clk_w));

	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0);

	/* video hardware */
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(PIXEL_CLOCK, HTOTAL, HBEND, HBSTART, VTOTAL, VBEND, VBSTART);
	m_screen->set_screen_update(FUNC(gorgon_state::screen_update));
	m_screen->screen_vblank().set(FUNC(gorgon_state::vblank));
	m_screen->set_video_attributes(VIDEO_ALWAYS_UPDATE);

	PALETTE(config, m_palette).set_entries(0x8000);

	GFXDECODE(config, m_gfxdecode, m_palette, gfx_gorgon);

	/* sound hardware */
	SPEAKER(config, "speaker", 2).front();

	c352_device &c352(C352(config, "c352", C352CLOCK, C352DIV));
	c352.add_route(0, "speaker", 1.00, 0);
	c352.add_route(1, "speaker", 1.00, 1);
	c352.add_route(2, "speaker", 1.00, 0);
	c352.add_route(3, "speaker", 1.00, 1);

	JVS_PORT(config, m_jvs, jvs_port_devices, nullptr);
	m_jvs->rxd().set(m_subcpu, FUNC(h8_device::sci_rx_w<0>));
	m_subcpu->write_sci_tx<0>().set(m_jvs, FUNC(jvs_port_device::txd));
	m_jvs->sense().set([this](u8 state) { m_jvs_sense = state; });

	for (const auto &option : m_jvs->option_list())
		m_jvs->set_option_machine_config(option.first.c_str(), [this](device_t *device)
	{
		configure_jvs(dynamic_cast<device_jvs_interface &>(*device));
	});
}

void gorgon_state::finfurl(machine_config &config)
{
	gorgon(config);
	m_jvs->set_default_option("namco_asca1");
}

void namcos23_state::s23(machine_config &config)
{
	/* basic machine hardware */
	R4650BE(config, m_maincpu, BUSCLOCK*5);
	m_maincpu->set_icache_size(8192);   // VERIFIED
	m_maincpu->set_dcache_size(8192);   // VERIFIED
	m_maincpu->set_addrmap(AS_PROGRAM, &namcos23_state::mips_map);

	H83002(config, m_subcpu, H8CLOCK);
	m_subcpu->set_addrmap(AS_PROGRAM, &namcos23_state::s23h8rwmap);
	m_subcpu->read_adc<0>().set_constant(0);
	m_subcpu->read_adc<1>().set_constant(0);
	m_subcpu->read_adc<2>().set_constant(0);
	m_subcpu->read_adc<3>().set_constant(0);
	m_subcpu->read_port6().set(FUNC(namcos23_state::mcu_p6_r));
	m_subcpu->write_port6().set(FUNC(namcos23_state::mcu_p6_w));
	m_subcpu->read_port7().set_constant(0);
	m_subcpu->read_port8().set(FUNC(namcos23_state::mcu_p8_r));
	m_subcpu->write_port8().set(FUNC(namcos23_state::mcu_p8_w));
	m_subcpu->read_porta().set(FUNC(namcos23_state::mcu_pa_r));
	m_subcpu->write_porta().set(FUNC(namcos23_state::mcu_pa_w));
	m_subcpu->read_portb().set(FUNC(namcos23_state::mcu_pb_r));
	m_subcpu->write_portb().set(FUNC(namcos23_state::mcu_pb_w));

	// Timer at 115200*16 for the jvs serial clock
	m_subcpu->sci_set_external_clock_period(0, attotime::from_hz(JVSCLOCK/8));

	NAMCO_SETTINGS(config, m_settings, 0);

	RTC4543(config, m_rtc, XTAL(32'768));
	m_rtc->data_cb().set(m_subcpu, FUNC(h8_device::sci_rx_w<1>));

	m_subcpu->write_sci_tx<1>().set(m_settings, FUNC(namco_settings_device::data_w));
	m_subcpu->write_sci_clk<1>().set(m_rtc, FUNC(rtc4543_device::clk_w)).invert();
	m_subcpu->write_sci_clk<1>().append(m_settings, FUNC(namco_settings_device::clk_w));

	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0);

	// Twin-cabinet RS-422 link controller (silkscreen "C422", believed
	// to be a faster-clocked C139).  Currently only wired into the
	// namcos23_state::mips_map path; gorgon/crszone variants still use
	// the legacy inline c422_t stub.
	NAMCO_C139(config, m_c139, 0);
	m_c139->irq_handler().set(FUNC(namcos23_state::c139_irq_w));

	/* video hardware */
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(PIXEL_CLOCK, HTOTAL, HBEND, HBSTART, VTOTAL, VBEND, VBSTART);
	m_screen->set_screen_update(FUNC(namcos23_state::screen_update));
	m_screen->screen_vblank().set(FUNC(namcos23_state::vblank));
	m_screen->set_video_attributes(VIDEO_ALWAYS_UPDATE);

	PALETTE(config, m_palette).set_entries(0x8000);

	/* sound hardware */
	SPEAKER(config, "speaker", 2).front();

	c352_device &c352(C352(config, "c352", C352CLOCK, C352DIV));
	c352.add_route(0, "speaker", 1.00, 0);
	c352.add_route(1, "speaker", 1.00, 1);
	c352.add_route(2, "speaker", 1.00, 0);
	c352.add_route(3, "speaker", 1.00, 1);

	JVS_PORT(config, m_jvs, jvs_port_devices, nullptr);
	m_jvs->rxd().set(m_subcpu, FUNC(h8_device::sci_rx_w<0>));
	m_subcpu->write_sci_tx<0>().set(m_jvs, FUNC(jvs_port_device::txd));
	m_jvs->sense().set([this](u8 state) { m_jvs_sense = state; });

	for (const auto &option : m_jvs->option_list())
		m_jvs->set_option_machine_config(option.first.c_str(), [this](device_t *device)
	{
		configure_jvs(dynamic_cast<device_jvs_interface &>(*device));
	});
}

void namcos23_state::configure_jvs(device_jvs_interface &io)
{
	io.system().set_ioport("^^JVS_SYSTEM");
	io.player<0>().set_ioport("^^JVS_PLAYER1");
	io.coin<0>().set_ioport("^^JVS_COIN1");
	io.analog_input<0>().set_ioport("^^JVS_ANALOG_INPUT1");
	io.analog_input<1>().set_ioport("^^JVS_ANALOG_INPUT2");
	io.analog_input<2>().set_ioport("^^JVS_ANALOG_INPUT3");
	io.analog_input<3>().set_ioport("^^JVS_ANALOG_INPUT4");
	io.analog_input<4>().set_ioport("^^JVS_ANALOG_INPUT5");
	io.analog_input<5>().set_ioport("^^JVS_ANALOG_INPUT6");
	io.analog_input<6>().set_ioport("^^JVS_ANALOG_INPUT7");
	io.analog_input<7>().set_ioport("^^JVS_ANALOG_INPUT8");
	io.rotary_input<0>().set_ioport("^^JVS_ROTARY_INPUT1");
	io.screen_position_x<0>().set_ioport("^^JVS_SCREEN_POSITION_INPUT_X1");
	io.screen_position_y<0>().set_ioport("^^JVS_SCREEN_POSITION_INPUT_Y1");
}

void namcos23_state::downhill(machine_config &config)
{
	s23(config);
	m_jvs->set_default_option("namco_asca3");
}

void namcos23_state::panicprk(machine_config &config)
{
	s23(config);
	m_jvs->set_default_option("namco_asca3a");
}

void namcos23_state::timecrs2(machine_config &config)
{
	s23(config);
	m_jvs->set_default_option("namco_tssio");

	// P040 EXPERIMENT (branch patch/op6f-394-clamp): when the op6F 394-adoption
	// store filter is armed, force the main-CPU INTERPRETER.  The filter keys on
	// the PC observed inside a write tap; under the DRC (the MAME default)
	// m_core->pc is only synced at block exits/exceptions, so pc() reads a
	// STALE value inside a mid-block store and the filter would silently fail
	// open.  The interpreter steps m_core->pc every instruction (advancing it
	// BEFORE the op body executes - at store time pc() reads store-PC+4, a
	// deterministic offset the filter's range compare absorbs; see the member-
	// block comment).  This MUST run at machine-config time: mips3 latches
	// m_isdrc = allow_drc() in device_start(), before the driver's
	// machine_start() (same driver-side switch as atvtrack/aristmk6/cougar).
	// The env parse duplicates the machine_start idiom ("non-empty and not
	// literal 0") because machine_start is too late.  Env unset -> stock DRC,
	// bit-exact stock behavior.  Experiment branch only.
	char const *const no394_cfg_env = std::getenv("NAMCOS23_PATCH_OP6F_NO394");
	if (no394_cfg_env && no394_cfg_env[0] != '\0'
			&& !(no394_cfg_env[0] == '0' && no394_cfg_env[1] == '\0'))
		m_maincpu->set_force_no_drc(true);
}

void namcoss23_state::ss23(machine_config &config)
{
	s23(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &namcoss23_state::mips_map);
}

void namcoss23_state::timecrs2v4a(machine_config &config)
{
	ss23(config);
	m_jvs->set_default_option("namco_tssio");
}

void namcoss23_state::_500gp(machine_config &config)
{
	ss23(config);
	m_jvs->set_default_option("namco_fca10");
}

void namcoss23_state::aking(machine_config &config)
{
	ss23(config);
	m_jvs->set_default_option("namco_fca10");
}

void namcoss23_gmen_state::gmen(machine_config &config)
{
	ss23(config);

	m_maincpu->set_addrmap(AS_PROGRAM, &namcoss23_gmen_state::mips_map);

	SH7604(config, m_sh2, XTAL(20'000'000));
	m_sh2->set_addrmap(AS_PROGRAM, &namcoss23_gmen_state::sh2_map);

	VPX3220A(config, m_vpx, 0);
	m_vpx->sda_callback().set(FUNC(namcoss23_gmen_state::vpx_i2c_sdao_w));
	m_vpx->vref_callback().set_inputline(m_sh2, 4);

	MD8412B_S23(config, m_firewire, 0);
	m_firewire->int_callback().set_inputline(m_sh2, 8);
}

void namcoss23_gmen_state::gunwars(machine_config &config)
{
	gmen(config);
	m_jvs->set_default_option("namco_asca5");
}

void namcoss23_gmen_state::raceon(machine_config &config)
{
	gmen(config);
	m_jvs->set_default_option("namco_asca5");
}

void crszone_state::crszone(machine_config &config)
{
	ss23(config);

	/* basic machine hardware */
	m_maincpu->set_clock(BUSCLOCK * 6);
	m_maincpu->set_addrmap(AS_PROGRAM, &crszone_state::mips_map);

	m_jvs->set_default_option("namco_csz1");

	/* debug hardware */
	ACIA6850(config, m_acia, 0);
	m_acia->txd_handler().set("rs232", FUNC(rs232_port_device::write_txd));
	m_acia->irq_handler().set(FUNC(crszone_state::acia_irq_w));

	clock_device &acia_clock(CLOCK(config, "acia_clock", 1'843'200));
	acia_clock.signal_handler().set("acia", FUNC(acia6850_device::write_txc));
	acia_clock.signal_handler().append("acia", FUNC(acia6850_device::write_rxc));

	rs232_port_device &rs232(RS232_PORT(config, "rs232", default_rs232_devices, nullptr));
	rs232.rxd_handler().set(m_acia, FUNC(acia6850_device::write_rxd));
}


/***************************************************************************

  Inputs

***************************************************************************/

static INPUT_PORTS_START(s23)
	// You can go to the pcb test mode by pressing P1-A in some games.
	// Use P1-A to select, P1-Sel+P1-A to exit, up/down to navigate
	PORT_START("P1")
	PORT_BIT(0x001, IP_ACTIVE_LOW, IPT_BUTTON5) PORT_PLAYER(2) PORT_NAME("Dev Service P1-D")
	PORT_BIT(0x002, IP_ACTIVE_LOW, IPT_BUTTON6) PORT_PLAYER(2) PORT_NAME("Dev Service P1-E")
	PORT_BIT(0x004, IP_ACTIVE_LOW, IPT_BUTTON7) PORT_PLAYER(2) PORT_NAME("Dev Service P1-F")
	PORT_BIT(0x008, IP_ACTIVE_LOW, IPT_BUTTON1) PORT_PLAYER(2) PORT_NAME("Dev Service P1-A")
	PORT_BIT(0x010, IP_ACTIVE_LOW, IPT_BUTTON8) PORT_PLAYER(2) PORT_NAME("Dev Service P1-G")
	PORT_BIT(0x020, IP_ACTIVE_LOW, IPT_BUTTON9) PORT_PLAYER(2) PORT_NAME("Dev Service P1-H")
	PORT_BIT(0x040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN) PORT_PLAYER(2) PORT_NAME("Dev Service Down")
	PORT_BIT(0x080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP) PORT_PLAYER(2) PORT_NAME("Dev Service Up")
	PORT_BIT(0x100, IP_ACTIVE_LOW, IPT_START2) PORT_NAME("Dev Service Start")
	PORT_BIT(0x200, IP_ACTIVE_LOW, IPT_BUTTON2) PORT_PLAYER(2) PORT_NAME("Dev Service P1-Sel")
	PORT_BIT(0x400, IP_ACTIVE_LOW, IPT_BUTTON3) PORT_PLAYER(2) PORT_NAME("Dev Service P1-B")
	PORT_BIT(0x800, IP_ACTIVE_LOW, IPT_BUTTON4) PORT_PLAYER(2) PORT_NAME("Dev Service P1-C")

	PORT_START("P2")
	PORT_BIT(0xfff, IP_ACTIVE_LOW, IPT_UNKNOWN)

	PORT_START("DSW")
	PORT_DIPNAME(0x01, 0x01, "Service Mode DIP") PORT_DIPLOCATION("DIP:8")
	PORT_DIPSETTING(0x01, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x02, 0x02, "Skip POST") PORT_DIPLOCATION("DIP:7")
	PORT_DIPSETTING(0x02, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x04, 0x04, "Freeze?") PORT_DIPLOCATION("DIP:6")
	PORT_DIPSETTING(0x04, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x08, 0x08, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:5")
	PORT_DIPSETTING(0x08, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x10, 0x10, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:4")
	PORT_DIPSETTING(0x10, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x20, 0x20, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:3")
	PORT_DIPSETTING(0x20, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x40, 0x40, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:2")
	PORT_DIPSETTING(0x40, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x80, 0x80, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:1")
	PORT_DIPSETTING(0x80, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))

	PORT_START("JVS_SYSTEM")
	PORT_SERVICE(0x80, IP_ACTIVE_HIGH)

	PORT_START("JVS_PLAYER1")
	PORT_BIT(0x00000040, IP_ACTIVE_HIGH, IPT_SERVICE1)
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1)
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON2)
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_BUTTON3)
	PORT_BIT(0x00004000, IP_ACTIVE_HIGH, IPT_BUTTON4)
	PORT_BIT(0x00002000, IP_ACTIVE_HIGH, IPT_BUTTON5)
	PORT_BIT(0x00001000, IP_ACTIVE_HIGH, IPT_BUTTON6)
	PORT_BIT(0x00000800, IP_ACTIVE_HIGH, IPT_BUTTON7)
	PORT_BIT(0x00000400, IP_ACTIVE_HIGH, IPT_BUTTON8)
	PORT_BIT(0x00000200, IP_ACTIVE_HIGH, IPT_BUTTON9)
	PORT_BIT(0x00000100, IP_ACTIVE_HIGH, IPT_BUTTON10)
	PORT_BIT(0x00800000, IP_ACTIVE_HIGH, IPT_BUTTON11)
	PORT_BIT(0x00400000, IP_ACTIVE_HIGH, IPT_BUTTON12)
	PORT_BIT(0x00200000, IP_ACTIVE_HIGH, IPT_BUTTON13)

	PORT_START("JVS_COIN1")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_COIN1)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_OTHER) PORT_NAME("Counter disconnected")

	/* Dummy so we can easily get the analog ch # */
	PORT_START("JVS_ANALOG_INPUT1")
	PORT_BIT(0x80ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT2")
	PORT_BIT(0x81ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT3")
	PORT_BIT(0x82ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT4")
	PORT_BIT(0x83ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT5")
	PORT_BIT(0x84ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT6")
	PORT_BIT(0x85ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT7")
	PORT_BIT(0x86ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ANALOG_INPUT8")
	PORT_BIT(0x87ff, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("JVS_ROTARY_INPUT1")
	PORT_BIT(0xffff, 0x0000, IPT_UNUSED)

	PORT_START("JVS_SCREEN_POSITION_INPUT_X1")
	PORT_BIT(0xffff, 0x0000, IPT_UNUSED)

	PORT_START("JVS_SCREEN_POSITION_INPUT_Y1")
	PORT_BIT(0xffff, 0x0000, IPT_UNUSED)
INPUT_PORTS_END

static INPUT_PORTS_START(gorgon)
	PORT_INCLUDE(s23)

	PORT_MODIFY("P1")
	PORT_BIT(0xfff, IP_ACTIVE_LOW, IPT_UNKNOWN)

	PORT_MODIFY("P2")
	PORT_BIT(0xfff, IP_ACTIVE_LOW, IPT_UNKNOWN)

	PORT_MODIFY("DSW")
	PORT_DIPNAME(0x01, 0x01, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:8")
	PORT_DIPSETTING(0x01, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x02, 0x02, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:7")
	PORT_DIPSETTING(0x02, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x04, 0x04, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:6")
	PORT_DIPSETTING(0x04, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x08, 0x08, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:5")
	PORT_DIPSETTING(0x08, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x10, 0x10, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:4")
	PORT_DIPSETTING(0x10, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x20, 0x20, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:3")
	PORT_DIPSETTING(0x20, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x40, 0x40, DEF_STR(Unknown)) PORT_DIPLOCATION("DIP:2")
	PORT_DIPSETTING(0x40, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x80, 0x80, "Service Mode DIP") PORT_DIPLOCATION("DIP:1")
	PORT_DIPSETTING(0x80, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
INPUT_PORTS_END

static INPUT_PORTS_START(gmen)
	PORT_INCLUDE(s23)

	PORT_START("GMENDSW")
	PORT_DIPNAME( 0x01, 0x01, "SH-2 DIP Bit 0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "SH-2 DIP Bit 1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "SH-2 DIP Bit 2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "SH-2 DIP Bit 3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "SH-2 DIP Bit 4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "SH-2 DIP Bit 5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "SH-2 DIP Bit 6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "SH-2 DIP Bit 7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

static INPUT_PORTS_START(500gp)
	PORT_INCLUDE(s23)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Select Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Select Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_START1) // BUTTON1
	PORT_BIT(0x00001000, IP_ACTIVE_HIGH, IPT_BUTTON4) PORT_NAME("Tank") // BUTTON6
	PORT_BIT(0x00000200, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("View Button") // BUTTON9
	PORT_CONFNAME(0x00000100, 0x00000100, DEF_STR(Cabinet)) // BUTTON10
	PORT_CONFSETTING(0x00000100, DEF_STR(Standard))
	PORT_CONFSETTING(0x00000000, "Deluxe")
	PORT_BIT(0x00e0ec01, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON2/BUTTON3/BUTTON4/BUTTON5/BUTTON7/BUTTON8/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Throttle")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL2) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Brake")

	PORT_MODIFY("JVS_ANALOG_INPUT3")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Bank")
INPUT_PORTS_END

static INPUT_PORTS_START(aking)
	PORT_INCLUDE(s23)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Up Select")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Down Select")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Enter") // BUTTON1

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_PADDLE) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Arm Yaw") PORT_REVERSE

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_PADDLE_V) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Arm Pitch")

	PORT_MODIFY("JVS_ANALOG_INPUT3")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Rod Yaw")

	PORT_MODIFY("JVS_ANALOG_INPUT4")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Rod Pitch")

	PORT_MODIFY("JVS_ROTARY_INPUT1")
	PORT_BIT(0xffff, 0, IPT_DIAL_V) PORT_SENSITIVITY(100) PORT_KEYDELTA(8) PORT_NAME("Reel")
INPUT_PORTS_END

static INPUT_PORTS_START(downhill)
	PORT_INCLUDE(s23)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Select Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Select Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000400, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Right Brake") // BUTTON8
	PORT_BIT(0x00000200, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Left Brake") // BUTTON9
	PORT_BIT(0x00000100, IP_ACTIVE_HIGH, IPT_START1) // BUTTON10
	PORT_BIT(0x00e0f801, IP_ACTIVE_HIGH, IPT_BUTTON2) // BUTTON2/BUTTON3/BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT7")
	PORT_BIT(0xffff, 0x8000, IPT_PADDLE) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Steering")

	PORT_MODIFY("JVS_ROTARY_INPUT1")
	PORT_BIT(0xffff, 0, IPT_DIAL_V) PORT_SENSITIVITY(100) PORT_KEYDELTA(8) PORT_NAME("Encoder")
INPUT_PORTS_END

static INPUT_PORTS_START(finfurl)
	PORT_INCLUDE(gorgon)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Whip Button L") // BUTTON2
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Whip Button R") // BUTTON3
	PORT_BIT(0x00e07f00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Swing")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x5000, 0xb000) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Handle") PORT_REVERSE
INPUT_PORTS_END

static INPUT_PORTS_START(finfurl2)
	PORT_INCLUDE(gmen)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Whip Button L") // BUTTON2
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Whip Button R") // BUTTON3
	PORT_BIT(0x00e07f00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Swing")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x5000, 0xb000) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Handle") PORT_REVERSE
INPUT_PORTS_END

static INPUT_PORTS_START(gunwars)
	PORT_INCLUDE(gmen)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Select Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Select Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_START) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000400, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Gun Trigger") // BUTTON8
	PORT_BIT(0x00e0fb01, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON2/BUTTON3/BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13/

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Slide Left/Right")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Slide Forward/Back")

	PORT_MODIFY("JVS_ANALOG_INPUT3")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Z) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Gun Yawning Left/Right")
INPUT_PORTS_END

static INPUT_PORTS_START(motoxgo)
	PORT_INCLUDE(s23)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Select Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Select Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_START1) // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("View") // BUTTON2
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_CUSTOM) // BUTTON3 // FREEZE/RELAY? motors ignored if pressed
	PORT_BIT(0x00e07f00, IP_ACTIVE_HIGH, IPT_BUTTON4) // BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_MINMAX(0x000, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Steering")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL) PORT_MINMAX(0x8000, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Throttle")

	PORT_MODIFY("JVS_ANALOG_INPUT3")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL2) PORT_MINMAX(0x0000, 0x8000) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Brake")

	PORT_MODIFY("JVS_ANALOG_INPUT4")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x0000, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Bank")
INPUT_PORTS_END

static INPUT_PORTS_START(panicprk)
	PORT_INCLUDE(s23)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Up Select")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Down Select")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_START1) // BUTTON2
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_START2) // BUTTON3
	PORT_BIT(0x00e07f00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x000, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_CENTERDELTA(0)

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_PLAYER(2) PORT_MINMAX(0x000, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_CENTERDELTA(0)
INPUT_PORTS_END

static INPUT_PORTS_START(raceon)
	PORT_INCLUDE(gmen)

	PORT_MODIFY("DSW")
	PORT_DIPNAME(0x10, 0x00, "Activate Wheel Test") PORT_DIPLOCATION("DIP:4")
	PORT_DIPSETTING(0x10, DEF_STR(On))
	PORT_DIPSETTING(0x00, DEF_STR(Off))

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Select Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Select Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Enter") // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("View Change") // BUTTON2
	PORT_BIT(0x00e0ff00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON3/BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13/

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_PADDLE) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Handle")

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Gas") PORT_REVERSE

	PORT_MODIFY("JVS_ANALOG_INPUT3")
	PORT_BIT(0xffff, 0x8000, IPT_PEDAL2) PORT_MINMAX(0x0100, 0xffff) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Brake") PORT_REVERSE
INPUT_PORTS_END

static INPUT_PORTS_START(rapidrvr)
	PORT_INCLUDE(gorgon)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("Service Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("Service Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Service Enter")
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_START1) // BUTTON2
	PORT_BIT(0x00e0ff00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON3/BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_ANALOG_INPUT1")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_Y) PORT_MINMAX(0x4c80, 0xb340) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Yaw") PORT_REVERSE

	PORT_MODIFY("JVS_ANALOG_INPUT2")
	PORT_BIT(0xffff, 0x8000, IPT_AD_STICK_X) PORT_MINMAX(0x4c80, 0xb340) PORT_SENSITIVITY(100) PORT_KEYDELTA(2560) PORT_NAME("Pitch")
INPUT_PORTS_END

static INPUT_PORTS_START(rapidrvrp)
	PORT_INCLUDE(rapidrvr)

	// To fully use test mode, both Service Mode dipswitches need to be enabled.
	// Some of the developer menus require you to navigate with the Dev keys,
	// but usually the User keys work fine too.
	PORT_MODIFY("P1")
	PORT_BIT(0x0001, IP_ACTIVE_LOW, IPT_BUTTON4) PORT_NAME("Dev Service D")
	PORT_BIT(0x0002, IP_ACTIVE_LOW, IPT_BUTTON5) PORT_NAME("Dev Service E") // I/O Unknown Status
	PORT_BIT(0x0004, IP_ACTIVE_LOW, IPT_BUTTON6) PORT_NAME("Dev Service F") // I/O Air Damper FR
	PORT_BIT(0x0008, IP_ACTIVE_LOW, IPT_BUTTON1) PORT_PLAYER(2) PORT_NAME("Dev Service A") // + I/O Air Damper RR
	PORT_BIT(0x0010, IP_ACTIVE_LOW, IPT_BUTTON7) PORT_NAME("Dev Service G")
	PORT_BIT(0x0020, IP_ACTIVE_LOW, IPT_BUTTON8) PORT_NAME("Dev Service H")
	PORT_BIT(0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN) PORT_PLAYER(2) PORT_NAME("Dev Service Down")
	PORT_BIT(0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP) PORT_PLAYER(2) PORT_NAME("Dev Service Up")
	PORT_BIT(0x0100, IP_ACTIVE_LOW, IPT_START2) PORT_NAME("Dev Start")
	PORT_BIT(0x0200, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x0400, IP_ACTIVE_LOW, IPT_BUTTON3) PORT_NAME("Dev Service C") // + I/O Air Damper FL
	PORT_BIT(0x0800, IP_ACTIVE_LOW, IPT_BUTTON2) PORT_PLAYER(2) PORT_NAME("Dev Service B") // + I/O Air Damper RL
	PORT_BIT(0x1000, IP_ACTIVE_LOW, IPT_BUTTON9)
	PORT_BIT(0x2000, IP_ACTIVE_LOW, IPT_BUTTON10)
	PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_BUTTON11)
	PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_BUTTON12)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("User Service Up")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("User Service Down")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("User Service Enter")
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_START1) PORT_NAME("User Start") // BUTTON2
	PORT_BIT(0x00e0ff00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON3/BUTTON4/BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("DSW")
	PORT_DIPNAME(0x08, 0x08, "Debug Messages") PORT_DIPLOCATION("DIP:5")
	PORT_DIPSETTING(0x08, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x40, 0x40, "Dev Service Mode") PORT_DIPLOCATION("DIP:2")
	PORT_DIPSETTING(0x40, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
	PORT_DIPNAME(0x80, 0x80, "User Service Mode") PORT_DIPLOCATION("DIP:1")
	PORT_DIPSETTING(0x80, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))
INPUT_PORTS_END

static INPUT_PORTS_START(timecrs2)
	PORT_INCLUDE(s23)

	// P068 (branch patch/linkplay-menu): DIP:5 (port :DSW mask 0x08) is the
	// switch that empirically gates the ROM's link-search flow (LINK-SETUP.md;
	// cfg: <port tag=":DSW" mask="8" value="0"> = On).  Relabelled from the s23
	// base "Unknown" HERE (timecrs2 family only) so other s23 games' unrelated
	// DIP:5 meanings keep their labels.  Tag/mask/defaults unchanged, so
	// existing per-instance cfg files keep working.  MODEL PROVENANCE: Fable 5.
	PORT_MODIFY("DSW")
	PORT_DIPNAME(0x08, 0x08, "Link Play Enabled") PORT_DIPLOCATION("DIP:5")
	PORT_DIPSETTING(0x08, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00000020, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_NAME("User Service Down")
	PORT_BIT(0x00000010, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_NAME("User Service Up")
	PORT_BIT(0x00000002, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("User Enter") // BUTTON1
	PORT_BIT(0x00000001, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Gun Trigger") // BUTTON2
	PORT_BIT(0x00008000, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Foot Pedal") // BUTTON3
	PORT_CONFNAME(0x00004000, 0x00000000, "Link ID" ) // BUTTON4
	PORT_CONFSETTING(         0x00004000, "Right/Blue" )
	PORT_CONFSETTING(         0x00000000, "Left/Red" )
	PORT_BIT(0x00e03f00, IP_ACTIVE_HIGH, IPT_UNUSED) // BUTTON5/BUTTON6/BUTTON7/BUTTON8/BUTTON9/BUTTON10/BUTTON11/BUTTON12/BUTTON13

	PORT_MODIFY("JVS_SCREEN_POSITION_INPUT_X1") // tuned for CRT
	PORT_BIT(0xfff, 91 + 733 / 2, IPT_LIGHTGUN_X) PORT_CROSSHAIR(X, 1.0, 0.0, 0) PORT_MINMAX(91, 91 + 733) PORT_SENSITIVITY(50) PORT_KEYDELTA(20)

	PORT_MODIFY("JVS_SCREEN_POSITION_INPUT_Y1") // tuned for CRT - can't shoot below the statusbar?
	PORT_BIT(0xfff, 38 + 247 / 2, IPT_LIGHTGUN_Y) PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_MINMAX(38, 38 + 247) PORT_SENSITIVITY(50) PORT_KEYDELTA(10)
INPUT_PORTS_END

static INPUT_PORTS_START(crszone)
	PORT_INCLUDE(timecrs2)

	PORT_MODIFY("JVS_PLAYER1")
	PORT_BIT(0x00002000, IP_ACTIVE_LOW, IPT_UNUSED) // BUTTON5 (Motor test shows NG if this is not pressed)

	PORT_MODIFY("JVS_SCREEN_POSITION_INPUT_X1")
	PORT_BIT(0xfff, 0x1bf, IPT_LIGHTGUN_X) PORT_CROSSHAIR(X, 1.0, 0.0, 0) PORT_MINMAX(0x040, 0x33f) PORT_SENSITIVITY(50) PORT_KEYDELTA(20)

	PORT_MODIFY("JVS_SCREEN_POSITION_INPUT_Y1")
	PORT_BIT(0xfff, 0x08f, IPT_LIGHTGUN_Y) PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_MINMAX(0x020, 0x0ff) PORT_SENSITIVITY(50) PORT_KEYDELTA(10)
INPUT_PORTS_END

// a note about "user1" ROMs:
// serial number data is at offset 0x201 (interleaved) and it's the only difference between sets marked as 'set 1' and 'set 2'
// TODO: verify if it's better to just keep one set and note the alternate serial numbers

ROM_START( rapidrvr )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "rd3verc.ic2",  0x000000, 0x200000, CRC(c15c0f30) SHA1(9f529232818f3e184f81f62408a5cad615b05613) )
	ROM_LOAD16_BYTE( "rd3verc.ic1",  0x000001, 0x200000, CRC(9d7f4411) SHA1(d049efaa539d36ed0f73ca3f50a8f7112e67f865) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "rd3verc.ic3",  0x000000, 0x080000, CRC(6e26fbaf) SHA1(4ab6637d22f0d26f7e1d10e9c80059c56f64303d) )

	ROM_REGION32_BE( 0x800000, "data", 0 )  /* data */
	ROM_LOAD16_BYTE( "rd1mtah.3j",   0x000000, 0x400000, CRC(d8fa0f3d) SHA1(0d5bdb3a2e7be1dffe11b74baa2c10bfe011ae92) )
	ROM_LOAD16_BYTE( "rd1mtal.1j",   0x000001, 0x400000, CRC(8f0efa86) SHA1(9953461c258f2a96be275a7b18d6518ddfac3860) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "rd1cgll.8b",   0x0000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7b",   0x0800000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6b",   0x1000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5b",   0x1800000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )

	ROM_REGION( 0x1000000, "sprites", 0 )   /* sprites tiles */
	ROM_LOAD( "rd1sprll.12t", 0x0000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11p", 0x0400000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10p", 0x0800000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9p",  0x0c00000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "rd1ccrl.11a",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "rd1ccrh.11b",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "rd1pt0h.9l",   0x0000000, 0x400000, CRC(6f280eff) SHA1(9dd8c8903581d7a412146e50f4009e1d2b743f06) )
	ROM_LOAD32_WORD_SWAP( "rd1pt0l.9j",   0x0000002, 0x400000, CRC(47b1c5a5) SHA1(021d4ca7b8674d8ed5daa701bf41b4a7164d992a) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1h.10l",  0x0800000, 0x400000, CRC(37bd9bdf) SHA1(b26c284024ea4ad4c67b2eefbfdd5ebb35a0118e) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1l.10j",  0x0800002, 0x400000, CRC(91131cb3) SHA1(e42c5e190c719f1cf2d6e91444062ab901be0e73) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2h.11l",  0x1000000, 0x400000, CRC(fa601e83) SHA1(45c420538910f566e75d668306735f54c901669f) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2l.11j",  0x1000002, 0x400000, CRC(3423ff9f) SHA1(73823c179c866cbb601a23417acbbf5b3dc97213) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3h.12l",  0x1800000, 0x400000, CRC(e82ff66a) SHA1(9e2c951136b26d969d2c9d030b7e0bad8bbbe3fb) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3l.12j",  0x1800002, 0x400000, CRC(7216d63e) SHA1(77088ff05c2630996f4bdc87fe466f9b97611467) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "rd1wavel.2s",  0x000000, 0x800000, CRC(bf52c08c) SHA1(6745062e078e520484390fad1f723124aa4076d0) )
	ROM_LOAD( "rd1waveh.3s",  0x800000, 0x800000, CRC(ef0136b5) SHA1(a6d923ededca168fe555e0b86a72f53bec5424cc) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "rd1cgll.8f",   0x000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7f",   0x000000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6f",   0x000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5f",   0x000000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )
	ROM_LOAD( "rd1sprll.12p", 0x000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11t", 0x000000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10t", 0x000000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9t",  0x000000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )
	ROM_LOAD( "rd1ccrl.11e",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )
	ROM_LOAD( "rd1ccrh.11f",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )
ROM_END


ROM_START( rapidrvrv2c )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "rd2verc.ic2",  0x000000, 0x200000, CRC(234fc2f4) SHA1(64374f4de19855f1980d8e088049b0c112107f43) )
	ROM_LOAD16_BYTE( "rd2verc.ic1",  0x000001, 0x200000, CRC(651c5da4) SHA1(0e73e2cfafda626597d2ce08bf07458509fb79de) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "rd2verc.ic3",  0x000000, 0x080000, CRC(6e26fbaf) SHA1(4ab6637d22f0d26f7e1d10e9c80059c56f64303d) )

	ROM_REGION32_BE( 0x800000, "data", 0 )  /* data */
	ROM_LOAD16_BYTE( "rd1mtah.3j",   0x000000, 0x400000, CRC(d8fa0f3d) SHA1(0d5bdb3a2e7be1dffe11b74baa2c10bfe011ae92) )
	ROM_LOAD16_BYTE( "rd1mtal.1j",   0x000001, 0x400000, CRC(8f0efa86) SHA1(9953461c258f2a96be275a7b18d6518ddfac3860) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "rd1cgll.8b",   0x0000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7b",   0x0800000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6b",   0x1000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5b",   0x1800000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )

	ROM_REGION( 0x1000000, "sprites", 0 )   /* sprites tiles */
	ROM_LOAD( "rd1sprll.12t", 0x0000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11p", 0x0400000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10p", 0x0800000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9p",  0x0c00000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "rd1ccrl.11a",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "rd1ccrh.11b",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "rd1pt0h.9l",   0x0000000, 0x400000, CRC(6f280eff) SHA1(9dd8c8903581d7a412146e50f4009e1d2b743f06) )
	ROM_LOAD32_WORD_SWAP( "rd1pt0l.9j",   0x0000002, 0x400000, CRC(47b1c5a5) SHA1(021d4ca7b8674d8ed5daa701bf41b4a7164d992a) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1h.10l",  0x0800000, 0x400000, CRC(37bd9bdf) SHA1(b26c284024ea4ad4c67b2eefbfdd5ebb35a0118e) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1l.10j",  0x0800002, 0x400000, CRC(91131cb3) SHA1(e42c5e190c719f1cf2d6e91444062ab901be0e73) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2h.11l",  0x1000000, 0x400000, CRC(fa601e83) SHA1(45c420538910f566e75d668306735f54c901669f) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2l.11j",  0x1000002, 0x400000, CRC(3423ff9f) SHA1(73823c179c866cbb601a23417acbbf5b3dc97213) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3h.12l",  0x1800000, 0x400000, CRC(e82ff66a) SHA1(9e2c951136b26d969d2c9d030b7e0bad8bbbe3fb) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3l.12j",  0x1800002, 0x400000, CRC(7216d63e) SHA1(77088ff05c2630996f4bdc87fe466f9b97611467) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "rd1wavel.2s",  0x000000, 0x800000, CRC(bf52c08c) SHA1(6745062e078e520484390fad1f723124aa4076d0) )
	ROM_LOAD( "rd1waveh.3s",  0x800000, 0x800000, CRC(ef0136b5) SHA1(a6d923ededca168fe555e0b86a72f53bec5424cc) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "rd1cgll.8f",   0x000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7f",   0x000000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6f",   0x000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5f",   0x000000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )
	ROM_LOAD( "rd1sprll.12p", 0x000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11t", 0x000000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10t", 0x000000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9t",  0x000000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )
	ROM_LOAD( "rd1ccrl.11e",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )
	ROM_LOAD( "rd1ccrh.11f",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )
ROM_END


ROM_START( rapidrvrp ) // prototype board
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "29f016.ic2",  0x000000, 0x200000, CRC(9f72a7cd) SHA1(06245f1d3cc6ffb5b0123a8eea0dc8338bdfc0d6) )
	ROM_LOAD16_BYTE( "29f016.ic1",  0x000001, 0x200000, CRC(d395a244) SHA1(7f7b7b75b4bf9ac8808a27afed87f503df28e49f) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "29f400.ic3",  0x000000, 0x080000, CRC(f194c942) SHA1(b581c97327dea092e30ba46ad630d10477343a39) )

	ROM_REGION32_BE( 0x800000, "data", 0 )  /* data */
	ROM_LOAD16_BYTE( "rd1mtah.3j",   0x000000, 0x400000, CRC(d8fa0f3d) SHA1(0d5bdb3a2e7be1dffe11b74baa2c10bfe011ae92) )
	ROM_LOAD16_BYTE( "rd1mtal.1j",   0x000001, 0x400000, CRC(8f0efa86) SHA1(9953461c258f2a96be275a7b18d6518ddfac3860) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "rd1cgll.8b",   0x0000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7b",   0x0800000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6b",   0x1000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5b",   0x1800000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )

	ROM_REGION( 0x1000000, "sprites", 0 )   /* sprites tiles */
	ROM_LOAD( "rd1sprll.12t", 0x0000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11p", 0x0400000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10p", 0x0800000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9p",  0x0c00000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "rd1ccrl.11a",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "rd1ccrh.11b",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "rd1pt0h.9l",   0x0000000, 0x400000, CRC(6f280eff) SHA1(9dd8c8903581d7a412146e50f4009e1d2b743f06) )
	ROM_LOAD32_WORD_SWAP( "rd1pt0l.9j",   0x0000002, 0x400000, CRC(47b1c5a5) SHA1(021d4ca7b8674d8ed5daa701bf41b4a7164d992a) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1h.10l",  0x0800000, 0x400000, CRC(37bd9bdf) SHA1(b26c284024ea4ad4c67b2eefbfdd5ebb35a0118e) )
	ROM_LOAD32_WORD_SWAP( "rd1pt1l.10j",  0x0800002, 0x400000, CRC(91131cb3) SHA1(e42c5e190c719f1cf2d6e91444062ab901be0e73) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2h.11l",  0x1000000, 0x400000, CRC(fa601e83) SHA1(45c420538910f566e75d668306735f54c901669f) )
	ROM_LOAD32_WORD_SWAP( "rd1pt2l.11j",  0x1000002, 0x400000, CRC(3423ff9f) SHA1(73823c179c866cbb601a23417acbbf5b3dc97213) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3h.12l",  0x1800000, 0x400000, CRC(e82ff66a) SHA1(9e2c951136b26d969d2c9d030b7e0bad8bbbe3fb) )
	ROM_LOAD32_WORD_SWAP( "rd1pt3l.12j",  0x1800002, 0x400000, CRC(7216d63e) SHA1(77088ff05c2630996f4bdc87fe466f9b97611467) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "rd1wavel.2s",  0x000000, 0x800000, CRC(bf52c08c) SHA1(6745062e078e520484390fad1f723124aa4076d0) )
	ROM_LOAD( "rd1waveh.3s",  0x800000, 0x800000, CRC(ef0136b5) SHA1(a6d923ededca168fe555e0b86a72f53bec5424cc) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "rd1cgll.8f",   0x000000, 0x800000, CRC(b58b92ac) SHA1(70ee6e0e5347e05817aa30d53d766b8ce0fc44e4) )
	ROM_LOAD( "rd1cglm.7f",   0x000000, 0x800000, CRC(447067fa) SHA1(e2052373773594feb303e1924a4a820cf34ab55b) )
	ROM_LOAD( "rd1cgum.6f",   0x000000, 0x800000, CRC(c50de2ef) SHA1(24758a72b3569ce6a643a5786fce7c34b8aa692d) )
	ROM_LOAD( "rd1cguu.5f",   0x000000, 0x800000, CRC(611bab41) SHA1(84cddb2b63bf8336e92aecb06eddf1b34af73540) )
	ROM_LOAD( "rd1sprll.12p", 0x000000, 0x400000, CRC(8d450259) SHA1(27cccd1e7dad8880147bb85185982d8d27076e69) )
	ROM_LOAD( "rd1sprlm.11t", 0x000000, 0x400000, CRC(6c8db3a5) SHA1(24d81fa11e9c835cddadec4cbd530738e258346c) )
	ROM_LOAD( "rd1sprum.10t", 0x000000, 0x400000, CRC(8e08b2c6) SHA1(a17331a4e41f677f604d1b74e7694cf920b03b66) )
	ROM_LOAD( "rd1spruu.9t",  0x000000, 0x400000, CRC(f20a9673) SHA1(e5f1d552b0c42e102593ab578ff0b9ff814f8650) )
	ROM_LOAD( "rd1ccrl.11e",  0x000000, 0x200000, CRC(b0ea2b32) SHA1(0dc45846725b0de619bc6bae69e3eb166ed21bf0) )
	ROM_LOAD( "rd1ccrh.11f",  0x000000, 0x200000, CRC(fafffb86) SHA1(15b0ba0252b99d0cac29fcb374fb895643f528fe) )
ROM_END


ROM_START( finfurl )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ff2vera.ic2",  0x000000, 0x200000, CRC(e10f9dfa) SHA1(6f6989cd722fec5e3ed3ad1bb4866c5831041ae1) )
	ROM_LOAD16_BYTE( "ff2vera.ic1",  0x000001, 0x200000, CRC(5a90ffbf) SHA1(e22dc0ae2d3c3b3a521369fe3f63412ae2ae0a12) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ff2vera.ic3",  0x000000, 0x080000, CRC(ab681078) SHA1(ec8367404458a54893ab6bea29c8a2ba3272b816) )

	ROM_REGION32_BE( 0x800000, "data", 0 )  /* data */
	ROM_LOAD16_BYTE( "ff2mtah.3j",   0x000000, 0x400000, CRC(161003cd) SHA1(04409333a4776b17700fc6d1aa06a39560132e03) )
	ROM_LOAD16_BYTE( "ff2mtal.1j",   0x000001, 0x400000, CRC(ed1a5bf2) SHA1(bd05388a125a0201a41af95fb2aa5fe1c8b0f270) )

	ROM_REGION( 0x1000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ff2cgll.8b",   0x0000000, 0x400000, CRC(8e6c34eb) SHA1(795631c8019011246ed1e5546de4433dc22dd9e7) )
	ROM_LOAD( "ff2cglm.7b",   0x0400000, 0x400000, CRC(406f321b) SHA1(41a2b0229d5370f141b9d6a4e1801e2f9973f660) )
	ROM_LOAD( "ff2cgum.6b",   0x0800000, 0x400000, CRC(b808be59) SHA1(906bfbb5d34feef9697da545a93930fe6e56685c) )
	ROM_LOAD( "ff2cguu.5b",   0x0c00000, 0x400000, CRC(595deee4) SHA1(b29ff9c6ba17737f1f87c05b2d899d80b0b72dbb) )

	ROM_REGION( 0x1000000, "sprites", 0 )   /* texture tiles bank 2? */
	ROM_LOAD( "ff2sprll.12t", 0x0000000, 0x400000, CRC(1b305a13) SHA1(3d213a77b7a019fe4511097e7a27aa0688a3a586) )
	ROM_LOAD( "ff2sprlm.11p", 0x0400000, 0x400000, CRC(421a8fbf) SHA1(8bd6f3e1ac9c7b0ac9d25dfbce35f5b7a5d5bcc7) )
	ROM_LOAD( "ff2sprum.10p", 0x0800000, 0x400000, CRC(cb53c03e) SHA1(c39a44cad240c5b77c235c07ea700f9847ab9482) )
	ROM_LOAD( "ff2spruu.9p",  0x0c00000, 0x400000, CRC(c134b0de) SHA1(cea9d9f4ce2f45a93c797ed467d8458521db9b3d) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "ff2ccrl.11a",  0x000000, 0x200000, CRC(f1f9e77c) SHA1(adf659a4671ea066817e6620b7d7d5f60f6e01e5) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ff2ccrh.11b",  0x000000, 0x200000, CRC(71228c61) SHA1(b39d0b51f36c0d00a6144ae20613bebee3ed22bc) )

	ROM_REGION32_BE( 0x800000, "pointrom", 0 )  /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ff2pt0h.9l",   0x000000, 0x400000, CRC(344ce7a5) SHA1(79d2c4495b47592be4dee6e39294dd3194eb1d5f) )
	ROM_LOAD32_WORD_SWAP( "ff2pt0l.9j",   0x000002, 0x400000, CRC(7eeda441) SHA1(78648559abec5e1f04622cd1cfd5d94bddda7dbf) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ff2wavel.2s",  0x000000, 0x800000, CRC(6235c605) SHA1(521eaee80ac17c0936877d49394e5390fa0ff8a0) )
	ROM_LOAD( "ff2waveh.3s",  0x800000, 0x800000, CRC(2a59492a) SHA1(886ec0a4a71048d65f93c52df96416e74d23b3ec) )

	ROM_REGION( 0x400000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "ff2cguu.5f",   0x000000, 0x400000, CRC(595deee4) SHA1(b29ff9c6ba17737f1f87c05b2d899d80b0b72dbb) )
	ROM_LOAD( "ff2cgum.6f",   0x000000, 0x400000, CRC(b808be59) SHA1(906bfbb5d34feef9697da545a93930fe6e56685c) )
	ROM_LOAD( "ff2cgll.8f",   0x000000, 0x400000, CRC(8e6c34eb) SHA1(795631c8019011246ed1e5546de4433dc22dd9e7) )
	ROM_LOAD( "ff2cglm.7f",   0x000000, 0x400000, CRC(406f321b) SHA1(41a2b0229d5370f141b9d6a4e1801e2f9973f660) )
	ROM_LOAD( "ff2spruu.9t",  0x000000, 0x400000, CRC(c134b0de) SHA1(cea9d9f4ce2f45a93c797ed467d8458521db9b3d) )
	ROM_LOAD( "ff2sprum.10t", 0x000000, 0x400000, CRC(cb53c03e) SHA1(c39a44cad240c5b77c235c07ea700f9847ab9482) )
	ROM_LOAD( "ff2sprll.12p", 0x000000, 0x400000, CRC(1b305a13) SHA1(3d213a77b7a019fe4511097e7a27aa0688a3a586) )
	ROM_LOAD( "ff2sprlm.11t", 0x000000, 0x400000, CRC(421a8fbf) SHA1(8bd6f3e1ac9c7b0ac9d25dfbce35f5b7a5d5bcc7) )
	ROM_LOAD( "ff2ccrl.11e",  0x000000, 0x200000, CRC(f1f9e77c) SHA1(adf659a4671ea066817e6620b7d7d5f60f6e01e5) )
	ROM_LOAD( "ff2ccrh.11f",  0x000000, 0x200000, CRC(71228c61) SHA1(b39d0b51f36c0d00a6144ae20613bebee3ed22bc) )
ROM_END


ROM_START( motoxgo )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "mg3vera.ic2",  0x000000, 0x200000, CRC(1bf06f00) SHA1(e9d04e9f19bff7a58cb280dd1d5db12801b68ba0) )
	ROM_LOAD16_BYTE( "mg3vera.ic1",  0x000001, 0x200000, CRC(f5e6e25b) SHA1(1de30e8e831be66987112645a9db3a3001b89fe6) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "mg3vera.ic3",  0x000000, 0x080000, CRC(9e3d46a8) SHA1(9ffa5b91ea51cc0fb97def25ce47efa3441f3c6f) )

	ROM_REGION( 0x20000, "exioboard", 0 )   /* "extra" I/O board (uses Fujitsu MB90611A MCU) */
	ROM_LOAD( "mg1prog0a.3a", 0x000000, 0x020000, CRC(b2b5be8f) SHA1(803652b7b8fde2196b7fb742ba8b9843e4fcd2de) )

	ROM_REGION32_BE( 0x2000000, "data", ROMREGION_ERASEFF ) /* data ROMs */
	ROM_LOAD16_BYTE( "mg1mtah.2j",   0x000000, 0x800000, CRC(845f4768) SHA1(9c03b1f6dcd9d1f43c2958d855221be7f9415c47) )
	ROM_LOAD16_BYTE( "mg1mtal.2h",   0x000001, 0x800000, CRC(fdad0f0a) SHA1(420d50f012af40f80b196d3aae320376e6c32367) )

	ROM_REGION( 0x2000000, "textile", ROMREGION_ERASEFF )   /* texture tiles */
	ROM_LOAD( "mg1cgll.4m",   0x0000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.4k",   0x0800000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.4j",   0x1000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "mg1ccrl.7f",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "mg1ccrh.7e",   0x000000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )

	ROM_REGION32_BE( 0x1000000, "pointrom", ROMREGION_ERASEFF ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "mg1pt0h.7a",   0x000000, 0x400000, CRC(c9ba1b47) SHA1(42ec0638edb4c502ff0a340c4cf590bdd767cfe2) )
	ROM_LOAD32_WORD_SWAP( "mg1pt0l.7c",   0x000002, 0x400000, CRC(3b9e95d3) SHA1(d7823ed6c590669ccd4098ed439599a3eb814ed1) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1h.5a",   0x800000, 0x400000, CRC(8d4f7097) SHA1(004e9ed0b5d6ce83ffadb9bd429fa7560abdb598) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1l.5c",   0x800002, 0x400000, CRC(0dd2f358) SHA1(3537e6be3fec9fec8d5a8dd02d9cf67b3805f8f0) )

	ROM_REGION( 0x1000000, "c352", ROMREGION_ERASEFF ) /* C352 PCM samples */
	ROM_LOAD( "mg1wavel.2c",  0x000000, 0x800000, CRC(f78b1b4d) SHA1(47cd654ec0a69de0dc81b8d83692eebf5611228b) )
	ROM_LOAD( "mg1waveh.2a",  0x800000, 0x800000, CRC(8cb73877) SHA1(2e2b170c7ff889770c13b4ab7ac316b386ada153) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "mg1cgll.5m",   0x000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.5k",   0x000000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.5j",   0x000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )
	ROM_LOAD( "mg1ccrl.7m",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )
	ROM_LOAD( "mg1ccrh.7k",   0x400000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )
ROM_END


ROM_START( motoxgov2a )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "mg2vera.ic2",  0x000000, 0x200000, CRC(66093336) SHA1(c87874245a70a1642fb9ecfc94cbbc89f0fd633f) )
	ROM_LOAD16_BYTE( "mg2vera.ic1",  0x000001, 0x200000, CRC(3dc7736f) SHA1(c5137aa449918a124415f8ea5581e037f841129c) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "mg3vera.ic3",  0x000000, 0x080000, CRC(9e3d46a8) SHA1(9ffa5b91ea51cc0fb97def25ce47efa3441f3c6f) )

	ROM_REGION( 0x20000, "exioboard", 0 )   /* "extra" I/O board (uses Fujitsu MB90611A MCU) */
	ROM_LOAD( "mg1prog0a.3a", 0x000000, 0x020000, CRC(b2b5be8f) SHA1(803652b7b8fde2196b7fb742ba8b9843e4fcd2de) )

	ROM_REGION32_BE( 0x2000000, "data", ROMREGION_ERASEFF ) /* data ROMs */
	ROM_LOAD16_BYTE( "mg1mtah.2j",   0x000000, 0x800000, CRC(845f4768) SHA1(9c03b1f6dcd9d1f43c2958d855221be7f9415c47) )
	ROM_LOAD16_BYTE( "mg1mtal.2h",   0x000001, 0x800000, CRC(fdad0f0a) SHA1(420d50f012af40f80b196d3aae320376e6c32367) )

	ROM_REGION( 0x2000000, "textile", ROMREGION_ERASEFF )   /* texture tiles */
	ROM_LOAD( "mg1cgll.4m",   0x0000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.4k",   0x0800000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.4j",   0x1000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "mg1ccrl.7f",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "mg1ccrh.7e",   0x000000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )

	ROM_REGION32_BE( 0x1000000, "pointrom", ROMREGION_ERASEFF ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "mg1pt0h.7a",   0x000000, 0x400000, CRC(c9ba1b47) SHA1(42ec0638edb4c502ff0a340c4cf590bdd767cfe2) )
	ROM_LOAD32_WORD_SWAP( "mg1pt0l.7c",   0x000002, 0x400000, CRC(3b9e95d3) SHA1(d7823ed6c590669ccd4098ed439599a3eb814ed1) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1h.5a",   0x800000, 0x400000, CRC(8d4f7097) SHA1(004e9ed0b5d6ce83ffadb9bd429fa7560abdb598) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1l.5c",   0x800002, 0x400000, CRC(0dd2f358) SHA1(3537e6be3fec9fec8d5a8dd02d9cf67b3805f8f0) )

	ROM_REGION( 0x1000000, "c352", ROMREGION_ERASEFF ) /* C352 PCM samples */
	ROM_LOAD( "mg1wavel.2c",  0x000000, 0x800000, CRC(f78b1b4d) SHA1(47cd654ec0a69de0dc81b8d83692eebf5611228b) )
	ROM_LOAD( "mg1waveh.2a",  0x800000, 0x800000, CRC(8cb73877) SHA1(2e2b170c7ff889770c13b4ab7ac316b386ada153) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "mg1cgll.5m",   0x000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.5k",   0x000000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.5j",   0x000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )
	ROM_LOAD( "mg1ccrl.7m",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )
	ROM_LOAD( "mg1ccrh.7k",   0x400000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )
ROM_END


ROM_START( motoxgov1a )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "mg1vera.ic2",  0x000000, 0x200000, CRC(5ba13d9e) SHA1(7f6484df644772f2478155c05844532f8abbd196) )
	ROM_LOAD16_BYTE( "mg1vera.ic1",  0x000001, 0x200000, CRC(193b463e) SHA1(f62eed49f7f8bf01b8b4deb1578ddee1d4a54ca3) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "mg3vera.ic3",  0x000000, 0x080000, CRC(9e3d46a8) SHA1(9ffa5b91ea51cc0fb97def25ce47efa3441f3c6f) )

	ROM_REGION( 0x20000, "exioboard", 0 )   /* "extra" I/O board (uses Fujitsu MB90611A MCU) */
	ROM_LOAD( "mg1prog0a.3a", 0x000000, 0x020000, CRC(b2b5be8f) SHA1(803652b7b8fde2196b7fb742ba8b9843e4fcd2de) )

	ROM_REGION32_BE( 0x2000000, "data", ROMREGION_ERASEFF ) /* data ROMs */
	ROM_LOAD16_BYTE( "mg1mtah.2j",   0x000000, 0x800000, CRC(845f4768) SHA1(9c03b1f6dcd9d1f43c2958d855221be7f9415c47) )
	ROM_LOAD16_BYTE( "mg1mtal.2h",   0x000001, 0x800000, CRC(fdad0f0a) SHA1(420d50f012af40f80b196d3aae320376e6c32367) )

	ROM_REGION( 0x2000000, "textile", ROMREGION_ERASEFF )   /* texture tiles */
	ROM_LOAD( "mg1cgll.4m",   0x0000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.4k",   0x0800000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.4j",   0x1000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "mg1ccrl.7f",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "mg1ccrh.7e",   0x000000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )

	ROM_REGION32_BE( 0x1000000, "pointrom", ROMREGION_ERASEFF ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "mg1pt0h.7a",   0x000000, 0x400000, CRC(c9ba1b47) SHA1(42ec0638edb4c502ff0a340c4cf590bdd767cfe2) )
	ROM_LOAD32_WORD_SWAP( "mg1pt0l.7c",   0x000002, 0x400000, CRC(3b9e95d3) SHA1(d7823ed6c590669ccd4098ed439599a3eb814ed1) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1h.5a",   0x800000, 0x400000, CRC(8d4f7097) SHA1(004e9ed0b5d6ce83ffadb9bd429fa7560abdb598) )
	ROM_LOAD32_WORD_SWAP( "mg1pt1l.5c",   0x800002, 0x400000, CRC(0dd2f358) SHA1(3537e6be3fec9fec8d5a8dd02d9cf67b3805f8f0) )

	ROM_REGION( 0x1000000, "c352", ROMREGION_ERASEFF ) /* C352 PCM samples */
	ROM_LOAD( "mg1wavel.2c",  0x000000, 0x800000, CRC(f78b1b4d) SHA1(47cd654ec0a69de0dc81b8d83692eebf5611228b) )
	ROM_LOAD( "mg1waveh.2a",  0x800000, 0x800000, CRC(8cb73877) SHA1(2e2b170c7ff889770c13b4ab7ac316b386ada153) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "mg1cgll.5m",   0x000000, 0x800000, CRC(175dfe34) SHA1(66ae35b0084159aea1afeb1a6486fffa635992b5) )
	ROM_LOAD( "mg1cglm.5k",   0x000000, 0x800000, CRC(b3e648e7) SHA1(98018ae2276f905a7f74e1dab540a44247524436) )
	ROM_LOAD( "mg1cgum.5j",   0x000000, 0x800000, CRC(46a77d73) SHA1(132ce2452ee68ba374e98b59032ac0a1a277078d) )
	ROM_LOAD( "mg1ccrl.7m",   0x000000, 0x400000, CRC(5372e300) SHA1(63a49782289ed93a321ca7d193241fb83ca97e6b) )
	ROM_LOAD( "mg1ccrh.7k",   0x400000, 0x200000, CRC(2e77597d) SHA1(58dd83c1b0c08115e728c5e7dea5e62135b821ba) )
ROM_END


ROM_START( timecrs2 )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "tss3verb.2",   0x000000, 0x200000, CRC(c7be691f) SHA1(5e2e7a0db3d8ce6dfeb6c0d99e9fe6a9f9cab467) )
	ROM_LOAD16_BYTE( "tss3verb.1",   0x000001, 0x200000, CRC(6e3f232b) SHA1(8007d8f31a605a5df89938d7c9f9d3d209c934be) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "tss1vera.3",   0x000000, 0x080000, CRC(41e41994) SHA1(eabc1a307c329070bfc6486cb68169c94ff8a162) ) /* Flash ROM type 29F400TC - Common code throughout all versions */

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "tss1mtah.2j",  0x0000000, 0x800000, CRC(697c26ed) SHA1(72f6f69e89496ba0c6183b35c3bde71f5a3c721f) )
	ROM_LOAD16_BYTE( "tss1mtal.2h",  0x0000001, 0x800000, CRC(bfc79190) SHA1(04bda00c4cc5660d27af4f3b0ee3550dea8d3805) )
	ROM_LOAD16_BYTE( "tss1mtbh.2m",  0x1000000, 0x800000, CRC(82582776) SHA1(7c790d09bac660ea1c62da3ffb21ab43f2461594) )
	ROM_LOAD16_BYTE( "tss1mtbl.2f",  0x1000001, 0x800000, CRC(e648bea4) SHA1(3803d03e72b25fbcc124d5b25066d25629b76b94) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "tss1cgll.4m",  0x0000000, 0x800000, CRC(18433aaa) SHA1(08539beb2e66ec4e41062621fc098b121c669546) )
	ROM_LOAD( "tss1cglm.4k",  0x0800000, 0x800000, CRC(669974c2) SHA1(cfebe199631e38f547b38fcd35f1645b74e8dd0a) )
	ROM_LOAD( "tss1cgum.4j",  0x1000000, 0x800000, CRC(c22739e1) SHA1(8671ee047bb248033656c50befd1c35e5e478e1a) )
	ROM_LOAD( "tss1cguu.4f",  0x1800000, 0x800000, CRC(76924e04) SHA1(751065d6ce658cbbcd88f854f6937ebd2204ec68) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "tss1ccrl.7f",  0x000000, 0x400000, CRC(3a325fe7) SHA1(882735dce7aeb36f9e88a983498360f5de901e9d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "tss1ccrh.7e",  0x000000, 0x200000, CRC(f998de1a) SHA1(371f540f505608297c5ffcfb623b983ca8310afb) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "tss1pt0h.7a",  0x0000000, 0x400000, CRC(cdbe0ba8) SHA1(f8c6da31654c0a2a8024888ffb7fc1c783b2d629) )
	ROM_LOAD32_WORD_SWAP( "tss1pt0l.7c",  0x0000002, 0x400000, CRC(896f0fb4) SHA1(bdfa99eb21ce4fc8021f9d95a5558a34f9942c57) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1h.5a",  0x0800000, 0x400000, CRC(63647596) SHA1(833412be8f61686bd7e06c2738df740e0e585d0f) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1l.5c",  0x0800002, 0x400000, CRC(5a09921f) SHA1(c23885708c7adf0b81c2c9346e21b869634a5b35) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2h.4a",  0x1000000, 0x400000, CRC(9b06e22d) SHA1(cff5ed098112a4f0a2bc8937e226f50066e605b1) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2l.4c",  0x1000002, 0x400000, CRC(4b230d79) SHA1(794cee0a19993e90913f58507c53224f361e9663) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "tss1wavel.2c", 0x000000, 0x800000, CRC(deaead26) SHA1(72dac0c3f41d4c3c290f9eb1b50236ae3040a472) )
	ROM_LOAD( "tss1waveh.2a", 0x800000, 0x800000, CRC(5c8758b4) SHA1(b85c8f6869900224ef83a2340b17f5bbb2801af9) )
ROM_END


ROM_START( timecrs2v2b )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "tss2verb.2", 0x000000, 0x200000, CRC(fb129049) SHA1(c975ea022b3a2a249a6ab60e2e0358f9dc507775) )
	ROM_LOAD16_BYTE( "tss2verb.1", 0x000001, 0x200000, CRC(2d6a1d3e) SHA1(2b6bc54427c1ae2fcdb57a33b2b2b00bd2065109) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "tss1vera.3",   0x000000, 0x080000, CRC(41e41994) SHA1(eabc1a307c329070bfc6486cb68169c94ff8a162) ) /* Flash ROM type 29F400TC - Common code throughout all versions */

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "tss1mtah.2j",  0x0000000, 0x800000, CRC(697c26ed) SHA1(72f6f69e89496ba0c6183b35c3bde71f5a3c721f) )
	ROM_LOAD16_BYTE( "tss1mtal.2h",  0x0000001, 0x800000, CRC(bfc79190) SHA1(04bda00c4cc5660d27af4f3b0ee3550dea8d3805) )
	ROM_LOAD16_BYTE( "tss1mtbh.2m",  0x1000000, 0x800000, CRC(82582776) SHA1(7c790d09bac660ea1c62da3ffb21ab43f2461594) )
	ROM_LOAD16_BYTE( "tss1mtbl.2f",  0x1000001, 0x800000, CRC(e648bea4) SHA1(3803d03e72b25fbcc124d5b25066d25629b76b94) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "tss1cgll.4m",  0x0000000, 0x800000, CRC(18433aaa) SHA1(08539beb2e66ec4e41062621fc098b121c669546) )
	ROM_LOAD( "tss1cglm.4k",  0x0800000, 0x800000, CRC(669974c2) SHA1(cfebe199631e38f547b38fcd35f1645b74e8dd0a) )
	ROM_LOAD( "tss1cgum.4j",  0x1000000, 0x800000, CRC(c22739e1) SHA1(8671ee047bb248033656c50befd1c35e5e478e1a) )
	ROM_LOAD( "tss1cguu.4f",  0x1800000, 0x800000, CRC(76924e04) SHA1(751065d6ce658cbbcd88f854f6937ebd2204ec68) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "tss1ccrl.7f",  0x000000, 0x400000, CRC(3a325fe7) SHA1(882735dce7aeb36f9e88a983498360f5de901e9d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "tss1ccrh.7e",  0x000000, 0x200000, CRC(f998de1a) SHA1(371f540f505608297c5ffcfb623b983ca8310afb) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "tss1pt0h.7a",  0x0000000, 0x400000, CRC(cdbe0ba8) SHA1(f8c6da31654c0a2a8024888ffb7fc1c783b2d629) )
	ROM_LOAD32_WORD_SWAP( "tss1pt0l.7c",  0x0000002, 0x400000, CRC(896f0fb4) SHA1(bdfa99eb21ce4fc8021f9d95a5558a34f9942c57) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1h.5a",  0x0800000, 0x400000, CRC(63647596) SHA1(833412be8f61686bd7e06c2738df740e0e585d0f) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1l.5c",  0x0800002, 0x400000, CRC(5a09921f) SHA1(c23885708c7adf0b81c2c9346e21b869634a5b35) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2h.4a",  0x1000000, 0x400000, CRC(9b06e22d) SHA1(cff5ed098112a4f0a2bc8937e226f50066e605b1) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2l.4c",  0x1000002, 0x400000, CRC(4b230d79) SHA1(794cee0a19993e90913f58507c53224f361e9663) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "tss1wavel.2c", 0x000000, 0x800000, CRC(deaead26) SHA1(72dac0c3f41d4c3c290f9eb1b50236ae3040a472) )
	ROM_LOAD( "tss1waveh.2a", 0x800000, 0x800000, CRC(5c8758b4) SHA1(b85c8f6869900224ef83a2340b17f5bbb2801af9) )
ROM_END


ROM_START( timecrs2v1b )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "tss1verb.2", 0x000000, 0x200000, CRC(10ebcb71) SHA1(9aeb9bf70f8a5949927ed84490b9212b19fe57ab) ) /* Flash ROM type 29F016A */
	ROM_LOAD16_BYTE( "tss1verb.1", 0x000001, 0x200000, CRC(4f9a38ef) SHA1(7e38dae63b68f7ab43355b22247938d6e8f43dee) ) /* Flash ROM type 29F016A */

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "tss1vera.3",   0x000000, 0x080000, CRC(41e41994) SHA1(eabc1a307c329070bfc6486cb68169c94ff8a162) ) /* Flash ROM type 29F400TC - Common code throughout all versions */

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "tss1mtah.2j",  0x0000000, 0x800000, CRC(697c26ed) SHA1(72f6f69e89496ba0c6183b35c3bde71f5a3c721f) )
	ROM_LOAD16_BYTE( "tss1mtal.2h",  0x0000001, 0x800000, CRC(bfc79190) SHA1(04bda00c4cc5660d27af4f3b0ee3550dea8d3805) )
	ROM_LOAD16_BYTE( "tss1mtbh.2m",  0x1000000, 0x800000, CRC(82582776) SHA1(7c790d09bac660ea1c62da3ffb21ab43f2461594) )
	ROM_LOAD16_BYTE( "tss1mtbl.2f",  0x1000001, 0x800000, CRC(e648bea4) SHA1(3803d03e72b25fbcc124d5b25066d25629b76b94) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "tss1cgll.4m",  0x0000000, 0x800000, CRC(18433aaa) SHA1(08539beb2e66ec4e41062621fc098b121c669546) )
	ROM_LOAD( "tss1cglm.4k",  0x0800000, 0x800000, CRC(669974c2) SHA1(cfebe199631e38f547b38fcd35f1645b74e8dd0a) )
	ROM_LOAD( "tss1cgum.4j",  0x1000000, 0x800000, CRC(c22739e1) SHA1(8671ee047bb248033656c50befd1c35e5e478e1a) )
	ROM_LOAD( "tss1cguu.4f",  0x1800000, 0x800000, CRC(76924e04) SHA1(751065d6ce658cbbcd88f854f6937ebd2204ec68) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "tss1ccrl.7f",  0x000000, 0x400000, CRC(3a325fe7) SHA1(882735dce7aeb36f9e88a983498360f5de901e9d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "tss1ccrh.7e",  0x000000, 0x200000, CRC(f998de1a) SHA1(371f540f505608297c5ffcfb623b983ca8310afb) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "tss1pt0h.7a",  0x0000000, 0x400000, CRC(cdbe0ba8) SHA1(f8c6da31654c0a2a8024888ffb7fc1c783b2d629) )
	ROM_LOAD32_WORD_SWAP( "tss1pt0l.7c",  0x0000002, 0x400000, CRC(896f0fb4) SHA1(bdfa99eb21ce4fc8021f9d95a5558a34f9942c57) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1h.5a",  0x0800000, 0x400000, CRC(63647596) SHA1(833412be8f61686bd7e06c2738df740e0e585d0f) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1l.5c",  0x0800002, 0x400000, CRC(5a09921f) SHA1(c23885708c7adf0b81c2c9346e21b869634a5b35) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2h.4a",  0x1000000, 0x400000, CRC(9b06e22d) SHA1(cff5ed098112a4f0a2bc8937e226f50066e605b1) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2l.4c",  0x1000002, 0x400000, CRC(4b230d79) SHA1(794cee0a19993e90913f58507c53224f361e9663) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "tss1wavel.2c", 0x000000, 0x800000, CRC(deaead26) SHA1(72dac0c3f41d4c3c290f9eb1b50236ae3040a472) )
	ROM_LOAD( "tss1waveh.2a", 0x800000, 0x800000, CRC(5c8758b4) SHA1(b85c8f6869900224ef83a2340b17f5bbb2801af9) )
ROM_END


ROM_START( timecrs2v4a )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "tss4vera.2",   0x000000, 0x200000, CRC(c84edd3b) SHA1(0b577a8ef6e74afa991dd81c2db19041787724da) )
	ROM_LOAD16_BYTE( "tss4vera.1",   0x000001, 0x200000, CRC(26f57c83) SHA1(c8983c26b7524a35257a242b66a9413eb354ca0d) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "tss1vera.3",   0x000000, 0x080000, CRC(41e41994) SHA1(eabc1a307c329070bfc6486cb68169c94ff8a162) ) /* Flash ROM type 29F400TC - Common code throughout all versions */

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "tss1mtah.2j",  0x0000000, 0x800000, CRC(697c26ed) SHA1(72f6f69e89496ba0c6183b35c3bde71f5a3c721f) )
	ROM_LOAD16_BYTE( "tss1mtal.2h",  0x0000001, 0x800000, CRC(bfc79190) SHA1(04bda00c4cc5660d27af4f3b0ee3550dea8d3805) )
	ROM_LOAD16_BYTE( "tss1mtbh.2m",  0x1000000, 0x800000, CRC(82582776) SHA1(7c790d09bac660ea1c62da3ffb21ab43f2461594) )
	ROM_LOAD16_BYTE( "tss1mtbl.2f",  0x1000001, 0x800000, CRC(e648bea4) SHA1(3803d03e72b25fbcc124d5b25066d25629b76b94) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "tss1cgll.4m",  0x0000000, 0x800000, CRC(18433aaa) SHA1(08539beb2e66ec4e41062621fc098b121c669546) )
	ROM_LOAD( "tss1cglm.4k",  0x0800000, 0x800000, CRC(669974c2) SHA1(cfebe199631e38f547b38fcd35f1645b74e8dd0a) )
	ROM_LOAD( "tss1cgum.4j",  0x1000000, 0x800000, CRC(c22739e1) SHA1(8671ee047bb248033656c50befd1c35e5e478e1a) )
	ROM_LOAD( "tss1cguu.4f",  0x1800000, 0x800000, CRC(76924e04) SHA1(751065d6ce658cbbcd88f854f6937ebd2204ec68) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "tss1ccrl.7f",  0x000000, 0x400000, CRC(3a325fe7) SHA1(882735dce7aeb36f9e88a983498360f5de901e9d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "tss1ccrh.7e",  0x000000, 0x200000, CRC(f998de1a) SHA1(371f540f505608297c5ffcfb623b983ca8310afb) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "tss1pt0h.7a",  0x0000000, 0x400000, CRC(cdbe0ba8) SHA1(f8c6da31654c0a2a8024888ffb7fc1c783b2d629) )
	ROM_LOAD32_WORD_SWAP( "tss1pt0l.7c",  0x0000002, 0x400000, CRC(896f0fb4) SHA1(bdfa99eb21ce4fc8021f9d95a5558a34f9942c57) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1h.5a",  0x0800000, 0x400000, CRC(63647596) SHA1(833412be8f61686bd7e06c2738df740e0e585d0f) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1l.5c",  0x0800002, 0x400000, CRC(5a09921f) SHA1(c23885708c7adf0b81c2c9346e21b869634a5b35) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2h.4a",  0x1000000, 0x400000, CRC(9b06e22d) SHA1(cff5ed098112a4f0a2bc8937e226f50066e605b1) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2l.4c",  0x1000002, 0x400000, CRC(4b230d79) SHA1(794cee0a19993e90913f58507c53224f361e9663) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "tss1wavel.2c", 0x000000, 0x800000, CRC(deaead26) SHA1(72dac0c3f41d4c3c290f9eb1b50236ae3040a472) )
	ROM_LOAD( "tss1waveh.2a", 0x800000, 0x800000, CRC(5c8758b4) SHA1(b85c8f6869900224ef83a2340b17f5bbb2801af9) )
ROM_END


ROM_START( timecrs2v5a )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "tss5vera.2",   0x000000, 0x200000, CRC(71ef4821) SHA1(03999c2c3219725f8716b7d32efbf810b6401806) )
	ROM_LOAD16_BYTE( "tss5vera.1",   0x000001, 0x200000, CRC(8dfcdd76) SHA1(ba8b2e7070814a225205010ab44ca73628e2913c) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "tss5vera.3",   0x000000, 0x080000, CRC(41e41994) SHA1(eabc1a307c329070bfc6486cb68169c94ff8a162) ) /* Flash ROM type 29F400TC - Common code throughout all versions */

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "tss1mtah.2j",  0x0000000, 0x800000, CRC(697c26ed) SHA1(72f6f69e89496ba0c6183b35c3bde71f5a3c721f) )
	ROM_LOAD16_BYTE( "tss1mtal.2h",  0x0000001, 0x800000, CRC(bfc79190) SHA1(04bda00c4cc5660d27af4f3b0ee3550dea8d3805) )
	ROM_LOAD16_BYTE( "tss1mtbh.2m",  0x1000000, 0x800000, CRC(82582776) SHA1(7c790d09bac660ea1c62da3ffb21ab43f2461594) )
	ROM_LOAD16_BYTE( "tss1mtbl.2f",  0x1000001, 0x800000, CRC(e648bea4) SHA1(3803d03e72b25fbcc124d5b25066d25629b76b94) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "tss1cgll.4m",  0x0000000, 0x800000, CRC(18433aaa) SHA1(08539beb2e66ec4e41062621fc098b121c669546) )
	ROM_LOAD( "tss1cglm.4k",  0x0800000, 0x800000, CRC(669974c2) SHA1(cfebe199631e38f547b38fcd35f1645b74e8dd0a) )
	ROM_LOAD( "tss1cgum.4j",  0x1000000, 0x800000, CRC(c22739e1) SHA1(8671ee047bb248033656c50befd1c35e5e478e1a) )
	ROM_LOAD( "tss1cguu.4f",  0x1800000, 0x800000, CRC(76924e04) SHA1(751065d6ce658cbbcd88f854f6937ebd2204ec68) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "tss1ccrl.7f",  0x000000, 0x400000, CRC(3a325fe7) SHA1(882735dce7aeb36f9e88a983498360f5de901e9d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "tss1ccrh.7e",  0x000000, 0x200000, CRC(f998de1a) SHA1(371f540f505608297c5ffcfb623b983ca8310afb) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "tss1pt0h.7a",  0x0000000, 0x400000, CRC(cdbe0ba8) SHA1(f8c6da31654c0a2a8024888ffb7fc1c783b2d629) )
	ROM_LOAD32_WORD_SWAP( "tss1pt0l.7c",  0x0000002, 0x400000, CRC(896f0fb4) SHA1(bdfa99eb21ce4fc8021f9d95a5558a34f9942c57) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1h.5a",  0x0800000, 0x400000, CRC(63647596) SHA1(833412be8f61686bd7e06c2738df740e0e585d0f) )
	ROM_LOAD32_WORD_SWAP( "tss1pt1l.5c",  0x0800002, 0x400000, CRC(5a09921f) SHA1(c23885708c7adf0b81c2c9346e21b869634a5b35) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2h.4a",  0x1000000, 0x400000, CRC(9b06e22d) SHA1(cff5ed098112a4f0a2bc8937e226f50066e605b1) )
	ROM_LOAD32_WORD_SWAP( "tss1pt2l.4c",  0x1000002, 0x400000, CRC(4b230d79) SHA1(794cee0a19993e90913f58507c53224f361e9663) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "tss1wavel.2c", 0x000000, 0x800000, CRC(deaead26) SHA1(72dac0c3f41d4c3c290f9eb1b50236ae3040a472) )
	ROM_LOAD( "tss1waveh.2a", 0x800000, 0x800000, CRC(5c8758b4) SHA1(b85c8f6869900224ef83a2340b17f5bbb2801af9) )
ROM_END


ROM_START( aking )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ag1vera.ic2",   0x000000, 0x200000, CRC(dc98fefb) SHA1(d173c5c6d23f1dae61d448bb6fae27daca525221) )
	ROM_LOAD16_BYTE( "ag1vera.ic1",   0x000001, 0x200000, CRC(f1a08d5c) SHA1(f11bee1093b237067b84ddec8e1bca0b70fc6678) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ag1vera.ic3",   0x000000, 0x080000, CRC(266ac71c) SHA1(648a64adc0e4a2cefd71c31a6a71359b6c196430) )

	ROM_REGION( 0x40000, "iocpu2", 0 ) // I/O board MB90F574 MCU code
	ROM_LOAD( "fcaf10.bin", 0x000000, 0x040000, NO_DUMP ) // 256KB internal flash ROM

	ROM_REGION( 0x10000, "iocpu3", 0 ) // I/O board PIC16F84 code
	ROM_LOAD( "fcap10.ic2", 0x000000, 0x004010, NO_DUMP )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "ag1mtah.2j",  0x0000000, 0x800000, CRC(f2d8ca9d) SHA1(8158d13d74f2aae7c0d1238619ce1ad3a17d8047) )
	ROM_LOAD16_BYTE( "ag1mtal.2h",  0x0000001, 0x800000, CRC(7facbfd4) SHA1(c42988e274a1b4f40f4b4379e94653ef07429c58) )
	ROM_LOAD16_BYTE( "ag1mtbh.2m",  0x1000000, 0x800000, CRC(890bdb52) SHA1(a38f039187448ee328547582eab22813ce625615) )
	ROM_LOAD16_BYTE( "ag1mtbl.2f",  0x1000001, 0x800000, CRC(62d771c9) SHA1(69a47af1366d351157131472756fd05e0fdbf87f) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ag1cgll.4m",  0x0000000, 0x800000, CRC(9db7e939) SHA1(7be8d6f6d1e236f2655784493bdf4f9869ecd6eb) )
	ROM_LOAD( "ag1cglm.4k",  0x0800000, 0x800000, CRC(17792dba) SHA1(367676870820e44b0092d5ff6d4ee4e80bbf91d2) )
	ROM_LOAD( "ag1cgum.4j",  0x1000000, 0x800000, CRC(5dfa863d) SHA1(a1cde62f00dd8b70538a8eba2aa7ec497cdcaa5c) )
	ROM_LOAD( "ag1cguu.4f",  0x1800000, 0x800000, CRC(86396786) SHA1(d20121eb7d595567cd3438c66ae4c07dbaaaaeb8) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "ag1ccrl.7f",  0x000000, 0x400000, CRC(86bbe1f9) SHA1(3d8484aadc48638ad2b6806118416ac69345e35a) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ag1ccrh.7e",  0x000000, 0x200000, CRC(abe2aab1) SHA1(b43ddf9b0f4a7ac75dc16fa5b2ed86ac5a273a50) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ag1pt0h.7a",  0x0000000, 0x400000, CRC(b5582ca7) SHA1(0e48b7e3595f9be4e9403a2db939ec140726a880) )
	ROM_LOAD32_WORD_SWAP( "ag1pt0l.7c",  0x0000002, 0x400000, CRC(10e7f54f) SHA1(caf1d28991a9d082b5ddc5def62586b09fa8aff2) )
	ROM_LOAD32_WORD_SWAP( "ag1pt1h.5a",  0x0800000, 0x400000, CRC(25e4776a) SHA1(31e7c9dd3aba01e425839a0ffe1eb0001ac16770) )
	ROM_LOAD32_WORD_SWAP( "ag1pt1l.5c",  0x0800002, 0x400000, CRC(5d3d7099) SHA1(d80d6b692c513945857bcd2c8cfc12b8ec0f3be5) )
	ROM_LOAD32_WORD_SWAP( "ag1pt2h.4a",  0x1000000, 0x400000, CRC(f0eb9012) SHA1(a867f09162e0b0a4eead0bd212df76ba1abb2c19) )
	ROM_LOAD32_WORD_SWAP( "ag1pt2l.4c",  0x1000002, 0x400000, CRC(bf92c054) SHA1(9d676c3bb63bf29d7b18fe5d7e6912a922f06350) )
	ROM_LOAD32_WORD_SWAP( "ag1pt3h.3a",  0x1800000, 0x400000, CRC(e11a12ce) SHA1(4ee78a4d7ada9c26734132baac47b0cbede3d4fd) )
	ROM_LOAD32_WORD_SWAP( "ag1pt3l.3c",  0x1800002, 0x400000, CRC(04b475db) SHA1(3ea28e51185dc2c2bfa50a87031580524eaacc4a) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ag1wavel.2c", 0x000000, 0x800000, CRC(d7fefbd4) SHA1(2cf31661feb6aef40621621897be8e0bc248c1d9) )
	ROM_LOAD( "ag1waveh.2a", 0x800000, 0x800000, CRC(37a61daa) SHA1(34632809f49975d9dc4c76b09ef896df0bc03a52) )
ROM_END


ROM_START( 500gp )
	/* r4650-generic-xrom-generic: NMON 1.0.8-sys23-19990105 P for SYSTEM23 P1 */
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "5gp3verc.2",   0x000000, 0x200000, CRC(e2d43468) SHA1(5e861dd223c7fa177febed9803ac353cba18e19d) )
	ROM_LOAD16_BYTE( "5gp3verc.1",   0x000001, 0x200000, CRC(f6efc94a) SHA1(785eee2bec5080d4e8ef836f28d446328c942b0e) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "5gp3verc.3",   0x000000, 0x080000, CRC(b323abdf) SHA1(8962e39b48a7074a2d492afb5db3f5f3e5ae2389) )

	ROM_REGION( 0x40000, "iocpu2", 0 ) // I/O board MB90F574 MCU code
	ROM_LOAD( "fcaf10.bin", 0x000000, 0x040000, NO_DUMP ) // 256KB internal flash ROM

	ROM_REGION( 0x10000, "iocpu3", 0 ) // I/O board PIC16F84 code
	ROM_LOAD( "fcap10.ic2", 0x000000, 0x004010, NO_DUMP )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "5gp1mtah.2j",  0x0000000, 0x800000, CRC(246e4b7a) SHA1(75743294b8f48bffb84f062febfbc02230d49ce9) )
	ROM_LOAD16_BYTE( "5gp1mtal.2h",  0x0000001, 0x800000, CRC(1bb00c7b) SHA1(922be45d57330c31853b2dc1642c589952b09188) )
	ROM_LOAD16_BYTE( "5gp1mtbh.2m",  0x1000000, 0x800000, CRC(352360e8) SHA1(d621dfac3385059c52d215f6623901589a8658a3) )
	ROM_LOAD16_BYTE( "5gp1mtbl.2f",  0x1000001, 0x800000, CRC(66640606) SHA1(c69a0219748241c49315d7464f8156f8068e9cf5) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "5gp1cgll.4m",  0x0000000, 0x800000, CRC(0cc5bf35) SHA1(b75510a94fa6b6d2ed43566e6e84c7ae62f68194) )
	ROM_LOAD( "5gp1cglm.4k",  0x0800000, 0x800000, CRC(31557d48) SHA1(b85c3db20b101ba6bdd77487af67c3324bea29d5) )
	ROM_LOAD( "5gp1cgum.4j",  0x1000000, 0x800000, CRC(0265b701) SHA1(497a4c33311d3bb315100a78400cf2fa726f1483) )
	ROM_LOAD( "5gp1cguu.4f",  0x1800000, 0x800000, CRC(c411163b) SHA1(ae644d62357b8b806b160774043e41908fba5d05) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "5gp1ccrl.7f",  0x000000, 0x400000, CRC(e7c77e1f) SHA1(0231ddbe2afb880099dfe2657c41236c74c730bb) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "5gp1ccrh.7e",  0x000000, 0x200000, CRC(b2eba764) SHA1(5e09d1171f0afdeb9ed7337df1dbc924f23d3a0b) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "5gp1pt0h.7a",  0x0000000, 0x400000, CRC(5746a8cd) SHA1(e70fc596ab9360f474f716c73d76cb9851370c76) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt0l.7c",  0x0000002, 0x400000, CRC(a0ece0a1) SHA1(b7aab2d78e1525f865214c7de387ccd585de5d34) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt1h.5a",  0x0800000, 0x400000, CRC(b1feb5df) SHA1(45db259215511ac3e472895956f70204d4575482) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt1l.5c",  0x0800002, 0x400000, CRC(80b25ad2) SHA1(e9a03fe5bb4ce925f7218ab426ed2a1ca1a26a62) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt2h.4a",  0x1000000, 0x400000, CRC(9a693771) SHA1(c988e04cd91c3b7e75b91376fd73be4a7da543e7) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt2l.4c",  0x1000002, 0x400000, CRC(9289dbeb) SHA1(ec546ad3b1c90609591e599c760c70049ba3b581) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt3h.3a",  0x1800000, 0x400000, CRC(26eaa400) SHA1(0157b76fffe81b40eb970e84c98398807ced92c4) )
	ROM_LOAD32_WORD_SWAP( "5gp1pt3l.3c",  0x1800002, 0x400000, CRC(480b120d) SHA1(6c703550faa412095d9633cf508050614e15fbae) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "5gp1wavel.2c", 0x000000, 0x800000, CRC(aa634cc2) SHA1(e96f5c682039bc6ef22bf90e98f4da78486bd2b1) )
	ROM_LOAD( "5gp1waveh.2a", 0x800000, 0x800000, CRC(1e3523e8) SHA1(cb3d0d389fcbfb728fad29cfc36ef654d28d553a) )
ROM_END


ROM_START( raceon )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ro2vera.ic2",  0x000000, 0x200000, CRC(08b94548) SHA1(6363f1724540c2671555bc5bb11e22611614baf5) )
	ROM_LOAD16_BYTE( "ro2vera.ic1",  0x000001, 0x200000, CRC(4270884b) SHA1(82e4d4376907ee5dbabe047b9d2279f08cff5f71) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ro2vera.ic3",  0x000000, 0x080000, CRC(a763ecb7) SHA1(6b1ab63bb56342abbf7ddd7d17d413779fbafce1) )

	ROM_REGION( 0x80000, "ffb", 0 ) /* STR steering force-feedback board code */
	ROM_LOAD( "ro1_str-0a.ic16", 0x000000, 0x080000, CRC(27d39e1f) SHA1(6161cbb27c964ffab1db3b3c1f073ec514876e61) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "ro1mtah.2j",   0x000000, 0x800000, CRC(216abfb1) SHA1(8db7b17dc6441adc7a4ec8b941d5a84d73c735d6) )
	ROM_LOAD16_BYTE( "ro1mtal.2h",   0x000001, 0x800000, CRC(17646306) SHA1(8d1af777f8e884b650efee8e4c26e032e1c088b7) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ro1cgll.4m",   0x0000000, 0x800000, CRC(12c64936) SHA1(14a0d3d336f2fbe7992eedb3900748763368bc6b) )
	ROM_LOAD( "ro1cglm.4k",   0x0800000, 0x800000, CRC(7e8bb4fc) SHA1(46a7940989576239a720fde8ec4e4b623b0b6fe6) )
	ROM_LOAD( "ro1cgum.4j",   0x1000000, 0x800000, CRC(b9767735) SHA1(87fec452998a782db2cf00d369149b200a00d163) )
	ROM_LOAD( "ro1cguu.4f",   0x1800000, 0x800000, CRC(8fef8bd4) SHA1(6870590f585dc8d87ebe5181da870715c9c4fee3) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15*/
	ROM_LOAD( "ro1ccrl.7f",   0x000000, 0x400000, CRC(fe50e424) SHA1(8317c998db687e1c40398e0005a037dcded19c25) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ro1ccrh.7e",   0x000000, 0x200000, CRC(1c958de2) SHA1(4893350999d5d377e68b9577187828de7a4c77c2) )

	ROM_REGION32_LE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ro1pt0h.7a",   0x0000000, 0x400000, CRC(b052c0dc) SHA1(ed60e2df68315da02a48a701841cd861e8dd46c2) )
	ROM_LOAD32_WORD_SWAP( "ro1pt0l.7c",   0x0000002, 0x400000, CRC(4f09e946) SHA1(4c0e1edb0e807b580849510aa45313ceb700b8b9) )
	ROM_LOAD32_WORD_SWAP( "ro1pt1h.5a",   0x0800000, 0x400000, CRC(94c6be22) SHA1(27fb80975a3332fe2ec91996d2c74b2f4fb5a2de) )
	ROM_LOAD32_WORD_SWAP( "ro1pt1l.5c",   0x0800002, 0x400000, CRC(307b30f2) SHA1(24e0e5d392751e1e4126a604c7a18f90be1e6771) )
	ROM_LOAD32_WORD_SWAP( "ro1pt2h.4a",   0x1000000, 0x400000, CRC(b77886f2) SHA1(efdeb11c9783dbd7eb776c2a99204fb2e55ecb6d) )
	ROM_LOAD32_WORD_SWAP( "ro1pt2l.4c",   0x1000002, 0x400000, CRC(6eaec66b) SHA1(d96d96d3b11a5109fed07e455e2abc9a8450080c) )
	ROM_LOAD32_WORD_SWAP( "ro1pt3h.3a",   0x1800000, 0x400000, CRC(87b8f7c5) SHA1(51f996199f7fc020f12c4ebd3e47acbc04341ce3) )
	ROM_LOAD32_WORD_SWAP( "ro1pt3l.3c",   0x1800002, 0x400000, CRC(ee1a4d1d) SHA1(4fa71edd26edcb3efc56b71f603fabcafcc2d3f3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ro1wavel.2c",  0x000000, 0x800000, CRC(c6aca840) SHA1(09a021459b6326fe161ffcee36376648a5bf0e00) )
	ROM_LOAD( "ro2waveh.2a",  0x800000, 0x800000, CRC(ceecbf0d) SHA1(f0a5e57c04b661685833b209bd5e072666068391) )

	ROM_REGION( 0x800000, "spares", 0 ) /* duplicate ROMs for the second texel pipeline on the PCB, not used for emulation */
	ROM_LOAD( "ro1ccrl.7m",   0x000000, 0x400000, CRC(fe50e424) SHA1(8317c998db687e1c40398e0005a037dcded19c25) )
	ROM_LOAD( "ro1ccrh.7k",   0x000000, 0x200000, CRC(1c958de2) SHA1(4893350999d5d377e68b9577187828de7a4c77c2) )
	ROM_LOAD( "ro1cgll.5m",   0x000000, 0x800000, CRC(12c64936) SHA1(14a0d3d336f2fbe7992eedb3900748763368bc6b) )
	ROM_LOAD( "ro1cglm.5k",   0x000000, 0x800000, CRC(7e8bb4fc) SHA1(46a7940989576239a720fde8ec4e4b623b0b6fe6) )
	ROM_LOAD( "ro1cgum.5j",   0x000000, 0x800000, CRC(b9767735) SHA1(87fec452998a782db2cf00d369149b200a00d163) )
	ROM_LOAD( "ro1cguu.5f",   0x000000, 0x800000, CRC(8fef8bd4) SHA1(6870590f585dc8d87ebe5181da870715c9c4fee3) )
ROM_END


ROM_START( raceonj )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ro1verb.ic2",  0x000000, 0x200000, CRC(92952b85) SHA1(4a360479546fccd6932415fb9deae664355e4879) )
	ROM_LOAD16_BYTE( "ro1verb.ic1",  0x000001, 0x200000, CRC(92fb212c) SHA1(8d22c3fcf5e928a8fc1ea97422021e42a5eedc8b) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ro1verb.ic3",  0x000000, 0x080000, CRC(a763ecb7) SHA1(6b1ab63bb56342abbf7ddd7d17d413779fbafce1) )

	ROM_REGION( 0x80000, "ffb", 0 ) /* STR steering force-feedback board code */
	ROM_LOAD( "ro1_str-0a.ic16", 0x000000, 0x080000, CRC(27d39e1f) SHA1(6161cbb27c964ffab1db3b3c1f073ec514876e61) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "ro1mtah.2j",   0x000000, 0x800000, CRC(216abfb1) SHA1(8db7b17dc6441adc7a4ec8b941d5a84d73c735d6) )
	ROM_LOAD16_BYTE( "ro1mtal.2h",   0x000001, 0x800000, CRC(17646306) SHA1(8d1af777f8e884b650efee8e4c26e032e1c088b7) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ro1cgll.4m",   0x0000000, 0x800000, CRC(12c64936) SHA1(14a0d3d336f2fbe7992eedb3900748763368bc6b) )
	ROM_LOAD( "ro1cglm.4k",   0x0800000, 0x800000, CRC(7e8bb4fc) SHA1(46a7940989576239a720fde8ec4e4b623b0b6fe6) )
	ROM_LOAD( "ro1cgum.4j",   0x1000000, 0x800000, CRC(b9767735) SHA1(87fec452998a782db2cf00d369149b200a00d163) )
	ROM_LOAD( "ro1cguu.4f",   0x1800000, 0x800000, CRC(8fef8bd4) SHA1(6870590f585dc8d87ebe5181da870715c9c4fee3) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15*/
	ROM_LOAD( "ro1ccrl.7f",   0x000000, 0x400000, CRC(fe50e424) SHA1(8317c998db687e1c40398e0005a037dcded19c25) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ro1ccrh.7e",   0x000000, 0x200000, CRC(1c958de2) SHA1(4893350999d5d377e68b9577187828de7a4c77c2) )

	ROM_REGION32_LE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ro1pt0h.7a",   0x0000000, 0x400000, CRC(b052c0dc) SHA1(ed60e2df68315da02a48a701841cd861e8dd46c2) )
	ROM_LOAD32_WORD_SWAP( "ro1pt0l.7c",   0x0000002, 0x400000, CRC(4f09e946) SHA1(4c0e1edb0e807b580849510aa45313ceb700b8b9) )
	ROM_LOAD32_WORD_SWAP( "ro1pt1h.5a",   0x0800000, 0x400000, CRC(94c6be22) SHA1(27fb80975a3332fe2ec91996d2c74b2f4fb5a2de) )
	ROM_LOAD32_WORD_SWAP( "ro1pt1l.5c",   0x0800002, 0x400000, CRC(307b30f2) SHA1(24e0e5d392751e1e4126a604c7a18f90be1e6771) )
	ROM_LOAD32_WORD_SWAP( "ro1pt2h.4a",   0x1000000, 0x400000, CRC(b77886f2) SHA1(efdeb11c9783dbd7eb776c2a99204fb2e55ecb6d) )
	ROM_LOAD32_WORD_SWAP( "ro1pt2l.4c",   0x1000002, 0x400000, CRC(6eaec66b) SHA1(d96d96d3b11a5109fed07e455e2abc9a8450080c) )
	ROM_LOAD32_WORD_SWAP( "ro1pt3h.3a",   0x1800000, 0x400000, CRC(87b8f7c5) SHA1(51f996199f7fc020f12c4ebd3e47acbc04341ce3) )
	ROM_LOAD32_WORD_SWAP( "ro1pt3l.3c",   0x1800002, 0x400000, CRC(ee1a4d1d) SHA1(4fa71edd26edcb3efc56b71f603fabcafcc2d3f3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ro1wavel.2c",  0x000000, 0x800000, CRC(c6aca840) SHA1(09a021459b6326fe161ffcee36376648a5bf0e00) )
	ROM_LOAD( "ro1waveh.2a",  0x800000, 0x800000, CRC(24ba72e8) SHA1(c9ad8be152918758f3bdbc21c2b83fbf70d16a5d) )

	ROM_REGION( 0x800000, "spares", 0 ) /* duplicate ROMs for the second texel pipeline on the PCB, not used for emulation */
	ROM_LOAD( "ro1ccrl.7m",   0x000000, 0x400000, CRC(fe50e424) SHA1(8317c998db687e1c40398e0005a037dcded19c25) )
	ROM_LOAD( "ro1ccrh.7k",   0x000000, 0x200000, CRC(1c958de2) SHA1(4893350999d5d377e68b9577187828de7a4c77c2) )
	ROM_LOAD( "ro1cgll.5m",   0x000000, 0x800000, CRC(12c64936) SHA1(14a0d3d336f2fbe7992eedb3900748763368bc6b) )
	ROM_LOAD( "ro1cglm.5k",   0x000000, 0x800000, CRC(7e8bb4fc) SHA1(46a7940989576239a720fde8ec4e4b623b0b6fe6) )
	ROM_LOAD( "ro1cgum.5j",   0x000000, 0x800000, CRC(b9767735) SHA1(87fec452998a782db2cf00d369149b200a00d163) )
	ROM_LOAD( "ro1cguu.5f",   0x000000, 0x800000, CRC(8fef8bd4) SHA1(6870590f585dc8d87ebe5181da870715c9c4fee3) )
ROM_END


ROM_START( finfurl2 )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ffs2vera.ic2",   0x000000, 0x200000, CRC(13cbc545) SHA1(3e67a7bfbb1c1374e8e3996a0c09e4861b0dca14) )
	ROM_LOAD16_BYTE( "ffs2vera.ic1",   0x000001, 0x200000, CRC(5b04e4f2) SHA1(8099fc3deab9ed14a2484a774666fbd928330de8) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ffs1vera.ic3",  0x000000, 0x080000, CRC(9fd69bbd) SHA1(53a9bf505de70495dcccc43fdc722b3381aad97c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "ffs1mtah.2j",  0x0000000, 0x800000, CRC(f336d81d) SHA1(a9177091e1412dea1b6ea6c53530ae31361b32d0) )
	ROM_LOAD16_BYTE( "ffs1mtal.2h",  0x0000001, 0x800000, CRC(98730ad5) SHA1(9ba276ad88ec8730edbacab80cdacc34a99593e4) )
	ROM_LOAD16_BYTE( "ffs1mtbh.2m",  0x1000000, 0x800000, CRC(0f42c93b) SHA1(26b313fc5c33afb0a1ee42243486e38f052c95c2) )
	ROM_LOAD16_BYTE( "ffs1mtbl.2f",  0x1000001, 0x800000, CRC(0abc9e50) SHA1(be5e5e2b637811c59804ef9442c6da5a5a1315e2) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ffs1cgll.4m",  0x0000000, 0x800000, CRC(171bba76) SHA1(4a63a1f34de8f341a0ef9b499a21e8fec758e1cd) )
	ROM_LOAD( "ffs1cglm.4k",  0x0800000, 0x800000, CRC(48acf207) SHA1(ea902efdd94aba34dadb20762219d2d25441d199) )
	ROM_LOAD( "ffs1cgum.4j",  0x1000000, 0x800000, CRC(77447199) SHA1(1eeae30b3dd1ac467bdbbdfe4be36ca0f0816496) )
	ROM_LOAD( "ffs1cguu.4f",  0x1800000, 0x800000, CRC(52c0a19f) SHA1(e6b4b90ff88da09cb2e653e450e7ae66942a719e) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15*/
	ROM_LOAD( "ffs1ccrl.7f",  0x000000, 0x200000, CRC(ffbcfec1) SHA1(9ab25f1543da4b72784eec93985abaa2e1dafc83) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ffs1ccrh.7e",  0x000000, 0x200000, CRC(8be4aeb4) SHA1(ec344f6fba42092083e737e436451f5d7be12c15) )

	ROM_REGION32_LE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ffs1pt0h.7a",  0x0000000, 0x400000, CRC(6f9da966) SHA1(97ed7eba833d6c6065bb8cbb4673dd09abe20b21) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt0l.7c",  0x0000002, 0x400000, CRC(63d80064) SHA1(d4c08fc0baca9b1d9f1203673acb6830a2d5e4b0) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt1h.5a",  0x0800000, 0x400000, CRC(180edc67) SHA1(7c8e0266cf3481eea5416100a913eda046d30185) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt1l.5c",  0x0800002, 0x400000, CRC(21948286) SHA1(e4185eeb78195f2f691bad7c54b30a40718bf3b2) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt2h.4a",  0x1000000, 0x400000, CRC(f5664030) SHA1(0444ec9780417cff35404b961a75a49461fc1c50) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt2l.4c",  0x1000002, 0x400000, CRC(8cf96442) SHA1(1ccbe5cf700a63adc06e8c6afe4527d27942207d) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt3h.3a",  0x1800000, 0x400000, CRC(81c675ee) SHA1(4417862f481ace6152762634a3a012161829501e) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt3l.3c",  0x1800002, 0x400000, CRC(a3541604) SHA1(dffec3756051773bd1110d98463eb3282834b00c) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ffs1wavel.2c", 0x000000, 0x800000, CRC(67ba16cf) SHA1(00b38617c2185b9a3bf279962ad0c21a7287256f) )
	ROM_LOAD( "ffs1waveh.2a", 0x800000, 0x800000, CRC(178e8bd3) SHA1(8ab1a97003914f70b09e96c5924f3a839fe634c7) )
ROM_END


ROM_START( finfurl2j )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "ffs1vera.ic2", 0x000000, 0x200000, CRC(0215125d) SHA1(a99f601441c152b0b00f4811e5752c71897b1ed4) )
	ROM_LOAD16_BYTE( "ffs1vera.ic1", 0x000001, 0x200000, CRC(38c9ae96) SHA1(b50afc7276662267ff6460f82d0e5e8b00b341ea) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "ffs1vera.ic3",  0x000000, 0x080000, CRC(9fd69bbd) SHA1(53a9bf505de70495dcccc43fdc722b3381aad97c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "ffs1mtah.2j",  0x0000000, 0x800000, CRC(f336d81d) SHA1(a9177091e1412dea1b6ea6c53530ae31361b32d0) )
	ROM_LOAD16_BYTE( "ffs1mtal.2h",  0x0000001, 0x800000, CRC(98730ad5) SHA1(9ba276ad88ec8730edbacab80cdacc34a99593e4) )
	ROM_LOAD16_BYTE( "ffs1mtbh.2m",  0x1000000, 0x800000, CRC(0f42c93b) SHA1(26b313fc5c33afb0a1ee42243486e38f052c95c2) )
	ROM_LOAD16_BYTE( "ffs1mtbl.2f",  0x1000001, 0x800000, CRC(0abc9e50) SHA1(be5e5e2b637811c59804ef9442c6da5a5a1315e2) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "ffs1cgll.4m",  0x0000000, 0x800000, CRC(171bba76) SHA1(4a63a1f34de8f341a0ef9b499a21e8fec758e1cd) )
	ROM_LOAD( "ffs1cglm.4k",  0x0800000, 0x800000, CRC(48acf207) SHA1(ea902efdd94aba34dadb20762219d2d25441d199) )
	ROM_LOAD( "ffs1cgum.4j",  0x1000000, 0x800000, CRC(77447199) SHA1(1eeae30b3dd1ac467bdbbdfe4be36ca0f0816496) )
	ROM_LOAD( "ffs1cguu.4f",  0x1800000, 0x800000, CRC(52c0a19f) SHA1(e6b4b90ff88da09cb2e653e450e7ae66942a719e) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15*/
	ROM_LOAD( "ffs1ccrl.7f",  0x000000, 0x200000, CRC(ffbcfec1) SHA1(9ab25f1543da4b72784eec93985abaa2e1dafc83) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "ffs1ccrh.7e",  0x000000, 0x200000, CRC(8be4aeb4) SHA1(ec344f6fba42092083e737e436451f5d7be12c15) )

	ROM_REGION32_LE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "ffs1pt0h.7a",  0x0000000, 0x400000, CRC(6f9da966) SHA1(97ed7eba833d6c6065bb8cbb4673dd09abe20b21) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt0l.7c",  0x0000002, 0x400000, CRC(63d80064) SHA1(d4c08fc0baca9b1d9f1203673acb6830a2d5e4b0) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt1h.5a",  0x0800000, 0x400000, CRC(180edc67) SHA1(7c8e0266cf3481eea5416100a913eda046d30185) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt1l.5c",  0x0800002, 0x400000, CRC(21948286) SHA1(e4185eeb78195f2f691bad7c54b30a40718bf3b2) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt2h.4a",  0x1000000, 0x400000, CRC(f5664030) SHA1(0444ec9780417cff35404b961a75a49461fc1c50) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt2l.4c",  0x1000002, 0x400000, CRC(8cf96442) SHA1(1ccbe5cf700a63adc06e8c6afe4527d27942207d) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt3h.3a",  0x1800000, 0x400000, CRC(81c675ee) SHA1(4417862f481ace6152762634a3a012161829501e) )
	ROM_LOAD32_WORD_SWAP( "ffs1pt3l.3c",  0x1800002, 0x400000, CRC(a3541604) SHA1(dffec3756051773bd1110d98463eb3282834b00c) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "ffs1wavel.2c", 0x000000, 0x800000, CRC(67ba16cf) SHA1(00b38617c2185b9a3bf279962ad0c21a7287256f) )
	ROM_LOAD( "ffs1waveh.2a", 0x800000, 0x800000, CRC(178e8bd3) SHA1(8ab1a97003914f70b09e96c5924f3a839fe634c7) )
ROM_END


ROM_START( panicprk )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "pnp2vera.ic2", 0x000000, 0x200000, CRC(cd528597) SHA1(cf390e78228eb10d5f50ff7e7e37063a2d87f469) )
	ROM_LOAD16_BYTE( "pnp2vera.ic1", 0x000001, 0x200000, CRC(80fea853) SHA1(b18003bde060ebb3c892a6d7fa4abf868cadc777) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "pnp1vera.ic3", 0x000000, 0x080000, CRC(fe4bc6f4) SHA1(2114dc4bc63d589e6c3b26a73dbc60924f3b1765) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "pnp1mtah.2j",  0x000000, 0x800000, CRC(37addddd) SHA1(3032989653304417df80606bc3fde6e9425d8cbb) )
	ROM_LOAD16_BYTE( "pnp1mtal.2h",  0x000001, 0x800000, CRC(6490faaa) SHA1(03443746009b434e5d4074ea6314910418907360) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "pnp1cgll.4m",  0x0000000, 0x800000, CRC(d03932cf) SHA1(49240e44923cc6e815e9457b6290fd18466658af) )
	ROM_LOAD( "pnp1cglm.5k",  0x0800000, 0x800000, CRC(abf4ccf2) SHA1(3848e26d0ba6c872bbc6d5e0eb23a9d4b34152d5) )
	ROM_LOAD( "pnp1cgum.4j",  0x1000000, 0x800000, CRC(206217ca) SHA1(9c095bba7764f3405c3fab10513b9b78981ec44d) )
	ROM_LOAD( "pnp1cguu.5f",  0x1800000, 0x800000, CRC(cd64f57f) SHA1(8780270298e0823db1acbbf79396788df0c3c19c) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "pnp1ccrl.7f",  0x000000, 0x200000, CRC(b7bc43c2) SHA1(f4b470540194486ca6822f438fc1d4700cfb2ab1) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "pnp1ccrh.7e",  0x000000, 0x200000, CRC(caaf1b73) SHA1(b436992817ab4e4dad05e7429eb102d4fb57fa6a) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "pnp1pt0h.7a",  0x000000, 0x400000, CRC(43fc2246) SHA1(301d321cd4a01ebd7ccfa6f295d6c3daf0a19efe) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt0l.7c",  0x000002, 0x400000, CRC(26af5fa1) SHA1(12fcf98c2a59643e0fdfdd7186f9f16baf54a9cf) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt1h.5a",  0x800000, 0x400000, CRC(1ff470c0) SHA1(ca8fad90743589744939d681b0ce94f368337b3f) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt1l.5c",  0x800002, 0x400000, CRC(15c6f236) SHA1(e8c393359a91cdce6e9110a48c0a80708f8fc132) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "pnp1wavel.2c", 0x000000, 0x800000, CRC(35c6a9bd) SHA1(4b56fdc37525c15e57d93091e6609d6a6905fc5c) )
	ROM_LOAD( "pnp1waveh.2a", 0x800000, 0x800000, CRC(6fa1826a) SHA1(20a5af49e65ae2bc57c016b5cd9bafa5a5220d35) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "pnp1cguu.4f",  0x000000, 0x800000, CRC(cd64f57f) SHA1(8780270298e0823db1acbbf79396788df0c3c19c) )
	ROM_LOAD( "pnp1cgum.5j",  0x000000, 0x800000, CRC(206217ca) SHA1(9c095bba7764f3405c3fab10513b9b78981ec44d) )
	ROM_LOAD( "pnp1cgll.5m",  0x000000, 0x800000, CRC(d03932cf) SHA1(49240e44923cc6e815e9457b6290fd18466658af) )
	ROM_LOAD( "pnp1cglm.4k",  0x000000, 0x800000, CRC(abf4ccf2) SHA1(3848e26d0ba6c872bbc6d5e0eb23a9d4b34152d5) )
	ROM_LOAD( "pnp1ccrl.7m",  0x000000, 0x200000, CRC(b7bc43c2) SHA1(f4b470540194486ca6822f438fc1d4700cfb2ab1) )
	ROM_LOAD( "pnp1ccrh.7k",  0x000000, 0x200000, CRC(caaf1b73) SHA1(b436992817ab4e4dad05e7429eb102d4fb57fa6a) )

	ROM_REGION( 0x010000, "nvram", 0 )
	ROM_LOAD( "nvram",        0x000000, 0x010000, CRC(10fb7b12) SHA1(32b12dfaa52a1c4b2df2002489b7b4040f361a6c) )
ROM_END


ROM_START( panicprkj )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "pnp1verb.ic2", 0x000000, 0x200000, CRC(a46e34f8) SHA1(c84eb701a1e01e706dea515acaaf6d98ad53f453) )
	ROM_LOAD16_BYTE( "pnp1verb.ic1", 0x000001, 0x200000, CRC(4de52a64) SHA1(7edd974d52e17bcbdb029f718c06d603ed558d90) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "pnp1vera.ic3", 0x000000, 0x080000, CRC(fe4bc6f4) SHA1(2114dc4bc63d589e6c3b26a73dbc60924f3b1765) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "pnp1mtah.2j",  0x000000, 0x800000, CRC(37addddd) SHA1(3032989653304417df80606bc3fde6e9425d8cbb) )
	ROM_LOAD16_BYTE( "pnp1mtal.2h",  0x000001, 0x800000, CRC(6490faaa) SHA1(03443746009b434e5d4074ea6314910418907360) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "pnp1cgll.4m",  0x0000000, 0x800000, CRC(d03932cf) SHA1(49240e44923cc6e815e9457b6290fd18466658af) )
	ROM_LOAD( "pnp1cglm.5k",  0x0800000, 0x800000, CRC(abf4ccf2) SHA1(3848e26d0ba6c872bbc6d5e0eb23a9d4b34152d5) )
	ROM_LOAD( "pnp1cgum.4j",  0x1000000, 0x800000, CRC(206217ca) SHA1(9c095bba7764f3405c3fab10513b9b78981ec44d) )
	ROM_LOAD( "pnp1cguu.5f",  0x1800000, 0x800000, CRC(cd64f57f) SHA1(8780270298e0823db1acbbf79396788df0c3c19c) )

	ROM_REGION16_LE( 0x200000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "pnp1ccrl.7f",  0x000000, 0x200000, CRC(b7bc43c2) SHA1(f4b470540194486ca6822f438fc1d4700cfb2ab1) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "pnp1ccrh.7e",  0x000000, 0x200000, CRC(caaf1b73) SHA1(b436992817ab4e4dad05e7429eb102d4fb57fa6a) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "pnp1pt0h.7a",  0x000000, 0x400000, CRC(43fc2246) SHA1(301d321cd4a01ebd7ccfa6f295d6c3daf0a19efe) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt0l.7c",  0x000002, 0x400000, CRC(26af5fa1) SHA1(12fcf98c2a59643e0fdfdd7186f9f16baf54a9cf) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt1h.5a",  0x800000, 0x400000, CRC(1ff470c0) SHA1(ca8fad90743589744939d681b0ce94f368337b3f) )
	ROM_LOAD32_WORD_SWAP( "pnp1pt1l.5c",  0x800002, 0x400000, CRC(15c6f236) SHA1(e8c393359a91cdce6e9110a48c0a80708f8fc132) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "pnp1wavel.2c", 0x000000, 0x800000, CRC(35c6a9bd) SHA1(4b56fdc37525c15e57d93091e6609d6a6905fc5c) )
	ROM_LOAD( "pnp1waveh.2a", 0x800000, 0x800000, CRC(6fa1826a) SHA1(20a5af49e65ae2bc57c016b5cd9bafa5a5220d35) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "pnp1cguu.4f",  0x000000, 0x800000, CRC(cd64f57f) SHA1(8780270298e0823db1acbbf79396788df0c3c19c) )
	ROM_LOAD( "pnp1cgum.5j",  0x000000, 0x800000, CRC(206217ca) SHA1(9c095bba7764f3405c3fab10513b9b78981ec44d) )
	ROM_LOAD( "pnp1cgll.5m",  0x000000, 0x800000, CRC(d03932cf) SHA1(49240e44923cc6e815e9457b6290fd18466658af) )
	ROM_LOAD( "pnp1cglm.4k",  0x000000, 0x800000, CRC(abf4ccf2) SHA1(3848e26d0ba6c872bbc6d5e0eb23a9d4b34152d5) )
	ROM_LOAD( "pnp1ccrl.7m",  0x000000, 0x200000, CRC(b7bc43c2) SHA1(f4b470540194486ca6822f438fc1d4700cfb2ab1) )
	ROM_LOAD( "pnp1ccrh.7k",  0x000000, 0x200000, CRC(caaf1b73) SHA1(b436992817ab4e4dad05e7429eb102d4fb57fa6a) )

	ROM_REGION( 0x010000, "nvram", 0 )
	ROM_LOAD( "nvram",        0x000000, 0x010000, CRC(22362b2a) SHA1(144956188e3cba78015e78cdcb52142b72a5aa1e) )
ROM_END


ROM_START( gunwars )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "gm1verb.ic2",  0x000000, 0x200000, CRC(401f8264) SHA1(281f245ae0fbc2b82248c7aacaa5dfcdb114e2ee) )
	ROM_LOAD16_BYTE( "gm1verb.ic1",  0x000001, 0x200000, CRC(f9fd0f2b) SHA1(53dadd49d0d43f0693c84853ba3de1b5faa9e1d8) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "gm1vera.ic3",  0x000000, 0x080000, CRC(5582fdd4) SHA1(8aae8bc6688d531888f2de509c07502ee355b3ab) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "gm1mtah.2j",   0x000000, 0x800000, CRC(3cea9094) SHA1(497395425e409de47e1114de9aeeaf05e4f6a9a1) )
	ROM_LOAD16_BYTE( "gm1mtal.2h",   0x000001, 0x800000, CRC(d531dfcd) SHA1(9f7cbe9a03c1f7649bf05a7a30d47511573b50ba) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "gm1cgll.4m",   0x0000000, 0x800000, CRC(936c0079) SHA1(3aec8caada35b7ed790bb3a8bcf6e01cad068fcd) )
	ROM_LOAD( "gm1cglm.4k",   0x0800000, 0x800000, CRC(e2ee5493) SHA1(1ffd74646796ad554d7967ba9fc18deab4fedadf) )
	ROM_LOAD( "gm1cgum.4j",   0x1000000, 0x800000, CRC(a7728944) SHA1(c187c6d66128554fcecc96e81d4f5396197e8280) )
	ROM_LOAD( "gm1cguu.5f",   0x1800000, 0x800000, CRC(26a74698) SHA1(3f07d273abb3f2552dc6a29300f5dc2f2744c852) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "gm1ccrl.7f",   0x000000, 0x400000, CRC(2c54c182) SHA1(538dfb04653f8d86f976e702456bf4da97e3fda9) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "gm1ccrh.7e",   0x000000, 0x200000, CRC(8563ef01) SHA1(59f09a08008a71a4bb12bd43a1b5dbe633d3061d) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "gm1pt0h.7a",   0x000000, 0x400000, CRC(5ebd658c) SHA1(9e7b89a726b11b6da3327d72ec6adcc30fbb384d) )
	ROM_LOAD32_WORD_SWAP( "gm1pt0l.7c",   0x000002, 0x400000, CRC(62e9bedb) SHA1(7043c5e6f26139c9e6e18d4f35fac6a16d4dabd1) )
	ROM_LOAD32_WORD_SWAP( "gm1pt1h.5a",   0x800000, 0x400000, CRC(5f6cebab) SHA1(95bd30d30ea25509b66a107fb255d0af1e6a357e) )
	ROM_LOAD32_WORD_SWAP( "gm1pt1l.5c",   0x800002, 0x400000, CRC(f44c149f) SHA1(9f995de02ea6ac35ccbabbba5bb473a10e1ec667) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "gm1wave.2c",   0x000000, 0x800000, CRC(7d5c79a4) SHA1(b800a46bcca10cb0d0d9e0acfa68af63ae64dcaf) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "gm1cguu.4f",   0x000000, 0x800000, CRC(26a74698) SHA1(3f07d273abb3f2552dc6a29300f5dc2f2744c852) )
	ROM_LOAD( "gm1cgum.5j",   0x000000, 0x800000, CRC(a7728944) SHA1(c187c6d66128554fcecc96e81d4f5396197e8280) )
	ROM_LOAD( "gm1cgll.5m",   0x000000, 0x800000, CRC(936c0079) SHA1(3aec8caada35b7ed790bb3a8bcf6e01cad068fcd) )
	ROM_LOAD( "gm1cglm.5k",   0x000000, 0x800000, CRC(e2ee5493) SHA1(1ffd74646796ad554d7967ba9fc18deab4fedadf) )
	ROM_LOAD( "gm1ccrl.7m",   0x000000, 0x400000, CRC(2c54c182) SHA1(538dfb04653f8d86f976e702456bf4da97e3fda9) )
	ROM_LOAD( "gm1ccrh.7k",   0x000000, 0x200000, CRC(8563ef01) SHA1(59f09a08008a71a4bb12bd43a1b5dbe633d3061d) )
ROM_END


ROM_START( gunwarsa )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "gm1vera.ic2",  0x000000, 0x200000, CRC(cf61467f) SHA1(eae79e4e540340cba7d576a36085f802b8032f4f) )
	ROM_LOAD16_BYTE( "gm1vera.ic1",  0x000001, 0x200000, CRC(abc9ffe6) SHA1(d833b9b9d8bb0cc4b53f30507c9603df9e63fa2f) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "gm1vera.ic3",  0x000000, 0x080000, CRC(5582fdd4) SHA1(8aae8bc6688d531888f2de509c07502ee355b3ab) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "gm1mtah.2j",   0x000000, 0x800000, CRC(3cea9094) SHA1(497395425e409de47e1114de9aeeaf05e4f6a9a1) )
	ROM_LOAD16_BYTE( "gm1mtal.2h",   0x000001, 0x800000, CRC(d531dfcd) SHA1(9f7cbe9a03c1f7649bf05a7a30d47511573b50ba) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "gm1cgll.4m",   0x0000000, 0x800000, CRC(936c0079) SHA1(3aec8caada35b7ed790bb3a8bcf6e01cad068fcd) )
	ROM_LOAD( "gm1cglm.4k",   0x0800000, 0x800000, CRC(e2ee5493) SHA1(1ffd74646796ad554d7967ba9fc18deab4fedadf) )
	ROM_LOAD( "gm1cgum.4j",   0x1000000, 0x800000, CRC(a7728944) SHA1(c187c6d66128554fcecc96e81d4f5396197e8280) )
	ROM_LOAD( "gm1cguu.5f",   0x1800000, 0x800000, CRC(26a74698) SHA1(3f07d273abb3f2552dc6a29300f5dc2f2744c852) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "gm1ccrl.7f",   0x000000, 0x400000, CRC(2c54c182) SHA1(538dfb04653f8d86f976e702456bf4da97e3fda9) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "gm1ccrh.7e",   0x000000, 0x200000, CRC(8563ef01) SHA1(59f09a08008a71a4bb12bd43a1b5dbe633d3061d) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "gm1pt0h.7a",   0x000000, 0x400000, CRC(5ebd658c) SHA1(9e7b89a726b11b6da3327d72ec6adcc30fbb384d) )
	ROM_LOAD32_WORD_SWAP( "gm1pt0l.7c",   0x000002, 0x400000, CRC(62e9bedb) SHA1(7043c5e6f26139c9e6e18d4f35fac6a16d4dabd1) )
	ROM_LOAD32_WORD_SWAP( "gm1pt1h.5a",   0x800000, 0x400000, CRC(5f6cebab) SHA1(95bd30d30ea25509b66a107fb255d0af1e6a357e) )
	ROM_LOAD32_WORD_SWAP( "gm1pt1l.5c",   0x800002, 0x400000, CRC(f44c149f) SHA1(9f995de02ea6ac35ccbabbba5bb473a10e1ec667) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "gm1wave.2c",   0x000000, 0x800000, CRC(7d5c79a4) SHA1(b800a46bcca10cb0d0d9e0acfa68af63ae64dcaf) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "gm1cguu.4f",   0x000000, 0x800000, CRC(26a74698) SHA1(3f07d273abb3f2552dc6a29300f5dc2f2744c852) )
	ROM_LOAD( "gm1cgum.5j",   0x000000, 0x800000, CRC(a7728944) SHA1(c187c6d66128554fcecc96e81d4f5396197e8280) )
	ROM_LOAD( "gm1cgll.5m",   0x000000, 0x800000, CRC(936c0079) SHA1(3aec8caada35b7ed790bb3a8bcf6e01cad068fcd) )
	ROM_LOAD( "gm1cglm.5k",   0x000000, 0x800000, CRC(e2ee5493) SHA1(1ffd74646796ad554d7967ba9fc18deab4fedadf) )
	ROM_LOAD( "gm1ccrl.7m",   0x000000, 0x400000, CRC(2c54c182) SHA1(538dfb04653f8d86f976e702456bf4da97e3fda9) )
	ROM_LOAD( "gm1ccrh.7k",   0x000000, 0x200000, CRC(8563ef01) SHA1(59f09a08008a71a4bb12bd43a1b5dbe633d3061d) )
ROM_END


ROM_START( downhill ) // Dump has been reprogrammed on blank flash ROMs and tested working on real PCB
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "dh2vera.ic2",  0x000000, 0x200000, CRC(81bca744) SHA1(0335960126e41f02442828213990f0d30af86696) )
	ROM_LOAD16_BYTE( "dh2vera.ic1",  0x000001, 0x200000, CRC(ea7dcf68) SHA1(fd5110e3dab04f8c8503fd6fe9edbfc7c5a22aaf) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "dh3vera.ic3",  0x000000, 0x080000, CRC(98f9fc8b) SHA1(5152b9e11773033a26da11d1f3774a261e61a2c5) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "dh1mtah.2j",   0x000000, 0x800000, CRC(3b56faa7) SHA1(861db7f549bedbb2b837516fcc966ad5890007ce) )
	ROM_LOAD16_BYTE( "dh1mtal.2h",   0x000001, 0x800000, CRC(9fa07bfe) SHA1(a6b847ff7d5eadbf60b434a0d905051ea4227113) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "dh1cgll.4m",   0x0000000, 0x800000, CRC(c0d5ad87) SHA1(bc1992516c63aebdae0322def77f082d799a327a) )
	ROM_LOAD( "dh1cglm.4k",   0x0800000, 0x800000, CRC(5d9a5e35) SHA1(d746abb45f04aa4eb9d43d9c79051e71bf024e38) )
	ROM_LOAD( "dh1cgum.4j",   0x1000000, 0x800000, CRC(1044d0a0) SHA1(e0bf843616e166495fcdc76f076eb53a28287d30) )
	ROM_LOAD( "dh1cguu.5f",   0x1800000, 0x800000, CRC(66cb0dd7) SHA1(1f67320f150f1b55c97eae4b9fe4890fabc8dc7e) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "dh1ccrl.7f",   0x000000, 0x400000, CRC(65c857df) SHA1(5d67b17cf272f042b4264d9871d6e4088c20b788) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "dh1ccrh.7e",   0x000000, 0x200000, CRC(f21c482d) SHA1(bfcead2ff3d10f996ac0bf81470d050bd6374156) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "dh1pt0h.7a", 0x0000000, 0x400000, CRC(0e84a5d8) SHA1(28559f978b86d88bb18c3e58e28a97ecfb5f7fa9) )
	ROM_LOAD32_WORD_SWAP( "dh1pt0l.7c", 0x0000002, 0x400000, CRC(d120eee5) SHA1(fa1269d891f4e0510491aa70c4abd5f36852e691) )
	ROM_LOAD32_WORD_SWAP( "dh1pt1h.5a", 0x0800000, 0x400000, CRC(88cd4c90) SHA1(94016c72a9da983e55c74cbdd3691b596ea50c31) )
	ROM_LOAD32_WORD_SWAP( "dh1pt1l.5c", 0x0800002, 0x400000, CRC(dee2f2bf) SHA1(258f9a6e324502550d27b8feaf36244766fa19da) )
	ROM_LOAD32_WORD_SWAP( "dh1pt2h.4a", 0x1000000, 0x400000, CRC(7e167c65) SHA1(018bf6aea4c1640ef728cf7b8e491f11742ede0d) )
	ROM_LOAD32_WORD_SWAP( "dh1pt2l.4c", 0x1000002, 0x400000, CRC(714e3090) SHA1(39827f645dacbb57c7c40193f3f58e879899a4f3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "dh1wavel.2c",  0x000000, 0x800000, CRC(10954726) SHA1(50ee0346c46194dada7b5c0d8b1efe9a7f211b90) )
	ROM_LOAD( "dh1waveh.2a",  0x800000, 0x800000, CRC(2adfa312) SHA1(d01a46af2c95d1ea64e9778979ae147298d921e3) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "dh1cguu.4f",   0x000000, 0x800000, CRC(66cb0dd7) SHA1(1f67320f150f1b55c97eae4b9fe4890fabc8dc7e) )
	ROM_LOAD( "dh1cgum.5j",   0x000000, 0x800000, CRC(1044d0a0) SHA1(e0bf843616e166495fcdc76f076eb53a28287d30) )
	ROM_LOAD( "dh1cgll.5m",   0x000000, 0x800000, CRC(c0d5ad87) SHA1(bc1992516c63aebdae0322def77f082d799a327a) )
	ROM_LOAD( "dh1cglm.5k",   0x000000, 0x800000, CRC(5d9a5e35) SHA1(d746abb45f04aa4eb9d43d9c79051e71bf024e38) )
	ROM_LOAD( "dh1ccrl.7m",   0x000000, 0x400000, CRC(65c857df) SHA1(5d67b17cf272f042b4264d9871d6e4088c20b788) )
	ROM_LOAD( "dh1ccrh.7k",   0x000000, 0x200000, CRC(f21c482d) SHA1(bfcead2ff3d10f996ac0bf81470d050bd6374156) )

	ROM_REGION( 0x010000, "nvram", 0 )
	ROM_LOAD( "nvram",        0x000000, 0x010000, CRC(1195e532) SHA1(1c88b2d83c290f79e9505dda5beb4ae3a85d5d30) )
ROM_END


ROM_START( downhillu )
	ROM_REGION32_BE( 0x400000, "user1", 0 ) /* 4 megs for main R4650 code */
	ROM_LOAD16_BYTE( "dh3vera.ic2",  0x000000, 0x200000, CRC(5d9952e9) SHA1(d38422330bd708c247b9968429fbff36fe706598) )
	ROM_LOAD16_BYTE( "dh3vera.ic1",  0x000001, 0x200000, CRC(64a236f3) SHA1(aac59e0db5cfefc4b442e6c3a5189a8418742201) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "dh3vera.ic3",  0x000000, 0x080000, CRC(98f9fc8b) SHA1(5152b9e11773033a26da11d1f3774a261e61a2c5) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "dh1mtah.2j",   0x000000, 0x800000, CRC(3b56faa7) SHA1(861db7f549bedbb2b837516fcc966ad5890007ce) )
	ROM_LOAD16_BYTE( "dh1mtal.2h",   0x000001, 0x800000, CRC(9fa07bfe) SHA1(a6b847ff7d5eadbf60b434a0d905051ea4227113) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "dh1cgll.4m",   0x0000000, 0x800000, CRC(c0d5ad87) SHA1(bc1992516c63aebdae0322def77f082d799a327a) )
	ROM_LOAD( "dh1cglm.4k",   0x0800000, 0x800000, CRC(5d9a5e35) SHA1(d746abb45f04aa4eb9d43d9c79051e71bf024e38) )
	ROM_LOAD( "dh1cgum.4j",   0x1000000, 0x800000, CRC(1044d0a0) SHA1(e0bf843616e166495fcdc76f076eb53a28287d30) )
	ROM_LOAD( "dh1cguu.5f",   0x1800000, 0x800000, CRC(66cb0dd7) SHA1(1f67320f150f1b55c97eae4b9fe4890fabc8dc7e) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "dh1ccrl.7f",   0x000000, 0x400000, CRC(65c857df) SHA1(5d67b17cf272f042b4264d9871d6e4088c20b788) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "dh1ccrh.7e",   0x000000, 0x200000, CRC(f21c482d) SHA1(bfcead2ff3d10f996ac0bf81470d050bd6374156) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "dh1pt0h.7a", 0x0000000, 0x400000, CRC(0e84a5d8) SHA1(28559f978b86d88bb18c3e58e28a97ecfb5f7fa9) )
	ROM_LOAD32_WORD_SWAP( "dh1pt0l.7c", 0x0000002, 0x400000, CRC(d120eee5) SHA1(fa1269d891f4e0510491aa70c4abd5f36852e691) )
	ROM_LOAD32_WORD_SWAP( "dh1pt1h.5a", 0x0800000, 0x400000, CRC(88cd4c90) SHA1(94016c72a9da983e55c74cbdd3691b596ea50c31) )
	ROM_LOAD32_WORD_SWAP( "dh1pt1l.5c", 0x0800002, 0x400000, CRC(dee2f2bf) SHA1(258f9a6e324502550d27b8feaf36244766fa19da) )
	ROM_LOAD32_WORD_SWAP( "dh1pt2h.4a", 0x1000000, 0x400000, CRC(7e167c65) SHA1(018bf6aea4c1640ef728cf7b8e491f11742ede0d) )
	ROM_LOAD32_WORD_SWAP( "dh1pt2l.4c", 0x1000002, 0x400000, CRC(714e3090) SHA1(39827f645dacbb57c7c40193f3f58e879899a4f3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "dh1wavel.2c",  0x000000, 0x800000, CRC(10954726) SHA1(50ee0346c46194dada7b5c0d8b1efe9a7f211b90) )
	ROM_LOAD( "dh1waveh.2a",  0x800000, 0x800000, CRC(2adfa312) SHA1(d01a46af2c95d1ea64e9778979ae147298d921e3) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "dh1cguu.4f",   0x000000, 0x800000, CRC(66cb0dd7) SHA1(1f67320f150f1b55c97eae4b9fe4890fabc8dc7e) )
	ROM_LOAD( "dh1cgum.5j",   0x000000, 0x800000, CRC(1044d0a0) SHA1(e0bf843616e166495fcdc76f076eb53a28287d30) )
	ROM_LOAD( "dh1cgll.5m",   0x000000, 0x800000, CRC(c0d5ad87) SHA1(bc1992516c63aebdae0322def77f082d799a327a) )
	ROM_LOAD( "dh1cglm.5k",   0x000000, 0x800000, CRC(5d9a5e35) SHA1(d746abb45f04aa4eb9d43d9c79051e71bf024e38) )
	ROM_LOAD( "dh1ccrl.7m",   0x000000, 0x400000, CRC(65c857df) SHA1(5d67b17cf272f042b4264d9871d6e4088c20b788) )
	ROM_LOAD( "dh1ccrh.7k",   0x000000, 0x200000, CRC(f21c482d) SHA1(bfcead2ff3d10f996ac0bf81470d050bd6374156) )

	ROM_REGION( 0x010000, "nvram", 0 )
	ROM_LOAD( "nvram",        0x000000, 0x010000, CRC(1195e532) SHA1(1c88b2d83c290f79e9505dda5beb4ae3a85d5d30) )
ROM_END


ROM_START( crszone )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo4verb.ic4", 0x000000, 0x800000, CRC(6192533d) SHA1(d102b91fe193bf255ea4e57a2bd964aa1cdfd21d) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END


ROM_START( crszonev4a )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo4vera.ic4", 0x000000, 0x800000, CRC(3755b402) SHA1(e169fded9d136af7ce6997868629eed5196b8cdd) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END


ROM_START( crszonev3b )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic4", 0x000000, 0x800000, CRC(4cb26465) SHA1(078dfd0d8c920707df14e9a26658fa63421fcb0b) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END


ROM_START( crszonev3b2 )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic4", 0x000000, 0x800000, CRC(cabee8c3) SHA1(4887b8550038c072f988c5999d57ec40e82e4072) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END


ROM_START( crszonev3a )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo3vera.ic4", 0x000000, 0x800000, CRC(09b0c91e) SHA1(226c3788d6a50272e2544d04d9ca20df81014fb6) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END


ROM_START( crszonev2a )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo2vera.ic4", 0x000000, 0x800000, CRC(1426d8d0) SHA1(e8049df1b2db1180f9edf6e5fa9fe8692ae81086) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END

ROM_START( crszonev2b )
	ROM_REGION32_BE( 0x800000, "user1", 0 ) /* 8 megs for main R4650 code */
	ROM_LOAD16_WORD_SWAP( "cszo2verb.ic4", 0x000000, 0x800000, CRC(9a1a456d) SHA1(7f3a10bb6cc1de0613c9bec292eda54c8e95ff8a) )

	ROM_REGION( 0x80000, "subcpu", 0 )  /* Hitachi H8/3002 MCU code */
	ROM_LOAD16_WORD_SWAP( "cszo3verb.ic1", 0x000000, 0x080000, CRC(c790743b) SHA1(5fa7b83a7a1b1105a3aa0870b782cf2741b7d11c) )

	ROM_REGION32_BE( 0x2000000, "data", 0 ) /* data ROMs */
	ROM_LOAD16_BYTE( "csz1mtah.2j",  0x0000000, 0x800000, CRC(66b076ad) SHA1(edd32e0b380f01a9626d32f5eec860f841c8be8a) )
	ROM_LOAD16_BYTE( "csz1mtal.2h",  0x0000001, 0x800000, CRC(38dc639a) SHA1(aa9b5b35174c1b007a57a4bd7a53bc3f479b5b71) )
	ROM_LOAD16_BYTE( "csz1mtbh.2m",  0x1000000, 0x800000, CRC(bdec4188) SHA1(a098651fbd8a69a0afc17f4b6c93350926cacd6b) )
	ROM_LOAD16_BYTE( "csz1mtbl.2f",  0x1000001, 0x800000, CRC(9c8f8d7a) SHA1(f61bcc9763df15428c82931a605ee40334d5ad98) )

	ROM_REGION( 0x2000000, "textile", 0 )   /* texture tiles */
	ROM_LOAD( "csz1cgll.4m",  0x0000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.4k",  0x0800000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1cgum.4j",  0x1000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cguu.5f",  0x1800000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )

	ROM_REGION16_LE( 0x400000, "textilemapl", 0 )   /* texture tilemap 0-15 */
	ROM_LOAD( "csz1ccrl.7f",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )

	ROM_REGION( 0x200000, "textilemaph", 0 )        /* texture tilemap 16-17 + attr */
	ROM_LOAD( "csz1ccrh.7e",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )

	ROM_REGION32_BE( 0x2000000, "pointrom", 0 ) /* 3D model data */
	ROM_LOAD32_WORD_SWAP( "csz1pt0h.7a",  0x0000000, 0x400000, CRC(e82f1abb) SHA1(b1c57152cc27835e06e429fd1659fe0973638142) )
	ROM_LOAD32_WORD_SWAP( "csz1pt0l.7c",  0x0000002, 0x400000, CRC(b0d66afe) SHA1(7cda4eebf1bb1191d17e4b5e616be2fbe4ae9328) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1h.5a",  0x0800000, 0x400000, CRC(e54f80ad) SHA1(3b3fbb3001e630d800b02ec8e653d74878ac5116) )
	ROM_LOAD32_WORD_SWAP( "csz1pt1l.5c",  0x0800002, 0x400000, CRC(527171c8) SHA1(0b2ce3858f40bdedf1543309a6bc28d780415250) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2h.4a",  0x1000000, 0x400000, CRC(e295137a) SHA1(37b18af1b3d9f0e69b45135f89b49a1ceec79127) )
	ROM_LOAD32_WORD_SWAP( "csz1pt2l.4c",  0x1000002, 0x400000, CRC(c87d6dbd) SHA1(686f39073c521d6b21ef8bc1161b41b680697c63) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3h.3a",  0x1800000, 0x400000, CRC(05f65bdf) SHA1(0c349fe5381fe7aeb81f9365a2b44a212f6bd33e) )
	ROM_LOAD32_WORD_SWAP( "csz1pt3l.3c",  0x1800002, 0x400000, CRC(5d077c0f) SHA1(a4fd0167d89bf9417766405726e0334e7c7eaec3) )

	ROM_REGION( 0x1000000, "c352", 0 ) /* C352 PCM samples */
	ROM_LOAD( "csz1wavel.2c", 0x000000, 0x800000, CRC(d0d74132) SHA1(a293d93bca8e12e388a088a592cfa7bcb9a976f7) )
	ROM_LOAD( "csz1waveh.2a", 0x800000, 0x800000, CRC(de9d14a8) SHA1(e5006861928bb1d29bf80c7304f1a6d044b094fd) )

	ROM_REGION( 0x800000, "dups", 0 )   /* duplicate ROMs */
	ROM_LOAD( "csz1cguu.4f",  0x000000, 0x800000, CRC(e1d1bf24) SHA1(daf2c68e2d9a8f313d262d221cc990c93dfdf22f) )
	ROM_LOAD( "csz1cgum.5j",  0x000000, 0x800000, CRC(913c98b5) SHA1(b952dbc19053796077d4f33e8da836893e933b12) )
	ROM_LOAD( "csz1cgll.5m",  0x000000, 0x800000, CRC(0bcd41f2) SHA1(80b74f9398e8bd074f79a14490d06cfeb875c874) )
	ROM_LOAD( "csz1cglm.5k",  0x000000, 0x800000, CRC(d4af93d1) SHA1(0df37b793ce8da02d14f714722382786ae5d3ce2) )
	ROM_LOAD( "csz1ccrl.7m",  0x000000, 0x400000, CRC(1c20768d) SHA1(6cf4280e26f3625d6f750837bf344163e7e93c3d) )
	ROM_LOAD( "csz1ccrh.7k",  0x000000, 0x200000, CRC(bc2fa03c) SHA1(e63d8e75494a383bf9a213edfa9c472a010f8efe) )
ROM_END

} // anonymous namespace


/* Games */
#define GAME_FLAGS ( MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )
//    YEAR, NAME,        PARENT,   MACHINE,     INPUT,     CLASS,                INIT,        MNTR, COMPANY, FULLNAME,                                FLAGS
GAME( 1997, rapidrvr,    0,        rapidrvr,    rapidrvr,  rapidrvr_state,       empty_init,  ROT0, "Namco", "Rapid River (US, RD3 Ver. C)",          GAME_FLAGS ) // 97/11/27, USA
GAME( 1997, rapidrvrv2c, rapidrvr, rapidrvr,    rapidrvr,  rapidrvr_state,       empty_init,  ROT0, "Namco", "Rapid River (World, RD2 Ver. C)",       GAME_FLAGS ) // 97/11/27, Europe
GAME( 1997, rapidrvrp,   rapidrvr, rapidrvr,    rapidrvrp, rapidrvr_state,       empty_init,  ROT0, "Namco", "Rapid River (prototype)",               GAME_FLAGS ) // 97/11/10, USA
GAME( 1997, finfurl,     0,        finfurl,     finfurl,   gorgon_state,         empty_init,  ROT0, "Namco", "Final Furlong (World, FF2 Ver. A)",     GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, downhill,    0,        downhill,    downhill,  namcos23_state,       empty_init,  ROT0, "Namco", "Downhill Bikers (World, DH2 Ver. A)",   GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, downhillu,   downhill, downhill,    downhill,  namcos23_state,       empty_init,  ROT0, "Namco", "Downhill Bikers (US, DH3 Ver. A)",      GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, motoxgo,     0,        motoxgo,     motoxgo,   motoxgo_state,        empty_init,  ROT0, "Namco", "Motocross Go! (World, MG3 Ver. A)",     GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, motoxgov2a,  motoxgo,  motoxgo,     motoxgo,   motoxgo_state,        empty_init,  ROT0, "Namco", "Motocross Go! (US, MG2 Ver. A)",        GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, motoxgov1a,  motoxgo,  motoxgo,     motoxgo,   motoxgo_state,        empty_init,  ROT0, "Namco", "Motocross Go! (Japan, MG1 Ver. A)",     GAME_FLAGS | MACHINE_NODEVICE_LAN )
// LOCAL DEV (phase 9d): all warning-triggering flags dropped so no startup
// warning screens appear during link-emulation testing.  Only MACHINE_SUPPORTS_SAVE
// is kept (it declares save-state support and does not trigger a warning).
// Restore GAME_FLAGS | MACHINE_NODEVICE_LAN before any commit / upstream PR.
GAME( 1997, timecrs2,    0,        timecrs2,    timecrs2,  namcos23_state,       empty_init,  ROT0, "Namco", "Time Crisis II (US, TSS3 Ver. B)",      MACHINE_SUPPORTS_SAVE )
GAME( 1997, timecrs2v2b, timecrs2, timecrs2,    timecrs2,  namcos23_state,       empty_init,  ROT0, "Namco", "Time Crisis II (World, TSS2 Ver. B)",   GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, timecrs2v1b, timecrs2, timecrs2,    timecrs2,  namcos23_state,       empty_init,  ROT0, "Namco", "Time Crisis II (Japan, TSS1 Ver. B)",   GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, timecrs2v4a, timecrs2, timecrs2v4a, timecrs2,  namcoss23_state,      empty_init,  ROT0, "Namco", "Time Crisis II (World, TSS4 Ver. A)",   GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, timecrs2v5a, timecrs2, timecrs2v4a, timecrs2,  namcoss23_state,      empty_init,  ROT0, "Namco", "Time Crisis II (US, TSS5 Ver. A)",      GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1997, panicprk,    0,        panicprk,    panicprk,  namcos23_state,       empty_init,  ROT0, "Namco", "Panic Park (World, PNP2 Ver. A)",       GAME_FLAGS )
GAME( 1997, panicprkj,   panicprk, panicprk,    panicprk,  namcos23_state,       empty_init,  ROT0, "Namco", "Panic Park (Japan, PNP1 Ver. B)",       GAME_FLAGS )
GAME( 1998, gunwars,     0,        gunwars,     gunwars,   namcoss23_gmen_state, empty_init,  ROT0, "Namco", "Gunmen Wars (Japan, GM1 Ver. B)",       GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1998, gunwarsa,    gunwars,  gunwars,     gunwars,   namcoss23_gmen_state, empty_init,  ROT0, "Namco", "Gunmen Wars (Japan, GM1 Ver. A)",       GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1998, raceon,      0,        raceon,      raceon,    namcoss23_gmen_state, empty_init,  ROT0, "Namco", "Race On! (World, RO2 Ver. A)",          GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1998, raceonj,     raceon,   raceon,      raceon,    namcoss23_gmen_state, empty_init,  ROT0, "Namco", "Race On! (Japan, RO1 Ver. B)",          GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1998, 500gp,       0,        _500gp,      500gp,     namcoss23_state,      empty_init,  ROT0, "Namco", "500 GP (US, 5GP3 Ver. C)",              GAME_FLAGS | MACHINE_NODEVICE_LAN )
GAME( 1998, aking,       0,        aking,       aking,     namcoss23_state,      empty_init,  ROT0, "Namco", "Angler King (Japan, AG1 Ver. A)",       GAME_FLAGS )
GAME( 1998, finfurl2,    0,        finfurl2,    finfurl2,  finfurl2_state,       empty_init,  ROT0, "Namco", "Final Furlong 2 (World, FFS2 Ver. A)",  GAME_FLAGS | MACHINE_NODEVICE_LAN ) // 99/02/26  15:08:47 Overseas
GAME( 1998, finfurl2j,   finfurl2, finfurl2,    finfurl2,  finfurl2_state,       empty_init,  ROT0, "Namco", "Final Furlong 2 (Japan, FFS1 Ver. A)",  GAME_FLAGS | MACHINE_NODEVICE_LAN ) // 99/02/26  15:03:14 Japanese
GAME( 1999, crszone,     0,        crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (World, CSZO4 Ver. B)",     GAME_FLAGS )
GAME( 1999, crszonev4a,  crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (World, CSZO4 Ver. A)",     GAME_FLAGS )
GAME( 1999, crszonev3b,  crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (US, CSZO3 Ver. B, set 1)", GAME_FLAGS )
GAME( 1999, crszonev3b2, crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (US, CSZO3 Ver. B, set 2)", GAME_FLAGS )
GAME( 1999, crszonev3a,  crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (US, CSZO3 Ver. A)",        GAME_FLAGS )
GAME( 1999, crszonev2a,  crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (World, CSZO2 Ver. A)",     GAME_FLAGS )
GAME( 1999, crszonev2b,  crszone,  crszone,     crszone,   crszone_state,        empty_init,  ROT0, "Namco", "Crisis Zone (World, CSZO2 Ver. B)",     GAME_FLAGS )

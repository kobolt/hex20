# hex20
Epson HX-20 Emulator

This software attempts to emulate the [Epson HX-20](https://en.wikipedia.org/wiki/Epson_HX-20) portable computer in a Linux terminal.

Features:
* Auto-loading and running of BASIC program text files through automatic key input.
* Dual HD6301 CPU setup supporting almost all instructions.
* LCD panel emulated as '#' pixels using curses in a large 120x32 terminal window.
* Optional LCD panel emulation with ASCII characters in a smaller 20x4 window.
* All 8 international character sets selectable: US, FR, DE, GB, DK, SE, IT, ES
* Most of the keyboard keys are supported, check source code for mapping.
* Selection between 16K (default) or 32K (expansion) RAM possible.
* RTC provided by actual system host clock, so no need to set it.
* Using "Ctrl/@" is not needed because the emulator already initializes the necessary data.
* Debugger with CPU trace and other memory dumping facilities available.
* RS-232 receive (of a file) is possible at 1200 baud through debugger.
* Piezo speaker (audio) support through SDL2.
* External cassette emulation by reading or writing (Mono 8-bit 44100Hz) WAV files.
* Needs the 1.0 or 1.1 system ROM set for the master CPU and the ROM for the slave CPU to run.
* CRC32 check on system ROM files is performed on startup to ensure correct setup.
* Loading of a option ROM at address 0x6000 is also possible.
* Loading of S-records into the MONITOR to set memory through automatic key input.

Known issues and missing features:
* No TF-20 floppy emulation.
* No micro-cassette emulation.
* No micro-printer emulation.
* DAA, SWI and WAI CPU instructions are not implemented.
* RS-232 has issues with large files and does not support higher 2400/4800 baudrates and handshaking.

Tips:
* Use Ctrl+C to enter the debugger, then enter the 'q' command to quit the emulator.
* F9 is mapped to the "BREAK" key, used to break running BASIC programs.
* F8 is mapped to the "MENU" key, to get back to the HX-20 main menu.

Screenshot of the 120x32 pixel LCD emulation:
```
 ###  ##### ####  #            ###         ###          #     #     #          ##     #
#   #   #   #   # #         # #   #         #                 #                 #
#       #   #   # #        #  # ###         #   ####   ##   #####  ##   ####    #    ##   #####  ###
#       #   ####  #	  #   # # #         #   #   #   #     #     #       #   #     #      #  #   #
#       #   # #   #	 #    # ###         #   #   #   #     #     #    ####   #     #     #   #####
#   #   #   #  #  #     #     #             #   #   #   #     #     #   #   #   #     #    #    #
 ###    #   #   # #####        ###         ###  #   #  ###     ##  ###   ####  ###   ###  #####  ####

  #         #   #  ###  #   #  ###  #####  ###  ####
 ##         ## ## #   # #   #   #     #   #   # #   #
  #         # # # #   # ##  #   #     #   #   # #   #
  #         # # # #   # # # #   #     #   #   # ####
  #         #   # #   # #  ##   #     #   #   # # #
  #         #   # #   # #   #   #     #   #   # #  #
 ###        #   #  ###  #   #  ###    #    ###  #   #

 ###        ####    #    ###   ###   ###
#   #       #   #  # #  #   #   #   #   #
    #       #   # #   # #	#   #
   #        ####  #   #  ###    #   #
  #         #   # #####     #   #   #
 #          #   # #   # #   #   #   #   #
#####       ####  #   #  ###   ###   ###
```

ROM information and checksums:
| Filename    | Version | CRC32    | MD5                              |
|-------------|---------|----------|----------------------------------|
| basic1.rom  | 1.0     | 33fbb1ab | 6b7541f35820ce50cc375e1fda39dfd9 |
| basic2.rom  | 1.0     | 27d743ed | ee29f72df2b55f21cfe8dd7fcc4e2e92 |
| monitor.rom | 1.0     | ed7482c6 | a110e9d42af302fa36fbb2c2edb5fe88 |
| utility.rom | 1.0     | f5cc8868 | 2e8a5acce5208341f7200277c8d398a5 |
| basic1.rom  | 1.1     | 4de0b4b6 | 0853e1c34c1183b6c8e0be63a6ed189e |
| basic2.rom  | 1.1     | 10d6ae76 | 3e1a2d2db6e41f15cb1f65b475f7c05c |
| monitor.rom | 1.1     | 101cb3e8 | cd18aca262fdb4fa1f5d145e6039c141 |
| utility.rom | 1.1     | 26c203a1 | 3d46c1cd4bc95ebf3d486499aadfd009 |
| slave.rom   | N/A     | b36f5b99 | 51053c9c726edeef95d2debba8649f0c |

Information on my blog:
* [Epson HX-20 Emulator](https://kobolt.github.io/article-203.html)
* [Epson HX-20 Emulator Sound Support](https://kobolt.github.io/article-214.html)
* [Epson HX-20 Emulator External Cassette Support](https://kobolt.github.io/article-218.html)

YouTube videos:
* [Pick a Match](https://www.youtube.com/watch?v=dpQw2QPLM_Q)
* [Minesweeper](https://www.youtube.com/watch?v=atJrgReYC5I)
* [Artillery](https://www.youtube.com/watch?v=u1FT2iOwCAQ)
* [Attack of the Sine Wave from Outer Space](https://www.youtube.com/watch?v=q-rid6iUhw8)
* [Data Transfer](https://www.youtube.com/watch?v=No1LgJNcGDE)


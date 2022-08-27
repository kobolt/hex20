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
* Needs the 1.0 or 1.1 ROM set for the master CPU and the ROM for the slave CPU to run.
* CRC32 check on ROM files is performed on startup to ensure correct setup.

Known issues and missing features:
* No buzzer/sound emulation.
* No TF-20 floppy emulation.
* No micro-cassette emulation.
* DAA, SWI and SWI CPU instructions are not implemented.
* POINT() function in BASIC does not work. (Due to missing bi-directional LCD controller communication.)
* RS-232 has issues with large files and does not support higher 2400/4800 baudrates and handshaking.

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


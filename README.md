USB DFU bootloader for Bondtech INDX board
==========================================

This is a DFU-capable USB bootloader for the Bondtech INDX toolboard.

It supports directly updating and booting a firmware image, as well as
chainloading to a second stage bootloader. This is used to support RepRapFirmware
CAN programming as well as DFU-based programming.

This directory contains only the USB bootloader itself. To build the amalgation
bootloader, the correct Duet3 bootloader must be supplied as part of the build.

## Usage

To download a firmware to the board, use `dfu-util` or other suitable DFU
programming utility, e.g.:

```bash
dfu-util -D firmware.dfu
```

The firmware must prepared to be compatible with RepRapFirmware. In particular:

- The firmware must be built to run at offset 0x10000, where the bootloader will
  place it.
- At offset 0x1C of the firmware binary must be a pointer to the end of the
  firmware in memory. I.e. the pointer should be to 0x10000 + the length of the
  firmware binary.
- Finally, a CRC32 of the entire preceeding firmware must be appended to the end
  of the firmware binary.

## Building

To build the bootloader, you'll need at least:

- uv and Python ≥3.9
- an `arm-none-eabi-gcc` toolchain including newlib or other embedded-suitable libc
- git

The bootloader can be compiled in either standalone or chainloading mode.

To compile in standalone mode, simply run:
```bash
$ make clean
$ make all
```

The resulting binary will be at `build/indxboot.elf` and `build/indxboot.bin`.

To compile in chainloading mode, run:
```bash
$ make clean
$ make BOOT2=<path_to_chained_bootloader> amalgamation 
```

The chainloaded bootloader _must_ be in RepRapFirmware compatible format (see
Usage section). This will produce a bootloader at `build/amalgamation.bin`.

## Programming device

To program the board, OpenOCD can be used. If using a standard CMSIS-DAP programming,
the following can be used:

```
openocd -f interface/cmsis-dap.cfg -f target/atsame5x.cfg -c "program build/amalgamation.bin verify reset exit 0x0"

```


## Acknowledgements

This bootloader is based on the [1kB SAMDx1-USB-DFU-Bootloader](https://github.com/majbthrd/SAMDx1-USB-DFU-Bootloader).

![Badge Front](doc/dcfurs-mask-render.png)

DEFCON Furs Badge Animations
============================

The 2020 DEFCON Furs badge, made in honor of DEF CON being cancelled. This year's badge takes the form of an mask that can be worn ontop of your PPE to add a bit
of bling.

### Features Include
 * Lattice iCE40 UltraPlus FPGA with 5k LUTs and 128kB of embedded memory.
 * 20x14 pixel RGB matrix display.
 * RISC-V soft core processor.
 * 64Mbit QSPI flash to store animation programs.
 * MEMS microphone allows the badge to react to your voice.
 * Powered by micro-USB.
 * Animations, firmware, and the FPGA can all be upgraded using USB DFU.

### Further Reading
 * Source code for the FPGA and bootloaders are available on [GitHub](https://github.com/defconfurs/dc28-fur-fpga)
 * A web-based tool for creating JSON animations URL TBD!

Building the Animations
-----------------------
The animations are programmed as RISC-V applications that are loaded by a
small runtime BIOS. To build animations, you will need to install a RISC-V
cross compiler. On Ubuntu 20.04 or later this can be accomplished by installing the `gcc-riscv64-unknown-elf` package. For older systems, prebuilt toolchains
can be downloaded from [SiFive](https://www.sifive.com/software)

To reprogram the badge with new animations, you will also need to install the
`dfu-util` package.

TODO: Document how to setup on Windows using PlatformIO.

To build the animations, you can use the `make.py` script to compile, link and
package the animations together into an image suitable to be programmed to the
user data partition in the flash. This script works like a Makefile, and will
rebuild only the animations that have changed. The compilation process is as
follows:

```
furry@trash:~/dcfurs-badge-dc28$ ./make.py build
BIOS [bios.bin] is up to date
Animation [lineface.bin] is up to date
Animation [djmode.bin] is up to date
Animation [mic-test.bin] is up to date
Animation [matrix.bin] is up to date
Animation [northern-lights.bin] is up to date
Animation [lightning-voice.bin] is up to date
Animation [rainbow-grin.bin] is up to date
Animation [marquee-image.bin] is up to date
Animation [rainbow.bin] building...
   Rendering [rainbow.json]
   Compiling [jsmain.c]
   Compiling [syscalls.c]
   Compiling [framebuf.c]
   Compiling [muldiv.c]
   Linking   [rainbow.elf]
   Packing   [rainbow.bin]
Animation [test.bin] is up to date
Bundling [animations.bin]
```

To clean your working directory, the `make.py` script can also be invoked with
the `clean` command to remove all compiled and generated files:

```
furry@trash:~dcfurs-badge-dc28$ ./make.py clean
Cleaning [bios]
Cleaning [animations.bin]
Cleaning [json]
Cleaning [animations.bin]
Cleaning [lineface]
Cleaning [djmode]
Cleaning [mic-test]
Cleaning [matrix]
Cleaning [northern-lights]
Cleaning [lightning-voice]
Cleaning [rainbow-grin]
Cleaning [marquee-image]
Cleaning [animations.bin]
```

The final command available in `make.py` is the `upload` command, which will
perform the DFU update process, and reboot the badge back into runtime mode
when the update is complete.
```
furry@trash:~dcfurs-badge-dc28$ ./make.py upload
dfu-util 0.9

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2016 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
Opening DFU capable USB device...
ID 26f3:1338
Run-time device DFU version 0110
Claiming USB DFU Interface...
Setting Alternate Setting #1 ...
Determining device status: state = dfuIDLE, status = 0
dfuIDLE, continuing
DFU mode device DFU version 0110
Device returned transfer size 256
Copying data from PC to DFU device
Download	[=========================] 100%       721920 bytes
Download done.
state(2) = dfuIDLE, status(0) = No error condition is present
Done!
Resetting USB to switch back to runtime mode
```

Animation Programming with C
============================

Hello World
-----------
As a simple demonstration, the following C code will generate an animation which
cycles through rainbow of colors.

```C
/* A simple hello world and rainbow example */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "badge.h"

static const uint16_t colours[8] = {
   0xF800, /* Red */
   0xF300, /* Orange */
   0xF5E0, /* Yellow */
   0x07C0, /* Green */
   0x07FF, /* Cyan */
   0x001F, /* Blue */
   0x7817, /* Purple */
   0xFFFF  /* White */
};

void main(void)
{
   struct framebuf *buf = framebuf_alloc();
   int index;

   printf("Hello World!\n");

   for (index = 0; buf; index++) {
      uint16_t color;
      int i;

      /* Update the color. */
      color = colours[index % (sizeof(colours)/sizeof(colours[0]))];
      for (i = 0; i < (DISPLAY_VRES * DISPLAY_HWIDTH); i++) {
         buf->data[i] = color;
      }
      framebuf_render(buf);

      /* Do nothing for 1 second. */
      usleep(1000000);
   }
}
```

To add this animation to your badge, we must first create a new directory in the
project, with the name of this animation (eg: `mkdir hello-world`), then save this
file in that directory with a file extension of `.c`.

Then we must modify the `make.py` script to include the new animation. Look for the
`animations` list near the top of the file, and add our new animation. The list is
sorted in the order through which the animations are played.

```Python
animations = ['hello-world',
              'marquee-image',
              'lineface',
              'djmode',
              'mic-test',
              'matrix',
              'missingno',
              'northern-lights',
              'lightning-voice',
              'rainbow-grin']
```

And finally, we can rebuild the animations by running `make.py` to produce an
updated `animations.bin` file.

When connected by USB to a PC, the badge will also enumerate as a USB serial
device. Connecting to the badge with a console will show the following text
when starting our new animation:
```
Animation at slot 9 exitied with code=1
Hello World!
```

Rendering Frames
----------------
A small C interface is provided to access the LED matrix. These functions make use
of the `struct framebuf` type:

```C
#define DISPLAY_HRES    20  /* Number of active pixels per row */
#define DISPLAY_VRES    14  /* Number of active pixels per column */
#define DISPLAY_HWIDTH  32  /* Number of total pixels per row */

struct framebuf {
    uint16_t data[DISPLAY_HWIDTH * DISPLAY_VRES];
};
```

Pixels in the frame buffer are in little-endian byte order, and use a 16-bit RGB
color space (5-bits red, 6-bits green and 5 bits red).

### `struct framebuf *framebuf_alloc(void)`
This method allocates a buffer from the frame buffer memory region, suitable for
rendering to the LED matrix. If there is sufficient space availalbe in the region
this will return a pointer to the newly allocated frame buffer, otherwise a `NULL`
pointer will be returned.

Memory allocated by the `framebuf_alloc()` function should be freed by a matching
call to `framebuf_free()` when the frame is no longer in use.

### `void framebuf_free(struct framebuf *frame)`
Return an allocated frame buffer back to the frame buffer region when the frame
is no longer in use.

### `void framebuf_render(const struct framebuf *frame)`
Render the contents of a `struct framebuf` to the LED matrix. The memory pointed
to by `frame` must be contained within the frame buffer region, otherwise it is
not defined what will be displayed.

### `uint16_t hsv2pixel(unsigned int hue, uint8_t sat, uint8_t value)`
A small helper routine is provided to convert the HSV color space into 16-bit RGB
suitable for rendering to the display. The `hue` argument is an color angle in
degrees from `0` to `359` degrees. While `sat` and `value` repesent the saturation
and value components in the range of `0` to `255`.

Accessing the Microphone
------------------------
A MEMS PDM microphone is provided to measure audio levels as seen by the badge, this
can be used to write animations that react to voice and other sources of noise. An
instantaneous measurement from the microphone by reading the `mic` register out of
the `MISC` register blocks. The value of this register is a signed 32-bit integer
giving the noise level.

```C
int audio_sample = MISC->mic;
```

TODO: Write some library code to make it easier to get a filtered value and do
sensible peak detection.

Pre-Rendered Frames
-------------------
In addition to generating frames programmatically, it is also possible to include
PNG and JPEG images for the animations. When these images are found, the `make.py`
script will rescale and convert them into frames suitable for display by the badge.

For example, if a PNG image with the filename `example.png` is present in the
animation directory. That animation can render it directly to the LED matrix with
the following code:

```C
#include "badge.h"

extern const struct framebuf example_png;

framebuf_render(&example_png);
```

Animation Programming with JSON
===============================
For simple animations that don't require user algorithmic generation, the frames
can also be provided in JSON format. These animations should be placed in the
`json/` directory and named with a `.json` file extension. The `make.py` script
will generate animation programs for each JSON file found.

The JSON file should contain an array, with each element of the array containing
a JSON object that defines the frame, and the duration for which that frame should
be displayed. The animation will run indefinetely looping over each frame in the
array. Each frame object must contain an `interval` member encoded as an integer
number of milliseconds and at least one valid encoding of frame data.

Frame data is always encoded as a hexadecimal string, with the colon character
used to delimit rows of the display. The name of the object member is used to
distinguish between the supported pixel encodings.

Frame objects which encode color shall include an `rgb` member that contains a
hexadecimal string representation of the frame. Each pair of nibbles of the string
sets the color for a single pixel in 8-bit color.

Frame objects may also use a palette color scheme by including a `palette` member,
in which case a single nibble encodes each pixel using the 16 xterm system colors.

Memory Layout and Boot Process
------------------------------

| Address    | Size   | Description
|------------|--------|--------------------------------------
| 0x00000000 | 4KiB   | BIOS and initial bootstrap code.
| 0x10000000 | 4KiB   | Stack memory
| 0x20000000 | 64KiB  | Animation code, RAM and heap memory.
| 0x30000000 | 8MiB   | Memory mapped QSPI flash (read only).
| 0x30000000 | 256KiB | Firstboot FPGA bitstream.
| 0x30040000 | 256KiB | USB-DFU bootloader bitstream.
| 0x30080000 | 256KiB | RISC-V application bitstream.
| 0x30100000 | 7MiB   | BIOS and animation data partition.
| 0x40000000 | 256B   | Miscellaneous peripheral block.
| 0x40010000 | 256B   | USB/UART peripheral block.
| 0x40020000 | 64KiB  | Frame buffer memory.

At initial power-on, the FPGA loads and begins to execute the firstboot
bitstream. This bitstream checks the status of the buttons, and selects
the next bitstream to be loaded. If the top button is held at boot, the
USB-DFU bootloader will be loaded, otherwise the RISC-V application
bitstream will be loaded.

After loading the RISC-V application bitstream, the BIOS memory will be loaded
with a small boot stub and the RISC-V processor will begin to execute from
address 0x00000000. This bootstub will check for an application image header
at the start of the BIOS and animation data partition. If found, the boot stub
will overwrite itself with the BIOS and jump to its entry point.

Once started, the BIOS will then manage the loading of animations out of the
QSPI flash and into RAM and executing them.

Firmware and Animations Upgrade
===============================
The badge includes a USB-DFU bootloader, which is capable of upgrade the badge's
firmware and animation data from a PC over USB. To perform this upgrade, you will
need the `multiboot.bin` or `animations.bin` images, as well as DFU programming
software such as the [dfu-util](http://dfu-util.sourceforge.net/) program for
Linux or OSX, or [DfuSe](https://www.st.com/en/development-tools/stsw-stm32080.html)
for Windows.

When in DFU mode, the badge offers three partitions, or alt-modes, which can be
upgraded as follows:

| Alt Mode | Name       | Description
|----------|------------|-------------------
| 0        | User Image | RISC-V application firmware.
| 1        | User Data  | BIOS and animation data partition.
| 2        | Bootloader | USB-DFU bootloader firmware.

### Linux and Mac OSX
1. Completely power down the badge by removing USB power.
2. Power on badge via USB while holding down switch `SW1`, located closest
    to the DCFurs logo. The badge should enumerate with the PC as a
    `DEFCON Furs DC28 Booploader` device.
3. Execute the command `dfu-util -a0 -d 26f3:1338 -D multiboot.bin -R`. Note that
    this may require `sudo` depending on your operating system and USB permissions.
4. Wait for the upgrade to complete, which may take up to 30 seconds.
5. At the end of the upgrade, the badge will reboot into the new firmware.

### Windows
1. Completely power down the badge by removing USB power.
2. Power on badge via USB while holding down switch `SW1`, located closest
    to the DCFurs logo.
3. Run the `DfuSe demonstration` application from STMicroelectronics.
4. Click the `Choose` button to select the `multiboot.bin` file.
5. Uncheck the `Optimize upgrade duration` checkbox to ensure all data is written.
6. Check the `Verify after download` checkbox if you want to launch the verification process
    after downloading the firmware image to the badge.
7. Click the `Upgrade` button to start upgrading file content to the memory.
8. Click the `Verify` button to verify if the data was successfully downloaded.
9. Unplug the badge from USB and restart the badge to boot into the new firmware.

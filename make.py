#!/usr/bin/python3
import os
import sys
import json
import argparse
import subprocess
from PIL import Image, ImageOps
from glob import glob

animSize = 65536

srcdir = os.path.dirname(os.path.abspath(__file__))
ldsfile = os.path.join(srcdir,'animation.lds')

# The list of animations to build.
animations = ['lineface',
              'djmode',
              'mic-test',
              'matrix',
              'northern-lights',
              'lightning-voice',
              'rainbow-grin',
              'marquee-image']

# The list of  JSON animations to build.
jsdir = os.path.join(srcdir, 'json')
jsanimations = glob(os.path.join(jsdir, '*.json'))

CFLAGS = ['-Os', '-march=rv32i', '-mabi=ilp32', '-I', srcdir]
CFLAGS += ['-ffunction-sections', '-fdata-sections', '--specs=nano.specs']
CFLAGS += ['-D', '_POSIX_TIMERS', '-D', '_POSIX_MONOTONIC_CLOCK=200112L']

#######################################
## Locate Toolchain Paths
#######################################
if os.name=='nt':
    platformio_rel = '.platformio\\packages'
    #pio_rel = '.platformio\\packages\\toolchain-icestorm\\bin'
    home_path = os.getenv('HOMEPATH')

    # Build the full path to risc-v tools
    platformio = os.path.join(home_path, platformio_rel)

    # Tools used in the flow
    gcc           = os.path.join(platformio, 'toolchain-riscv\\bin\\riscv64-unknown-elf-gcc.exe')
    objcopy       = os.path.join(platformio, 'toolchain-riscv\\bin\\riscv64-unknown-elf-objcopy.exe')
    objdump       = os.path.join(platformio, 'toolchain-riscv\\bin\\riscv64-unknown-elf-objdump.exe')
    
else:
    pio_rel = '.platformio/packages/'
    pio = os.path.join(os.environ['HOME'], pio_rel)
    
    # Use PlatformIO, if it exists.
    if os.path.exists(pio):
        gcc           = os.path.join(pio, 'toolchain-riscv/bin/riscv64-unknown-elf-gcc')
        objcopy       = os.path.join(pio, 'toolchain-riscv/bin/riscv64-unknown-elf-objcopy')
    # Otherwise, assume the tools are in the PATH.
    else:
        gcc           = 'riscv64-unknown-elf-gcc'
        objcopy       = 'riscv64-unknown-elf-objcopy'


#######################################
## Check if Recompilation is Needed
#######################################
def check_rebuild(*args, target):
    """Checks if the firmware needs to be rebuilt.

    Args:
       target (string): Name of the target to be built.
       *args (string): All of the dependencies of the target.
    
    Returns:
        True if the target needs to be rebuilt, and False otherwise.
    """
    if not os.path.exists(target):
        return True
    
    targetTime = os.path.getmtime(target)

    if (os.path.getmtime(__file__) > targetTime):
        return True
    
    for dep in args:
        if (os.path.getmtime(dep) > targetTime):
            return True

    return False

#######################################
## Compile a Single Source File
#######################################
def filename_to_cname(name):
    return "".join([ch if ch.isalnum() else '_' for ch in name])

def js_to_frame(frame, name, fp):
    fstruct = "const struct framebuf __attribute__((section(\".frames\"))) %s = { .data = {" % name
    fp.write(fstruct.encode('utf-8'))

    ## 8-bit RGB format
    if 'rgb' in frame:
        for line in frame['rgb'].split(":"):
            fp.write("\n   ".encode('utf-8'))
            pxdata = bytearray.fromhex(line)
            for i in range(0,32):
                try:
                    r = (pxdata[i] & 0xE0) << 8
                    g = (pxdata[i] & 0x1C) << 6
                    b = (pxdata[i] & 0x03) << 3
                except Exception as e:
                    r = 0
                    g = 0
                    b = 0

                pxstr = " 0x%04x," % (r | g | b)
                fp.write(pxstr.encode('utf-8'))
    ## Xterm Pallette format
    elif 'palette' in frame:
        palette = [
            0x0000, # Black
            0x8000, # Maroon
            0x0400, # Green
            0x8400, # Olive
            0x0010, # Navy
            0x8010, # Purple
            0x0410, # Teal
            0xC618, # Silver
            0x8410, # Grey
            0xF800, # Red
            0x07E0, # Lime
            0xFFE0, # Yellow
            0x001F, # Blue
            0xF81F, # Fuchsia
            0x07FF, # Aqua
            0xFFFF, # White
        ]
        for line in frame['palette'].split(":"):
            fp.write("\n   ".encode('utf-8'))
            for i in range(0,32):
                try:
                    ch = line[i]
                    pixel = palette[int(ch, base=16)]
                except Exception as e:
                    pixel = 0x0000
                pxstr = " 0x%04x," % pixel
                fp.write(pxstr.encode('utf-8'))

    ftail = "\n}}; /* %s */\n\n" % name
    fp.write(ftail.encode('utf-8'))

def compile(target, source):
    """Compile a single source.

    Args:
       target (string): Name of the output file to be generated.
       source (string): Name of the source file to be compiled.
    """
    ext = os.path.splitext(source)[1]

    if (ext == '.c' or ext == '.s'):
        print("   Compiling [" + os.path.basename(source) + "]")
        subprocess.check_call([gcc] + CFLAGS + ['-c', '-o', target, source], stderr=subprocess.STDOUT)
        return
    
    if ext == '.json':
        # Parse the JSON file.
        with open(source) as jsfile:
            frames = json.load(jsfile)

        # Output the JSON animation data and compile it.
        print("   Rendering [" + os.path.basename(source) + "]")
        with subprocess.Popen([gcc] + CFLAGS + ['-c', '-o', target, '-xc', '-'], stderr=subprocess.STDOUT, stdin=subprocess.PIPE) as p:
            #with subprocess.Popen(["sed", "-n", "w %s" % target], stderr=subprocess.STDOUT, stdin=subprocess.PIPE) as p:
            boilerplate = "/* Generated by make.py from %s */\n" % os.path.basename(source)
            boilerplate += "#include <stdint.h>\n"
            boilerplate += "#include <stdlib.h>\n"
            boilerplate += "#include <badge.h>\n\n"
            boilerplate += "const char *json_name = \"%s\";\n\n" % os.path.splitext(os.path.basename(source))[0]
            p.stdin.write(boilerplate.encode('utf-8'))

            # Render the frames into a framebuf structure.
            frameno = 0
            for f in frames:
                js_to_frame(f, "frame%u" % frameno, p.stdin)
                frameno += 1
            
            # And output the animation schedule.
            p.stdin.write("const struct frame_schedule schedule[] = {\n".encode('utf-8'))
            frameno = 0
            for f in frames:
                interval = int(f['interval']) * 1000
                schedule = "    { .interval = %u, .frame = &frame%u},\n" % (interval, frameno)
                p.stdin.write(schedule.encode('utf-8'))
                frameno += 1
            p.stdin.write("    { 0, NULL }\n".encode('utf-8'))
            p.stdin.write("}; /* schedule */\n\n".encode('utf-8'))

        return

    if ext in ('.png', '.jpg', '.jpeg'):
        frame = Image.open(source)
        if not "_fullframe." in source:
            frame.resize((20,14))
            frame = ImageOps.pad(frame, (32,14), centering=(0,0))

        print("   Rendering [" + os.path.basename(source) + "]")
        with subprocess.Popen([gcc] + CFLAGS + ['-c', '-o', target, '-xc', '-'], stderr=subprocess.STDOUT, stdin=subprocess.PIPE) as p:
            if not "_fullframe." in source:
                boilerplate = """
                    /* Generated by make.py */
                    #include <stdint.h>
                    #include <badge.h>

                    const struct framebuf __attribute__((section(".frames"))) %s = { .data = {
                    """ % (filename_to_cname(os.path.basename(source)))
                tail = "}};"
            else:
                image_width = frame.size[0]
                boilerplate = """
                    /* Generated by make.py */
                    #include <stdint.h>
                    #include <badge.h>

                    const int %s_width = %d;

                    const uint16_t __attribute__((section(".frames"))) %s[] = { 
                    """ % (filename_to_cname(os.path.basename(source)), image_width, filename_to_cname(os.path.basename(source)))
                tail = "};"

            p.stdin.write(boilerplate.encode('utf-8'))
            for pixel in list(frame.getdata()):
                r = (pixel[0] >> 3) & 0x1F
                g = (pixel[1] >> 2) & 0x3F
                b = (pixel[2] >> 3) & 0x1F
                line = "   0x%04x," % ((r << 11) | (g << 5) | b)
                p.stdin.write(line.encode('utf-8'))
            
            p.stdin.write(tail.encode('utf-8'))
            p.stdin.close()
        return
    
    # Otherwise, we don't understand this file type.
    raise Exception("Unknown file type for " + os.path.basename(source))

#######################################
## Recompile the BIOS
#######################################
def buildrom(name):
    """Rebuild the badge BIOS

    BIOS sources will be found at srcdir/name, where
    srcdir is the location of the make.py script. Compiled
    files will be generated in $(pwd)/name, and the output
    animation file will be at $(pwd)/name/name.bin

    Args:
       name (string): Name of the bios to be build.
    """
    # Assemble the list of sources for this animation.
    biosdir = os.path.join(srcdir, name)
    objdir = name
    sources  = glob(os.path.join(biosdir, '*.c'))
    sources += glob(os.path.join(biosdir, '*.s'))
    objects = []

    # Firmware target image(s) should match the dirname.
    elf_target = os.path.join(objdir, name + '.elf')
    bin_target = os.path.join(objdir, name + '.bin')
    print("BIOS [" + os.path.basename(bin_target) + "] ", end='')
    if not check_rebuild(*sources, ldsfile, target=bin_target):
        print("is up to date")
        return
    else:
        print("building...")
    
    # Create the output directory, if it doesn't already exist.
    if not os.path.exists(objdir):
        os.mkdir(objdir)
    
    # Rebuild each source into an object file.
    for srcfile in sources:
        (root, ext) = os.path.splitext(srcfile)
        objfile = root + '.o'
        compile(objfile, srcfile)
        objects.append(objfile)

    # Link the BIOS together.
    LDFLAGS = ['-Wl,-Bstatic,-T,%s,--gc-sections' % glob(os.path.join(biosdir, '*.lds'))[0]]
    print("   Linking   [" + os.path.basename(elf_target) + "]")
    if subprocess.call([gcc] + CFLAGS + LDFLAGS + ['-o', elf_target] + objects, stderr=subprocess.STDOUT) != 0:
        return

    # Convert to a binary file.
    print("   Packing   [" + os.path.basename(bin_target) + "]")
    if subprocess.call([objcopy, '-O', 'binary', elf_target, bin_target], stderr=subprocess.STDOUT) != 0:
        return

#######################################
## Recompile Animations
#######################################
def build(name):
    """Rebuild a single animation.

    Animation sources will be found at srcdir/name, where
    srcdir is the location of the make.py script. Compiled
    files will be generated in $(pwd)/name, and the output
    animation file will be at $(pwd)/name/name.bin

    Args:
       name (string): Name of the animation to be build.
    """
    # Assemble the list of sources for this animation.
    animdir = os.path.join(srcdir, name)
    objdir = name
    sources  = [os.path.join(srcdir, 'syscalls.c')]
    sources += [os.path.join(srcdir, 'framebuf.c')]
    #sources += [os.path.join(srcdir, 'muldiv.c')]
    sources += glob(os.path.join(animdir, '*.c'))
    sources += glob(os.path.join(animdir, '*.s'))
    sources += glob(os.path.join(animdir, '*.png'))
    sources += glob(os.path.join(animdir, '*.jpg'))
    sources += glob(os.path.join(animdir, '*.jpeg'))
    objects = []

    # Firmware target image(s) should match the dirname.
    elf_target = os.path.join(objdir, name + '.elf')
    bin_target = os.path.join(objdir, name + '.bin')
    print("Animation [" + os.path.basename(bin_target) + "] ", end='')
    if not check_rebuild(*sources, ldsfile, target=bin_target):
        print("is up to date")
        return
    else:
        print("building...")

    # Create the output directory, if it doesn't already exist.
    if not os.path.exists(objdir):
        os.mkdir(objdir)
    
    # Rebuild each source into an object file.
    for srcfile in sources:
        (root, ext) = os.path.splitext(srcfile)
        objfile = root + '.o'
        compile(objfile, srcfile)
        objects.append(objfile)
    
    # Link the animation together.
    LDFLAGS = ['-Wl,-Bstatic,-T,%s,--gc-sections' % ldsfile]
    print("   Linking   [" + os.path.basename(elf_target) + "]")
    if subprocess.call([gcc] + CFLAGS + LDFLAGS + ['-o', elf_target] + objects, stderr=subprocess.STDOUT) != 0:
        return

    # Convert to a binary file.
    print("   Packing   [" + os.path.basename(bin_target) + "]")
    if subprocess.call([objcopy, '-O', 'binary', elf_target, bin_target], stderr=subprocess.STDOUT) != 0:
        return

#######################################
## Recompile JSON Animations
#######################################
def buildjson(jsfile):
    """Rebuild a single animation from JSON syntax.

    An animation may be provided in JSON format, which contains
    an array of frames, and the interval between each frame in
    milliseconds. The name of the animation is the base filename
    of the jsfile with its extension removed.

    Compiled files will be generated in $(pwd)/name, and the
    output animation binary will be at $(pwd)/name/name.bin

    Args:
       jsfile (string): Filename of the JSON source.
    """
    name = os.path.splitext(os.path.basename(jsfile))[0]
    objdir = 'json'
    objects = []

    sources  = [jsfile]
    sources += [os.path.join(srcdir, 'jsmain.c')]
    sources += [os.path.join(srcdir, 'syscalls.c')]
    sources += [os.path.join(srcdir, 'framebuf.c')]
    sources += [os.path.join(srcdir, 'muldiv.c')]

    # Firmware target image(s) should match the dirname.
    elf_target = os.path.join(objdir, name + '.elf')
    bin_target = os.path.join(objdir, name + '.bin')
    print("Animation [" + os.path.basename(bin_target) + "] ", end='')
    if not check_rebuild(*sources, ldsfile, target=bin_target):
        print("is up to date")
        return
    else:
        print("building...")
    
    # Create the output directory, if it doesn't already exist.
    if not os.path.exists(objdir):
        os.mkdir(objdir)
    
    # Rebuild each source into an object file.
    for srcfile in sources:
        (root, ext) = os.path.splitext(srcfile)
        objfile = root + '.o'
        compile(objfile, srcfile)
        objects.append(objfile)
    
    # Link the animation together.
    LDFLAGS = ['-Wl,-Bstatic,-T,%s,--gc-sections' % ldsfile]
    print("   Linking   [" + os.path.basename(elf_target) + "]")
    if subprocess.call([gcc] + CFLAGS + LDFLAGS + ['-o', elf_target] + objects, stderr=subprocess.STDOUT) != 0:
        return

    # Convert to a binary file.
    print("   Packing   [" + os.path.basename(bin_target) + "]")
    if subprocess.call([objcopy, '-O', 'binary', elf_target, bin_target], stderr=subprocess.STDOUT) != 0:
        return

#######################################
## Bundle Animcations Together
#######################################
def bundle(*args, target):
    """Bundles the animations together into a data image.

    Args:
       target (string): Name of the target bundle to be built.
       *args (string): Names of the animation files to include in the image.
    """
    if not check_rebuild(target=target, *args):
        print("Image [" + os.path.basename(target) + "] is up to date")
        return
    else:
        print("Bundling [" + os.path.basename(target) + "]")
    
    with open(target, 'wb') as outfile:
        for filename in args:
            length = 0
            
            # Copy the animation data.
            with open(filename, 'rb') as infile:
                while(1):
                    chunk = infile.read(4)
                    if (len(chunk) < 4):
                        break
                    outfile.write(chunk)
                    length += 4
            
            # Pad the animation with zeros.
            while (length < animSize):
                outfile.write(b"\x00\x00\x00\x00")
                length += 4
        
        # Append an extra marker to the end o the image.
        for i in range(256):
            outfile.write(b"\xFF\xFF\xFF\xFF")


#######################################
## Cleanup After Ourselves
#######################################
def clean(*args, target='animations.bin'):
    """Clean any compiled or built files.

    Args:
       target (string): Filename of the animation bundle.
       *args (string): Animations to be cleaned.
    """
    # Cleanup the animation objdirs
    for name in args:
        print("Cleaning [" + name + "]")
        del_files  = glob(os.path.join(name, '*.bin'))
        del_files += glob(os.path.join(name, '*.elf'))
        del_files += glob(os.path.join(name, '*.o'))
        for x in del_files:
            os.remove(x)
    
    # Cleanup the bundled animation.
    print("Cleaning [" + target + "]")
    if os.path.exists(target):
        os.remove(target)

#######################################
## Make Script Entry Point
#######################################
def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('command', metavar='COMMAND', nargs='*', default=['build'],
                        help="Make target to run, one of build|clean|package|upload")
    args = parser.parse_args()

    # Run Commands.
    for command in args.command:
        if command == 'build':
            # Build the BIOS
            buildrom('bios')

            # Build individual animations.
            for x in animations:
                build(x)
            
            for x in jsanimations:
                buildjson(x)
            
            # Bundle animations together into a final image.
            bundle('bios/bios.bin',
                *[ '%s/%s.bin' % (x, x) for x in animations ],
                *[ 'json/%s.bin' % os.path.splitext(os.path.basename(x))[0] for x in jsanimations ],
                target='animations.bin')

        elif command == 'upload':
            if subprocess.call(['dfu-util', '-d', '1d50:6130', '-a1', '-D', 'animations.bin', '-R'], stderr=subprocess.STDOUT) != 0:
                return
        
        elif command == 'clean':
            clean('bios')
            clean('json')
            clean(*animations)
            
        else:
            raise Exception('Invalid command', command)

if __name__=='__main__':
    main()

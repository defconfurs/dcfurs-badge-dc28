#!/usr/bin/python3
import os
import sys
import argparse
import subprocess
from glob import glob

animSize = 65536

srcdir = os.path.dirname(os.path.abspath(__file__))
ldsfile = os.path.join(srcdir,'animation.lds')

CFLAGS = ['-Os', '-march=rv32ic', '-mabi=ilp32', '-I', srcdir]
LDFLAGS = CFLAGS + ['-Wl,-Bstatic,-T,'+ldsfile+',--gc-sections']

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
    sources += glob(os.path.join(animdir, '*.c'))
    sources += glob(os.path.join(animdir, '*.s'))
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
        print("   Compiling [" + os.path.basename(srcfile) + "]")
        if subprocess.call([gcc] + CFLAGS + ['-c', '-o', objfile, srcfile], stderr=subprocess.STDOUT) != 0:
            return
        objects.append(objfile)
    
    # Render image data into frames.
    makeframes = os.path.join(animdir, 'make_frames.py')
    if os.path.exists(makeframes):
        print("   Rendering [" + os.path.basename(makeframes) + "]")
        objfile = os.path.join(objdir, 'frames.o')
        subprocess.call(["python3", makeframes], cwd=animdir, stderr=subprocess.STDOUT)
        subprocess.call([objcopy, '-I', 'binary', '-O', 'elf32-littleriscv',
                '-B', 'riscv',
                'frames.bin', os.path.abspath(objfile)], cwd=animdir, stderr=subprocess.STDOUT)
        objects.append(objfile)
    
    # Link the animation together.
    print("   Linking   [" + os.path.basename(elf_target) + "]")
    if subprocess.call([gcc] + LDFLAGS + ['-o', elf_target] + objects, stderr=subprocess.STDOUT) != 0:
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

    # Each subdirectory should contain the sources for a single animation.
    animations = []
    for x in os.listdir(srcdir):
        if os.path.isdir(os.path.join(srcdir, x)) and os.path.basename(x)[0] != '.':
            animations.append(os.path.basename(x))

    # Run Commands.
    for command in args.command:
        if command == 'build':
            # Build individual animations.
            for x in animations:
                build(x)
            
            # Bundle animations together into a final image.
            bundle(*[ '%s/%s.bin' % (x, x) for x in animations ], target='animations.bin')

        elif command == 'upload':
            if subprocess.call(['dfu-util', '-d', '1d50:6130', '-a1', '-D', 'animations.bin', '-R'], stderr=subprocess.STDOUT) != 0:
                return
        
        elif command == 'clean':
            clean(*animations)
            
        else:
            raise Exception('Invalid command', command)

if __name__=='__main__':
    main()

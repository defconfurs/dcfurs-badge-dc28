#!/usr/bin/python3
import os
import sys
import argparse
from subprocess import call
from glob import glob

animSize = 65536

srcdir = os.path.dirname(os.path.abspath(__file__))
ldsfile = os.path.join(srcdir,'animation.lds')

imageFile = os.path.join(srcdir, 'animations.bin')

firmware_dirs = [os.path.join(srcdir, "northern-lights"),
                 os.path.join(srcdir, "mic-test"),
                 os.path.join(srcdir, "rainbow-grin")]
                 
CFLAGS = ['-Os', '-march=rv32ic', '-mabi=ilp32', '-I', '.']
CFLAGS += ['-DPRINTF_DISABLE_SUPPORT_FLOAT=1', '-DPRINTF_DISABLE_SUPPORT_EXPONENTIAL=1']
CFLAGS += ['-DPRINTF_DISABLE_SUPPORT_LONG_LONG=1', '-DPRINTF_DISABLE_SUPPORT_PTRDIFF_T=1']
LDFLAGS = CFLAGS + ['-Wl,-Bstatic,-T,'+ldsfile+',--gc-sections']

firmwareFiles  = [ os.path.join(srcdir, "entry.s") ]
for x in firmware_dirs:
    firmwareFiles += glob(os.path.join(x, '*.c'))


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
## Check if recompile needed
#######################################
def check_rebuild(*args, imageFile):
    """Checks if the firmware needs to be rebuilt.

    Args:
       name (string): Name of the firmware image (default: "firmware.mem")
       *args (string): All other non-kwargs should provide the source files for the firmware.
    """
    if not os.path.exists(imageFile):
        return True

    imageFileTime = os.path.getmtime(imageFile)
    
    if os.path.getmtime(os.path.join(srcdir, 'make.py')) > imageFileTime:
        return True
    
    for x in args:
        if os.path.getmtime(os.path.join(srcdir, x)) > imageFileTime:
            return True

    return False
    
#######################################
## Recompile firmware
#######################################
def build(*args, imageFile):
    """Rebuilds the firmware.

    Args:
       name (string): Name of the firmware image (default: "firmware")
       *args (string): All other non-kwargs should provide the source files for the firmware.
    """

    objFiles = []
    binFiles = []
    
    for x in args:
        inFile = x
        outFile = os.path.splitext(inFile)[0] + '.o'
        objFiles += [outFile]
        
        if call([gcc] + CFLAGS + ['-c', '-o', outFile, inFile]) != 0:
            print("---- Error compiling ----")
            return

    for firmwareDir in firmware_dirs:
        elfFile = os.path.join(firmwareDir, 'anim.elf')
        objFiles = [ os.path.join(srcdir, "entry.o") ]
        objFiles += glob(os.path.join(firmwareDir, '*.o'))
        print('------------------------------')
        print(objFiles)

        makeFrames = os.path.join(firmwareDir, 'make_frames.py')
        if os.path.exists(makeFrames):
            call(["python3", makeFrames], cwd=firmwareDir)
            call([objcopy, '-I', 'binary', '-O', 'elf32-littleriscv',
                  '-B', 'riscv',
                  'frames.bin', 'frames.o'], cwd=firmwareDir)

            framesObjFile = os.path.join(firmwareDir, 'frames.o')
            if not framesObjFile in objFiles:
                objFiles += [framesObjFile]
            
            print(call([objdump, '-t', 'frames.o'], cwd=firmwareDir))
            
        if call([gcc] + LDFLAGS + ['-o', elfFile] + objFiles) != 0:
            print("---- Error compiling ----")
            return

        
        binFile = os.path.join(firmwareDir, 'anim.bin')
        if call([objcopy, '-O', 'binary', elfFile, binFile]) != 0:
            print("---- Error on objcopy ----")
            return
        binFiles += [binFile]

    with open(imageFile, 'wb') as outFile:
        for binFile in binFiles:
            wordCount = 0
            with open(binFile, 'rb') as inFile:
                while(1):
                    word = inFile.read(4)
                    if (len(word) < 4):
                        break
                    outFile.write(word)
                    wordCount += 1
                while (wordCount < animSize/4):
                    outFile.write(b"\x00\x00\x00\x00")
                    wordCount += 1
        # put a marker in the file to show that this is
        # an invalid entry
        for i in range(256):
            outFile.write(b"\xFF\xFF\xFF\xFF")


#######################################
## Cleanup After Ourselves
#######################################
def clean():
    del_files = glob('*.bin')+glob('*.o')
    for firmwareDir in firmware_dirs:
        del_files += glob(os.path.join(firmwareDir, '*.o'))
        del_files += glob(os.path.join(firmwareDir, '*.elf'))
    for del_file in del_files:
        os.remove(del_file)

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('command', metavar='COMMAND', nargs='*', default=['auto'],
                        help="Make target to run, one of build|clean|package|upload")
    args = parser.parse_args()

    if args.command[0] == 'auto':
        newstuff = []

        if check_rebuild(*firmwareFiles, imageFile=imageFile):
            newstuff += ['build']
            
        # get rid of the auto
        args.command = newstuff + args.command[1:]

        
    for command in args.command:
        # run command
        if command == 'build':
            build(*firmwareFiles, imageFile=imageFile)

        elif command == 'upload':
            if call(['dfu-util', '-e', '-a1', '-D', imageFile, '-R']) != 0:
                return
        
        elif command == 'clean':
            clean()
            
        else:
            raise Exception('Invalid command', command)

if __name__=='__main__':
    main()

# this will make a demo loop for the internal system

import json
from PIL import Image, ImageOps
import struct
import io

reason_map = {"now"        :0x00,  # right away - display once (also makes it wrap around if the header is all zeros)
              "frames"     :0x01,  # when the screen has refreshed this many times
              "time"       :0x02,  # after this many ms
              "volume_list":0x03,  # special - the "value" is the number of bits of volume to use (4 means 16 images)
              "boop"       :0xB0}  # no idea, this thing doesn't have a boop feature

def load_file(filename):
    with open(filename, "r") as f:
        frames = json.load(f)
        return frames

def save_bin(frames, filename):
    output = bytearray()
    
    for item in frames:
        frame = Image.open(item["image"])
        frame.resize((20,14))
        frame = ImageOps.pad(frame, (32,14), centering=(0,0))
        
        header = bytearray(128)
        index = 0
        
        for transition in item["transitions"].keys():
            if transition in reason_map.keys():
                code      = reason_map[transition]
                value     = item["transitions"][transition]["value"]
                nextframe = item["transitions"][transition]["value"]
                struct.pack_into("BBh", header, index, code, nextframe, value)
                index += 4

        pixel_list = list(frame.getdata())
        imgstr = bytearray()
        for pixel in pixel_list:
            r = (pixel[0] >> 3) & 0x1F
            g = (pixel[1] >> 2) & 0x3F
            b = (pixel[2] >> 3) & 0x1F
            imgstr += struct.pack("H", (r << 11) | (g << 5) | b)

        output += header + imgstr

    with open(filename, "wb") as f:
        f.write(output)

frames = load_file("frames.json")
save_bin(frames, "frames.bin")


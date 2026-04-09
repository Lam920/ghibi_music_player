#!/usr/bin/env python3
"""
Convert GIF → raw .bin for zero-RAM streaming on Milk-V Duo
Usage: python3 gif2bin.py input.gif output.bin [width] [height]
"""
import sys
import struct
from PIL import Image

ST7789_1_56 = True 
ST7789_2_8 = False

def rgb565(r, g, b):
    """Convert RGB to BGR565 with bit inversion for ST7789 with INVON"""
    # Swap R and B channels for BGR565
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    # Invert bits to compensate for hardware INVON (0x21)
    if ST7789_2_8:
        v = ~v & 0xFFFF
    return v

def convert(gif_path, bin_path, dst_w, dst_h):
    gif = Image.open(gif_path)

    frames = []
    try:
        while True:
            delay_ms = gif.info.get("duration", 40)
            img = gif.convert("RGB").resize((dst_w, dst_h), Image.LANCZOS)
            px  = img.load()

            buf = bytearray(dst_w * dst_h * 2)
            idx = 0
            for y in range(dst_h):
                for x in range(dst_w):
                    r, g, b = px[x, y]
                    v = rgb565(r, g, b)
                    buf[idx]   =  v & 0xFF
                    buf[idx+1] = (v >> 8) & 0xFF
                    idx += 2

            frames.append((delay_ms, bytes(buf)))
            gif.seek(gif.tell() + 1)
    except EOFError:
        pass

    frame_size = dst_w * dst_h * 2

    with open(bin_path, "wb") as f:
        # Header: magic(4) + nframes(4) + w(2) + h(2) + fps(2) + frame_size(4)
        f.write(struct.pack("<4sIHHHI",
            b"ANIM",
            len(frames),
            dst_w, dst_h,
            25,          # target fps
            frame_size))

        for delay_ms, pixels in frames:
            f.write(struct.pack("<H", delay_ms))
            f.write(pixels)

    total = len(frames)
    size_kb = (total * (frame_size + 2)) / 1024
    print(f"Done: {total} frames, {dst_w}x{dst_h}, {size_kb:.1f} KB")
    print(f"Output: {bin_path}")

if __name__ == "__main__":
    gif_path = sys.argv[1]
    bin_path = sys.argv[2] if len(sys.argv) > 2 else "output.bin"
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 240
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 320
    convert(gif_path, bin_path, w, h)
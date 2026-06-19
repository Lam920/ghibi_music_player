#!/usr/bin/env python3
"""
Convert PNG/JPG → raw RGB565 .bin for background loading
Usage: python3 img2bin.py input.png output.bin [width] [height]

The output is a raw binary file containing RGB565 pixels (2 bytes per pixel),
compatible with the load_background() function in text_util.c
"""
import sys
import struct
from PIL import Image

# Display configuration - match your hardware
ST7789_2_8 = True  # Set to True if using ST7789 2.8" with INVON

def rgb565(r, g, b):
    """
    Convert RGB888 to RGB565 format (standard, no channel swap)
    Matches gif2bin.py and display hardware pixel format
    """
    # Standard RGB565: R in high bits [15:11], G in middle [10:5], B in low bits [4:0]
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    
    # Invert bits if needed for ST7789 2.8" displays with INVON mode
    if ST7789_2_8:
        v = ~v & 0xFFFF
    return v

def convert_image(img_path, bin_path, dst_w=240, dst_h=240, rotate=90):
    """
    Convert image to raw RGB565 binary format
    
    Args:
        img_path: Path to input PNG/JPG file
        bin_path: Path to output .bin file
        dst_w: Target width (default: 240)
        dst_h: Target height (default: 240)
    """
    # Open and convert image
    img = Image.open(img_path)
    
    # Convert to RGB and resize
    img = img.rotate(-rotate, expand=True)  # Rotate clockwise by default
    img = img.convert("RGB").resize((dst_w, dst_h), Image.LANCZOS)
    px = img.load()
    
    # Create pixel buffer (2 bytes per pixel)
    buf = bytearray(dst_w * dst_h * 2)
    idx = 0
    
    # Convert each pixel to RGB565
    for y in range(dst_h):
        for x in range(dst_w):
            r, g, b = px[x, y]
            v = rgb565(r, g, b)
            
            # Write as little-endian uint16_t
            buf[idx]     = v & 0xFF
            buf[idx + 1] = (v >> 8) & 0xFF
            idx += 2
    
    # Write raw binary file (no header)
    with open(bin_path, "wb") as f:
        f.write(buf)
    
    size_kb = len(buf) / 1024
    print(f"✓ Converted: {img_path} → {bin_path}")
    print(f"  Resolution: {dst_w}×{dst_h}")
    print(f"  Size: {size_kb:.1f} KB ({len(buf)} bytes)")
    print(f"  Format: RGB565 (uint16_t little-endian)")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 img2bin.py input.png output.bin [width] [height] [rotate]")
        print("\nExamples:")
        print("  python3 img2bin.py background.png 00.bin")
        print("  python3 img2bin.py photo.jpg 01.bin 240 240 90")
        sys.exit(1)
    
    img_path = sys.argv[1]
    bin_path = sys.argv[2] if len(sys.argv) > 2 else "output.bin"
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 240
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 320
    rotate = int(sys.argv[5]) if len(sys.argv) > 5 else 90

    try:
        convert_image(img_path, bin_path, width, height)
    except FileNotFoundError:
        print(f"Error: File not found: {img_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

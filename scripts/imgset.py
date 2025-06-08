import time
from PIL import Image

def hex_to_rgb(hex_color):
    if hex_color.startswith('0x'):
        hex_color = hex_color[2:]
    elif hex_color.startswith('#'):
        hex_color = hex_color[1:]
    return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))

def replace_color(image_path, find_color_hex, set_color_hex):
    find_color_rgb = hex_to_rgb(find_color_hex)
    set_color_rgb = hex_to_rgb(set_color_hex)

    image = Image.open(image_path)
    image = image.convert("RGBA")
    data = image.getdata()

    new_data = []
    for item in data:
        if item[:3] == find_color_rgb:
            new_data.append(set_color_rgb + (item[3],))
        else:
            new_data.append(item)

    image.putdata(new_data)
    image.save(image_path)

def main():
    image_path = "./bitmaps/lamblogo.png"
    color_replacements = [

         # I WANT REPLACE ...       WITH THESE...
        ("0x080000",               "0x55FFFF")
    ]

    for find_color, set_color in color_replacements:
        replace_color(image_path, find_color, set_color)
        time.sleep(0.5)

if __name__ == '__main__':
    main()


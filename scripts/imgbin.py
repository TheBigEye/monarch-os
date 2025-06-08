import argparse
from PIL import Image
import pathlib
from pathlib import Path

# TODO: for faster convertions, we should make that the script converts
# all the images at same time using threading and make a queue check on
# the main thread for not break the Makefile ... IDK, its a idea ..

# 3000iq HERE ...
pixels_cache: dict[tuple[int, int, int], int] = {}

class ImgBin:
    # VGA color constants
    # (each value is 0xXX;
    # lower nibble is the
    # 4bpp index)

    VGA_PALETTE: dict[str, int] = {
        'black':     0x00,
        'blue':      0x11,
        'green':     0x22,
        'cyan':      0x33,
        'red':       0x44,
        'magenta':   0x55,
        'brown':     0x66,
        'ltgray':    0x77,
        'dkgray':    0x88,
        'ltblue':    0x99,
        'ltgreen':   0xAA,
        'ltcyan':    0xBB,
        'ltred':     0xCC,
        'ltmagenta': 0xDD,
        'yellow':    0xEE,
        'white':     0xFF
    }

    VGA_RGB: dict[str, tuple[int, int, int]] = {
        'black':     (0x00, 0x00, 0x00), # 000 000 000
        'blue':      (0x00, 0x00, 0xAA), # 000 000 170
        'green':     (0x00, 0xAA, 0x00), # 000 170 000
        'cyan':      (0x00, 0xAA, 0xAA), # 000 170 170
        'red':       (0xAA, 0x00, 0x00), # 170 000 000
        'magenta':   (0xAA, 0x00, 0xAA), # 170 000 170
        'brown':     (0xAA, 0x55, 0x00), # 170 085 000
        'ltgray':    (0xAA, 0xAA, 0xAA), # 170 170 170
        'dkgray':    (0x55, 0x55, 0x55), # 085 085 085
        'ltblue':    (0x55, 0x55, 0xFF), # 085 085 255
        'ltgreen':   (0x55, 0xFF, 0x55), # 085 255 085
        'ltcyan':    (0x55, 0xFF, 0xFF), # 085 255 255
        'ltred':     (0xFF, 0x55, 0x55), # 255 085 085
        'ltmagenta': (0xFF, 0x55, 0xFF), # 255 085 255
        'yellow':    (0xFF, 0xFF, 0x55), # 255 255 085
        'white':     (0xFF, 0xFF, 0xFF)  # 255 255 255
    }


    @staticmethod
    def dither_me(img: Image.Image) -> None:
        """ Apply Floyd-Steinberg dithering to the image """

        pixels = img.load()
        width, height = img.size
        vga_colors: list[tuple[int, int, int]] = list(ImgBin.VGA_RGB.values())

        # Convert image to float array for dithering
        pixel_array: list[list[list[int]]] = [[list(pixels[x, y]) for x in range(width)] for y in range(height)]

        # Apply Floyd-Steinberg dithering
        for y in range(height):
            for x in range(width):
                old_pixel: list[int] = pixel_array[y][x]

                # Find closest VGA color
                best_color: tuple[int, int, int] = min(vga_colors, key=lambda c: sum((a - b) ** 2 for a, b in zip(old_pixel, c)))

                # Calculate and distribute error
                error: list[int] = [old - new for old, new in zip(old_pixel, best_color)]
                pixels[x, y] = best_color

                # Error diffusion matrix:
                # [      *   7/16]
                # [3/16 5/16 1/16]

                if x + 1 < width:
                    pixel_array[y][x + 1] = [(p + e * 7/16) for p, e in zip(pixel_array[y][x + 1], error)]

                if y + 1 < height:
                    if x > 0:
                        pixel_array[y + 1][x - 1] = [(p + e * 3/16) for p, e in zip(pixel_array[y + 1][x - 1], error)]

                    pixel_array[y + 1][x] = [(p + e * 5/16) for p, e in zip(pixel_array[y + 1][x], error)]

                    if x + 1 < width:
                        pixel_array[y + 1][x + 1] = [(p + e * 1/16) for p, e in zip(pixel_array[y + 1][x + 1], error)]


    @staticmethod
    def get_binary(img: Image.Image) -> bytes:
        """Convert image to packed 4bpp binary format."""

        # Check if dithering is needed
        if len(set(img.getdata())) > 16:
            ImgBin.dither_me(img)

        pixels = img.getdata()
        num_pixels: int = len(pixels)
        nibble_values: list[int] = [0] * num_pixels

        # Map pixels to 4-bit color indices
        for idx, pixel in enumerate(pixels):
            if pixel in pixels_cache:
                nibble_values[idx] = pixels_cache[pixel]
                continue

            # Find closest VGA color
            r, g, b = pixel
            min_distance: float = float('inf')
            best_value: int = None

            for color_name, (vr, vg, vb) in ImgBin.VGA_RGB.items():
                d: float = (r - vr) ** 2 + (g - vg) ** 2 + (b - vb) ** 2
                if d < min_distance:
                    min_distance = d
                    best_value = ImgBin.VGA_PALETTE[color_name]

            color_index: int = best_value >> 4
            pixels_cache[pixel] = color_index
            nibble_values[idx] = color_index

        # Pack nibbles into bytes
        packed_length: int = (num_pixels + 1) // 2
        output: bytearray = bytearray(packed_length)

        for i in range(0, num_pixels, 2):
            upper_nibble: int = nibble_values[i] << 4
            lower_nibble: int = nibble_values[i + 1] if i + 1 < num_pixels else 0
            output[i // 2] = upper_nibble | (lower_nibble & 0x0F)

        return bytes(output)


    @staticmethod
    def convert_file(input_file: Path, output_path: Path = None, verbose: bool = False) -> bool:
        """Convert a single BMP file to binary format."""
        # Only process BMP files
        if input_file.suffix.lower() != '.bmp':
            if verbose:
                print(f"Skipping {input_file.name}: not a BMP file.")
            return False

        try:
            if output_path:
                if output_path.suffix:  # If output has extension, use as full path
                    output_file: str = str(output_path)
                else:  # Use as directory
                    output_path.mkdir(parents=True, exist_ok=True)
                    output_file: str = str(output_path / input_file.with_suffix('.bin').name)
            else:
                output_file: str = str(input_file.with_suffix('.bin'))

            img: Image.Image = Image.open(input_file).convert('RGB')

            if verbose:
                print(f"\nProcessing {input_file.name}...")
                print(f"Image size: {img.size[0]}x{img.size[1]} pixels")

            binary_data: bytes = ImgBin.get_binary(img)
            with open(output_file, 'wb') as f:
                f.write(binary_data)

            if verbose:
                print(f"Successfully converted to {output_file}")
                print(f"Output file size: {len(binary_data)} bytes")
            return True

        except Exception as e:
            print(f"Error processing {input_file.name}: {str(e)}")
            return False


    @staticmethod
    def convert_directory(directory: str, output_path: str = None, verbose: bool = False) -> bool:
        """Convert all BMP files in a directory to binary format."""
        dir_path: pathlib.Path = pathlib.Path(directory)
        if not dir_path.is_dir():
            print(f"Error: {directory} is not a directory")
            return False

        # Ensure output directory exists if specified
        if output_path:
            output_path: pathlib.Path = pathlib.Path(output_path)
            output_path.mkdir(parents=True, exist_ok=True)

        # Search only for BMP files
        bmp_files: list[pathlib.Path] = list(dir_path.glob("*.bmp"))
        if not bmp_files:
            print(f"No BMP files found in {directory}")
            return False

        if verbose:
            print(f"Found {len(bmp_files)} BMP files in {directory}")

        success_count: int = sum(1 for f in bmp_files if ImgBin.convert_file(f, output_path, verbose))

        if verbose:
            print(f"\nProcessing complete: {success_count}/{len(bmp_files)} files converted successfully")
        return success_count > 0


if __name__ == '__main__':
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description='Convert BMP images (4bpp) to a packed 4bpp binary format')
    parser.add_argument('input', help='BMP file or directory containing BMP files')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose output')
    parser.add_argument('-o', '--output', help='Output directory or file path')
    args: argparse.Namespace = parser.parse_args()

    input_path: pathlib.Path = pathlib.Path(args.input)
    output_path: pathlib.Path = pathlib.Path(args.output) if args.output else None

    if input_path.is_dir():
        ImgBin.convert_directory(input_path, output_path, args.verbose)
    else:
        ImgBin.convert_file(input_path, output_path, args.verbose)

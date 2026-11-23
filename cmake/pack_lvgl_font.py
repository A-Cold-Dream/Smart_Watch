"""Pack GUI Guider's row-aligned A8 font exports for the F411 software renderer."""
import argparse
import re
from pathlib import Path


def array_body(source, name):
    match = re.search(rf"\b{name}\[\]\s*=\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"Missing {name} array")
    return match


def replace_body(source, match, body):
    return source[:match.start(1)] + body + source[match.end(1):]


def pack(source, clock):
    stride = re.search(r"\.stride\s*=\s*(\d+)", source)
    if not (stride and int(stride[1]) > 1 and
            re.search(r"\.bpp\s*=\s*8\b", source) and
            re.search(r"\.bitmap_format\s*=\s*0\b", source)):
        return source  # Already packed or exported in a different pixel format.
    alignment = int(stride[1])
    bitmap = array_body(source, "glyph_bitmap")
    pixels = re.sub(r"/\*.*?\*/", "", bitmap[1], flags=re.S)
    pixels = bytes(int(v, 0) for v in pixels.split(",") if v.strip())
    desc = array_body(source, "glyph_dsc")
    glyphs = re.findall(r"\{[^{}]+\}", desc[1])
    if not glyphs:
        raise ValueError("Missing glyph descriptors")

    if clock:
        # Alatsi 48 is used exclusively for HH:MM. Keep the original glyph IDs
        # and kerning tables so all digit pairs have the designer's spacing.
        cmap = array_body(source, "cmaps")
        first = re.search(r"\{[^{}]+\}", cmap[1])[0]
        fields = dict(re.findall(r"\.(\w+)\s*=\s*(\w+)", first))
        if (fields.get("range_start"), fields.get("range_length"),
            fields.get("glyph_id_start"), fields.get("type")) == (
                "48", "11", "1", "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY"):
            glyphs = glyphs[:12]  # Reserved glyph 0, ten digits, colon.
            source = replace_body(source, cmap, "\n    " + first + "\n")
            source = re.sub(r"(\.cmap_num\s*=\s*)\d+", r"\g<1>1", source)

    packed = bytearray()
    output_glyphs = []
    for glyph in glyphs:
        fields = dict(re.findall(r"\.(\w+)\s*=\s*(-?\d+)", glyph))
        offset = int(fields["bitmap_index"])
        width, height = int(fields["box_w"]), int(fields["box_h"])
        pitch = (width + alignment - 1) // alignment * alignment
        if offset + pitch * height > len(pixels):
            raise ValueError(f"Glyph at {offset} exceeds bitmap bounds")
        output_glyphs.append(re.sub(r"(\.bitmap_index\s*=\s*)\d+",
                                    rf"\g<1>{len(packed)}", glyph))
        for row in range(height):
            start = offset + row * pitch
            packed.extend(pixels[start:start + width])

    source = replace_body(source, array_body(source, "glyph_dsc"),
                          "\n    " + ",\n    ".join(output_glyphs) + "\n")
    rows = [", ".join(f"0x{v:02x}" for v in packed[i:i + 16])
            for i in range(0, len(packed), 16)]
    source = replace_body(source, array_body(source, "glyph_bitmap"),
                          "\n    " + ",\n    ".join(rows) + "\n")
    return re.sub(r"(\.stride\s*=\s*)\d+", r"\g<1>1", source)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--clock", action="store_true")
    args = parser.parse_args()
    result = pack(args.source.read_text(encoding="utf-8"), args.clock)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result, encoding="utf-8")

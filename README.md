# icomake
Trivial tool that combines multiple ICO/PNGs into single .ICO file that keeps format of each sub-image.

## Usage
Command line:

    icomake.exe output.ico input1 [input2 [...]]

Input file formats supported:
* Any existing ICO containing either uncompressed bitmaps (BMPs) or PNGs
* 32-bit PNGs (with alpha channel) up to 256×256 pixels in size.

Outputs single ICO file containing all input images.

* If multiple images of the same resolution and color depth are provided, the last (in order it appears on the command line) is used, i.e. you can use the tool to replace single sub-image within existing .ico file.

## PNG icons

Windows Vista and later supports ICO files that contain PNG sub-images. Most graphics applications (at least those that I use) will only save the 256×256 one as PNG, but all, even 16×16, can safely be PNGs too. Thus enter the purpose of this tool, as PNGs are by magnitudes smaller than uncompressed bitmaps.

**Note:** In order for ICOs to show on Windows XP and older, and perhaps other OSs, at least some icons should be stored as bitmaps. At least 32×32 icon, ideally also 16×16, 24×24, and 48×48. This tool does not and can not convert PNG to BMP ICO.

## Sub-image order optimization

Windows icon loader iterates over all icons in file, assessing difference between requested and available icons, stopping only on exact match. The assessment is in both resolution and color depth, preferring resolution over color depth; only to a certain degree though.

In attempt to save it a few cycles and trigger the early return on match, this tool places the most probable icons first. It also honors Microsoft's guidelines for XP software by placing the most common 9 icon sizes first. If all resolutions were available, the order would be following:

1. 48×48×32
1. 32×32×32
1. 16×16×32
1. 48×48×8
1. 32×32×8
1. 16×16×8
1. 48×48×4
1. 32×32×4
1. 16×16×4
1. 48×48×24
1. 32×32×24
1. 16×16×24
1. 24×24×32
1. 24×24×8
1. 24×24×4
1. 24×24×24
1. ?×?×32 (from smallest to largest)
1. ?×?×24 (from smallest to largest)
1. ?×?×8 (from smallest to largest)
1. ?×?×4 (from smallest to largest)
1. ?×?×1 (from smallest to largest)

*Of course, whether this is the best order is open for a debate.*

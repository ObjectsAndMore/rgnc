# rgnc
Win32 region compiler

When creating non-rectangular windows in WinAPI, specifically irregularly shaped ones, you often have to deal with **regions** (to clip a windows to a well-defined shape). See https://learn.microsoft.com/en-us/windows/win32/gdi/about-regions.

The WinAPI provides several functions for creating regions (like **CreateRectRgn()**), which are relatively simple to use when working with regularly shaped regions (rectangles, ellipses, polygons).

Things get more complicated for irregular shapes, as the clipping region has to be assembled from elementary regions with the help of functions like **CombineRgn()** (https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-combinergn)

So, using a complex region shape boils down to coming up with a clever way of defining the shape as the union of simpler shapes. 

https://github.com/IvRogoz/WierdApps demonstrates an intuitive approach of specifying a clipping region graphically, by coloring a bitmap image with a pre-defined RGB color to indicate transparency. In this approach, the same bitmap serves both as the image to be displayed in the window and as the specification of the window's clipping region.

My idea was to take this idea a bit further and separate the region specification from the image to be displayed (or any arbitrary window for that matter). The command line tool **rgnc.exe** allows you to factor out the region definition into a stand-alone file, which may then be used independently for clipping any window. The region definition may either be loaded from a file at runtime or imported as a data resource at compile time (using a resource compiler). See the **examples** subdirectory for details.

Note that the window to be clipped will (probably) have to be at least as large as the clipping region. I have no idea what happens when clipping a region at least outside of the window's borders. Perhaps Windows handles that situation gracefully? Remains to be tested.

I have added an "**examine**" (**-e**) option to rgnc.exe to determine the dimensions of a given region (in a .rgn file) or image (in a .bmp file).

## Usage

```
Usage: rgnc.exe [options] <input_file>

Options:
  -h, --help                    Show this help message and exit
  -v, --verbose                 Enable verbose logging output
  -e, --examine                 Examine .rgn or .bmp file for region/image dimensions (overrides -o, disables any file output)
  -o <file>                     Specify the output file name - override the default name (".rgn") derived from input_file
  -t <rgb_r> <rgb_g> <rgb_b>    Specify the transparent color as RGB value, default (Magenta): 255 0 255
  ```
Note that any exiting output file of the same name will be overwritten without confirmation!
A **sample_image.bmp** is included.

## Examples

Create a .rgn file from a .bmp file (default naming of the output file and using the default transparency color cyan):
```
rgnc.exe sample_image.bmp
```

Create a .rgn file from a .bmp file (custom output file name, custom transparency color green, specified as RGB values 0/255/0):
```
rgnc.exe -t 0 255 0 -o sample_image_green.rgn sample_image.bmp
```

Examine an existing .rgn file and print the dimensions of the region's bounding rectangle (as a pair of (x,y) coordinates):
```
rgnc.exe -e sample_image.rgn
Bounding rectangle for the region in logical (x,y) units: (121,120)..(484,255)
```

Examine an existing .rgn file and print the dimensions of the region's bounding rectangle (as a pair of (x,y) coordinates):
```
rgnc.exe -e sample_image.rgn
Bounding rectangle for the region in logical (x,y) units: (121,120)..(484,255)
```

Examine an existing .bmp file and print the x and y dimensions of the image:
```
rgnc.exe -e sample_image.bmp
Bitmap size in logical (x,y) units: (596,330)
```



## How to build

```
build.bat
```

Building prerequisites on Windows 11 include:

- a C compiler, e.g. Winlibs/mingw64 (see https://winlibs.com/ and https://github.com/brechtsanders/winlibs_mingw)
- unpack winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64msvcrt-14.0.0-r3.zip locally (no admin privileges required)
- in a Windows cmd window, run winlibs\mingw64\mingwvars.bat to set up PATH and other variables. Then continue the build from that cmd window running **build.bat**


## Other notes

- the bitmap file should be a 24-bit or 32-bit BMP for predictable results
- when exporting to a bmp from Inkscape, make sure to select an "Antialias" setting of 0 to avoid oddly colored fringes around your clipping region
- When converting PNG files to BMP with imagemagick, force it to output the legacy BMP format (not the new BMP4 format) like so: ```magick img01.png BMP3:img01.bmp```

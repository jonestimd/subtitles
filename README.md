# Subtitles

Qt application for converting DVD subtitle images to .srt file.
Reads `sub.sub` output file from `vob2sub` and uses Tesseract
OCR library to extract text from the `.bmp` files.  Displays
each `.bmp` with a text editor for making corrections.

## Building

Set `CMAKE_PREFIX_PATH` based on install directory.  For example,
```sh
CMAKE_PREFIX_PATH=/opt/Qt/6.7.2/gcc_64/lib/cmake
```

Run cmake:
```sh
cmake -S . -B build
cmake --build build -v
```

## Running
May need to set `TESSDATA_DIR` to the location of the tesseract data files.
Defaults to `/usr/share/tesseract-ocr/5/tessdata`.

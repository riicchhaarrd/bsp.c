# bsp.c

![Build](https://github.com/riicchhaarrd/bsp.c/actions/workflows/cmake-multi-platform.yml/badge.svg)

A BSP parser and decompiler for Call of Duty 2 (IW2 engine). This tool reads compiled `.d3dbsp` files and can display detailed information about the BSP structure or decompile them back to editable `.map` format for use with CoD Radiant.

## Features


- **Display Information**: View detailed statistics about BSP lumps and contents
- **BSP Decompilation**: Export compiled BSP files back to `.map` format with:
  - Brush geometry reconstruction
  - Patch/terrain surface export
  - Entity data preservation
  - Portal reconstruction
  - Support for brush models (script_brushmodel, triggers)
  
## Usage

```
./bsp [options] <input_file>
```

### Options

- **`-info`**
  Print detailed information about the BSP file including lump sizes, entity counts, and statistics.

- **`-export`**
  Export the BSP file to .MAP format. If no export path is specified, the output will be saved to the input filename with `_exported` appended.
  Example: `map.d3dbsp` → `map_exported.map`

- **`-export_path <path>`**
  Specify a custom output path for the exported .MAP file. Requires `-export`.

- **`-exclude_patches`**
  Don't export patches (curved surfaces) to the .MAP file.

- **`-original_brush_portals`**
  Use the original portals stored in brushes instead of converting them to proper portal brushes (default behavior converts portals).

- **`-help`, `-?`, `-usage`**
  Display help message and exit.

### Examples

#### Display BSP information
```bash
./bsp -info mp_toujane.d3dbsp
```

#### Export BSP to MAP
```bash
./bsp -export mp_toujane.d3dbsp
# Creates: mp_toujane_exported.map
```

## Limitations

- **Game compatibility**: Currently only supports Call of Duty 2 BSP format (IBSP version 4)
- **Export Quality**: Exported .MAP files may not be identical to the original source due to compilation transformations
- **Patch Complexity**: Complex curved surfaces may not export perfectly
- **Texture Alignment**: UV coordinates and texture alignment may differ from the original
- **Entity Properties**: Some entity properties may be lost during BSP compilation and cannot be recovered


## Building

### Prerequisites

- CMake
- C compiler (GCC, Clang, MSVC)
- Git (for submodules)

### Linux / macOS

```bash
# Clone the repository with submodules
git clone --recurse-submodules https://github.com/riicchhaarrd/bsp.c
cd bsp.c

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make
```

### Windows (Native)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```


## Dependencies

This project uses the following third-party libraries (included as submodules):

- [linmath.h](https://github.com/datenwolf/linmath.h) - Lightweight 3D math library
- [growable-buf](https://github.com/skeeto/growable-buf) - Dynamic array implementation
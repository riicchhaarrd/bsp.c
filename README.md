# bsp.c
Prints information about a IW2 .d3dbsp file.
```
Usage: ./bsp [options] <input_file>

Options:
  -info                 Print information about the input file.
  -export            	Export the input file to a .MAP.
                        If no export path is provided, it will write to the input file with _exported appended.
                        Example: /path/to/your/bsp.d3dbsp will write to /path/to/your/bsp_exported.map

  -export_path <path> 	Specify the path where the export should be saved. Requires an argument.
  -help              	Display this help message and exit.

Arguments:
  <input_file>       	The input file to be processed.

Examples:
  ./bsp -info input_file.d3dbsp
  ./bsp -export -export_path /path/to/exported_file.map input_file.d3dbsp
```
![Build](https://github.com/riicchhaarrd/bsp.c/actions/workflows/cmake-multi-platform.yml/badge.svg)

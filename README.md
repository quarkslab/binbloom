# About

The purpose of this project is to analyse a raw binary firmware and determine automatically some of its features.
This tool is compatible with all architectures as basically, it just does simple statistics on it.

In order to compute the loading address, you will need the help of an external reverse engineering tool to extract a list of potential functions, before using binbloom.

Main features:

  * **Loading address**: binbloom can parse a raw binary firmware and determine its loading address.
  * **Endianness**: binbloom can use heuristics to determine the endianness of a firmware.
  * **UDS Database**: binbloom can parse a raw binary firmware and check if it contains an array containing UDS command IDs.


## Download / Install

First, clone the git repository:

```console
git clone https://github.com/quarkslab/binbloom.git
cd binbloom
```

To build the latest version:

```console
make
```

To install the latest version (linux only):

```console
make install
```

## Getting started

### Determine the endianness

```console
binbloom -f firmware.bin -e
```

This command should give an output like this:

```console
Loaded firmware.bin, size:624128, bit:fff00000, 000fffff, nb_segments:4096, shift:20
End address:00098600
Determining the endianness
Computing heuristics in big endian order:
Base: 00000000: unique pointers:1839, number of array elements:217900
Base: 01000000: unique pointers:1343, number of array elements:13085
Base: 02000000: unique pointers:621, number of array elements:5735
Base: 03000000: unique pointers:566, number of array elements:3823
Base: 05000000: unique pointers:575, number of array elements:6139
Base: 80000000: unique pointers:642, number of array elements:528
247210
Computing score in little endian order:
Base: 00000000: unique pointers:8309, number of array elements:515404
515404
This firmware seems to be LITTLE ENDIAN
```

In this output, the last line is the most important one as it gives the result of the analysis.
The other lines are information about the number of unique pointers and number of array elements binbloom has been able to find in the firmware, both in big endian and in little endian mode. These lines can provide useful information to corroborate the heuristic used to determine the endianness.

### Determine the loading address

First, you have to provide a file containing a list of potential functions addresses, in hexadecimal (one per line), like this:

```console
00000010
00000054
000005f0
00000a50
00000a54
00000ac0
00000b40
00000b6c
00000b74
00000bc0
```

This file should be named after the firmware itself, followed with the ".fun" extension.

This file can be generated with the tag_code() function of the provided tag_code.py python script, using IDA Pro:

- Load the firmware in IDA Pro at address 0 (select the correct architecture/endianness)
- From the File menu, choose Script File and select ```tag_code.py```
- In the console at the bottom of IDA Pro, use ```tag_code()```. The functions file is automatically generated.

If you prefer to use another tool to generate the functions file, you can do it as long as you load the firmware at address 0 (i.e. the hex values in the functions file correspond to offsets in the firmware).

You can then ask binbloom to compute a (list of) potential loading address(es) by computing a correlation score between the potential functions and the arrays of functions pointers that can be found in the firmware:

```console
binbloom -f firmware.bin -b
```

This command should give an output like this:

```console
Loaded firmware.bin, size:2668912, bit:ffc00000, 003fffff, nb_segments:1024, shift:22
End address:0028b970
loaded 14903 functions

Highest score for base address: 1545, for base address 80010000
For information, here are the best scores:
For base address 80010000, found 1545 functions
Saving function pointers for this base address...
Done.
```

In this output, we can see that on the 14903 provided potential functions, 1545 were found in function pointers arrays when the program takes the assumption that the loading address is 0x80010000.

If there are several sections in the binary firmware, binbloom lists the different sections with the corresponding guess for the loading address:

```
Highest score for base address: 93, for base address 00000000
For information, here are the best scores:
For base address 00000000, found 93 functions
For base address 00040000, found 93 functions
```

Here we have a section of code at address 0x00000000, and another one at 0x00040000.

Binbloom generates 2 output files:

- ```firmware.fad``` : this file contains the addresses of identified functions
- ```firmware.fpt``` : this file contains the addresses of the pointers to the identified functions


You can now start IDA Pro again (or any reverse engineering software), load the firmware at the specified address and import the addresses of the 1545 identified functions: 

- Load the firmware in IDA Pro at the specified address (in this example 0x80010000)
- From the File menu, choose Script File and select ```import_entry_points.py```
- Select the ```.fad``` file
- Select the ```.fpt``` file

*Note:*

binbloom will start by determining the endianness, as this information is needed to look for the arrays of functions pointers. If the automatic analysis of the endianness is wrong, you can override its result with the following option:

```-E b```: force big endian mode

```-E l```: force little endian mode

### Find the UDS database (for an ECU's firmware)

binbloom can try to search an array containing UDS/KWP2000 IDs, with the ```-u``` option:

```console
binbloom -f firmware.bin -u
```

This command should give an output like this:

```console
Loaded firmware.bin, size:1540096, bit:ffe00000, 001fffff, nb_segments:2048, shift:21
End address:00178000
UDS DB position: 1234 with a score of 12 and a stride of 12:
10 00 31 00 26 27 00 80 00 00 00 00 
11 00 31 00 24 3d 01 80 00 00 00 00 
22 00 10 00 2c 42 01 80 00 00 00 00 
27 00 10 00 1c 41 01 80 60 a8 01 80 
28 00 31 00 36 7f 01 80 00 00 00 00 
2e 00 10 00 18 88 01 80 08 ae 01 80 
31 00 30 00 10 41 01 80 00 00 00 00 
34 00 10 00 46 4e 01 80 00 00 00 00 
36 00 10 00 2a 2d 01 80 00 00 00 00 
37 00 10 00 32 3c 00 80 00 00 00 00 
3e 00 31 00 54 5b 01 80 00 b2 01 80 
85 00 31 00 6a 2f 01 80 00 00 00 00 
```

This output shows that at address 0x1234, a potential UDS database was found with a stride of 12 (meaning that UDS IDs are present in an array in which each element is 12-byte long).
In this example, the UDS IDs are in the first column (10, 11, 22, 27, 28, 2e, 31, 34, 36, 37, 3e and 85).

The list of supported UDS IDs is hard-coded in binbloom.c, you can change it if needed.

This analysis is based on heuristics so it can give false positives. You have to read the list of potential UDS databases found by binbloom and check and see which one is the correct one, if any.

In this example, we can see that there is a pointer in little endian in each line (26 27 00 80 for the first line, which corresponds to address 0x80002726). There is probably a function at this address to manage UDS command 10. You have to disassemble the code to make sure, and search for cross-references to this UDS database.

## About

### Authors

Guillaume Heilles ([@PapaZours](https://twitter.com/PapaZours))

### License

binbloom is provided under the [Apache 2.0 license](https://github.com/quarkslab/binbloom/blob/master/LICENSE).


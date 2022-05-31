# About

The purpose of this project is to analyse a raw binary firmware and determine automatically some of its features.
This tool is compatible with all architectures as basically, it just does simple statistics on it.

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

To build the latest version (linux only):

```console
autoreconf -i
./configure
make
sudo make install
```

## Getting started

### Determine the endianness and the base address of a firmware

```console
binbloom firmware.bin
```

This command should give an output like this:

```console
[i] 32-bit architecture selected.
[i] File read (20480 bytes)
[i] Endianness is LE                                
[i] 6 strings indexed                                    
[i] Found 3 base addresses to test                    
[i] Base address seems to be 0x60000000 (not sure)
 More base addresses to consider (just in case):
  0x005b5000 (0)
  0x0bcd0000 (0)
```

In this output, the third line displays the guessed endianness (*LE*, little-endian) and the sixth line gives the guessed
address (*0x60000000*). 6 text strings and 3 possible base addresses have been identified. If architecture is not specified,
32-bit architecture is considered by default.

The value in parenthesis after each candidate address is the corresponding score. The higher the score, the likelier
the address.

### Determine the endianness and the base address of a 64-bit firmware

```console
binbloom -a 64 firmware.bin
```

```console
[i] 64-bit architecture selected.
[i] File read (327680 bytes)
[i] Endianness is LE                                
[i] 717 strings indexed                                  
[i] Found 7535 base addresses to test                 
[i] Base address found: 0x0000000000010000.                          
 More base addresses to consider (just in case):
  0x000000000000e000 (276)
  0x000000000000f000 (242)
  0x0000000000011000 (175)
  0x000000000000d000 (167)
  0x000000000000b000 (121)
  0x0000000000013000 (107)
  0x0000000000012000 (100)
  [...]
```

The `-a` option tells binbloom to consider a 64-bit firmware, the above output shows a guessed base address of 0x10000.

### Force the endianness if binbloom does not get it right

When dealing with small firmwares (size < 10 Kbytes) binbloom endianness detection may not be reliable and give a false
result that leads to unexpected base addresses. In this case, you can use the `-e` option to specify the endianness:

```console
binbloom -e be firmware.bin
```

It then produces the following output:

```console
[i] Selected big-endian architecture.
[i] File read (1048576 bytes)
[i] Endianness is BE
[i] 764 strings indexed                                  
[i] Found 18615 base addresses to test                
[i] Base address seems to be 0x00000000 (not sure).
 More base addresses to consider (just in case):
  0x3f740000 (121043)
  0x7ff48000 (61345)
  0x41140000 (59552)
  [...]
```

Endianness is then forced (in this case big-endian) and binbloom relies on this configuration to guess the base address.


### Find the UDS database (for an ECU's firmware)

```console
binbloom -a 32 -e be -b 0x0 firmware.bin
```

```console
[i] 32-bit architecture selected.
[i] Selected big-endian architecture.
[i] Base address 0x0000000000000000 provided.
[i] 764 strings indexed                                  
Most probable UDS DB is located at @000ee8c8, found 7 different UDS RID
Identified structure:
struct {
	code *p_field_0;
	code *p_field_1;
	uint32_t dw_2;
}
```

This analysis is based on heuristics so it can give false positives. You have to read the list of potential UDS databases found by binbloom and check and see which one is the correct one, if any. Binbloom provides the identified structure in its output, allowing some disassemblers
to parse the memory following the structure declaration.

## Advanced options

You can speed up the base address lookup process by enabling multi-threading with the `-t` option. By default, a single thread is used.

```console
binbloom -t 8 firmware.bin
```

A *deep search mode*, enable with the `-d` option, is also implemented but is still experimental. This mode may be useful in very rare occasions as it may
find a valid base address when nothing else works, but it is a slower mode that may take some time to complete.

If you want the tool to display more information, use one or more `-v` options.

## About

### Authors

* Guillaume Heilles ([@PapaZours](https://twitter.com/PapaZours))
* Damien Cauquil ([@virtualabs](https://twitter.com/virtualabs))

### License

binbloom is provided under the [Apache 2.0 license](https://github.com/quarkslab/binbloom/blob/master/LICENSE).


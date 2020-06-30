/* Copyright 2020 G. Heilles
 * Copyright 2020 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#define CHUNK_SIZE 4096
#define UDS_SIZE sizeof(UDS)
#define CANDB_SIZE 512

#define READ32LE(i) ((firmware[(i)+3]<<24)|(firmware[(i)+2]<<16)|(firmware[(i)+1]<<8)|(firmware[i]))
#define READ32BE(i) ((firmware[i]<<24)|(firmware[(i)+1]<<16)|(firmware[(i)+2]<<8)|(firmware[(i)+3]))

typedef struct {
    uint32_t la;
    uint32_t fptr;
    uint32_t ptr;
} fptr_t;

typedef struct {
    uint32_t la;
    uint32_t score;
} score_t;

unsigned char *firmware;
unsigned int *function_address;
unsigned int *is_function;
unsigned int size;
unsigned int nb_functions;
unsigned int main_segment;
score_t *score_base_address;
uint32_t g_loading_address = 0xFFFFFFFF;
char filename[1024] = "";
char func_name[1024] = "";
char func_ptr_name[1024] = "";
char ptr_name[1024] = "";
fptr_t *funcptrs;
unsigned int funcptrs_size;
unsigned int funcptrs_index = 0;
unsigned int score_base_address_index = 0;
unsigned int score_base_address_size;
unsigned int max_score_la = 0;
unsigned int max_score_base_address = 0;

int flag_compute_base_address = 0;
int flag_compute_UDS_DB = 0;
int flag_compute_endianness = 0;
int flag_override_endianness = 0;
int flag_override_bigendian = 0;
int flag_override_littleendian = 0;
int flag_verbose = 0;
int threshold = 4;

unsigned char UDS[] = {
    0x10, 0x11, 0x27, 0x28, 0x3E, 0x83, 0x84, 0x85, 0x86, 0x87, 0x22, 0x23, 0x24, 0x2A, 0x2C, 0x2D, 0x2E, 0x3D, 0x14,
    0x19, 0x2F, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x18, 0x3B, 0x20, 0x21, 0x1A
};

unsigned char UDS_local[UDS_SIZE];
unsigned int *score;
int *refcount;
int *popcount;
int *segment;
unsigned int nb_segments, shift;

FILE *fp;
FILE *fp2;

typedef enum {
    B_BIG_ENDIAN,
    B_LITTLE_ENDIAN
} t_endian;

t_endian endianness = B_BIG_ENDIAN;

int flag = 0;
unsigned int base_address = 0;
unsigned int end_address, mask_segment, mask_pointer;
int offset = 0;

uint32_t read32(uint32_t x) {
    if (endianness == B_BIG_ENDIAN) {
        return READ32BE(x);
    } else {
        return READ32LE(x);
    }
}

uint32_t p2(uint32_t x) {
    return 1 << (32 - __builtin_clz(x - 1));
}

unsigned int read_file(char *name, unsigned char **mem) {
    int fd, nb;
    struct stat buf;
    unsigned char *ptr;

    fd = open(name, O_RDONLY);
    if (fd == -1) {
        printf("Can not open input file %s\n", name);
        exit(-1);
    }
    assert(0 == fstat(fd, &buf));
    *mem = malloc(buf.st_size + 1024);
    assert(mem != NULL);
    ptr = *mem;
    while ((nb = read(fd, ptr, CHUNK_SIZE)))
        ptr += nb;
    close(fd);

    return buf.st_size;
}

int is_unique_UDS(unsigned int position) {
    unsigned int j, flag;
    flag = 0;
    for (j = 0; j < UDS_SIZE; j++) {
        if (firmware[position] == UDS[j]) {
            flag = 1;
            break;
        }
    }

    if (flag == 1) {
        for (j = position - CANDB_SIZE / 2; j < position + CANDB_SIZE / 2; j++) {
            if ((firmware[j] == firmware[position]) && (j != position)) {
                flag = 0;
                break;
            }
        }
    }
    return flag;
}

void locate_can_db(void) {
    unsigned int i, j, k, max_score, stride;
    unsigned int candb_position = 0;
    unsigned int *stride_score;

    score = (unsigned int *)calloc(size, sizeof(int));
    assert(score);
    stride_score = (unsigned int *)calloc(size, sizeof(int));
    assert(stride_score);

    for (i = 1; i < size - CANDB_SIZE * 2; i++) {
        for (stride = 6; stride <= 32; stride += 2) {
            // prepare a local copy of the UDS service list, to remove them once each is found. This will ensure each UDS service is matched only once.
            memcpy(UDS_local, UDS, UDS_SIZE);
            max_score = 0;
            for (k = 0; k < CANDB_SIZE; k += stride) {
                int flag = 0;
                for (j = 0; j < UDS_SIZE; j++) {
                    // printf("j=%d, k=%d ", j, k);
                    if ((firmware[i + k] == UDS_local[j]) && (UDS_local[j] != 0)) {
                        //printf("at %#08x + %#08x, firmware=%#02x (j=%d, stride=%d)\n", i, k, firmware[i+k], j, stride);
                        max_score++;
                        UDS_local[j] = 0;
                        flag = 1;
                    }
                }
                if (flag == 0)
                    break;
            }
            //  if (max_score) printf("Score at %#08x for stride %d: %d\n", i, stride, max_score);
            if (max_score > score[i]) {
                // printf("New high score for %#08x: %d\n", i, max_score);
                score[i] = max_score;
                stride_score[i] = stride;
            }
        }
    }

    // print 20 best candidates
    for (j = 0; j < 20; j++) {
        max_score = 0;
        unsigned int max_stride = 0;
        for (i = 0; i < size; i++) {
            if (score[i] > max_score) {
                max_score = score[i];
                max_stride = stride_score[i];
                candb_position = i;
            }
        }
        printf("UDS DB position: %x with a score of %d and a stride of %d:\n", candb_position, max_score, max_stride);
        for (unsigned int k = candb_position; k < candb_position + max_score * max_stride; k++) score[k] = 0; // skip all other references to this same candb
        for (i = 0; i < max_score; i++) {
            for (j = 0; j < max_stride; j++) {
                printf("%02x ", firmware[candb_position + i * max_stride + j]);
            }
            printf("\n");
        }
        printf("\n");
        score[candb_position] = 0;
    }
    free(score);
    free(stride_score);
}

int is_pointer(unsigned int base, unsigned int p) {
    unsigned int lowp;
    int res = 0;
    if (p == 0) return 0;
    if (p == 0xFFFFFFFF) return 0;
    lowp = p & mask_pointer;
    if (p >> shift == base) {
        if (lowp < size) {
            res = 1;
        }
    }
    return res;
}

int count_array_elements(int base) {
    int count = 0;
    int total_count = 0;
    int stride;
    uint32_t last_pointer = 0;

//    printf("Checking score for base %x, shift=%d, mask_pointer=%x\n", base, shift, mask_pointer);
    for (stride = 4; stride < 16; stride += 2) {
        for (unsigned int i = 0; i < size; i += 2) {
            unsigned int p = read32(i);
            unsigned int j = i;
            count = 0;
            while (is_pointer(base, p)) {
//                printf("%x is pointer, count = %d\n", p, count);
                if (p == last_pointer) break; // do not take into account arrays with constant values
                count++;
                j += stride;
                if (j > size) break;
                last_pointer = p;
                p = read32(j);
            }
            last_pointer = 0;       // reset for the next search
            if (count > 3) {        // take into account the tables of at least 4 consecutive pointers to the base address
                total_count += count;
            }
        }
    }

    return total_count;
}


int count_segments(void) {
    unsigned int base;
    int count = 0;
    unsigned int i;
    int max = 0;
    for (base = 0; base < nb_segments; base++) {
        memset(refcount, 0, size * sizeof(int));
        for (i = 0; i < size - 4; i += 2) {
            unsigned int p = 0;
            unsigned int lowp;
            p = read32(i);
            lowp = p & mask_pointer;
            if (p >> shift == base) {
                if (lowp < size) {
                    refcount[lowp]++;   // increment the refcount of each pointer
                }
            }
        }

        // now we count the number of unique pointers (which refcount is != 0)
        count = 0;
        for (i = 0; i < size - 4; i += 2) {
            if (refcount[i]) {
                count++;
            }
        }
        segment[base] = count;  // nb of unique pointers
        if (count > max)
            max = count;
    }
    return max;
}

void check_pointer(void) {
    unsigned int base;
    int max = 0;
    int total_score_be = 0;
    int total_score_le = 0;
    int s;

    printf("Determining the endianness\n");
    /*
       // This is a previous attempt, kept here for history in case you want to try it yourself.

       // count all pointers to each segment, including all occurrences of the same pointer.
       // scan the firmware
       for (i=0; i<size-4; i+=2) {
       unsigned int p = 0;
       // read 4 bytes in the firmware -> in p
       for (j = 0; j < 4; j++) {
       p = p << 8 | firmware[i+3-j];
       }
       int s = p>>shift;    // compute the base address (segment) of the pointer
       segment[s]++;                // increment its refcount
       }
     */
    // the previous approach counts each occurrence of the same pointer. When a pointer is referenced several times, this is most probably
    // NOT a pointer. Thus, in this version, we only count unique pointers.

    endianness = B_BIG_ENDIAN;
    printf("Computing heuristics in big endian order:\n");
    max = count_segments();
    for (base = 0; base < nb_segments; base++) {
        if (segment[base] > max / threshold) {
            s = count_array_elements(base);
            printf("Base: %08x: unique pointers:%d, number of array elements:%d\n", base << shift, segment[base], s);
            total_score_be += s;
        }
    }
    printf("%d\n", total_score_be);
    endianness = B_LITTLE_ENDIAN;
    printf("Computing score in little endian order:\n");
    max = count_segments();
    for (base = 0; base < nb_segments; base++) {
        if (segment[base] > max / threshold) {
            s = count_array_elements(base);
            printf("Base: %08x: unique pointers:%d, number of array elements:%d\n", base << shift, segment[base], s);
            total_score_le += s;
        }
    }
    printf("%d\n", total_score_le);

    if (total_score_le > total_score_be + total_score_be / 2) {
        endianness = B_LITTLE_ENDIAN;
        printf("This firmware seems to be LITTLE ENDIAN\n");
    } else if (total_score_be > total_score_le + total_score_le / 2) {
        endianness = B_BIG_ENDIAN;
        printf("This firmware seems to be BIG ENDIAN\n");
    } else {
        printf("Scores for Big endian and Little endian are too close to determine the endianness.\n");
    }

}

int find_score_base_address(uint32_t la) {
    for (unsigned int i = 0; i < score_base_address_index; i++) {
        if (score_base_address[i].la == la) {
            return i;
        }
    }
    return -1;
}

void count_pointers(unsigned int stride, unsigned int address) {
    unsigned int i;
    uint32_t loading_address;
    unsigned int ptr, count, last_ptr, imax, countmax = 0;
    for (i = 0; i < nb_functions; i++) {
        ptr = (read32(address)) & 0xFFFFFFFE;
        if (ptr == 0) return;
        if (ptr == 0xFFFFFFFE) return;
        loading_address = ptr - function_address[i];
//        printf("At %08x, Ptr:%x, function(%d):%08x, Loading address:%08x\n", address, ptr, i, function_address[i], loading_address);
        if (ptr < function_address[i]) continue;
        count = 0;
        do {
            last_ptr = ptr;
            ptr = (read32(address + stride * (count + 1))) & 0xFFFFFFFE;
//            printf("Next (%d): %x\n", count, ptr);
            count++;
            if (ptr == 0) break;
            if (ptr == 0xFFFFFFFE) break;
        } while ((ptr != last_ptr) && (ptr - loading_address < size) && (is_function[ptr - loading_address] == 1));
        if (count > countmax) {
            countmax = count;
            imax = i;
        }
    }
    if (countmax > 4) {
        ptr = (read32(address)) & 0xFFFFFFFE;
        loading_address = ptr - function_address[imax];
        int j = find_score_base_address(loading_address);
        if (j == -1) {
            score_base_address[score_base_address_index].la = loading_address;
            score_base_address[score_base_address_index].score = 1;
            score_base_address_index++;
            if (score_base_address_index == score_base_address_size) {
                score_base_address_size *= 2;
                score_base_address = realloc(score_base_address, score_base_address_size);
            }
        } else {
            score_base_address[j].score++;
            if (score_base_address[j].score > max_score_base_address) {
                max_score_base_address = score_base_address[j].score;
                max_score_la = loading_address;
            }
        }

        // store all function pointers in the array for this loading address
        count = 0;
        do {
            funcptrs[funcptrs_index].la = loading_address;
            funcptrs[funcptrs_index].fptr = ptr;
            funcptrs[funcptrs_index].ptr = address + stride * count;
            funcptrs_index++;
            if (funcptrs_index > funcptrs_size) {
                funcptrs_size *= 2;
                funcptrs = realloc(funcptrs, funcptrs_size);
            }
            ptr = (read32(address + stride * (count + 1))) & 0xFFFFFFFE;
            count++;
        } while (count != countmax);
    }
}

void get_pointer_array(void) {
    unsigned long k;
    if (g_loading_address == 0xFFFFFFFF) {
        for (int s = 4; s <= 16; s += 2) {
            //printf("Scanning with stride %d\n", s);
            for (unsigned int i = 0; i < size; i += 2) {
                count_pointers(s, i);
            }
        }
        // display the loading addresses which score is above a threshold
        printf("Best scores for the loading address:\n");
        for (k = 0; k < score_base_address_index; k++) {
            if (score_base_address[k].score > max_score_base_address / 2) {
                printf("Base address:%08x, score:%d\n", score_base_address[k].la, score_base_address[k].score);
            }
        }

        g_loading_address = max_score_la;
        printf("\nBest loading address: %08x\n", g_loading_address);
    }
    strcpy(func_ptr_name, filename);
    strcat(func_ptr_name, ".fad");
    strcpy(ptr_name, filename);
    strcat(ptr_name, ".fpt");
    fp = fopen(func_ptr_name, "w");
    assert(fp);
    fp2 = fopen(ptr_name, "w");
    assert(fp2);

    printf("Saving function pointers for this base address...\n");
    for (k = 0; k < funcptrs_index; k++) {
        if (funcptrs[k].la == g_loading_address) {
            fprintf(fp, "%08x\n", funcptrs[k].fptr);
            fprintf(fp2, "%08x\n", g_loading_address + funcptrs[k].ptr);
        }
    }

    printf("Done.\n");
    fclose(fp);
    fclose(fp2);
}

void load_functions(void) {

    strcpy(func_name, filename);
    strcat(func_name, ".fun");

    FILE *fp = fopen(func_name, "r");
    if (!fp) {
        printf("functions file is missing.\n");
        exit(-1);
    }

    unsigned int address;
    int ret, i = 0;

    ret = fscanf(fp, "%x", &address);
    while (ret == 1) {
        if (address > size) {
            printf("The functions file has not been generated with a 0 base address. You should do that.\n");
            printf("i:%d, address:%08x, size: %08x\n", i, address, size);
            exit(-1);
        }
//        printf("loaded %x\n", address);
        is_function[address] = 1;
        function_address[i] = address;
        if ((i > 1) && (function_address[i] - function_address[i - 1] <= 8)) { // skip short functions (<=8 bytes)
            function_address[i - 1] = function_address[i];
            i--;
        }
        /*        if ((i>2)&&(function_address[i] - function_address[i-1] == function_address[i-1] - function_address[i-2])) { // skip constant length functions (to avoid false positives with arrays of data)
                    function_address[i-1] = function_address[i];
                    i--;
                }
        */
        ret = fscanf(fp, "%x", &address);
        i++;
    }
    nb_functions = i;
    fclose(fp);
    printf("loaded %d functions\n", nb_functions);
}

void usage(char *progname) {
    printf(
        "Usage: %s -f firmware.bin [-b] [-B base_address] [-e] [-E <b|l>] [-u] [-v]\n"
        "         -f firmware.bin    firmware to analyse (mandatory)\n"
        "         -u                 search a UDS database\n"
        "         -e                 infer the endianness\n"
        "         -E                 override the endianness computation. -E b for big endian, -E l for little endian\n"
        "         -b                 infer the base address (need a list of functions in a \"functions\" file, in hex. See README.txt for instructions)\n"
        "         -B 0xaaaaaaaa      override the base address. To be used with -b\n"
        "         -v                 verbose: display more results\n"
        "\n\nExamples:\n"
        "    binbloom -f firmware.bin -u : compute the address of the UDS database\n"
        "    binbloom -f firmware.bin -e : compute the endianness\n"
        "    binbloom -f firmware.bin -b : compute the base address, computes the endianness first\n"
        "    binbloom -f firmware.bin -b -E <b|l> : compute the base address and specify the endianness, in case the automatic detection of the endianness is wrong, then write the .fad and .fpt files\n"
        "    binbloom -f firmware.bin -B <base_address> [-E <b|l>] : write the .fad and .fpt files. Overrire the computation of the base address, and of the endianness if -E is specified\n"
    , progname);

    exit(-1);
}

int main(int argc, char **argv) {
    int c;

    if (argc == 1) {
        usage(argv[0]);
    }

    while ((c = getopt(argc, argv, "hf:bB:euvE:")) != -1)
        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'f':
                strcpy(filename, optarg);
                size = read_file(filename, &firmware);
                break;
            case 'b':
                flag_compute_base_address = 1;
                break;
            case 'v':
                flag_verbose = 1;
                break;
            case 'u':
                flag_compute_UDS_DB = 1;
                break;
            case 'e':
                flag_compute_endianness = 1;
                break;
            case 'E':
                flag_override_endianness = 1;
                if (optarg[0] == 'b') flag_override_bigendian = 1;
                if (optarg[0] == 'l') flag_override_littleendian = 1;
                break;
            case 'B':
                if (1 != sscanf(optarg, "%x", &g_loading_address)) {
                    printf("Can not read the loading address. It should be in hex : -B 0x1234\n");
                    exit(-1);
                } else {
                    printf("Overriding the loading address: %08x\n", g_loading_address);
                }
                break;
            default:
                printf("Invalid argument.\n\n");
                usage(argv[0]);
                exit(1);
        }



    mask_segment = ~(p2(size) - 1);
    mask_pointer = p2(size) - 1;
    nb_segments = mask_segment;
    while ((nb_segments & 1) == 0)
        nb_segments = nb_segments >> 1;
    nb_segments++;
    shift = __builtin_clz(nb_segments - 1);
    printf("Loaded %s, size:%d, bit:%08x, %08x, nb_segments:%d, shift:%d\n", filename, size, mask_segment, mask_pointer,
           nb_segments, shift);

    refcount = (int *)calloc(size, sizeof(int));
    assert(refcount);
    popcount = (int *)calloc(nb_segments, sizeof(int));
    assert(popcount);

    segment = (int *)calloc(nb_segments, sizeof(int));
    assert(segment);

    is_function = (unsigned int *)calloc(size, sizeof(int));
    assert(is_function);

    function_address = (unsigned int *)calloc(size, sizeof(int));
    assert(function_address);

    score_base_address_size = size;
    score_base_address = (score_t *)calloc(score_base_address_size, sizeof(score_t));
    assert(score_base_address);

    funcptrs = (fptr_t *)calloc(size, sizeof(fptr_t *));
    funcptrs_size = size;

    end_address = base_address + size;
    printf("End address:%08x\n", end_address);

    if (flag_verbose == 1) {
        threshold = 10;
    }
    // can not compute the base address without finding out the endianness.
    // Force computation of the endianness if needed
    if ((flag_compute_base_address == 1) && (flag_compute_endianness == 0))
        flag_compute_endianness = 1;

    if (flag_compute_endianness == 1) {
        if (flag_override_endianness == 1) {
            if (flag_override_bigendian == 1) endianness = B_BIG_ENDIAN;
            if (flag_override_littleendian == 1) endianness = B_LITTLE_ENDIAN;
        } else {
            check_pointer();
        }
    }

    if (flag_compute_UDS_DB == 1) {
        locate_can_db();
    }

    if (flag_compute_base_address == 1) {
        load_functions();
        get_pointer_array();
    }

    free(firmware);
    free(refcount);
    free(popcount);
    free(segment);
    free(score_base_address);

    return 0;

}

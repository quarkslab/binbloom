#include "helpers.h"

pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;


/**
 * @brief   Check if an address (pointer) is made of ASCII characters
 * @param   u64_address     address
 * @param   arch            target architecture
 * @return  1 if address contains only ASCII, 0 otherwise
 **/

int is_ascii_ptr(uint64_t u64_address, arch_t arch)
{
    int i;

    if (arch == ARCH_32)
    {
        for (i=0;i<4;i++)
        {
            if ( ((u64_address&0xff) < 0x20) || ((u64_address&0xff) > 0x7f))
                return 0;
            u64_address = u64_address>>8;
        }
    }
    else
    {
        for (i=0;i<8;i++)
        {
            if ( ((u64_address&0xff) < 0x20) || ((u64_address&0xff) > 0x7f))
                return 0;
            u64_address = u64_address>>8;
        }
    }

    return 1;
}


/**
 * @brief   Get pointer size depending on the architecture (32 or 64 bit)
 * @param   arch    target architecture
 * @return  size of pointer
 **/

int get_arch_pointer_size(arch_t arch)
{
    return (arch==ARCH_32)?4:8;
}


/**
 * @brief   Read pointer from file content at given offset
 * @param   arch        target architecture
 * @param   endian      target endianness
 * @param   p_content   pointer to firmware content
 * @param   offset      offset to read
 **/

uint64_t read_pointer(arch_t arch, endianness_t endian, unsigned char *p_content, unsigned int offset)
{
    uint64_t v;

    /* Read pointer from content. */
    if (arch == ARCH_32)
    {
        v = (uint64_t)(*(uint32_t *)&p_content[offset]);
    }
    else
    {
        v = (uint64_t)(*(uint64_t *)&p_content[offset]);
    }

    if (endian == ENDIAN_BE)
    {
        if (arch == ARCH_32)
        {
            v = (uint64_t)(BSWAP32((uint32_t)v));
        }
        else
        {
            v = BSWAP64(v);
        }
    }

    return v;
}


/**
 * @brief   Compute history (used for entropy)
 * @param   S       pointer to source data
 * @param   hist    pointer to histogram data
 * @param   len     size of S
 * @return  number of different values seen
 **/

int makehist(unsigned char *S, unsigned int *hist,int len){
	int wherechar[256];
	int i,histlen;
	histlen=0;
	for(i=0;i<256;i++)wherechar[i]=-1;
	for(i=0;i<len;i++){
		if(wherechar[(int)S[i]]==-1){
			wherechar[(int)S[i]]=histlen;
			histlen++;
		}
		hist[wherechar[(int)S[i]]]++;
	}
	return histlen;
}
 
/**
 * @brief   Compute Shannon entropy
 * @param   p_data      data to analyze
 * @param   size        data size
 **/

double entropy(unsigned char *p_data, int size)
{
	int i, histlen;
	double H;
    unsigned int history[256];
    int lookup[256];
    unsigned char c;

    /* Initialize structures. */
    for (i=0; i<256; i++)
    {
        lookup[i] = -1;
        history[i] = 0;
    }

    /* Parse data. */
    histlen = 0;
    for (i=0; i<size; i++)
    {
        c = p_data[i];
        if (lookup[c] < 0)
        {
            lookup[c] = histlen++;
        }
        history[lookup[c]]++;
    }
    
    H=0;
	for(i=0;i<histlen;i++){
		H-=(double)history[i]/size*log2((double)history[i]/size);
	}

    return H/8.0;

}


/**
 * @brief   Displays/update a progress bar
 * @param   current     Current value
 * @param   max         Maximum value
 * @param   desc        Description string
 **/

void progress_bar(uint64_t current, uint64_t max, char *desc)
{
    uint64_t percent, curpos, i;
    struct winsize w;

    /* Get terminal size. */
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    percent = (current*100)/max;

    if (w.ws_col >= 40)
    {
        pthread_mutex_lock(&io_lock);

        /* If we have at least 30 characters, display a progress bar. */
        printf("\r%-30s", desc);
        printf("[");
        curpos = (current * (w.ws_col - 40))/max;
        for (i=0; i<w.ws_col - 40; i++)
        {
            if (i<=curpos)
                printf("=");
            else
                printf(" ");
        }
        printf("] %3d%%   ", percent);
        pthread_mutex_unlock(&io_lock);
    }
    else
    {
        pthread_mutex_lock(&io_lock);
        printf("\r%-30s [%3d%%]", desc, percent);
        pthread_mutex_unlock(&io_lock);
    }   
    fflush(stdout);
}


/**
 * @brief   Clear progress bar from screen
 **/

void progress_bar_done(void)
{
    int i;
    struct winsize w;

    /* Get terminal size. */
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    /* Clear line. */
    pthread_mutex_lock(&io_lock);
    printf("\r");
    for (i=0; i<w.ws_col; i++)
    {
        printf(" ");
    }
    printf("\r");
    fflush(stdout);
    pthread_mutex_unlock(&io_lock);
}

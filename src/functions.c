#include "functions.h"

/**
 * @brief   Load a list of functions addresses from a file.
 *
 * Read a list of point of interest from a file. Each line must follow this
 * pattern: <Address in hex> <POI name>
 *
 * @param   psz_file        Path to file
 * @param   p_poi_list      pointer to a list of POI to fill
 **/

void read_poi_from_file(char *psz_file, poi_t *p_poi_list)
{
    FILE *poi_file;
    int i, sol, eol, eov, eof = 0;
    int nb_bytes_read;
    unsigned char line_buffer[4096];
    int line_len = 0;
    uint64_t symbol;
    int nb_symbols = 0;

    /* Open file. */
    poi_file = fopen(psz_file, "rb");
    if (poi_file != NULL)
    {
        /* Parse file content. */
        line_len = 0;
        while (line_len >= 0)
        {
            eov = -1;
            eol = -1;
            sol = -1;

            /* Read 4096 bytes from file and fill our line buffer. */
            if (!feof(poi_file))
            {
                nb_bytes_read = fread(&line_buffer[line_len], 1, 4096 - line_len, poi_file);
                line_len += nb_bytes_read;
                debug("Read %d bytes from symbols file\n", nb_bytes_read);
            }
            else
                eof = 1;

            /* Line is supposed to start with '0x', if not just chop content until we meet a '0x'. */
            if ((line_buffer[0] == '0') && (line_buffer[1] == 'x'))
            {
                //debug("Line starts with '0x', sol=0\n");
                sol = 0;
                for (i=0; i<line_len; i++)
                {
                    if (line_buffer[i] == '\n')
                    {
                        eol = i;
                        break;
                    }
                    if (line_buffer[i] == ' ')
                    {
                        eov = i;
                    }
                }
                //debug("Line ends at offset eol=%d\n", eol);
            }
            else
            {
                for (i=0; i<(line_len-1); i++)
                {
                    if ((line_buffer[i] == '0') && (line_buffer[i+1] == 'x'))
                    {
                        sol = i;
                    } else if (sol > 0)
                    {
                        if ((line_buffer[i] == ' ') && (eov<0))
                            eov = i;
                        else if (line_buffer[i] == '\n')
                        {
                            eol = i;
                            break;
                        }
                    }
                }
            }


            if ((eol>0) && (sol>=0) && (eov > 1))
            {
                /* Found EOL, parse value */
                line_buffer[eov] = '\0';
                symbol = strtol((char *)&line_buffer[sol+2], NULL, 16);
                debug("Loaded symbol 0x%016x\n", symbol);
                poi_add_unique(p_poi_list, symbol, 1, POI_FUNCTION);
                nb_symbols++;
            }
            
            /* If no start of line, forget the buffer. */
            if (sol < 0)
            {
                line_len = 0;
            }
            else if (eol < 0) /* Got a start of line but still waiting end of line */
            {
                /* Reached end of file ? Stop here. */
                if (eof)
                    break;

                /* Move start of line to the beginning of the buffer. */
                for (i=sol; i<line_len; i++)
                    line_buffer[i-sol] = line_buffer[i];
                line_len -= sol;
            }
            else if (eol >= 0)
            {
                /* Move start of line to the beginning of the buffer. */
                for (i=(eol+1); i<line_len; i++)
                    line_buffer[i-(eol+1)] = line_buffer[i];
                line_len -= (eol+1);
            }
            if (line_len == 0)
                break;
        }
    }

    fclose(poi_file);
    debug("Loaded %d symbols !\n", nb_symbols);
}
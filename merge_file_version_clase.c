#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define true 1
#define false 0

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define MAX_FILES_N 16
#define MIN_INPUT_FILES 1

#define ERROR -1
#define ERR_OPEN -2
#define ERR_MALLOC -3
#define ERR_WRITE -4

#define MIN_SIZE_BUFFER 1
#define MAX_SIZE_BUFFER 128*1024*1024

#define HELP_MSG "Uso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE\tTamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-o FILEOUT\tUsa FILEOUT en lugar de la salida estandar\n"

/*
    Merge multiple files into one file.
    Input: ./merge_files -t 16 -o salida entrada1 entrada2 entrada3 ... entradan

    -t: buffer size
    -o: output file

*/

int errflag;

typedef struct file
{
    int fd;
    char * buffer;
    unsigned int length;
    unsigned int offset;

} file_t;

int atomicWrite(int fd, char *buffer, unsigned int length)
{
    unsigned int written = 0;
    int ret;

    while (written < length)
    {
        ret = write(fd, buffer + written, length - written);
        if (ret == -1)
        {
            perror("write()");
            errflag = ERR_WRITE;
            return ERROR;
        }
        written += ret;
    }

    return 0;
}


int main(int argc, char *argv[])
{

    // Get the input
    int opt;
    int buffer_size = 1024;
    int N_files = 0;
    char *output = NULL;
    file_t *files = NULL;

    while ((opt = getopt(argc, argv, "t:o:h")) != -1)
    {
        switch (opt)
        {
        case 'h':
            fprintf(stdout, HELP_MSG);
            exit(EXIT_SUCCESS);
            break;
        case 't':
            buffer_size = atoi(optarg);
            break;
        case 'o':
            output = optarg;
            break;
        }
    }

    N_files = argc - optind;

    if (buffer_size < MIN_SIZE_BUFFER || buffer_size > MAX_SIZE_BUFFER)
    {
        fprintf(stderr, "Error: Tamaño de buffer incorrecto.\n");
        fprintf(stderr, HELP_MSG);
        exit(EXIT_FAILURE);
    }
    
    if (N_files < MIN_INPUT_FILES)
    {
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        fprintf(stderr, HELP_MSG);
        exit(EXIT_FAILURE);
    } // if there are not enough input files available then exit now

    if (N_files > MAX_FILES_N)
    {
        fprintf(stderr, "Error: Demasiados ficheros de entrada. Máximo 16 ficheros.\n");
        fprintf(stderr, HELP_MSG);
        exit(EXIT_FAILURE);
    } // if there are too many input files available then exit now

    if ((files = malloc(N_files * sizeof(file_t))) == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int n_open_file_errors = 0;
    for (int i = optind; i < argc; i++)
    {

        int fdin = open(argv[i], O_RDONLY);
        if (fdin == -1)
        {
            fprintf(stderr, "Aviso: No se puede abrir '%s': No such file or directory\n", argv[i]);
            n_open_file_errors++;
            continue;
        }

        file_t file;
        file.fd = fdin;
        file.buffer = malloc(buffer_size * sizeof(char));
        if (file.buffer == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        };
        file.length = 0;
        file.offset = 0;

        files[i - optind - n_open_file_errors] = file;
    }

    N_files -= n_open_file_errors;
    // Realloc files
    if (N_files > 0)
        files = realloc(files, N_files * sizeof(file_t)); // Since we have removed some files from the array we need to realloc it

    if (files == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    // Change the stdout to the output file if it is specified
    if (output != NULL)
    {
        // Use dup2 to change the stdout to the output file
        int fdout = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fdout == -1)
        {
            perror("open(fdout)");
            exit(EXIT_FAILURE);
        }

        if( fdout != STDOUT_FILENO )
        {
            if (dup2(fdout, STDOUT_FILENO) == -1)
            {
                perror("dup2(fdout)");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Allocate the buffer
    char * buffer = malloc( buffer_size * sizeof(char) );

    if (buffer == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Merge files
    int files_left = N_files;
    int index = 0;
    int offset = 0;

    int ret;

    while (files_left)
    {

        if (files[index].fd == -1)
        {
            index = (index + 1) % N_files;
            continue;
        }; // Skip the files that are already finished


        if (files[index].length == files[index].offset)
        {
            ret = read(files[index].fd, files[index].buffer, buffer_size);

            if (ret == -1)
            {
                perror("read()");
                exit(EXIT_FAILURE);
            } else if (ret == 0)
            {
                close(files[index].fd);
                files[index].fd = -1;
                files_left--;
                continue;
            }

            files[index].length = ret;
            files[index].offset = 0;
        }

        //offset can not exceed buffer_size, and also we cannot read more than the remaining size of the file buffer 
        int maximo = min( buffer_size, offset+(files[index].length-files[index].offset) ); 

        while( offset < maximo ){
        
            buffer[offset++] = files[index].buffer[files[index].offset++]; //update offsets after copying

            if( buffer[offset-1] == '\n'){
                index = (index + 1) % N_files;
                break;
            }
        }

        if (offset == buffer_size)
        {
            atomicWrite(STDOUT_FILENO, buffer, buffer_size);
            offset = 0;
        }

        
    }

    if (atomicWrite(STDOUT_FILENO, buffer, offset) == ERROR)
    {
        exit(EXIT_FAILURE); 
    }

    for(int i = 0; i < N_files; i++)
    {
        free(files[i].buffer);
    }

    free(buffer);
    free(files);
}
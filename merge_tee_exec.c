#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define true 1
#define false 0

#define HELP_MSG "Uso: ./merge_tee_exec -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE\tTamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-l LOGFILE\tNombre del archivo de log.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n"
#define MAX_SIZE_BFF 128 * 1024 * 1024
#define MIN_SIZE_BFF 1
#define MAX_NUM_PROC 8
#define MIN_NUM_PROC 1
#define DEFAULT_NUM_PROC "1"
#define DEFAULT_SIZE_BFF "1024"

int main(int argc, char *argv[])
{

    // Example: ./merge_tee_exec -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]
    //  -l: log file
    //  -t: buffer size
    //  -p: number of processes
    //  FILEIN1: input file 1
    //  FILEIN2: input file 2
    //  ...
    //  FILEINn: input file n

    // Get the args

    int opt;
    char *log_file_name = NULL;
    char *buffer_size = DEFAULT_SIZE_BFF;
    char *num_proc = DEFAULT_NUM_PROC;
    char **files = NULL;

    while ((opt = getopt(argc, argv, "l:t:p:h")) != -1)
    {
        switch (opt)
        {
        case 'l':
            log_file_name = optarg;
            break;
        case 't':
            buffer_size = optarg;
            break;
        case 'p':
            num_proc = optarg;
            break;
        case 'h':
            fprintf(stdout, "%s", HELP_MSG);
            exit(EXIT_SUCCESS);
        default:
            fprintf(stderr, "%s", HELP_MSG);
            exit(EXIT_FAILURE);
        }
    }

    // Get number of files
    int num_files = argc - optind;
    if( num_files > 0 ){
        files = malloc( num_files * sizeof(char*) );
        if( files == NULL ){
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
        for( int i = 0; i < num_files; i++ ){
            files[i] = argv[optind + i];
        }
    }

    // Check the args
    if (log_file_name == NULL)
    {
        fprintf(stderr, "Error: No hay fichero de log.\n");
        fprintf(stderr, "%s", HELP_MSG);
        exit(EXIT_FAILURE);
    }

    int bf_size_int = atoi(buffer_size);
    if (bf_size_int < MIN_SIZE_BFF || bf_size_int > MAX_SIZE_BFF)
    {
        fprintf(stderr, "Error: Tamaño de buffer incorrecto.\n");
        fprintf(stderr, "%s", HELP_MSG);
        exit(EXIT_FAILURE);
    }

    if( num_files == 0 ){
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        fprintf(stderr, "%s", HELP_MSG);
        exit(EXIT_FAILURE);
    } 

    int num_proc_int = atoi(num_proc);
    if (num_proc_int < MIN_NUM_PROC || num_proc_int > MAX_NUM_PROC)
    {
        fprintf(stderr, "Error: El número de procesos en ejecución tiene que estar entre 1 y 8.\n");
        fprintf(stderr, "%s", HELP_MSG);
        exit(EXIT_FAILURE);
    }


    //Pipe for conecting the output of the merge_files to the input of the tee through a pipe


    int pipe_out_merge_files_in_tee[2];

    if (pipe(pipe_out_merge_files_in_tee) == -1)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    //Launching the merge_files

    pid_t pid_merge_files = fork();

    switch( pid_merge_files ){
        case -1:
            perror("fork()");
            exit(EXIT_FAILURE);
        case 0:
            //Child process
            //merge_files
            if ( close(pipe_out_merge_files_in_tee[0]) == -1 ){ //Close the read end of the pipe
                perror("close()");
                exit(EXIT_FAILURE);
            }

            if( pipe_out_merge_files_in_tee[1] != STDOUT_FILENO ){ /*Defensive check*/

                if( dup2(pipe_out_merge_files_in_tee[1], STDOUT_FILENO) == -1 ){
                    perror("dup2()");
                    exit(EXIT_FAILURE);
                }

                if( close(pipe_out_merge_files_in_tee[1]) == -1 ){
                    perror("close()");
                    exit(EXIT_FAILURE);
                }
            }

            /*
            Uso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]
            No admite lectura de la entrada estandar.
            -t BUFSIZE      Tamaño de buffer donde 1 <= BUFSIZE <= 128MB
            -o FILEOUT      Usa FILEOUT en lugar de la salida estandar
            */

            //Create the args for the merge_files
            int num_args = 4 + num_files; //1 for "merge_files", 1 for "-t", 1 for buffer_size, 1 for NULL
            char ** merge_files_args = malloc( num_args * sizeof(char*) ); 
            merge_files_args[0] = "merge_files";
            merge_files_args[1] = "-t";
            merge_files_args[2] = buffer_size;
            for( int i = 0; i < num_files; i++ ){
                merge_files_args[3 + i] = files[i];
            }
            merge_files_args[num_args - 1] = NULL;

            execvp("./merge_files", merge_files_args);
            perror("execvp()");   //If execvp() returns, it must have failed       
            exit(EXIT_FAILURE);
        default:
            //parent process 
            break;
    }

    //Pipe for conecting the output of the tee to the input of the exec_lines through a pipe

    int pipe_out_tee_in_exec_lines[2];
    if( pipe(pipe_out_tee_in_exec_lines) == -1 ){
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    //Launching the tee
    pid_t pid_tee = fork();
    switch(pid_tee){
        case -1:
            perror("fork()");
            exit(EXIT_FAILURE);
        case 0:
            //Child process
            //tee
            if( close(pipe_out_merge_files_in_tee[1]) == -1 ){ // Write end of pipe_out_merge_files_in_tee is not used by the child
                perror("close()");
                exit(EXIT_FAILURE);
            }

            if ( close(pipe_out_tee_in_exec_lines[0]) == -1 ){ //Close the read end of the pipe
                perror("close()");
                exit(EXIT_FAILURE);
            }

            if( pipe_out_merge_files_in_tee[0] != STDIN_FILENO ){ /*Defensive check*/

                if( dup2(pipe_out_merge_files_in_tee[0], STDIN_FILENO) == -1 ){
                    perror("dup2()");
                    exit(EXIT_FAILURE);
                }

                if( close(pipe_out_merge_files_in_tee[0]) == -1 ){
                    perror("close()");
                    exit(EXIT_FAILURE);
                }
            }

            if( pipe_out_tee_in_exec_lines[1] != STDOUT_FILENO ){ /*Defensive check*/

                if( dup2(pipe_out_tee_in_exec_lines[1], STDOUT_FILENO) == -1 ){
                    perror("dup2()");
                    exit(EXIT_FAILURE);
                }

                if( close(pipe_out_tee_in_exec_lines[1]) == -1 ){
                    perror("close()");
                    exit(EXIT_FAILURE);
                }
            }

            //Create the args for the tee
            execlp("tee", "tee", log_file_name, NULL);
            perror("execlp(tee)");   //If execlp() returns, it must have failed
            exit(EXIT_FAILURE);
        default:
            //parent process
            break;
    }

    //Closing unused ends of the pipes
    if( close(pipe_out_merge_files_in_tee[0]) == -1 ){
        perror("close() pipe_out_merge_files_in_tee[0] ");
        exit(EXIT_FAILURE);
    }

    if( close(pipe_out_merge_files_in_tee[1]) == -1 ){
        perror("close() pipe_out_merge_files_in_tee[1] ");
        exit(EXIT_FAILURE);
    }

    if( close(pipe_out_tee_in_exec_lines[1]) == -1 ){
        perror("close() pipe_out_tee_in_exec_lines[1] ");
        exit(EXIT_FAILURE);
    }

    //Launching the exec_lines
    pid_t pid_exec_lines = fork();
    switch(pid_exec_lines){
        case -1:
            perror("fork()");
            exit(EXIT_FAILURE);
        case 0:
            //Child process
            //exec_lines
            if( pipe_out_tee_in_exec_lines[0] != STDIN_FILENO ){ /*Defensive check*/

                if( dup2(pipe_out_tee_in_exec_lines[0], STDIN_FILENO) == -1 ){
                    perror("dup2()");
                    exit(EXIT_FAILURE);
                }

                if( close(pipe_out_tee_in_exec_lines[0]) == -1 ){
                    perror("close()");
                    exit(EXIT_FAILURE);
                }
            }

            //Create the args for the exec_lines
            execlp("exec_lines", "exec_lines", "-p", num_proc, NULL);
            perror("execlp(exec_lines)");   //If execlp() returns, it must have failed
            exit(EXIT_FAILURE);
        default:
            //parent process
            break;
    }

    //Closing unused ends of the pipes
    if( close(pipe_out_tee_in_exec_lines[0]) == -1 ){
        perror("close() pipe_out_tee_in_exec_lines[0] ");
        exit(EXIT_FAILURE);
    }

    //Waiting for the children to finish
    int flag = 0;
    int status;
    if( waitpid(pid_merge_files, &status, 0) == -1 ){
        perror("waitpid()");
        exit(EXIT_FAILURE);
    }

    if( WIFEXITED(status) && WEXITSTATUS(status) != 0 ){
        flag = 1;
    }

    if( waitpid(pid_tee, &status, 0) == -1 ){
        perror("waitpid()");
        exit(EXIT_FAILURE);
    }

    if( WIFEXITED(status) && WEXITSTATUS(status) != 0 ){
        flag = 1;
    }

    if( waitpid(pid_exec_lines, &status, 0) == -1 ){
        perror("waitpid()");
        exit(EXIT_FAILURE);
    }

    if( WIFEXITED(status) && WEXITSTATUS(status) != 0 ){
        flag = 1;
    }

    if( flag ){
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

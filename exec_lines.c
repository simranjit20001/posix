#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFSIZE 16
#define MAX_LINE_SIZE 128
#define CADENA_AYUDA "Uso: exec_lines [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes\npara ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n"

#define true 1
#define false 0

int err_flag;

#define ERR -1
#define ERR_FORK -2
#define ERR_EXECLP -3
#define ERR_WAIT -4
#define ERR_VERY_LONG_LINE -5
#define ERR_BAD_NUMPROC -6
#define ERR_MALLOC -7
#define ERR_BAD_ARGS -8

pid_t execute(char **args)
{
    pid_t pid; /* Usado en el proceso padre para guardar el PID del proceso hijo */

    switch (pid = fork())
    {
    case -1: /* fork() falló */
        perror("fork()");
        err_flag = ERR_FORK;
        return ERR;            // dejamos que decida el "caller" que hacer
    case 0:                    /* Ejecución del proceso hijo tras fork() con éxito */
        execvp(args[0], args); /* Sustituye el binario actual por /bin/ls */
        perror("execlp()");    /* Esta línea no se debería ejecutar si la anterior tuvo éxito */
        err_flag = ERR_EXECLP;
        return ERR; // dejamos que decida el "caller" que hacer
    }

    return pid;
}

int main(int argc, char **argv)
{
    int opt, MAX_PROCESOS = 1;
    optind = 1;

    while ((opt = getopt(argc, argv, "p:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            MAX_PROCESOS = atoi(optarg);
            break;

        case 'h':
            fprintf(stdout, CADENA_AYUDA);
            exit(EXIT_SUCCESS);
            break;
        default:
            fprintf(stderr, CADENA_AYUDA);
            err_flag = ERR_BAD_ARGS;
            exit(EXIT_FAILURE);
        }
    }

    if (MAX_PROCESOS < 1 || MAX_PROCESOS > 8)
    {
        fprintf(stderr, "Error: El número de procesos en ejecución tiene que estar entre 1 y 8.\n");
        fprintf(stderr, CADENA_AYUDA);
        err_flag = ERR_BAD_NUMPROC;
        exit(EXIT_FAILURE);
    }

    int procesos = 0;
    int offset = 0;
    int readed_length = 1;
    char buffer[BUFSIZE];
    int args_index = 0;

    // el sentido de hacer MAX_LINE_SIZE+1 es para poder poner un '\0' al final de la cadena en caso de que sea la ultima y no haya un '\n'
    // el salto de linea se trata como un caracter mas
    // por lo tanto el maximo de caracteres que se pueden leer es MAX_LINE_SIZE incluido el '\n'

    char *lines = malloc(sizeof(char) * MAX_LINE_SIZE + 1); // No es necesario hacer malloc, podemos usar un array lines[MAX_LINE_SIZE + 1].

    if (lines == NULL)
    {
        perror("malloc()");
        err_flag = ERR_MALLOC;
        exit(EXIT_FAILURE);
    }

    char **args = malloc(sizeof(char *) * MAX_LINE_SIZE + 1); // No es necesario hacer malloc, podemos usar un array args[MAX_LINE_SIZE + 1].

    // Se contemplan argumentos vacios como " "
    // Como el tamaño es de tan solo 128 caracteres, no merece la pena adaptar el tamaño a la linea de la entrada al ser tan pequeño

    if (args == NULL)
    {
        free(lines);
        err_flag = ERR_MALLOC;
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    char *start_pointer = lines; // Es un puntero que apunta siempre al argumento que estamos leyendo

    while (readed_length > 0) // Lectura es correcta
    {
        readed_length = read(STDIN_FILENO, buffer, BUFSIZE);

        for (int i = 0; i < readed_length; i++)
        {

            lines[offset] = buffer[i];
            offset++;

            if (offset >= MAX_LINE_SIZE)
            {

                fprintf(stderr, "Error: Tamaño de línea mayor que 128.\n");
                free(args);
                free(lines);
                err_flag = ERR_VERY_LONG_LINE;
                exit(EXIT_FAILURE);
            }

            if (lines[offset - 1] == ' ')
            {
                lines[offset - 1] = '\0';

                if (*start_pointer == '\0')
                {
                    // Si el argumento es vacio, no se añade a la lista de argumentos
                    start_pointer = lines + offset;
                    continue;
                }

                args[args_index] = start_pointer;
                args_index++;
                start_pointer = lines + offset;

                /*
                    nuestro objetivo es obtener un vector como el siguiente:
                    args = [ "path", "arg1", "arg2", ... , NULL ]
                    con una entrada
                    "path arg1 arg2 ..."
                    en este if mantenemos el puntero start_pointer apuntando al principio de la siguiente palabra
                    y cada vez que llegamos a un espacio, ponemos un \0 en la posición anterior y guardamos el puntero
                    en args.
                    De este modo la entrada se conviere:
                    "path\0arg1\0arg2\0...\0"
                    y args queda como:
                    args = [ "path", "arg1", "arg2", ... , NULL ]
                */
            }

            else if (lines[offset - 1] == '\n')
            {
                lines[offset - 1] = '\0'; // Remplazamos el \n por un \0. El -1 es porque offset ya ha sido incrementado

                args[args_index] = start_pointer; // Añadimos el ultimo argumento
                args[args_index + 1] = NULL;      // Añadimos el NULL al final para indicar fin de argumentos

                if (*start_pointer != '\0')
                {
                    args[args_index] = start_pointer; // Añadimos el ultimo argumento si no es vacio
                    args[args_index + 1] = NULL;      // Añadimos el NULL al final para indicar fin de argumentos
                }
                else
                {
                    args[args_index] = NULL; // Añadimos el NULL al final para indicar fin de argumentos
                }

                if (procesos == MAX_PROCESOS)
                {
                    if (wait(NULL) == -1)
                    {
                        perror("wait()");
                        free(args);
                        free(lines);
                        err_flag = ERR_WAIT;
                        exit(EXIT_FAILURE);
                    }

                    procesos--;
                }

                procesos++;

                if (execute(args) == ERR)
                {
                    free(args);
                    free(lines);
                    exit(EXIT_FAILURE);
                };

                args_index = 0;
                offset = 0;
                start_pointer = lines; // Reseteamos las variables temporales
            }
        }
    }

    if (readed_length == -1)
    {
        perror("read()");
        free(args);
        free(lines);
        err_flag = _IO_ERR_SEEN;
        exit(EXIT_FAILURE);
    } // Verificamos si la lectura fallo en algun punto

    if (offset > 0)
    {

        lines[offset] = '\0';
        if (*start_pointer != '\0')
        {
            args[args_index] = start_pointer; // Añadimos el ultimo argumento si no es vacio
            args[args_index + 1] = NULL;      // Añadimos el NULL al final para indicar fin de argumentos
        }
        else
        {
            args[args_index] = NULL; // Añadimos el NULL al final para indicar fin de argumentos
        }

        if (procesos == MAX_PROCESOS)
        {

            if (wait(NULL) == -1)
            {
                perror("wait()");
                err_flag = ERR_WAIT;
                exit(EXIT_FAILURE);
            }

            procesos--;
        }

        procesos++;

        if (execute(args) == ERR)
        {
            free(args);
            free(lines);
            exit(EXIT_FAILURE);
        };

    } // Treat the last command if it doesn't end with a newline

    // Check if execution failed because of errors

    for (int i = 0; i < procesos; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait()");
            free(args);
            free(lines);
            err_flag = ERR_WAIT;
            exit(EXIT_FAILURE);
        }
    } // Wait for all processes to finish

    // Free memory
    free(args);
    free(lines);

    return EXIT_SUCCESS;
}

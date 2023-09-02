OBJS	= exec_lines.o merge_file_memchr.o merge_file_version_clase.o merge_files_alternativo.o merge_tee_exec.o
OUT	= exec_lines merge_files merge_file_version_clase merge_files_alternativo merge_tee_exec

OBJS0	= exec_lines.o
SOURCE0	= exec_lines.c
HEADER0	= 
OUT0	= exec_lines

OBJS1	= merge_file_memchr.o
SOURCE1	= merge_file_memchr.c
HEADER1	= 
OUT1	= merge_files

OBJS2	= merge_file_version_clase.o
SOURCE2	= merge_file_version_clase.c
HEADER2	= 
OUT2	= merge_file_version_clase

OBJS3	= merge_files_alternativo.o
SOURCE3	= merge_files_alternativo.c
HEADER3	= 
OUT3	= merge_files_alternativo

OBJS4	= merge_tee_exec.o
SOURCE4	= merge_tee_exec.c
HEADER4	= 
OUT4	= merge_tee_exec

CC	 = gcc
FLAGS	 = -g3 -c -Wall
LFLAGS	 = 

all: exec_lines merge_files merge_file_version_clase merge_files_alternativo merge_tee_exec

exec_lines: $(OBJS0) $(LFLAGS)
	$(CC) -g $(OBJS0) -o $(OUT0)

merge_files: $(OBJS1) $(LFLAGS)
	$(CC) -g $(OBJS1) -o $(OUT1)

merge_file_version_clase: $(OBJS2) $(LFLAGS)
	$(CC) -g $(OBJS2) -o $(OUT2)

merge_files_alternativo: $(OBJS3) $(LFLAGS)
	$(CC) -g $(OBJS3) -o $(OUT3)

merge_tee_exec: $(OBJS4) $(LFLAGS)
	$(CC) -g $(OBJS4) -o $(OUT4)

exec_lines.o: exec_lines.c
	$(CC) $(FLAGS) exec_lines.c 

merge_file_memchr.o: merge_file_memchr.c
	$(CC) $(FLAGS) merge_file_memchr.c 

merge_file_version_clase.o: merge_file_version_clase.c
	$(CC) $(FLAGS) merge_file_version_clase.c 

merge_files_alternativo.o: merge_files_alternativo.c
	$(CC) $(FLAGS) merge_files_alternativo.c 

merge_tee_exec.o: merge_tee_exec.c
	$(CC) $(FLAGS) merge_tee_exec.c 


clean:
	rm -f $(OBJS) $(OUT)

debug: $(OUT)
	valgrind $(OUT)

valgrind: $(OUT)
	valgrind $(OUT)

valgrind_leakcheck: $(OUT)
	valgrind --leak-check=full $(OUT)

valgrind_extreme: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(OUT)
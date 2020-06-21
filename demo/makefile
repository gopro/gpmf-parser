gpmfdemo : GPMF_demo.o GPMF_parser.o GPMF_mp4reader.o GPMF_print.o str_split.o
		gcc -o gpmfdemo GPMF_demo.o GPMF_parser.o GPMF_mp4reader.o GPMF_print.o str_split.o -lasan

GPMF_demo.o : GPMF_demo.c ../GPMF_parser.h
		gcc -g -c GPMF_demo.c
GPMF_mp4reader.o : GPMF_mp4reader.c ../GPMF_parser.h
		gcc -g -c GPMF_mp4reader.c
GPMF_print.o : GPMF_print.c ../GPMF_parser.h
		gcc -g -c GPMF_print.c
GPMF_parser.o : ../GPMF_parser.c ../GPMF_parser.h
		gcc -g -c ../GPMF_parser.c
str_split.o : str_split.c str_split.h
		gcc -g -c str_split.c
clean :
		rm gpmfdemo GPMF_demo.o GPMF_parser.o GPMF_mp4reader.o GPMF_print.o str_split.o
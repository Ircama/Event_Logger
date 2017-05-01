struct MATRIX
{
	int xMin;		// Stored on FLASH address 0x10c
	int xMax;		// Stored on FLASH address 0x110
	int yMin;		// Stored on FLASH address 0x104
	int yMax;		// Stored on FLASH address 0x108
} ts_matrix;

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

extern int errno;

int
main(int argc, char *argv[]) {
FILE * cal_file;
int ret;

printf("1.");fflush(stdout);

if (argc == 2) {
	printf("2.");fflush(stdout);

	cal_file = fopen (argv[1],"rb");
	printf("3.");fflush(stdout);
	if (cal_file == NULL) {
    printf("setcal: cannot open filename %s.\n	%s\nUsage: setcal <filename> [ <xMin> <xMax> <yMin> <yMax> ]\n", argv[1], strerror( errno ) );
		exit(1);
	}
		printf("4.");fflush(stdout);
		if ((ret=fread (&ts_matrix, sizeof(int), 4, cal_file)) == 4) {
		printf("setcal: filename=%s, xMin=%d, xMax=%d, yMin=%d, yMax=%d\n", argv[1], ts_matrix.xMin, ts_matrix.xMax, ts_matrix.yMin, ts_matrix.yMax);
		exit(0);
		}
		printf("setcal: wrong content of filename %s\n	%s\n%d chars.\n", argv[1], strerror( errno ) , ret);
		exit(1);
}

if (argc != 6) {
     printf("Usage: setcal <filename> [ <xMin> <xMax> <yMin> <yMax> ]\n");
     exit(1);
}

printf("5.");fflush(stdout);

cal_file = fopen (argv[1],"rb");
printf("6.");fflush(stdout);

if (cal_file == NULL) {
  printf("setcal: previous filename %s was not existing.\n", argv[1]);
} else {
	printf("7.");fflush(stdout);
	if ((ret=fread (&ts_matrix, sizeof(int), 4, cal_file)) == 4) {
	printf("setcal: original content in filename %s was: xMin=%d, xMax=%d, yMin=%d, yMax=%d\n", argv[1], ts_matrix.xMin, ts_matrix.xMax, ts_matrix.yMin, ts_matrix.yMax);
	} else
		printf("setcal: cannot read previous content of existing filename %s\n	%s\nchars=%d\n", argv[1], strerror( errno ), ret);

	printf("8.");fflush(stdout);
	fclose(cal_file);
}

printf("9.");fflush(stdout);

cal_file = fopen (argv[1],"wb");
printf("a.");fflush(stdout);
if (cal_file == NULL) {
		printf("setcal: cannot open filename %s for writing.\n	%s\n", argv[1],strerror( errno ) );
		exit(1);
}

ts_matrix.xMin=(int) atoi(argv[2]);
ts_matrix.xMax=(int) atoi(argv[3]);
ts_matrix.yMin=(int) atoi(argv[4]);
ts_matrix.yMax=(int) atoi(argv[5]);

printf("b.");fflush(stdout);
if (fwrite (&ts_matrix, sizeof(int), 4, cal_file) == 4) {
	fclose(cal_file);

	printf("c.");fflush(stdout);
	cal_file = fopen (argv[1],"rb");

	if (cal_file == NULL) {
    printf("setcal: cannot open filename %s after writing new content.\n	%s\n", argv[1],strerror( errno ) );
		exit(1);
	}

	printf("d.");fflush(stdout);
	if ((ret=fread (&ts_matrix.xMin, sizeof(int), 4, cal_file)) == 4) {
		printf("setcal: new content in filename %s is: xMin=%d, xMax=%d, yMin=%d, yMax=%d\n", argv[1], ts_matrix.xMin, ts_matrix.xMax, ts_matrix.yMin, ts_matrix.yMax);
		exit(0);
		}
	printf("setcal: cannot read new content of filename %s.\n	%s\nChars=%d.\n", argv[1], strerror( errno ) , ret);
	exit(1);
}

printf("setcal: cannot write to filename %s\n	%s\n", argv[1], strerror( errno ) );
exit(1);
}

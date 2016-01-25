#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "options.h"


int debug = 0;
double *results;
double *ratios;
unsigned long   *sizes;

int no_sz = 1, no_ratio =1, no_version=1;



static inline
double gettime(void) {
  // to be implemented
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec / 1000000.;
}


static void toupper_simple(char * text) {
  // to be implemented
    char c;
    while((c=*text) != 0){
        *text++=c & 0xdf;
    }
}

unsigned char c[] = {0xdf};
static void toupper_128_npw(char *text){
    asm(
        ".intel_syntax noprefix             \n" //gnarf shitty AT&T synax ... wer will schon sowas
        "   VPXOR xmm1,xmm1,xmm1            \n"
        "   VPBROADCASTB xmm2, [%1]         \n" //fuellt xmm1 mit 0xdf bytes
        "loop_128_npw:                      \n"
        "   VPAND xmm3, xmm2, [rdi]         \n" //c = c & 0xdf fuer jeden char -> klein zu gross
        "   VPCMPISTRM xmm1, xmm3, 0x58     \n" //vergleicht mit 0x00 string, und erstellt maske -> 
                                                //0xff fuer byte vor least siginificant null-byte und != 0x00
                                                //setzt flags [ZFlag fuer xmm3 enthaelt null byte]
        "   VMASKMOVDQU xmm3, xmm0          \n" //schreibt ergebnis zurueck unter beruecksichtigung der maske (nur valid mask bytes werden geschrieben)
        "   LEA rdi, [rdi+16]               \n" //add 16 to rdi
        "   JNZ loop_128_npw                \n"
        ".att_syntax                        \n" //TAKE THIS UGLY SYNTAX, GCC!!!
        : /* no output registers*/
        : "D"(text), "r"(c)
        : "xmm0", "xmm1", "xmm2", "xmm3");
}

static void toupper_128_pw_strm(char *text){
    asm(
        ".intel_syntax noprefix             \n" //gnarf shitty AT&T synax ... wer will schon sowas
        "   VPXOR xmm1,xmm1,xmm1            \n" //zero xmm1
        "   VPBROADCASTB xmm2, [%1]         \n" //fuellt xmm2 mit 0xdf bytes
        "loop_128_pw_strm:                  \n"
        "   VPAND xmm3, xmm2, [rdi]         \n" //c = c & 0xdf fuer jeden char -> klein zu gross
        "   VPCMPISTRM xmm1, xmm3, 0x58     \n" //setzt flags [ZFlag fuer xmm3 enthaelt null byte]
        "   VMOVDQU [rdi], xmm3             \n" //moves all 16 bytes to memory
                                                //(!ueberschreibt auch alles nach dem nullbyte -> fuer correct andere version nehmen)
        "   LEA rdi, [rdi+16]                \n" //add 16 to rdi -> next block of bytes
        "   JNZ loop_128_pw_strm            \n"
        ".att_syntax                        \n" //TAKE THIS UGLY SYNTAX, GCC!!!
        : /* no output registers*/
        : "D"(text), "r"(c)
        : "xmm0", "xmm1", "xmm2", "xmm3");
}

static void toupper_128_pw_cmpb(char *text){
    asm(
        ".intel_syntax noprefix             \n" //gnarf shitty AT&T synax ... wer will schon sowas
        "   VPXOR xmm1,xmm1,xmm1            \n" //zero xmm1
        "   VPBROADCASTB xmm2, [%1]         \n" //fuellt xmm2 mit 0xdf bytes
        "loop_128_pw_cmpb:                  \n"
        "   VPAND xmm3, xmm2, [rdi]         \n" //c = c & 0xdf fuer jeden char -> klein zu gross
        "   VMOVDQU [rdi], xmm3             \n" //moves all 16 bytes to memory
                                                //(!ueberschreibt auch alles nach dem nullbyte -> fuer correct andere version nehmen)
        "   VPCMPEQB xmm0, xmm3, xmm1       \n"
        "   VPTEST xmm0, xmm0               \n"
        "   LEA rdi,[rdi+16]                \n" //add 16 to rdi -> next block of bytes
        "   JZ loop_128_pw_cmpb            \n"
        ".att_syntax                        \n" //TAKE THIS UGLY SYNTAX, GCC!!!
        : /* no output registers*/
        : "D"(text), "r"(c)
        : "xmm0", "xmm1", "xmm2", "xmm3");
}

static void toupper_256_pw(char *text){
    asm(
        ".intel_syntax noprefix             \n" //gnarf shitty AT&T synax ... wer will schon sowas
        "   VPXOR ymm1,ymm1,ymm1           \n" //zero ymm1
        "   VPBROADCASTB ymm2, [%1]         \n" //fuellt ymm2 mit 0xdf bytes
        "loop_256_pw:                       \n"
        "   VPAND ymm3, ymm2, [rdi]         \n" //c = c & 0xdf fuer jeden char -> klein zu gross
        "   VMOVDQU [rdi], ymm3             \n" //moves all 32 bytes to memory
                                                //(!ueberschreibt auch alles nach dem nullbyte -> fuer correct andere version nehmen)
        "   VPCMPEQB ymm0, ymm3, ymm1       \n" //vergleicht jedes byte aus ymm3 mit 0 und setzt 0xff (==0) oder 0x00 (!=0) in ymm0
        "   VPTEST ymm0, ymm0               \n" //gab es ein null byte aka ymm0 != 0
        "   LEA rdi,[rdi+32]                \n" //add 32 to rdi -> next block of bytes
        "   JZ loop_256_pw                  \n"
        ".att_syntax                        \n" //TAKE THIS UGLY SYNTAX, GCC!!!
        : /* no output registers*/
        : "D"(text), "r"(c)
        : "ymm0", "ymm1", "ymm2", "ymm3");
}

/*****************************************************************/

// align at 16byte boundaries
void* mymalloc(unsigned long int size)
{
     void* addr = malloc(size+64);
     if (addr == NULL){
        perror("myalloc failed");
        exit(errno);
     }
     *(void**)((unsigned long int) addr /32*32+32-sizeof(void*)) = addr;
     return (void*)((unsigned long int)addr /32*32+32);
}

void myfree(void *ptr){
    void* rptr = *(void**)(ptr - sizeof(void*));
    free(rptr);
}

char createChar(int ratio){
	char isLower = rand()%100;

	// upper case=0, lower case=1
	if(isLower < ratio)
		isLower =0;
	else
		isLower = 1;

	char letter = rand()%26+1; // a,A=1; b,B=2; ...

	return 0x40 + isLower*0x20 + letter;

}

char * init(unsigned long int sz, int ratio){
    int i=0;
    char *text = (char *) mymalloc(sz+1);
    srand(1);// ensures that all strings are identical
    for(i=0;i<sz;i++){
			char c = createChar(ratio);
			text[i]=c;
	  }
    text[i] = '\0';
    return text;
}



/*
 * ******************* Run the different versions **************
 */

typedef void (*toupperfunc)(char *text);

void run_toupper(int size, int ratio, int version, toupperfunc f, const char* name)
{
   double start, stop;
		int index;

		index =  ratio;
		index += size*no_ratio;
		index += version*no_sz*no_ratio;

    char *text = init(sizes[size], ratios[ratio]);


    if(debug) printf("Before: %.40s...\n",text);

    start = gettime();
    (*f)(text);
    stop = gettime();
    results[index] = stop-start;

    if(debug) printf("After:  %.40s...\n",text);
    myfree(text);
}

struct _toupperversion {
    const char* name;
    toupperfunc func;
} toupperversion[] = {
    { "simple",    toupper_simple },
    { "optimised128npw", toupper_128_npw },
    { "optimised128pwstrm", toupper_128_pw_strm },
    { "optimised128pwcmpb", toupper_128_pw_cmpb },
    { "optimised256pw", toupper_256_pw },
    { 0,0 }
};


void run(int size, int ratio)
{
	int v;
	for(v=0; toupperversion[v].func !=0; v++) {
		run_toupper(size, ratio, v, toupperversion[v].func, toupperversion[v].name);
	}

}

void printresults(){
	int i,j,k,index;
	printf("%s\n", OPTS);

	for(j=0;j<no_sz;j++){
		for(k=0;k<no_ratio;k++){
			printf("Size: %ld \tRatio: %f \tRunning time:", sizes[j], ratios[k]);
			for(i=0;i<no_version;i++){
				index =  k;
				index += j*no_ratio;
				index += i*no_sz*no_ratio;
				printf("\t%s: %f", toupperversion[i].name, results[index]);
			}
			printf("\n");
		}
	}
}

int main(int argc, char* argv[])
{
    unsigned long int min_sz=800000, max_sz = 0, step_sz = 10000;
		int min_ratio=50, max_ratio = 0, step_ratio = 1;
		int arg,i,j,v;
		int no_exp;

		for(arg = 1;arg<argc;arg++){
			if(0==strcmp("-d",argv[arg])){
				debug = 1;
			}
			if(0==strcmp("-l",argv[arg])){
					min_sz = atoi(argv[arg+1]);
					if(arg+2>=argc) break;
					if(0==strcmp("-r",argv[arg+2])) break;
					if(0==strcmp("-d",argv[arg+2])) break;
					max_sz = atol(argv[arg+2]);
					step_sz = atol(argv[arg+3]);
			}
			if(0==strcmp("-r",argv[arg])){
					min_ratio = atoi(argv[arg+1]);
					if(arg+2>=argc) break;
					if(0==strcmp("-l",argv[arg+2])) break;
					if(0==strcmp("-d",argv[arg+2])) break;
					max_ratio = atol(argv[arg+2]);
					step_ratio = atol(argv[arg+3]);
			}

		}
    for(v=0; toupperversion[v].func !=0; v++)
		no_version=v+1;
		if(0==max_sz)  no_sz =1;
		else no_sz = (max_sz-min_sz)/step_sz+1;
		if(0==max_ratio)  no_ratio =1;
		else no_ratio = (max_ratio-min_ratio)/step_ratio+1;
		no_exp = v*no_sz*no_ratio;
		results = (double *)malloc(sizeof(double[no_exp]));
		ratios = (double *)malloc(sizeof(double[no_ratio]));
		sizes = (long *)malloc(sizeof(long[no_sz]));

		for(i=0;i<no_sz;i++)
			sizes[i] = min_sz + i*step_sz;
		for(i=0;i<no_ratio;i++)
			ratios[i] = min_ratio + i*step_ratio;

		for(i=0;i<no_sz;i++)
			for(j=0;j<no_ratio;j++)
				run(i,j);

		printresults();
    return 0;
}

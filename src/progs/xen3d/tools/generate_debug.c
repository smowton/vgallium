
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

int main(int argc, char** argv) {

    if(argc < 3) {
	printf("Usage: generate_switches (file-in) (file-out)\nFinds lines like REMREQ_BLAH in file-in and produces a dispatcher switch statement in file-out\n");
	return 1;
    }

    FILE* fdin = fopen(argv[1], "r");
    FILE* fdout = fopen(argv[2], "w");
    
    char* nextline = 0;
    size_t linesize = 0;
    
    while(getline(&nextline, &linesize, fdin) != -1) {
    
	char* begin = strstr(nextline, "REMRE");
	if(!begin)
	    continue;
	
	char* end = begin;
	while(*end != ' ' && (end - nextline < linesize)) {
	    end++;
	}
	
	if(*end != ' ')
	    continue;
	
	*end = '\0';
	
	fprintf(fdout, "\tcase %s:\n\t{\n", begin);
	fprintf(fdout, "\t\treturn \"%s\";\n\n", begin);
	
	free(nextline);
	nextline = 0;
	linesize = 0;
	
    }

}

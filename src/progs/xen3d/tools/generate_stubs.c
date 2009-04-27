
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
    
	char* begin = strstr(nextline, "REMREQ");
	if(!begin)
	    continue;
	
	char* end = begin;
	while(*end != ' ' && (end - nextline < linesize)) {
	    end++;
	}
	
	if(*end != ' ')
	    continue;
	
	*end = '\0';
	
	char* c;
	
	for(c = begin; c < end; c++)
	    *c = tolower(*c);
	    
	fprintf(fdout, "\tint dispatch_%s(struct client_list_entry* client, struct %s* message);\n\n", begin, begin);
	
	free(nextline);
	nextline = 0;
	linesize = 0;
	
    }

}

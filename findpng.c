#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "png.h"
extern int errno;

int findPNG(const char* root, const char* base){
	if (!root){
        return -1;
    }

	static int numPNG = 0;
	DIR* p_dir;
	struct dirent* p_dirent;

	if((p_dir = opendir(root)) == NULL){
		printf("Attempted opendir(%s)", root);
		closedir(p_dir);
		exit(-1);
	}

	while((p_dirent = readdir(p_dir)) != NULL){

		struct stat buf;

		char* filename = (char*)malloc(256*sizeof(char));
		strcpy(filename, root);
		strcat(filename, "/");
		strcat(filename, p_dirent->d_name);

		if(!p_dirent->d_name){
			fprintf(stderr, "Null pointer found in current path");
			exit(3);
		}

		if(stat(filename, &buf) < 0){
			perror("lstat error");
			continue;
		}

		if(S_ISDIR(buf.st_mode) && p_dirent->d_name[0] != '.'){	
			numPNG += findPNG(filename, base);
		}

		else if(S_ISREG(buf.st_mode)){
			if( isActualPNG(filename) ){
				numPNG++;
				printf("%s\n", filename + strlen(base)+1 );
			}
		}
		free(filename);
	}

	if( closedir(p_dir) != 0 ){
		perror("closedir");
		exit(-2);
	}
	
	return numPNG;
}

int isActualPNG(char* filename){
	FILE* in;

	in = fopen(filename, "rb" );
	if(in == NULL){
		printf("Failed to open file %s", filename);
		exit(1);
	}

	char buf[4];
	memset(&buf, '\0', 4*sizeof(char));
	
	fread( buf, sizeof(char), 4, in );

	fclose(in);

	return ((buf[1] == 'P') && (buf[2] == 'N') && (buf[3] == 'G'));
}

int main( int argc, const char* argv[] ){

	if(argc != 2) {
		printf("Please input only root directory for search function\n");
		exit(1);
	}

	if(findPNG(argv[1], argv[1]) == 0){
		printf("findpng: No PNG file found");
		exit(0);
	}	

	return 0;
}

#include <sys/stat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

int main(int argc, char *argv[]){
    int in_fd;
    int file_size;
    FILE* file;
    if ( ( in_fd = open ( argv [1] , O_RDONLY ) ) == -1 ) {
        fprintf(stderr, "Couldn't read the file!\n");
        exit(1);
    }
    file_size = lseek ( in_fd , 0 , SEEK_END ) ;
    close ( in_fd );
    char* input_text;
    file = fopen(argv[1],"r");
    input_text = (char *)malloc((file_size + 1) * sizeof(char));
    if(input_text == NULL){
        fprintf(stderr, "Failed to allocate memory for text!\n");
        exit(1);
    }
    fread(input_text, 1, file_size, file);
    input_text[file_size] = '\0';
    printf("%s\n",input_text);
    char* c = "hello\nhello";
    if(c == input_text){
        printf("found a match");
    }else{
        printf("no match");
    }
    // for(int i = 0; i < file_size; i++){
    //     if(input_text[i] = '\n'){
    //         printf("%i:found new line\n",i);
    //     }else{
    //         printf("%c\n",input_text[i]);
    //     }
    // }
}
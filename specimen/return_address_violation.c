# include <stdio.h>
# include <string.h>
# include <stdlib.h>

void foo(void);

// -fno-stack-protectorオプションでコンパイル
int main(int argc, char *argv[]){
    if(argc<2){
        fprintf(stderr, "引数の数が足りません\n");
        exit(1);
    }

    char *flag = argv[1];

    if(strcmp(flag, "root")==0){
        printf("this is 'if' branch\n");
        foo();
        printf("this is 'if' branch\n");
    } else {
        printf("this is 'else' branch\n");
        foo();
        printf("this is 'else' branch\n");
    }
}


void foo(){
    char str[32];
    char copy[40];
    char *tmp = str;
    printf("function 'foo' is called\n");

    for(int i=0; i<32; i++)tmp++;
    for(int i=0; i<40; i++){
        copy[i] = *tmp;
        tmp++;
    }

    scanf("%s", str); // user input

    tmp = str;
    for(int i=0; i<32; i++)tmp++;
    for(int i=0; i<40; i++){
        *tmp = copy[i];
        tmp++;
    }

    printf("function 'foo' is end\n");
}
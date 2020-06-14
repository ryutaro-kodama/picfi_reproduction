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
    char str[8];
    char *tmp = str;
    printf("function 'foo' is called\n");

    scanf("%s", str); // user input

    for(int i=0; i<40; i++)tmp++;
    *tmp++ = 0x13;
    *tmp++ = 0x07;
    *tmp++ = 0x40;

    printf("function 'foo' is end\n");
}
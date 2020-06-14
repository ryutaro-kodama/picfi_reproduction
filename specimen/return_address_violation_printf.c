# include <stdio.h>
# include <string.h>
# include <stdlib.h>

void foo(void);

// -fno-stack-protectorオプションでコンパイル
int main(int argc, char *argv[]){
    int num = argc;

    if(num==1){
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
    char copy[16];
    char *tmp = str;
    printf("function 'foo' is called\n");

    for(int i=0; i<56; i++)tmp++;
    for(int i=0; i<16; i++){
        copy[i] = *tmp;
        tmp++;
    }

    scanf("%s", str); // user input

    tmp = str;
    for(int i=0; i<56; i++)tmp++;
    for(int i=0; i<16; i++){
        *tmp = copy[i];
        tmp++;
    }

    printf("function 'foo' is end\n");
}
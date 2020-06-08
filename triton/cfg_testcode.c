# include <stdio.h>
# include <string.h>
# include <stdlib.h>

void foo(void);

// -fno-stack-protectorオプションでコンパイル
int main(int argc, char *argv[]){
    int num = argc;

    if(num<5){
        if(num<3){
            foo();
        } else {
            foo();
        }
    } else {
        if(num<7){
            foo();
        } else {
            foo();
        }
    }
}

void foo(){
    char str[32];
    printf("function 'foo' is called\n");

    scanf("%s", str); // user input

    printf("function 'foo' is end\n");
}
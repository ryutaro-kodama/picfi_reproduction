# include <stdio.h>
# include <string.h>
# include <stdlib.h>

void if_func(void){
    printf("function 'if' is called\n");
}

void else_func(void){
    printf("function 'else' is called\n");
}

// -fno-stack-protectorオプションでコンパイル
int main(int argc, char *argv[]){
    int num = argc;
    void (*ptr)();
    char str[8];

    if(num==1){
        printf("this is 'if' branch\n");
        ptr = &if_func;
    } else {
        printf("this is 'else' branch\n");
        ptr = &else_func;
    }

    scanf("%s", str); // user input

    ptr();
}
# include <stdio.h>

void foo(void){
    printf("foo called\n");
}
void bar(void){
    printf("bar called\n");
}
void (*fp)() = &foo;
int main() {
    void (*bp)() = &bar;
    fp();
    bp();

    return 0;
}
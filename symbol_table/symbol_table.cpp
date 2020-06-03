#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <map>
#include <string>
#include <sstream>
#include <iostream>

std::map<std::string, std::string> symbol_table;

static void
get_symbol_table(){
    int fd[2], backup_stdin;
    std::string binaryname = "/media/sf_binary/picfi/specimen/bin/spe_no_ssp";

    pipe(fd);

    pid_t pid = fork();
    if(pid == -1){
        perror("fork");
        exit(errno);
    } else if(pid==0){
        close(fd[0]);dup2(fd[1], 1);close(fd[1]);
        execlp("nm","nm", binaryname.c_str(), NULL);
    }

    backup_stdin = dup(0);
    close(fd[1]);dup2(fd[0], 0);close(fd[0]);

    std::string buf, address, label;
    while(std::getline(std::cin, buf)){
        // printf("[%s]\n",buf.c_str());
        std::stringstream ss;
        ss << buf;

        getline(ss, address, ' ');
        if(address.size()<=1) continue; //addressが明示されていないラベルの場合
        getline(ss, label, ' ');
        getline(ss, label, ' ');

        symbol_table[address] = label;
    }
    wait(NULL);

    dup2(backup_stdin, 0);
    close(backup_stdin);
}

static void
print_table(){
    std::string address, label;
    std::map<std::string, std::string>::iterator i;

    printf("******* SYMBOL TABLE *******\n");
    for(i = symbol_table.begin(); i != symbol_table.end(); i++) {
        address = i->first;
        label = i->second;
        printf("address:[%s] -> label:[%s]\n", address.c_str(), label.c_str());
    }
    printf("******* SYMBOL TABLE END *******\n");
}

int main(void){
    // char str[32];
    get_symbol_table();
    print_table();
    // scanf("%s", str); // user input
}
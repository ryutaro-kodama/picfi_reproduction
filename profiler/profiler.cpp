#include <stdio.h>
#include <map>
#include <string>
#include <asm-generic/unistd.h>

#include "pin.H"

#include <vector>
#include <fstream>

// symbol_table作成関連
#include <unistd.h>
// #include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <sstream>
#include <iostream>


// std::map<UINT32, ADDRINT> ecfg;
std::vector<ADDRINT> return_address_array;

std::map<ADDRINT, std::string> symbol_table;
std::map<size_t, ADDRINT> branch_table;
std::map<size_t, std::pair<ADDRINT, size_t> > target_table;

// void printMovingAddress(ADDRINT src_addr, ADDRINT target_address){
//   fprintf(stderr, "0x%08jx -> 0x%08jx\n", src_addr, target_address);
// }

/*****************************************************************************
 *                             Analysis functions                            *
 *****************************************************************************/

// static void
// sample_call(ADDRINT IARG_INST_PTR, ADDRINT IARG_RETURN_IP, ADDRINT IARG_BRANCH_TARGET_ADDR){
//   printf("[0x%08jx]で[0x%08jx]を[0x%08jx]のrtnで登録\n",
//           IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_RETURN_IP);
// }

static void
activate_address(ADDRINT target_address){
  return_address_array.push_back(target_address);

  // std::map<size_t, std::string>::iterator i;
  // std::vector<size_t> tmp_id;
  // for(i=branch_table.begin(); i!=branch_table.end(); i++){
  //   if(i->second==branch_address) tmp_id.push_back(i->first);
  // }

  // std::vector<size_t>::j;
  // for(j=tmp_id.begin(); j!=tmp_id.end(); j++){
  //   if(target_table[j].first==target_address){
  //     target_table[j].second = 1;
  //     return 0;
  //   }
  // }

  // // End roop means this ControlFrow isn't in table.
  // detect_violation();
}

static void
activate_return_address(ADDRINT IARG_RETURN_IP){
  activate_address(IARG_RETURN_IP);
}

// static void
// activatefunction_address(ADDRINT IARG_BRANCH_TARGET_ADDR){
//   if(patchstub_at_address!=IARG_BRANCH_TARGET_ADDR) return;
//
//   slackでもらったpatchstub_atの処理により、target_addressを引き抜く
//   (target_address = Inderect Callされる関数のアドレス)
//

//   activate_address(target_address);
// }

static void
check_activated_address(ADDRINT target_address){
  std::vector<ADDRINT>::iterator itr;
  int flag=0;

  // search
  for(itr=return_address_array.begin(); itr!=return_address_array.end(); itr++){
    if(*itr==target_address){
      flag=1;
      break;
    }
  }

  if(flag== 1){
    // 正しいreturn先に飛んでいる
  } else {
    // 間違ったreturn先に飛んでいる
    fprintf(stderr, "間違ったreturn先ですね\n");
  }
}

static void
check_activated_function_address(ADDRINT target_address){
  // check_activated_address(target_address);
}

static void
check_activated_return_address(ADDRINT target_address){
  check_activated_address(target_address);
}

/*****************************************************************************
 *                         Instrumentation functions                         *
 *****************************************************************************/

static void
instrument_insn(INS ins, void *v)
{
  // IMG img = IMG_FindByAddress(INS_Address(ins));
  // if(!IMG_Valid(img) || !IMG_IsMainExecutable(img)) return;

  if(INS_IsCall(ins)) {
    // INS_InsertCall(
    //   ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)sample_call,
    //   IARG_INST_PTR, // The address of the instrumented instruction.
    //   IARG_RETURN_IP, // Return address for function call, valid only at the function entry point.
    //   IARG_BRANCH_TARGET_ADDR,
    //   IARG_END
    // );



    // Call命令の引数がpatchstub_atか確認
    // INS_InsertCall(
    //   ins, IPOINT_BEFORE, (AFUNPTR)judge_patchstub_at,
    //   IARG_BRANCH_TARGET_ADDR,
    //   IARG_END
    // );

    //IndirectCall -> 呼び出しアドレスがactivateされているかチェック
    if(INS_IsIndirectBranchOrCall(ins)) {
      INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)check_activated_function_address,
        IARG_BRANCH_TARGET_ADDR, //Target address of this branch instruction,
        IARG_END
      );
    }

    // return_addressをactivate
    INS_InsertCall(
      ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)activate_return_address,
      IARG_RETURN_IP, // Return address for function call, valid only at the function entry point.
      IARG_END
    );
  }

  // return_addressがactivatedかチェック
  if(INS_IsRet(ins)){
    INS_InsertCall(
      ins, IPOINT_BEFORE, (AFUNPTR)check_activated_return_address,
      IARG_BRANCH_TARGET_ADDR,
      IARG_END
    );
  }
}


static void
print_usage()
{
  std::string help = KNOB_BASE::StringKnobSummary();

  fprintf(stderr, "\nProfile call and jump targets\n");
  fprintf(stderr, "%s\n", help.c_str());
}

static void
get_symbol_table(char* binaryname){
    int fd[2], backup_stdin;
    // std::string binaryname = "./bin/spe_no_ssp2";

    pipe(fd);

    pid_t pid = fork();
    if(pid == -1){
        perror("fork");
        exit(errno);
    } else if(pid==0){
        close(fd[0]);dup2(fd[1], 1);close(fd[1]);
        execlp("nm","nm", binaryname, NULL);
        // execlp("nm","nm", binaryname.c_str(), NULL);
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

        symbol_table[AddrintFromString(address)] = label;
    }
    // wait(NULL);

    dup2(backup_stdin, 0);
    close(backup_stdin);
}

static int
get_scfg(const char* cfg_filename){
  size_t id = 1;

  std::ifstream reading_file;
  reading_file.open(cfg_filename, std::ios::in);
  if(reading_file.fail()) return 1;

  // Read from cfg-file per line
  std::string reading_line_buffer;
  while( std::getline(reading_file, reading_line_buffer)){
    std::string branch_address, target_address;
    std::stringstream ss;
    ss << reading_line_buffer;
    std::getline(ss, branch_address, ',');
    std::getline(ss, target_address);

    // Set values to each table
    branch_table[id] = Uint64FromString(branch_address);
    target_table[id] = std::make_pair(Uint64FromString(target_address), 0);
    id++;
  }

  return 0;
}

// static void
// print_results(INT32 code, void *v)
// {
//   UINT32 id;
//   ADDRINT return_addr;
//   std::map<UINT32, ADDRINT>::iterator j;

//   printf("******* CONTROL TRANSFERS *******\n");
//   for(j = ecfg.begin(); j != ecfg.end(); j++) {
//     id = j->first;
//     return_addr = j->second;
//     printf("FunctionID:%d -> ReturnAddress:[0x%08jx]\n", 
//             id, return_address);
//   }
// }

static void
print_table(INT32 code, void *v){
    ADDRINT address;
    std::string label;
    std::map<ADDRINT, std::string>::iterator i;

    printf("******* SYMBOL TABLE *******\n");
    for(i = symbol_table.begin(); i != symbol_table.end(); i++) {
        address = i->first;
        label = i->second;
        printf("address:[%08jx] -> label:[%s]\n", address, label.c_str());
    }
    printf("******* SYMBOL TABLE END *******\n");
}

static void
print_cfg(INT32 code, void *v){
  std::map<size_t, ADDRINT>::iterator i;
  std::map<size_t, std::pair<ADDRINT, size_t> >::iterator j;
  size_t id, flag;
  ADDRINT address;

  printf("******* BRANCH TABLE *******\n");
  for(i = branch_table.begin(); i != branch_table.end(); i++) {
      id = i->first;
      address = i->second;
      printf("id:[%zu] --- address:[%08jx]\n", id, address);
  }
  printf("******* BRANCH TABLE END *******\n");

  printf("******* TARGET TABLE *******\n");
  for(j = target_table.begin(); j != target_table.end(); j++) {
      id = j->first;
      address = j->second.first;
      flag = j->second.second;
      printf("id:[%zu] --- address:[%08jx] --- flag[%zu]\n", id, address, flag);
  }
  printf("******* TARGET TABLE END *******\n");
}

int
main(int argc, char *argv[])
{
  PIN_InitSymbols();
  if(PIN_Init(argc,argv)) {
    print_usage();
    return 1;
  }

  get_symbol_table(argv[6]);
  if(get_scfg("cfg/printf.txt")) return 1;

  // IMG_AddInstrumentFunction(parse_funcsyms, NULL);
  INS_AddInstrumentFunction(instrument_insn, NULL);
  // TRACE_AddInstrumentFunction(instrument_trace, NULL);
  // if(ProfileSyscalls.Value()) {
  //   PIN_AddSyscallEntryFunction(log_syscall, NULL);
  // }
  PIN_AddFiniFunction(print_table, NULL);
  PIN_AddFiniFunction(print_cfg, NULL);

  /* Never returns */
  PIN_StartProgram();
    
  return 0;
}


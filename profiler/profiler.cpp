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
// #include <iostream>
// #include <ext/stdio_filebuf.h>
// #include <cstdlib>

KNOB<bool> PrintTables(KNOB_MODE_WRITEONCE, "pintool", "q", "0", "Print tables");

std::map<ADDRINT, std::string> symbol_table;
std::map<size_t, ADDRINT> branch_table;
std::map<size_t, std::pair<ADDRINT, size_t> > target_table;

static ADDRINT entry_address;
static ADDRINT exit_address;
static bool instrument_flag = false;

static void
activate_address(ADDRINT target_address){
  std::map<size_t, std::pair<ADDRINT, size_t> >::iterator i;
  for(i=target_table.begin(); i!=target_table.end(); i++){
    if(i->second.first==target_address) i->second.second=1;
  }
}

static void
activate_return_address(ADDRINT return_address){
  activate_address(return_address);
}

static void
activate_function_address(ADDRINT instruction_address){
  std::string label = symbol_table[instruction_address];

  std::string function_name = label.substr(8);

  std::map<ADDRINT, std::string>::iterator i;
  for(i = symbol_table.begin(); i != symbol_table.end(); i++) {
    if(i->second==function_name){
      activate_address(i->first);
      return;
    }
  }
}

static void
check_activated_address(ADDRINT branch_address, ADDRINT target_address){
  std::map<size_t, ADDRINT>::iterator i;
  std::vector<size_t> tmp_id;
  for(i=branch_table.begin(); i!=branch_table.end(); i++){
    if(i->second==branch_address) tmp_id.push_back(i->first);
  }

  std::vector<size_t>::iterator j;
  for(j=tmp_id.begin(); j!=tmp_id.end(); j++){
    if(target_table[*j].first==target_address && target_table[*j].second==1){
      return;
    }
  }

  // End roop means this ControlFrow isn't in table.
  fprintf(stderr, "activateされてないですね\n");
  fprintf(stderr, "0x%08jx -> 0x%08jx\n", branch_address, target_address);
  exit(1);
}

static bool
isIndirectCall(INS ins){
  return INS_IsCall(ins) && INS_IsIndirectBranchOrCall(ins);
}

static bool
isAddressLabeledPicfi(ADDRINT instruction_address){
  std::map<ADDRINT, std::string>::iterator i;

  for(i = symbol_table.begin(); i != symbol_table.end(); i++) {
    if(i->first==instruction_address){
      if(strncmp(i->second.c_str(), "__picfi_", 8)==0){
        return true;
      }
    }
  }
  return false;
}

/*****************************************************************************
 *                         Instrumentation functions                         *
 *****************************************************************************/

static void
instrument_insn(INS ins, void *v)
{
  ADDRINT instruction_address = INS_Address(ins);

  IMG img = IMG_FindByAddress(instruction_address);
  if(!IMG_Valid(img) || !IMG_IsMainExecutable(img)) return;

  if(instruction_address==entry_address) instrument_flag=true;
  if(instruction_address==exit_address) instrument_flag=false;

  if(instrument_flag==false) return;

  if( INS_IsCall(ins) ) {
    // Activate "return_address".
    INS_InsertCall(
      ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)activate_return_address,
      IARG_RETURN_IP,
      IARG_END
    );
  }

  if( isAddressLabeledPicfi(instruction_address) ){
    // Activate "function_address".
    INS_InsertCall(
      ins, IPOINT_BEFORE, (AFUNPTR)activate_function_address,
      IARG_INST_PTR,
      IARG_END
    );
  }

  if( isIndirectCall(ins) || INS_IsRet(ins) ) {
    // Check "target_address" is activated.
    INS_InsertCall(
      ins, IPOINT_BEFORE, (AFUNPTR)check_activated_address,
      IARG_INST_PTR,
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
get_symbol_table(std::string binaryname){
  std::string cmd = "nm " + binaryname;
  FILE *fp = popen(cmd.c_str(), "r");

  char c_buf[128];
  while(fgets(c_buf, 128, fp)){
    std::string buf = c_buf;
    std::string address, label;
    std::stringstream ss;
    ss << buf;

    std::getline(ss, address, ' ');
    if(address.size()<=1) continue; //addressが明示されていないラベルの場合
    std::getline(ss, label, ' ');
    std::getline(ss, label, '\n');

    symbol_table[AddrintFromString(address)] = label;
  }
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
  if(get_scfg(argv[8])) return 1;

  entry_address = AddrintFromString(argv[9]);
  exit_address = AddrintFromString(argv[10]);

  INS_AddInstrumentFunction(instrument_insn, NULL);

    PIN_AddFiniFunction(print_table, NULL);
  if(PrintTables.Value()){
    PIN_AddFiniFunction(print_cfg, NULL);
  }

  /* Never returns */
  PIN_StartProgram();

  return 0;
}


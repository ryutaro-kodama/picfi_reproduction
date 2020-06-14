#include <stdio.h>
#include <map>
#include <string>
#include <asm-generic/unistd.h>

#include "pin.H"

#include <vector>
#include <fstream>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sstream>
#include <iostream>

// For checking tables' data.
KNOB<bool> PrintTables(KNOB_MODE_WRITEONCE, "pintool", "q", "0", "Print tables");

// This table relates address to label.
std::map<ADDRINT, std::string> symbol_table;

// These tables means static CFG.
std::map<size_t, ADDRINT> branch_table;     // <id, branch_address>
std::map<size_t, std::pair<ADDRINT, size_t> > target_table;   // <id, (target_address, flag)>

static ADDRINT entry_address;
static ADDRINT exit_address;
// If you have passed 'entry_address' and haven't 'exit_address',
// 'instrument_flag' is true and you instrument.
static bool instrument_flag = false;

// Change the flag(0 to 1) in the 'target_table'.
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

// Get function's entry address from 'instruction_address'(current program counter).
static void
activate_function_address(ADDRINT instruction_address){
  // Get label '__picfi_' related to 'instruction_address'
  std::string label = symbol_table[instruction_address];

  // Remove prefix '__picfi_' . You can get function's name(label).
  std::string function_name = label.substr(8);

  // Find function's address from its name.
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

  // Get id related to 'branch_address' from 'branch_table', and push to 'tmp_id'.
  for(i=branch_table.begin(); i!=branch_table.end(); i++){
    if(i->second==branch_address) tmp_id.push_back(i->first);
  }

  // For each id in 'tmp_id', check whether the target address is equal to 'target_address' and flag is 1.
  // If there is an all matching data, this control flow is correct one.
  std::vector<size_t>::iterator j;
  for(j=tmp_id.begin(); j!=tmp_id.end(); j++){
    if(target_table[*j].first==target_address && target_table[*j].second==1){
      return;
    }
  }

  // Exit of roop means this control flow is incorrect one.
  fprintf(stderr, "!!!!! violation detected !!!!!\n");
  // fprintf(stderr, "0x%08jx -> 0x%08jx\n", branch_address, target_address);
  exit(1);
}

static bool
isIndirectCall(INS ins){
  return INS_IsCall(ins) && INS_IsIndirectBranchOrCall(ins);
}

// Check whether current program counter is labeld "__picfi_*".
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

static int
get_symbol_table(std::string binaryname){
  std::string cmd = "nm " + binaryname;
  FILE *fp = popen(cmd.c_str(), "r");
  if(fp==NULL) return 1;

  char c_buf[128];
  // Read from 'nm' command's outputs per line
  while(fgets(c_buf, 128, fp)){
    std::string buf = c_buf;
    std::string address, label;
    std::stringstream ss;
    ss << buf;

    std::getline(ss, address, ' ');
    if(address.size()<=1) continue; // For labels which don't have address.
    std::getline(ss, label, ' ');
    std::getline(ss, label, '\n');

    // Add address and label.
    symbol_table[AddrintFromString(address)] = label;
  }

  if(pclose(fp)==-1) return 1;

  return 0;
}

static int
get_scfg(const char* cfg_filename){
  size_t id = 1;

  std::ifstream reading_file;
  reading_file.open(cfg_filename, std::ios::in);
  if(reading_file.fail()) return 1;

  // Read from 'cfg-file' per line
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

// For confirming 'symbol_table' data.
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

// For confirming 'branch_table' and 'target_table' data.
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

  if(get_symbol_table(argv[6])) return 1;
  if(get_scfg(argv[8])) return 1;

  entry_address = AddrintFromString(argv[9]);
  exit_address = AddrintFromString(argv[10]);

  INS_AddInstrumentFunction(instrument_insn, NULL);

  if(PrintTables.Value()){
    PIN_AddFiniFunction(print_table, NULL);
    PIN_AddFiniFunction(print_cfg, NULL);
  }

  /* Never returns */
  PIN_StartProgram();

  return 0;
}
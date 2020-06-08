/*
 * Symbolically execute up to and including a given jump instruction, and then compute an input
 * to take the branch direction that wasn't taken previously.
 *
 * Uses Triton's symbolic emulation mode.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../inc/loader.h"
#include "triton_util.h"
#include "disasm_util.h"

#include <string>
#include <map>
#include <vector>
#include <fstream>

#include <triton/api.hpp>
#include <triton/x86Specifications.hpp>

triton::API api;
triton::arch::registers_e ip;

std::multimap<uint64_t, uint64_t> cfg;

std::vector<std::map<triton::arch::registers_e, uint64_t>> input_regs;
std::vector<std::map<uint64_t, uint8_t>> input_mems;
std::vector<size_t> calc_model_bases;

int set_triton_arch(Binary &bin, triton::API &api, triton::arch::registers_e &ip)
{
  if(bin.arch != Binary::BinaryArch::ARCH_X86) {
    fprintf(stderr, "Unsupported architecture\n");
    return -1;
  }

  if(bin.bits == 32) {
    api.setArchitecture(triton::arch::ARCH_X86);
    ip = triton::arch::ID_REG_EIP;
  } else if(bin.bits == 64) {
    api.setArchitecture(triton::arch::ARCH_X86_64);
    ip = triton::arch::ID_REG_RIP;
  } else {
    fprintf(stderr, "Unsupported bit width for x86: %u bits\n", bin.bits);
    return -1;
  }

  return 0;
}

void set_new_input(Section *sec, size_t base)
{
  triton::ast::AstContext &ast = api.getAstContext();
  triton::ast::AbstractNode *constraint_list = ast.equal(ast.bvtrue(), ast.bvtrue());

  std::vector<bool> bool_list;

  const std::vector<triton::engines::symbolic::PathConstraint> &path_constraints = api.getPathConstraints();
  size_t height = -1;
  for(auto &pc: path_constraints) {
    if(!pc.isMultipleBranches()) continue;
    height++;
    if(height < base) continue;
    triton::ast::AbstractNode *true_constraint = ast.equal(ast.bvtrue(), ast.bvtrue());
    for(auto &branch_constraint: pc.getBranchConstraints()) {
      bool flag         = std::get<0>(branch_constraint);
      triton::ast::AbstractNode *constraint = std::get<3>(branch_constraint);

      bool_list.push_back(flag);

      if(flag) {
        // 現在通るやつ->そのまま
        true_constraint = constraint;
      } else {
        // 現在通らないやつ->そのinputを求める
        triton::ast::AbstractNode *current_constraint_list = ast.land(constraint_list, constraint);

        std::map<triton::arch::registers_e, uint64_t> new_input_reg;
        std::map<uint64_t, uint8_t> new_input_mem;
        triton::arch::registers_e triton_regnum;
        for(auto &kv: api.getModel(current_constraint_list)) {
          printf("      SymVar %u (%s) = 0x%jx\n", 
                  kv.first, 
                  api.getSymbolicVariableFromId(kv.first)->getComment().c_str(), 
                  (uint64_t)kv.second.getValue());
          // new_inputs.push_back((uint64_t)kv.second.getValue());
          const char *key = api.getSymbolicVariableFromId(kv.first)->getComment().c_str();

          triton_regnum = get_triton_regnum(key);
          if(triton_regnum == triton::arch::ID_REG_INVALID) {
            // uint64_t address = //api.getSymbolicVariableFromId(kv.first)->getComment()
            // new_input_reg[address] = (uint8_t)kv.second.getValue();
          } else {
            new_input_reg[triton_regnum] = (uint64_t)kv.second.getValue();
          }
          // printf("[%d]番目スタート[0x%jx]の値を追加\n", height+1, (uint64_t)kv.second.getValue());
        }
        input_regs.push_back(new_input_reg);
        input_mems.push_back(new_input_mem);
        calc_model_bases.push_back(height+1);
      }
    }
    constraint_list = ast.land(constraint_list, true_constraint);
  }
}

void print_cfg(){
    uint64_t branch_address, target_address;
    std::multimap<uint64_t, uint64_t>::iterator i;

    printf("******* CFG *******\n");
    for(i = cfg.begin(); i != cfg.end(); i++) {
        branch_address = i->first;
        target_address = i->second;
        printf("branch:[%llx] -> target:[%llx]\n", branch_address, target_address);
    }
    printf("******* CFG END *******\n");
}

bool is_new_cf(uint64_t branch_address, uint64_t target_address){
  // if() return false;
  return true;
}

int emulation(Section *sec, uint64_t pc, uint64_t end_address){
  bool cf_flag = false;
  uint64_t branch_address;

  while(sec->contains(pc)) {
    char mnemonic[32], operands[200];
    int len = disasm_one(sec, pc, mnemonic, operands);
    if(len <= 0) return 1;

    triton::arch::Instruction insn;
    insn.setOpcode(sec->bytes+(pc-sec->vma), len);
    insn.setAddress(pc);

    // jump先がライブラリなどだったら保留
    if(strncmp(mnemonic, "call", 4)==0){
      uint64_t jump_pc = strtoul(operands, NULL, 0);
      if(!sec->contains(jump_pc)){
        pc = insn.getNextAddress();
        continue;
      }
    }

    api.processing(insn);

    if(cf_flag){
      if(is_new_cf(branch_address, pc)){
        cfg.insert(std::make_pair(branch_address, pc));
      }
      cf_flag = false;
    }

    if(insn.isControlFlow()){
      if (strncmp (mnemonic, "ret", 3)==0 || strncmp (mnemonic, "call", 4)==0) {
        // printf("来ました[%s][%llx]\n", mnemonic, pc);
        branch_address = pc;
        cf_flag = true;
      }
    }

    if(pc==end_address){
      break;
    }

    pc = (uint64_t)api.getConcreteRegisterValue(api.getRegister(ip));
  }


  // printf("emulation終了\n");
  return 0;
}

void export_cfg(const char* filename){
  std::ofstream writing_file;
  writing_file.open(filename, std::ios::out);

  std::multimap<uint64_t, uint64_t>::iterator i;
  for(i = cfg.begin(); i != cfg.end(); i++) {
    writing_file << i->first << "," << i->second << std::endl;
  }

  writing_file.close();
}

int
main(int argc, char *argv[])
{
  Binary bin;
  std::map<triton::arch::registers_e, uint64_t> first_input_regs;
  std::map<uint64_t, uint8_t> first_input_mems;
  size_t input_index = 0;

  std::vector<triton::arch::registers_e> symregs;
  std::vector<uint64_t> symmem;

  if(argc < 6) {
    printf("Usage: %s <binary> <sym-config> <entry> <end> <output>\n", argv[0]);
    return 1;
  }

  std::string fname(argv[1]);
  if(load_binary(fname, &bin, Binary::BIN_TYPE_AUTO) < 0) return 1;

  if(set_triton_arch(bin, api, ip) < 0) return 1;
  api.enableMode(triton::modes::ALIGNED_MEMORY, true);

  if(parse_sym_config(argv[2], &first_input_regs, &first_input_mems, &symregs, &symmem) < 0) return 1;
  input_regs.push_back(first_input_regs);
  input_mems.push_back(first_input_mems);
  calc_model_bases.push_back(0);

  Section *sec = bin.get_text_section();

  while(input_index < input_regs.size() && input_index < input_mems.size()){
    api.clearPathConstraints ();
    api.concretizeAllRegister ();
    api.concretizeAllMemory ();

    for(auto &kv: input_regs[input_index]) {
      triton::arch::Register r = api.getRegister(kv.first);
      api.setConcreteRegisterValue(r, kv.second);
    }
    for(auto regid: symregs) {
      triton::arch::Register r = api.getRegister(regid);
      api.convertRegisterToSymbolicVariable(r)->setComment(r.getName());
    }
    for(auto &kv: input_mems[input_index]) {
      api.setConcreteMemoryValue(kv.first, kv.second);
    }
    for(auto memaddr: symmem) {
      api.convertMemoryToSymbolicVariable(triton::arch::MemoryAccess(memaddr, 1))->setComment(std::to_string(memaddr));
    }

    // printf("emulation開始\n");
    emulation(sec, strtoul(argv[3], NULL, 0), strtoul(argv[4], NULL, 0));
    // printf("emulation終了\n");

    set_new_input(sec, calc_model_bases[input_index]);
    print_cfg();

    input_index++;
  }

  unload_binary(&bin);

  export_cfg(argv[5]);

  return 0;
}


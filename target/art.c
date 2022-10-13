#include <ir/ir.h>
#include <target/util.h>

int PC = 0;

static void init_state_art(Data* data) {

  emit_line("section .data");

  // init reg
  emit_line("");
  for (int i = 0; i < 7; i++) {
    emit_line("reg_%s: dd 0", reg_names[i]);
  }

  // init device buffers
  emit_line("");
  emit_line("putc: dd 0");

  // init mem
  emit_line("mem:");
  inc_indent();
  int mp = 0;
  for (; data; data = data->next) {
    mp++;
    emit_line("dd 0x%x", data->v);
  }
  emit_line("times 0x%x dd 0", (1<<24) - mp + 1);

  dec_indent();
  emit_line("");

  emit_line("section .text");
  emit_line("global _start");
  emit_line("_start:");

  // Set constants
  inc_indent();
  emit_line("mov ebx, 0x1 ; fd");
  emit_line("mov edx, 1 ; len");
  emit_line("mov edi, 0");
  emit_line("mov eax, 1");
  dec_indent();

  // init jump
  emit_line("PCJMP:");
  inc_indent();
  emit_line("cmp eax, 1");
  emit_line("je _PCJMP");
  emit_line("ret");
  dec_indent();
  emit_line("_PCJMP:");
  inc_indent();
  emit_line("pop eax");
  emit_line("mov eax, [JMP_TABLE + 4*edi]");
  emit_line("sub eax, 0xabad1dea");
  emit_line("jz BB0");
  emit_line("jmp eax");
  dec_indent();
}

static void art_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("; ----- Start of %d -----", func_id);
  inc_indent();
}

static void art_emit_func_epilogue(void) {
  dec_indent();
  emit_line("; ----- End of func -----");
}

static void art_emit_pc_change(int pc) {
  PC++;
  emit_line("");
  dec_indent();
  emit_line("BB%d:", pc);
  inc_indent();
}

static void art_emit_inst(Inst* inst) {

  switch (inst->op) {
  case MOV:
    emit_line("; op: MOV");
    switch (inst->src.type){
    case REG:
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("mov dword [reg_%s], eax", reg_names[inst->dst.reg]);
      break;
    case IMM:
      emit_line("mov dword [reg_%s], 0x%x", reg_names[inst->dst.reg], inst->src.imm);
      break;
    }
    break;

  case ADD:
    emit_line("; op: ADD");
    switch (inst->src.type) {
    case REG:
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("add dword [reg_%s], eax", reg_names[inst->dst.reg]);
      break;
    case IMM:
      emit_line("add dword [reg_%s], 0x%x", reg_names[inst->dst.reg], inst->src.imm);
      break;
    }
    emit_line("and dword [reg_%s], 0xffffff", reg_names[inst->dst.reg]);
    break;

  case SUB:
    emit_line("; op: SUB");
    switch (inst->src.type) {
    case REG:
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("sub dword [reg_%s], eax", reg_names[inst->dst.reg]);
      break;
    case IMM:
      emit_line("sub dword [reg_%s], 0x%x", reg_names[inst->dst.reg], inst->src.imm);
      break;
    }
    emit_line("and dword [reg_%s], 0xffffff", reg_names[inst->dst.reg]);
    break;

  case LOAD:
    emit_line("; op: LOAD");
    switch (inst->src.type) {
    case REG:
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("mov eax, dword [mem + 4*eax]");
      break;
    case IMM:
      emit_line("mov eax, [mem + 4*0x%x]", inst->src.imm);
      break;
    }
    emit_line("mov dword [reg_%s], eax", reg_names[inst->dst.reg]);
    break;

  case STORE:
    emit_line("; op: STORE");
    emit_line("mov eax, dword [reg_%s]", reg_names[inst->dst.reg]);
    switch (inst->src.type) {
    case REG:
      emit_line("mov ecx, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("mov dword [mem + 4*ecx], eax");
      break;
    case IMM:
      emit_line("mov dword [mem + 4*0x%x], eax", inst->src.imm);
      break;
    }
    break;

  case PUTC:
    emit_line("; op: PUTC");
    switch (inst->src.type) {
    case REG:
      emit_line("mov eax, 0x4");
      emit_line("mov ecx, reg_%s", reg_names[inst->src.reg]);
      emit_line("int 0x80");
      break;
    case IMM:
      emit_line("mov dword [putc], 0x%x", inst->src.imm);
      emit_line("mov eax, 0x4");
      emit_line("mov ecx, putc");
      emit_line("int 0x80");
      break;
    }
    break;

  case GETC:
    emit_line("; op: GETC");
    emit_line("mov eax, 0x3");
    emit_line("mov ecx, reg_%s", reg_names[inst->dst.reg]);
    emit_line("int 0x80");
    break;

  case EXIT:
    emit_line("; op: EXIT");
    emit_line("mov eax, 0x1");
    emit_line("mov ebx, 0x0");
    emit_line("int 0x80");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("; op: CMP");
    switch (inst->src.type) {
    case REG:
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("cmp dword [reg_%s], eax", reg_names[inst->dst.reg]);
      break;
    case IMM:
      emit_line("cmp dword [reg_%s], 0x%x", reg_names[inst->dst.reg], inst->src.imm);
      break;
    }
    switch (inst->op) {
    case EQ: emit_line("sete  al"); break;
    case NE: emit_line("setne al"); break;
    case LT: emit_line("setb  al"); break;
    case GT: emit_line("seta  al"); break;
    case LE: emit_line("setbe al"); break;
    case GE: emit_line("setae al"); break;
    default: error("[ERROR: Line %d]", __LINE__);
    }
    emit_line("movzx eax, al", reg_names[inst->dst.reg]);
    emit_line("mov dword [reg_%s], eax", reg_names[inst->dst.reg]);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:

    emit_line("; op: JMPc");

    switch (inst->src.type) {
    case REG:
      emit_line("mov edi, dword [reg_%s]", reg_names[inst->jmp.reg]);
      emit_line("mov eax, dword [reg_%s]", reg_names[inst->src.reg]);
      emit_line("cmp dword [reg_%s], eax", reg_names[inst->dst.reg]);
      break;
    case IMM:
      emit_line("mov edi, 0x%x", inst->jmp.imm);
      emit_line("cmp dword [reg_%s], 0x%x", reg_names[inst->dst.reg], inst->src.imm);
      break;
    }
    switch (inst->op) {
    case JEQ: emit_line("sete  al"); break;
    case JNE: emit_line("setne al"); break;
    case JLT: emit_line("setb  al"); break;
    case JGT: emit_line("seta  al"); break;
    case JLE: emit_line("setbe al"); break;
    case JGE: emit_line("setae al"); break;
    default: error("[ERROR: Line %d]", __LINE__);
    }
    emit_line("call PCJMP");
    // TODO: Restore edi?
    break;

  case JMP:
    emit_line("; op: JMP");
    switch (inst->jmp.type) {
    case REG:
      emit_line("mov edi, dword [reg_%s]", reg_names[inst->jmp.reg]);
      break;
      
    case IMM:
      emit_line("mov edi, 0x%x", inst->jmp.imm);
      break;
    }
    emit_line("mov eax, 1");
    emit_line("call PCJMP");
    break;

  default:
    error("[ERROR: Line %d]", __LINE__);
  }
}

void target_art(Module* module) {
  init_state_art(module->data);

  emit_chunked_main_loop(module->text,
    art_emit_func_prologue,
    art_emit_func_epilogue,
    art_emit_pc_change,
    art_emit_inst);

  // Init jump table
  emit_line("JMP_TABLE:");
  inc_indent();
  for (int i = 0; i < PC; ++i) {
    emit_line("dd BB%d + 0xabad1dea", i);
  }
  dec_indent();
}

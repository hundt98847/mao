//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef __MAODEFS_H_INCLUDED
#define __MAODEFS_H_INCLUDED

#include <stdio.h>

// Bitmasks for operands

#define  DEF_OP0      (1 << 0)
#define  DEF_OP1      (1 << 1)
#define  DEF_OP2      (1 << 2)
#define  DEF_OP3      (1 << 3)
#define  DEF_OP4      (1 << 4)
#define  DEF_OP5      (1 << 5)

#define  DEF_OP_ALL (DEF_OP0 | DEF_OP1 | DEF_OP2 | DEF_OP3 | DEF_OP4 | DEF_OP5)

// Bitmasks for registers
class BitString {
 public:
  BitString(int index) {
    for (int i = 0; i < 4; i++) 
      word[i] = 0;
    Set(index);
  }

  void Set(int index) {
    word[index / (sizeof(unsigned long long) * 8)] |= 
      1ull << (index % (sizeof(unsigned long long) * 8));
  }  

  bool Get(int index) {
    return word[index / (sizeof(unsigned long long) * 8)] &
      (1ull << (index % (sizeof(unsigned long long) * 8)));
  }

  // Union
  BitString &operator+(const BitString &b) {
    for (int i = 0; i < 4; i++) 
      word[i] |= b.word[i];
    return *this;
  }

  // Intersect
  BitString &operator-(const BitString &b) {
    for (int i = 0; i < 4; i++) 
      word[i] &= b.word[i];
    return *this;
  }

  bool IsEmpty() {
    return !word[0] && !word[1] && !word[2] && !word[3];
  }

  bool operator==(const BitString &b) {
    return 
      word[0] == b.word[0] &&
      word[1] == b.word[1] &&
      word[2] == b.word[2] &&
      word[3] == b.word[3];
  }

  void Print() {
    fprintf(stderr, "bits: %llx:%llx:%llx:%llx\n",
	    word[3], word[2], word[1], word[0] );
  }

 private:
  unsigned long long word[4]; // 256 bits
};

// 8-bit
#define   REG_AL      (1 <<  0)
#define   REG_CL      (1 <<  1)
#define   REG_DL      (1 <<  2)
#define   REG_BL      (1 <<  3)
#define   REG_AH      (1 <<  4)
#define   REG_CH      (1 <<  5)
#define   REG_DH      (1 <<  6)
#define   REG_BH      (1 <<  7)

// 16-bit
#define   REG_AX      (1 <<  8)
#define   REG_CX      (1 <<  9)
#define   REG_DX      (1 << 10)
#define   REG_BX      (1 << 11)
#define   REG_SP      (1 << 12)
#define   REG_BP      (1 << 13)
#define   REG_SI      (1 << 14)
#define   REG_DI      (1 << 15)

// 32-bit
#define   REG_EAX     (1 << 16)
#define   REG_ECX     (1 << 17)
#define   REG_EDX     (1 << 18)
#define   REG_EBX     (1 << 19)
#define   REG_ESP     (1 << 20)
#define   REG_EBP     (1 << 21)
#define   REG_ESI     (1 << 22)
#define   REG_EDI     (1 << 23)

// 64-bit
#define   REG_RAX     (1 << 24)
#define   REG_RCX     (1 << 25)
#define   REG_RDX     (1 << 26)
#define   REG_RBX     (1 << 27)
#define   REG_RSP     (1 << 28)
#define   REG_RBP     (1 << 29)
#define   REG_RSI     (1 << 30)
#define   REG_RDI     (1 << 31)

#define   REG_RAX_ALL (REG_RAX | REG_EAX | REG_AX | RAG_AH | REG_AL)
#define   REG_RCX_ALL (REG_RCX | REG_ECX | REG_CX | RAG_CH | REG_CL)
#define   REG_RDX_ALL (REG_RDX | REG_EDX | REG_DX | RAG_DH | REG_DL)
#define   REG_RBX_ALL (REG_RBX | REG_EBX | REG_BX | RAG_BH | REG_BL)

#define   REG_EAX_ALL (REG_EAX | REG_AX | RAG_AH | REG_AL)
#define   REG_ECX_ALL (REG_ECX | REG_CX | RAG_CH | REG_CL)
#define   REG_EDX_ALL (REG_EDX | REG_DX | RAG_DH | REG_DL)
#define   REG_EBX_ALL (REG_EBX | REG_BX | RAG_BH | REG_BL)

#define   REG_AX_ALL (REG_AX | RAG_AH | REG_AL)
#define   REG_CX_ALL (REG_CX | RAG_CH | REG_CL)
#define   REG_DX_ALL (REG_DX | RAG_DH | REG_DL)
#define   REG_BX_ALL (REG_BX | RAG_BH | REG_BL)

#define   REG_R8B     (1ULL << 32)
#define   REG_R8W     (1ULL << 33)
#define   REG_R8D     (1ULL << 34)
#define   REG_R8      (1ULL << 35)

#define   REG_R9B     (1ULL << 36)
#define   REG_R9W     (1ULL << 37)
#define   REG_R9D     (1ULL << 38)
#define   REG_R9      (1ULL << 39)

#define   REG_R10B     (1ULL << 40)
#define   REG_R10W     (1ULL << 41)
#define   REG_R10D     (1ULL << 42)
#define   REG_R10      (1ULL << 43)

#define   REG_R11B     (1ULL << 44)
#define   REG_R11W     (1ULL << 45)
#define   REG_R11D     (1ULL << 46)
#define   REG_R11      (1ULL << 47)

#define   REG_R12B     (1ULL << 48)
#define   REG_R12W     (1ULL << 49)
#define   REG_R12D     (1ULL << 50)
#define   REG_R12      (1ULL << 51)

#define   REG_R13B     (1ULL << 52)
#define   REG_R13W     (1ULL << 53)
#define   REG_R13D     (1ULL << 54)
#define   REG_R13      (1ULL << 55)

#define   REG_R14B     (1ULL << 56)
#define   REG_R14W     (1ULL << 57)
#define   REG_R14D     (1ULL << 58)
#define   REG_R14      (1ULL << 59)

#define   REG_R15B     (1ULL << 60)
#define   REG_R15W     (1ULL << 61)
#define   REG_R15D     (1ULL << 62)
#define   REG_R15      (1ULL << 63)

#define   REG_ALL     (-1ULL)
// more are possible...

typedef struct DefEntry {
  int                opcode;      // matches table gen-opcodes.h
  unsigned int       op_mask;     // if insn defs operand(s)
  unsigned long long reg_mask;    // if insn defs register(s)
  unsigned long long reg_mask8;   //   for  8-bit addressing modes
  unsigned long long reg_mask16;  //   for 16-bit addressing modes
  unsigned long long reg_mask32;  //   for 32-bit addressing modes
  unsigned long long reg_mask64;  //   for 64-bit addressing modes
};
extern DefEntry def_entries[];

// external entry points
class InstructionEntry;

unsigned long long GetRegisterDefMask(InstructionEntry *insn);
void               PrintRegisterDefMask(unsigned long long mask, FILE *f);
unsigned long long GetMaskForRegister(const char *reg);
bool               DefinesSubReg64(unsigned long long pmask,
                                   unsigned long long imask);

#endif // __MAODEFS_H_INCLUDED

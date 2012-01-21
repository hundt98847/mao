//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

// Insert low-overhead nops in function prolog that can be patched at runtime to
// hijack function entries.
//
#include "Mao.h"

namespace {

PLUGIN_VERSION

static const char* kPassName = "FUNHIJACK";

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(FUNHIJACK, "Insert nops in function prolog that can be "\
                   "patched at runtime to hijack function entries", 2) {
  OPTION_BOOL("entry", false, "Enable function entries to be hijacked"),
  OPTION_BOOL("return", false, "Enable function returns to be hijacked"),
};


class EnableFunHijackPass : public MaoFunctionPass {
 public:
  EnableFunHijackPass(MaoOptionMap *options, MaoUnit *mao,
                              Function *func)
      : MaoFunctionPass(kPassName, options, mao, func) {
    hijack_fn_entry_ = GetOptionBool("entry");
    hijack_fn_return_ = GetOptionBool("return");
    Trace(1, "entry:%d return:%d", hijack_fn_entry_, hijack_fn_return_);
  }

  // Insert nop before function entry and at return points as specified by
  // options.
  bool Go() {
    bool first_instrn = true;

    FORALL_FUNC_ENTRY(function_,iter) {
      if (!(*iter)->IsInstruction()) continue;
      InstructionEntry *entry = (*iter)->AsInstruction();

      if (first_instrn) {
        first_instrn = false;

        if (hijack_fn_entry_) {
          InsertSpacesBefore();
          InstructionEntry *nop = unit_->Create2ByteNop(function_);
          entry->LinkBefore(nop);

          TraceC(1, "Inserted nop before:");
          if (tracing_level() > 0) {
            entry->PrintEntry(stderr);
          }
        }
      } else {  // check if returns need to be hijacked
        if (!hijack_fn_return_) {
          // we're done, skip over rest of instructions
          break;
        }
        if (entry->AsInstruction()->op() == OP_ret) {
          // The following is always safe, but it would be more elegant to
          // generate a 4-byte nop. Also, a possible optimization is to check
          // the instruction following a ret is not already a 4 byte nop.
          InstructionEntry *nop1 = unit_->Create2ByteNop(function_);
          InstructionEntry *nop2 = unit_->Create2ByteNop(function_);
          entry->LinkAfter(nop1);
          entry->LinkAfter(nop2);

          TraceC(1, "Inserted nop after:");
          if (tracing_level() > 0) {
            entry->PrintEntry(stderr);
          }
        }
      }
    }
    CFG::InvalidateCFG(function_);
    return true;
  }

 private:
  // Insert 5-byte space before the function. This has to be done before the
  // label for the start of the function and after all other directives.
  void InsertSpacesBefore() {
    FORALL_FUNC_ENTRY(function_, iter) {
      MaoEntry *entry = *iter;
      if (entry->IsDirective()) {
        continue;
      }
      if (entry->IsInstruction()) {
        // Expected to find a label before an instruction.
        Trace(1, "Unable to insert nops before start of function %s",
               function_->name().c_str());
        return;
      }
      if (entry->IsLabel()) {
        SubSection *ss = function_->GetSubSection();
        DirectiveEntry::OperandVector operands;
        const int size = 5;
        operands.push_back(new DirectiveEntry::Operand(size));
        DirectiveEntry *space_entry = unit_->CreateDirective(
            DirectiveEntry::SPACE, operands,
            function_, ss);
        entry->LinkBefore(space_entry);

        Trace(1, "Inserted %d bytes before function %s", size, function_->name().c_str());
        return;
      }
    }
  }


  bool  hijack_fn_entry_;
  bool  hijack_fn_return_;
};

REGISTER_PLUGIN_FUNC_PASS(kPassName, EnableFunHijackPass)
}  // namespace

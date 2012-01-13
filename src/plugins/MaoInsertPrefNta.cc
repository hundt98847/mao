//
// Copyright 2009 and later Google Inc.
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
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "Mao.h"

#include <iostream>
#include <stdio.h>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <set>

#include "libiberty.h"

namespace {

PLUGIN_VERSION

using std::insert_iterator;
using std::pair;
using std::string;

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(INSPREFNTA, "Inserts prefetches before a set of "\
                   "specified instructions", 1) {
  OPTION_STR("instn_list", "/dev/null",
	     "Filename from which to read list of file name and function name and offset pairs."),
};
// --------------------------------------------------------------------

struct InstructionSample {
  InstructionSample(const string &file_a, long offset_a) :
      file(file_a), offset(offset_a) { }

  const string file;
  const long offset;
};

struct InstructionSampleLessThan {
  bool operator()(const InstructionSample *sample1,
                  const InstructionSample *sample2) const {
    return (sample1->offset < sample2->offset);
  }
};

typedef std::set<InstructionSample *,
                 InstructionSampleLessThan> InstructionSampleSet;

// Map from function name to InstructionSample *
typedef std::map<string, InstructionSampleSet *> InstructionSampleMap;


class ListReader {
 public:
  ListReader(const char *filename)
      : filename_(filename), data_file_(NULL), buffer_(NULL),
        buffer_size_(0) { }

  bool Read(InstructionSampleMap *samples);

 private:
  bool ReadLine();
  void CleanUp();

  static const int kBufferSizeIncrement;
  const char *const filename_;
  FILE *data_file_;
  char *buffer_;
  int buffer_size_;

  ListReader(const ListReader&);
  void operator=(const ListReader&);
};

const int ListReader::kBufferSizeIncrement = BUFSIZ;

bool ListReader::ReadLine() {
  if (feof(data_file_))
    return false;

  char *ptr = buffer_;
  int amt = buffer_size_;
  while (fgets(ptr, amt, data_file_)) {
    int len = strlen(ptr);
    if (ptr[strlen(ptr) - 1] != '\n' && !feof(data_file_)) {
      // The buffer must not have been large enough, so let's grow it.
      buffer_size_ += kBufferSizeIncrement;
      buffer_ = static_cast<char *>(xrealloc(buffer_, buffer_size_));

      ptr += len;
      amt += (kBufferSizeIncrement - len);
    } else {
      return true;
    }
  }
  return false;
}

void ListReader::CleanUp() {
  if (data_file_) {
    fclose(data_file_);
    data_file_ = NULL;
  }

  if (buffer_) {
    free(buffer_);
    buffer_size_ = 0;
  }
}

bool ListReader::Read(InstructionSampleMap *samples) {
  // Open the file
  data_file_ = fopen(filename_, "r");
  if (!data_file_) {
    fprintf(stderr, "Could not open sample profile file: %s\n", filename_);
    CleanUp();
    return false;
  }

  // Initialize the buffer used to read data
  buffer_size_ = kBufferSizeIncrement;
  buffer_ = static_cast<char *>(xmalloc(buffer_size_));

  // Read the data line by line
  while (ReadLine()) {
    char *ptr, *delim, *endptr;
    int len;

    // Extract the filename
    ptr = buffer_;
    delim = strchr(ptr, '\t');
    if (!delim)
      goto parse_error;
    len = delim - ptr;
    string file(ptr, len);

    // Extract the function name
    ptr = delim+1;
    delim = strchr(ptr, '+');
    if (!delim)
      goto parse_error;
    len = delim - ptr;
    string func(ptr, len);

    // Extract the function offset
    ptr = delim+1;
    delim = strchr(ptr, '\t');
    long offset = strtoll(ptr, &endptr, 0);
    if (*endptr != '\n' && *endptr != '\0')
      goto parse_error;

    // Store the sample
    pair<InstructionSampleMap::iterator, bool> map_status =
        samples->insert(InstructionSampleMap::value_type(func, NULL));
    if (map_status.second)
      map_status.first->second = new InstructionSampleSet();

    InstructionSample *sample = new InstructionSample(file, offset);
    pair<InstructionSampleSet::iterator, bool> set_status =
        map_status.first->second->insert(sample);
    if (!set_status.second) {
      // There are two samples for the same address in the data file.
      // This should not happen, but we can gracefully deal with it
      // anyway.
      MAO_ASSERT((*set_status.first)->offset == sample->offset);
      if ((*set_status.first)->file != sample->file) {
        fprintf(stderr, "Two sample entries exist for %s+0x%ld but each "
                "refers to a different file: %s and %s\n",
                func.c_str(), sample->offset,
                (*set_status.first)->file.c_str(), sample->file.c_str());
      }
      delete sample;
    }
  }

  CleanUp();
  return true;

parse_error:
  fprintf(stderr, "Could not parse sample data file line: %s\n", buffer_);
  CleanUp();
  return false;
}

class InsertPrefetchNtaPass : public MaoPass {
 public:
  InsertPrefetchNtaPass(MaoOptionMap *options, MaoUnit *mao)
      : MaoPass("INSPREFNTA", options, mao),
        sample_profile_(GetOptionString("instn_list")) { }
  virtual ~InsertPrefetchNtaPass();
  virtual bool Go();

 private:
  void BuildFileTable();
  const string *UpdateSourceFile(MaoEntry *entry,
                                 const string *current_source_file) const;

  const char *const sample_profile_;
  InstructionSampleMap samples_;
  std::vector<string> file_table_;
};

InsertPrefetchNtaPass::~InsertPrefetchNtaPass() {
  // Free the allocated samples
  for (InstructionSampleMap::iterator map_iter = samples_.begin();
       map_iter != samples_.end(); ++map_iter) {
    for (InstructionSampleSet::iterator set_iter = map_iter->second->begin();
         set_iter != map_iter->second->end(); ++set_iter) {
      delete *set_iter;
    }
    delete map_iter->second;
  }
}

void InsertPrefetchNtaPass::BuildFileTable() {
  // The first entry of the file table should be empty.
  file_table_.push_back("");

  for (ConstSectionIterator section = unit_->ConstSectionBegin();
       section != unit_->ConstSectionEnd(); ++section) {
    for (EntryIterator entry = (*section)->EntryBegin();
         entry != (*section)->EntryEnd(); ++entry) {
      if (!(*entry)->IsDirective())
        continue;
      DirectiveEntry *directive = (*entry)->AsDirective();

      // Only process .file directives.
      if (directive->op() != DirectiveEntry::FILE)
        continue;

      // Only process ELF-style 2 operand file directives.
      if (directive->NumOperands() != 2)
        continue;

      const DirectiveEntry::Operand *number = directive->GetOperand(0);
      const DirectiveEntry::Operand *file = directive->GetOperand(1);

      if (number->type != DirectiveEntry::INT)
        continue;
      if (file->type != DirectiveEntry::STRING)
        continue;

      // Strip off the quotes around the filename.
      string filename((*file->data.str), 1, (*file->data.str).length() - 2);

      if (number->data.i == 0) {
        file_table_.push_back(filename);
      } else {
        if (number->data.i < 0) {
          fprintf(stderr, "File directive uses a negative file index: ");
          directive->PrintIR(stderr);
          fprintf(stderr, "\n");
          exit(1);
        }

        unsigned int filenum = (unsigned int)number->data.i;
        if (file_table_.size() <= filenum)
          file_table_.resize(filenum + 1);
        file_table_[filenum] = filename;
      }
    }
  }
}

const string *InsertPrefetchNtaPass::UpdateSourceFile(
    MaoEntry *entry, const string *current_source_file) const {
  if (!entry->IsDirective())
    return current_source_file;

  DirectiveEntry *directive = entry->AsDirective();
  if (directive->op() != DirectiveEntry::LOC)
    return current_source_file;

  MAO_ASSERT(directive->NumOperands() >= 1);
  const DirectiveEntry::Operand *operand = directive->GetOperand(0);
  MAO_ASSERT(operand->type == DirectiveEntry::INT);

  if (operand->data.i < 0) {
    fprintf(stderr, "Location directive uses a negative file index: ");
    directive->PrintIR(stderr);
    fprintf(stderr, "\n");
    exit(1);
  }
  unsigned int file_number = operand->data.i;

  if (file_number >= file_table_.size()) {
    fprintf(stderr, "Debug information refers to non-existant file: ");
    directive->PrintIR(stderr);
    fprintf(stderr, "\n");
    exit(1);
  }

  return &file_table_[file_number];
}


bool InsertPrefetchNtaPass::Go() {
  ListReader reader(sample_profile_);
  reader.Read(&samples_);

  BuildFileTable();
  const string *current_source_file = &file_table_[0];
	long insertions_ = 0;

  for (MaoUnit::FunctionIterator function_iter = unit_->FunctionBegin();
       function_iter != unit_->FunctionEnd(); ++function_iter) {
    // Get the samples for this function
    Function *function = *function_iter;
    InstructionSampleMap::iterator function_samples_iterator =
        samples_.find(function->name());
    if (function_samples_iterator == samples_.end())
      continue;

    // Get the size map for this function
    Section *section = function->GetSection();
    MaoEntryIntMap *sizes = MaoRelaxer::GetSizeMap(unit_, section);

    // For each sample, attribute it to the corresponding instruction
    InstructionSampleSet *function_samples = function_samples_iterator->second;
    long offset = 0;
    EntryIterator entry_iter = function->EntryBegin();
    current_source_file = UpdateSourceFile(*entry_iter, current_source_file);

    for (InstructionSampleSet::iterator sample = function_samples->begin();
         sample != function_samples->end(); ++sample) {
      while (offset < (*sample)->offset && entry_iter != function->EntryEnd()) {
        int size = (*sizes)[*entry_iter];

        offset += size;
        ++entry_iter;
		// current_source_file = UpdateSourceFile(*entry_iter, current_source_file);
      }

      while (offset == (*sample)->offset) {
        // Only annotate profiles on to instructions with matching filenames.
		// if ((*entry_iter)->Type() == MaoEntry::INSTRUCTION &&  (*current_source_file) == (*sample)->file) {
        if ((*entry_iter)->Type() == MaoEntry::INSTRUCTION ) {

			  	if (!(*entry_iter)->IsInstruction()) continue;
		        InstructionEntry *insn = (*entry_iter)->AsInstruction();

		         if (insn->NumOperands()>1 && insn->IsMemOperand(0)) {
	//		 		insn->PrintEntry();
		           InstructionEntry *pref = unit_->CreatePrefetch(function, 0,insn,0, 0);
		           insn->LinkBefore(pref);
		           ++insertions_;
		         }

		         else if (insn->NumOperands()>1 && insn->IsMemOperand(1)) {
	//		 		insn->PrintEntry();
		           InstructionEntry *pref = unit_->CreatePrefetch(function, 0,insn,1, 0);
		           insn->LinkBefore(pref);
		           ++insertions_;
		         }
        }

        int size = (*sizes)[*entry_iter];
        offset += size;
        ++entry_iter;
		// current_source_file = UpdateSourceFile(*entry_iter, current_source_file);
      }
    }
  }

//	std::cout<<"Total Insertions:"<<insertions_<<std::endl;
  return true;
}

REGISTER_PLUGIN_UNIT_PASS("INSPREFNTA", InsertPrefetchNtaPass)
}  // namespace

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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <iostream>

#include "MaoDebug.h"
#include "MaoUnit.h"

extern "C" const char *S_GET_NAME(symbolS *s);

//
// Class: MaoUnit
//

// Default to no subsection selected
// A default will be generated if necessary later on.
MaoUnit::MaoUnit()
    : current_subsection_(0) {
  entry_vector_.clear();
  entry_list_.clear();
  sub_sections_.clear();
  sections_.clear();
  bb_list_.clear();
  bb_vector_.clear();
}

MaoUnit::~MaoUnit() {
  // Remove entries and free allocated memory
  for (std::list<MaoUnitEntryBase *>::iterator iter =
           entry_list_.begin();
      iter != entry_list_.end(); ++iter) {
    delete *iter;
  }

  // Remove subsections and free allocated memory
  for (std::vector<SubSection *>::iterator iter =
           sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    delete *iter;
  }

  // Remove sections and free allocated memory
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
      iter != sections_.end(); ++iter) {
    delete iter->second;
  }

  // Remove basicblocks and free allocated memory
  for (std::vector<BasicBlockEdge *>::iterator iter =
           bb_edges_.begin();
      iter != bb_edges_.end(); ++iter) {
    delete *iter;
  }

  // Remove basicblocks and free allocated memory
  for (std::list<BasicBlock *>::iterator iter =
           bb_list_.begin();
      iter != bb_list_.end(); ++iter) {
    delete *iter;
  }
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

// Prints all entries in the MaoUnit
void MaoUnit::PrintMaoUnit(FILE *out) const {
  for (std::list<MaoUnitEntryBase *>::const_iterator iter =
           entry_list_.begin();
      iter != entry_list_.end(); ++iter) {
    MaoUnitEntryBase *e = *iter;
    e->PrintEntry(out);
  }
}

void MaoUnit::PrintIR() const {
  PrintIR(stdout);
}


void MaoUnit::PrintIR(FILE *out) const {
  // Print the entries
  for (std::list<MaoUnitEntryBase *>::const_iterator e_iter =
           entry_list_.begin();
       e_iter != entry_list_.end(); ++e_iter) {
    MaoUnitEntryBase *e = *e_iter;
    fprintf(out, "[%5d][%c] ", e->index(), e->GetDescriptiveChar());
    if (MaoUnitEntryBase::INSTRUCTION == e->entry_type()) {
      fprintf(out, "\t");
    }
    e->PrintIR(out);
    fprintf(out, "\n");
  }

  // Print the sections
  unsigned int index = 0;
  fprintf(out, "Sections : \n");
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
      iter != sections_.end(); ++iter) {
    Section *section = iter->second;
    fprintf(out, "[%3d] %s [", index, section->name());
    // Print out the subsections in this section as a list of indexes
    std::vector<subsection_index_t> *subsection_indexes =
        section->GetSubSectionIndexes();
    for (std::vector<subsection_index_t>::const_iterator si_iter =
             subsection_indexes->begin();
         si_iter != subsection_indexes->end(); ++si_iter) {
    fprintf(out, " %d", *si_iter);
    }
    fprintf(out, "]\n");
    index++;
  }

  // Print the subsections
  index = 0;
  fprintf(out, "Subsections : \n");
  for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    SubSection *ss = *iter;
    fprintf(out, "[%3d] [%d-%d]: %s (%s)\n", index,
            ss->first_entry_index(), ss->last_entry_index(), ss->name(),
            ss->creation_op());
    index++;
  }
}

Section *MaoUnit::FindOrCreateAndFind(const char *section_name) {
  Section *section;
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // create it!
    section = new Section(section_name);
    sections_[section->name()] = section;
  } else {
    section = it->second;
  }
  MAO_ASSERT(section);
  return section;
}

// Called when found a new subsection reference in the assembly.
void MaoUnit::SetSubSection(const char *section_name,
                            unsigned int subsection_number,
                            const char *creation_op) {
  // Get (and possibly create) the section
  Section *section = FindOrCreateAndFind(section_name);
  MAO_ASSERT(section);
  // Create a new subsection, even if the same subsection number
  // have already been used.
  SubSection *subsection = new SubSection(subsection_number, section_name,
                                          creation_op);
  sub_sections_.push_back(subsection);
  section->AddSubSectionIndex(sub_sections_.size()-1);

  // set current_subsection!
  current_subsection_ = subsection;

  current_subsection_->set_first_entry_index(entry_list_.size());
  current_subsection_->set_last_entry_index(entry_list_.size());
}


BasicBlock *MaoUnit::CreateAndAddBasicblock(
    const char *label,
    std::list<MaoUnitEntryBase *>::iterator start_iterator,
    subsection_index_t end_index) {
  // TODO(martint): add assert to make sure label does not alerady exist as
  //                a basic block.
  BasicBlock *bb = new BasicBlock(label);
  bb->set_last_entry_index(end_index);
  bb->set_first_iter(start_iterator);
  bb_list_.push_back(bb);
  bb_vector_.push_back(bb);
  bb->set_index(bb_vector_.size()-1);
  return bb;
}

BasicBlock *MaoUnit::GetBasicBlockByID(const basicblock_index_t id) const {
  MAO_ASSERT(id < bb_vector_.size());
  return bb_vector_[id];
}

MaoUnitEntryBase *MaoUnit::GetEntryByID(const entry_index_t id) const {
  MAO_ASSERT(id < entry_vector_.size());
  MAO_ASSERT(entry_vector_[id] != 0);
  return entry_vector_[id];
}

BasicBlockEdge *MaoUnit::CreateBasicBlockEdge(
    const basicblock_index_t source_index,
    const basicblock_index_t target_index,
    const bool fallthrough) {
  BasicBlockEdge *bbe = new BasicBlockEdge();
  bbe->set_source_index(source_index);
  bbe->set_target_index(target_index);
  bbe->set_fallthrough(fallthrough);
  bb_edges_.push_back(bbe);
  return bbe;
}

void MaoUnit::RegisterBasicBlockEdge(BasicBlockEdge *bbe,
                                     const bool register_source,
                                     const bool register_target) {
  MAO_ASSERT(register_source || register_target);
  if (register_source) {
    BasicBlock *bb = GetBasicBlockByID(bbe->source_index());
    MAO_ASSERT(bb);
    bb->AddOutEdge(bbe);
  }
  if (register_target) {
    BasicBlock *bb = GetBasicBlockByID(bbe->target_index());
    MAO_ASSERT(bb);
    bb->AddInEdge(bbe);
  }
}

BasicBlockEdge *MaoUnit::AddBasicBlockEdge(basicblock_index_t source_index,
                                           basicblock_index_t target_index,
                                           bool fallthrough) {
  BasicBlockEdge *bbe = CreateBasicBlockEdge(source_index, target_index,
                                             fallthrough);
  RegisterBasicBlockEdge(bbe, true, true);
  return bbe;
}


bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                       bool create_default_section) {
  return AddEntry(entry, build_sections, create_default_section,
                  entry_list_.end());
}

// Add an entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                       bool create_default_section,
                       std::list<MaoUnitEntryBase *>::iterator list_iter) {
  // Variables that _might_ get used.
  LabelEntry *label_entry;
  Symbol *symbol;

  // next free ID for the entry
  entry_index_t entry_index = entry_vector_.size();

  MAO_ASSERT(entry);
  entry->set_index(entry_index);

  // Create a subsection if necessary
  if (build_sections && (create_default_section && !current_subsection_)) {
    SetSubSection(DEFAULT_SECTION_NAME, 0, DEFAULT_SECTION_CREATION_OP);
    MAO_ASSERT(current_subsection_);
  }

  // Check the type
  switch (entry->entry_type()) {
    case MaoUnitEntryBase::INSTRUCTION:
      break;
    case MaoUnitEntryBase::LABEL:
      // A Label will generate in a new symbol in the symbol table
      label_entry = (LabelEntry *)entry;
      symbol = symbol_table_.FindOrCreateAndFind(label_entry->name());
      MAO_ASSERT(symbol);
      // TODO(martint): The codes does not currently set the correct
      //                section for all labels in the symboltable.
      break;
    case MaoUnitEntryBase::DEBUG:
      break;
    case MaoUnitEntryBase::DIRECTIVE:
      break;
    case MaoUnitEntryBase::UNDEFINED:
      break;
    default:
      // should never happen. Catch all cases above.
      MAO_RASSERT_MSG(0, "Entry type not recognised.");
  }

  // Add the entry to the compilation unit
  entry_vector_.push_back(entry);
  entry_list_.insert(list_iter, entry);

  // Update subsection information
  if (build_sections && current_subsection_) {
    current_subsection_->set_last_entry_index(entry_index);
  }

  return true;
}


// Adds a common symbol
bool MaoUnit::AddCommSymbol(const char *name, unsigned int common_size,
                            unsigned int common_align) {
  // A common symbol is different as it allows
  // several definitions of the symbol.
  // See http://sourceware.org/binutils/docs-2.19/as/Comm.html#Comm
  Symbol *symbol;
  if ( !symbol_table_.Exists(name) ) {
    // If symbol does not exists,
    // insert it. with default properties
    symbol = symbol_table_.Add(name, new Symbol(name));
    symbol->set_symbol_type(OBJECT_SYMBOL);
  } else {
    // Get the symbol
    symbol = symbol_table_.Find(name);
  }
  // Set the attributes associated with common symbols
  symbol->set_common(true);
  if (symbol->common_size() < common_size) {
    symbol->set_common_size(common_size);
    MAO_ASSERT(symbol->size() < symbol->common_size());
    symbol->set_size(symbol->common_size());
  }
  if (symbol->common_align() < common_align) {
    symbol->set_common_align(common_align);
  }
  return true;
}

const char *MaoUnit::GetCurrentLabelIfAny(const entry_index_t index) {
  MaoUnitEntryBase *entry = GetEntryByID(index);
  if (MaoUnitEntryBase::LABEL == entry->entry_type()) {
    LabelEntry *le = (LabelEntry *)entry;
    MAO_ASSERT(le);
    return le->name();
  }
  return 0;
}

void MaoUnit::UpdateLabel2BasicblockMap(labels2basicblock_map_t *labels2bb,
                                        BasicBlock *bb) {
  unsigned int offset = 0;
  std::list<MaoUnitEntryBase *>::iterator it = bb->first_iter();

  // For each label in the basic block, add the ID of the basic block and the
  // offset into the basic block to the map
  while (0 == *it || (*it)->index() != bb->last_entry_index()) {
    if (*it) {
      // process *it
      if (MaoUnitEntryBase::LABEL == (*it)->entry_type()) {
        LabelEntry *le = (LabelEntry *)(*it);
        MAO_ASSERT(le);
        std::pair<basicblock_index_t, unsigned int> p(bb->index(), offset);
        (*labels2bb)[le->name()] = p;
      }
    }
    offset++;
    it++;
  }
  // process *it
  if (MaoUnitEntryBase::LABEL == (*it)->entry_type()) {
    LabelEntry *le = (LabelEntry *)(*it);
    MAO_ASSERT(le);
    std::pair<basicblock_index_t, unsigned int> p(bb->index(), offset);
    (*labels2bb)[le->name()] = p;
  }
}


void MaoUnit::ClearOldLabel2BasicblockMap(labels2basicblock_map_t *labels2bb,
                                          BasicBlock *bb) {
  std::list<MaoUnitEntryBase *>::iterator it = bb->first_iter();
  // For each label in the basic block, add the ID of the basic block and the
  // offset into the basic block to the map
  while ((*it)->index() != bb->last_entry_index()) {
    MAO_ASSERT((*it));
    // process *it
    if (MaoUnitEntryBase::LABEL == (*it)->entry_type()) {
      LabelEntry *le = (LabelEntry *)(*it);
      MAO_ASSERT(le);
      std::map<const char *,
          std::pair<basicblock_index_t, unsigned int>, ltstr>::iterator l_it =
          labels2bb->find(le->name());
      if (l_it != labels2bb->end()) {
        // remove it!
        labels2bb->erase(l_it);
      }
    }
    it++;
  }
  // process *it
  if (MaoUnitEntryBase::LABEL == (*it)->entry_type()) {
    if (MaoUnitEntryBase::LABEL == (*it)->entry_type()) {
      LabelEntry *le = (LabelEntry *)(*it);
      MAO_ASSERT(le);
      std::map<const char *,
          std::pair<basicblock_index_t, unsigned int>, ltstr>::iterator l_it =
          labels2bb->find(le->name());
      if (l_it != labels2bb->end()) {
        // remove it!
        labels2bb->erase(l_it);
      }
    }
  }
}



bool MaoUnit::GetBasicblockFromLabel(const char * label,
                                     const labels2basicblock_map_t *labels2bb,
                                     std::pair<basicblock_index_t, unsigned int>
                                     &out_pair) {
  std::map<const char *,
      std::pair<basicblock_index_t, unsigned int>, ltstr>::const_iterator it =
      labels2bb->find(label);
  if (it == labels2bb->end()) {
    return false;
  }
  out_pair = it->second;
  return true;
}

void MaoUnit::MoveBasicBlockEdgeSource(BasicBlockEdge *bbe,
                         const basicblock_index_t new_basicblock_source_index) {
  MAO_TRACE("Move Edge (%d -> %d) to (%d -> %d)", bbe->source_index(),
            bbe->target_index(), new_basicblock_source_index,
            bbe->target_index());
  // Remove it from the original basic block
  BasicBlock *old_bb = GetBasicBlockByID(bbe->source_index());
  old_bb->RemoveOutEdge(bbe);
  // Change the actual label.
  bbe->set_source_index(new_basicblock_source_index);
  // Add it to the new basic block
  RegisterBasicBlockEdge(bbe, true, false);
}


// Split basic block at the offset given.
void MaoUnit::SplitBasicBlock(basicblock_index_t basicblock_index,
                              unsigned int offset, labels2basicblock_map_t &labels2bb) {
  MAO_TRACE("SplitBasicBlock: Split %s at offset %d",
            GetBasicBlockByID(basicblock_index)->label(),
            offset);

  // The original basic block. Will also be used for the first of the two new
  // basic blocks.
  BasicBlock *orig_bb = GetBasicBlockByID(basicblock_index);

  MAO_ASSERT(offset > 0);
  MAO_ASSERT(offset < (orig_bb->NumberOfEntries()-1));

  // The new basic block
  BasicBlock *new_bb = 0;

  // Iterator that will point at the end of the second basic block
  std::list<MaoUnitEntryBase *>::iterator end_it = orig_bb->first_iter();
  // Iterator that will point to the first entry int he second basic block
  std::list<MaoUnitEntryBase *>::iterator break_it;
  unsigned int entry_count = 0;
  MAO_ASSERT(*end_it != 0);
  while ((*end_it)->index() != orig_bb->last_entry_index()) {
    if (entry_count == offset) {
      break_it = end_it;
    }
    entry_count++;
    end_it++;
    MAO_ASSERT(*end_it != 0);
  }
  // now end_it points to the last entry
  MAO_ASSERT(*break_it);
  MAO_ASSERT(*end_it);
  MAO_TRACE("break id: %d", (*break_it)->index());
  MAO_TRACE("end id: %d", (*end_it)->index());

  // Currently only support breaking at labels!
  MAO_ASSERT((*break_it)->entry_type() == MaoUnitEntryBase::LABEL);
  const char *new_label_name = GetCurrentLabelIfAny((*break_it)->index());
  MAO_ASSERT(new_label_name);

  // what is the end index of the new_bb??
  new_bb = CreateAndAddBasicblock(new_label_name, break_it, (*end_it)->index());
  break_it--;
  orig_bb->set_last_entry_index((*break_it)->index());
  break_it++;

  // Update in and out edges of the basic blocks
  // Move the out edges from the old basic block to the new basic block
  std::vector<BasicBlockEdge *> outedges = orig_bb->GetOutEdges();
  for (std::vector<BasicBlockEdge *>::iterator iter = outedges.begin();
       iter != outedges.end(); ++iter) {
    MoveBasicBlockEdgeSource(*iter, new_bb->index());
  }
  // Create a new edge to link the two basic blocks
  AddBasicBlockEdge(orig_bb->index(),
                    new_bb->index(),
                    true);

  // Now we should update the labels in the map if necessary!
  // ID and offset must change!
  // 1 remove the stale labels
  ClearOldLabel2BasicblockMap(&labels2bb, new_bb);
  // 2 re-add the labels
  UpdateLabel2BasicblockMap(&labels2bb, new_bb);


  MAO_TRACE("Finished splitting");
}


void MaoUnit::PrintLabels2BasicBlock(labels2basicblock_map_t &labels2bb) {
  PrintLabels2BasicBlock(stdout, labels2bb);
}


void MaoUnit::PrintLabels2BasicBlock(FILE *out,
                                     labels2basicblock_map_t &labels2bb) {
  for (std::map<const char *,
           std::pair<basicblock_index_t, unsigned int>, ltstr>::const_iterator
           iter = labels2bb.begin();
       iter != labels2bb.end();
       iter++) {
    MAO_TRACE("Label: %s, Basic block:%d", (*iter).first, (*iter).second.first);
  }
}


void MaoUnit::BuildCFG() {
  // Need to hold a temporary list of future targets!
  // each target has the following information
  //  - name of target, which basic block jumps to this block
  std::multimap<const char *, basicblock_index_t, ltstr> future_targets;
  future_targets.clear();

  // Holds a map that maps between labels and basic blocks
  // The map holds the ID of the basic block, and the offset of the label into
  // the basic block.
  // TODO(martint): it would make more sence to have a way to identify if
  //                the label identifies the top of this basic block or not.
  //                Currently, only one label is being identified with the
  //                start of the given basic block.
  labels2basicblock_map_t labels2bb;
  labels2bb.clear();

  // Used when building the basic blocks
  // Points to the current basic block, or NULL if none.
  BasicBlock *current_basicblock = 0;
  // If not null, then a fallthrough edge should be created
  // when a new basic block is build.
  BasicBlockEdge *fallthrough = 0;

  // Add the source and sink BB
  // index 0 = source
  BasicBlock *bb = CreateAndAddBasicblock("source", entry_list_.end(), 0);
  bb->set_source(true);
  // TODO(martint): Add a label from every exit-point to the sink.
  // index 1 = sink
  bb = CreateAndAddBasicblock("sink", entry_list_.end(), 0);
  bb->set_sink(true);

  // Initial fallthrough edge from the source basic block
  fallthrough = new BasicBlockEdge();
  fallthrough->set_source_index(0);

  // Iterate over entries
  for (std::list<MaoUnitEntryBase *>::iterator e_iter =
           entry_list_.begin();
       e_iter != entry_list_.end(); ++e_iter) {
    MaoUnitEntryBase *entry = *e_iter;

    //fprintf(stderr, "Working on: ");
    //entry->PrintEntry(stderr);

    // Check if entry is a LABEL and a Future Edge exists for this label
    // If that is true, this label should en the previous basic block
    if (entry->entry_type() == MaoUnitEntryBase::LABEL) {
      const char *label_name = GetCurrentLabelIfAny(entry->index());
      MAO_ASSERT(label_name);
      if (future_targets.count(label_name) > 0) {
        //  - create new fallthrough edge
        MAO_ASSERT(fallthrough == 0);
        if (current_basicblock) {
          fallthrough = new BasicBlockEdge();
          fallthrough->set_source_index(current_basicblock->index());
        }
        //  - update label2basic blocks for the prev. block
        UpdateLabel2BasicblockMap(&labels2bb, current_basicblock);
        //  - set current_basicblock to 0 so a new one can be created
        current_basicblock = 0;
      }
    }

    // Entry belongs in basic block, process it.
    if (entry->BelongsInBasicBlock()) {
      // If we are not currently processing a basic block, create a new one.
      if (!current_basicblock) {
        // Get the name of the current label, or a generated one if needed
        const char *label_name = GetCurrentLabelIfAny((*e_iter)->index());
        // If no label exists for this basic block, createa a new one.
        if (!label_name) {
          label_name = BBNameGen::GetUniqueName();
          AddEntry(new LabelEntry(label_name, 0, 0), false, false, e_iter);
          // Move iterator so it points to our new label.
          e_iter--;
          entry = *e_iter;
        }
        MAO_ASSERT(label_name);
        // Create the new basic block.
        // Assume the basic block is only one entry long. This newly created
        // basic block is updated later when more instructions are found
        current_basicblock = CreateAndAddBasicblock(label_name,
                                                     e_iter,
                                                     (*e_iter)->index());

        // Check for future edges to this entry
        // For each one found, create the edges
        if (entry->entry_type() == MaoUnitEntryBase::LABEL) {
          MAO_ASSERT(label_name);
          std::multimap<const char *, basicblock_index_t, ltstr>::iterator iter;
          while ((iter = future_targets.find(label_name)) !=
                 future_targets.end()) {
            MAO_TRACE("Future tragets found in BuildCFG %s %d ",
                      (*iter).first, (*iter).second);
            // create the new edges!
            AddBasicBlockEdge((*iter).second ,
                              current_basicblock->index(),
                              false);
            future_targets.erase(iter);
          }
        }

        // check if we have a current fallthrough edge to this block.
        if (fallthrough) {
          fallthrough->set_target_index(current_basicblock->index());
          AddBasicBlockEdge(fallthrough->source_index(),
                            fallthrough->target_index(),
                            true);
          delete fallthrough;
          fallthrough = 0;
        }
      } else {
        // Update the current basic block to also include this entry.
        current_basicblock->set_last_entry_index((*e_iter)->index());
      }
    }

    // This forces a new basic block to be created when the next
    // entry that belongs in a basic block is encountered.
    if (entry->EndsBasicBlock()) {
      // Here we can check if the current instruction suggests that a
      // fallthrough edge should be created
      if (MaoUnitEntryBase::INSTRUCTION == entry->entry_type()) {
        InstructionEntry *ie = (InstructionEntry *)entry;
        if (ie->HasFallthrough()) {
          MAO_ASSERT(!fallthrough);
          fallthrough = new BasicBlockEdge();
          fallthrough->set_source_index(current_basicblock->index());
        }
      }

      // When the basicblock ends, add all labels in the block to the map
      UpdateLabel2BasicblockMap(&labels2bb, current_basicblock);

      // If the current instruction jumps to a label:
      //   1. label exists and is the first entry in a basic block
      //        - add edge
      //   2. label exists but is in the middle of a basic block
      //        - split the basic block at the label..
      //        - update edges, labels, and fallthrough
      //   3. a label we have not seen yet (a future label)
      //        - keep track of future labels, and from where we jump using
      //          the multimap future_edges.
      //      search the map when adding new labels so we can add the edges
      //      as we find them.

      if (MaoUnitEntryBase::INSTRUCTION == entry->entry_type()) {
        InstructionEntry *ie = (InstructionEntry *)entry;
        if (ie->HasTarget()) {
          // Target is  ie->GetTarget()
          std::pair<basicblock_index_t, unsigned int> pair;
          if (GetBasicblockFromLabel(ie->GetTarget(), &labels2bb, pair)) {
            // We found the target. it does already exists
            if (0 == pair.second) {
              // Target is the first entry of the basic block
              AddBasicBlockEdge(current_basicblock->index(),
                                pair.first,
                                false);
            } else {
              // Target is in the middle of the basic block
              SplitBasicBlock(pair.first, pair.second, labels2bb);
            }
          } else {
            // Target not yet processed. Add it to the future targets
            future_targets.insert(
                std::pair<const char *, basicblock_index_t>(ie->GetTarget(),
                                                  current_basicblock->index()));
          }
        }
      }
      // Get ready for the next basic block.
      current_basicblock = 0;
    }
    //fprintf(stderr, "After Processing - CurrentCFG: ");
    //PrintCFG();
    //fprintf(stderr, "\n\n\n");
  }
  // Dont forget the final basic block.
  if (current_basicblock) {
    UpdateLabel2BasicblockMap(&labels2bb, current_basicblock);
  }

  if (current_basicblock && fallthrough) {
    AddBasicBlockEdge(current_basicblock->index(),
                      1,  // The sink has index 1
                      true);
    current_basicblock = 0;
  }

  if (fallthrough) {
    // Add the sink
    delete fallthrough;
  }

  // print out future targets
  MAO_ASSERT_MSG(future_targets.size() == 0,
                 "Not all future targets were resolved");
}

void MaoUnit::PrintBasicBlocks() {
  PrintBasicBlocks(stdout);
}

void MaoUnit::PrintBasicBlocks(FILE *out) {
  // Print the basic blocks
  fprintf(out, "Basic blocks:\n");
  for (std::vector<BasicBlock *>::iterator iter =
           bb_vector_.begin();
       iter != bb_vector_.end(); ++iter) {
    BasicBlock *bb = *iter;
    if (bb) {
      (*iter)->PrintInfo(out);
    } else {
      fprintf(out, "<DELETED>\n");
    }
  }
}

void MaoUnit::PrintBasicBlockEdges() {
  PrintBasicBlocks(stdout);
}

void MaoUnit::PrintBasicBlockEdges(FILE *out) {
  // Print the basic blocks
  fprintf(out, "Basic block edges:\n");
  for (std::vector<BasicBlockEdge *>::iterator iter =
           bb_edges_.begin();
       iter != bb_edges_.end(); ++iter) {
    BasicBlockEdge *bbe = *iter;
    if (bbe) {
      fprintf(out, " bb%d(%s) -> bb%d(%s) %s\n", bbe->source_index(),
              GetBasicBlockByID(bbe->source_index())->label(),
              bbe->target_index(),
              GetBasicBlockByID(bbe->target_index())->label(),
              bbe->fallthrough()?"[Fallthrough]":"");
    } else {
      fprintf(out, "<DELETED>\n");
    }
  }
}

void MaoUnit::PrintCFG() {
  PrintCFG(stdout);
}


void MaoUnit::PrintCFG(FILE *out) {
  // Use the IR to build the CFG
  PrintBasicBlocks(out);
  PrintBasicBlockEdges(out);
}


int MaoUnit::BBNameGen::i = 0;
const char *MaoUnit::BBNameGen::GetUniqueName() {
  char *buff = strdup(".mao_label_XXXX");
  MAO_ASSERT(i <= 9999);
  sprintf(buff, ".mao_label_%04d", i);
  i++;
  return buff;
}


//
// Class: AsmInstruction
//

AsmInstruction::AsmInstruction(i386_insn *instruction) {
  MAO_ASSERT(instruction);
  instruction_ = CreateInstructionCopy(instruction);
}

AsmInstruction::~AsmInstruction() {
  MAO_ASSERT(instruction_);
  FreeInstruction();
}

const char *AsmInstruction::GetOp() const {
  MAO_ASSERT(instruction_);
  MAO_ASSERT(instruction_->tm.name);
  return(instruction_->tm.name);
}

// This deallocates memory allocated in CreateInstructionCopy().
void AsmInstruction::FreeInstruction() {
  MAO_ASSERT(instruction_);
  for (unsigned int i = 0; i < instruction_->operands; i++) {
    if (IsImmediateOperand(instruction_, i)) {
      free(instruction_->op[i].imms);
    }
    if (IsMemOperand(instruction_, i)) {
      free(instruction_->op[i].disps);
    }
    if (IsRegisterOperand(instruction_, i)) {
      free((reg_entry *)instruction_->op[i].regs);
    }
  }
  for (unsigned int i = 0; i < 2; i++) {
    free((seg_entry *)instruction_->seg[i]);
  }
  free((reg_entry *)instruction_->base_reg);
  free((reg_entry *)instruction_->index_reg);
  free(instruction_);
}


// Given a register, create a copy to be used in our instruction
reg_entry *AsmInstruction::CopyRegEntry(const reg_entry *in_reg) {
  if (!in_reg)
    return 0;
  reg_entry *tmp_r;
  tmp_r = (reg_entry *)malloc(sizeof(reg_entry) );
  MAO_ASSERT(tmp_r);
  MAO_ASSERT(strlen(in_reg->reg_name) < kMaxRegisterNameLength);
  tmp_r->reg_name = strdup(in_reg->reg_name);
  tmp_r->reg_type = in_reg->reg_type;
  tmp_r->reg_flags = in_reg->reg_flags;
  tmp_r->reg_num = in_reg->reg_num;
  return tmp_r;
}


bool AsmInstruction::IsMemOperand(const i386_insn *instruction,
                                  const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.disp8
          || t.bitfield.disp16
          || t.bitfield.disp32
          || t.bitfield.disp32s
          || t.bitfield.disp64
          || t.bitfield.baseindex);
}

bool AsmInstruction::IsImmediateOperand(const i386_insn *instruction,
                                        const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
      return (t.bitfield.imm1
              || t.bitfield.imm8
              || t.bitfield.imm8s
              || t.bitfield.imm16
              || t.bitfield.imm32
              || t.bitfield.imm32s
              || t.bitfield.imm64);
}

bool AsmInstruction::IsRegisterOperand(const i386_insn *instruction,
                                       const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.reg8
          || t.bitfield.reg16
          || t.bitfield.reg32
          || t.bitfield.reg64);
}

// Prints out the instruction. This is work-in-progress, but currently
// supports the assembly instructions found in the assembled version
// of mao. Please add functionality when unsupported instructions are found.
void AsmInstruction::PrintInstruction(FILE *out) const {
  int scale[] = { 1, 2, 4, 8 };

  if (((strcmp(instruction_->tm.name, "movsbl") == 0) ||
       (strcmp(instruction_->tm.name, "movswl") == 0) ||
       (strcmp(instruction_->tm.name, "movzbl") == 0) ||
       (strcmp(instruction_->tm.name, "movzwl") == 0) ||
       (strcmp(instruction_->tm.name, "movswl") == 0) ||
       (strcmp(instruction_->tm.name, "cmovl") == 0) ||
       (strcmp(instruction_->tm.name, "cmovnl") == 0) ||
       (strcmp(instruction_->tm.name, "cwtl") == 0) ||
       (strcmp(instruction_->tm.name, "cltd") == 0)) &&
       (instruction_->suffix == 'l')) {
    // these functions should not have the prefix written out as its already in
    // the name property.
    fprintf(out, "\t%s\t", instruction_->tm.name);
  } else {
    fprintf(out, "\t%s%c\t", instruction_->tm.name,
            (instruction_->suffix &&
             instruction_->tm.name[strlen(instruction_->tm.name)-1] != 'q') ?
            instruction_->suffix : ' ');
     // 'grep q\" i386-tbl.h' lists the instruction that ends with q.
  }

  for (unsigned int i =0; i < instruction_->operands; i++) {
    // Segment overrides are always placed in seg[0], even
    // if it applies to the second operand.
    if (IsMemOperand(instruction_, i) && instruction_->seg[0]) {
      fprintf(out, "%%%s:", instruction_->seg[0]->seg_name);
    }
    if (IsImmediateOperand(instruction_, i)) {
      if (instruction_->op[i].imms->X_op == O_constant) {
        // The cast allows us to compile and run for targets
        // configured to either 32 bit or 64 bit architectures.
        fprintf(out, "$%lld",
                (long long)instruction_->op[i].imms->X_add_number);
      }
      if (instruction_->op[i].imms->X_op == O_symbol) {
        fprintf(out, "$%s",
                S_GET_NAME(instruction_->op[i].imms->X_add_symbol));
        if (instruction_->op[i].imms->X_add_number) {
          fprintf(out, "%s%d",
                  (((int)instruction_->op[i].imms->X_add_number)<0)?"":"+",
                  (int)instruction_->op[i].imms->X_add_number);
        }
      }
    }
    // This case is now handles in the displacement
//     if (instruction_->tm.opcode_modifier.jump ||
//         instruction_->tm.opcode_modifier.jumpdword ||
//         instruction_->tm.opcode_modifier.jumpbyte) {
//       if (instruction_->op[i].disps->X_op == O_symbol)
//         fprintf(out, "%s",
//                 S_GET_NAME(instruction_->op[i].disps->X_add_symbol) );
//       else
//         fprintf(out, "*unk*");
//     }
    if (IsMemOperand(instruction_, i)) {
      // symbol?
      if (instruction_->types[i].bitfield.disp8 ||
          instruction_->types[i].bitfield.disp16 ||
          instruction_->types[i].bitfield.disp32 ||
          instruction_->types[i].bitfield.disp32s ||
          instruction_->types[i].bitfield.disp64) {
        if (instruction_->op[i].disps->X_op == O_symbol) {
          fprintf(out, "%s",
                  S_GET_NAME(instruction_->op[i].disps->X_add_symbol) );
        }
        if (instruction_->op[i].disps->X_op == O_symbol &&
            instruction_->op[i].disps->X_add_number != 0) {
          fprintf(out, "+");
        }
        if (instruction_->types[i].bitfield.jumpabsolute) {
          fprintf(out, "*");
        }
        if (instruction_->op[i].disps->X_add_number != 0) {
          // The cast allows us to compile and run for targets
          // configured to either 32 bit or 64 bit architectures.
          fprintf(out, "%lld",
                  (long long)instruction_->op[i].disps->X_add_number);
        }
      }
      if (instruction_->base_reg || instruction_->index_reg)
        fprintf(out, "(");
      if (instruction_->base_reg)
        fprintf(out, "%%%s", instruction_->base_reg->reg_name);
      if (instruction_->index_reg)
        fprintf(out, ",%%%s", instruction_->index_reg->reg_name);
      if (instruction_->log2_scale_factor)
        fprintf(out, ",%d", scale[instruction_->log2_scale_factor]);
      if (instruction_->base_reg || instruction_->index_reg)
        fprintf(out, ")");
    }
    if (instruction_->types[i].bitfield.acc) {
      // need to find out size in order to print the correct ACC register!
      if (instruction_->types[i].bitfield.byte )
        fprintf(out, "%%al");
      if (instruction_->types[i].bitfield.word )
        fprintf(out, "%%ax");
      if (instruction_->types[i].bitfield.dword )
        fprintf(out, "%%eax");
      if (instruction_->types[i].bitfield.qword )
        fprintf(out, "%%rax");
    }
// TODO(martint): fix floatacc
// if ((instruction_->types[i].bitfield.floatacc) {
// }

    if (IsRegisterOperand(instruction_, i)) {
      if (instruction_->types[i].bitfield.jumpabsolute) {
        fprintf(out, "*");
      }
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }
    if (instruction_->types[i].bitfield.shiftcount) {
      // its a register name!
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }

    if (i < instruction_->operands-1)
      fprintf(out, ", ");
  }
}

// From an instruction given by gas, allocate new memory and populate the
// members.
i386_insn *AsmInstruction::CreateInstructionCopy(i386_insn *in_inst) {
  i386_insn *new_inst = (i386_insn *)malloc(sizeof(i386_insn) );
  MAO_ASSERT(new_inst);

  // Template related members
  new_inst->tm = in_inst->tm;
  new_inst->suffix = in_inst->suffix;
  new_inst->operands = in_inst->operands;
  new_inst->reg_operands = in_inst->reg_operands;
  new_inst->disp_operands = in_inst->disp_operands;
  new_inst->mem_operands = in_inst->mem_operands;
  new_inst->imm_operands = in_inst->imm_operands;

  // Types
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    for (unsigned int j = 0; j < OTNumOfUints; j++) {
      new_inst->types[i].array[j] = in_inst->types[i].array[j];
    }
  }

  // Ops
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    // Copy the correct part of the op[i] union
    if (IsImmediateOperand(in_inst, i)) {
      new_inst->op[i].imms = (expressionS *)malloc(sizeof(expressionS) );
      MAO_ASSERT(new_inst->op[i].imms);
      new_inst->op[i].imms->X_op = in_inst->op[i].imms->X_op;

      // TODO(martint): Check if we need to allocate memory for these instead
      // of just blindly copying them.
      new_inst->op[i].imms->X_add_number = in_inst->op[i].imms->X_add_number;
      new_inst->op[i].imms->X_add_symbol = in_inst->op[i].imms->X_add_symbol;
    }

//     if (in_inst->tm.opcode_modifier.jump ||
//         in_inst->tm.opcode_modifier.jumpdword ||
//         in_inst->tm.opcode_modifier.jumpbyte) {
//       // TODO(martint): make sure the full contents off disps is copied
//         new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS) );
//         new_inst->op[i].disps->X_add_symbol =
//            in_inst->op[i].disps->X_add_symbol; // ERROR
//         new_inst->op[i].disps->X_add_number =
//            in_inst->op[i].disps->X_add_number; // ERROR
//         new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
//     }

    if (IsMemOperand(in_inst, i)) {
      if (in_inst->op[i].disps) {
        new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS));
        MAO_ASSERT(new_inst->op[i].disps);
        new_inst->op[i].disps->X_add_symbol =
            in_inst->op[i].disps->X_add_symbol;
        new_inst->op[i].disps->X_add_number =
            in_inst->op[i].disps->X_add_number;
        new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
      } else {
        new_inst->op[i].disps = 0;
      }
    }
    if (IsRegisterOperand(in_inst, i)) {
      new_inst->op[i].regs = CopyRegEntry(in_inst->op[i].regs);
    }
    if (in_inst->types[i].bitfield.shiftcount) {
      new_inst->op[i].regs = CopyRegEntry(in_inst->op[i].regs);
    }
  }
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    new_inst->flags[i]= in_inst->flags[i];
  }
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    new_inst->reloc[i] = in_inst->reloc[i];
  }
  new_inst->base_reg = CopyRegEntry(in_inst->base_reg);
  new_inst->index_reg = CopyRegEntry(in_inst->index_reg);
  new_inst->log2_scale_factor = in_inst->log2_scale_factor;

  // Segment overrides
  for (unsigned int i = 0; i < 2; i++) {
    if (in_inst->seg[i]) {
      seg_entry *tmp_seg = (seg_entry *)malloc(sizeof(seg_entry) );
      MAO_ASSERT(tmp_seg);
      MAO_ASSERT(strlen(in_inst->seg[i]->seg_name) < MAX_SEGMENT_NAME_LENGTH);
      tmp_seg->seg_name = strdup(in_inst->seg[i]->seg_name);
      tmp_seg->seg_prefix = in_inst->seg[i]->seg_prefix;
      new_inst->seg[i] = tmp_seg;
    } else {
      new_inst->seg[i] = 0;
    }
  }

  // Prefixes
  new_inst->prefixes = in_inst->prefixes;
  for (unsigned int i = 0; i < 2; i++) {
    new_inst->prefix[i] = in_inst->prefix[i];
  }

  new_inst->rm = in_inst->rm;
  new_inst->rex = in_inst->rex;
  new_inst->sib = in_inst->sib;
  new_inst->drex = in_inst->drex;
  new_inst->vex = in_inst->vex;

  return new_inst;
}

bool AsmInstruction::EndsBasicBlock() const {
  if (!strcmp(GetOp(), "ret")  || !strcmp(GetOp(), "hlt")) {
    return true;
  }
  // TODO(martint): Create a setting which controls
  // if call ends basic blocks.
  if (!strcmp(GetOp(), "call") || !strcmp(GetOp(), "lcall")) {
    return false;
  }

  if (instruction_->tm.opcode_modifier.jump ||
      instruction_->tm.opcode_modifier.jumpdword ||
      instruction_->tm.opcode_modifier.jumpbyte) {
    return true;
  }
  return false;
}

bool AsmInstruction::IsInList(const char *string, const char *list[],
                              const unsigned int number_of_elements) const {
  for (unsigned int i = 0; i < number_of_elements; i++) {
    if (0 == strcmp(string, list[i]))
      return true;
  }
  return false;
}

const char *cond_jumps[] = {
// Conditional jumps.
"jo", "jno", "jb", "jc", "jnae", "jnb", "jnc", "jae", "je", "jz", "jne", "jnz",
"jbe", "jna", "jnbe", "ja", "js", "jns", "jp", "jpe", "jnp", "jpo", "jl",
"jnge", "jnl", "jge", "jle", "jng", "jnle", "jg",

// jcxz vs. jecxz is chosen on the basis of the address size prefix.
"jcxz", "jecxz", "jecxz", "jrcxz"

// loop variants
"loop", "loopz", "loope", "loopnz", "loopne"
};


bool AsmInstruction::HasFallthrough() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  if (IsInList(GetOp(), cond_jumps, sizeof(cond_jumps)/sizeof(char *))) {
    return true;
  }

  return false;
}

bool AsmInstruction::HasTarget() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  const char *insn[] = {"jmp", "jmpl"};
  if (IsInList(GetOp(), insn, sizeof(insn)/sizeof(char *)))
    return true;
  if (IsInList(GetOp(), cond_jumps, sizeof(cond_jumps)/sizeof(char *)))
    return true;

  return false;
}

const char *AsmInstruction::GetTarget() const {
  //
  for (unsigned int i =0; i < instruction_->operands; i++) {
    if (IsMemOperand(instruction_, i)) {
      // symbol?
      if (instruction_->types[i].bitfield.disp8 ||
          instruction_->types[i].bitfield.disp16 ||
          instruction_->types[i].bitfield.disp32 ||
          instruction_->types[i].bitfield.disp32s ||
          instruction_->types[i].bitfield.disp64) {
        if (instruction_->op[i].disps->X_op == O_symbol) {
          return S_GET_NAME(instruction_->op[i].disps->X_add_symbol);
        }
      }
    }
  }
  return "<UKNOWN>";
}

//
// Class: Directive
//
Directive::Directive(const char *key, const char *value) {
  MAO_ASSERT(key);
  MAO_ASSERT(value);
  MAO_ASSERT(strlen(key) < MAX_DIRECTIVE_NAME_LENGTH);
  key_ = strdup(key);
  MAO_ASSERT(strlen(value) < MAX_DIRECTIVE_NAME_LENGTH);
  value_ = strdup(value);
}

Directive::~Directive() {
  MAO_ASSERT(key_);
  free(key_);
  MAO_ASSERT(value_);
  free(value_);
}

const char *Directive::key() const {
  MAO_ASSERT(key_);
  return key_;
};

const char *Directive::value() const {
  MAO_ASSERT(value_);
  return value_;
}

//
// Class: Label
//

Label::Label(const char *name) {
  MAO_ASSERT(name);
  MAO_ASSERT(strlen(name) < MAX_SEGMENT_NAME_LENGTH);
  name_ = strdup(name);
}

Label::~Label() {
  MAO_ASSERT(name_);
  free(name_);
}

const char *Label::name() const {
  MAO_ASSERT(name_);
  return name_;
}

//
// Class: MaoUnitEntryBase
//

MaoUnitEntryBase::MaoUnitEntryBase(unsigned int line_number,
                                   const char *line_verbatim) :
    line_number_(line_number),
    index_(0) {
  if (line_verbatim) {
    MAO_ASSERT(strlen(line_verbatim) < MAX_VERBATIM_ASSEMBLY_STRING_LENGTH);
    line_verbatim_ = strdup(line_verbatim);
  } else {
    line_verbatim_ = 0;
  }
}

MaoUnitEntryBase::~MaoUnitEntryBase() {
}


void MaoUnitEntryBase::Spaces(unsigned int n, FILE *outfile) const {
  for (unsigned int i = 0; i < n; i++) {
    fprintf(outfile, " ");
  }
}

//
// Class: LabelEntry
//

LabelEntry::LabelEntry(const char *name, unsigned int line_number,
                       const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  label_ = new Label(name);
}

LabelEntry::~LabelEntry() {
  delete label_;
}

void LabelEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(label_);
  fprintf(out, "%s:", label_->name() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void LabelEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(label_);
  fprintf(out, "%s", label_->name() );
}


MaoUnitEntryBase::EntryType LabelEntry::entry_type() const {
  return LABEL;
}

const char *LabelEntry::name() {
  return label_->name();
}

//
// Class: DirectiveEntry
//

DirectiveEntry::DirectiveEntry(const char *key, const char *value,
                               unsigned int line_number,
                               const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  directive_ = new Directive(key, value);
}

DirectiveEntry::~DirectiveEntry() {
  delete directive_;
}

void DirectiveEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DirectiveEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "%s %s", directive_->key(),
          directive_->value());
}


MaoUnitEntryBase::EntryType DirectiveEntry::entry_type() const {
  return DIRECTIVE;
}


//
// Class: DebugEntry
//

DebugEntry::DebugEntry(const char *key, const char *value,
                       unsigned int line_number, const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  directive_ = new Directive(key, value);
}

DebugEntry::~DebugEntry() {
  delete directive_;
}

void DebugEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DebugEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "%s %s", directive_->key(),
          directive_->value());
}


MaoUnitEntryBase::EntryType DebugEntry::entry_type() const {
  return DEBUG;
}


//
// Class: InstructionEntry
//

InstructionEntry::InstructionEntry(i386_insn *instruction,
                                   unsigned int line_number,
                                   const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  instruction_ = new AsmInstruction(instruction);
}

InstructionEntry::~InstructionEntry() {
  delete instruction_;
}

void InstructionEntry::PrintEntry(FILE *out) const {
  instruction_->PrintInstruction(out);
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}


void InstructionEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(instruction_);
  instruction_->PrintInstruction(out);
}


MaoUnitEntryBase::EntryType InstructionEntry::entry_type() const {
  return INSTRUCTION;
}


//
// Class: Section
//

Section::Section(const char *name) {
  MAO_ASSERT(name);
  MAO_ASSERT(strlen(name) < MAX_SEGMENT_NAME_LENGTH);
  name_ = strdup(name);
}

const char *Section::name() const {
  MAO_ASSERT(name_);
  return name_;
}

Section::~Section() {
  MAO_ASSERT(name_);
  free(name_);
}

void Section::AddSubSectionIndex(subsection_index_t index) {
  sub_section_indexes_.push_back(index);
}

//
// Class: SubSection
//

SubSection::SubSection(unsigned int subsection_number, const char *name,
                       const char *creation_op) :
    number_(subsection_number),
    first_entry_index_(0),
    last_entry_index_(0) {
  MAO_ASSERT(creation_op);
  creation_op_ = strdup(creation_op);  // this way free() works
  MAO_ASSERT(name);
  name_ = strdup(name);
}

unsigned int SubSection::number() const {
  return number_;
}

const char *SubSection::name() const {
  return name_;
}

const char *SubSection::creation_op() const {
  MAO_ASSERT(creation_op_);
  return creation_op_;
}

SubSection::~SubSection() {
  MAO_ASSERT(name_);
  free(name_);

  MAO_ASSERT(creation_op_);
  free(creation_op_);
}

//
// Class: BasicBlock
//

BasicBlock::BasicBlock(const char *label)
    : source_(false),
      sink_(false),
      index_(0),
      label_(label) {
  //  Constructor
  in_edges_.clear();
  out_edges_.clear();
}


const char *BasicBlock::label() const {
  MAO_ASSERT(label_);
  return label_;
}


void BasicBlock::AddInEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  in_edges_.push_back(edge);
}

void BasicBlock::RemoveInEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  for (std::vector<BasicBlockEdge *>::iterator iter =
           in_edges_.begin();
       iter != in_edges_.end(); ++iter) {
    if (*iter == edge) {
      in_edges_.erase(iter);
      return;
    }
  }
  MAO_ASSERT_MSG(0, "Could not find edge to remove");
}

void BasicBlock::AddOutEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  out_edges_.push_back(edge);
}

void BasicBlock::RemoveOutEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  for (std::vector<BasicBlockEdge *>::iterator iter =
           out_edges_.begin();
       iter != out_edges_.end(); ++iter) {
    if (*iter == edge) {
      out_edges_.erase(iter);
      return;
    }
  }
  MAO_ASSERT_MSG(0, "Could not find edge to remove");
}


unsigned int  BasicBlock::NumberOfEntries() const {
  std::list<MaoUnitEntryBase *>::const_iterator iter = first_iter();
  unsigned int entry_count = 0;
  MAO_ASSERT(*iter != 0);
  while ((*iter)->index() != last_entry_index()) {
    entry_count++;
    iter++;
    MAO_ASSERT(*iter != 0);
  }
  return entry_count;
}


void BasicBlock::PrintInfo() const {
  PrintInfo(stdout);
}

void BasicBlock::PrintInfo(FILE *out) const {
  fprintf(out, "bb%d:%s %20s ", index(),
          index()<10 ? "  " : index() < 100 ? " " : "", label());
  if (!source() && !sink()) {
    fprintf(out, " range:%3d-%3d",
            (*first_iter())->index(), last_entry_index());
  } else {
    fprintf(out, "              ");
  }
  if (in_edges_.size()) {
    fprintf(out, " in:  ");
    for (std::vector<BasicBlockEdge *>::const_iterator iter =
           in_edges_.begin();
         iter != in_edges_.end(); ++iter) {
      printf("bb%d ", (*iter)->source_index());
    }
  }
  if (out_edges_.size()) {
    fprintf(out, " out: ");
    for (std::vector<BasicBlockEdge *>::const_iterator iter =
           out_edges_.begin();
         iter != out_edges_.end(); ++iter) {
      printf("bb%d ", (*iter)->target_index());
    }
  }
  fprintf(out, "\n");
}

//
// Class: BasicBlockEdge
//

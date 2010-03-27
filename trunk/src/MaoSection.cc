//
// Copyright 2010 Google Inc.
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

#include "MaoEntry.h"
#include "MaoSection.h"

//
// Class: SectionIterator
//

Section *SectionIterator::operator *() const {
  return section_iter_->second;
}

SectionIterator &SectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

SectionIterator &SectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool SectionIterator::operator ==(const SectionIterator &other) const {
  return (section_iter_ == other.section_iter_);
}

bool SectionIterator::operator !=(const SectionIterator &other) const {
  return !((*this) == other);
}


//
// Class: ConstSectionIterator
//

Section * const ConstSectionIterator::operator *() const {
  return section_iter_->second;
}

ConstSectionIterator const &ConstSectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

ConstSectionIterator const &ConstSectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool ConstSectionIterator::operator ==(const ConstSectionIterator &other)
    const {
  return (section_iter_ == other.section_iter_);
}

bool ConstSectionIterator::operator !=(const ConstSectionIterator &other)
    const {
  return !((*this) == other);
}


//
// Class: SubSection
//

void SubSection::set_last_entry(MaoEntry *entry) {
  // Link the entries, unless we insert the first entry. This special case is
  // handled in AddEntry().
  if (entry != first_entry_) {
    last_entry_->set_next(entry);
    entry->set_prev(last_entry_);
  }
  last_entry_ = entry;
}


EntryIterator SubSection::EntryBegin() {
  return EntryIterator(first_entry());
}

EntryIterator SubSection::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return EntryIterator(entry);
}



//
// Class: Section
//

void Section::AddSubSection(SubSection  *subsection) {
  subsections_.push_back(subsection);
}


std::vector<SubSectionID> Section::GetSubsectionIDs() const {
  std::vector<SubSectionID> subsections;
  for (std::vector<SubSection *>::const_iterator ss_iter = subsections_.begin();
       ss_iter != subsections_.end();
       ++ss_iter) {
    subsections.push_back((*ss_iter)->id());
  }
  return subsections;
}

EntryIterator Section::EntryBegin() const {
  if (subsections_.size() == 0)
    return EntryEnd();
  SubSection *ss = subsections_[0];
  return EntryIterator(ss->first_entry());
}

EntryIterator Section::EntryEnd() const {
  return EntryIterator(NULL);
}

SubSection *Section::GetLastSubSection() const {
  if (subsections_.size() == 0) {
    return NULL;
  } else {
    return subsections_[subsections_.size()-1];
  }
}

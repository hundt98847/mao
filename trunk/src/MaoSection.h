//
// Copyright 2010 Google Inc.
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

#ifndef MAOSECTION_H_
#define MAOSECTION_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "MaoTypes.h"
#include "MaoUtil.h"

// Forward declarations
class MaoEntry;
class EntryIterator;

// Global Map Types
typedef std::map<MaoEntry *, int> MaoEntryIntMap;

class Section;
// A Subsection is part of a section. The subsection concept allows the assembly
// file to write the code more freely, but still keep the data organized in
// sections.
class SubSection {
 public:
  // Constructor needs subsection number, a pointer to the actual section, and
  // the assembly code needed to create the subsection.
  explicit SubSection(const SubSectionID id, unsigned int subsection_number,
                      const char *name, Section *section)
      : number_(subsection_number),
        name_(name),
        id_(id),
        first_entry_(NULL),
        last_entry_(NULL),
        start_section_(false),
        section_(section) { }

  ~SubSection() {}

  unsigned int number() const { return number_; }
  const std::string &name() const { return name_; }

  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  void set_first_entry(MaoEntry *entry) { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry);
  SubSectionID id() const { return id_;}

  void set_start_section(bool value) {start_section_ = value;}
  bool start_section() const {return start_section_;}

  Section *section() const {return section_;}

  EntryIterator EntryBegin();
  EntryIterator EntryEnd();

 private:
  // The subsection number
  const unsigned int number_;
  const std::string name_;

  const SubSectionID id_;

  // Points to the first and last entry for the subsection.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;

  // For special section that holds the inital directies that are not
  // part of the first "real" section.
  bool start_section_;

  // Points to the section
  Section *section_;
};

// A section
class Section {
 public:
  // Memory for the name is allocated in the constructor and freed
  // in the destructor.
  explicit Section(const char *name, const SectionID id) :
    name_(name), id_(id), sizes_(NULL), offsets_(NULL) {}
  ~Section() {}
  std::string name() const {return name_;}
  SectionID id() const {return id_;}

  void AddSubSection(SubSection *subsection);

  EntryIterator EntryBegin() const;
  EntryIterator EntryEnd() const;

  std::vector<SubSectionID> GetSubsectionIDs() const;

  // Return the last subsection in the section or NULL if section is empty.
  SubSection *GetLastSubSection() const;

  // The next 4 methods are only to be used by MaoRelaxer.  Others
  // should invoke the utility functions there to get access to the
  // size and offset.
  MaoEntryIntMap *sizes() {return sizes_;}
  MaoEntryIntMap *offsets() {return offsets_;}
  void set_sizes(MaoEntryIntMap *sizes) {
    if (sizes_ != NULL)
      delete sizes_;
    sizes_ = sizes;
  }
  void set_offsets(MaoEntryIntMap *offsets) {
    if (offsets_ != NULL)
      delete offsets_;
    offsets_ = offsets;
  }

 private:

  const std::string name_;  // e.g. ".text", ".data", etc.
  const SectionID id_;

  std::vector<SubSection *> subsections_;

  // Sizes as determined by the relaxer
  // TODO(martint): fix types to use the named type in relax.h
  // NULL if not set.
  MaoEntryIntMap *sizes_;

  // Store corresponding entry offsets
  MaoEntryIntMap *offsets_;
};
// Iterator wrapper for iterating over all the Sections in a MaoUnit.
class SectionIterator {
 public:
  SectionIterator(std::map<const char *, Section *, ltstr>::iterator
                  section_iter)
      : section_iter_(section_iter) { }
  Section *operator *() const;
  SectionIterator &operator ++();
  SectionIterator &operator --();
  bool operator ==(const SectionIterator &other) const;
  bool operator !=(const SectionIterator &other) const;
 private:
  std::map<const char *, Section *, ltstr>::iterator section_iter_;
};

class ConstSectionIterator {
 public:
  ConstSectionIterator(std::map<const char *, Section *, ltstr>::const_iterator
                       section_iter)
      : section_iter_(section_iter) { }
  Section *const operator *() const;
  ConstSectionIterator const &operator ++();
  ConstSectionIterator const &operator --();
  bool operator ==(const ConstSectionIterator &other) const;
  bool operator !=(const ConstSectionIterator &other) const;
 private:
  std::map<const char *, Section *, ltstr>::const_iterator section_iter_;
};



#endif // MAOSECTION_H_

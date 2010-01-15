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

#ifndef MAOUTIL_H_
#define MAOUTIL_H_

#include <string.h>

#include <cstdarg>
#include <string>

#include "MaoDebug.h"

// Used by the STL-maps of sections and subsections.
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }

  bool operator()(std::string s1, std::string s2) const {
    return strcmp(s1.c_str(), s2.c_str()) < 0;
  }
};

// BitString implementation.
//
class BitString {
 public:
  explicit BitString(int number_of_bits) {
    InitObj(number_of_bits);
  }

  // Defaults to 256 bits.
  BitString() {
    InitObj(256);
  }


  // Create a bitstring and initialize it with the given "unsigned long long"
  // words. It is the users responsibility to make sure that the number of words
  // given is large enough (but not larger) to fix the bits. Any extra bits in
  // the last word must be zero, or the function will exit.
  BitString(int number_of_bits, int number_of_words, ...) {
     va_list vl;

     number_of_words_ = number_of_words;
     number_of_bits_ = number_of_bits;

     word_ = new unsigned long long[number_of_words_];

     va_start(vl, number_of_words);
     for (int i = 0; i < number_of_words_; ++i) {
       word_[i] = va_arg(vl, unsigned long long);
     }
     va_end(vl);

     // Verify that the invariances we keep for this classholds.
     VerifyBitString();
  }

  // Copy constructor performs a deep copy.
  BitString(const BitString& b) {
    CopyObj(b);
  }

  // Assignment performs a deep copy.
  BitString& operator = (const BitString& other) {
    CopyObj(other);
    return *this;
  }

  ~BitString() {
    MAO_ASSERT(word_);
    delete[] word_;
  }

  void Set(int index) {
    word_[index / (sizeof(unsigned long long) * 8)] |=
        1ULL << (index % (sizeof(unsigned long long) * 8));
  }

  void Clear(int index) {
    word_[index / (sizeof(unsigned long long) * 8)] &=
        ~(1ULL << (index % (sizeof(unsigned long long) * 8)));
  }

  bool Get(int index) const {
    return word_[index / (sizeof(unsigned long long) * 8)] &
        (1ULL << (index % (sizeof(unsigned long long) * 8)));
  }

  int NextSetBit(int from_index) {
    unsigned int word_pos = from_index/(sizeof(unsigned long long) * 8);
    unsigned int bit_pos = from_index%(sizeof(unsigned long long) * 8);
    while ((int) word_pos < number_of_words_) {
      while (word_[word_pos] && (bit_pos < (sizeof(unsigned long long) * 8))) {
        if (word_[word_pos]&(1ULL << bit_pos))
          return word_pos*sizeof(unsigned long long)*8+bit_pos;
        bit_pos++;
      }
      word_pos++;
      bit_pos = 0;
    }
    return -1;
  }

  unsigned long long GetWord(int index) {
    MAO_ASSERT(index < number_of_words_);
    return word_[index];
  }

  // Union
  BitString operator |(const BitString &b) const {
    MAO_ASSERT(b.number_of_bits() == number_of_bits_);
    BitString bs_new(number_of_bits_);
    for (int i = 0; i < number_of_words_; ++i) {
      bs_new.word_[i] = word_[i] | b.word_[i];
    }
    return bs_new;
  }

  // Intersect
  BitString operator &(const BitString &b) const {
    MAO_ASSERT(b.number_of_bits() == number_of_bits_);
    BitString bs_new(number_of_bits_);
    for (int i = 0; i < number_of_words_; ++i) {
      bs_new.word_[i] = word_[i] & b.word_[i];
    }
    return bs_new;
  }

  // Flip the bits
  BitString operator ~() const {
    BitString bs_new(number_of_bits_);
    for (int i = 0; i < number_of_words_; ++i) {
      bs_new.word_[i] = ~word_[i];
    }
    // Make sure unused bits are set to zero to
    // make equality checks easier.
    bs_new.ClearUnusedBits();
    return bs_new;
  }

  // Remove bits
  BitString operator -(const BitString &b) const {
    MAO_ASSERT(b.number_of_bits() == number_of_bits_);
    BitString bs_new(number_of_bits_);
    for (int i = 0; i < number_of_words_; ++i) {
      bs_new.word_[i] = word_[i] & ~b.word_[i];
    }
    return bs_new;
  }

  bool operator == (const BitString &b) const {
    MAO_ASSERT(b.number_of_bits() == number_of_bits_);
    for (int i = 0; i < number_of_words_; ++i) {
      if (word_[i] != b.word_[i])
        return false;
    }
    return true;
  }

  bool operator != (const BitString &b) const {
    return !(*this == b);
  }

  bool IsNull() const {
    for (int i = 0; i < number_of_words_; ++i) {
      if (word_[i] != 0)
        return false;
    }
    return true;
  }

  bool IsNonNull() const {
    for (int i = 0; i < number_of_words_; ++i) {
      if (word_[i] != 0)
        return true;
    }
    return false;
  }

  bool IsUndef() const {
    for (int i = 0; i < number_of_words_; ++i) {
      if (word_[i] != (-1ULL))
        return false;
    }
    return true;
  }

  void SetUndef() {
    for (int i = 0; i < number_of_words_; ++i) {
      word_[i] = (-1ULL);
    }
  }

  void Print() const {
    fprintf(stderr, "bits: ");
    for (int i = 0; i < number_of_words_; ++i) {
      fprintf(stderr, "%016llx ", word_[i]);
    }
    fprintf(stderr, "\n");
  }

  void ToString(char *s, int max_size) const {
    MAO_ASSERT(s != NULL);
    MAO_ASSERT(max_size > 0);
    char *sp = s;
    int space_left = max_size;
    int n = snprintf(sp, space_left, "bits: ");
    space_left -= n;
    sp += n;
    for (int i = 0; i < number_of_words_; ++i) {
       n = snprintf(sp, space_left, "%016llx ", word_[i]);
       if (n >= space_left) {
         // Output was truncated, no more space left.
         space_left = 0;
         break;
       }
       space_left -= n;
       sp += n;
    }
    snprintf(sp, space_left, "\n");
  }

  void PrintInitializer(FILE *f) const {
    if (IsNull()) {
      fprintf(f, "BNULL");
    } else {
      fprintf(f, "BitString(%d, %d, ", number_of_bits_ , number_of_words_);
      for (int i = 0; i < number_of_words_; ++i) {
        if (i > 0)
          fprintf(f, ", ");
        fprintf(f, "0x%llxULL", word_[i]);
      }
      fprintf(f, ")");
    }
  }

  int number_of_bits() const {return number_of_bits_;}

 private:
  // Implementation assumes that unused bits in word_ are always set to 0.
  // Special case is the Undefined value, then all words are set to -1ULL.
  unsigned long long *word_;
  // Number of bits in the bit-string.
  int number_of_bits_;
  // Number of words used to represent the bit-string.
  int number_of_words_;

  void CopyObj(const BitString& b) {
    number_of_bits_ = b.number_of_bits_;
    number_of_words_ = b.number_of_words_;
    word_ = new unsigned long long[number_of_words_];
    for (int i = 0; i < number_of_words_; ++i)
      word_[i] = b.word_[i];
  }

  void InitObj(int number_of_bits) {
    MAO_ASSERT(number_of_bits > 0);
    number_of_bits_ = number_of_bits;
    number_of_words_ = (number_of_bits-1)/(sizeof(unsigned long long) * 8)
        + 1;
    word_ = new unsigned long long[number_of_words_];
    for (int i = 0; i < number_of_words_; i++)
      word_[i] = 0;
  }

  // This makes sure that unused bits in the array are set to zero.
  // Keeping them zero simplifies the implementation of the following functions:
  //  - NextSetBit(), ==, IsNull(), IsNonNull()
  void ClearUnusedBits() {
    // Zero the unused bits.
    int unused_bits = sizeof(unsigned long long)*8 -
        number_of_bits_ % (sizeof(unsigned long long)*8);
    for (int i = 0; i < unused_bits; ++i) {
      Clear(number_of_bits_ + i);
    }
  }

  void VerifyBitString() {
    // Check relation between number_of_bits_ and number_of_words_.
#ifdef NDEBUG
    int bits_per_word = sizeof(unsigned long long) * 8;
    MAO_ASSERT(((number_of_words_ * bits_per_word) >= number_of_bits_)
               && (((number_of_words_ - 1) * bits_per_word) < number_of_bits_));
#endif
    if (!IsUndef()) {
      // Assert that all unused bits are zero.
      int unused_bits = sizeof(unsigned long long)*8 -
          number_of_bits_ % (sizeof(unsigned long long)*8);
      unused_bits = unused_bits % (sizeof(unsigned long long)*8);
      for (int i = 0; i < unused_bits; ++i) {
        MAO_ASSERT(Get(number_of_bits_ + i) == false);
      }
    }
  }
};

#endif  // MAOUTIL_H_

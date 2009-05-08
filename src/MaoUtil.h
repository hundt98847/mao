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

// BitString implementation, 256 bits
//
class BitString {
 public:
  BitString(int index) {
    for (int i = 0; i < 4; i++)
      word[i] = 0;
    Set(index);
  }
  BitString() {
    for (int i = 0; i < 4; i++)
      word[i] = 0;
  }
  BitString(unsigned long long w0, unsigned long long w1,
            unsigned long long w2, unsigned long long w3) {
    word[0] = w0;
    word[1] = w1;
    word[2] = w2;
    word[3] = w3;
  }

  void Set(int index) {
    word[index / (sizeof(unsigned long long) * 8)] |=
      1ull << (index % (sizeof(unsigned long long) * 8));
  }

  bool Get(int index) {
    return word[index / (sizeof(unsigned long long) * 8)] &
      (1ull << (index % (sizeof(unsigned long long) * 8)));
  }

  unsigned long long GetWord(int index) {
    return word[index];
  }

  // Union
  BitString operator | (const BitString &b) {
    BitString bs(word[0] | b.word[0],
                 word[1] | b.word[1],
                 word[2] | b.word[2],
                 word[3] | b.word[3]);
    return bs;
  }

  // Intersect
  BitString operator & (const BitString &b) {
    BitString bs(word[0] & b.word[0],
                 word[1] & b.word[1],
                 word[2] & b.word[2],
                 word[3] & b.word[3]);
    return bs;
  }

  // Remove bits
  BitString operator - (const BitString &b) {
    BitString bs(word[0] & !b.word[0],
                 word[1] & !b.word[1],
                 word[2] & !b.word[2],
                 word[3] & !b.word[3]);

    return bs;
  }

  bool operator == (const BitString &b) {
    return
      word[0] == b.word[0] &&
      word[1] == b.word[1] &&
      word[2] == b.word[2] &&
      word[3] == b.word[3];
  }

  bool IsNull() {
    return
      word[0] == 0 &&
      word[1] == 0 &&
      word[2] == 0 &&
      word[3] == 0;
  }

  bool IsNonNull() {
    if (word[0]) return true;
    if (word[1]) return true;
    if (word[2]) return true;
    if (word[3]) return true;
    return false;
  }

  bool IsUndef() {
    return
      word[0] == (-1ULL) &&
      word[1] == (-1ULL) &&
      word[2] == (-1ULL) &&
      word[3] == (-1ULL);
  }

  void SetUndef() {
    word[0] = word[1] = word[2] = word[3] = -1ULL;
  }

  void Print() {
    fprintf(stderr, "bits: %016llx:%016llx:%016llx:%016llx\n",
	    word[3], word[2], word[1], word[0] );
  }

  void PrintInitializer(FILE *f) {
    if (IsNull())
      fprintf(f, "BNULL");
    else
      fprintf(f, "BitString(0x%llxull, 0x%llxull, 0x%llxull, 0x%llxull)",
              word[0], word[1], word[2], word[3] );
  }

 private:
  unsigned long long word[4]; // 256 bits
};

#endif  // MAOUTIL_H_

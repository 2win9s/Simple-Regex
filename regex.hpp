// regex engine using thompson's method
// inspired by Russell Cox's articles on regex
// https://swtch.com/~rsc/regexp/regexp2.html

#pragma once
// C++ 20 needed for attributes (optional std::popcount)

#include <bit>        // for std::popcount    // requires C++ 20
#include <cstdint>    // for fixed width types
#include <cstring>    // for std::memcpy (type punning)
#include <iostream>   // for overloading << and for cout of course
#include <stdexcept>  // error handling
#include <string>     // for c++ strings
#include <vector>     // for vector

namespace simple_regex {
using byte = unsigned char;
/* getting and setting bits in a char */

// e.g. 0b10000000 the 7th bit is 1 (start from least significant at 0)
inline bool test_bit(byte bits, byte idx) { return (bits >> idx) & 1; }
// (idx start from least significant at 0)
inline byte set_bit(byte bits, byte idx) { return bits | (1 << idx); }

// (idx start from least significant at 0)
inline byte reset_bit(byte bits, byte idx) { return bits & (~(1 << idx)); }

// (idx start from least significant at 0)
inline byte flip_bit(byte bits, byte idx) { return bits ^ (1 << idx); }

/*
For abusing ADL in case std::popcount is unavailable
Brian Kernighan's Algorithm which
hopefully compiler optimises to popcnt with march native or -mpopcnt
*/
template <typename... Sink>
inline uint32_t popcount(uint64_t n) {
  uint32_t cnt = 0;
  while (n) {
    n &= n - 1;
    ++cnt;
  }
  return cnt;
}

// size snapped to neearest 64 bits
template <uint32_t bitsize>
struct bitmap {
  inline bool test(uint32_t idx) const {
    return test_bit(bits[idx >> 3], idx & 7);
  }
  inline void set(uint32_t idx) {
    bits[idx >> 3] = set_bit(bits[idx >> 3], idx & 7);
  }
  inline void reset(uint32_t idx) {
    bits[idx >> 3] = reset_bit(bits[idx >> 3], idx & 7);
  }
  inline void flip(uint32_t idx) {
    bits[idx >> 3] = flip_bit(bits[idx >> 3], idx & 7);
  }
  inline uint32_t count() const {
    using namespace std;  // ADL for popcount
    uint64_t temp;
    std::memcpy(&temp, &bits[0], 8);
    uint32_t retv = popcount(temp);
    for (uint32_t i = 8; i < sizeof(bits); i += 8) {
      std::memcpy(&temp, &bits[i], 8);
      retv += popcount(temp);
    }
    return retv;
  }
  inline bitmap& operator^=(const bitmap& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < sizeof(bits); i += 8) {
      std::memcpy(&temp_a, &bits[i], 8);
      std::memcpy(&temp_b, &other.bits[i], 8);
      temp_a ^= temp_b;
      std::memcpy(&bits[i], &temp_a, 8);
    }
    return *this;
  }
  inline bitmap& operator&=(const bitmap& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < sizeof(bits); i += 8) {
      std::memcpy(&temp_a, &bits[i], 8);
      std::memcpy(&temp_b, &other.bits[i], 8);
      temp_a &= temp_b;
      std::memcpy(&bits[i], &temp_a, 8);
    }
    return *this;
  }
  inline bitmap& operator|=(const bitmap& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < sizeof(bits); i += 8) {
      std::memcpy(&temp_a, &bits[i], 8);
      std::memcpy(&temp_b, &other.bits[i], 8);
      temp_a |= temp_b;
      std::memcpy(&bits[i], &temp_a, 8);
    }
    return *this;
  }
  inline bitmap& operator~() {
    uint64_t temp_a;
    for (uint32_t i = 0; i < sizeof(bits); i += 8) {
      std::memcpy(&temp_a, &bits[i], 8);
      temp_a = ~temp_a;
      std::memcpy(&bits[i], &temp_a, 8);
    }
    return *this;
  }
  inline friend bitmap operator^(bitmap lhs, const bitmap& rhs) {
    return lhs ^= rhs;
  }
  inline friend bitmap operator&(bitmap lhs, const bitmap& rhs) {
    return lhs &= rhs;
  }
  inline friend bitmap operator|(bitmap lhs, const bitmap& rhs) {
    return lhs |= rhs;
  }
  inline void clear() { std::memset(bits, 0, sizeof(bits)); }
  inline byte* data() { return bits; }
  inline const byte* data() const { return bits; }
  inline bitmap() : bits{} {}
  inline bitmap(const bitmap& other) {
    std::memcpy(bits, other.bits, sizeof(bits));
  }
  inline bitmap(bitmap&& other) { std::memcpy(bits, other.bits, sizeof(bits)); }
  inline bitmap& operator=(const bitmap& other) {
    std::memcpy(bits, other.bits, sizeof(bits));
    return *this;
  }
  inline bitmap& operator=(bitmap&& other) {
    std::memcpy(bits, other.bits, sizeof(bits));
    return *this;
  }
  inline friend void swap(bitmap& lhs, bitmap& rhs) {
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < sizeof(lhs.bits); i += 8) {
      std::memcpy(&temp_a, &lhs.bits[i], 8);
      std::memcpy(&temp_b, &rhs.bits[i], 8);
      std::memcpy(&lhs.bits[i], &temp_b, 8);
      std::memcpy(&rhs.bits[i], &temp_a, 8);
    }
  }

  friend std::ostream& operator<<(std::ostream& stream, const bitmap& map) {
    for (auto i = 0; i < 8 * sizeof(map); ++i) {
      if (map.test(i)) {
        stream << '1';
      } else {
        stream << '0';
      }
    }
    return stream;
  }

 protected:
  byte bits[8 * (int)((bitsize + 63) / 64)] = {0};
};

void error_invalid_utf8(const char* func_name) {
  std::string err_msg = "ERROR: Invalid UTF-8 passed to:";
  err_msg += func_name;
  throw std::invalid_argument(err_msg);
}

char peek_next(const std::string& s, uint32_t st) {
  if ((st + 1) < (s.size())) {
    return s[st + 1];
  }
  return 0;
}

char peek_next(const char* s, uint32_t st, uint32_t sz) {
  if (st + 1 < sz) {
    return s[st + 1];
  }
  return 0;
}

// not quite a complete bitmap
// performance is linear for the 4byte codes, don't care about them
struct utf8_bitmap {
  // given start byte how many bytes in this utf8 encoded code point
  inline static byte utf_bytes(byte start_byte) {
    // optimise for ASCII
    if (start_byte < 192) [[likely]] {
      return 1;  // if binary doesn't start with 110 not multi byte utf8 i.e.
                 // < 128+64
    } else {
      return 4 - (start_byte < 224) - (start_byte < 240);
    }
  }
  // extract the 16 unique bits of a 3 byte code point
  // i.e. a perfect hash for 3 byte UTF8 code points
  inline static uint16_t utf_3byte_h(byte a, byte b, byte c) {
    uint16_t ret = (c & 0b00111111);
    ret += static_cast<uint16_t>(b & 0b00111111) << 6;
    ret += static_cast<uint16_t>(a & 0b00001111) << 12;
    return ret;
  }
  // extract the 11 unique bits of a 2 byte code point
  // i.e. a perfect hash for 2 byte UTF8 code points
  inline static uint16_t utf_2byte_h(byte a, byte b) {
    uint16_t ret = (b & 0b00111111);
    ret += static_cast<uint16_t>(a & 0b00011111) << 6;
    return ret;
  }

  inline static uint32_t pack_4byte(byte a, byte b = 0, byte c = 0,
                                    byte d = 0) {
    uint32_t ret = d;
    ret += static_cast<uint32_t>(c) << 8;
    ret += static_cast<uint32_t>(b) << 16;
    ret += static_cast<uint32_t>(a) << 24;
    return ret;
  }
  inline static uint32_t pack_rev4byte(byte a, byte b = 0, byte c = 0,
                                       byte d = 0) {
    uint32_t ret = a;
    ret += static_cast<uint32_t>(b) << 8;
    ret += static_cast<uint32_t>(c) << 16;
    ret += static_cast<uint32_t>(d) << 24;
    return ret;
  }

  inline bool test(byte a) const { return ascii.test(a); }
  inline bool test(byte a, byte b) const {
    if (latin == nullptr) {
      return false;
    }
    return (*latin).test(utf_2byte_h(a, b));
  }
  inline bool test(byte a, byte b, byte c) const {
    if (bmp == nullptr) {
      return false;
    }
    return (*bmp).test(utf_3byte_h(a, b, c));
  }
  inline bool test(byte a, byte b, byte c, byte d) const {
    if (others == nullptr) {
      return false;
    }
    uint16_t idx = ((static_cast<uint16_t>(a & 7)) << 6) + (b & 31);
    if (others[idx] == nullptr) {
      return false;
    }
    uint16_t mapidx = (static_cast<uint16_t>(c & 31) << 6) + (d & 31);
    return (*others[idx]).test(mapidx);
  }
  inline bool test_4byte(uint32_t bytes) const {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return test(a);
        {
          case 2:
            byte b = bytes >> 16;
            return test(a, b);
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return test(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return test(a, b, c, bytes);
        }
    }
  }
  // order of bytes reversed
  inline bool test_rev4byte(uint32_t bytes) const {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return test(a);
        {
          case 2:
            byte b = bytes >> 8;
            return test(a, b);
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            return test(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            return test(a, b, c, d);
        }
    }
  }

  inline void insert(byte a) { ascii.set(a); }
  inline void insert(byte a, byte b) {
    if (latin) {
    } else {
      latin = new bitmap<2048>{};
    }
    (*latin).set(utf_2byte_h(a, b));
  }
  inline void insert(byte a, byte b, byte c) {
    if (bmp) {
    } else {
      bmp = new bitmap<65536>{};
    }
    (*bmp).set(utf_3byte_h(a, b, c));
  }
  inline void insert(byte a, byte b, byte c, byte d) {
    if (others) {
    } else {
      others = new bitmap<4096>*[512]{};
    }
    uint16_t idx = ((static_cast<uint16_t>(a & 7)) << 6) + (b & 31);
    if (others[idx]) {
    } else {
      others[idx] = new bitmap<4096>{};
    }
    uint16_t mapidx = (static_cast<uint16_t>(c & 31) << 6) + (d & 31);
    (*others[idx]).set(mapidx);
  }
  inline void insert_4byte(uint32_t bytes) {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] insert(a);
        return;
        {
          case 2:
            byte b = bytes >> 16;
            insert(a, b);
            return;
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            insert(a, b, c);
            return;
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            insert(a, b, c, bytes);
            return;
        }
    }
  }
  inline void insert_rev4byte(uint32_t bytes) {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] insert(a);
        return;
        {
          case 2:
            byte b = bytes >> 8;
            insert(a, b);
            return;
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            insert(a, b, c);
            return;
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            insert(a, b, c, d);
            return;
        }
    }
  }

  inline void remove(byte a) { ascii.reset(a); }
  inline void remove(byte a, byte b) {
    if (latin) {
      (*latin).reset(utf_2byte_h(a, b));
    } else {
      return;
    }
  }
  inline void remove(byte a, byte b, byte c) {
    if (bmp) {
      (*bmp).reset(utf_3byte_h(a, b, c));
    } else {
      return;
    }
  }
  inline void remove(byte a, byte b, byte c, byte d) {
    if (others) {
      uint16_t idx = ((static_cast<uint16_t>(a & 7)) << 6) + (b & 31);
      if (others[idx]) {
        uint16_t mapidx = (static_cast<uint16_t>(c & 31) << 6) + (d & 31);
        (*others[idx]).reset(mapidx);
      } else {
        return;
      }
    } else {
      return;
    }
  }
  inline void remove_4byte(uint32_t bytes) {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] remove(a);
        return;
        {
          case 2:
            byte b = bytes >> 16;
            remove(a, b);
            return;
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            remove(a, b, c);
            return;
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            remove(a, b, c, bytes);
            return;
        }
    }
  }
  inline void remove_rev4byte(uint32_t bytes) {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] remove(a);
        return;
        {
          case 2:
            byte b = bytes >> 8;
            remove(a, b);
            return;
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            remove(a, b, c);
            return;
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            remove(a, b, c, d);
            return;
        }
    }
  }

  inline uint32_t count() const {
    uint32_t ret = ascii.count();
    if (latin) {
      ret += (*latin).count();
    }
    if (bmp) {
      ret += (*bmp).count();
    }
    if (others) {
      for (auto i = 0; i < 512; ++i) {
        if (others[i]) {
          ret += (*others[i]).count();
        }
      }
    }
    return ret;
  }

  inline void shrink_to_fit() {
    if (latin) {
      if (!((bool)(*latin).count())) {
        delete latin;
        latin = nullptr;
      }
    }
    if (bmp) {
      if (!((bool)(*bmp).count())) {
        delete bmp;
        bmp = nullptr;
      }
    }
    if (others) {
      for (auto i = 0; i < 512; ++i) {
        if (others[i]) {
          if (!((bool)(*others[i]).count())) {
            delete others[i];
            others[i] = nullptr;
          }
        }
      }
    }
  }

  inline utf8_bitmap& operator&=(const utf8_bitmap& other) {
    ascii &= other.ascii;
    if ((latin) && (other.latin)) {
      *latin &= *other.latin;
    }
    if ((bmp) && (other.bmp)) {
      *bmp &= *other.bmp;
    }
    if ((others) && (other.others)) {
      for (auto i = 0; i < 512; ++i) {
        if ((others[i]) && (other.others[i])) {
          *others[i] &= *other.others[i];
        }
      }
    }
    return *this;
  }
  inline utf8_bitmap& operator|=(const utf8_bitmap& other) {
    ascii |= other.ascii;
    if (other.bmp) {
      if (bmp) {
        *bmp |= *other.bmp;
      } else {
        bmp = new bitmap<65536>;
        *bmp = *(other.bmp);
      }
    }
    if (other.others) {
      if (others) {
        for (auto i = 0; i < 512; ++i) {
          if ((other.others[i])) {
            if (others[i]) {
              *others[i] |= *other.others[i];
            } else {
              others[i] = new bitmap<4096>{};
              *others[i] = *(other.others[i]);
            }
          }
        }
      } else {
        others = new bitmap<4096>*[512]{nullptr};
        for (auto i = 0; i < 512; ++i) {
          if (other.others[i]) {
            others[i] = new bitmap<4096>{};
            *others[i] = *(other.others[i]);
          }
        }
      }
    }
    return *this;
  }
  inline friend utf8_bitmap operator&(utf8_bitmap lhs, const utf8_bitmap& rhs) {
    return lhs &= rhs;
  }
  inline friend utf8_bitmap operator|(utf8_bitmap lhs, const utf8_bitmap& rhs) {
    return lhs |= rhs;
  }
  inline utf8_bitmap()
      : ascii(), latin(nullptr), bmp(nullptr), others(nullptr) {}
  inline utf8_bitmap(const utf8_bitmap& other)
      : ascii(),
        latin(other.latin ? new bitmap<2048> : nullptr),
        bmp(other.bmp ? new bitmap<65536> : nullptr),
        others(other.others ? new bitmap<4096>*[512]{nullptr} : nullptr) {
    ascii = other.ascii;
    if (latin) {
      *latin = *(other.latin);
    }
    if (bmp) {
      *bmp = *(other.bmp);
    }
    if (others) {
      for (auto i = 0; i < 512; ++i) {
        if (other.others[i]) {
          others[i] = new bitmap<4096>{};
          *others[i] = *(other.others[i]);
        }
      }
    }
  }
  inline utf8_bitmap(const std::string& s)
      : ascii(), latin(nullptr), bmp(nullptr), others(nullptr) {
    for (auto i = 0; i < s.size();) {
      switch (utf8_bitmap::utf_bytes(s[i])) {
        default:
          insert(s[i]);
          ++i;
          break;
        case 2:
          if ((i < (s.size() - 1))) {
            insert(s[i], s[i + 1]);
            i += 2;
          } else {
            error_invalid_utf8(
                "simple_regex::utf8_bitmap, constructor passed invalid utf8 "
                "string");
          }
          break;
        case 3:
          if (i < (s.size() - 2)) {
            insert(s[i], s[i + 1], s[i + 2]);
            i += 3;
          } else {
            error_invalid_utf8(
                "simple_regex::utf8_bitmap, constructor passed invalid utf8 "
                "string");
          }
          break;
        case 4:
          if (i < (s.size() - 3)) {
            insert(s[i], s[i + 1], s[i + 2], s[i + 3]);
            i += 4;
          } else {
            error_invalid_utf8(
                "simple_regex::utf8_bitmap, constructor passed invalid utf8 "
                "string");
          }
          break;
      }
    }
  }
  inline utf8_bitmap& operator=(const utf8_bitmap& other) {
    ascii = other.ascii;
    if (latin) {
      if (other.latin) {
        *latin = *(other.latin);
      } else {
        delete latin;
        latin = nullptr;
      }
    } else if (other.latin) {
      latin = new bitmap<2048>;
      *latin = *(other.latin);
    }
    if (bmp) {
      if (other.bmp) {
        *bmp = *(other.bmp);
      } else {
        delete bmp;
        bmp = nullptr;
      }
    } else if (other.bmp) {
      bmp = new bitmap<65536>;
      *bmp = *(other.bmp);
    }
    if (others) {
      if (other.others) {
        for (auto i = 0; i < 512; ++i) {
          if (others[i]) {
            if (other.others[i]) {
              *others[i] = *(other.others[i]);
            } else {
              delete others[i];
              others[i] = nullptr;
            }
          } else if (other.others[i]) {
            others[i] = new bitmap<4096>{};
            *others[i] = *(other.others[i]);
          }
        }
      } else {
        for (auto i = 0; i < 512; ++i) {
          if (others[i]) {
            delete others[i];
            others[i] = nullptr;
          }
        }
        delete[] others;
        others = nullptr;
      }
    } else if (other.others) {
      others = new bitmap<4096>*[512]{nullptr};
      for (auto i = 0; i < 512; ++i) {
        if (other.others[i]) {
          others[i] = new bitmap<4096>{};
          *others[i] = *(other.others[i]);
        }
      }
    }
    return *this;
  }

  inline utf8_bitmap(utf8_bitmap&& other)
      : latin(nullptr), bmp(nullptr), others(nullptr) {
    // ADL
    ascii = other.ascii;
    using namespace std;
    swap(latin, other.latin);
    swap(bmp, other.bmp);
    swap(others, other.others);
  }
  inline utf8_bitmap& operator=(utf8_bitmap&& other) {
    ascii = other.ascii;
    using namespace std;
    swap(latin, other.latin);
    swap(bmp, other.bmp);
    swap(others, other.others);
    return *this;
  }
  inline friend void swap(utf8_bitmap& a, utf8_bitmap& b) {
    using namespace std;
    swap(a.ascii, b.ascii);
    swap(a.latin, b.latin);
    swap(a.bmp, b.bmp);
    swap(a.others, b.others);
  }

  ~utf8_bitmap() {
    if (latin) {
      delete latin;
    }
    if (bmp) {
      delete bmp;
    }
    if (others) {
      for (auto i = 0; i < 512; ++i) {
        if (others[i]) {
          delete others[i];
          others[i] = nullptr;
        }
      }
      delete[] others;
    }
  }

  inline bitmap<256>& ascii_bitmap() { return ascii; }
  inline bitmap<2048>& latin_bitamp() {
    if (latin) {
      return *latin;
    } else {
      throw std::runtime_error(
          "error no latin_bitmap found in utf8_bitmap instance");
    }
  }
  inline bitmap<65536>& bmp_bitmap() {
    if (bmp) {
      return *bmp;
    } else {
      throw std::runtime_error(
          "error no bmp_bitmap found in utf8_bitmap instance");
    }
  }
  inline bitmap<4096>*& others_bitmap_arr() {
    if (others) {
      return *others;
    } else {
      throw std::runtime_error(
          "error no bitmaps for 4byte UTF 8 found in utf8_bitmap instance");
    }
  }

  friend std::ostream& operator<<(std::ostream& stream,
                                  const utf8_bitmap& map) {
    for (uint32_t i = 0; i < 256; ++i) {
      if (map.ascii.test(i)) {
        byte o = i;
        stream << o;
      }
    }
    if (map.latin) {
      for (uint32_t i = 0; i < 2048; ++i) {
        byte b1 = 192;
        byte b2 = 128;
        if ((*map.latin).test(i)) {
          b2 += (i & 0b00111111);
          b1 += (i >> 6);
          std::string o = "";
          o += b1;
          o += b2;
          stream << o;
        }
      }
    }
    if (map.bmp) {
      for (uint32_t i = 0; i < 65536; ++i) {
        byte b1 = 224;
        byte b2 = 128;
        byte b3 = 128;
        if ((*map.bmp).test(i)) {
          b3 += (i & 0b00111111);
          b2 += ((i >> 6) & 0b00111111);
          b1 += (i >> 12);
          std::string o = "";
          o += b1;
          o += b2;
          o += b3;
          stream << o;
        }
      }
    }
    if (map.others) {
      uint32_t idx = 0;
      for (uint32_t i = 0; i < 512; ++i) {
        if (map.others[i]) {
          idx = 4096 * i;
          for (uint32_t j = 0; j < 4096; ++j) {
            byte b1 = 240;
            byte b2 = 128;
            byte b3 = 128;
            byte b4 = 128;
            const uint32_t idy = idx + j;
            if (((*map.others[i])).test(j)) {
              b4 += (idy & 0b00111111);
              b3 += ((idy >> 6) & 0b00111111);
              b2 += ((idy >> 12) & 0b00111111);
              b1 += idy >> 18;
              std::string o = "";
              o += b1;
              o += b2;
              o += b3;
              o += b4;
              stream << o;
            }
          }
        }
      }
    }
    return stream;
  }

 protected:
  // the 1 byte code points
  bitmap<256> ascii;
  // the two byte code points latin script alphabets + more
  bitmap<2048>* latin = nullptr;
  // the three byte code points basic multilingual plane
  bitmap<65536>* bmp = nullptr;
  // the 4 byte codes default stored in reverse
  bitmap<4096>** others = nullptr;
};

// idx is left at end of code point (not past the end)
uint32_t get_utf8_n_inc(const std::string& str, uint32_t& idx) {
  uint32_t utf8_char = static_cast<byte>(str[idx]);
  switch (utf8_bitmap::utf_bytes(str[idx])) {
    default:
      break;
    case 2:
      if ((idx < (str.size() - 1))) {
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 1])) << 8);
        idx += 1;
      } else {
        error_invalid_utf8(
            "simple_regex::get_utf8, constructor passed invalid "
            "utf8");
      }
      break;
    case 3:
      if (idx < (str.size() - 2)) {
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 1])) << 8);
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 2])) << 16);
        idx += 2;
      } else {
        error_invalid_utf8(
            "simple_regex::get_utf8, constructor passed invalid "
            "utf8");
      }
      break;
    case 4:
      if (idx < (str.size() - 3)) {
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 1])) << 8);
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 2])) << 16);
        utf8_char +=
            (static_cast<uint32_t>(static_cast<byte>(str[idx + 3])) << 24);
        idx += 3;
      } else {
        error_invalid_utf8(
            "simple_regex::get_utf8, constructor passed invalid "
            "utf8");
      }
  }
  return utf8_char;
}
std::string uint32_revto_utf8(uint32_t code_point) {
  byte a = code_point;
  byte b, c, d;
  std::string ch = "";
  ch += a;
  switch (utf8_bitmap::utf_bytes(a)) {
    default:
      break;
    case 2:
      b = code_point >> 8;
      ch += b;
      break;
    case 3:
      b = code_point >> 8;
      ch += b;
      c = code_point >> 16;
      ch += c;
      break;
    case 4:
      b = code_point >> 8;
      ch += b;
      c = code_point >> 16;
      ch += c;
      d = code_point >> 24;
      ch += d;
      break;
  }
  return ch;
}

template <char End = ']'>
utf8_bitmap char_class(const std::string& s, uint32_t st, uint32_t& ret_end) {
  utf8_bitmap ret{};
  char next_char = s[st];
  while (next_char != End) {
    switch (next_char) {
      case 'a':
        if ((peek_next(s, st) == '-') && (peek_next(s, st + 1) == 'z')) {
          bitmap<256> lower_mask;
          uint64_t lower_data[4] = {0, 576460743713488896L, 0, 0};
          std::memcpy(lower_mask.data(), lower_data, sizeof(lower_data));
          ret.ascii_bitmap() |= lower_mask;
          st += 3;
        } else {
          ret.insert(next_char);
          ++st;
        }
        break;
      case 'A':
        if ((peek_next(s, st) == '-') && (peek_next(s, st + 1) == 'Z')) {
          bitmap<256> lower_mask;
          uint64_t lower_data[4] = {0, 134217726L, 0, 0};
          std::memcpy(lower_mask.data(), lower_data, sizeof(lower_data));
          ret.ascii_bitmap() |= lower_mask;
          st += 3;
        } else {
          ret.insert(next_char);
          ++st;
        }
        break;
      case '0':
        if ((peek_next(s, st) == '-') && (peek_next(s, st + 1) == '9')) {
          bitmap<256> lower_mask;
          uint64_t lower_data[4] = {287948901175001088L, 0, 0, 0};
          std::memcpy(lower_mask.data(), lower_data, sizeof(lower_data));
          ret.ascii_bitmap() |= lower_mask;
          st += 3;
        } else {
          ret.insert(next_char);
          ++st;
        }
        break;
      default:
        switch (utf8_bitmap::utf_bytes(next_char)) {
          default:
            ret.insert(next_char);
            ++st;
            break;
          case 2:
            if ((st < (s.size() - 2))) {
              if (s[st + 1] == End) {
                error_invalid_utf8("simple_regex::char_class");
              }
              ret.insert(next_char, s[st + 1]);
              st += 2;
            } else {
              error_invalid_utf8(
                  "simple_regex::char_class, last element must be end token, "
                  "default ]");
            }
            break;
          case 3:
            if ((st < (s.size() - 3))) {
              if ((s[st + 1] == End) || (s[st + 2] == End)) {
                error_invalid_utf8("simple_regex::char_class");
              }
              ret.insert(next_char, s[st + 1], s[st + 2]);
              st += 3;
            } else {
              error_invalid_utf8(
                  "simple_regex::char_class, last element must be end token, "
                  "default ]");
            }
            break;
          case 4:
            if ((st < (s.size() - 4))) {
              if ((s[st + 1] == End) || (s[st + 2] == End) ||
                  (s[st + 3] == End)) {
                error_invalid_utf8("simple_regex::char_class");
              }
              ret.insert(next_char, s[st + 1], s[st + 2], s[st + 3]);
              st += 4;
            } else {
              error_invalid_utf8(
                  "simple_regex::char_class, last element must be end token, "
                  "default ]");
            }
            break;
        }
        break;
    }
    if (st >= s.size()) {
      std::string err_msg =
          "ERROR: Invalid string passed to char_class, must end character "
          "class with \']\' and start initial index beyond opening sbracket";
      throw std::invalid_argument(err_msg);
    }
    next_char = s[st];
  }
  ret_end = st;
  return ret;
}

struct nfa_vm {
  struct op {
    enum optype : uint32_t {
      // the base
      CHAR = 'c',
      MATCH = 'm',
      SPLIT = 'f',
      ANY = 'a',
      SAVE = 's',
      CLASS = 'g'
    };
    op(uint32_t p, uint32_t dat, op* nxt, op* branch = nullptr)
        : opt(p), data(dat), gen(-1), lb(nxt), rb(branch) {}
    uint32_t opt;
    uint32_t data;
    int64_t gen = -1;
    op* lb;
    op* rb;
  };
  // fragment of full nfa, tracks an op and a list of dangling connections
  struct nfa_frag {
    nfa_frag(op* spos, op** st, op** ed) : sp(spos), start(st), end(ed) {}
    nfa_frag(op& spos) : sp(&spos), start(&(spos.lb)), end(&(spos.lb)) {}
    nfa_frag() : sp(nullptr), pad(0), start(nullptr), end(nullptr) {}
    op* sp;
    // pad it out to a neat 32 bytes (should benchmark to see if it matters)
   private:
    int64_t pad = 0;

   public:
    op** start;
    op** end;
  };

  struct thread {
    thread(op* o, uint32_t n = 0) : ops(o), m_loc(n) {}
    thread(op* o, std::vector<uint32_t>&& relay) : ops(o), m_loc(relay) {}
    op* ops;
    std::vector<uint32_t> m_loc;
  };

 protected:
  uint opt_precedence(char c) {
    switch (c) {
      case '\\':
        return 100;
      case '(':
        return 90;
      case '[':
        return 80;
      case '?':
      case '*':
      case '+':
        return 70;
      case 0:
        return 60;
      case '|':
        return 50;
      default:
        std::string err_msg = "Invalid argument:";
        err_msg.push_back(c);
        err_msg += " to simple_regex::nfa_vm::opt_precendence";
        throw std::invalid_argument(err_msg);
        return 0;
    }
  }

  void compile_char(std::string& processed, uint32_t& ret_idx) {
    if (processed[ret_idx] == '.') {
      prog.emplace_back(op(op::optype::ANY, 0, nullptr));
    } else {
      uint32_t utf8_char = get_utf8_n_inc(processed, ret_idx);
      prog.emplace_back(op(op::optype::CHAR, utf8_char, nullptr));
    };
    f_stack.emplace_back(prog.back());
  }

  void pop_stack_presedence(byte c, std::string& optstack,
                            std::string& processed) {
    while (true) {
      if (optstack.size() == 0) {
        optstack += c;
        return;
      }  // everything is left associative
      if ((opt_precedence(c) > opt_precedence(optstack.back())) ||
          (optstack.back() == '(')) {
        optstack += c;
        return;
      } else {
        processed += optstack.back();
        optstack.pop_back();
      }
    }
  }

  // add in implicit concatenation operators with null
  void tokenise(const std::string& regex, std::string& tokenised) {
    tokenised = "";
    tokenised.reserve(regex.size() * 2);
    for (auto i = 0; i < regex.size(); ++i) {
      if (regex[i] == '[') {
        while (regex[i] != ']') {
          tokenised += regex[i];
          ++i;
          if (i == regex.size()) {
            throw std::invalid_argument(
                "simple_regex::nfa_vm, stray ] in regex");
          }
        }
      }
      byte n;
      switch (utf8_bitmap::utf_bytes(regex[i])) {
        default:
          tokenised += regex[i];
          break;
        case 2:
          if ((i < (regex.size() - 1))) {
            tokenised += regex[i];
            tokenised += regex[i + 1];
            i += 1;
          } else {
            error_invalid_utf8(
                "simple_regex::nfa_vm, constructor passed invalid "
                "utf8");
          }
          break;
        case 3:
          if (i < (regex.size() - 2)) {
            tokenised += regex[i];
            tokenised += regex[i + 1];
            tokenised += regex[i + 2];
            i += 2;
          } else {
            error_invalid_utf8(
                "simple_regex::nfa_vm, constructor passed invalid "
                "utf8");
          }
          break;
        case 4:
          if (i < (regex.size() - 3)) {
            tokenised += regex[i];
            tokenised += regex[i + 1];
            tokenised += regex[i + 2];
            tokenised += regex[i + 3];
            i += 3;
          } else {
            error_invalid_utf8(
                "simple_regex::nfa_vm, constructor passed invalid "
                "utf8");
          }
          break;
      }
      n = peek_next(regex, i);
      if ((regex[i] == '|') || (regex[i] == '(')) {
        continue;
      }
      if ((regex[i] == '\\') && n) {
        ++i;
        tokenised += regex[i];
      }
      if (n && (n != ')') && (n != '|') && (n != '*') && (n != '+') &&
          (n != '?')) {
        tokenised.push_back(0);
      }
    }
  }

  // not exactly shunting yard
  void nearly_shunting_yard(const std::string& tokenised,
                            std::string& processed) {
    std::string operator_stack = "";
    processed = "";
    operator_stack.reserve(tokenised.size());
    processed.reserve(tokenised.size());
    bool atom = false;
    char p = 0;
    for (auto i = 0; i < tokenised.size(); ++i) {
      switch (tokenised[i]) {
        case '\\':
          processed += tokenised[i];
          ++i;
          processed += tokenised[i];
          break;
        case '(':
          processed += tokenised[i];
          operator_stack += tokenised[i];
          break;
        case ')':
          p = tokenised[i];
          // processed += tokenised[i];
          while (operator_stack.back() != '(') {
            processed += operator_stack.back();
            operator_stack.pop_back();
            if (operator_stack.size() == 0) {
              throw std::invalid_argument(
                  "simple_regex::nfa_vm, stray ) in regex");
            }
          }
          processed += p;
          operator_stack.pop_back();
          break;
        case '[':
          while (tokenised[i] != ']') {
            processed += tokenised[i];
            ++i;
            if (i == tokenised.size()) {
              throw std::invalid_argument(
                  "simple_regex::nfa_vm, stray ] in regex");
            }
          }
          processed += tokenised[i];
          break;
        case ']':
          throw std::invalid_argument("simple_regex::nfa_vm, stray ] in regex");
          break;
        case '?':
        case '*':
        case '+':
        case 0:
        case '|':
          pop_stack_presedence(tokenised[i], operator_stack, processed);
          break;
        default:
          switch (utf8_bitmap::utf_bytes(tokenised[i])) {
            default:
              processed += tokenised[i];
              break;
            case 2:
              if ((i < (tokenised.size() - 1))) {
                processed += tokenised[i];
                processed += tokenised[i + 1];
                i += 1;
              } else {
                error_invalid_utf8(
                    "simple_regex::nfa_vm, constructor passed invalid "
                    "utf8");
              }
              break;
            case 3:
              if (i < (tokenised.size() - 2)) {
                processed += tokenised[i];
                processed += tokenised[i + 1];
                processed += tokenised[i + 2];
                i += 2;
              } else {
                error_invalid_utf8(
                    "simple_regex::nfa_vm, constructor passed invalid "
                    "utf8");
              }
              break;
            case 4:
              if (i < (tokenised.size() - 3)) {
                processed += tokenised[i];
                processed += tokenised[i + 1];
                processed += tokenised[i + 2];
                processed += tokenised[i + 3];
                i += 3;
              } else {
                error_invalid_utf8(
                    "simple_regex::nfa_vm, constructor passed invalid "
                    "utf8");
              }
              break;
          }
          break;
      }
    }
    while (operator_stack.size()) {
      processed += operator_stack.back();
      operator_stack.pop_back();
    }
  }

  // patch nfa fragments, Thompson's algorithm
  void patch(nfa_frag nfa_frag, op* pos) {
    op* next = *nfa_frag.start;
    op** tmp;
    *nfa_frag.start = pos;
    while (next != nullptr) {
      std::memcpy(&tmp, &next, sizeof(tmp));
      next = *tmp;
      *tmp = pos;
    }
  }

  // link these two linked lists
  void fuse(nfa_frag f1, nfa_frag f2) {
    op** tmp = f2.start;
    std::memcpy(&(*(f1.end)), &tmp, sizeof(tmp));
    f1.end = f2.end;
  }

  // compile the not quite postfix notation regex
  void compile_nfa_sg(std::string& processed) {
    prog.reserve(processed.size() + 4);
    uint32_t lsave = 2;
    uint32_t rsave = 3;
    uint32_t class_c = 0;
    prog.emplace_back(op(op::optype::SAVE, save_points, nullptr));
    save_points += 2;
    f_stack.emplace_back(prog.back());
    int64_t f1, f2;
    op** tmp;
    for (uint32_t i = 0; i < processed.size(); ++i) {
      // std::cout << "iteration:" << i << " " << processed[i] << std::endl;
      switch (processed[i]) {
        case '\\':
          if (peek_next(processed, i)) {
            ++i;
            compile_char(processed, i);
          }
          break;
        case '(':
          prog.emplace_back(op(op::optype::SAVE, lsave, nullptr));
          f1 = f_stack.size() - 1;
          patch(f_stack[f1], &prog.back());
          tmp = &(prog.back().lb);
          f_stack[f1].start = tmp;
          f_stack[f1].end = tmp;
          lsave += 2;
          break;
        case ')':
          prog.emplace_back(op(op::optype::SAVE, rsave, nullptr));
          f1 = f_stack.size() - 1;
          patch(f_stack[f1], &prog.back());
          tmp = &(prog.back().lb);
          f_stack[f1].start = tmp;
          f_stack[f1].end = tmp;
          rsave += 2;
          break;
        case '[':
          ++i;
          classes.emplace_back(char_class(processed, i, i));
          prog.emplace_back(op(op::optype::CLASS, class_c, nullptr));
          f_stack.emplace_back(prog.back());
          ++class_c;
          break;
        case ']':
          throw std::invalid_argument("simple_regex::nfa_vm, stray ] in regex");
          break;
        case '?':
          f1 = f_stack.size() - 1;
          prog.emplace_back(op(op::optype::SPLIT, 0, f_stack[f1].sp, nullptr));
          tmp = &(prog.back().rb);
          std::memcpy(&(*f_stack[f1].end), &tmp, sizeof(tmp));
          f_stack[f1].end = tmp;
          f_stack[f1].sp = &prog.back();
          break;
        case '*':
          f1 = f_stack.size() - 1;
          prog.emplace_back(op(op::optype::SPLIT, 0, f_stack[f1].sp, nullptr));
          patch(f_stack[f1], &prog.back());
          f_stack[f1].sp = &(prog.back());
          f_stack[f1].start = &(prog.back().rb);
          f_stack[f1].end = &(prog.back().rb);
          break;
        case '+':
          f1 = f_stack.size() - 1;
          prog.emplace_back(op(op::optype::SPLIT, 0, f_stack[f1].sp, nullptr));
          patch(f_stack[f1], &prog.back());
          f_stack[f1].start = &(prog.back().rb);
          f_stack[f1].end = &(prog.back().rb);
          break;
        case 0:
          f2 = f_stack.size() - 1;
          f1 = f_stack.size() - 2;
          patch(f_stack[f1], f_stack[f2].sp);
          f_stack[f1].start = f_stack[f2].start;
          f_stack[f1].end = f_stack[f2].end;
          f_stack.resize(f2);
          break;
        case '|':
          f2 = f_stack.size() - 1;
          f1 = f_stack.size() - 2;
          prog.emplace_back(
              op(op::optype::SPLIT, 0, f_stack[f1].sp, f_stack[f2].sp));
          fuse(f_stack[f1], f_stack[f2]);
          f_stack.resize(f2);
          f_stack[f1].sp = &prog.back();
          break;
        default:
          compile_char(processed, i);
          break;
      }
    }
    f1 = f_stack.size() - 1;
    if (f1 != 1) {
      std::cout << "Error extra fragments after compiling: " << f1 - 1
                << std::endl;
      throw std::runtime_error("simple_regex::nfa failed to parse regex");
    }
    prog.emplace_back(op(op::optype::SAVE, 1, nullptr));
    patch(f_stack[0], f_stack[f1].sp);
    patch(f_stack[f1], &prog.back());
    prog.emplace_back(op(op::optype::MATCH, 0, nullptr));
    prog[prog.size() - 2].lb = &prog.back();
    save_points = lsave;
  }

  void clear_compile_info() {
    f_stack.clear();
    prog.clear();
    classes.clear();
    save_points = 0;
  }

 public:
  nfa_vm(const std::string& regex) {
    std::string tokens;
    tokenise(regex, tokens);
    std::string notquitepostfix;
    nearly_shunting_yard(tokens, notquitepostfix);
    compile_nfa_sg(notquitepostfix);
  }
  nfa_vm() = delete;
  void recompile(const std::string& regex) {
    clear_compile_info();
    std::string tokens;
    tokenise(regex, tokens);
    std::string notquitepostfix;
    nearly_shunting_yard(tokens, notquitepostfix);
    compile_nfa_sg(notquitepostfix);
  }
  void print_classes() {
    std::cout << "Classes:" << std::endl;
    for (auto i = 0; i < classes.size(); ++i) {
      std::cout << "[" << i << "]" << "\t[" << classes[i] << "]" << std::endl;
    }
  }

  void print_prog() {
    std::cout << "Printing NFA ops" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    for (auto i = 0; i < prog.size(); ++i) {
      std::string ch = "";
      switch (prog[i].opt) {
        case op::optype::CHAR:
          ch = uint32_revto_utf8(prog[i].data);
          std::cout << "[" << i << "]\t";
          std::cout << ch;
          std::cout << "\t\tjmp " << prog[i].lb - prog.data();
          break;
        case op::optype::MATCH:
          std::cout << "[" << i << "]\t" << "match";
          break;
        case op::optype::SPLIT:
          std::cout << "[" << i << "]\t" << "split";
          std::cout << "\t\t" << prog[i].lb - prog.data() << ", "
                    << prog[i].rb - prog.data();
          break;
        case op::optype::ANY:
          std::cout << "[" << i << "]\t" << "any";
          std::cout << "\t\tjmp " << prog[i].lb - prog.data();
          break;
        case op::optype::SAVE:
          std::cout << "[" << i << "]\t" << "save  " << prog[i].data;
          std::cout << "\t\tjmp " << prog[i].lb - prog.data();
          break;
        case op::optype::CLASS:
          std::cout << "[" << i << "]\t" << "class " << prog[i].data;
          std::cout << "\t\tjmp " << prog[i].lb - prog.data();
          break;
      }
      std::cout << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;
    if (classes.size() != 0) {
      print_classes();
      std::cout << "--------------------------------" << std::endl;
    }
  }

  void new_thread(std::vector<thread>& pool, thread t, uint32_t i) {
    auto& op = *t.ops;
    if (op.gen == gen_id) {
      return;
    }
    op.gen = gen_id;
    if (op.opt == op::optype::SPLIT) {
      new_thread(pool, thread(op.lb, std::move(t.m_loc)), i);
      new_thread(pool, thread(op.rb, std::move(t.m_loc)), i);
      return;
    }
    if (op.opt == op::optype::SAVE) {
      t.m_loc[op.data] = i + 1;
      new_thread(pool, thread(op.lb, std::move(t.m_loc)), i);
      return;
    }
    pool.emplace_back(t);
  }

  void clear_match_info() {
    cur.clear();
    nxt.clear();
    matches.clear();
    gen_id = 0;
  }

  template <bool Unanchored = false, bool Match_one = true>
  bool match(const std::string& str) {
    clear_match_info();
    cur.reserve(prog.size());
    nxt.reserve(prog.size());
    bool match = false;
    new_thread(cur, thread(&prog[0], save_points), -1);
    for (uint32_t i = 0; i < str.size(); ++i) {
      gen_id = i;
      if constexpr (Unanchored) {
        new_thread(cur, thread(&prog[0], save_points), i - 1);
      }
      for (uint32_t j = 0; j < cur.size(); ++j) {
        auto& op = *cur[j].ops;
        uint32_t utf8;
        uint32_t i_c = i;
        switch (op.opt) {
          default:
            continue;
          case op::optype::CHAR:
            utf8 = get_utf8_n_inc(str, i_c);
            if (utf8 != op.data) {
              continue;
            }
            i = i_c;
            new_thread(nxt, thread(op.lb, std::move(cur[j].m_loc)), i);
            continue;
          case op::optype::CLASS:
            utf8 = get_utf8_n_inc(str, i_c);
            if (classes[op.data].test_rev4byte(utf8)) {
              i = i_c;
              new_thread(nxt, thread(op.lb, std::move(cur[j].m_loc)), i);
            }
            continue;
          case op::optype::ANY:
            new_thread(nxt, thread(op.lb, std::move(cur[j].m_loc)), i);
            continue;
          case op::optype::MATCH:
            match = true;
            matches.emplace_back(cur[j].m_loc);
            break;
        }
      }
      std::swap(cur, nxt);
      nxt.clear();
      if constexpr (Match_one) {
        if (match) {
          return match;
        }
      }
    }
    for (uint32_t j = 0; j < cur.size(); ++j) {
      auto& op = *cur[j].ops;
      byte a;
      uint32_t utf8;
      switch (op.opt) {
        default:
          continue;
        case op::optype::MATCH:
          match = true;
          matches.emplace_back(cur[j].m_loc);
          break;
      }
    }
    return match;
  }

  template <bool Unanchored = false, bool Match_one = true>
  bool match(const std::string& str, bool print_flag) {
    bool m = match<Unanchored, Match_one>(str);
    if (m) {
      std::cout << "Regex matching successsful!" << "\n";
      std::cout << str.substr(0, (str.size() > 1000) ? 1000 : str.size())
                << ((str.size() > 1000) ? " ..." : "") << std::endl;
      for (auto i = 0; i < save_points; i += 2) {
        std::cout << "Matches for group [" << i / 2 << "]" << "\n";
        for (auto j = 0; j < matches.size(); ++j) {
          for (auto k = matches[j][i]; k < matches[j][i + 1]; ++k) {
            std::cout << str[k];
          }
          std::cout << "\n";
        }
      }
      std::cout << std::flush;
    }
    return m;
  }
  bool multi_match(const std::string& str) { return match<true, true>(str); }

  std::vector<std::vector<uint32_t>>& match_indices() { return matches; }
  const std::vector<std::vector<uint32_t>>& match_indices() const {
    return matches;
  }

 protected:
  uint32_t save_points = 0;
  std::vector<nfa_frag> f_stack;
  std::vector<std::vector<uint32_t>> matches;
  std::vector<op> prog;
  std::vector<utf8_bitmap> classes;
  uint64_t gen_id = 0;
  std::vector<thread> cur{};
  std::vector<thread> nxt{};
};

}  // namespace simple_regex
// regex engine using thompson's method
// inspired by Russell Cox's articles on regex
// https://swtch.com/~rsc/regexp/regexp2.html

#pragma once
// C++ 20 needed for attributes (optional: std::popcount, std::countl_zero)

#include <bit>  // for std::popcount and std::countl_zero   // requires C++ 20
#include <cstdint>    // for fixed width types
#include <cstring>    // for std::memcpy (type punning)
#include <iostream>   // for overloading << and for cout of course
#include <map>        // probably a bst? no point in handrolling one
#include <stdexcept>  // error handling
#include <string>     // for c++ strings
#include <vector>     // for vector the GOAT of STL

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
For abusing ADL in case std::countl_zero is not available
*/
template <typename... Sink>
inline uint32_t countl_zero(uint64_t n) {
  static_assert(sizeof(double) == 8,
                "error coutl_zero requires doubles to be 8 bytes, also little "
                "endian is assumed");
  double x;
  double y;
  uint32_t bits[2];
  uint64_t bitrep;
  uint64_t bitrep2;
  std::memcpy(bits, &n, sizeof(uint64_t));
  x = bits[1];
  std::memcpy(&bitrep, &x, sizeof(double));
  bitrep = bitrep >> 52;
  bitrep += 2 * (bits[1] > 0);
  bitrep = bitrep & 31;
  y = bits[0];
  std::memcpy(&bitrep2, &y, sizeof(double));
  bitrep2 = bitrep2 >> 52;
  bitrep2 += 2 * (bits[0] > 0);
  bitrep2 = bitrep2 & 31;
  return 64 - bitrep - bitrep2 * (bits[1] == 0);
}
template <typename... Sink>
inline uint32_t countl_zero(uint32_t n) {
  static_assert(sizeof(double) == 8,
                "error coutl_zero requires doubles to be 8 bytes, also little "
                "endian is assumed");
  double x;
  uint64_t bitrep;
  x = n;
  std::memcpy(&bitrep, &x, sizeof(double));
  bitrep = bitrep >> 52;
  bitrep += 2 * (n > 0);
  bitrep = bitrep & 31;
  return 32 - bitrep;
}
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

  inline bool operator==(const bitmap& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < sizeof(bits); i += 8) {
      std::memcpy(&temp_a, &bits[i], 8);
      std::memcpy(&temp_b, &other.bits[i], 8);
      if (temp_a != temp_b) {
        return false;
      }
    }
    return true;
  }
  inline bool operator!=(const bitmap& other) { return !(*this == (other)); }
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
  byte bits[8 * (uint32_t)((bitsize + 63) / 64)] = {0};
};

struct bitvector {
  inline bool test(uint32_t idx) const {
    return test_bit(data[idx >> 3], idx & 7);
  }
  inline void set(uint32_t idx) {
    data[idx >> 3] = set_bit(data[idx >> 3], idx & 7);
  }
  inline void reset(uint32_t idx) {
    data[idx >> 3] = reset_bit(data[idx >> 3], idx & 7);
  }
  inline void flip(uint32_t idx) {
    data[idx >> 3] = flip_bit(data[idx >> 3], idx & 7);
  }
  inline uint32_t count() const {
    if (data.size() == 0) {
      return 0;
    }
    using namespace std;  // ADL for popcount
    uint64_t temp;
    std::memcpy(&temp, &data[0], 8);
    uint32_t retv = popcount(temp);
    for (uint32_t i = 1; i < data.size(); i += 8) {
      std::memcpy(&temp, &data[i], sizeof(uint64_t));
      retv += popcount(temp);
    }
    return retv;
  }
  inline bitvector& operator^=(const bitvector& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    uint32_t ms =
        (data.size() < other.data.size()) ? data.size() : other.data.size();
    for (uint32_t i = 0; i < ms; i += 8) {
      std::memcpy(&temp_a, &data[i], sizeof(uint64_t));
      std::memcpy(&temp_b, &other.data[i], sizeof(uint64_t));
      temp_a ^= temp_b;
      std::memcpy(&data[i], &temp_a, sizeof(uint64_t));
    }
    return *this;
  }
  inline bitvector& operator&=(const bitvector& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    uint32_t ms =
        (data.size() < other.data.size()) ? data.size() : other.data.size();
    for (uint32_t i = 0; i < ms; i += 8) {
      std::memcpy(&temp_a, &data[i], sizeof(uint64_t));
      std::memcpy(&temp_b, &other.data[i], sizeof(uint64_t));
      temp_a &= temp_b;
      std::memcpy(&data[i], &temp_a, sizeof(uint64_t));
    }
    return *this;
  }
  inline bitvector& operator|=(const bitvector& other) {
    uint64_t temp_a;
    uint64_t temp_b;
    uint32_t ms =
        (data.size() < other.data.size()) ? data.size() : other.data.size();
    for (uint32_t i = 0; i < ms; i += 8) {
      std::memcpy(&temp_a, &data[i], sizeof(uint64_t));
      std::memcpy(&temp_b, &other.data[i], sizeof(uint64_t));
      temp_a |= temp_b;
      std::memcpy(&data[i], &temp_a, sizeof(uint64_t));
    }
    return *this;
  }
  inline bitvector& operator~() {
    uint64_t temp_a;
    for (uint32_t i = 0; i < data.size(); i += 8) {
      std::memcpy(&temp_a, &data[i], sizeof(uint64_t));
      temp_a = ~temp_a;
      std::memcpy(&data[i], &temp_a, sizeof(uint64_t));
    }
    return *this;
  }
  inline friend bitvector operator^(bitvector lhs, const bitvector& rhs) {
    return lhs ^= rhs;
  }
  inline friend bitvector operator&(bitvector lhs, const bitvector& rhs) {
    return lhs &= rhs;
  }
  inline friend bitvector operator|(bitvector lhs, const bitvector& rhs) {
    return lhs |= rhs;
  }
  inline bool operator==(const bitvector& other) {
    if (data.size() != other.data.size()) {
      return false;
    }
    uint64_t temp_a;
    uint64_t temp_b;
    for (uint32_t i = 0; i < data.size(); i += 8) {
      std::memcpy(&temp_a, &data[i], 8);
      std::memcpy(&temp_b, &other.data[i], 8);
      if (temp_a != temp_b) {
        return false;
        break;
      }
    }
    return true;
  }
  inline bool operator!=(const bitvector& other) { return !(*this == (other)); }
  inline friend bool comp(const bitvector& lhs, const bitvector& rhs) {
    if (lhs.data.size() <= rhs.data.size()) {
      if (lhs.data.size() < rhs.data.size()) {
        return true;
      }
      uint64_t temp_a;
      uint64_t temp_b;
      for (int64_t i = lhs.data.size() - 8; i >= 0; i -= 8) {
        std::memcpy(&temp_a, &lhs.data[i], 8);
        std::memcpy(&temp_b, &rhs.data[i], 8);
        if (temp_a < temp_b) {
          return true;
        }
      }
      return false;
    } else {
      return false;
    }
  }
  inline void clear() { std::memset(&data[0], 0, data.size() * sizeof(byte)); }

  friend std::ostream& operator<<(std::ostream& stream, const bitvector& map) {
    for (auto i = 0; i < 8 * map.data.size(); ++i) {
      if (map.test(i)) {
        stream << '1';
      } else {
        stream << '0';
      }
    }
    return stream;
  }

  // size in bits
  uint32_t size() { return static_cast<uint32_t>(data.size()) * 8; }
  // capacity in bits
  uint32_t capacity() { return static_cast<uint32_t>(data.capacity()) * 8; }

  void resize(uint32_t size) {
    data.resize((8 * (uint32_t)((size + 63) / 64)), 0);
  }

  void reserve(uint32_t size) {
    data.reserve(8 * (uint32_t)((size + 63) / 64));
  }
  bitvector() = default;
  bitvector(uint32_t size) : data(8 * (uint32_t)((size + 63) / 64), 0) {}

  bitvector(const bitvector&) = default;
  bitvector(bitvector&&) = default;
  bitvector& operator=(const bitvector&) = default;
  bitvector& operator=(bitvector&&) = default;

  // protected:
  std::vector<byte> data;
};

struct sparse_set {
  inline uint32_t operator[](uint32_t idx) const { return dense[idx]; }
  inline uint32_t size() const { return dense.size(); }
  inline void insert(uint32_t i) {
    sparse[i] = dense.size();
    dense.emplace_back(i);
  }
  inline bool test(uint32_t i) const {
    if ((sparse[i] < dense.size())) {
      if (dense[sparse[i]] == i) {
        return true;
      }
    }
    return false;
  }
  inline void test_insert(uint32_t i) {
    if (test(i)) {
      return;
    }
    sparse[i] = dense.size();
    dense.emplace_back(i);
  }
  inline void remove(uint32_t i) {
    if (test(i)) {
      dense[sparse[i]] = dense.back();
      sparse[dense.back()] = sparse[i];
      dense.pop_back();
    } else {
      throw std::invalid_argument(
          "Error invalid argument to simple_regex::sparse_set::remove, element "
          "not in set");
    }
  }
  inline void set_range(uint32_t r) {
    if (r > dense.size()) {
      sparse.resize(r);
    } else {
      throw std::invalid_argument(
          "Error invalid argument to simple_regex::sparse_set::set_range, new "
          "range is less than cardinality of set");
    }
  }
  inline void clear() { dense.resize(0); }
  inline void shrink_to_fit() { dense.shrink_to_fit(); }
  sparse_set() : dense(), sparse() {}
  sparse_set(uint32_t size) : dense(size), sparse(size) {}
  std::vector<uint32_t> dense;
  std::vector<uint32_t> sparse;
};

// sparse set sacrificing constant(probably) clear for comparison
// or maybe iterate and construct bitset on the fly when you need it?
struct hybrid_set {
  inline uint32_t operator[](uint32_t idx) const { return sparse[idx]; }
  inline uint32_t size() const { return sparse.size(); }
  inline void insert(uint32_t i) {
    sparse.insert(i);
    bitset.set(i);
  }
  inline void test_insert(uint32_t i) {
    sparse.test_insert(i);
    bitset.set(i);
  }
  inline bool test(uint32_t i) const { return bitset.test(i); }
  inline void remove(uint32_t i) {
    bitset.reset(i);  // from a safety perspective maybe a bounds check but...
    sparse.remove(i);
  }
  inline void set_range(uint32_t size) {
    sparse.set_range(size);
    bitset.resize(size);
  }
  inline void clear() {
    sparse.clear();
    bitset.clear();
  }
  inline bool operator==(const hybrid_set& other) {
    if (sparse.size() != other.sparse.size()) {
      return false;
    }
    return (bitset == other.bitset);
  }
  inline bool operator!=(const hybrid_set& other) {
    return !(*this == (other));
  }
  hybrid_set() : sparse(), bitset() {}
  hybrid_set(uint32_t size) : sparse(size), bitset(size) {}
  hybrid_set(const hybrid_set&) = default;
  hybrid_set(hybrid_set&&) = default;
  hybrid_set& operator=(const hybrid_set&) = default;
  hybrid_set& operator=(hybrid_set&&) = default;
  sparse_set sparse;
  bitvector bitset;
};
// for use with std::map
struct hybrid_set_comp {
  bool operator()(const hybrid_set& a, const hybrid_set& b) const {
    return comp(a.bitset, b.bitset);
  }
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

inline static uint32_t utf_4byte_h(byte a, byte b, byte c, byte d) {
  uint32_t ret = (d & 0b00111111);
  ret += static_cast<uint32_t>(c & 0b00111111) << 6;
  ret += static_cast<uint32_t>(b & 0b00111111) << 12;
  ret += static_cast<uint32_t>(a & 0b00000111) << 18;
  return ret;
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

inline static uint32_t pack_4byte(byte a, byte b = 0, byte c = 0, byte d = 0) {
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

// not quite a complete bitmap
// performance is linear for the 4byte codes, don't care about them
struct utf8_bitmap {
  inline bool test(byte a) const { return ascii.test(a); }
  inline bool test(byte a, byte b) const {
    if (latin) {
      return (*latin).test(utf_2byte_h(a, b));
    }
    return false;
  }
  inline bool test(byte a, byte b, byte c) const {
    if (bmp) {
      return (*bmp).test(utf_3byte_h(a, b, c));
    }
    return false;
  }
  inline bool test(byte a, byte b, byte c, byte d) const {
    if (others) {
      uint16_t idx = ((static_cast<uint16_t>(a & 7)) << 6) + (b & 31);
      if (others[idx]) {
        uint16_t mapidx = (static_cast<uint16_t>(c & 31) << 6) + (d & 31);
        return (*others[idx]).test(mapidx);
      }
    }
    return false;
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
      bool empty = true;
      for (auto i = 0; i < 512; ++i) {
        if (others[i]) {
          empty = false;
          break;
        }
      }
      if (empty) {
        delete[] others;
        others = nullptr;
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
      switch (utf_bytes(s[i])) {
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
    bitmap<256> tmp;
    tmp = a.ascii;
    a.ascii = b.ascii;
    b.ascii = tmp;
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

// utf8 codepoint to pointer map
template <typename T>
struct utf8_ptrmap {
  inline const T* operator()(byte a) const { return ascii[a]; }
  inline const T* operator()(byte a, byte b) const {
    if (latin) {
      return (latin[utf_2byte_h(a, b)]);
    }
    return nullptr;
  }
  inline const T* operator()(byte a, byte b, byte c) const {
    if (bmp) {
      uint16_t idx = utf_3byte_h(a, b, c);
      uint16_t x = idx >> 11;
      uint16_t y = idx & 2047;
      return (bmp[x][y]);
    }

    return nullptr;
  }
  inline const T* operator()(byte a, byte b, byte c, byte d) const {
    if (others) {
      uint32_t idx = utf_4byte_h(a, b, c, d);
      uint32_t x = idx >> 11;
      uint32_t y = idx & 2047;
      return (others[x][y]);
    }
    return nullptr;
  }

  inline T* operator()(byte a) { return ascii[a]; }
  inline T* operator()(byte a, byte b) {
    if (latin) {
      return (latin[utf_2byte_h(a, b)]);
    }
    return nullptr;
  }
  inline T* operator()(byte a, byte b, byte c) {
    if (bmp) {
      uint16_t idx = utf_3byte_h(a, b, c);
      uint16_t x = idx >> 11;
      uint16_t y = idx & 2047;
      return (bmp[x][y]);
    }

    return nullptr;
  }
  inline T* operator()(byte a, byte b, byte c, byte d) {
    if (others) {
      uint32_t idx = utf_4byte_h(a, b, c, d);
      uint32_t x = idx >> 11;
      uint32_t y = idx & 2047;
      return (others[x][y]);
    }
    return nullptr;
  }

  inline const T* get_4byte(uint32_t bytes) const {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return this->operator()(a);
        {
          case 2:
            byte b = bytes >> 16;
            return this->operator()(a, b);
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return this->operator()(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return this->operator()(a, b, c, bytes);
        }
    }
  }
  // order of bytes reversed
  inline const T* get_rev4byte(uint32_t bytes) const {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return this->operator()(a);
        {
          case 2:
            byte b = bytes >> 8;
            this->operator()(a, b);
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            return this->operator()(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            return this->operator()(a, b, c, bytes);
        }
    }
  }

  inline T* set_4byte(uint32_t bytes) {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return this->operator()(a);
        {
          case 2:
            byte b = bytes >> 16;
            return this->operator()(a, b);
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return this->operator()(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            return this->operator()(a, b, c, bytes);
        }
    }
  }
  // order of bytes reversed
  inline T* set_rev4byte(uint32_t bytes) {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] return this->operator()(a);
        {
          case 2:
            byte b = bytes >> 8;
            this->operator()(a, b);
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            return this->operator()(a, b, c);
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            return this->operator()(a, b, c, bytes);
        }
    }
  }

  inline void add_element(byte a, T* ptr = nullptr) { ascii[a] = ptr; }
  inline void add_element(byte a, byte b, T* ptr = nullptr) {
    if (latin) {
    } else {
      latin = new T*[2048]{nullptr};
    }
    latin[utf_2byte_h(a, b)] = ptr;
  }
  inline void add_element(byte a, byte b, byte c, T* ptr = nullptr) {
    if (bmp) {
    } else {
      bmp = new T**[32]{nullptr};
    }
    uint16_t idx = utf_3byte_h(a, b, c);
    uint16_t x = idx >> 11;
    uint16_t y = idx & 2047;
    if (bmp[x]) {
    } else {
      bmp[x] = new T*[2048];
    }
    bmp[x][y] = ptr;
  }
  inline void add_element(byte a, byte b, byte c, byte d, T* ptr) {
    if (others) {
    } else {
      others = new T**[1024]{nullptr};
    }
    uint32_t idx = utf_4byte_h(a, b, c, d);
    uint32_t x = idx >> 11;
    uint32_t y = idx & 2047;
    if (others[x]) {
    } else {
      others[x] = new T*[2048]{nullptr};
    }
    others[x][y] = ptr;
  }
  inline void add_4byte_el(uint32_t bytes, T* ptr = nullptr) {
    byte a = bytes >> 24;
    switch (utf_bytes(a)) {
      default:
        [[likely]] add_element(a, ptr);
        return;
        {
          case 2:
            byte b = bytes >> 16;
            add_element(a, b, ptr);
            return;
        }
        {
          case 3:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            add_element(a, b, c, ptr);
            return;
        }
        {
          case 4:
            byte b = bytes >> 16;
            byte c = bytes >> 8;
            add_element(a, b, c, bytes, ptr);
            return;
        }
    }
  }
  inline void add_rev4byte_el(uint32_t bytes, T* ptr = nullptr) {
    byte a = bytes;
    switch (utf_bytes(a)) {
      default:
        [[likely]] add_element(a, ptr);
        return;
        {
          case 2:
            byte b = bytes >> 8;
            add_element(a, b, ptr);
            return;
        }
        {
          case 3:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            add_element(a, b, c, ptr);
            return;
        }
        {
          case 4:
            byte b = bytes >> 8;
            byte c = bytes >> 16;
            byte d = bytes >> 24;
            add_element(a, b, c, d, ptr);
            return;
        }
    }
  }

  inline void shrink_to_fit() {
    if (latin) {
      bool empty = true;
      for (auto i = 0; i < 2048; ++i) {
        if ((*latin)[i]) {
          empty = false;
          break;
        }
      }
      if (empty) {
        delete[] latin;
        latin = nullptr;
      }
    }
    if (bmp) {
      for (auto i = 0; i < 32; ++i) {
        bool empty = true;
        for (auto j = 0; j < 2048; ++j) {
          if (bmp[i][j]) {
            empty = false;
            break;
          }
        }
        if (empty) {
          delete[] bmp[i];
          bmp[i] = nullptr;
        }
      }
      bool empty = true;
      for (auto i = 0; i < 32; ++i) {
        if (bmp[i]) {
          empty = false;
          break;
        }
      }
      if (empty) {
        delete[] bmp;
      }
    }
    if (others) {
      for (auto i = 0; i < 1024; ++i) {
        bool empty = true;
        for (auto j = 0; j < 2048; ++j) {
          if (others[i][j]) {
            empty = false;
            break;
          }
        }
        if (empty) {
          delete[] others[i];
          others[i] = nullptr;
        }
      }
      bool empty = true;
      for (auto i = 0; i < 1024; ++i) {
        if (others[i]) {
          empty = false;
          break;
        }
      }
      if (empty) {
        delete[] others;
      }
    }
  }

  inline utf8_ptrmap()
      : ascii(), latin(nullptr), bmp(nullptr), others(nullptr) {}
  inline utf8_ptrmap(const utf8_ptrmap& other)
      : ascii(),
        latin(other.latin ? new T*[2048]{nullptr} : nullptr),
        bmp(other.bmp ? new T**[32]{nullptr} : nullptr),
        others(other.others ? new T**[1024]{nullptr} : nullptr) {
    std::memcpy(&ascii[0], &other.ascii[0], sizeof(T* [256]));
    if (latin) {
      std::memcpy(latin, other.latin, sizeof(T* [2048]));
    }
    if (bmp) {
      for (auto i = 0; i < 32; ++i) {
        if (other.bmp[i]) {
          bmp[i] = new T*[2048]{nullptr};
          std::memcpy(bmp[i], other.bmp[i], sizeof(T* [2048]));
        }
      }
    }
    if (others) {
      for (auto i = 0; i < 1024; ++i) {
        if (other.others[i]) {
          others[i] = new T*[2048]{nullptr};
          std::memcpy(others[i], other.others[i], sizeof(T* [2048]));
        }
      }
    }
  }
  inline utf8_ptrmap& operator=(const utf8_ptrmap& other) {
    std::memcpy(&ascii[0], &other.ascii[0], sizeof(T* [256]));
    if (latin) {
      if (other.latin) {
        std::memcpy(latin, other.latin, sizeof(T* [2048]));
      } else {
        delete[] latin;
        latin = nullptr;
      }
    } else if (other.latin) {
      latin = new T*[2048]{nullptr};
      std::memcpy(latin, other.latin, sizeof(T* [2048]));
    }
    if (bmp) {
      if (other.bmp) {
        for (auto i = 0; i < 32; ++i) {
          if (bmp[i]) {
            if (other.bmp[i]) {
              std::memcpy(bmp[i], other.bmp[i], sizeof(T* [2048]));
            } else {
              delete[] bmp[i];
              bmp[i] = nullptr;
            }
          } else if (other.bmp[i]) {
            bmp[i] = new T*[2048];
            std::memcpy(bmp[i], other.bmp[i], sizeof(T* [2048]));
          }
        }
      } else {
        for (auto i = 0; i < 32; ++i) {
          if (bmp[i]) {
            delete[] bmp[i];
            bmp[i] = nullptr;
          }
        }
        delete[] bmp;
        bmp = nullptr;
      }
    } else if (other.bmp) {
      bmp = new T**[32]{nullptr};
      for (auto i = 0; i < 32; ++i) {
        if (other.bmp[i]) {
          bmp[i] = new T*[2048];
          std::memcpy(bmp[i], other.bmp[i], sizeof(T* [2048]));
        }
      }
    }
    if (others) {
      if (other.others) {
        for (auto i = 0; i < 1024; ++i) {
          if (others[i]) {
            if (other.others[i]) {
              std::memcpy(others[i], other.others[i], sizeof(T* [2048]));
            } else {
              delete[] others[i];
              others[i] = nullptr;
            }
          } else if (other.others[i]) {
            others[i] = new T*[2048];
            std::memcpy(others[i], other.others[i], sizeof(T* [2048]));
          }
        }
      } else {
        for (auto i = 0; i < 1024; ++i) {
          if (others[i]) {
            delete[] others[i];
            others[i] = nullptr;
          }
        }
        delete[] others;
        others = nullptr;
      }
    } else if (other.others) {
      others = new T**[1024]{nullptr};
      for (auto i = 0; i < 1024; ++i) {
        if (other.others[i]) {
          others[i] = new T*[2048];
          std::memcpy(others[i], other.others[i], sizeof(T* [2048]));
        }
      }
    }
    return *this;
  }

  inline utf8_ptrmap(utf8_ptrmap&& other)
      : ascii(), latin(nullptr), bmp(nullptr), others(nullptr) {
    // ADL
    std::memcpy(&ascii[0], &other.ascii[0], sizeof(T* [256]));
    using namespace std;
    swap(latin, other.latin);
    swap(bmp, other.bmp);
    swap(others, other.others);
  }
  inline utf8_ptrmap& operator=(utf8_ptrmap&& other) {
    std::memcpy(&ascii[0], &other.ascii[0], sizeof(T* [256]));
    using namespace std;
    swap(latin, other.latin);
    swap(bmp, other.bmp);
    swap(others, other.others);
    return *this;
  }
  inline friend void swap(utf8_ptrmap& a, utf8_ptrmap& b) {
    T* tmp[256];
    std::memcpy(&tmp[0], &a.ascii[0], sizeof(T* [256]));
    std::memcpy(&a.ascii[0], &b.ascii[0], sizeof(T* [256]));
    std::memcpy(&b.ascii[0], &tmp[0], sizeof(T* [256]));
    using namespace std;
    swap(a.latin, b.latin);
    swap(a.bmp, b.bmp);
    swap(a.others, b.others);
  }

  ~utf8_ptrmap() {
    if (latin) {
      delete[] latin;
    }
    if (bmp) {
      for (auto i = 0; i < 32; i++) {
        if (bmp[i]) {
          delete[] bmp[i];
        }
      }
      delete[] bmp;
    }
    if (others) {
      for (auto i = 0; i < 1024; i++) {
        if (others[i]) {
          delete[] others[i];
        }
      }
      delete[] others;
    }
  }

 protected:
  T* ascii[256];
  T** latin = nullptr;    // pointer to 2048 pointer array
  T*** bmp = nullptr;     // pointer to array 32 of pointer to array 2048 of
                          // pointer to T
  T*** others = nullptr;  // pointer to array 1024 of pointer to

  // array 2048 of pointer to T
};

// idx is left at end of code point (not past the end)
// defautl order is byte 4 byte 3 byte 2 byte 1 (reverse)
uint32_t get_utf8_n_inc(const std::string& str, uint32_t& idx) {
  uint32_t utf8_char = static_cast<byte>(str[idx]);
  switch (utf_bytes(str[idx])) {
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
  switch (utf_bytes(a)) {
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
        switch (utf_bytes(next_char)) {
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
    op() : opt('c'), data(0), gen(-1), lb(nullptr), rb(nullptr) {}
    uint32_t opt;
    uint32_t data;
    int64_t gen = -1;
    op* lb;
    op* rb;
  };
  // fragment of full nfa, tracks an op and a list of dangling connections
  struct nfa_frag {
    nfa_frag() : sp(nullptr), pad(0), start(nullptr), end(nullptr) {}
    nfa_frag(op* spos, op** st, op** ed) : sp(spos), start(st), end(ed) {}
    nfa_frag(op& spos) : sp(&spos), start(&(spos.lb)), end(&(spos.lb)) {}

    op** start;
    op** end;
    op* sp;
    // pad it out to a neat 32 bytes (should benchmark to see if it matters)
   private:
    int64_t pad = 0;
  };

  struct thread {
    thread() : ops(nullptr), m_loc(0) {}
    thread(op* o, uint32_t n = 0) : ops(o), m_loc(n) {}
    thread(op* o, std::vector<uint32_t>&& relay) : ops(o), m_loc(relay) {}
    op* ops;
    std::vector<uint32_t> m_loc;
  };

  struct cache_element {
    void addop(op* nxtop) {}
    void resolve_op(std::vector<thread>& pool, thread t, uint32_t i) {}
    void dfa_states() {}
    cache_element* step(const std::string& s, uint32_t i) {
      uint32_t i_c = i;
      uint32_t utf8 = get_utf8_n_inc(s, i_c);
      if (filter.test_rev4byte(utf8)) {
        return next_state.set_rev4byte(utf8);
      }
      return next_state(255);
    }
    const cache_element* step(std::string& s, uint32_t i) const {
      uint32_t utf8 = get_utf8_n_inc(s, i);
      if (filter.test_rev4byte(utf8)) {
        return next_state.get_rev4byte(utf8);
      }
      return next_state(255);
    }

    static void resolve_split(hybrid_set& list, op* nxt, op* vec_start,
                              utf8_bitmap& filter,
                              std::vector<utf8_bitmap>& classes) {
      std::vector<op*> resolve_list;
      resolve_list.reserve(8);  // picked a magic number 64 bytes
      resolve_list.emplace_back(nxt);
      do {
        if (list.test(resolve_list.back() - vec_start)) {
          resolve_list.pop_back();
          continue;
        }
        list.insert(resolve_list.back() - vec_start);
        auto& op = *resolve_list.back();
        if (op.opt == op::optype::SPLIT) {
          resolve_list.emplace_back(op.lb);
          resolve_list.emplace_back(op.rb);
        } else {
          if (op.opt == op::optype::CHAR) {
            filter.insert_rev4byte(op.data);
          } else if (op.opt == op::optype::SPLIT) {
            filter |= classes[op.data];
          }
          resolve_list.pop_back();
        }

      } while (resolve_list.size());
    }
    static void resolve_split(hybrid_set& list, op* nxt, op* vec_start) {
      std::vector<op*> resolve_list;
      resolve_list.reserve(8);  // picked a magic number 64 bytes
      resolve_list.emplace_back(nxt);
      do {
        if (list.test(resolve_list.back() - vec_start)) {
          resolve_list.pop_back();
          continue;
        }
        list.insert(resolve_list.back() - vec_start);
        auto& op = *resolve_list.back();
        if (op.opt == op::optype::SPLIT) {
          resolve_list.emplace_back(op.lb);
          resolve_list.emplace_back(op.rb);
        } else {
          resolve_list.pop_back();
        }
      } while (resolve_list.size());
    }
    cache_element construct_next(uint32_t utf8, std::vector<op>& oplist,
                                 std::vector<utf8_bitmap>& classes) {
      cache_element new_ce{};
      new_ce.ops.set_range(oplist.size());
      for (uint32_t j = 0; j < ops.size(); ++j) {
        auto& op = oplist[ops[j]];
        switch (op.opt) {
          default:
            continue;
          case op::optype::CHAR:
            if (utf8 == op.data) {
              resolve_split(new_ce.ops, op.lb, oplist.data(), new_ce.filter,
                            classes);
            }
            break;
          case op::optype::CLASS:
            if (classes[op.data].test_rev4byte(utf8)) {
            } else {
              break;
            }
          case op::optype::ANY:
            resolve_split(new_ce.ops, op.lb, oplist.data(), new_ce.filter,
                          classes);
            break;
          case op::optype::MATCH:
            ///??? idk ignore it?
            break;
        }
      }
      return new_ce;
    }
    friend void swap(cache_element a, cache_element b) {
      using namespace std;
      swap(a.filter, b.filter);
      swap(a.next_state, b.next_state);
      swap(a.ops, b.ops);
    }
    utf8_bitmap filter;
    utf8_ptrmap<cache_element>
        next_state;  // use element at position 255 for everything not in filter
                     // (usually nullptr unless wildcard)
    hybrid_set ops;  // point to all the operations? incase we need to walk and
                     // generate the next state its bitvector should also be the
                     // key / tag of a cached_state
  };
  // note cache snaps to next power of two size
  struct cache {
    cache_element& operator[](uint32_t idx) {
      return ring_buffer[(idx + start) & (size_less1)];
    }
    const cache_element& operator[](uint32_t idx) const {
      return ring_buffer[(idx + start) & (size_less1)];
    }
    void pop() {
      tree.erase(tree.find(this->operator[](0).ops));
      start += 1;
      start = start & size_less1;
    }
    void reset() {
      start = 0;
      end = 0;
      tree.clear();
    }
    void push(cache_element& c) {
      if (((end + 1) & size_less1) == start) {
        pop();
        overflow_c += 1;
        if (overflow_c == overflow_lim) {
          reset();
          rebuild_c += 1;
        }
      }
      tree.insert({c.ops, end});
      ring_buffer[end] = c;
      end += 1;
      end = end & size_less1;
    }
    void push(cache_element&& c) {
      if ((end + 1) == start) {
        pop();
        overflow_c += 1;
        if (overflow_c == overflow_lim) {
          reset();
          rebuild_c += 1;
        }
      }
      tree.insert({c.ops, end});
      ring_buffer[end] = c;
      end += 1;
      end = end & size_less1;
    }
    // this also resets the cache!
    void resize(uint32_t n) {
      uint32_t log2_next_2pow = 32 - countl_zero(n + 1) - 1;
      uint32_t new_size = 1 << log2_next_2pow;
      ring_buffer.reserve(new_size);
      ring_buffer.resize(new_size);
      size_less1 = new_size - 1;
      start = 0;
      end = 0;
      tree.clear();
    }
    bool test(const hybrid_set& rep) { return tree.count(rep); }
    cache_element& operator()(const hybrid_set& rep) {
      return this->operator[](tree.find(rep)->second);
    }
    template <bool Unanchored>
    bool run(const std::string& s, uint32_t& i, std::vector<op>& oplist,
             uint32_t st_op, std::vector<utf8_bitmap>& classes, int64_t& last) {
      auto& current_cel = strt;  // ensure first element has been pushed
      if (current_cel.ops.test(oplist.size() - 1)) {
        return true;
      }
      for (uint32_t idx = i; idx < s.size();) {
        cache_element* nxt = current_cel.step(s, idx);
        if (nxt) {
          current_cel = *nxt;
        } else {
          if (rebuild_lim == rebuild_c) {
            idx += utf_bytes(s[idx]);
            i = idx;
            last = &current_cel - &ring_buffer[start];
            return false;
          }
          uint32_t i_c = idx;
          uint32_t utf8 = get_utf8_n_inc(s, i_c);
          // this should be checking the cache to see if it is there and tying
          // things up
          auto tmp = current_cel.construct_next(utf8, oplist, classes);
          if (test(tmp.ops)) {
            auto pos = &(this->operator()(tmp.ops));
            if (current_cel.filter.test(utf8)) {
              current_cel.next_state.add_rev4byte_el(utf8, pos);
            } else {
              current_cel.next_state.add_element(255, pos);
            }
            current_cel = *pos;
          } else {
            push(std::move(tmp));
            auto pos = &ring_buffer[((end - 1) & size_less1)];
            if (current_cel.filter.test(utf8)) {
              current_cel.next_state.add_rev4byte_el(utf8, pos);
            } else {
              current_cel.next_state.add_element(255, pos);
            }
            current_cel = *pos;
          }
        }
        if (current_cel.ops.test(oplist.size() - 1)) {
          idx += utf_bytes(s[idx]);
          i = idx;
          return true;
        }
        if constexpr (Unanchored) {
          cache_element::resolve_split(current_cel.ops, &oplist[st_op],
                                       oplist.data(), current_cel.filter,
                                       classes);
        }
        idx += utf_bytes(s[idx]);
        i = idx;
      }
      last = -1;
      return false;
    }

    void init_s(std::vector<op>& oplist, uint32_t st_op,
                std::vector<utf8_bitmap>& classes) {
      strt = cache_element{};
      strt.ops.set_range(oplist.size());
      cache_element::resolve_split(strt.ops, &oplist[st_op], oplist.data(),
                                   strt.filter, classes);
    }
    uint32_t start;
    uint32_t end;         // start and end track what is initialised
    uint32_t size_less1;  // power of two size
    uint32_t overflow_lim = 5;
    uint32_t rebuild_lim = 5;
    uint32_t overflow_c = 0;
    uint32_t rebuild_c = 0;
    cache_element strt;
    std::vector<cache_element> ring_buffer;
    std::map<hybrid_set, uint32_t, hybrid_set_comp> tree;
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
      regex_chars.insert_rev4byte(utf8_char);
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
      // ignore all nulls in a string, what the hell is it doing here
      if (regex[i] == 0) {
        continue;
      }
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
      switch (utf_bytes(regex[i])) {
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
          switch (utf_bytes(tokenised[i])) {
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
          regex_chars |= classes[class_c];
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

  void create_prog_ruin() {
    prog_ruin.reserve(prog.size());
    std::vector<uint32_t> save_count(prog.size());
    if (prog[0].opt == op::optype::SAVE) {
      save_count[0] = 1;
      auto lp = prog[0].lb;
      while (lp->opt == op::optype::SAVE) {
        lp = lp->lb;
      }
      prog_ruin_start = lp - prog.data();
    }
    for (uint32_t i = 1; i < prog.size(); ++i) {
      save_count[i] = save_count[i - 1];
      if (prog[i].opt == op::optype::SAVE) {
        save_count[i] += 1;
      }
    }
    prog_ruin_start -= save_count[prog_ruin_start];
    for (uint32_t i = 0; i < prog.size(); ++i) {
      if (prog[i].opt != op::optype::SAVE) {
        prog_ruin.emplace_back(prog[i]);
        if (prog_ruin.back().lb) {
          auto lp = prog[i].lb;
          while (lp->opt == op::optype::SAVE) {
            lp = lp->lb;
          }
          prog_ruin.back().lb = prog_ruin.data();
          prog_ruin.back().lb += lp - prog.data();
          auto diff_to_s = prog_ruin.back().lb - prog_ruin.data();
          prog_ruin.back().lb -= save_count[diff_to_s];
        }
        if (prog_ruin.back().rb) {
          auto lp = prog[i].rb;
          while (lp->opt == op::optype::SAVE) {
            lp = lp->lb;
          }
          prog_ruin.back().rb = prog_ruin.data();
          prog_ruin.back().rb += lp - prog.data();
          auto diff_to_s = prog_ruin.back().rb - prog_ruin.data();
          prog_ruin.back().rb -= save_count[diff_to_s];
        }
      }
    }
  }
  void clear_compile_info() {
    f_stack.clear();
    prog.clear();
    prog_ruin.clear();
    classes.clear();
    save_points = 0;
    prog_ruin_start = 0;
  }

 public:
  nfa_vm(const std::string& regex)
      : prog(),
        classes(),
        regex_chars(),
        save_points(),
        f_stack(),
        gen_id(),
        cur(),
        nxt(),
        matches() {
    std::string tokens;
    tokenise(regex, tokens);
    // std::cout << tokens << std::endl;
    std::string notquitepostfix;
    nearly_shunting_yard(tokens, notquitepostfix);
    // std::cout << notquitepostfix << std::endl;
    compile_nfa_sg(notquitepostfix);
    create_prog_ruin();
    mem.resize(32);
    mem.init_s(prog_ruin, prog_ruin_start, classes);
  }
  nfa_vm() = delete;
  void recompile(const std::string& regex) {
    clear_compile_info();
    std::string tokens;
    tokenise(regex, tokens);
    std::string notquitepostfix;
    nearly_shunting_yard(tokens, notquitepostfix);
    compile_nfa_sg(notquitepostfix);
    create_prog_ruin();
  }
  void print_classes() {
    std::cout << "Classes:" << std::endl;
    for (auto i = 0; i < classes.size(); ++i) {
      std::cout << "[" << i << "]" << "\t[" << classes[i] << "]" << std::endl;
    }
  }

  void print_oplist(std::vector<op>& oplist) {
    std::cout << "Printing NFA ops" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    for (auto i = 0; i < oplist.size(); ++i) {
      std::string ch = "";
      switch (oplist[i].opt) {
        case op::optype::CHAR:
          ch = uint32_revto_utf8(oplist[i].data);
          std::cout << "[" << i << "]\t";
          std::cout << ch;
          std::cout << "\t\tjmp " << oplist[i].lb - oplist.data();
          break;
        case op::optype::MATCH:
          std::cout << "[" << i << "]\t" << "match";
          break;
        case op::optype::SPLIT:
          std::cout << "[" << i << "]\t" << "split";
          std::cout << "\t\t" << oplist[i].lb - oplist.data() << ", "
                    << oplist[i].rb - oplist.data();
          break;
        case op::optype::ANY:
          std::cout << "[" << i << "]\t" << "any";
          std::cout << "\t\tjmp " << oplist[i].lb - oplist.data();
          break;
        case op::optype::SAVE:
          std::cout << "[" << i << "]\t" << "save  " << oplist[i].data;
          std::cout << "\t\tjmp " << oplist[i].lb - oplist.data();
          break;
        case op::optype::CLASS:
          std::cout << "[" << i << "]\t" << "class " << oplist[i].data;
          std::cout << "\t\tjmp " << oplist[i].lb - oplist.data();
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

  void print_prog() { print_oplist(prog); }
  void print_prog_ruin() {
    std::cout << "Starting op: " << prog_ruin_start << std::endl;
    print_oplist(prog_ruin);
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

  void new_thread(std::vector<thread>& pool, thread t, uint32_t i,
                  bitvector& bitvec) {
    auto& op = *t.ops;
    if (op.gen == gen_id) {
      return;
    }
    op.gen = gen_id;
    if (op.opt == op::optype::SPLIT) {
      new_thread(pool, thread(op.lb, std::move(t.m_loc)), i, bitvec);
      new_thread(pool, thread(op.rb, std::move(t.m_loc)), i, bitvec);
      return;
    }
    if (op.opt == op::optype::SAVE) {
      t.m_loc[op.data] = i + 1;
      new_thread(pool, thread(op.lb, std::move(t.m_loc)), i, bitvec);
      return;
    }
    bitvec.set(t.ops - prog.data());
    pool.emplace_back(t);
  }

  void clear_match_info() {
    cur.clear();
    nxt.clear();
    matches.clear();
    gen_id = 0;
  }

  template <bool Unanchored = false>
  bool test(const std::string& str) {
    mem.rebuild_c = 0;
    mem.overflow_c = 0;
    bool match = false;
    bool cache = true;
    hybrid_set current{};
    hybrid_set next{};
    for (uint32_t i = 0; i < str.size(); ++i) {
      if (cache) {
        int64_t last_c = 0;
        match = mem.run<Unanchored>(str, i, prog_ruin, prog_ruin_start, classes,
                                    last_c);
        if (match) {
          return match;
        }
        if (last_c != -1) {
          current.set_range(prog_ruin.size());
          next.set_range(prog_ruin.size());
          current = mem[last_c].ops;
        } else {
          return false;
        }
      }
      uint32_t i_c = i;
      uint32_t utf8 = get_utf8_n_inc(str, i_c);
      if constexpr (Unanchored) {
        cache_element::resolve_split(current, &prog_ruin[prog_ruin_start],
                                     prog_ruin.data());
      }

      for (uint32_t j = 0; j < current.size(); ++j) {
        auto& op = prog_ruin[current[j]];
        switch (op.opt) {
          default:
            continue;
          case op::optype::CHAR:
            if (utf8 == op.data) {  // fallthrough
            } else {
              break;
            }
          case op::optype::CLASS:
            if (classes[op.data].test_rev4byte(utf8)) {  // fallthrough
            } else {
              break;
            }
          case op::optype::ANY:
            cache_element::resolve_split(next, op.lb, prog_ruin.data());
            break;
          case op::optype::MATCH:
            return true;
        }
      }
      {
        using namespace std;
        swap(current, next);
        next.clear();
      }
    }
    return current.test(prog_ruin.size() - 1);
  }

  template <bool Unanchored = false, bool Match_one = true>
  bool match(const std::string& str) {
    clear_match_info();
    cur.reserve(prog.size());
    nxt.reserve(prog.size());
    bool match = false;
    new_thread(cur, thread(&prog[0], save_points), -1);
    for (uint32_t i = 0; i < str.size();) {
      gen_id = i;
      if constexpr (Unanchored) {
        new_thread(cur, thread(&prog[0], save_points), i - 1);
      }
      uint32_t i_c = i;  // temporary to avoid change in i
      for (uint32_t j = 0; j < cur.size(); ++j) {
        auto& op = *cur[j].ops;
        uint32_t utf8;
        switch (op.opt) {
          default:
            continue;
          case op::optype::CHAR:
            i_c = i;
            utf8 = get_utf8_n_inc(str, i_c);
            if (utf8 != op.data) {
              continue;
            }
            new_thread(nxt, thread(op.lb, std::move(cur[j].m_loc)), i);
            continue;
          case op::optype::CLASS:
            i_c = i;
            utf8 = get_utf8_n_inc(str, i_c);
            if (classes[op.data].test_rev4byte(utf8)) {
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
      i += utf_bytes(str[i]);
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

  void free_memory(bool free_prog_vec = false) {
    if (free_prog_vec) {
      prog = std::move(std::vector<op>(0));
      classes = std::move(std::vector<utf8_bitmap>(0));
      save_points = 0;  // for consistency sake
    }
    // matching will still work but might have a bit of warmup
    f_stack = std::move(std::vector<nfa_frag>(0));
    cur = std::move(std::vector<thread>(0));
    nxt = std::move(std::vector<thread>(0));
    matches = std::move(std::vector<std::vector<uint32_t>>(0));
  }

  // protected:
  //  nfa related
  std::vector<op> prog;
  std::vector<op> prog_ruin;  // prog without save op
  uint32_t prog_ruin_start;   // track where the first instruction should be
  std::vector<utf8_bitmap> classes;
  utf8_bitmap regex_chars;
  uint32_t save_points = 0;
  // compilation only
  std::vector<nfa_frag> f_stack;
  // matching related
  uint64_t gen_id = 0;
  std::vector<thread> cur{};
  std::vector<thread> nxt{};
  cache mem;
  std::vector<std::vector<uint32_t>> matches;
};

}  // namespace simple_regex
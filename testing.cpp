#include <bit>
#include <chrono>
#include <iostream>
#include <regex>

#include "regex.hpp"
// uint32_t utf8_codepoint(){}
uint32_t roughie_mac_toughie(uint32_t x) {
  int n = 32;
  unsigned y;

  y = x >> 16;
  if (y != 0) {
    n = n - 16;
    x = y;
  }
  y = x >> 8;
  if (y != 0) {
    n = n - 8;
    x = y;
  }
  y = x >> 4;
  if (y != 0) {
    n = n - 4;
    x = y;
  }
  y = x >> 2;
  if (y != 0) {
    n = n - 2;
    x = y;
  }
  y = x >> 1;
  if (y != 0) return n - 2;
  return n - x;
}

int main() {
  std::string regex = "f.*l ";
  simple_regex::nfa_vm oh(regex);
  std::regex re(regex, std::regex::optimize);  // compilation should happen here
  // for fair comparison
  // shakespeare quote 1
  std::string str =
      "If music be the food of love, play on;Give me excess of it, that, "
      "surfeiting, The appetite may sicken, and so die.That strain again !it "
      "had a dying fall : O, it came o'er my ear like the sweet sound, That "
      "breathes upon a bank of violets, Stealing and giving odour !Enough; no "
      "more : 'Tis not so sweet now as it was before. O spirit of love !how "
      "quick and fresh art thou, That, notwithstanding thy capacity Receiveth "
      "as the sea, nought enters there, Of what validity and pitch soe'er, But "
      "falls into abatement and low price, Even in a minute: so full of shapes "
      "is fancy That it alone is high fantastical.";
  // oh.match<true,false>(str, true)
  std::cout << "Regex: " << regex << std::endl;
  std::cout << "Shakespeare quote 1 test" << std::endl;
  std::cout << "----------------------" << std::endl;
  for (auto i = 0; i < 3; i++) {
    auto start = std::chrono::steady_clock::now();
    bool match = oh.test<true>(str);
    auto end = std::chrono::steady_clock::now();
    auto time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "run " << i << " simple_regex took:\t" << time.count()
              << " ns to check if match exists, output:" << match << std::endl;
    start = std::chrono::steady_clock::now();
    match = std::regex_search(str, re);
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "run " << i << " std::regex took:\t\t" << time.count()
              << " ns to check if match exists, output:" << match << std::endl;
  }
  std::string str2 =
      "All the world's a stage, And all the men and women merely players; They "
      "have their exits and their entrances; And one man in his time plays "
      "many parts, His acts being seven ages.";
  std::cout << "Shakespeare quote 1 test" << std::endl;
  std::cout << "----------------------" << std::endl;
  for (auto i = 0; i < 3; i++) {
    auto start = std::chrono::steady_clock::now();
    bool match = oh.test<true>(str2);
    auto end = std::chrono::steady_clock::now();
    auto time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "run " << i << " simple_regex took:\t" << time.count()
              << " ns to check if match exists, output:" << match << std::endl;
    start = std::chrono::steady_clock::now();
    match = std::regex_search(str2, re);
    end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "run " << i << " std::regex took:\t\t" << time.count()
              << " ns to check if match exists, output:" << match << std::endl;
  }
}
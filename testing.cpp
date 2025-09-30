#include <iostream>

#include "regex.hpp"

// uint32_t utf8_codepoint(){}

int main() {
  std::string basics = "(a(b))(c|😊)(p|[😊d])";
  simple_regex::nfa_vm oh(basics);
  oh.print_prog();
  std::string simple = "bbcab😊cac😊bacbcabab😊😊ababafdbab";
  oh.match<true>(simple, true);
}
#include <iostream>

#include "regex.hpp"

// uint32_t utf8_codepoint(){}

int main() {
  std::string basics = "a+";
  simple_regex::nfa_vm oh(basics);
  oh.print_prog();
  std::string simple = "aa?";
  oh.match<true>(simple, true);
}

/*
Bitmap of characters in regex excluding wildcard (like classess but not)
Bitmap length of entire program and so can put together NFA states
Figure out a hashtable just save an index with cached ones lmao as in with transition to cached one from cache write next index?
// or nvm
hash on leading zeros or size? I think leading zeros?
// or count up to it!
then ring buffer of cache states with first in first out (recency bias) and also use pointers for easy swapping of order (maybe if state is activated swap with preceding but how to fix the end issue with ring buffers? maybe if first do nothing?)
*/
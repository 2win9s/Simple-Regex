## A simple NFA Regex Engine 

Inspired by the [Russell Cox article on regex engines](https://swtch.com/~rsc/regexp/regexp1.html)

A lightweight (though a lot longer than the articles 400 line C code) regex engine that supports ()[]*?+. and \ for escape.
Works with 7 bit ASCII and UTF-8 encodings all in a single header file.

The engine uses Thompson's algorithm for the nfa.
Compilation of the regex is in 3 passes (to make it easier and clearer), tokenisation where null tokens are inserted instead of implicit concatentation, a modified shunting yard algorithm (the output isn't truly postix, left classes and capture groups as is) and finally the compilation. A single pass method may require a bit more juggling of stacks to simulate recursion or might make the already modified shunting yard scheme less extensible. Though the tokenisation step is probably superfluous.

Character classes are handled with a handrolled bitmap specifically for UTF-8 code points.

testing.cpp output():

```
Regex: f.*l 
Shakespeare quote 1 test
----------------------
run 0 simple_regex took:        9652 ns to check if match exists, output:1
run 0 std::regex took:          37934 ns to check if match exists, output:1
run 1 simple_regex took:        2026 ns to check if match exists, output:1
run 1 std::regex took:          11336 ns to check if match exists, output:1
run 2 simple_regex took:        1994 ns to check if match exists, output:1
run 2 std::regex took:          11217 ns to check if match exists, output:1

Shakespeare quote 2 test
----------------------
run 0 simple_regex took:        2393 ns to check if match exists, output:0
run 0 std::regex took:          1468 ns to check if match exists, output:0
run 1 simple_regex took:        2183 ns to check if match exists, output:0
run 1 std::regex took:          1433 ns to check if match exists, output:0
run 2 simple_regex took:        2202 ns to check if match exists, output:0
run 2 std::regex took:          1420 ns to check if match exists, output:0
```

With caching provided the fixed size cache is enough i.e. the regex is small/simple enough it seems to behave well for some text after warmup.

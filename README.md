## A simple NFA Regex Engine 

Inspired by the [Russell Cox article on regex engines](https://swtch.com/~rsc/regexp/regexp1.html)

A lightweight regex engine that supports ()[]*?+. and \ for escape, no backreferences.
Works with 7 bit ASCII and UTF-8 encodings all in a single header file.

The engine uses Thompson's algorithm for the nfa.
Compilation of the regex is in 3 passes (to make it easier and clearer), tokenisation where null tokens are inserted instead of implicit concatentation, a modified shunting yard algorithm (the output isn't truly postix, left classes and capture groups as is) and finally the compilation. A single pass method may require a bit more juggling of stacks to simulate recursion or might make the already modified shunting yard scheme less extensible. Though the tokenisation step is probably superfluous.

Character classes are handled with a handrolled bitmap specifically for UTF-8 code points (though it is optimised for ascii).



## A simple NFA Regex Engine 

Inspired by the [Russell Cox article on regex engines](https://swtch.com/~rsc/regexp/regexp1.html)
Supports ()[]*?+. and \ for escape, no backreferences.
Works with 7 bit ASCII and UTF8 encodings all in a single header file.

The engine uses Thompson's algorithm for the nfa.
Compilation of the regex is in 3 passes (to make it easier and clearer), tokenisation where null tokens are inserted instead of implicit concatentation, a modified shunting yard algorithm (the output isn't truly postix) and finally the compilation.

Character classes are handled with a handrolled bitmap for UTF8 code points (though it is optimised for ascii).



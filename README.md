# max--

A compiler for my own language I invented. I haven't made a formal spec website or sheet yet, but I probably will at some point. It'll be kind of like C but a bit nicer (no pointers :) ) Also, it will have heterogeneous containers (like lists in python), mutable by by default types, is statically typed, and... I'll think of more stuff.

The point was to help me learn both how the bootstrapping process works, how to self-host (just for fun), and the fun of inventing my own language.

**UPDATE:** I am not bootstrapping this anymore. It's too much work. I just want to ship a really minimal language that compiles successfully at this point. Might bootstrap one day later.

Right now I'm focusing on max-1, a simple, minimal language spec I can use to help self-host the rest of the compiler. I'll write it in C (I was going to do assembly for pureism but that sounded a bit too painful).

The language will only compile to ARM64 for mac. I could have used LLVM as my target but I wanted to also learn about the backend compilation process.

## Update:
I don't know if I can bootstrap. I chose to implement the backend myself. What a world of pain I was not ready for. Had I used LLVM
maybe bootstrapping was feasible but now I'm not so sure. max-1 might be it for this. :(

## Current Progress

Currently supports:

Frontend:
- lexes functions and integer declarations, and parses integer declarations/assignments, as well as arithmetic expressions.
- A scope resolution pass.
- A definite assignment analysis pass.
- A max-- intermediate representation (MIR) generation pass.

Backend:

**UPDATE:** Scrapping my entire backend, it sucks. Going to rewrite the whole thing in C++. Oh well!

- Currently working on. Learning about how backends work in general.
- Made an IR verification pass, frame layout pass, and LIR generation pass. Will do codegen pass, and consider doing 1-2 optimization passes later. Also will probably do register allocation at some point. Then I think I'm done with this for a bit.

- Can correctly emit ARM64 for declarations and assignments. 
    - First, run the maxc binary to compile your code into a .s file. Then, you can see exit code output using `./assm.sh`.

Lots of fixes in the making; will refactor error handling of parsing. I chose ARM64 now instead of x86.

## Basic setup

- Run `./compile.sh`. It'll take care of everything for you I think :)

If you'd rather configure some stuff:
```
cmake -S . -B build
cmake --build build
```


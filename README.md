# max--

A compiler for my own language I invented. I haven't made a formal spec website or sheet yet, but I probably will at some point. It'll be kind of like C but a bit nicer (no pointers :) ) Also, it will have heterogeneous containers (like lists in python), mutable by by default types, is statically typed, and... I'll think of more stuff.

The point was to help me learn both how the bootstrapping process works, how to self-host (just for fun), and the fun of inventing my own language.


Right now I'm focusing on max-1, a simple, minimal language spec I can use to help self-host the rest of the compiler. I'll write it in C (I was going to do assembly for pureism but that sounded a bit too painful).

The language will only compile to ARM64 for mac. I could have used LLVM as my target but I wanted to also learn about the backend compilation process.

## Updates:

**UPDATE 2 (01/18/2026):** I am much further along. Entire backend was rewritten in C++. Working on function declarations/function calls going through entire pipeline. After this, I think I'm done for a while. Adding more features would make it more of "my own" language but I'm burnt out, I need a break from this for a little bit.

Lots of thoughts. Currently values are mutable by default but I think that's bad design; they should be immutable by default without a keyword. That wouldn't be too hard but I'm too lazy right now.

And wow. This took a lot longer than I thought. Hundreds of hours at this point for a minimal language, and I skipped some passes that verify the integrity of the code. Crazy stuff!

**UPDATE:** Scrapping my entire backend, it sucks. Going to rewrite the whole thing in C++. Oh well!

**UPDATE:** I am not bootstrapping this anymore. It's too much work. I just want to ship a really minimal language that compiles successfully at this point. Might bootstrap one day later.

I don't know if I can bootstrap. I chose to implement the backend myself. What a world of pain I was not ready for. Had I used LLVM
maybe bootstrapping was feasible but now I'm not so sure. max-1 might be it for this. :(

## Current Progress

Currently supports:

Features:
- Int type
- Assignments
- Expressions
- Declarations
- If/else (with block)
- While loop (with block)
- Functions (in progress: works upto max-- IR generation.)

Frontend:
- Lexer
- Parser
- Scope/Symbol resolution
- Simple Intra-block linear definite assignment
- max-- intermediate representation (MIR) gen

Backend:
- MIR verification

(Rest is in C++)
- Lower IR generation (LIR)
- Linear scan register allocation (phew! tough one.)
- Frame Layout
- ARM emission


- Currently working on: learning about how backends work in general.
- Made an IR verification pass, frame layout pass, and LIR generation pass. Will do codegen pass, and consider doing 1-2 optimization passes later. Also will probably do register allocation at some point. Then I think I'm done with this for a bit.

- Can correctly emit ARM64 for declarations and assignments. 


Lots of fixes in the making; will refactor error handling of parsing. I chose ARM64 now instead of x86.

## Basic setup

**THIS ONLY RUNS ON MACOS/ARM MACHINES. Doesn't support x86 or any other architecture.**
I configured everything for macOS. Theoretically should be fine on Linux machines, but
the compiler won't create any compileable code unless it's an arm linux machine.

- Run `./compile.sh`. This will build the compiler (the `bin/maxc` binary).

### Using the compiler
- First, run the maxc binary to compile your code into a .s file. Then, you can see exit code output using `./assm.sh`. Doesn't support any printing so there's no other way of seeing if your program works yet, haha. (Remember the exit code is mod 256)

If you'd rather configure some stuff:
```
cmake -S . -B build
cmake --build build
```


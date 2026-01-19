#pragma once
#include "codegen/backend/reg_ids.hpp"
#include "bitset/bitset.hpp"
#include <cstddef>
#include <vector>


// For variable range creation/analysis
// All unique per function.
using Pos = std::size_t;
struct Range { 
    Range(Pos start_, Pos end_): start(start_), end(end_) {}
    Pos start, end;
    VRegId vreg;
    PRegId assigned_preg;
}; // [start, end)
struct Interval { 
    std::vector<Range> ranges; /* maybe also vreg id */ 
    int start() const {
        return ranges.front().start;
    }
};

// Register allocation stuff.
enum LocKind {
    LOC_PREG,
    LOC_SLOT,
    LOC_UNINIT
};
struct Location {
    explicit Location(LocKind kind_, std::size_t id_)
        :kind(kind_), id(id_) {}
    explicit Location() {
        kind = LOC_UNINIT;
        id = SIZE_MAX;
    }

    LocKind kind;
    std::size_t id; // either slot(id) or preg(id)
};

// Created per-function.
struct RegAllocInfo {

    explicit RegAllocInfo(std::size_t num_pregs, std::size_t num_callee_regs, std::size_t max_slots, std::size_t num_vregs)
        : slot_counter(max_slots+1) {
            free_regs = BitSet(num_pregs);
            free_callee_regs = BitSet(num_callee_regs);
            locs.resize(num_vregs, Location());
        }
    
    std::vector<int> used_callee_regs;

    // free pregs while scanning
    BitSet free_regs; // x9 - x15
    BitSet free_callee_regs; // x19 - x28

    // Active intervals during scan, sorted by end ascending
    std::vector<Range*> active;
    // New slots to assign if spill
    std::size_t slot_counter;
    // Location info (maps vreg -> (preg|slot).)
    // Indexed by vreg
    std::vector<Location> locs;
};
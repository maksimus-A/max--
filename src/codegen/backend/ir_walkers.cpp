#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/visitors/mir_visitor.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"
#include <variant>
extern "C" {
    #include "codegen/ir-gen/mir.h"
}
#include <iostream>

void walk_mir_linear(BackendContext& ctx, MIRVisitor& v) {
    for (auto& func : ctx.mir_functions()) {
        v.pre_func(func);

        for (auto& block : ctx.mir_blocks(func)) {
            v.pre_block(block);

            for (auto& inst : ctx.mir_insts(block)) {
                v.visit_inst(inst);
            }
            // Visit terminator.
            v.visit_inst(block.term);

            v.post_block(block);
        }

        v.post_func(func);
    }
}

void walk_lir_linear(BackendContext& ctx, LIRVisitor& v) {
    std::size_t insts_visited = 0;
    
    for (auto& func : ctx.lir_funcs) {
        v.pre_func(func);

        for (auto& block : ctx.lir_blocks(func)) {
            v.pre_block(block);

            for (auto& inst : ctx.lir_insts(block)) {
                v.visit_inst(inst);
                insts_visited++;
            }
            v.visit_terminator(block.term);
            insts_visited++;
            //std::cout << "LIR Walker: Instructions Visited: " << insts_visited << std::endl;
            v.post_block(block);
        }
        v.post_func(func);
    }
}

// Walks instructions backwards.
void walk_lir_backwards_insts_linear(BackendContext& ctx, LIRVisitor& v) {
    std::size_t insts_visited = 0;
    
    for (auto& func : ctx.lir_funcs) {
        v.pre_func(func);

        for (auto& block : ctx.lir_blocks(func)) {
            v.pre_block(block);

            // Visit terminator first.
            std::optional<LIRTerm>& opt = block.term;
            if (opt.has_value()) {
                const LIRTerm& term = *opt;
                v.visit_inst(term);
            }
            
            for (auto it = ctx.lir_insts(block).rbegin();
                    it != ctx.lir_insts(block).rend();
                    ++it)
                {
                    auto& inst = *it;
                    v.visit_inst(inst);
                    insts_visited++;
                }
            v.visit_terminator(block.term);
            insts_visited++;
            //std::cout << "LIR Backwards Instruction Walker: Instructions Visited: " << insts_visited << std::endl;
            v.post_block(block);
        }
        v.post_func(func);
    }
}
    /*
    // Grabs i-th mir function.
    const IRFunction& get_mir_function(size_t i) {
        VecView<IRFunction> funcs = ctx.mir_functions();
        const IRFunction& f = funcs[i];
        return f;
    }

    // Grabs i-th mir block from current function.
    const IRBlock& get_mir_block(size_t i) {
        const IRFunction& f = get_mir_function(curr_func_index);
        VecView<IRBlock> blocks = ctx.mir_blocks(f);
        return blocks[i];
    }

    // Grabs i-th mir instruction from current function/block.
    const IRInstruct& get_mir_instruct(size_t i) {
        const IRBlock& b = get_mir_block(curr_block_index);
        VecView<IRInstruct> insts = ctx.mir_insts(b);
        return insts[i];
    }
    */

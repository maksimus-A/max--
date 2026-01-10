#pragma once
#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/lir/lir.hpp"

struct LIRVisitor {
    virtual ~LIRVisitor() = default;


    virtual void pre_func(const LIRFunction&) {}
    virtual void post_func(const LIRFunction&) {}

    virtual void pre_block(const LIRBlock&) {}
    virtual void post_block(const LIRBlock&) {}

    virtual void visit_inst(const LIRInstruct&) {}
    virtual void visit_terminator(const std::optional<LIRTerm>&) {}

    // Visit payloads.
    virtual void visit(const LIRLoad&) {}
    virtual void visit(const LIRStore&) {}
    virtual void visit(const LIRBinOp&) {}
    virtual void visit(const LIRConst&) {}
    virtual void visit(const LIRRet&) {}
};

void construct_cfg(BackendContext& ctx, bool debug);
void walk_lir_linear(BackendContext& ctx, LIRVisitor& v);
void walk_lir_backwards_insts_linear(BackendContext& ctx, LIRVisitor& v);

// Liveness analysis
void liveness_analysis(BackendContext& ctx, bool debug);
void regalloc(BackendContext& ctx, bool debug);
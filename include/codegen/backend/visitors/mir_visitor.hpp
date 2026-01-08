#pragma once
extern "C" {
    #include "codegen/ir-gen/mir.h"
}
#include "codegen/backend/backend_context.hpp"

struct MIRVisitor {
    virtual ~MIRVisitor() = default;

    virtual void pre_func(const IRFunction&) {}
    virtual void post_func(const IRFunction&) {}

    virtual void pre_block(const IRBlock&) {}
    virtual void post_block(const IRBlock&) {}

    virtual void visit_inst(const IRInstruct&) {}
};

void walk_mir_linear(BackendContext& ctx, MIRVisitor& v);
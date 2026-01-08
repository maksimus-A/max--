#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/visitors/lir_visitor.hpp"
#include <iostream>
#include <iterator>


struct CFGGen : LIRVisitor {
    /*
    All of this analysis will have to wait until I add branching;
    right now it would do nothing.
    */
public:
    explicit CFGGen(BackendContext& ctx_, bool debug_, std::ostream& out_)
        :ctx(ctx_), debug(debug_), out(out_) {}

    virtual ~CFGGen() = default;
    using LIRVisitor::visit;// this allows all other virtual dispatches to work even if not defined.

    void visit_inst(const LIRInstruct& inst) override {
        
    }

    // Construct succs/preds.
    void visit_terminator(const std::optional<LIRTerm>& term) override {
        if (term.has_value()) {
            std::visit([&](auto& payload) {
                visit(payload);
            }, term->pl);
        }
        // todo: add diag that term doesn't exist? it should tho.
    }

    void visit(const LIRRet& ret) override {
        // Well... I guess this doesn't do anything. Return has
        // No successors. Dang lol.
    }

private:
    BackendContext& ctx;
    bool debug;
    std::ostream& out;
};

void construct_cfg(BackendContext& ctx, bool debug) {
    CFGGen cfg_gen(ctx, debug, std::cout);
    walk_lir_linear(ctx, cfg_gen);
}
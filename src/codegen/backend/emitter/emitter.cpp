#include "codegen/backend/backend_context.hpp"
#include "codegen/backend/emitter/emitter.hpp"
#include "codegen/backend/lir/lir.hpp"
#include "codegen/backend/reg_ids.hpp"
#include "codegen/ir-gen/mir.h"
#include "common.hpp"
#include <ostream>
#include <stdint.h>


struct ARMEmitter {
public:
    explicit ARMEmitter(BackendContext& ctx_, bool debug_, std::ostream& dout_, std::ostream& armout_)
        :ctx(ctx_), debug(debug_), dout(dout_), armout(armout_) {
            curr_frame_info = nullptr;
        }

    void emit_arch_arm() {

        emit_globals();
        for (auto& f: ctx.lir_funcs) {

            curr_frame_info = ctx.get_frame_info_ptr(f.id);

            // TODO: Remove 'main' hack when real funcs exist.
            emit_main_label();
            emit_prologue();
            for (auto& b: ctx.lir_blocks(f)) {
                if (b.id.id != f.entry.id) emit_block_label(f.id, b.id);

                for (auto& i: ctx.lir_insts(b)) {
                    emit_instruction(i, f.id);
                }
                const LIRInstruct& t = ctx.get_terminator(b.term);
                emit_instruction(t, f.id);
            }

            emit_epilogue(f.id);
        }
    }

    void emit_instruction(const LIRInstruct& i, const FuncId& fid) {

        std::visit(overloaded{
            [&](const LIRLoad& load) {
                SlotId slot = load.src;
                const SlotFrameInfo& sfi = curr_frame_info->slot_map[slot.id];
                std::size_t offset = sfi.fp_offset;

                emit_line("\tldr ", reg(load.dst), ", ", mem(FP, offset));
            },
            [&](const LIRStore& store) {
                SlotId slot = store.dst;
                const SlotFrameInfo& sfi = curr_frame_info->slot_map[slot.id];
                std::size_t offset = sfi.fp_offset;

                emit_line("\tstr ", reg(store.src), ", ", mem(FP, offset));
            },
            [&](const LIRConst& cons) {
                emit_line("\tmov ", reg(cons.dst), ", #", cons.src);
            },
            [&](const LIRRet& ret) {
                emit_line("\tmov ", RET, ", ", reg(ret.id));
                emit_line("b ", func_ret_label(fid));
            },
            [&](const LIRBinOp& binop) {
                auto emit_op = [&](const auto& op) {
                    if (alt<Reg>(op)) {
                        emit(reg(get<Reg>(op)));
                    } else if (alt<int64_t>(op)) {
                        emit(get<int64_t>(op));  // relies on ostream << int64_t
                    } else {
                        // unreachable / diagnostic
                    }
                };

                emit("\t", binop_name(binop.bin_op_kind), " ", reg(binop.dst), ", ");
                emit_op(binop.lhs);
                emit(", ");
                emit_op(binop.rhs);
                emit("\n");
            }
        }, i.pl);
    }

    // FuncId's are unique per program.
    void emit_function_label(const FuncId& fid) {
        emit_line("function_", fid.id, ":");
    }

    // Needed for archARM to run properly.
    void emit_main_label() {
        emit_line("_main:");
    }

    // Each functionid/blockid pair is unique.
    void emit_block_label(const FuncId& fid, const BlockId& bid) {
        emit_line("block_", fid.id, bid.id, ":");
    }

    // Prologue/epilogue emission
    void emit_prologue() {
        emit_line("\tstp x29, x30, [sp, #-16]!");
        emit_line("\tmov x29, sp");
        std::size_t frame_size = curr_frame_info->total_frame_size;
        emit_line("\tsub sp, sp, #", frame_size);
        
    }

    // Assumes 'ret' is stored in x0 already.
    void emit_epilogue(const FuncId& fid) {
        emit_line(func_ret_label(fid), ":");
        emit_line("\tadd sp, sp, #", curr_frame_info->total_frame_size);
        emit_line("\tldp x29, x30, [sp], #16");
        emit_line("\tret");
    }

    // tODO: cheating, just hardcoding. remove hack later.
    void emit_globals() {
        emit_line("\t.text");
        emit_line("\t.p2align 2");
        emit_line("\t.global _main");
    }

    
    

private:
    BackendContext& ctx;
    bool debug;
    std::ostream& dout; // debug out
    std::ostream& armout; // actual ARM file out
    FrameInfo* curr_frame_info;

    static constexpr ArchReg ARM_GPR_POOL[ARM_FREE_REGS] = { // pool of free regs
        ArchReg::X9,
        ArchReg::X10,
        ArchReg::X11,
        ArchReg::X12,
        ArchReg::X13,
        ArchReg::X14,
        ArchReg::X15,
    };

    const char* arch_reg_name(ArchReg r) {
        static constexpr const char* names[] = {
            "x0", "x1", "x2", "x3", "x4", "x5",
            "x6", "x7", "x8", "x8", "x10", 
            "x11", "x12", "x13", "x14", "x15", 
            "x16", "x17", "x18", "x19", "x20", 
            "x21", "x22", "x23", "x24", "x25", 
            "x26", "x27", "x28", "x29", "x30", "sp"
        };
        return names[static_cast<int>(r)];
    }

    const char* binop_name(BinOpKind kind) {
        switch (kind) {
            case BIN_ADD:
            {
                return "add";
            }
            case BIN_SUB:
            {
                return "sub";
            }
            case BIN_MUL:
            {
                return "mul";
            }
            case BIN_UDIV:
            {
                return "udiv";
            }
            case BIN_SDIV:
            {
                return "sdiv";
            }
            case BIN_ERROR:
            {
                return "ERR_NO_OP";
            }
        }
    }
    const char* FP = arch_reg_name(ArchReg::X29);
    const char* LR = arch_reg_name(ArchReg::X30);
    const char* SP = arch_reg_name(ArchReg::SP);
    const char* RET = arch_reg_name(ArchReg::X0);

    template <class... Ts>
    void emit(const Ts&... xs) {
        (armout << ... << xs);
    }

    template <class... Ts>
    void emit_line(const Ts&... xs) {
        (armout << ... << xs) << '\n';
    }

    // Returns ARM reg based on preg.
    const char* reg(Reg reg) {
        if (alt<PRegId>(reg.id)) {
            PRegId preg = get<PRegId>(reg.id);
            return arch_reg_name(ARM_GPR_POOL[preg.id]);
        }
        return "no-reg-found";
    }

};

void emit_arch_arm_64(BackendContext& ctx, bool debug, std::ostream& armout) {
    ARMEmitter emitter(ctx, debug, std::cout, armout);
    emitter.emit_arch_arm();
}
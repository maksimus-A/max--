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

            if (ctx.func_is_main(f.fn_sym_id)) {
                emit_main_label();
            }
            else {
                emit_function_label(f);
            }

            emit_prologue(f);
            for (auto& b: ctx.lir_blocks(f)) {
                if (b.id.id != f.entry.id) emit_block_label(f.id, b.id);

                for (auto& i: ctx.lir_insts(b)) {
                    emit_instruction(i, f);
                }
                const LIRInstruct& t = ctx.get_terminator(b.term);
                emit_instruction(t, f);
            }

            emit_epilogue(f);
        }

        if (debug) dout << "ARM Emitted successfully!\n";
    }

    void emit_instruction(const LIRInstruct& i, const LIRFunction& f) {

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
                emit_const(cons);
            },
            [&](const LIRRet& ret) {
                if (ret.has_value) {
                    emit_line("\tmov ", RET, ", ", reg(ret.id)); // RET = x0 or w0
                }
                emit_line("\tb ", func_ret_label(f.id));
            },
            [&](const LIRBinOp& binop) {

                emit("\t", binop_name(binop.bin_op_kind), " ", reg(binop.dst), ", ");
                emit_op(binop.lhs);
                emit(", ");
                emit_op(binop.rhs);
                emit("\n");
            },
            [&](const LIRSetCC& setcc) {
                /*TODO: Sooooo this is fundamentally flawed.
                It always materializes bools into regs instead of using the ARM
                flags. This isn't inherently wrong but it's very inefficient.
                One day I might peephole the LIR to use that op instead.
                Or just directly do it during emission IDK. */

                // First emit CMP op
                emit("\tcmp ");
                emit_op(setcc.lhs);
                emit(", ");
                emit_op(setcc.rhs);
                emit("\n");

                // Now emit 'CSET op'
                // TODO**: This has a flaw. It assumes all comparisons are signed.
                // they are, but they might not be later. Later, assign a 'signed'
                // bool to the instruction for proper emission.
                emit_line("\tcset ", reg(setcc.dst), ", " , cmp_name(setcc.kind));
            },
            [&](const LIRBranch& br) {
                // Emit 'CBNZ' (Conditional branch if not zero?)
                emit_line("\tcbnz ", reg(br.cmp), ", ", block_label(f.id, br.non_zero));
                // Emit unconditional branch to 'zero' target
                emit_line("\tb ", block_label(f.id, br.zero));
            },
            [&](const LIRJump& j) {
                emit_line("\tb ", block_label(f.id, j.jump_to));
            },
            [&](const LIRArgGet& arg_get) {
                // Doesn't support argument spills yet.
                assert(arg_get.src < 8);
                emit_line("\tmov ", reg(arg_get.dst), ", ", "x", arg_get.src);
            },
            [&](const LIRArgPut& arg_put) {
                assert(arg_put.dst < 8);
                emit_line("\tmov x", arg_put.dst, ", ", reg(arg_put.src));
            },
            [&](const LIRCall& call) {
                emit_line("\tbl ", function_label(call.fn_sym_id));
                emit_line("\tmov ", reg(call.dst), ", ", RET);
            },
            // [&](const auto&) {/* TODO: Implement rest of ops!*/}
        }, i.pl);
    }

    void emit_const(const LIRConst& cons) {
        // Gonna assume all immediates are 64 bits to materialize.
        std::int64_t val = cons.src;
        emit_line("\tmovz ", reg(cons.dst), ", #", val & 0xFFFF, ", lsl #0");
        for (int i = 1; i <= 3; i++) {
            uint64_t u = (uint64_t)val;
            std::int64_t chunk = (u >> (16*i)) & 0XFFFF;
            if (chunk != 0)
                emit_line("\tmovk ", reg(cons.dst), ", #", chunk, ", lsl #", 16*i);
        }
    }

    void emit_op(const Operand& op) {
        if (alt<Reg>(op)) {
            emit(reg(get<Reg>(op)));
        } else if (alt<int64_t>(op)) {
            emit("#", get<int64_t>(op));  // relies on ostream << int64_t
        } else {
            // unreachable / diagnostic
        }
    }

    // FuncId's are unique per program.
    void emit_function_label(const FuncId& fid) {
        // todo: later when real funcs exist emit the real name.
        emit_line("function_", fid.id, ":");
    }

    // Used for returns? IDK.
    std::string function_label(const FuncId& fid) {
        return "function_" +std::to_string(fid.id);
    }

    std::string function_label(int fn_sym_id) {
        if (ctx.func_is_main(fn_sym_id)) {
            return "_main";
        }

        const Symbol* fn_sym = ctx.get_symbol(fn_sym_id);
        const char* start = ctx.start_of_name(fn_sym_id);
        std::string_view fn_name(start, fn_sym->symbol_span.length);

        std::string label;
        label.reserve(1 + fn_name.size());
        label.push_back('_');
        label.append(fn_name);
        return label;
    }

    // Needed for archARM to run properly.
    void emit_main_label() {
        emit_line("_main:");
    }

    void emit_function_label(const LIRFunction& f) {
        const char* start = ctx.start_of_name(f.fn_sym_id);
        const Symbol* fn_sym = ctx.get_symbol(f.fn_sym_id);
        std::string_view fn_name(start, fn_sym->symbol_span.length);
        emit_line("_", fn_name, ":");
    }

    // Each functionid/blockid pair is unique.
    void emit_block_label(const FuncId& fid, const BlockId& bid) {
        // Local Basic Bloc {FuncId}_{BlockId}
        emit_line(".LBB", fid.id, "_", bid.id, ":");
    }

    std::string block_label(const FuncId& fid, const BlockId& bid) {
        return ".LBB" +std::to_string(fid.id) + "_" + std::to_string(bid.id);
    }

    // Prologue/epilogue emission
    void emit_prologue(const LIRFunction& f) {
        emit_line("\tstp x29, x30, [sp, #-16]!");
        emit_line("\tmov x29, sp");

        // emit a sp move if remaining size > 0
        std::size_t remaining_size = curr_frame_info->total_frame_size - 16;
        if (remaining_size > 0) {
            emit_line("\tsub sp, sp, #", remaining_size);
        }
        // create stack offsets of callee regs
        const std::vector<int>& used_regs = ctx.fn_used_callee_regs[f.id.id];

        // save Callee-Saved Registers to the VIP Area
        for (int reg_id : used_regs) {
            // Retrieve the pre-calculated offset 
            int32_t offset = curr_frame_info->callee_reg_offsets[reg_id];
            
            // Emit: str x19, [x29, #-24]
            emit_line("\tstr x", reg_id, ", [x29, #", offset, "]");
        }
    }

    // Assumes 'ret' is stored in x0 already.
    void emit_epilogue(const LIRFunction& f) {
        emit_line(func_ret_label(f.id), ":");

        const std::vector<int>& used_regs = ctx.fn_used_callee_regs[f.id.id];

        for (int reg_id: used_regs) {
            // Retrieve the pre-calculated offset 
            int32_t offset = curr_frame_info->callee_reg_offsets[reg_id];
            
            // Emit: str x19, [x29, #-24]
            emit_line("\tldr x", reg_id, ", [x29, #", offset, "]");
        }
        // dealloc stack pointer by moving to FP (safe way)
        emit_line("\tmov sp, x29"); 

        // load LR/FP and return
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

    const char* cmp_name(const CmpKind& kind) {
        switch (kind) {
            case CMP_EQ: return "EQ";
            case CMP_NEQ: return "NE";
            case CMP_GT: return "GT";
            case CMP_LT: return "LT";
            case CMP_GE: return "GE";
            case CMP_LE: return "LE";
            case CMP_ERR: return "CMP_ERR"; 
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
            return arch_reg_name(static_cast<ArchReg>(preg.id));
        }
        return "no-reg-found";
    }

};

void emit_arch_arm_64(BackendContext& ctx, bool debug, std::ostream& armout) {
    ARMEmitter emitter(ctx, debug, std::cout, armout);
    emitter.emit_arch_arm();
}
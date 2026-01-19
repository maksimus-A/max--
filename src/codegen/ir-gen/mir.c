#include "codegen/ir-gen/mir.h"
#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include "ast/parser/ast.h"
#include "codegen/ir-gen/ir_types.h"
#include "common.h"
#include "semantics/walker.h"
#include "table/ptrtable.h"
#include <assert.h>
#include <stdalign.h>

TempId create_temp_id(IRBuilder* builder);
bool emit_load(IRBuilder* builder, ASTNode* node, TempId dst, SlotId src);
static Symbol* get_symbol(IRBuilder* builder, ASTNode* node);
static IRValue lower_expr(IRBuilder* builder, ASTNode* expr);
static void lower_stmt(IRBuilder* builder, ASTNode* stmt);
void print_value_stack(FILE* output, IRBuilder* builder, ASTNode* node);

// Slot symbol or Symbol slot helpers.

SlotId get_next_slot(IRFunction* f) {
    return (SlotId) {.id=f->next_slot_id++};
}

// Returns NULLABLE symbol from symbol table (check if exists).
// Useful for debugging/printing; not during generation really.
Symbol* symbol_from_slot_sym(IRFunction* f, SlotId slot) {
    Symbol* sym = get_ptr_tbl(&f->slot_sym, slot.id);
    return sym;
}

// Create SlotId, map it to slot_sym table, and return the slot.
SlotId create_and_map_slot_to_sym(IRFunction* f, Symbol* sym) {
    // Add 1 to function slot count for later, and
    // add slot_id to function slots table.
    SlotId lhs_slot = get_next_slot(f);

    // Gets symbol from node, sets slot_i to symbol for function (LHS)
    set_ptr_tbl(&f->slot_sym, sym, lhs_slot.id);

    return lhs_slot;
}

// Maps the given SymbolID -> SlotID.
void map_sym_to_slot(IRBuilder* builder, IRFunction* f, size_t sym_id, SlotId slot_val) {
    // Allocate persistent memory for the ID
    SlotId* stored_slot = arena_alloc(builder->arena, sizeof(SlotId), alignof(SlotId));
    *stored_slot = slot_val;
    
    // Store the valid pointer
    set_ptr_tbl(&f->sym_slot, stored_slot, sym_id);
}

// Checks if SlotId exists in table. If it does, return that one.
// If not, create its entry and return that new entry.
SlotId slot_from_sym_slot(IRBuilder* builder, IRFunction* f, size_t sym_id) {
    SlotId* slot_p = get_ptr_tbl(&f->sym_slot, sym_id);
    if (slot_p == NULL) {
        // Create new entry.
        SlotId slot = get_next_slot(f);
        // Fix: Pass builder and pass 'slot' by value
        map_sym_to_slot(builder, f, sym_id, slot); 
        return slot;
    }
    return *slot_p;
}

// Grabs n-th function in function list.
IRFunction* get_nth_func(Vector* funcs, int i) {
    if (i == SIZE_MAX) return NULL;
    // todo: i want this api in other files but
    // they dont have the builder included.
    // if (i >= builder->funcs.count) return NULL;
    return VEC_AT_PTR_T(funcs, IRFunction, i);
}

// Grabs pointer to current IRFunction being used inside builder.
IRFunction* get_curr_func(IRBuilder* builder) {
    if (builder->curr_func_index == SIZE_MAX) return NULL;
    return get_nth_func(&builder->funcs, builder->curr_func_index);
}

// Grabs n-th block by index in specific function.
IRBlock* get_nth_block(IRFunction* func, int i) {
    if (i >= func->blocks.count) return NULL;
    return VEC_AT_PTR_T(&func->blocks, IRBlock, i);
}

IRBlock* get_curr_block(IRBuilder* builder, IRFunction* func) {
    if (builder->curr_block_index == SIZE_MAX) return NULL;
    return get_nth_block(func, builder->curr_block_index);
}

IRInstruct* get_nth_instruction(IRBlock* block, int i) {
    if (i >= block->instructions.count) return NULL;
    return VEC_AT_PTR_T(&block->instructions, IRInstruct, i);
}

static void add_successor(IRBuilder* builder, IRBlock* b, BlockId succ) {
    VEC_PUSH_T(&b->succs, succ);
}


// Insert instruction into proper placement in builder
// (should be holding a pointer to current func/block).
static bool insert_instruction(IRBuilder* builder, IRInstruct inst) {
    // bounds checks
    if (builder->curr_block_index == SIZE_MAX) return false;
    if (builder->curr_func_index == SIZE_MAX) return false;
    if (builder->curr_func_index >= builder->funcs.count) return false;
    // use macro to get pointer to current IRFunc element
    IRFunction* f = get_curr_func(builder);
    if (builder->curr_block_index >= f->blocks.count) return false;

    // use macro to get pointer to current IRBlock element
    IRBlock* b = get_curr_block(builder, f);
    // Add instruction to current block.
    VEC_PUSH_T(&b->instructions, inst);

    return true;
}

static bool insert_terminator(IRBuilder* builder, IRInstruct inst) {
    // bounds checks
    if (builder->curr_block_index == SIZE_MAX) return false;
    if (builder->curr_func_index == SIZE_MAX) return false;
    if (builder->curr_func_index >= builder->funcs.count) return false;
    // use macro to get pointer to current IRFunc element
    IRFunction* f = get_curr_func(builder);
    if (builder->curr_block_index >= f->blocks.count) return false;

    // use macro to get pointer to current IRBlock element
    IRBlock* b = get_curr_block(builder, f);
    // Add terminator to current block.
    b->term = inst;
    b->has_term = true;

    return true;
}



/*------ AST Node -> IR Value/Slot conversion ------*/

// Currently supports turning immediates and variables
// into IR values, and emits a load into temp.
IRValue emit_rvalue(IRBuilder* builder, ASTNode* node) {
    // TODO*: Be explicit this emits a load (for your future self).
    IRValue val = {0};

    switch (node->ast_kind) {
        case AST_INT_LIT: // leaf
        {
            val.value_kind = IRVAL_IMM;
            val.value_id.imm = node->node_info.int_lit.value;
            break;
        }
        case AST_NAME: // leaf
        {
            val.value_kind = IRVAL_TEMP;
            IRFunction* f = get_curr_func(builder);

            // Get symbol of name
            Symbol* name_sym = node->node_info.var_name.resolved_sym;
            assert(name_sym != NULL);

            // Construct new temp
            TempId temp = create_temp_id(builder);
            val.value_id.temp_id = temp;
            // Emit a load here with the temp we create.
            SlotId var_slot = slot_from_sym_slot(builder, f, name_sym->id);
            assert(var_slot.id != SIZE_MAX);
            emit_load(builder, node, temp, var_slot);
            break;
        }
        default:
        // todo: put some error code if this fails.
            break;
    }

    return val;
}

// Returns symbol for current LHS variable.
static Symbol* get_symbol(IRBuilder* builder, ASTNode* node) {
    switch (node->ast_kind) {
        case (AST_NAME):
        {
            assert(node->node_info.var_name.resolved_sym && "unresolved identifier in MIR gen");

            return node->node_info.var_name.resolved_sym;
            break;
        }
        case AST_VAR_DEC:
        {
            return node->node_info.var_decl.symbol;
            break;
        }
        case AST_ASSN:
        {
            return node->node_info.assn_stmt.resolved_sym;
            break;
        }
        case AST_FN_DEC:
        {
            return node->node_info.fn_dec.sym;
            break;
        }
        default:
        {
            return NULL; // error
            break;
        }
    }
    return NULL; // error
}


// Grabs our IR value from the stack.
IRValue get_ir_value_stack(IRBuilder* builder) {
    // Pop the value we need from the value stack.
    IRValue* expr_val_ptr = (IRValue*)vec_pop(&builder->val_stack);
    if (!expr_val_ptr) {
        IRValue err_val = (IRValue) {.value_kind = IRVAL_ERR};
        return err_val;
    }
    IRValue expr_val = *expr_val_ptr;
    return expr_val;
}

// Gets BinOp kind based on token type.
static BinOpKind get_binop_kind(ASTNode* node) {

    Token op = node->node_info.bin_op.op;
    switch (op.token_kind) {
        case PLUS: return BIN_ADD;
        case MINUS: return BIN_SUB;
        case MULT: return BIN_MUL;
        case DIV: {
            // TODO: This is complicated. I need to propogate the type
            // of the whole operation to LHS/RHS or something; need a way
            // to know what the type is. For now it'll always be SDIV.
            return BIN_SDIV;
        }
        default: return BIN_ERROR;
    }
}

static CmpKind get_cmp_kind(ASTNode* node) {

    Token op = node->node_info.bin_op.op;
    switch (op.token_kind) {
        case LESS_THAN: return CMP_LT;
        case GREATER_THAN: return CMP_GT;
        case EQQ: return CMP_EQ;
        case NEQ: return CMP_NEQ;
        default: return CMP_ERR;
    }
}

// Returns the next available temp id, and increments
// our temp id counter.
// TODO: Should store temp type as well; I need AST_NAME to store the type too.
TempId create_temp_id(IRBuilder* builder) {
    IRFunction* f = get_curr_func(builder);
    TempId temp = (TempId) {.id = f->next_temp_id.id};
    f->next_temp_id.id++;
    return temp;
}

/*------ Instruction Emissions ------*/
bool emit_store(IRBuilder* builder, ASTNode* node, SlotId dst, IRValue src) {
    Store store = (Store) {
        .dst = dst,
        .src = src
    };
    IRInstruct store_inst = (IRInstruct) {
        .type = IR_STORE,
        .ast_id = node->id,
        .payload.store_payload = store,
        .span = node->span,
    };
    if (!insert_instruction(builder, store_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'store' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

bool emit_load(IRBuilder* builder, ASTNode* node, TempId dst, SlotId src) {
    Load load = (Load) {
        .dst = dst,
        .src = src
    };
    IRInstruct load_inst = (IRInstruct) {
        .type = IR_LOAD,
        .ast_id = node->id,
        .payload.load_payload = load,
        .span = node->span,
    };
    if (!insert_instruction(builder, load_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'load' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}



static bool emit_binop(IRBuilder* builder, ASTNode* node, TempId dst, IRValue lhs, IRValue rhs) {
    // grab LHS/RHS from node, store them into op with opkind
    BinOpKind binop_kind = get_binop_kind(node);

    BinOp binop = (BinOp) {
        .dst = dst,
        .kind = binop_kind,
        .lhs = lhs,
        .rhs = rhs,
    };

    IRInstruct binop_inst = (IRInstruct) {
        .type = IR_BINOP,
        .ast_id = node->id,
        .span = node->span,
        .payload.binop_pl = binop
    };

    if (!insert_instruction(builder, binop_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'bin_op' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

// Comparison operation.
static bool emit_cmpop(IRBuilder* builder, ASTNode* node, TempId dst, IRValue lhs, IRValue rhs) {
    // grab LHS/RHS from node, store them into op with opkind
    CmpKind cmp_kind = get_cmp_kind(node);

    Cmp cmp = (Cmp) {
        .dst = dst,
        .kind = cmp_kind,
        .lhs = lhs,
        .rhs = rhs,
    };

    IRInstruct cmp_inst = (IRInstruct) {
        .type = IR_CMPOP,
        .ast_id = node->id,
        .span = node->span,
        .payload.cmp_pl = cmp
    };

    if (!insert_instruction(builder, cmp_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'cmp' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

bool emit_halt_with_val(IRBuilder* builder, ASTNode* node, IRValue ir_val) {
    Halt halt = (Halt) {
        .has_value = true,
        .code = ir_val
    };

    IRInstruct halt_inst = (IRInstruct) {
        .type = IR_HALT,
        .ast_id = node->id,
        .payload.halt_payload = halt,
        .span = node->span,
    };
    if (!insert_terminator(builder, halt_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'halt (with_ret)' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

bool emit_halt_no_val(IRBuilder* builder, ASTNode* node) {
    Halt halt = (Halt) {
        .has_value = false,
    };

    IRInstruct halt_inst = (IRInstruct) {
        .type = IR_HALT,
        .ast_id = node->id,
        .payload.halt_payload = halt,
        .span = node->span,
    };
    if (!insert_terminator(builder, halt_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'halt (no_ret)' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

bool emit_branch_if_zero(IRBuilder* builder, ASTNode* node, IRValue cmp, BlockId then_id, BlockId else_join_id) {

    Branch br = (Branch) {
        .cmp = cmp,
        .zero = else_join_id,
        .non_zero = then_id
    };

    IRInstruct branch_inst = (IRInstruct) {
        .type = IR_BRANCH_IF_ZERO,
        .span = node->span,
        .ast_id = node->id,
        .payload.br_pl = br
    };
    if (!insert_terminator(builder, branch_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'br' instruction into builder.", 0, 0);
        return false;
    }
    return true;

}

bool emit_jump(IRBuilder* builder, ASTNode* node, BlockId jump_to) {
    Jump jump = (Jump) {
        .jump_to = jump_to
    };

    IRInstruct jump_inst = (IRInstruct) {
        .type = IR_JUMP,
        .span = node->span,
        .ast_id = node->id,
        .payload.jump_pl = jump
    };
    if (!insert_terminator(builder, jump_inst)) {
        add_diag(builder->diags, ERROR, node->span, 
        "Failed to insert 'jump' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}

TempId emit_arg(IRBuilder* builder, const Symbol* sym) {
    TempId temp = create_temp_id(builder);
    Arg arg = (Arg) {
        .arg_id = get_curr_func(builder)->next_arg_id++,
        .dst = temp
    };

    IRInstruct arg_inst = (IRInstruct) {
        .type = IR_ARG,
        .span = sym->symbol_span,
        .ast_id = SIZE_MAX, // Default because args don't come from the AST.
        .payload.arg_pl = arg
    };

    if (!insert_instruction(builder, arg_inst)) {
        SrcSpan fake_span = (SrcSpan) {.start=0, .length=0};
        add_diag(builder->diags, ERROR, fake_span, 
        "Failed to insert 'arg' instruction into builder.", 0, 0);
        return (TempId){.id=SIZE_MAX};
    }
    return temp;

}

bool emit_call(IRBuilder* builder, Call* call, ASTNode* node) {
    IRInstruct call_inst = (IRInstruct) {
        .type = IR_CALL,
        .span = node->span,
        .ast_id = node->id,
        .payload.call_pl = *call
    };


    if (!insert_instruction(builder, call_inst)) {
        add_diag(builder->diags, ERROR, node->span,
        "Failed to insert 'call' instruction into builder.", 0, 0);
        return false;
    }
    return true;
}
/*------ Instruction Emissions ------*/


/*------ Initializations ------*/

// Initializes a new block, sets its blockid==vec_index, and pushes
// the block into our current function.
BlockId block_init(IRBuilder* b) {
    IRFunction* f = get_curr_func(b);

    assert(f && "no current function in block_init");

    IRBlock blk = {0};
    blk.id.id = f->blocks.count;     // <-- next index
    blk.term.type = IR_UNDEFINED;
    blk.has_term = false;
    VEC_INIT_T(&blk.instructions, b->arena, IRInstruct);
    VEC_INIT_T(&blk.preds, b->arena, BlockId);
    VEC_INIT_T(&blk.succs, b->arena, BlockId);

    VEC_PUSH_T(&f->blocks, blk);
    return blk.id;
}

BlockId block_init_in_func(IRBuilder* b, IRFunction* f) {

    assert(f && "no current function in block_init");

    IRBlock blk = {0};
    blk.id.id = f->blocks.count;     // <-- next index
    blk.term.type = IR_UNDEFINED;
    VEC_INIT_T(&blk.instructions, b->arena, IRInstruct);
    VEC_INIT_T(&blk.preds, b->arena, BlockId);
    VEC_INIT_T(&blk.succs, b->arena, BlockId);

    VEC_PUSH_T(&f->blocks, blk);
    return blk.id;
}

// Initializes an IRFunction, including an entry block,
// and inserts it into builder.
void begin_function(IRBuilder* builder, FnDeclInfo fndec) {

    IRFunction func = {0};
    // Create blocks vector
    VEC_INIT_T(&func.blocks, builder->arena, IRBlock);
    // Declare entry block
    func.entry = (BlockId) {.id = 0};
    // Create block
    BlockId block_id = block_init_in_func(builder, &func);

    // Set function id & increment
    size_t func_id = builder->funcs.count;
    func.id.id = func_id;
    func.next_temp_id = (TempId) {.id=0};

    // Set next slot id to 0 (separate from symbol ID).
    func.next_slot_id = 0;
    ptr_table_init(&func.slot_sym, builder->arena);
    ptr_table_init(&func.sym_slot, builder->arena);

    // Set fn signature stuff.

    // Set function symbol ID
    func.fn_sym_id = fndec.sym->id;

    //Set function return type
    func.ret_type = fndec.ret_type;
    // Param count
    func.param_count = fndec.params.count;
    func.param_types = fndec.sym->fn_info.sig.param_types; // stable arena pointer.

    // push function into builder
    VEC_PUSH_T(&builder->funcs, func);

    // Set builder insertion points.
    builder->curr_func_index = func_id;
    builder->curr_block_index = 0;
}
/*------ Initializations ------*/

/* Visitor Hooks*/
WalkChildren mir_gen_pre(void* user, ASTNode* node) {
    IRBuilder* builder = (IRBuilder*) user;

    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // todo: could create implicit function? IDK. Probably not a good idea.
            break;
        }
        case AST_FN_DEC: {
            // Create function (this sets new curr_func, inserts into builder, creates entry block, etc)
            begin_function(builder, node->node_info.fn_dec);
            IRFunction* f = get_curr_func(builder);
            // Emit 'arg' ops for all args per function.
            const Symbol* fn_sym = node->node_info.fn_dec.sym;
            assert(fn_sym != NULL);
            for (size_t i = 0; i < fn_sym->fn_info.param_count; i++) {
                Symbol* param_sym = get_ptr_tbl(&builder->sema->name_resolution, fn_sym->fn_info.param_sym_ids[i]);
                TempId temp = emit_arg(builder, param_sym);

                // Emit store arg into slot?
                SlotId dst = slot_from_sym_slot(builder, f, param_sym->id);
                set_ptr_tbl(&f->slot_sym, param_sym, dst.id);

                IRValue val = (IRValue) {
                    .value_kind = IRVAL_TEMP,
                    .value_type = param_sym->type,
                    .value_id.temp_id = temp
                };
                emit_store(builder, node, dst, val);
            }

            // Walk body manually (so i can check void ret type easily?)
            // ASTNode* fn_body = node->node_info.fn_dec.fn_block;
            // lower_stmt(builder, fn_body);
            return WALK_CHILDREN;
            break;
        }
        case AST_FN_CALL:
        {
            return SKIP_CHILDREN;
        }
        case AST_IF:
        {
            // First compute condition into a temp (and emit cmp op)
            IRValue cond_val = lower_expr(builder, node->node_info.if_stmt.cond);

            IRFunction* f = get_curr_func(builder);

            // Create then, else, join blocks
            BlockId then_block_id = block_init(builder);
            
            BlockId else_block_id;
            // Check if else-node exists
            ASTNode* else_node = node->node_info.if_stmt.else_block;
            if (else_node) {
                else_block_id = block_init(builder);
            }

            BlockId join_block_id = block_init(builder);
            
            // Fetch BlockId's for branch op
            BlockId nonzero_block_id = else_node == NULL ? join_block_id : else_block_id;

            // Emit branch op in current block.
            emit_branch_if_zero(builder, node, cond_val, then_block_id, nonzero_block_id);

            // Track if execution flow merges into the join block
            bool flow_reaches_join = false;

            // --- Then block ---
            builder->curr_block_index = then_block_id.id;
            lower_stmt(builder, node->node_info.if_stmt.then_block);
            // If no terminator, insert a jump to 'join'.
            if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                emit_jump(builder, node, join_block_id);
                flow_reaches_join = true;
            }

            // --- Else block ---
            if (else_node != NULL) {
                builder->curr_block_index = else_block_id.id;
                lower_stmt(builder, node->node_info.if_stmt.else_block);
                if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                    emit_jump(builder, node, join_block_id);
                    flow_reaches_join = true;
                }
            } else {
                // No else block implies the 'false' condition of the branch 
                // falls through directly to the join block.
                flow_reaches_join = true;
            }

            // Set current block to join for subsequent statements
            builder->curr_block_index = join_block_id.id;

            // FIX: If the join block is unreachable (because both branches returned),
            // we must terminate it to satisfy IR verification, even though it's dead code.
            if (!flow_reaches_join) {
                if (f->ret_type == TYPE_VOID) {
                    emit_halt_no_val(builder, node);
                } else {
                    // Emit a dummy 0 for non-void functions
                    IRValue dummy = {.value_kind = IRVAL_IMM, .value_id.imm = 0};
                    emit_halt_with_val(builder, node, dummy);
                }
            }

            return SKIP_CHILDREN;
            break;
        }
        case AST_WHILE:
        {
            IRFunction* f = get_curr_func(builder);

            // Create a 'header/cond' block
            BlockId header_block_id = block_init(builder);
            // Create loop_body block
            BlockId loop_block_id = block_init(builder);
            // Create join block
            BlockId join_block_id = block_init(builder);

            // Emit jump to header
            if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                emit_jump(builder, node, header_block_id);
            }

            // Set current block to header
            builder->curr_block_index = header_block_id.id;
            // Insert comparison into header
            IRValue cond_val = lower_expr(builder, node->node_info.while_stmt.cond);
            // True -> loop_block, false -> join_block
            if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                emit_branch_if_zero(builder, node, cond_val, loop_block_id, join_block_id);
            }


            // Set current block to loop_body
            builder->curr_block_index = loop_block_id.id;
            // Lower loop body to IR
            lower_stmt(builder, node->node_info.while_stmt.loop_block);
            // Set terminator (Branch to header)
            if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                emit_jump(builder, node, header_block_id);
            }

            // Set current block to join
            builder->curr_block_index = join_block_id.id;
            // Done! emit normally now.

            return SKIP_CHILDREN;
            break;
        }
        default: break;
    }
    return WALK_CHILDREN;
}

void mir_gen_post(void* user, ASTNode* node) {
    IRBuilder* builder = (IRBuilder*) user;


    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // Exit program.
            break;
        }
        case AST_FN_DEC:
        {
            // check if fn return type is 'void'.
            // If it is, and no return statement exists,
            // implciitly add one.
            IRFunction* f = get_curr_func(builder);
            IRBlock* b = get_curr_block(builder, f);
            // Check if the last block (where execution flow ended) has a terminator.
            if (!b->has_term) {
                if (f->ret_type == TYPE_VOID) {
                    // Implicit return for void functions
                    emit_halt_no_val(builder, node);
                } else {
                    // Control reaches end of non-void function.
                    // To satisfy the IR verifier, we MUST emit a terminator.
                    // In C, this is Undefined Behavior. Here, we can just return 0 
                    // or emit a specific "Trap/Unreachable" opcode if you have one.
                    
                    IRValue zero_val = {.value_kind = IRVAL_IMM, .value_id.imm = 0};
                    emit_halt_with_val(builder, node, zero_val);
                    
                    // Optional: You could emit a compiler warning here.
                    // fprintf(stderr, "Warning: Control reaches end of non-void function '%s'\n", ...);
                }
            }
            break;
        }
        case AST_FN_CALL:
        {
            
            IRFunction* f = get_curr_func(builder);

            TempId dst = create_temp_id(builder);
            size_t fn_sym_id = node->node_info.fn_call.callee_sym->id;
            Call call = (Call) {
                .dst = dst,
                .fn_sym_id = fn_sym_id
            };
            // init IRValue vec
            VEC_INIT_T(&call.args, builder->arena, IRValue);


            for (int i = 0; i < node->node_info.fn_call.args.count; i++) {
                // Lower function parameter
                ASTNode* expr = VEC_AT_T(&node->node_info.fn_call.args, ASTNode*, i);
                IRValue val = lower_expr(builder, expr);

                // Insert into IRValue vec?
                VEC_PUSH_T(&call.args, val);
            }

            emit_call(builder, &call, node);

            // Push the result of the call onto the stack
            IRValue call_result = (IRValue){
                .value_kind = IRVAL_TEMP,
                .value_type = node->node_info.fn_call.callee_sym->type,
                .value_id.temp_id = dst
            };
            VEC_PUSH_T(&builder->val_stack, call_result);
            break;
        }
        case AST_BLOCK:
        {
            // Pop scope? nah. not unless parent of block is cf.
            break;
        }
        case AST_VAR_DEC: 
        // DONE REFACTOR
        {
            IRFunction* f = get_curr_func(builder);
            Symbol* sym = get_symbol(builder, node);

            // Create slot and map REVERSE (Slot -> Sym)
            SlotId lhs_slot = create_and_map_slot_to_sym(f, sym);
            
            // Map FORWARD (Sym -> Slot) using the new safe function
            // Pass 'lhs_slot' by value, not address &lhs_slot
            map_sym_to_slot(builder, f, sym->id, lhs_slot);

            assert(lhs_slot.id != SIZE_MAX);

            // Pop the value we need from the value stack.
            IRValue expr_val = get_ir_value_stack(builder);
            if (expr_val.value_kind == IRVAL_ERR) {
                fprintf(stderr, "No value found in val_stack in var decl node.");
                break;
            }

            // Emit 1 store op
            emit_store(builder, node, lhs_slot, expr_val);
            break;
        }
        case AST_ASSN:
        {
            // DONE REFACTOR
            IRFunction* f = get_curr_func(builder);
            Symbol* sym = get_symbol(builder, node);

            // Use the safe lookup that creates if missing
            SlotId lhs_slot = slot_from_sym_slot(builder, f, sym->id);
            assert(lhs_slot.id != SIZE_MAX);

            // Pop the value we need from the value stack.
            IRValue expr_val = get_ir_value_stack(builder);
            if (expr_val.value_kind == IRVAL_ERR) {
                fprintf(stderr, "No value found in val_stack in assignment node.");
                break;
            }

            // Emit 1 store op
            emit_store(builder, node, lhs_slot, expr_val);
            break;
        }
        case AST_BIN_OP:
        case AST_CMP_OP:
        {
            // Emits bin op based on stack variables.
            IRValue* prhs = (IRValue*)vec_pop(&builder->val_stack);
            IRValue* plhs = (IRValue*)vec_pop(&builder->val_stack);

            if (!prhs || !plhs) {
                add_diag(builder->diags, ERROR, node->span, 
                "ERROR: not enough values for binop/cmp", 0, 0);
                fprintf(stderr, "ERROR: not enough values for binop/cmp\n");
                break;
            }

            IRValue rhs = *prhs;
            IRValue lhs = *plhs;

            // Create new temp?
            IRFunction* f = get_curr_func(builder);
            
            // now create new temp?
            TempId temp = create_temp_id(builder);

            if (node->ast_kind == AST_BIN_OP) emit_binop(builder, node, temp, lhs, rhs);
            if (node->ast_kind == AST_CMP_OP) emit_cmpop(builder, node, temp, lhs, rhs);

            // Push IRValue onto stack
            IRValue val = (IRValue) {
                .value_kind = IRVAL_TEMP,
                .value_id.temp_id = temp
            };
            VEC_PUSH_T(&builder->val_stack, val);
            break;
        }
        // Leaf nodes.
        case AST_INT_LIT:
        {
            // TODO: AST_INT_LIT should really store its type too.
            IRValue val = emit_rvalue(builder, node);
            // Push onto value stack
            VEC_PUSH_T(&builder->val_stack, val);
            break;
        }
        case AST_NAME: // ast_name should always be on rhs of expr.
        {
            IRValue val = emit_rvalue(builder, node);

            // Push onto value stack
            VEC_PUSH_T(&builder->val_stack, val);
            break;
        }
        case AST_EXIT:
        {
            if (node->node_info.exit_info.expr != NULL) {
                // Pop the value we need from the value stack.
                IRValue expr_val = get_ir_value_stack(builder);
                if (expr_val.value_kind == IRVAL_ERR) {
                    fprintf(stderr, "No value found in val_stack in exit node.");
                    break;
                }

                emit_halt_with_val(builder, node, expr_val);
            }
            else { // NULL == 'return;'
                emit_halt_no_val(builder, node);
            }

            break;
        }

        default: break;
    }
    // useful debug
    //print_value_stack(stdout, builder, node);
}

Visitor mir_gen_visitor = {
    .pre = mir_gen_pre,
    .post = mir_gen_post
};

/*------ Helpers ------*/

// Walks sub-tree of tree, starting from 'expr'
// Returns an IRVal after computing expression vals.
static IRValue lower_expr(IRBuilder* builder, ASTNode* expr) {
    // Computes value of expression
    walk_node(&mir_gen_visitor, builder, expr);

    // Gets expression value from value stack
    IRValue expr_val = get_ir_value_stack(builder);

    return expr_val;
}

// Walks sub-tree of tree, starting from 'stmt'.
static void lower_stmt(IRBuilder* builder, ASTNode* stmt) {
    size_t stack_size = builder->val_stack.count;
    walk_node(&mir_gen_visitor, builder, stmt);

    assert(builder->val_stack.count == stack_size);
}

//* A second pass that quickly creates a CFG based on the block structure.
void add_all_successors(IRBuilder* builder) {

    size_t block_index = 0;
    for (int i = 0; i < builder->funcs.count; i++) {
        IRFunction* f = get_nth_func(&builder->funcs, i);

        for (int j = 0; j < f->blocks.count; j++) {
            IRBlock* b = get_nth_block(f, j);

            IRInstruct inst = b->term;
            switch (inst.type) {
                case IR_JUMP:
                {
                    add_successor(builder, b, inst.payload.jump_pl.jump_to);
                    break;
                }
                case IR_HALT:
                {
                    // None.
                    break;
                }
                case IR_BRANCH_IF_ZERO:
                {
                    add_successor(builder, b, inst.payload.br_pl.non_zero);
                    add_successor(builder, b, inst.payload.br_pl.zero);
                    break;
                }
                default: break;
            }
        }
    }
}

//* Main call function. */ 
void run_mir_gen(ASTNode* ast_root, IRBuilder* builder) {
    walk_node(&mir_gen_visitor, builder, ast_root);

    // Build CFG from MIR.
    add_all_successors(builder);
}

void builder_init(IRBuilder* builder, Arena* arena, Diagnostics* diags, Semantics* sema, Source* source_file) {
    // initialize IRFunc
    VEC_INIT_T(&builder->funcs, arena, IRFunction);
    // initialize IRValue stack
    VEC_INIT_T(&builder->val_stack, arena, IRValue);
    builder->next_block_id.id = 0;

    builder->curr_func_index = SIZE_MAX;
    builder->curr_block_index = SIZE_MAX;

    builder->diags = diags;
    builder->arena = arena;
    builder->sema = sema;
    builder->source_file = source_file;
}

/*------ MIR PRINTING ------*/

void print_value_stack(FILE* output, IRBuilder* builder, ASTNode* node) {
    fprintf(output, "AST Kind: %d, node_id: %zu\n", node->ast_kind, node->id);
    fprintf(output, "VAL STACK:\n");
    for (int i = 0; i < builder->val_stack.count; i++) {
        const IRValue* val = VEC_AT_PTR_T(&builder->val_stack, IRValue, i);
        if (val->value_kind == IRVAL_IMM) {
            fprintf(output, "%d: %llu\n", i, val->value_id.imm);
        }
        else if (val->value_kind == IRVAL_TEMP) {
            fprintf(output, "%d: t%zu\n", i, val->value_id.temp_id.id);
        }
        
    }
    fprintf(output, "\n\n");
}

void print_block_label(FILE* output, FuncId fid, BlockId bid) {
    fprintf(output, "block_%zu_%zu", fid.id, bid.id);
}

void print_op(BinOpKind kind, FILE* output) {
    char* op_name;
    switch (kind) {
        case BIN_ADD: op_name = "add "; break;
        case BIN_SUB: op_name = "sub "; break;
        case BIN_MUL: op_name = "mul "; break;
        case BIN_SDIV: op_name = "sdiv "; break;
        case BIN_UDIV: op_name = "udiv "; break;
        case BIN_ERROR: op_name = "NO_OP_FOUND "; break;
    }
    fprintf(output, "%s", op_name);
}

void print_cmp(CmpKind kind, FILE* output) {
    char* cmp_name;
    switch (kind) {
        case CMP_EQ: cmp_name = "cmp_eq "; break;
        case CMP_NEQ: cmp_name = "cmp_neq "; break;
        case CMP_GT: cmp_name = "cmp_gt "; break;
        case CMP_LT: cmp_name = "cmp_lt "; break;
        case CMP_ERR: cmp_name = "NO_CMP_FOUND "; break;
    }
    fprintf(output, "%s", cmp_name);
}

// TODO: Refactor to use sym_slot instead.
void print_slot_with_id(IRBuilder* builder, IRFunction* f, SlotId slot, FILE* output) {

    Symbol* var_symbol = get_ptr_tbl(&f->slot_sym, slot.id);
    assert(var_symbol != NULL);
    SrcSpan name_span = var_symbol->symbol_span;
    char* name_ptr = start_of_name(name_span, builder->source_file);
    fprintf(output, "slot(");
    print_file_slice(name_ptr, name_span.length, output);
    fprintf(output, ":%zu)", slot.id);
}

void print_function_name(IRBuilder* builder, size_t fn_sym_id, FILE* output) {
    PtrTable name_res = builder->sema->name_resolution;
    Symbol* var_symbol = get_ptr_tbl(&name_res, fn_sym_id);
    SrcSpan name_span = var_symbol->symbol_span;
    char* name_ptr = start_of_name(name_span, builder->source_file);
    print_file_slice(name_ptr, name_span.length, output);
}

void print_instruction(IRBuilder* builder, IRInstruct* instr, FILE* output, IRFunction* f) {
    switch (instr->type) {
        case IR_LOAD:
        {
            // load dst, src
            // todo: change slot to be actual variable, not int.
            Load load = instr->payload.load_payload;
            fprintf(output, "load t%zu, ", load.dst.id);
            print_slot_with_id(builder, f, load.src, output);
            break;
        }
        case IR_STORE:
        {
            // store dst, src
            Store store = instr->payload.store_payload;
            if (store.src.value_kind == IRVAL_IMM) {
                fprintf(output, "store ");
                print_slot_with_id(builder, f, store.dst, output);
                fprintf(output, ", %lld", store.src.value_id.imm);
            }
            else if (store.src.value_kind == IRVAL_TEMP) {
                fprintf(output, "store ");
                print_slot_with_id(builder, f, store.dst, output);
                fprintf(output, ", t%zu", store.src.value_id.temp_id.id);
            }
            break;
        }
        case IR_BINOP:
        {
            BinOp binop = instr->payload.binop_pl;
            print_op(binop.kind, output);

            // Print destination
            fprintf(output, "t%zu, ", binop.dst.id);

            if (binop.lhs.value_kind == IRVAL_IMM) {
                fprintf(output, "%lld, ", binop.lhs.value_id.imm);
            }
            else if (binop.lhs.value_kind == IRVAL_TEMP) {
                fprintf(output, "t%zu, ", binop.lhs.value_id.temp_id.id);
            }

            if (binop.rhs.value_kind == IRVAL_IMM) {
                fprintf(output, "%lld", binop.rhs.value_id.imm);
            }
            else if (binop.rhs.value_kind == IRVAL_TEMP) {
                fprintf(output, "t%zu", binop.rhs.value_id.temp_id.id);
            }
            break;
        }
        case IR_CMPOP:
        {
            Cmp cmp = instr->payload.cmp_pl;
            print_cmp(cmp.kind, output);

            // Print destination
            fprintf(output, "t%zu, ", cmp.dst.id);

            if (cmp.lhs.value_kind == IRVAL_IMM) {
                fprintf(output, "%lld, ", cmp.lhs.value_id.imm);
            }
            else if (cmp.lhs.value_kind == IRVAL_TEMP) {
                fprintf(output, "t%zu, ", cmp.lhs.value_id.temp_id.id);
            }

            if (cmp.rhs.value_kind == IRVAL_IMM) {
                fprintf(output, "%lld", cmp.rhs.value_id.imm);
            }
            else if (cmp.rhs.value_kind == IRVAL_TEMP) {
                fprintf(output, "t%zu", cmp.rhs.value_id.temp_id.id);
            }
            break;
        }
        // TERMINATORS
        case IR_HALT:
        {
            // TODO: Rename halt to 'ret'. Halt is just bad naming.
            Halt halt = instr->payload.halt_payload;
            if (halt.has_value) {
                if (halt.code.value_kind == IRVAL_IMM) {
                    fprintf(output, "ret %lld", halt.code.value_id.imm);
                }
                else if (halt.code.value_kind == IRVAL_TEMP) {
                    fprintf(output, "ret t%zu", halt.code.value_id.temp_id.id);
                }
            }
            else {
                fprintf(output, "ret");
            }

            break;
        }
        case IR_BRANCH_IF_ZERO:
        {
            Branch br = instr->payload.br_pl;
            if (br.cmp.value_kind == IRVAL_IMM) {
                fprintf(output, "branch %lld ", br.cmp.value_id.imm);
            }
            else if (br.cmp.value_kind == IRVAL_TEMP) {
                fprintf(output, "branch t%zu ", br.cmp.value_id.temp_id.id);
            }
            print_block_label(output, f->id, br.non_zero); // true br
            fprintf(output, " ");
            print_block_label(output, f->id, br.zero); // false br

            break;
        }
        case IR_JUMP:
        {
            Jump jump = instr->payload.jump_pl;
            fprintf(output, "jump ");
            print_block_label(output, f->id, jump.jump_to);
            break;
        }
        // FUNCTION STUFF
        case IR_ARG:
        {
            Arg arg = instr->payload.arg_pl;
            fprintf(output, "arg t%zu, index:%zu", arg.dst.id, arg.arg_id);
            break;
        }
        case IR_CALL:
        {
            Call call = instr->payload.call_pl;
            fprintf(output, "call t%zu, ", call.dst.id);
            print_function_name(builder, call.fn_sym_id, output);
            if (call.args.count != 0)
                fprintf(output, ", ");
            for (int i = 0; i < call.args.count; i++) {
                IRValue val = VEC_AT_T(&call.args, IRValue, i);
                if (val.value_kind == IRVAL_IMM) {
                    fprintf(output, "%llu", val.value_id.imm);
                }
                else if (val.value_kind == IRVAL_TEMP) {
                    fprintf(output, "t%zu", val.value_id.temp_id.id);
                }
                if (i != call.args.count-1)
                    fprintf(output, ", ");
            }
            break;
        }
        case IR_UNDEFINED:
        {
            fprintf(output, "ERROR: Op not created/found (missing terminator? void function?).");
            break;
        }
        
    }
    fprintf(output, "\n");
}

bool dump_mir(IRBuilder* builder, FILE* output) {
    // Goes through all instructions and prints accordingly
    fprintf(output, "\n-------- MIR --------\n");
    size_t block_index = 0;
    for (int i = 0; i < builder->funcs.count; i++) {
        IRFunction* f = get_nth_func(&builder->funcs, i);
        fprintf(output, "\n");
        print_function_name(builder, f->fn_sym_id, output);
        fprintf(output, ":\n");

        for (int j = 0; j < f->blocks.count; j++) {
            fprintf(output, "  ");
            IRBlock* b = get_nth_block(f, j);
            print_block_label(output, f->id, b->id);
            fprintf(output, ":\n");

            for (int k = 0; k < b->instructions.count; k++) {
                fprintf(output, "    ");
                IRInstruct* instruction = get_nth_instruction(b, k);
                print_instruction(builder, instruction, output, f);
            }
            // Print terminator
            fprintf(output, "    ");
            print_instruction(builder, &b->term, output, f);
        }
    }
    fprintf(output, "\n");
    return true;
}
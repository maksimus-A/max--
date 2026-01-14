#include "codegen/ir-gen/mir.h"
#include "arena/arena.h"
#include "ast/lexer/lexer.h"
#include "ast/parser/ast.h"
#include "common.h"
#include "semantics/walker.h"
#include "table/ptrtable.h"
#include <assert.h>
#include <stdalign.h>

TempId create_temp_id(IRBuilder* builder);
bool emit_load(IRBuilder* builder, ASTNode* node, TempId dst, SlotId src);
SlotId get_symbol_id_id(IRBuilder* builder, ASTNode* node);
static IRValue lower_expr(IRBuilder* builder, ASTNode* expr);
static void lower_stmt(IRBuilder* builder, ASTNode* stmt);
void print_value_stack(FILE* output, IRBuilder* builder, ASTNode* node);

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
            // Construct new temp
            TempId temp = create_temp_id(builder);
            val.value_id.temp_id = temp;
            // Emit a load here with the temp we create.
            SlotId var_slot = get_symbol_id_id(builder, node);
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

// Returns slot ID for current LHS variable.
SlotId get_symbol_id_id(IRBuilder* builder, ASTNode* node) {
    SlotId slot_id = {0};
    switch (node->ast_kind) {
        case (AST_NAME):
        {
            assert(node->node_info.var_name.resolved_sym && "unresolved identifier in MIR gen");

            slot_id.id = node->node_info.var_name.resolved_sym->id;
            break;
        }
        case AST_VAR_DEC:
        {
            slot_id.id = node->node_info.var_decl.symbol->id;
            break;
        }
        case AST_ASSN:
        {
            slot_id.id = node->node_info.assn_stmt.resolved_sym->id;
            break;
        }
        default:
        {
            slot_id.id = SIZE_MAX; // error
            break;
        }
    }
    return slot_id;
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

bool emit_halt(IRBuilder* builder, ASTNode* node, IRValue ir_val) {
    Halt halt = (Halt) {
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
        "Failed to insert 'halt' instruction into builder.", 0, 0);
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
void begin_function(IRBuilder* builder) {

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
            // Create function.
            begin_function(builder);
            break;
        }
        case AST_IF:
        {
            // First compute condition into a temp (and emit cmp op)
            IRValue cond_val = lower_expr(builder, node->node_info.if_stmt.cond);

            // Now create branch op.

            IRFunction* f = get_curr_func(builder);

            // Create then, else, join blocks

            BlockId then_block_id = block_init(builder);
            size_t then_idx = then_block_id.id;

            BlockId else_block_id;
            size_t else_idx = SIZE_MAX;
            // Check if else-node exists
            ASTNode* else_node = node->node_info.if_stmt.else_block;
            if (else_node) {
                else_block_id = block_init(builder);
                else_idx = else_block_id.id;
            }

            BlockId join_block_id = block_init(builder);
            size_t join_idx = join_block_id.id;

            // Fetch BlockId's for branch op
            BlockId nonzero_block_id = else_node == NULL ? join_block_id : else_block_id;

            // Emit branch op in current block.
            emit_branch_if_zero(builder, node, cond_val, then_block_id, nonzero_block_id);
            // Current block's successors.
            VEC_PUSH_T(&get_curr_block(builder, f)->succs, then_block_id);
            VEC_PUSH_T(&get_curr_block(builder, f)->succs, nonzero_block_id);

            // Now we must process statements per-new block that exists.

            // Then block termination
            builder->curr_block_index = then_block_id.id;
            lower_stmt(builder, node->node_info.if_stmt.then_block);
            // If no terminator, insert a jump to 'join'.
            if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                emit_jump(builder, node, join_block_id);
            }
            // Then block successor.
            VEC_PUSH_T(&get_curr_block(builder, f)->succs, join_block_id);

            // Else block termination
            if (else_node != NULL) {
                builder->curr_block_index = else_block_id.id;
                lower_stmt(builder, node->node_info.if_stmt.else_block);
                if (get_curr_block(builder, f)->term.type == IR_UNDEFINED) {
                    // If no terminator, insert a jump to 'join'.
                    emit_jump(builder, node, join_block_id);
                }
                // Else block successor.
                VEC_PUSH_T(&get_curr_block(builder, f)->succs, join_block_id);
            }

            // My understanding is if I set the curr_block to join_block,
            // The over-arching walker of the entire AST from 'Program' will take back over,
            // and insert instructions into the proper block here.
            // So i think this is all.
            builder->curr_block_index = join_block_id.id;

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
        case AST_BLOCK:
        {
            // Pop scope? nah. not unless parent of block is cf.
            break;
        }
        case AST_VAR_DEC: 
        {
            // slot of RHS
            SlotId lhs_slot = get_symbol_id_id(builder, node);

            // Add 1 to function slot count for later, and
            // add slot_id to function slots table.
            IRFunction* f = get_curr_func(builder);
            
            // Gets symbol from node, sets slot_i to symbol for function (LHS)
            set_ptr_tbl(&f->slot_sym, node->node_info.var_decl.symbol, f->next_slot_id);
            f->next_slot_id++;

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
            // slot of RHS
            SlotId lhs_slot = get_symbol_id_id(builder, node);
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
            // Pop the value we need from the value stack.
            IRValue expr_val = get_ir_value_stack(builder);
            if (expr_val.value_kind == IRVAL_ERR) {
                fprintf(stderr, "No value found in val_stack in exit node.");
                break;
            }

            emit_halt(builder, node, expr_val);
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

// Main call function.
void run_mir_gen(ASTNode* ast_root, IRBuilder* builder) {
    walk_node(&mir_gen_visitor, builder, ast_root);
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

void print_slot(IRBuilder* builder, size_t symbol_id, FILE* output) {
    PtrTable name_res = builder->sema->name_resolution;
    Symbol* var_symbol = get_ptr_tbl(&name_res, symbol_id);
    SrcSpan name_span = var_symbol->symbol_span;
    char* name_ptr = start_of_name(name_span, builder->source_file);
    fprintf(output, "slot(");
    print_file_slice(name_ptr, name_span.length, output);
    fprintf(output, ")");
}

void print_slot_with_id(IRBuilder* builder, size_t symbol_id, FILE* output) {
    PtrTable name_res = builder->sema->name_resolution;
    Symbol* var_symbol = get_ptr_tbl(&name_res, symbol_id);
    SrcSpan name_span = var_symbol->symbol_span;
    char* name_ptr = start_of_name(name_span, builder->source_file);
    fprintf(output, "slot(");
    print_file_slice(name_ptr, name_span.length, output);
    fprintf(output, ":%zu)", symbol_id);
}

void print_instruction(IRBuilder* builder, IRInstruct* instr, FILE* output, FuncId fid) {
    switch (instr->type) {
        case IR_LOAD:
        {
            // load dst, src
            // todo: change slot to be actual variable, not int.
            Load load = instr->payload.load_payload;
            fprintf(output, "load t%zu, ", load.dst.id);
            print_slot_with_id(builder, load.src.id, output);
            break;
        }
        case IR_STORE:
        {
            // store dst, src
            Store store = instr->payload.store_payload;
            if (store.src.value_kind == IRVAL_IMM) {
                fprintf(output, "store ");
                print_slot_with_id(builder, store.dst.id, output);
                fprintf(output, ", %lld", store.src.value_id.imm);
            }
            else if (store.src.value_kind == IRVAL_TEMP) {
                fprintf(output, "store ");
                print_slot_with_id(builder, store.dst.id, output);
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
            Halt halt = instr->payload.halt_payload;
            if (halt.code.value_kind == IRVAL_IMM) {
                fprintf(output, "halt %lld", halt.code.value_id.imm);
            }
            else if (halt.code.value_kind == IRVAL_TEMP) {
                fprintf(output, "halt t%zu", halt.code.value_id.temp_id.id);
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
            print_block_label(output, fid, br.non_zero); // true br
            fprintf(output, " ");
            print_block_label(output, fid, br.zero); // false br

            break;
        }
        case IR_JUMP:
        {
            Jump jump = instr->payload.jump_pl;
            fprintf(output, "jump ");
            print_block_label(output, fid, jump.jump_to);
            break;
        }
        case IR_UNDEFINED:
        {
            fprintf(output, "ERROR: Op not created/found (likely missing terminator).");
        }
        
    }
    fprintf(output, "\n");
}

bool dump_mir(IRBuilder* builder, FILE* output) {
    // Goes through all instructions and prints accordingly
    size_t block_index = 0;
    for (int i = 0; i < builder->funcs.count; i++) {
        IRFunction* f = get_nth_func(&builder->funcs, i);
        fprintf(output, "function_%zu:\n", f->id.id);

        for (int j = 0; j < f->blocks.count; j++) {
            fprintf(output, "\n  ");
            IRBlock* b = get_nth_block(f, j);
            print_block_label(output, f->id, b->id);
            fprintf(output, ":\n");

            for (int k = 0; k < b->instructions.count; k++) {
                fprintf(output, "    ");
                IRInstruct* instruction = get_nth_instruction(b, k);
                print_instruction(builder, instruction, output, f->id);
            }
            // Print terminator
            fprintf(output, "    ");
            print_instruction(builder, &b->term, output, f->id);
        }
    }
    fprintf(output, "\n");
    return true;
}
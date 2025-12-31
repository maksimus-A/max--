#include "codegen/ir-gen/mir.h"
#include "arena/arena.h"
#include "ast/parser/ast.h"
#include "common.h"
#include "semantics/walker.h"
#include "table/ptrtable.h"
#include <assert.h>
#include <stdalign.h>

TempId create_temp_id(IRBuilder* builder);
bool emit_load(IRBuilder* builder, ASTNode* node, TempId dst, SlotId src);
SlotId get_slot_id(IRBuilder* builder, ASTNode* node);

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
bool insert_instruction(IRBuilder* builder, IRInstruct inst) {
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



/*------ AST Node -> IR Value/Slot conversion ------*/

// Currently supports turning immediates and variables
// into IR values, and emits a load into temp.
IRValue get_ir_value(IRBuilder* builder, ASTNode* node) {
    // TODO*: Be explicit this emits a load (for your future self).
    IRValue val = {0};

    switch (node->ast_kind) {
        case AST_INT_LIT:
        {
            val.value_kind = IRVAL_IMM;
            val.value_id.imm = node->node_info.int_lit.value;
            break;
        }
        case AST_NAME:
        {
            val.value_kind = IRVAL_TEMP;
            IRFunction* f = get_curr_func(builder);
            // Construct new temp
            TempId temp = create_temp_id(builder);
            val.value_id.temp_id = temp;
            // Emit a load here with the temp we create.
            SlotId var_slot = get_slot_id(builder, node);
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
SlotId get_slot_id(IRBuilder* builder, ASTNode* node) {
    SlotId slot_id = {0};
    switch (node->ast_kind) {
        case (AST_NAME):
        {
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

// Returns the next available temp id, and increments
// our temp id counter.
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
        fprintf(stderr, "Failed to insert 'store' instruction into builder.");
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
        fprintf(stderr, "Failed to insert 'load' instruction into builder.");
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
    if (!insert_instruction(builder, halt_inst)) {
        fprintf(stderr, "Failed to insert 'halt' instruction into builder.");
        return false;
    }
    return true;
}
/*------ Instruction Emissions ------*/

/*------ Initializations ------*/

IRBlock block_init(IRBuilder* builder) {
    IRBlock block = {0};
    block.id = builder->next_block_id;
    builder->next_block_id.id++;

    VEC_INIT_T(&block.instructions, builder->arena, IRInstruct);

    return block;
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
    IRBlock block = {0};
    block = block_init(builder);
    // Set block as 1st in block vec
    VEC_PUSH_T(&func.blocks, block);
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
void mir_gen_pre(void* user, ASTNode* node) {
    IRBuilder* builder = (IRBuilder*) user;

    switch (node->ast_kind) {
        case AST_PROGRAM:
        {
            // Create function.
            begin_function(builder);
            break;
        }
        default: break;
    }
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
            ASTNode* expr_node = get_child_expr(node);
            if (expr_node == NULL) return;
            // todo: heavily relies on the fact the RHS is either int lit or one var
            IRValue expr_val = get_ir_value(builder, expr_node); // emits load op
            // slot of RHS
            SlotId lhs_slot = get_slot_id(builder, node);

            // Add 1 to function slot count for later, and
            // add slot_id to function slots table.
            IRFunction* f = get_curr_func(builder);
            
            // Gets symbol from node, sets slot_i to symbol for function.
            set_ptr_tbl(&f->slot_sym, node->node_info.var_decl.symbol, f->next_slot_id);
            f->next_slot_id++;

            assert(lhs_slot.id != SIZE_MAX);
            // Emit 1 store op
            emit_store(builder, node, lhs_slot, expr_val);
            break;
        }
        case AST_ASSN:
        {
            ASTNode* expr_node = get_child_expr(node);
            if (expr_node == NULL) return;
            // todo: heavily relies on the fact the RHS is either int lit or one var
            IRValue expr_val = get_ir_value(builder, expr_node);
            // slot of RHS
            SlotId lhs_slot = get_slot_id(builder, node);
            assert(lhs_slot.id != SIZE_MAX);
            // Emit 1 store op
            emit_store(builder, node, lhs_slot, expr_val);
            break;
        }
        // TODO: In AST_BINOP (or whatever it's called), call 'get_ir_val' and store results in 'slots' table.
        case AST_NAME:
        {

            break;
        }
        case AST_EXIT:
        {
            
            ASTNode* expr_node = get_child_expr(node);
            if (expr_node == NULL) return;

            IRValue expr_val = get_ir_value(builder, expr_node);
            emit_halt(builder, node, expr_val);
            break;
        }

        default: break;
    }
}

Visitor mir_gen_visitor = {
    .pre = mir_gen_pre,
    .post = mir_gen_post
};

void run_mir_gen(ASTNode* ast_root, IRBuilder* builder) {
    walk_node(&mir_gen_visitor, builder, ast_root);
}

void builder_init(IRBuilder* builder, Arena* arena, Diagnostics* diags, Semantics* sema, Source* source_file) {
    // initialize IRFunc
    VEC_INIT_T(&builder->funcs, arena, IRFunction);
    builder->next_block_id.id = 0;

    builder->curr_func_index = SIZE_MAX;
    builder->curr_block_index = SIZE_MAX;

    builder->diags = diags;
    builder->arena = arena;
    builder->sema = sema;
    builder->source_file = source_file;
}

/*------ MIR PRINTING ------*/

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

void print_instruction(IRBuilder* builder, IRInstruct* instr, FILE* output) {
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

        default: break;
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
            fprintf(output, "  ");
            IRBlock* b = get_nth_block(f, j);
            fprintf(output, "block_%zu:\n", b->id.id);

            for (int k = 0; k < b->instructions.count; k++) {
                fprintf(output, "    ");
                IRInstruct* instruction = get_nth_instruction(b, k);
                print_instruction(builder, instruction, output);
            }
        }
    }
    return true;
}
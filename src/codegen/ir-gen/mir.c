#include "codegen/ir-gen/mir.h"
#include "arena/arena.h"
#include "ast/parser/ast.h"
#include "semantics/walker.h"
#include <stdalign.h>

// Insert instruction into proper placement in builder
// (should be holding a pointer to current func/block).
bool insert_instruction(IRBuilder* builder, IRInstruct inst) {
    // bounds checks
    if (builder->curr_block_index == SIZE_MAX) return false;
    if (builder->curr_func_index == SIZE_MAX) return false;
    if (builder->curr_func_index >= builder->funcs.count) return false;
    // use macro to get pointer to current IRFunc element
    IRFunction* f = VEC_AT_PTR_T(&builder->funcs, IRFunction, builder->curr_func_index);
    if (builder->curr_block_index >= f->blocks.count) return false;


    // use macro to get pointer to current IRBlock element
    IRBlock* b = VEC_AT_PTR_T(&f->blocks, IRBlock, builder->curr_block_index);
    // Add instruction to current block.
    VEC_PUSH_T(&b->instructions, inst);

    return true;
}

/*------ Instruction Emissions ------*/
IRInstruct emit_store(IRBuilder* builder, ASTNode* node, SlotId dst, IRValue src) {
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
    insert_instruction(builder, store_inst);

    // todo: might not need to return since it's inserted by builder.
    return store_inst;
}

bool emit_load() {

}

bool emit_halt() {

}
/*------ Instruction Emissions ------*/

/*------ Initializations ------*/

IRBlock block_init(IRBuilder* builder) {
    IRBlock block;
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
            // Pop scope
            break;
        }
        case AST_VAR_DEC: 
        {
            ASTNode* expr = get_child_expr(node);
            if (expr == NULL) return;

            // get RHS, emit store slot(x), RHS
            break;
        }
        case AST_ASSN:
        {

            break;
        }
        case AST_NAME:
        {

            break;
        }
        case AST_EXIT:
        {

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

void builder_init(IRBuilder* builder, Arena* arena, Diagnostics* diags) {
    // initialize IRFunc
    VEC_INIT_T(&builder->funcs, arena, IRFunction);
    builder->next_block_id.id = 0;

    builder->curr_func_index = SIZE_MAX;
    builder->curr_block_index = SIZE_MAX;

    builder->diags = diags;
    builder->arena = arena;
}
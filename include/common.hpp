// std::holds_alternative helper. Sick of writing it out man.
#include "codegen/backend/backend_context.hpp"
#include <variant>


template <typename T, typename Variant>
bool alt(const Variant& v) {
    return std::holds_alternative<T>(v);
}

template <typename T, typename Variant>
T& get(Variant& v) {
    return std::get<T>(v);
}

template <typename T, typename Variant>
const T& get(const Variant& v) {
    return std::get<T>(v);
}

void run_frame_layout(BackendContext& ctx, bool debug);
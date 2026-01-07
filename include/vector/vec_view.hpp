#include <type_traits>
#include <cstddef>   // size_t
#include <cassert>   // assert
extern "C" {
  #include "vector/vec.h"
}

/* Using this for accessing my C vectors nicely. 
Functions, blocks, and instructions are all vectors.*/
template <typename T>
class VecView {
public:
  // Construct from your C Vector
  explicit VecView(const Vector* v_) : v(v_) {
    assert(v && "VecView got null Vector*");
    // Optional safety checks (HIGHLY recommended):
    assert(v->item_size == sizeof(T));
    assert(v->item_align == std::alignment_of<T>::value);
  }

  // Size of C vector
  std::size_t size() const {return v->count;}

  // Lets me index vectors by []
  const T& operator[](std::size_t i) const {
    assert(i < v->count);
    return reinterpret_cast<const T*>(v->items)[i];
  }

  // Beginning/end functions of the C vector.
  const T* begin() const {return reinterpret_cast<const T*>(v->items);}
  const T* end() const {return begin() + v->count;}

private:
  const Vector* v;

};
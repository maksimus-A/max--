#include <iostream>
#include <stdint.h>
#include <vector>
#include <bitset>

struct BitSet {
public:
    std::vector<uint64_t> bits;
    size_t num_bits;

    explicit BitSet(size_t num_bits_): num_bits(num_bits_) {
        zero_bit_vec_init(num_bits);
    }

    void set(size_t bit_index) {

        uint64_t new_bits;
        uint64_t word = get_word(bit_index);
        uint64_t offset = bit_index % 64;
        new_bits = word | (1ULL << offset);

        bits[get_vec_index(bit_index)] = new_bits;
    }

    void clear(size_t bit_index) {
        uint64_t new_bits;
        uint64_t word = get_word(bit_index);
        uint64_t offset = bit_index % 64;

        // int of all 1's except 'bit_index' 0
        uint64_t mask = ~uint64_t{0};
        mask &= ~(uint64_t{1} << offset);
        new_bits = word & mask;

        bits[get_vec_index(bit_index)] = new_bits;
    }

    bool test(size_t bit_index) const {
        assert(bit_index < num_bits);
        size_t word = bit_index / 64;
        size_t offset = bit_index % 64;

        return (bits[word] & (1ULL << offset)) != 0;
    }

    // Zeroes existing vector.
    void zero_bit_vec() {
        for (int i = 0; i < bits.size(); i++) {
            bits[i] = 0ULL;
        }
    }

    // And's with entire vector. Assumes sizes are the same.
    BitSet and_with(const BitSet& bitset) const {
        assert(bitset.num_bits == num_bits);

        BitSet result = *this;

        for (size_t i = 0; i < bits.size(); i++) {
            result.bits[i] &= bitset.bits[i];
        }

        return result;
    }

    void and_assign(const BitSet& bitset) {
        assert(bitset.num_bits == num_bits);

        for (size_t i = 0; i < bits.size(); i++) {
            bits[i] &= bitset.bits[i];
        }
    }

    BitSet and_not_with(const BitSet& other) const {
        assert(other.num_bits == num_bits);

        BitSet result = *this;

        for (size_t i = 0; i < result.bits.size(); i++) {
            result.bits[i] &= ~other.bits[i];
        }

        return result;
    }

    void and_not_assign(const BitSet& other) {
        assert(other.num_bits == num_bits);

        for (size_t i = 0; i < bits.size(); i++) {
            bits[i] &= ~other.bits[i];
        }
    }

    BitSet or_with(const BitSet& bitset) const {
        assert(bitset.num_bits == num_bits);

        BitSet result = *this;

        for (size_t i = 0; i < bits.size(); i++) {
            result.bits[i] |= bitset.bits[i];
        }
        return result;
    }

    void or_assign(const BitSet& bitset) {
        assert(bitset.num_bits == num_bits);

        for (size_t i = 0; i < bits.size(); i++) {
            bits[i] |= bitset.bits[i];
        }
    }

    bool equals_with(const BitSet& bitset) {
        assert(bitset.num_bits == num_bits);

        for (size_t i = 0; i < bits.size(); i++) {
            if (bits[i] != bitset.bits[i]) return false;
        }
        return true;
    }

    
    void print_bitset() {
        for (auto& bit: bits) {
            print_bits(bit);
            std::cout << std::endl;
        }
    }


private:
    void print_bits(uint64_t x) {
        for (int i = 63; i >= 0; --i) {
            if ((i+1) % 8 == 0) std::cout << " ";
            std::cout << ((x >> i) & 1ULL);
        }
    }

    // Zeroes bit vector as init.
    void zero_bit_vec_init(size_t num_bits) {
        size_t num_vecs = (num_bits + 63) / 64;
        while (num_vecs > 0) {
            bits.emplace_back(0);
            num_vecs--;
        }
    }

    // Gets the proper vector based on index of bit specified.
    uint64_t get_word(size_t bit_index) {
        assert(bit_index < num_bits);

        size_t vec_index = get_vec_index(bit_index);
        return bits[vec_index];
    }

    size_t get_vec_index(size_t bit_index) {
        return bit_index / 64;
    }
};
#pragma once

#include <array>
#include <cstring>

namespace bf {


	/*CPU's memory. Has a size and type specified at compile time and uses std::array to reserve enough space for
	the data section of memory. As a default handles single bytes as elements.*/
	template<int SIZE, typename CELL = unsigned char>
	struct memory {

		using cell_t = CELL;
		static constexpr int size_ = SIZE;

		std::array<cell_t, size_> memory_ = { 0 };

		constexpr int size() const { return size_; }
		constexpr cell_t *data() const { return const_cast<cell_t*>(memory_.data()); }

		cell_t& operator[](int n) { return memory_[n]; }
		const cell_t& operator[](int n) const { return memory_[n]; }

		cell_t *begin() { return memory_.data(); }
		cell_t const *begin() const { return memory_.data(); }

		cell_t *end() { return memory_.data()  + memory_.size(); }
		cell_t const *end() const { memory_.data() + memory_.size(); }

		void reset() {
			std::memset(memory_.data(), 0, memory_.size());
		}

	};
}
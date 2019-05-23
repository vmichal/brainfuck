#pragma once

#include <array>
#include <cstring>

namespace bf {


	/*CPU's memory. Has a size and type specified at compile time and uses std::array to reserve enough space for
	the data section of memory. As a default handles single bytes as elements.*/
	template<int SIZE, typename CELL = unsigned char>
	struct memory : public std::array<CELL, SIZE> {

		using base_type = std::array<CELL, SIZE>;
		using cell_t = typename base_type::value_type;
		static constexpr int size_ = SIZE;

		void reset() {
			std::memset(this->data(), 0, size_);
		}
	};
}
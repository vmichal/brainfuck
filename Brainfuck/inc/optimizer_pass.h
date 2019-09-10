#pragma once

#include "program_code.h"

#include <vector>
#include <cstddef>

namespace bf::opt {

	class optimizer_pass {

	public:
		virtual std::ptrdiff_t run(basic_block*) = 0;
		virtual std::ptrdiff_t run(std::vector<basic_block*>&) = 0;
	};

}

#pragma once

#include "syntax_check.h"

#include <string_view>
#include <vector>
#include <string>

namespace bf {

	/*Class holding a list of the intermediate language instructions. Represents a program with capabilities of automatic 
	jump target relocation. And pseudo-vector interface*/
	class syntax_tree : public std::vector<instruction> {

	public:


		void add_instruction(instruction const&);
		void add_instruction(instruction&&);

		void add_instruction(instruction_type type, int argument, int source_offset);

		void relocate_jump_targets();

	};



}

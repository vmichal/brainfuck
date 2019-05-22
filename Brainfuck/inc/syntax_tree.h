#pragma once

#include "syntax_check.h"
#include <string_view>
#include <vector>
#include <string>

namespace bf {

	/*Class holding a list of the intermediate language instructions. Represents a program with capabilities of automatic 
	jump target relocation. And pseudo-vector interface*/
	class syntax_tree {

		std::vector<instruction> instructions_;

	public:


		void add_instruction(instruction);
		void add_instruction(instruction_type type, int argument, int source_offset);

		void relocate_jump_targets();


		//pseudo-vector-interface
		int size() const { return static_cast<int>(instructions_.size()); }
		instruction* data() { return instructions_.data(); }
		instruction const* data() const { return instructions_.data(); }

		instruction& operator[](int offset) { return instructions_[offset]; }
		instruction const& operator[](int offset) const { return instructions_[offset]; }

		decltype(instructions_)::const_iterator begin() const { return instructions_.begin(); };
		decltype(instructions_)::const_iterator end() const { return instructions_.end(); };

		decltype(instructions_)::iterator begin() { return instructions_.begin(); };
		decltype(instructions_)::iterator end() { return instructions_.end(); };

		void reserve(int capacity) { instructions_.reserve(capacity); }
		void swap(syntax_tree & rhs) { instructions_.swap(rhs.instructions_); }
	};



}

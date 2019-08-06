#include "program_code.h"
#include <cassert>
#include <unordered_map>
#include <unordered_set>


namespace bf {

	std::string const& get_mnemonic(op_code const code) {
		static std::unordered_map<op_code, std::string> const op_code_mnemonics = {
			{op_code::nop,                   "nop"},
			{op_code::inc,					 "inc"},
			{op_code::dec,					 "dec"},
			{op_code::left,			        "left"},
			{op_code::right,			   "right"},
			{op_code::jump,	                "jump"},
			{op_code::jump_not_zero,	 "jump_nz"},
			{op_code::read,				    "read"},
			{op_code::write,		       "write"},
			{op_code::breakpoint,     "breakpoint"},
			{op_code::load_const,     "load_const"},
			{op_code::program_exit,         "exit"},
			{op_code::program_entry,       "entry"}
		};
		assert(op_code_mnemonics.count(code));
		return op_code_mnemonics.at(code);
	}

	bool instruction::is_foldable() const {
		static std::unordered_set<op_code> const foldable {
			op_code::inc,
			op_code::dec,
			op_code::left,
			op_code::right
		};
		return foldable.count(op_code_);
	}

	std::ostream& operator<<(std::ostream& str, op_code code) {
		return str << get_mnemonic(code);
	}

}
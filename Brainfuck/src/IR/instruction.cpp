#include "IR/instruction.h"

#include <map>
#include <cassert>

namespace bf::IR {


	std::string const& get_mnemonic(op_code const code) {
		using namespace std::string_literals;
		static std::map<op_code, std::string> const op_code_mnemonics{
			{op_code::nop,                   "nop"s},
			{op_code::inc,					 "inc"s},
			{op_code::dec,					 "dec"s},
			{op_code::right,			   "right"s},
			{op_code::left,			        "left"s},
			{op_code::branch,	              "br"s},
			{op_code::branch_nz,           "br_nz"s},
			{op_code::read,				    "read"s},
			{op_code::write,		       "write"s},
			{op_code::search,             "search"s},
			{op_code::infinite,         "inf_when"s},
			{op_code::breakpoint,     "breakpoint"s},
			{op_code::load_const,     "load_const"s},
			{op_code::program_exit,         "exit"s},
			{op_code::program_entry,       "entry"s}
		};
		assert(op_code_mnemonics.count(code));
		return op_code_mnemonics.at(code);
	}


	std::ostream& operator<<(std::ostream& str, op_code const code) {
		return str << get_mnemonic(code);
	}

}
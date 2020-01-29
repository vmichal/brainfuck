#pragma once

#include "program_code.h"
#include "opt/optimizer_pass.h"
#include <vector>

namespace bf::opt {

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(local_const_propagator)

	enum class arithmetic_tag {
		pointer,	//Optimize shifts of the cell pointer
		value,   	//Operations modifying values of memory cells
		both		//Optimize value as well as pointer arithmetic
	};


	template<arithmetic_tag TAG>
	DEFINE_PEEPHOLE_OPTIMIZER_PASS(arithmetic_simplifier)




	/*Attempts to simplify computations within the given basic block by propagating values that are known at compile time.
	Identifies cells that have a known value after executing the load_const instruction and eliminates all other arithmetic
	instructions that modify this cell.*/

	/*Wrapper for optimizations of arithmetic operations in the given basic block. Under arithmetic operations fall both pointer
	arithmetic and arithmetic on cell values, which are chosen and perfomed based on the type of template tag parameter.
	Pointer arithmetic is a sequence of two or more consecutive shift instructions 'left' and 'right'. E.g. <<>><><<<><>>>>>>>><>><<><><>;
	Value arithmetic is a sequence of two or more consecutive instructions 'inc' and 'dec', which modify the values in memory. E.g +++++++---+-----;
	These sequences are identified, evaluated and in accordance to the as-if rule replaced by a single instruction that has the same effect.

	Returns the number of eliminated instructions.*/

}
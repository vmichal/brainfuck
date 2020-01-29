#pragma once

#include "program_code.h"
#include "opt/optimizer_pass.h"

namespace bf::opt {

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(pure_ujump_elimination);

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(cjump_destination_optimization);
	
	DEFINE_PEEPHOLE_OPTIMIZER_PASS(single_entry_cjump_optimization);



	/*Simplifies control flow in chains of pure conditional blocks, that is basic blocks that contain nothing but a single conditional jump instruction.
		Such chains have multiple cjumps in a row that all depend on the same condition. It is therefore not necessary to evaluate the same condition multiple
		times. Should the check fail it will fail for the whole chain; should it succeed, it will be true for all cjumps in a chain, as there ano no other
		instructions that would change the program's state.

		Function identifies such chains and redirects targets of all these jumps to blocks after the chain skipping excess condition checking.
		Blocks within identified chains lose a predecessor during this process, which can lead to further optimizations.
		Both true and false branches are optimized simultaneously.

		Returns the number of made changes (therefore zero means the code is already optimized). */


}
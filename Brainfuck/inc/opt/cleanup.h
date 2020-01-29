#pragma once

#include "program_code.h"
#include "opt/optimizer_pass.h"


#include <vector>
#include <numeric>
#include <execution>

namespace bf::opt {


	DEFINE_PEEPHOLE_OPTIMIZER_PASS(empty_block_elimination);

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(block_merging);

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(nop_elimination);

	DEFINE_GLOBAL_OPTIMIZER_PASS(dead_code_elimination);


	/*Traverses the program's graph and marks all basic blocks that are not reachable by the control flow.
		These marked blocks are said to be dead. They are orphaned and eliminated.*/

		/*Simplifies chains of unconditional jumps. Identifies all pure ujumps, that is basic blocks that do nothing but unconditionally jump elsewhere.
		Each pure ujump block is eliminated from the program by redirecting the jumps of its predecessors to its destination,
		effectively skipping the additional ujump entirely. A new bond is formed between all predecessors and the new destination,
		the ujump itself is orphaned and erased.
		Returns the number of modifications that were performed. Zero therefore means that no further optimization could take place. */


}
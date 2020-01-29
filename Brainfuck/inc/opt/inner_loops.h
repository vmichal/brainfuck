#pragma once

#include "program_code.h"
#include "opt/optimizer_pass.h"
#include <vector>
#include <variant>

namespace bf::opt {

	DEFINE_PEEPHOLE_OPTIMIZER_PASS(infinite_loop_optimizer);
	DEFINE_PEEPHOLE_OPTIMIZER_PASS(clear_loop_optimizer);
	DEFINE_PEEPHOLE_OPTIMIZER_PASS(search_loop_optimizer);


	/*Eliminates all loops which have no observable side effects. Such loops perform no IO and don't move the cell pointer anywhere, they only
	change the value of current cell. After this elimination, blocks that are no longer needed are removed and pointer connections between
	surrounding blocks are formed again.
	Since integer overflows are well-defined, a finite number of increments or decrements always results in the value zero,
	which would break the loop. This function therefore eliminates all loops with any number of + and - instructions.
	Returns the number of eliminated loops. (Zero therefore means that there are no more loops to optimize.)*/

	/*Identifies conditional jumps that target themselves and replaces them by an instruction marking an infinite loop.*/


//TODO implement optimization of cond branches that have known incoming values

//TODO all std::execution::par_unseq are now replaced by std::execution::seq for debugging
/*Eliminates all loops which have no observable side effects. Such loops perform no IO and don't move the cell pointer anywhere, they only
change the value of current cell. After this elimination, blocks that are no longer needed are removed and pointer connections between
surrounding blocks are formed again.
Since integer overflows are well-defined, a finite number of increments or decrements always results in the value zero,
which would break the loop. This function therefore eliminates all loops with any number of + and - instructions.
Returns the number of eliminated loops. (Zero therefore means that there are no more loops to optimize.)*/



}

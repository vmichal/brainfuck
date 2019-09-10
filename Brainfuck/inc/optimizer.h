#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#pragma once

#include "program_code.h"

#include <optional>
#include <set>

namespace bf::opt {

	enum class opt_level_t {
		op_folding = 1 << 0,
		dead_code_elimination = 1 << 1,
		const_propagation = 1 << 2,
		loop_analysis = 1 << 3,

		all = ~static_cast<std::underlying_type_t<opt_level_t>>(0),
	};

	/*Get optimization bitmask from its name.*/
	[[nodiscard]]
	std::optional<opt_level_t> get_opt_by_name(std::string_view optimization_name);


	/*Searches for orphaned basic blocks and removes them from the given std::vector.
	Orphaned blocks are those that have lost all connections to other basic blocks, i.e. they neither any predecessor,
	nor any successor. Many optimization routines use orphaning as a sign that the block can be safely deleted.*/
	std::ptrdiff_t erase_orphaned_blocks(std::vector<basic_block*>& program);

	void assert_program_invariants(std::vector<basic_block*> const& basic_blocks);


	namespace peephole {

		/*Removes all no-op instructions.*/
		std::ptrdiff_t remove_nops(basic_block* block);
		std::ptrdiff_t remove_nops(std::vector<basic_block*>& program);


		/*Attempts to simplify computations within the given basic block by propagating values that are known at compile time.
		Identifies cells that have a known value after executing the load_const instruction and eliminates all other arithmetic
		instructions that modify this cell.*/
		std::ptrdiff_t propagate_local_const(basic_block* block);
		std::ptrdiff_t propagate_local_const(std::vector<basic_block*>& program);

		/*Identifies conditional jumps that target themselves and replaces them by an instruction marking an infinite loop.*/
		std::ptrdiff_t eliminate_infinite_loops(basic_block* block);
		std::ptrdiff_t eliminate_infinite_loops(std::vector<basic_block*>& program);

		enum class arithmetic_tag {
			pointer,	//Optimize shifts of the cell pointer
			value,   	//Operations modifying values of memory cells
			both		//Optimize value as well as pointer arithmetic
		};

		/*Wrapper for optimizations of arithmetic operations in the given basic block. Under arithmetic operations fall both pointer
		arithmetic and arithmetic on cell values, which are chosen and perfomed based on the type of template tag parameter.
		Pointer arithmetic is a sequence of two or more consecutive shift instructions 'left' and 'right'. E.g. <<>><><<<><>>>>>>>><>><<><><>;
		Value arithmetic is a sequence of two or more consecutive instructions 'inc' and 'dec', which modify the values in memory. E.g +++++++---+-----;
		These sequences are identified, evaluated and in accordance to the as-if rule replaced by a single instruction that has the same effect.

		Returns the number of eliminated instructions.*/
		template<arithmetic_tag TAG>
		std::ptrdiff_t simplify_arithmetic(basic_block* block);
		template<arithmetic_tag TAG>
		std::ptrdiff_t simplify_arithmetic(std::vector<basic_block*>& program);

		/*Function performing optimizations on single given basic block folding multiple consecutive operations that do the same.
		Traverses the given basic block instruction by instruction and if there are multiple identical instructions next to each other, folds them into one.
		Returns the number of folds performed (zero therefore means that the most optimized code was generated).*/
		std::ptrdiff_t combine_adjacent_operations(basic_block* block);
		std::ptrdiff_t combine_adjacent_operations(std::vector<basic_block*>& program);

		/*Eliminates all loops which have no observable side effects. Such loops perform no IO and don't move the cell pointer anywhere, they only
		change the value of current cell. After this elimination, blocks that are no longer needed are removed and pointer connections between
		surrounding blocks are formed again.
		Since integer overflows are well-defined, a finite number of increments or decrements always results in the value zero,
		which would break the loop. This function therefore eliminates all loops with any number of + and - instructions.
		Returns the number of eliminated loops. (Zero therefore means that there are no more loops to optimize.)*/
		std::ptrdiff_t eliminate_clear_loops(basic_block* block);
		std::ptrdiff_t eliminate_clear_loops(std::vector<basic_block*>& program);

		//Locates all empty blocks and eliminates them
		std::ptrdiff_t eliminate_empty_blocks(basic_block* block);
		std::ptrdiff_t eliminate_empty_blocks(std::vector<basic_block*>& program);

		//TODO add optimization of search loops [>] or [<]
		//TODO merge some optimizations into "inner loop optimizer"


	}


	namespace global {

		std::ptrdiff_t merge_into_predecessor(basic_block* block);
		std::ptrdiff_t merge_into_predecessor(std::vector<basic_block*>& program);

		/*Traverses the program's graph and marks all basic blocks that are not reachable by the control flow.
		These marked blocks are said to be dead. They are orphaned and eliminated.*/
		std::ptrdiff_t delete_unreachable_blocks(std::vector<basic_block*>& program);

		/*Simplifies chains of unconditional jumps. Identifies all pure ujumps, that is basic blocks that do nothing but unconditionally jump elsewhere.
		Each pure ujump block is eliminated from the program by redirecting the jumps of its predecessors to its destination,
		effectively skipping the additional ujump entirely. A new bond is formed between all predecessors and the new destination,
		the ujump itself is orphaned and erased.
		Returns the number of modifications that were performed. Zero therefore means that no further optimization could take place. */
		std::ptrdiff_t eliminate_pure_uncond_jumps(basic_block* block);
		std::ptrdiff_t eliminate_pure_uncond_jumps(std::vector<basic_block*>& program);

		/*Simplifies control flow in chains of pure conditional blocks, that is basic blocks that contain nothing but a single conditional jump instruction.
		Such chains have multiple cjumps in a row that all depend on the same condition. It is therefore not necessary to evaluate the same condition multiple
		times. Should the check fail it will fail for the whole chain; should it succeed, it will be true for all cjumps in a chain, as there ano no other
		instructions that would change the program's state.

		Function identifies such chains and redirects targets of all these jumps to blocks after the chain skipping excess condition checking.
		Blocks within identified chains lose a predecessor during this process, which can lead to further optimizations.
		Both true and false branches are optimized simultaneously.

		Returns the number of made changes (therefore zero means the code is already optimized). */
		std::ptrdiff_t optimize_cond_jump_destination(basic_block* block);
		std::ptrdiff_t optimize_cond_jump_destination(std::vector<basic_block*>& program);

		std::ptrdiff_t eliminate_single_entry_conditionals(basic_block* block);
		std::ptrdiff_t eliminate_single_entry_conditionals(std::vector<basic_block*>& program);


	}

	void perform_optimizations(std::vector<std::unique_ptr<basic_block>>& program_basic_blocks, std::set<opt_level_t> const& optimizations);

	/*Initialize CLI commands that control the function of optimizer.
	Shall be called only once.*/
	void initialize();

}  //namespace bf::opt

#endif
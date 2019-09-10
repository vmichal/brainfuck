#include "optimizer.h"
#include "utils.h"
#include "analysis.h"

#include <algorithm>
#include <execution>
#include <functional>
#include <cassert>
#include <map>
#include <stack>
#include <iterator>

namespace bf::opt::peephole {



	std::ptrdiff_t remove_nops(basic_block* const block) {
		if (!block)
			return 0;
		auto const old_end = block->ops_.end();
		auto const new_end = std::remove_if(block->ops_.begin(), old_end, std::mem_fn(&instruction::is_nop));
		block->ops_.erase(new_end, old_end);

		return std::distance(new_end, old_end);
	}

	std::ptrdiff_t remove_nops(std::vector<basic_block*>& program) {
		assert_program_invariants(program);

		return std::transform_reduce(std::execution::seq, program.begin(), program.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(remove_nops));
	}


	namespace {

		void do_propagate_local_const_fwd(analysis::same_offset_iterator same_offset_iter) {
			assert(same_offset_iter);
			assert(same_offset_iter->is_const());
			instruction& constant = *same_offset_iter;

			while (++same_offset_iter)
				if (instruction& inst = *same_offset_iter; inst.is_arithmetic()) {
					constant.argument_ += inst.argument_;
					inst.make_nop();
				}
				else if (inst.is_const()) {
					constant.make_nop();
					break;
				}
				else if (inst.is_io())
					break;
		}

		void do_propagate_local_const_back(analysis::same_offset_iterator same_offset_iter) {
			assert(same_offset_iter);
			assert(same_offset_iter->is_const());

			while (--same_offset_iter)
				if (instruction& inst = *same_offset_iter; inst.is_arithmetic())
					inst.make_nop();
				else if (inst.is_const())
					MUST_NOT_BE_REACHED;
				else if (inst.is_io())
					break;
		}
	}

	std::ptrdiff_t propagate_local_const(basic_block* const block) {
		if (!block)
			return 0;

		analysis::ptr_movement_local_result analysis_res = analysis::analyze_pointer_movement_local(block);

		for (auto iter_to_const = std::find_if(block->ops_.begin(), block->ops_.end(), std::mem_fn(&instruction::is_const));
			iter_to_const != block->ops_.end();	iter_to_const = std::find_if(std::next(iter_to_const), block->ops_.end(), std::mem_fn(&instruction::is_const))) {

			analysis::same_offset_iterator const same_offset_iter = analysis_res.offset_iterator(iter_to_const);

			do_propagate_local_const_fwd(same_offset_iter);
			do_propagate_local_const_back(same_offset_iter);
		}

		return remove_nops(block);
	}

	std::ptrdiff_t propagate_local_const(std::vector<basic_block*>& basic_blocks) {

		return std::transform_reduce(std::execution::seq, basic_blocks.begin(), basic_blocks.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(&propagate_local_const));
	}

	namespace eliminate_infinite_loops_helper {

		std::ptrdiff_t eliminate_self_loop(basic_block* const condition) {
			assert(condition && condition->is_pure_cjump() && condition->has_self_loop());
			assert(condition->natural_successor_ != condition); //TODO this is only for testing purposes to see, whether it can happen
			assert(condition->jump_successor_ == condition);

			condition->remove_predecessor(condition);
			condition->jump_successor_ = nullptr;
			condition->ops_.front().make_infinite_on_not_zero();
			return 1;
		}

		std::ptrdiff_t eliminate_const_loop_body(basic_block* const condition) {
			assert(condition && condition->is_pure_cjump() && !condition->has_self_loop());
			basic_block* const loop_body = condition->jump_successor_;
			if (loop_body->predecessors_.size() > 1 || loop_body->is_jump() || !loop_body->has_successor(condition))
				return 0;

			analysis::block_evaluation_analyzer const body_eval{ loop_body };
			if (body_eval.has_sideeffects())
				return 0;
			if (body_eval.has_non_zero_result()) {
				condition->ops_.front().make_infinite_on_not_zero();
				loop_body->orphan();
			}
			return 0;
		}

	}

	//TODO implement optimization of cond branches that have known incoming values

	//TODO all std::execution::par_unseq are now replaced by std::execution::seq for debugging

	std::ptrdiff_t eliminate_infinite_loops(basic_block* const condition) {
		if (!condition || !condition->is_pure_cjump())
			return 0;

		namespace helper = eliminate_infinite_loops_helper;

		if (condition->has_self_loop())
			return helper::eliminate_self_loop(condition);
		else
			return helper::eliminate_const_loop_body(condition);
	}

	std::ptrdiff_t eliminate_infinite_loops(std::vector<basic_block*>& program) {

		return std::transform_reduce(std::execution::seq, program.begin(), program.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(eliminate_infinite_loops));
	}

	namespace {

		template<arithmetic_tag TAG>
		struct simplify_arithmetic_traits : std::false_type {};

		template<>
		struct simplify_arithmetic_traits<arithmetic_tag::value> : std::true_type {

			static constexpr bool(instruction::* PREDICATE)() const = &instruction::is_arithmetic;
			static constexpr op_code INST = op_code::inc;
		};

		template<>
		struct simplify_arithmetic_traits<arithmetic_tag::pointer> : std::true_type {
			static constexpr bool(instruction::* PREDICATE)() const = &instruction::is_shift;
			static constexpr op_code INST = op_code::right;
		};


		/*Performs the optimization of simplifying arithmetic operations in the given basic block.*/
		template<arithmetic_tag TAG>
		std::ptrdiff_t do_simplify_arithmetic(basic_block* const block) {

			using traits = simplify_arithmetic_traits<TAG>;
			static_assert(traits::value, "Invalid value of template tag used!");
			static_assert(std::is_signed_v<decltype(instruction::argument_)>); //for this code to work safely we need to have instruction::argument_ signed!

			/*Type parameters:
			TAG = Either 'arithmetic_tag::pointer' or 'arithmetic_tag::value'. Disambiguates the type of arithmetic to be optimized.
			PREDICATE = Returns true iff the given instruction has to be considered by the optimization.
			INST = Opcode that is considered positive in the given arithmetic operation. Either 'op_code::inc' or 'op_code::right.' */

			if (!block)
				return 0;
			//This function makes instrucitons nop when they shall be removed. This makes sure that it's ok!
			assert(std::none_of(std::execution::seq, block->ops_.begin(), block->ops_.end(), std::mem_fn(&instruction::is_nop)));

			/*Algorithm:
			Take each contiguous range of instructions from block's code that satisfy the predicate. If it consists of less than two instructions, ignore it.

			Compute the overall result of the range:
				Sum arguments of all instructions with op_code == traits::POSITIVE and subtract the sum of arguments of all instructions
				with op_code == NEGATIVE from it. These sums can be computed in parallel and unsequenced.

			Set all instructions in the range to nop. All nops will be removed later.

			Modify the leading instruction of the sequence in accordance with the as-if rule to perform such an operation, whose execution exhibits a set of
			observable behaviour that's identical to one that would have been produced, had the execution of the entire unoptimized
			range of instructions been carried out.

			After this process, all arithmetic instructions preceded by other AI can be deleted.
			Then all AI with argument == 0 can be deleted as well. (Additions and subtractions compensated each other and the range can be deletd as a whole.)*/

			for (auto const [head, end] : utils::iterate_ranges_if(block->ops_.begin(), block->ops_.end(), std::mem_fn(traits::PREDICATE))) {
				if (std::distance(head, end) < 2)
					continue;

				//Compute the result of all operations (reduce instructions' arguments)
				std::ptrdiff_t const result_of_operations = std::transform_reduce(std::execution::seq, head, end, std::ptrdiff_t{ 0 },
					std::plus{}, std::mem_fn(&instruction::argument));
				//Make all instructions in the range nops. The first one will be modified to perform requested operation
				std::for_each(std::execution::seq, head, end, std::mem_fn(&instruction::make_nop));

				//If result != 0 make the first instruction perform something
				if (result_of_operations != 0)
					* head = instruction{ traits::INST, result_of_operations, head->source_offset_ };
			}

			return remove_nops(block);
		}
	}

	/*Wrapper for optimizations of arithmetic operations in the given basic block. Under arithmetic operations fall both pointer
	arithmetic and arithmetic on cell values, which are chosen and perfomed based on the type of template tag parameter.
	Pointer arithmetic is a sequence of two or more consecutive shift instructions 'left' and 'right'. E.g. <<>><><<<><>>>>>>>><>><<><><>;
	Value arithmetic is a sequence of two or more consecutive instructions 'inc' and 'dec', which modify the values in memory. E.g +++++++---+-----;
	These sequences are identified, evaluated and in accordance to the as-if rule replaced by a single instruction that has the same effect.

	Returns the number of eliminated instructions.
	*/
	template<arithmetic_tag TAG>
	std::ptrdiff_t simplify_arithmetic(basic_block* const block) {
		if (!block)
			return 0;

		if constexpr (TAG == arithmetic_tag::both)
			return do_simplify_arithmetic<arithmetic_tag::value>(block) + do_simplify_arithmetic<arithmetic_tag::pointer>(block);
		else
			return do_simplify_arithmetic<TAG>(block);
	}


	template<arithmetic_tag TAG>
	std::ptrdiff_t simplify_arithmetic(std::vector<basic_block*>& basic_blocks) {
		//Simplify each basic block separately and reduce partial return values. It is possible to execute in parallel 
		return std::transform_reduce(std::execution::seq, basic_blocks.begin(), basic_blocks.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(&simplify_arithmetic<TAG>));
	}

	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::pointer>(basic_block*);
	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::pointer>(std::vector<basic_block*>& program);

	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::value>(basic_block*);
	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::value>(std::vector<basic_block*>& program);

	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::both>(basic_block*);
	template std::ptrdiff_t simplify_arithmetic<arithmetic_tag::both>(std::vector<basic_block*>& program);




	/*Function performing optimizations on single given basic block folding multiple consecutive operations that do the same.
	Traverses the given basic block instruction by instruction and if there are multiple identical instructions next to each other, folds them into one.
	Returns the number of folds performed (zero therefore means that the most optimized code was generated).*/
	std::ptrdiff_t combine_adjacent_operations(basic_block* const block) {

		for (auto const predicate : { &instruction::is_arithmetic, & instruction::is_shift })
			for (auto [head, end] : utils::iterate_ranges_if(block->ops_.begin(), block->ops_.end(), std::mem_fn(predicate))) {
				if (std::distance(head, end) < 2)
					continue;
				head->argument_ = std::transform_reduce(head, end, std::ptrdiff_t{ 0 }, std::plus{}, std::mem_fn(&instruction::argument));
				std::for_each(head->argument_ ? std::next(head) : head, end, std::mem_fn(&instruction::make_nop));
			}

		return remove_nops(block);
	}

	std::ptrdiff_t combine_adjacent_operations(std::vector<basic_block*>& program) {
		//Simplify each basic block separately and reduce partial return values. It is possible to execute in parallel 
		return std::transform_reduce(std::execution::seq, program.begin(), program.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block * const)>(&combine_adjacent_operations));
	}
	/*Eliminates all loops which have no observable side effects. Such loops perform no IO and don't move the cell pointer anywhere, they only
	change the value of current cell. After this elimination, blocks that are no longer needed are removed and pointer connections between
	surrounding blocks are formed again.
	Since integer overflows are well-defined, a finite number of increments or decrements always results in the value zero,
	which would break the loop. This function therefore eliminates all loops with any number of + and - instructions.
	Returns the number of eliminated loops. (Zero therefore means that there are no more loops to optimize.)*/
	std::ptrdiff_t eliminate_clear_loops(basic_block* const condition) {
		if (!condition || !condition->is_pure_cjump())
			return 0;

		basic_block* const loop_body = condition->jump_successor_;   //
		if (!loop_body || loop_body->is_jump() || !condition->has_predecessor(loop_body))
			return 0;

		//handful of sanity checks
		assert(loop_body->jump_successor_ == nullptr); //
		assert(loop_body->natural_successor_ == condition); //

		if (analysis::analyze_pointer_movement_local(loop_body).ptr_moves())
			return 0;

		analysis::block_evaluation_analyzer const body_eval{ loop_body };
		if (body_eval.has_sideeffects() || body_eval.has_const_result() || body_eval.value_delta() == 0)
			return 0;

		condition->ops_.front() = instruction{ op_code::load_const, 0, loop_body->ops_.front().source_offset_ };
		condition->jump_successor_ = nullptr;
		loop_body->remove_predecessor(condition);
		return 1;
	}

	std::ptrdiff_t eliminate_clear_loops(std::vector<basic_block*>& program) {
		std::ptrdiff_t const res = std::transform_reduce(program.begin(), program.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(&eliminate_clear_loops));
		erase_orphaned_blocks(program);
		return res;
	}

	std::ptrdiff_t eliminate_empty_blocks(basic_block* const block) {
		if (!block || !block->empty())
			return 0;

		assert(block->natural_successor_ && block->jump_successor_ == nullptr);

		basic_block* const new_target = block->natural_successor_;
		for (basic_block* const pred : block->predecessors_) {
			basic_block* basic_block::* modified_connection = pred->choose_successor_ptr(block);
			assert(pred->*modified_connection == block);

			pred->*modified_connection = new_target;
			new_target->add_predecessor(pred);
		}
		block->orphan();
		return 1;
	}

	std::ptrdiff_t eliminate_empty_blocks(std::vector<basic_block*>& program) {
		std::for_each(program.begin(), program.end(),
			static_cast<std::ptrdiff_t(*)(basic_block*)>(&eliminate_empty_blocks));
		return erase_orphaned_blocks(program);
	}


} //namespace bf::opt::peephole


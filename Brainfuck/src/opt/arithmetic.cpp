#include "opt/arithmetic.h"
#include "anal/analysis.h"


#include <execution>
#include <numeric>

namespace bf::opt {

	static void propagate_forward(analysis::same_offset_iterator iter) {
		assert(iter);
		assert(iter->is_const());
		instruction& constant = *iter;

		while (++iter)
			if (instruction& inst = *iter; inst.is_arithmetic()) {
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

	static void propagate_backward(analysis::same_offset_iterator iter) {
		assert(iter);
		assert(iter->is_const());

		while (--iter)
			if (instruction& inst = *iter; inst.is_arithmetic())
				inst.make_nop();
			else if (inst.is_const())
				MUST_NOT_BE_REACHED;
			else if (inst.is_io())
				break;
	}


	std::ptrdiff_t local_const_propagator::do_optimize(basic_block* const block) {
		if (!block)
			return 0;

		analysis::pointer_movement analysis_res{ block };

		for (auto const iter_to_const : block->inst_filter(&instruction::is_const)) {

			analysis::same_offset_iterator const same_offset_iter = analysis_res.offset_iterator(iter_to_const);

			propagate_backward(same_offset_iter);
			propagate_forward(same_offset_iter);
		}
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

			assert(block);
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
	std::ptrdiff_t arithmetic_simplifier<TAG>::do_optimize(basic_block* const block) {
		if (!block)
			return 0;

		if constexpr (TAG == arithmetic_tag::both)
			return do_simplify_arithmetic<arithmetic_tag::value>(block) + do_simplify_arithmetic<arithmetic_tag::pointer>(block);
		else
			return do_simplify_arithmetic<TAG>(block);
	}

#if 0
	template<arithmetic_tag TAG>
	std::ptrdiff_t arithmetic_simplifier<TAG>::optimize(basic_block* const block) {
		return do_optimize(block);
	}

	template<arithmetic_tag TAG>
	std::ptrdiff_t arithmetic_simplifier<TAG>::optimize(std::vector<basic_block*>& program) {
		//Simplify each basic block separately and reduce partial return values. It is possible to execute in parallel 
		return std::transform_reduce(std::execution::seq, program.begin(), program.end(), std::ptrdiff_t{ 0 },
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(optimize));
	}

#endif

	template class arithmetic_simplifier<arithmetic_tag::pointer>;
	template class arithmetic_simplifier<arithmetic_tag::value>;
	template class arithmetic_simplifier<arithmetic_tag::both>;


}
#include "optimizer.h"
#include "cli.h"
#include "compiler.h"
#include "emulator.h"
#include <iostream>
#include <algorithm>
#include <stack>

namespace bf {

	namespace optimizations {

		optimization_t get_opt_by_name(std::string_view const optimization_name) {
			using namespace std::string_view_literals;
			static std::unordered_map<std::string_view, optimization_t> const optimization_levels{
				{"op_folding"sv, op_folding},
				{"dead_code_elimination"sv, dead_code_elimination},
				{"const_propagation"sv, const_propagation}
			};

			if (optimization_levels.count(optimization_name) == 0)
				return none;
			return optimization_levels.at(optimization_name);
		}


		constexpr optimization_t operator|(optimization_t const a, optimization_t const b) {
			return static_cast<optimization_t>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
		}
		constexpr optimization_t& operator|=(optimization_t & a, optimization_t const b) {
			return a = a | b;
		}
		constexpr optimization_t operator&(optimization_t const a, optimization_t const b) {
			return static_cast<optimization_t>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
		}
		constexpr optimization_t& operator&=(optimization_t & a, optimization_t const b) {
			return a = a & b;
		}
		constexpr optimization_t operator^(optimization_t const a, optimization_t const b) {
			return static_cast<optimization_t>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
		}
		constexpr optimization_t& operator^=(optimization_t & a, optimization_t const b) {
			return a = a ^ b;
		}

	}



	namespace {

		/*Function performing optimizations on passed syntax_tree by folding multiple consecutive
		operations which do the same. New tree of optimized code is returned.
		Traverses the given source code instruction by instruction, if there are multiple identical instructions
		next to each other, folds them to one.*/
		syntax_tree fold_operations(syntax_tree const& old_tree) {
			std::cout << "Folding operations...\n";

			syntax_tree new_tree;

			int folds_performed = 0;

			//for loop itself does not advance the iterator, the old tree is traversed by while-lop inside
			for (auto iter = old_tree.begin(), end = old_tree.end(); iter != end;) {
				if (!iter->is_foldable()) {
					//if current instruction is jump, IO or breakpoint, nothing can be folded
					new_tree.add_instruction(*iter);
					++iter;
					continue;
				}
				auto const current = iter;
				//while instructions have same type, advance iterator
				iter = std::find_if(++iter, end, [&current](instruction const& i) {return i.type_ != current->type_; });
				int const distance = static_cast<int>(std::distance(current, iter)); //number of instruction to be folded
				if (distance > 1) {//if we are folding, print message
					std::cout << "Folding " << distance << ' ' << current->type_ << " instructions ("
						<< std::distance(old_tree.begin(), current) << '-' << std::distance(old_tree.begin(), iter) - 1
						<< ") => " << new_tree.size() << ".\n";
					++folds_performed;
				}
				//add instruction of same type and offset as the first instruction of block. Argument is the number of consecutive instructions
				new_tree.add_instruction(current->type_, distance, current->source_offset_);
			}
			std::cout << "Folding finished.\n";
			new_tree.relocate_jump_targets(); //make sure jumps in the new tree are correctly located
			return new_tree;
		}

		/*First we need to find out, how much code is executed independently on IO operations. This part will be executed by the local emulator.
		This function does exactly that - takes source code for the program and decides, which parts shall be executed at compile-time.*/
		syntax_tree prepare_speculative_execution_code(syntax_tree const& old_tree) {

			//the last instuction that could possibly be executed by the speculative execution
			auto const first_io_operation = std::find_if(old_tree.begin(), old_tree.end(), [](instruction const& i) { return i.is_io(); });
			if (first_io_operation == old_tree.end())
				return old_tree; //there is no IO in this program - it can be calculated at compile-time

			//traverse the code pushing opened loops on the stack until an IO operation is encountered
			std::stack<std::vector<instruction>::const_iterator> opened_loops;
			for (auto iter = old_tree.begin(); iter != first_io_operation; ++iter)
				switch (iter->type_) {
				case instruction_type::loop_begin:
					opened_loops.push(iter);
					break;
				case instruction_type::loop_end:
					assert(!opened_loops.empty());
					opened_loops.pop();
					break;
				}

			//iterator past the last guaranteed instruction
			auto const last_executable_instruction = opened_loops.empty() ? first_io_operation //if we are not in loop, we end at the IO
				: [&opened_loops]() { //otherwise find the outermost opened loop and take its opening brace
				for (int redundant_loops = opened_loops.size() - 1; redundant_loops; --redundant_loops)
					opened_loops.pop();
				return opened_loops.top();
			}();

			//construct tree with new source code and flash it into the local emulator
			syntax_tree speculative_execution_code;
			speculative_execution_code.reserve(std::distance(old_tree.begin(), last_executable_instruction));
			std::copy(old_tree.begin(), last_executable_instruction, std::back_inserter(speculative_execution_code));

			return speculative_execution_code;
		}

		syntax_tree precalculate_cell_values(execution::cpu_emulator & speculative_emulator, syntax_tree const& old_tree) {
			assert(speculative_emulator.has_program()); //sanity check that this function is not called from elsewhere
			assert(speculative_emulator.state() == execution::execution_state::not_started);

			speculative_emulator.suppress_stop_interrupt() = true;
			speculative_emulator.do_execute(); //execute the flashed code

			syntax_tree new_tree;
			new_tree.reserve(old_tree.size() + speculative_emulator.memory_size() * 2 + 1);

			using cell_t = execution::cpu_emulator::memory_cell_t;
			//save the state of emulator's memory in the form of load_const instructions
			for (cell_t * iter = static_cast<cell_t*>(speculative_emulator.memory_begin()), *end = static_cast<cell_t*>(speculative_emulator.memory_end());;) {
				cell_t *next = std::find_if(iter, end, [](cell_t cell) {return cell != 0; }); //find the next nonzero cell
				if (next == end) { //if we run out of cells, move the CPR to the desired position within the memory as if the code had been run normally
					new_tree.add_instruction(instruction_type::left,
						std::distance(static_cast<cell_t*>(speculative_emulator.memory_begin()), iter) - speculative_emulator.cell_pointer_offset(), 0);
					break;
				}
				//emit instructions to traverse emulator's memory and load constants
				new_tree.add_instruction(instruction_type::right, std::distance(iter, next) + 1, 0);
				new_tree.add_instruction(instruction_type::load_const, *next, 0);
				iter = std::next(next);
			}
			--new_tree.front().argument_; //decrese the number of cells the CPR moves due to the first instruction
			return new_tree;
		}


		/*Function performing optimizations on passed syntax_tree by propagting constants wherever possible.
		New tree of optimized code is returned. Traverses the given source code instruction by instruction,
		speculatively executes and if the engine finds out that some value is a constant, eliminates all additional operations.*/
		syntax_tree propagate_consts(syntax_tree const &old_tree) {
			std::cout << "Propagating const...\n";

			execution::cpu_emulator speculative_emulator; // we set up local emulator to preserve the global state
			speculative_emulator.flash_program(prepare_speculative_execution_code(old_tree));

			syntax_tree new_tree = precalculate_cell_values(speculative_emulator, old_tree);

			for (std::size_t i = speculative_emulator.program_counter(); i < old_tree.size(); ++i)
				new_tree.add_instruction(old_tree[i]); //copy the rest of executable code 
			new_tree.relocate_jump_targets();

			std::cout << "In summary execution of " << speculative_emulator.executed_instructions_counter() << " instruction"
				<< cli::print_plural(speculative_emulator.executed_instructions_counter())
				<< " will be prevented by the means of const propagation.\n";
			return new_tree;
		}

		syntax_tree eliminate_dead_code(syntax_tree const& old_tree) {
			std::cout << "Eliminating dead code...\n";
			//TODO implement

			std::cout << "Dead code elimination ended.\n";
			return old_tree;
		}
	}


	syntax_tree optimize(syntax_tree source_tree, opt_level_t const opt_level) {
		std::cout << "optimizing\n";

		if (opt_level & optimizations::op_folding) //folding of operations was requested
			source_tree = fold_operations(source_tree);

		if (opt_level & optimizations::dead_code_elimination)
			source_tree = eliminate_dead_code(source_tree);

		if (opt_level & optimizations::const_propagation)
			source_tree = propagate_consts(source_tree);


		//TODO opportunity to add other optimizations

		std::cout << "end of otimizations\n";
		return source_tree;
	}

	namespace {

		/*Function callback for the "optimize" cli command. Parses its arguments as optimization flags
		and then performs specified optimizations on the result of previous compilation.*/
		int optimize_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(2, std::numeric_limits<int>::max(), argv.size()); code)
				return code;

			if (!last_compilation::ready()) {
				std::cerr << "Cannot optimize, no program had been compiled.\n";
				return 6;
			}

			opt_level_t opt_level = optimizations::none;

			for (auto iter = std::next(argv.begin()), end = argv.end(); iter != end; ++iter)
				if (opt_level_t tmp = optimizations::get_opt_by_name(*iter); tmp == optimizations::none) {
					cli::print_command_error(cli::command_error::argument_not_recognized);
					return 4;
				}
				else
					opt_level |= tmp;

			syntax_tree & tree = last_compilation::syntax_tree();

			optimize(tree, opt_level).swap(tree);

			return 0;
		}

	}


	void optimizer_initialize() {
		ASSERT_CALLED_ONCE;
		cli::add_command("optimize", cli::command_category::optimization, "Optimizes compiled program's code.",
			"Usage: \"optimize\" optimizations...\n"
			"Performs specified optimizations on the saved program. Accepts unlimited number of arguments which specify all the\n"
			"optimizations that are to be performed, the order of which is irrelevant, as the optimizer chooses the optimal order of\n"
			"operations on its own. Resulting program is saved internally and ready to be flashed into the emulator's instruction memory\n"
			"using the \"flash\" command.\n\n"

			"Currently supported optimization flags:\n"
			"\top_folding         Folds multiple occurences of the same instruction in a row.\n"
			"\tconst_propagation  Precalculates values of cells if they are known at compile time, independent on the IO.\n"

			, &optimize_callback);

	}

}
#include "optimizer.h"
#include "cli.h"
#include "compiler.h"
#include "utils.h"
#include "emulator.h"

#include <iostream>
#include <algorithm>
#include <stack>


namespace bf::optimizations {

	std::optional<opt_level_t> get_opt_by_name(std::string_view const optimization_name) {
		using namespace std::string_view_literals;
		static std::unordered_map<std::string_view, opt_level_t> const optimization_levels{
			{"op_folding"sv, opt_level_t::op_folding},
			{"dead_code_elimination"sv, opt_level_t::dead_code_elimination},
			{"const_propagation"sv, opt_level_t::const_propagation},
			{"loop_analysis"sv, opt_level_t::loop_analysis}
		};

		if (optimization_levels.count(optimization_name) == 0) {
			std::cerr << "Unknown optimization " << optimization_name << '\n';
			return std::nullopt;
		}
		return optimization_levels.at(optimization_name);
	}


	namespace {

		namespace single_block_optimizations {

			/*Function performing optimizations on passed syntax_tree by folding multiple consecutive
			operations which do the same. New tree of optimized code is returned.
			Traverses the given source code instruction by instruction, if there are multiple identical instructions
			next to each other, folds them to one.*/
			void fold_operations(std::vector<std::unique_ptr<basic_block>>& basic_blocks) {
				std::cout << "Operation folding executed.\n";

				std::ptrdiff_t fold_counter = 0;//number of optimizations performed. Kept just as a fancy feedback for user

				for (std::unique_ptr<basic_block> const& block : basic_blocks) { //optimize each basic block separately

					auto constexpr is_foldable_lambda = [](instruction const& instruction) constexpr {return instruction.is_foldable(); };

					auto const end = block->ops_.end(); //Cache the end of instruction sequence
					auto iter = std::find_if(block->ops_.begin(), end, is_foldable_lambda); //Find the first foldable instruction
					if (iter == end) //If there is none, go to next block, there is nothing to optimize in this one.
						continue;
					//For the rest of this block's instruction sequence:
					for (; iter != end;) {  //Loop invariant: iter points to the head of a sequence of consecutive same foldable instructions.
						auto next_different = std::find_if(std::next(iter), end, //Find the closest instruction with different opcode (first one not folded)
							[this_op_code = iter->op_code_](instruction const& instruction) {return instruction.op_code_ != this_op_code; });
						iter->argument_ = std::distance(iter, next_different); //The new argument is number of folded instructions
						//Find the next foldable instruction to restore invariant. It is possibly already pointed to by next_different
						iter = std::find_if(next_different, end, is_foldable_lambda);
						++fold_counter; //increment counter of perfomed folds to keep track of how many optimizations we executed
					}
					/*Now first instructions of all sequences of consecutive foldable instructions have their argument_ set to the correct value.
					We can use std::unique, which keeps only the first instruction from a contiguous sequence of the same ones.
					We consider only the foldable instructions with the same opcode as equal, therefore multiple io or jumps in a row don't get removed.*/
					auto const new_end = std::unique(block->ops_.begin(), block->ops_.end(), [](instruction const& a, instruction const& b) -> bool {
						return a.op_code_ == b.op_code_ && a.is_foldable();
						});

					block->ops_.erase(new_end, block->ops_.end()); //And finally shrink the vector by deleting all unwanted instructions 
				}
				std::cout << "Folding finished, " << fold_counter << " fold" << utils::print_plural(fold_counter) << " performed.\n";
			}

		} //namespace bf::optimizations::`anonymous`::single_block_optimizations
		//TODO for now speculative execution had been postponed as it requires an extraordinary effort to perform
#if 0
		/*First we need to find out, how much code is executed independently on IO operations. This part will be executed by the local emulator.
		This function does exactly that - takes source code for the program and decides, which parts shall be executed at compile-time.*/
		void prepare_speculative_execution_code(std::vector<std::unique_ptr<basic_block>> & basic_blocks) {
			//TODO implement speculative execution


			//the last instuction that could possibly be executed by the speculative execution
			auto const first_io_operation = std::find_if(old_tree.begin(), old_tree.end(), [](instruction const& i) { return i.is_io(); });
			if (first_io_operation == old_tree.end())
				return old_tree; //there is no IO in this program - it can be calculated at compile-time

			//traverse the code pushing opened loops on the stack until an IO operation is encountered
			std::stack<std::vector<instruction>::const_iterator> opened_loops;
			for (auto iter = old_tree.cbegin(); iter != first_io_operation; ++iter)
				switch (iter->op_code_) {
				case op_code::jump:
					opened_loops.push(iter);
					break;
				case op_code::jump_not_zero:
					assert(!opened_loops.empty());
					opened_loops.pop();
					break;
				}

			//iterator past the last guaranteed instruction
			auto const last_executable_instruction = opened_loops.empty() ? first_io_operation //if we are not in loop, we end at the IO
				: [&opened_loops]() { //otherwise find the outermost opened loop and take its opening brace
				for (std::size_t redundant_loops = opened_loops.size() - 1; redundant_loops; --redundant_loops)
					opened_loops.pop();
				return opened_loops.top();
			}();

			//construct tree with new source code and flash it into the local emulator
			syntax_tree speculative_execution_code;
			speculative_execution_code.reserve(std::distance(old_tree.begin(), last_executable_instruction));
			std::copy(old_tree.begin(), last_executable_instruction, std::back_inserter(speculative_execution_code));

			return speculative_execution_code;
		}

		syntax_tree precalculate_cell_values(execution::cpu_emulator& speculative_emulator, syntax_tree const& old_tree) {
			assert(speculative_emulator.has_program()); //sanity check that this function is not called from elsewhere
			assert(speculative_emulator.state() == execution::execution_state::not_started);

			speculative_emulator.suppress_stop_interrupt() = true;
			speculative_emulator.do_execute(); //execute the flashed code

			syntax_tree new_tree;
			new_tree.reserve(old_tree.size() + speculative_emulator.memory_size() * 2 + 1);

			using cell_t = execution::cpu_emulator::memory_cell_t;
			//save the state of emulator's memory in the form of load_const instructions
			for (cell_t* iter = static_cast<cell_t*>(speculative_emulator.memory_begin()), *end = static_cast<cell_t*>(speculative_emulator.memory_end());;) {
				cell_t* next = std::find_if(iter, end, [](cell_t cell) {return cell != 0; }); //find the next nonzero cell
				if (next == end) { //if we run out of cells, move the CPR to the desired position within the memory as if the code had been run normally
					new_tree.add_instruction(op_code::right,
						std::distance(iter, static_cast<cell_t*>(speculative_emulator.memory_end()))
						+ speculative_emulator.cell_pointer_offset() + 1, 0);
					break;
				}
				//emit instructions to traverse emulator's memory and load constants
				new_tree.add_instruction(op_code::right, std::distance(iter, next) + 1, 0);
				new_tree.add_instruction(op_code::load_const, *next, 0);
				iter = std::next(next);
			}
			--new_tree.front().argument_; //decrese the number of cells the CPR moves due to the first instruction
			return new_tree;
		}


		/*Function performing optimizations on passed syntax_tree by propagting constants wherever possible.
		New tree of optimized code is returned. Traverses the given source code instruction by instruction,
		speculatively executes and if the engine finds out that some value is a constant, eliminates all additional operations.*/
		syntax_tree propagate_consts(syntax_tree const& old_tree) {
			std::cout << "Propagating const...\n";

			std::unique_ptr<execution::cpu_emulator> speculative_emulator = std::make_unique<execution::cpu_emulator>(); // we set up local emulator to preserve the global state

			speculative_emulator->flash_program(prepare_speculative_execution_code(old_tree));

			syntax_tree new_tree = precalculate_cell_values(*speculative_emulator, old_tree);

			for (std::size_t i = speculative_emulator->program_counter(); i < old_tree.size(); ++i)
				new_tree.add_instruction(old_tree[i]); //copy the rest of executable code 
			new_tree.relocate_jump_targets();

			std::cout << "In summary execution of " << speculative_emulator->executed_instructions_counter() << " instruction"
				<< utils::print_plural(speculative_emulator->executed_instructions_counter())
				<< " will be prevented by the means of const propagation.\n";
			return new_tree;
		}

		void eliminate_dead_code(std::vector<std::unique_ptr<basic_block>>& basic_blocks) {
			//TODO implement
			std::cout << "Eliminating dead code...\n";

			syntax_tree new_tree;
			int eliminated_instructions = 0;

			for (auto iter = old_tree.begin(), end = std::prev(old_tree.end()); iter != end; ++iter) {
				instruction const& current = *iter, next = *std::next(iter);

				switch (current.op_code_) {
				case op_code::inc:
					if (next.op_code_ == op_code::dec) {
					}
					break;
				case op_code::dec:
					if (next.op_code_ == op_code::inc) {

					}
					break;
				case op_code::left:
					if (next.op_code_ == op_code::right) {

					}
					break;
				case op_code::right:
					if (next.op_code_ == op_code::left) {

					}
					break;

				}
			}

			std::cout << "Dead code elimination ended, " << eliminated_instructions << " instruction" << utils::print_plural(eliminated_instructions) << " eliminated.\n";
			new_tree.relocate_jump_targets();
			return new_tree;
		}


		void loop_analysis(std::vector<std::unique_ptr<basic_block>>& program) {
			//TODO implement
		}
#endif

		/*Function callback for the "optimize" cli command. Parses its arguments as optimization flags
		and then performs specified optimizations on the result of previous compilation.*/
		int optimize_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(2, std::numeric_limits<int>::max(), argv))
				return code;

			if (!previous_compilation::ready()) {
				std::cerr << "Cannot optimize, no program had been compiled.\n";
				return 6;
			}

			std::unordered_set<opt_level_t> requested_optimizations;

			for (auto iter = std::next(argv.cbegin()), end = argv.cend(); iter != end; ++iter)
				if (auto const optimization = get_opt_by_name(*iter); optimization.has_value())
					requested_optimizations.insert(*optimization);
				else {
					cli::print_command_error(cli::command_error::argument_not_recognized);
					return 4;
				}

			if (!requested_optimizations.empty())
				perform_optimizations(previous_compilation::basic_blocks_mutable(), requested_optimizations);
			else
				std::cout << "No optimizations were performed.\n";

			return 0;
		}

	} //namespace bf::optimizations::`anonymous`

	void perform_optimizations(std::vector<std::unique_ptr<basic_block>>& program, std::unordered_set<opt_level_t> const& opt_level) {
		std::cout << "Optimizing engine initialized.\n";

		if (opt_level.count(opt_level_t::op_folding)) //folding of operations was requested
			single_block_optimizations::fold_operations(program);

#if 0
		if (opt_level & optimizations::loop_analysis)
			loop_analysis(program);

		if (opt_level & optimizations::dead_code_elimination)
			eliminate_dead_code(program);


		if (opt_level & optimizations::const_propagation)
			propagate_consts(program);

#endif

		//TODO opportunity to add other optimizations

		std::cout << "Optimizations ended.\n";
	}

	void initialize() {
		ASSERT_IS_CALLED_ONLY_ONCE;
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

} //namespace bf::optimizations

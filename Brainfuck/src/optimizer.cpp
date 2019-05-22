#include "optimizer.h"
#include "cli.h"
#include "compiler.h"
#include "emulator.h"
#include <iostream>

namespace bf {

	namespace optimizations {

		opt_level_t get_opt_by_name(std::string_view const optimization_name) {
			using namespace std::string_view_literals;
			static std::unordered_map<std::string_view, opt_level_t> const optimization_levels{
				{"op_folding"sv, op_folding}
			};
			assert(optimization_levels.count(optimization_name));
			return optimization_levels.at(optimization_name);
		}


		constexpr opt_level_t operator|(opt_level_t const a, opt_level_t const b) {
			return static_cast<opt_level_t>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
		}
		constexpr opt_level_t& operator|=(opt_level_t & a, opt_level_t const b) {
			return a = a | b;
		}
		constexpr opt_level_t operator&(opt_level_t const a, opt_level_t const b) {
			return static_cast<opt_level_t>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
		}
		constexpr opt_level_t& operator&=(opt_level_t & a, opt_level_t const b) {
			return a = a & b;
		}
		constexpr opt_level_t operator^(opt_level_t const a, opt_level_t const b) {
			return static_cast<opt_level_t>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
		}
		constexpr opt_level_t& operator^=(opt_level_t & a, opt_level_t const b) {
			return a = a ^ b;
		}

	}



	namespace {

		/*Function performing optimizations on passed syntax_tree by folding multiple consecutive
		operations which do the same. New tree of optimized code is returned.
		Traverses the given source code instruction by instruction, if there are multiple identical instructions
		next to each other, folds them to one.*/
		syntax_tree fold_operations(syntax_tree const old_tree) {
			std::cout << "Folding operations...\n";

			syntax_tree new_tree;

			int folds_performed = 0;

			//for loop itself does not advance the iterator, the old tree is traversed by while-lop inside
			for (auto iter = old_tree.begin(), end = old_tree.end(); iter != end;) {
				if (!iter->is_foldable()) {
					//if current instruction is jump, IO or breakpoint, nothing can be folded
					new_tree.add_instruction(iter->type_, 0, iter->source_offset_);
					++iter;
					continue;
				}
				auto const current = iter;
				while (++iter != end && iter->type_ == current->type_); //while instructions have same type, advance iterator 
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

		/*Function performing optimizations on passed syntax_tree by propagting constants wherever possible.
		New tree of optimized code is returned. Traverses the given source code instruction by instruction,
		speculatively executes and if the engine finds out that some value is a constant, eliminates all additional operations.*/
		syntax_tree propagate_consts(syntax_tree const old_tree) {
			std::cout << "Propagating const...\n";

			syntax_tree new_tree;
			int propagated_consts = 0;

			int const cell_count = execution::emulator.memory_size();
			std::vector<bool> field_consts(cell_count, true );

			execution::cpu_emulator emulator; //we set up local emulator to preserve the global state
			sizeof(emulator);







			std::cout << "Const propagation ended, propagated " << propagated_consts << " constant"
				<< cli::print_plural(propagated_consts) << ".\n";
			assert(false); //TODO RIP
			return {};
		}
	}


	syntax_tree optimize(syntax_tree source_tree, opt_level_t const opt_level) {
		std::cout << "optimizing\n";

		if (opt_level & optimizations::op_folding) //folding of operations was requested
			source_tree = fold_operations(source_tree);

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

			opt_level_t opt_level = opt_level_t::none;

			for (auto iter = std::next(argv.begin()), end = argv.end(); iter != end; ++iter)
				if (opt_level_t tmp = optimizations::get_opt_by_name(*iter); tmp == opt_level_t::none) {
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
			"\top_folding      Folds multiple occurences of the same instruction in a row.\n"

			, &optimize_callback);

	}

}
#include "opt/optimizer_pass.h"
#include "cli.h"
#include "utils.h"
#include "compiler.h"
#include "emulator.h"
#include <execution>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <queue>
#include <stack>
#include <set>
#include <memory>
#include <fstream>
#include <iomanip>
namespace bf::opt {

	void generate_dot_file(std::vector<basic_block*> const& blocks, std::string file_name) {
		assert_program_invariants(blocks);

		std::ofstream file(file_name);

		file << "digraph G {\n";
		for (basic_block* block : blocks) {

			if (block->is_orphaned())
				continue;

			if (block->empty())
				file << '\t' << block->label_ << " [shape=box, label=\"Block " << block->label_ << "\\nEMPTY\"];\n";
			else {

				file << '\t' << block->label_ << " [shape=box, label=\"Block " << block->label_ << ", length " << block->ops_.size() << ".\\n";
				for (instruction const& i : block->ops_)
					file << std::right << std::setw(6) << i.source_loc_ << ": " << std::left << std::setw(10) << i.op_code_
					<< std::setw(10) << (i.is_jump() ? block->jump_successor_->label_ : i.argument_) << "\\n";

				file << "\"];\n";

			}




			if (block->natural_successor_)
				file << '\t' << block->label_ << " -> " << block->natural_successor_->label_
				<< (block->is_cjump() ? "[color=red, label=\"F\"]" : "[style=dotted]") << ";\n";
			if (block->jump_successor_)
				file << '\t' << block->label_ << " -> " << block->jump_successor_->label_
				<< (block->is_cjump() ? "[color=green, label=\"T\"]" : "") << ";\n";

		}
		file << "}";
	}


	std::optional<opt_level_t> get_opt_by_name(std::string_view const optimization_name) {
		using namespace std::string_view_literals;
		static std::unordered_map<std::string_view, opt_level_t> const optimization_levels{

		};

		if (optimization_levels.count(optimization_name) == 0) {
			std::cerr << "Unknown optimization " << optimization_name << '\n';
			return std::nullopt;
		}
		return optimization_levels.at(optimization_name);
	}



	



	namespace {


		/*Function callback for the "optimize" cli command. Parses its arguments as optimization flags
		and then performs specified optimizations on the result of previous compilation.*/
		int optimize_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(2, std::numeric_limits<std::ptrdiff_t>::max(), argv))
				return code;

			if (!previous_compilation::ready()) {
				std::cerr << "Cannot optimize, no program had been compiled.\n";
				return 6;
			}

			std::set<opt_level_t> requested_optimizations;

			for (std::size_t i = 1; i < argv.size(); ++i)
				if (std::optional<opt_level_t> const optimization = get_opt_by_name(argv[i]); optimization.has_value())
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

	} //namespace bf::opt::`anonymous`

	void perform_optimizations(std::vector<std::unique_ptr<basic_block>>& program, std::set<opt_level_t> const& requested_optimizations) {

		if (requested_optimizations.empty())
			return;

		std::cout << "Optimizing engine initialized.\n";

		//First construct a new vector of raw pointers to prevent mixing them with std::unique_ptr. We do not take over any ownership.
		std::vector<basic_block*> block_ptrs;
		block_ptrs.reserve(program.size());
		std::transform(program.begin(), program.end(), std::back_inserter(block_ptrs), std::mem_fn(&std::unique_ptr<basic_block>::get));

		//if (requested_optimizations.count(opt_level_t::op_folding)) { // Folding of operations was requested
//		std::cout << "Operation folding initiated.\n";
//		int const fold_count = peephole::combine_adjacent_operations(optimized_blocks);
//		std::cout << "Folding finished, " << fold_count << " fold" << utils::print_plural(fold_count) << " performed.\n";
		//}

		//TODO opportunity to add other optimizations;

		std::cout << "Optimizations ended.\n";
	}

	//TODO add verbose mode to namespace ::bf::cli

	void initialize() {
		ASSERT_IS_CALLED_ONLY_ONCE;
		using namespace std::string_literals;
		cli::add_command("optimize", cli::command_category::optimization, "Optimizes compiled program's code.",
			"Usage: \"optimize\" optimizations...\n"
			"Performs specified optimizations on the saved program. Accepts unlimited number of arguments which specify all the\n"
			"optimizations that are to be performed, the order of which is irrelevant, as the optimizer chooses the optimal order of\n"
			"operations on its own. Resulting program is saved internally and ready to be flashed into the emulator's instruction memory\n"
			"using the \"flash\" command.\n\n"

			"Currently supported optimization flags:\n"
			"\top_folding         Folds multiple occurences of the same instruction in a row.\n"
			"\tconst_propagation  Precalculates values of cells if they are known at compile time, independent on the IO.\n"
			//TODO fix the help string

			, &optimize_callback);
	}

} //namespace bf::opt

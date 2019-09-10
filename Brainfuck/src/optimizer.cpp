#include "optimizer.h"
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
				file << '\t' << block->label_ << " [shape=box, label=\"Block " << block->label_ << ", EMPTY\"];\n";
			else {

				file << '\t' << block->label_ << " [shape=box,label=\"Block " << block->label_ << ", length " << block->ops_.size() << ".\\n";
				for (instruction const& i : block->ops_)
					file << std::right << std::setw(6) << i.source_offset_ << ": " << std::left << std::setw(10) << i.op_code_
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



	void assert_program_invariants(std::vector<basic_block*> const& program) {
		//Assert that there are no references to the to be deleted blocks
		for (basic_block* const block : program) {
			assert(block);
			assert(block->jump_successor_ == nullptr || (!block->jump_successor_->is_orphaned() && block->jump_successor_->predecessors_.count(block) == 1));
			assert(block->natural_successor_ == nullptr || (!block->natural_successor_->is_orphaned() && block->natural_successor_->predecessors_.count(block) == 1));
			assert(block->is_cjump() == block->is_pure_cjump());
			assert(std::none_of(block->predecessors_.begin(), block->predecessors_.end(), std::mem_fn(&basic_block::is_orphaned)));

			assert(block->jump_successor_ != block->natural_successor_ || block->jump_successor_ == nullptr);

			for (basic_block const* const predecessor : block->predecessors_) {
				assert(predecessor->has_successor(block));

			}

			for (auto successor : basic_block::successor_ptrs) {
				assert(block->*successor == nullptr || (block->*successor)->has_predecessor(block));
			}

			for ([[maybe_unused]] instruction const& inst : block->ops_) {
			}
		};

		assert(std::is_sorted(program.begin(), program.end(), basic_block::ptr_comparator{}));
	}

	/*Erases all blocks that have no predecessors. It is expected that said blocks are no longer referenced from any other blocks.*/
	std::ptrdiff_t erase_orphaned_blocks(std::vector<basic_block*>& program) {
		assert_program_invariants(program);

		auto const old_end = program.end();
		auto const new_end = std::remove_if(program.begin(), old_end, std::mem_fn(&basic_block::is_orphaned));

		program.erase(new_end, old_end);

		return std::distance(new_end, old_end);
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

		for (int i = 0; i < 10; ++i) {

			std::string round = std::to_string(i) + '.';

			generate_dot_file(block_ptrs, round + "1.dot");
			peephole::simplify_arithmetic<peephole::arithmetic_tag::value>(block_ptrs);
			peephole::simplify_arithmetic<peephole::arithmetic_tag::pointer>(block_ptrs);
			peephole::simplify_arithmetic<peephole::arithmetic_tag::both>(block_ptrs);


			generate_dot_file(block_ptrs, round + "2.dot");
			peephole::eliminate_clear_loops(block_ptrs);
			generate_dot_file(block_ptrs, round + "3.dot");
			peephole::propagate_local_const(block_ptrs);
			generate_dot_file(block_ptrs, round + "4.dot");
			global::eliminate_pure_uncond_jumps(block_ptrs);
			generate_dot_file(block_ptrs, round + "5.dot");
			peephole::eliminate_infinite_loops(block_ptrs);
			generate_dot_file(block_ptrs, round + "6.dot");
			global::optimize_cond_jump_destination(block_ptrs);
			generate_dot_file(block_ptrs, round + "7.dot");
			global::eliminate_single_entry_conditionals(block_ptrs);
			generate_dot_file(block_ptrs, round + "8.dot");
			global::delete_unreachable_blocks(block_ptrs);
			generate_dot_file(block_ptrs, round + "9.dot");
			global::merge_into_predecessor(block_ptrs);
			generate_dot_file(block_ptrs, round + "10.dot");



		}
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

#include "breakpoint.h"
#include "cli.h"
#include "emulator.h"
#include <cassert>
#include <iostream>
#include <locale>
#include <charconv>


namespace bf::breakpoints {

	breakpoint_manager bp_manager;

	void breakpoint_manager::clear_all() {
		assert(hit_breakpoints_.empty());
		tmp_breakpoints_.clear();
		all_breakpoints_.clear();
		breakpoint_locations_.clear();
		hit_breakpoints_.clear();
	}

	instruction const& breakpoint_manager::get_replaced_instruction(int address) const {
		assert(breakpoint_locations_.count(address));
		return breakpoint_locations_.at(address).replaced_instruction_;
	}

	int breakpoint_manager::get_unused_breakpoint_id() const {
		int result = -1; //starting at negative one
		auto iter = std::find_if(all_breakpoints_.begin(), all_breakpoints_.end(), [&result](auto pair) -> bool {
			auto const&[key, breakpoint_data] = pair; //traverse the map
			return key != ++result; //searching for a hole in the increasing array of indices. Return if there is some
			});

		return iter == all_breakpoints_.end() ? result + 1 : result; //if we've found no hole, increase the number by one
	}

	breakpoint * breakpoint_manager::do_set_breakpoint(int address) {
		if (!execution::emulator.has_program()) {
			std::cerr << "No program has been flashed to CPU's memory.\n";
			return nullptr;
		}
		if (address >= execution::emulator.instructions_size()) {
			std::cerr << "Breakpoint out of bounds. Valid range is 0 to " << execution::emulator.instructions_size() - 1 << " inclusive.\n";
			return nullptr;
		}
		// breakpoint_id of the new breakpoint. Smallest non-negative integer not identing a different breakpoint 
		int const new_index = get_unused_breakpoint_id();

		std::cout << "New breakpoint " << new_index << " created.\n";

		//creates new breakpoint struct and places it into the map of defined breakpoints
		breakpoint *const new_breakpoint = std::addressof(all_breakpoints_[new_index] = breakpoint{ new_index, address });
		//either existing location is returned or a new one is added if there is nothing at the specified address
		location& brk_location = breakpoint_locations_[address];
		brk_location.breakpoints_here_.insert(new_breakpoint); //insert pointer to this new breakpoint
		if (brk_location.breakpoints_here_.size() == 1) { //if that's the first breakpoint
			brk_location.replaced_instruction_ = execution::emulator.instructions_[address]; //save the original instruction
			execution::emulator.instructions_[address].type_ = instruction_type::breakpoint; //insert a breakpoint instruction to program code
		}
		return new_breakpoint;
	}

	int breakpoint_manager::set_breakpoint(int address) {
		return !do_set_breakpoint(address);
	}

	int breakpoint_manager::set_tmp_breakpoint(int address) {
		breakpoint * const bp = do_set_breakpoint(address);
		if (bp)
			tmp_breakpoints_.insert(bp);
		return !bp;
	}

	void breakpoint_manager::remove_breakpoint(breakpoint * const bp) {
		assert(all_breakpoints_.count(bp->id_));
		location & brk_location = breakpoint_locations_.at(bp->address_); //get the location
		brk_location.breakpoints_here_.erase(bp); //remove current breakpoint from this location
		if (brk_location.breakpoints_here_.empty()) { //if we erased the last one, we need to restore the original instruction
			execution::emulator.instructions_[bp->address_] = brk_location.replaced_instruction_;
			breakpoint_locations_.erase(bp->address_);
		}
		all_breakpoints_.erase(bp->id_); //finally erase the breakpoint's data itself
	}

	void breakpoint_manager::add_unknown_breakpoint(int address) {
		assert(breakpoint_locations_.count(address) == 0); //sanity check; this location should be empty
		std::cout << "Unknown breakpoint at address " << address << " has been hit.\n"
			"Would you like to add it to the collection of defined breakpoints? [Y/N]\n";
		char input_char;
		do {
			std::cin >> input_char;
			input_char = std::toupper(input_char, std::locale::classic());
		} while (input_char != 'Y' && input_char != 'N'); //prompt until the user types something meaningful
		if (input_char == 'N') {
			std::cout << "New breakpoint won't be defined, but execution stopped anyway.\n";
			return;
		}
		std::cout << "Creating a new breakpoint at address " << address << ".\n"
			"New breakpoint no. " << do_set_breakpoint(address)->id_ << " defined.\n";
		assert(breakpoint_locations_.at(address).breakpoints_here_.size() == 1); //sanity check; this location shall now contain just one breakpoint
	}

	void breakpoint_manager::handle_breakpoints(int address) {
		if (breakpoint_locations_.count(address) == 0) //if an unknown breakpoint had been hit, 
			return add_unknown_breakpoint(address); //initiate the defining procedure 

		for (breakpoint * hit_bp : hit_breakpoints_) {//traverse vector and print all breakpoints
			assert(hit_bp->address_ == address); //sanity check, we cannot hit breakpoints at multiple addresses simultaneously
			std::cout << "Breakpoint no. " << hit_bp->id_ << " at address " << hit_bp->address_ << " has been hit!\n";
			if (tmp_breakpoints_.count(hit_bp)) {
				tmp_breakpoints_.erase(hit_bp);
				remove_breakpoint(hit_bp);
			}
		}
		hit_breakpoints_.clear(); //remove processed breakpoints
	}

	bool breakpoint_manager::should_ignore_breakpoints(int address) {
		//we have encountered a breakpoint which has not been set via a command => programmatical breakpoint
		if (breakpoint_locations_.count(address) == 0) //let function handle_breakpoint take care of it
			return false;
		assert(hit_breakpoints_.empty()); //sanity check; all breakpoints had been processed before the new ones were reached

		auto & breaks_here = breakpoint_locations_.at(address).breakpoints_here_;
		/*Traverse all possibly hit breakpoints trying to hit them. If it's successful, add them to the vector of
		hit but unprocessed breakpoints.*/
		std::copy_if(breaks_here.begin(), breaks_here.end(), std::back_inserter(hit_breakpoints_)
			, [](breakpoint *const b) {return b->try_hit(); });
		return hit_breakpoints_.empty(); //if none has been hit, ignore
	}

	std::unordered_set<breakpoint*> const& breakpoint_manager::get_breakpoints(int address) {
		assert(execution::emulator.has_program());
		assert(address >= 0 && address < execution::emulator.instructions_size());
		assert(breakpoint_locations_.count(address)); //sanity check
		return breakpoint_locations_.at(address).breakpoints_here_;
	}

	int breakpoint_manager::breakpoint_count(int address) {
		assert(execution::emulator.has_program());
		assert(address >= 0 && address < execution::emulator.instructions_size());
		return breakpoint_locations_.count(address) ?
			static_cast<int>(breakpoint_locations_.at(address).breakpoints_here_.size())
			: 0;
	}

	breakpoint * breakpoint_manager::get_breakpoint(int id) {
		return all_breakpoints_.count(id) ? &all_breakpoints_.at(id) : nullptr;
	}


	bool breakpoint::try_hit() {
		assert(ignore_count_ >= 0); //sanity check
		if (!enabled_ || !condition_satisfied()) //if condition is not satisfied, return false straight away
			return false;
		//we have hit a breakpoint, but it may have ignore_count set; in such a case, decrease it 
		if (ignore_count_) {
			--ignore_count_;
			return false;
		}
		else
			return true; //return true if breakpoint shall no longer be ignored
	}

	namespace {
		/*Function callback for the break cli command.*/
		int break_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(2, 2, argv.size()); code)
				return code;
			int offset;
			if (std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), offset).ec != std::errc{}) {
				cli::print_command_error(cli::command_error::invalid_number_format);
				return 3;
			}
			if (offset < 0) {
				cli::print_command_error(cli::command_error::non_negative_number_expected);
				return 4;
			}

			//if parameters are ok, delegate the call to breakpoint_manager
			return bp_manager.set_breakpoint(offset);
		}

		/*Function callback for the tbreak cli command. Accepts one argument - the address at which temporary breakpoint shall be set*/
		int tbreak_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(2, 2, argv.size()))
				return code;

			int address;
			if (std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), address).ec != std::errc{}) {
				cli::print_command_error(cli::command_error::invalid_number_format);
				return 3;
			}
			if (address < 0) {
				cli::print_command_error(cli::command_error::non_negative_number_expected);
				return 4;
			}

			//if parameters are ok, delegate the call to breakpoint_manager
			return bp_manager.set_tmp_breakpoint(address);
		}

		int ignore_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(3, 3, argv.size()))
				return 0;

			int breakpoint_index, ignore_count;
			if (std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), breakpoint_index).ec != std::errc{}
				|| std::from_chars(argv[2].data(), argv[2].data() + argv[2].size(), ignore_count).ec != std::errc{}) {
				cli::print_command_error(cli::command_error::invalid_number_format);
				return 3;
			}
			if (breakpoint_index < 0 || ignore_count < 0) {
				cli::print_command_error(cli::command_error::non_negative_number_expected);
				return 4;
			}
			breakpoint * const bp = bp_manager.get_breakpoint(breakpoint_index);
			if (!bp) {
				std::cerr << "The specified breakpoint does not exist!\n";
				return 5;
			}
			bp->ignore_count_ = ignore_count;
			return 0;
		}

		/*Wrapper namespace for function used by disable and enable callbacks. */
		namespace disable_enable_helper {

			/*Parses the given string as breakpoint id, finds the breakpoint with mathing id and either enables or disables it.*/
			int modify_breakpoint_state(std::string_view const breakpoint_string, bool enabled) {
				int breakpoint_id;
				if (std::from_chars(breakpoint_string.data(), breakpoint_string.data() + breakpoint_string.size(), breakpoint_id).ec != std::errc{}) {
					cli::print_command_error(cli::command_error::invalid_number_format);
					return 4;
				}
				if (breakpoint_id < 0) {
					cli::print_command_error(cli::command_error::non_negative_number_expected);
					return 5;
				}
				struct breakpoint * breakpoint = bp_manager.get_breakpoint(breakpoint_id);
				std::cerr << "Breakpoint " << breakpoint_id;
				if (!breakpoint) {
					std::cerr << " does not exist.\n";
					return 6;
				}
				std::cerr << (enabled ? " enabled.\n" : " disabled.\n");
				breakpoint->enabled_ = enabled;
				return 0;
			}

		}

		/*Function callback for cli command disable. Parses its argument as breakpoint id and sets its state to disabled.*/
		int disable_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(2, 2, argv.size()))
				return code;

			return disable_enable_helper::modify_breakpoint_state(argv[1], false);
		}

		/*Function callback for cli command enable. Parses its argument as breakpoint id and sets its state to enabled.*/
		int enable_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(2, 2, argv.size()))
				return code;

			return disable_enable_helper::modify_breakpoint_state(argv[1], true);
		}

	}
	void initialize() {
		ASSERT_CALLED_ONCE;

		cli::add_command("break", cli::command_category::debug, "Creates a new breakpoint.",
			"Usage: \"break\" instruction_number\n"
			"Sets a new breakpoint at instruction residing at adress instruction_number."
			, &break_callback);
		cli::add_command_alias("breakpoint", "break");
		cli::add_command_alias("b", "break");
		cli::add_command_alias("br", "break");

		cli::add_command("tbreak", cli::command_category::debug, "Creates a temporary breakpoint.",
			"Usage: \"tbreak\" instruction_number\n"
			"Creates a new breakpoint at specified location which will be automatically destroyed after it is hit for the first time."
			, &tbreak_callback);

		cli::add_command("ignore", cli::command_category::debug, "Sets breakpoint's ignore count.",
			"Usage: \"ignore\" breakpoint_number ignore_count\n"
			"Set's the number of times the execution shall continue if the specified breakpoint is hit."
			, &ignore_callback);

		cli::add_command("disable", cli::command_category::debug, "Disables a breakpoint.",
			"Usage: \"disable\" breakpoint_number\n"
			"Disables the breakpoint with the same index as the specified parameter.\n"
			"Disabled breakpoints are ignored during execution."
			, &disable_callback);
		cli::add_command("enable", cli::command_category::debug, "Enables a breakpoint.",
			"Usage: \"enable\" breakpoint_num\n"
			"Enables the breakpoint with the same index as the specified parameter.\n"
			"Enabled breakpoints interrupt execution when hit.",
			&enable_callback);
	}
}
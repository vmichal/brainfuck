#include "breakpoint.h"
#include "utils.h"
#include "cli.h"
#include "emulator.h"
#include <cassert>
#include <sstream>
#include <iostream>
#include <charconv>
#include <iomanip>
#include <array>
#include <utility>

namespace bf::breakpoints {

	breakpoint_manager bp_manager;


	bool breakpoint::try_hit() {
		assert(ignore_count_ >= 0); //sanity check
		if (!enabled_ || !is_condition_satisfied()) //if condition is not satisfied, return false straight away
			return false;

		//we have hit a breakpoint, but it may have ignore_count set; in such a case, decrease it and ignore 
		if (ignore_count_) {
			--ignore_count_;
			return false;
		}
		return true; //return true in case the breakpoint shall no longer be ignored
	}

	void breakpoint_manager::clear_all() {
		assert(hit_breakpoints_.empty());
		temp_breakpoints_.clear();
		all_breakpoints_.clear();
		breakpoint_locations_.clear();
		hit_breakpoints_.clear();
	}


	int breakpoint_manager::get_unused_breakpoint_id() const {

		/*Map of breakpoints is sorted in ascending order. Find two consecutive elements, whose keys' difference
		is greater than one. If such two elements are found, they are separated by a gap - an unused ID is found.
		Iterator pointing to element with key one less than the unused id to be returned.*/
		auto const iter_before_gap = std::adjacent_find(all_breakpoints_.cbegin(), all_breakpoints_.cend(),
			[](std::pair<const int, breakpoint> const & this_pair, std::pair<const int, breakpoint> const& next_pair) -> bool {
				return this_pair.first + 1 != next_pair.first; //when this evaluates to true, search is terminated 
			});

		return iter_before_gap == all_breakpoints_.end()
			? all_breakpoints_.rbegin()->first + 1 //if we run out of elements, return last used key plus one
			: iter_before_gap->first + 1; //the next following element has key GREATER than this_iterator's key plus one
	}

	breakpoint* breakpoint_manager::do_set_breakpoint_at(int address) {
		if (!execution::emulator.has_program()) {
			std::cerr << "No program has been flashed to CPU's memory.\n";
			return nullptr;
		}
		if (address >= execution::emulator.instructions_size()) {
			std::cerr << "Breakpoint out of bounds. Valid range is [0, " << execution::emulator.instructions_size() - 1 << "] inclusive.\n";
			return nullptr;
		}

		//either an existing location is returned or a new one is created 
		location& bp_location = breakpoint_locations_[address];
		if (bp_location.breakpoints_here_.empty()) { //if that's the first breakpoint
			bp_location.replaced_instruction_ = execution::emulator.instructions_[address]; //save the original instruction
			execution::emulator.instructions_[address].op_code_ = op_code::breakpoint; //insert a breakpoint instruction to program code
		}

		// breakpoint_id of the new breakpoint. Smallest non-negative integer not yet denoting an existing breakpoint 
		int const new_index = get_unused_breakpoint_id();
		//creates new breakpoint struct and places it into the map of defined breakpoints
		breakpoint* const new_breakpoint_ptr = std::addressof(all_breakpoints_[new_index] = breakpoint{ new_index, address });
		bp_location.breakpoints_here_.insert(new_breakpoint_ptr); //insert pointer to the new breakpoint
		std::cout << "New breakpoint " << new_index << " created.\n";
		return new_breakpoint_ptr;
	}

	int breakpoint_manager::set_breakpoint_at(int address) {
		return do_set_breakpoint_at(address) == nullptr;
	}

	instruction const& breakpoint_manager::get_replaced_instruction_at(int const address) const {
		assert(breakpoint_locations_.count(address));
		return breakpoint_locations_.at(address).replaced_instruction_;
	}

	int breakpoint_manager::set_temp_breakpoint_at(int address) {
		breakpoint* const bp = do_set_breakpoint_at(address);
		if (bp)
			temp_breakpoints_.insert(bp);
		return !bp;
	}

	void breakpoint_manager::remove_breakpoint(breakpoint* const bp) {
		assert(all_breakpoints_.count(bp->id_)); //make sure the breakpoint exists at all
		assert(std::addressof(all_breakpoints_.at(bp->id_)) == bp); //make sure the argument bp points into the internal map
		assert(breakpoint_locations_.count(bp->address_)); //make sure breakpoint location exists
		assert(breakpoint_locations_.at(bp->address_).breakpoints_here_.count(bp)); //make sure specified location contains the bp
		assert(std::find(hit_breakpoints_.begin(), hit_breakpoints_.end(), bp) == hit_breakpoints_.end()); //make sure the breakpoint is no longer used

		location& brk_location = breakpoint_locations_.at(bp->address_); //get the breakpoint location
		brk_location.breakpoints_here_.erase(bp); //remove current breakpoint from this location

		if (brk_location.breakpoints_here_.empty()) { //if we erased the last one, we need to restore the original instruction
			execution::emulator.instructions_[bp->address_] = brk_location.replaced_instruction_;
			breakpoint_locations_.erase(bp->address_);
		}
		if (temp_breakpoints_.count(bp)) //if the breakpoint is temporary, 
			temp_breakpoints_.erase(bp); //delete it from the container as well

		all_breakpoints_.erase(bp->id_); //finally erase the breakpoint's data itself
	}

	void breakpoint_manager::handle_unknown_breakpoint_at(int address) {
		assert(breakpoint_locations_.count(address) == 0); //sanity check; this location must be empty

		std::cout << "Encountered an unknown breakpoint at address " << address << ".\n"
			"Would you like to add it to the collection of defined breakpoints?\n";
		if (!utils::prompt_user_yesno()) {
			std::cout << "New breakpoint won't be defined, but execution stopped anyway.\n";
			return;
		}
		std::cout << "Creating a new breakpoint at address " << address << ".\n"
			"New breakpoint no. " << do_set_breakpoint_at(address)->id_ << " defined.\n";
		assert(breakpoint_locations_.at(address).breakpoints_here_.size() == 1); //sanity check; this location shall now contain just one breakpoint
	}

	void breakpoint_manager::handle_breakpoints_at(int address) {
		if (breakpoint_locations_.count(address) == 0) //if an unknown breakpoint had been hit, 
			return handle_unknown_breakpoint_at(address); //initiate the defining procedure 

		for (breakpoint* hit_bp : hit_breakpoints_) {//traverse vector and print all breakpoints
			assert(hit_bp->address_ == address); //sanity check, we cannot hit breakpoints at multiple addresses simultaneously

			if (temp_breakpoints_.count(hit_bp)) {
				std::cout << "Temporary breakpoint no. " << hit_bp->id_ << " at address " << hit_bp->address_ << " has been hit!\n";
				remove_breakpoint(hit_bp); //hit_bp is now dangling!
			}
			else
				std::cout << "Breakpoint no. " << hit_bp->id_ << " at address " << hit_bp->address_ << " has been hit!\n";
		}
		hit_breakpoints_.clear(); //remove processed breakpoints
	}

	bool breakpoint_manager::should_ignore_breakpoints_at(int const address) {
		//If we encounter a breakpoint which has not been set via a command (i.e. programmatical breakpoint),
		if (breakpoint_locations_.count(address) == 0) //don't ignore. Let function handle_breakpoint take care of it
			return false;

		assert(execution::emulator.has_program()); //make sure there is some program..
		assert(hit_breakpoints_.empty()); //sanity check; all breakpoints had been processed before the new ones were reached
		assert(breakpoint_locations_.count(address)); //make sure the specified address has a corresponding breakpoint location
		assert(std::any_of(all_breakpoints_.begin(), all_breakpoints_.end(), //make sure at least one breakpoint resides at the specified address 
			[address](std::pair<int const, breakpoint> const& pair) -> bool { return pair.second.address_ == address; }));


		std::unordered_set<breakpoint*> const& breaks_here = get_breakpoints_at(address);
		/*Traverse all possibly hit breakpoints trying to hit them. If it's successful, add them to the vector of
		hit but unprocessed breakpoints.*/
		std::copy_if(breaks_here.begin(), breaks_here.end(), std::back_inserter(hit_breakpoints_),
			[](breakpoint* const b) {return b->try_hit(); });
		return hit_breakpoints_.empty(); //if none has been hit, ignore them
	}

	std::unordered_set<breakpoint*> const& breakpoint_manager::get_breakpoints_at(int address) {
		assert(execution::emulator.has_program());
		assert(address >= 0 && address < execution::emulator.instructions_size());
		assert(breakpoint_locations_.count(address)); //sanity check - this function assumes that the breakpoint location exists
		return breakpoint_locations_.at(address).breakpoints_here_;
	}

	int breakpoint_manager::count_breakpoints_at(int address) {
		assert(execution::emulator.has_program());
		assert(address >= 0 && address < execution::emulator.instructions_size());
		return breakpoint_locations_.count(address) ?
			static_cast<int>(breakpoint_locations_.at(address).breakpoints_here_.size())
			: 0;
	}

	breakpoint* breakpoint_manager::get_breakpoint(int const id) {
		return all_breakpoints_.count(id) ? &all_breakpoints_.at(id) : nullptr;
	}




	namespace {

		namespace break_helper {
			//Print a table containing information about breakpoints
			void print_breakpoint_info() {

				std::ostringstream buffer;

				static constexpr std::array<int, 4> const widths = { 6,12,10,8 }; //field widths

				buffer << "Defined breakpoints:\n" << std::right << std::setw(widths[0]) << "ID" << std::setw(widths[1]) << "ADDRESS"
					<< std::setw(widths[2]) << "ENABLED" << std::setw(widths[4]) << "IGNORE COUNT\n";

				for (auto const& [id, breakpoint] : bp_manager.all_breakpoints())
					buffer << std::setw(widths[0]) << std::right << breakpoint.id_ << '.' << std::setw(widths[1]) << breakpoint.address_
					<< std::setw(widths[2]) << (breakpoint.enabled_ ? "enabled" : "disabled") << std::setw(widths[3]) << breakpoint.ignore_count_;

				std::cout << buffer.str();
			}
		}

		/*Function callback for the break cli command. */
		int break_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(1, 2, argv)) //check the parameter count
				return code;

			if (argv.size() == 1) {
				break_helper::print_breakpoint_info();
				return 0;
			}

			std::optional<int> const parsed_address = utils::parse_nonnegative_argument(argv[1]);
			//if parameter is ok, delegate the call to breakpoint_manager, otherwise return an error code
			return parsed_address.has_value() ? bp_manager.set_breakpoint_at(*parsed_address) : 3;

		}

		/*Function callback for the tbreak cli command. Accepts one argument - the address at which temporary breakpoint shall be set*/
		int tbreak_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(2, 2, argv))
				return code;

			std::optional<int> const parsed_address = utils::parse_nonnegative_argument(argv[1]);

			//if parameters are ok, delegate the call to breakpoint_manager, otherwise return an error code
			return parsed_address.has_value() ? bp_manager.set_temp_breakpoint_at(*parsed_address) : 3;
		}

		int ignore_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(3, 3, argv))
				return code;

			std::optional<int> const breakpoint_id = utils::parse_nonnegative_argument(argv[1]),
				ignore_count = utils::parse_nonnegative_argument(argv[2]);
			if (!breakpoint_id.has_value() || !ignore_count.has_value())
				return 3;

			breakpoint* const bp = bp_manager.get_breakpoint(*breakpoint_id);
			if (!bp) {
				std::cerr << "The specified breakpoint does not exist!\n";
				return 5;
			}
			assert(bp->id_ == *breakpoint_id);
			bp->ignore_count_ = *ignore_count;
			std::cout << "Upcoming " << *ignore_count << utils::print_plural(*ignore_count, " hit ", " hits ")
				<< " of breakpoint " << *breakpoint_id << " will be ignored.\n";
			return 0;
		}

		/*Wrapper namespace for functions used by disable and enable callbacks. */
		namespace disable_enable_helper {

			/*Parses the given string as breakpoint id, finds the breakpoint with mathing id and either enables or disables it.*/
			int modify_breakpoint_state(std::string_view const bp_string, bool enabled) {
				std::optional<int> const breakpoint_id = utils::parse_nonnegative_argument(bp_string);
				if (!breakpoint_id.has_value()) //should the parsing fail, return error code
					return 3;

				//perform a lookup of the passed identifier
				struct breakpoint* breakpoint = bp_manager.get_breakpoint(*breakpoint_id);
				if (!breakpoint) {
					std::cerr << "Breakpoint " << *breakpoint_id << " does not exist.\n";
					return 6;
				}
				assert(*breakpoint_id == breakpoint->id_);
				breakpoint->enabled_ = enabled;
				std::cout << "Breakpoint " << *breakpoint_id << (enabled ? " enabled.\n" : " disabled.\n");
				return 0;
			}

		}

		/*Function callback for cli command disable. Parses its argument as breakpoint id and sets its state to disabled.*/
		int disable_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(2, 2, argv))
				return code;

			return disable_enable_helper::modify_breakpoint_state(argv[1], false);
		}

		/*Function callback for cli command enable. Parses its argument as breakpoint id and sets its state to enabled.*/
		int enable_callback(cli::command_parameters_t const& argv) {
			if (int const code = utils::check_command_argc(2, 2, argv))
				return code;

			return disable_enable_helper::modify_breakpoint_state(argv[1], true);
		}

	}
	void initialize() {
		ASSERT_CALLED_ONLY_ONCE;

		cli::add_command("break", cli::command_category::debug, "Creates a new breakpoint or lists existing ones.",
			"Usage: \"break\" [address]\n"
			"If no argument is specified, the command prints a list of all set breakpoints.\n"
			"If an integer address is specified, the command sets a new breakpoint at the given location."
			, &break_callback);
		cli::add_command_alias("breakpoint", "break");
		cli::add_command_alias("b", "break");
		cli::add_command_alias("br", "break");

		cli::add_command("tbreak", cli::command_category::debug, "Creates a temporary breakpoint.",
			"Usage: \"tbreak\" address\n"
			"Creates a new breakpoint at specified location which will be automatically destroyed after it is hit for the first time."
			, &tbreak_callback);

		cli::add_command("ignore", cli::command_category::debug, "Sets breakpoint's ignore count.",
			"Usage: \"ignore\" breakpoint_number ignore_count\n"
			"Sets the number of times the execution shall continue if the specified breakpoint is hit."
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
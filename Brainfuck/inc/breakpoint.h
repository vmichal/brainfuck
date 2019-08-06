#pragma once

#include "syntax_check.h"
#include "program_code.h"
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <map>
#include <vector>
#include <algorithm>

namespace bf::breakpoints {

	/*Structure representing a breakpoint in execution. It most importantly stores the address at which the corresponding breakpoint resides.*/
	struct breakpoint {
		int const id_; //unique id of this breakpoint; used to index into maps of breakpoints 
		int const address_; //address of the instruction at which the breakpoint resides.

		/*Number of times hitting this breakpoint shall be left unnoticed. If it reaches zero,
		next hit to this breakpoint shall be reported and execution shall be interrupted.*/
		int ignore_count_ = 0;

		/*If breakpoint is not enabled, it is automatically ignored. For simplicity it is not removed from the code,
		but since it's ignored, the replaced instruction is executed every time.*/
		bool enabled_ = true;

		//Function wrapper for breakpoint's condition. May contain a simple lambda determining whether breakpoint has been hit
		std::function<bool()> hit_predicate_;

	private:
		/*Checks whether breakpoint has satisfied its condition. Returns true iff the breakpoint is unconditional
		or the invocation of its condition predicate returns true.*/
		bool is_condition_satisfied() const {
			//TODO untill predicates are implemented, this always returns true
			//return hit_predicate_ ? hit_predicate_() : true;
			return true;
		}

	public:
		/*Tries to hit the breakpoint. If it's enabled, its condition is satisfied and ignore count is not zero, decrease it.
		If the ignore_count is zero, returns true and breakpoint handling in scheduled.*/
		bool try_hit();

		breakpoint() noexcept
			:id_(-1), address_(-1)
		{}

		breakpoint(int const id, int const address) noexcept 
			: id_(id), address_(address)
		{}

		breakpoint(breakpoint const& copy) = delete;
		breakpoint& operator=(breakpoint const& copy) = delete;

		breakpoint(breakpoint&& other) noexcept 
			: id_(other.id_), address_(other.address_), ignore_count_(other.ignore_count_),
			enabled_(other.enabled_), hit_predicate_(std::move(other.hit_predicate_))
		{}

		breakpoint& operator=(breakpoint&& other) {
			if (this != std::addressof(other))
				return *this;
			this->~breakpoint();
			new (this) breakpoint(std::move(other));
		}
	};


	/*Breakpoint location. Since there can be more breakpoints at one address, this structure
	stores a set of pointers as well as the replaced instruction.*/
	struct location {
		std::unordered_set<breakpoint*> breakpoints_here_; //pointers to breakpoints located at the corresponding address

		//stored instruction that had been replaced by breakpoints. It gets executed when breakpoints are ignored or the execution is resumed
		instruction replaced_instruction_;
	};

	class breakpoint_manager {
		std::map<int, breakpoint> all_breakpoints_; //map of all breakpoints with their id as key
		std::unordered_map<int, location> breakpoint_locations_; //map of breakpoint locations with their address as key
		std::unordered_set<breakpoint*> temp_breakpoints_; //set of temporary breakpoints that shall be erased after being hit

		//holds pointers to all breakpoints, which had been identified as reached in the should_ignore_breakpoints_at function
		std::vector<breakpoint*> hit_breakpoints_;

		/*Finds an unused integer as a new breakpoint id.*/
		int get_unused_breakpoint_id() const;

		/*Tries to set breakpoint at given address. After the creation of a new breakpoint, its address is checked.
		If some breakpoint had already been set before, the new one is added. Otherwise alteration of program's code
		is performed and the replaced instruction saved.
		Pointer to the new breakpoint is returned provided the operation ends successfully. Otherwise nullptr is returned.*/
		breakpoint* do_set_breakpoint_at(int address);

	public:

		//Returns a read only reference to the internal map of all existing breakpoints
		std::map<int, breakpoint> const& all_breakpoints() const { return all_breakpoints_; }

		/*Returns the saved instruction from address that has been replaced by a breakpoint. If address does not denote
		any location at which a breakpoint has been set, crashes painfully.*/
		instruction const& get_replaced_instruction_at(int const address) const;

		/*Delegates call to do_set_breakpoint_at, returns zero if operation ended successfully.*/
		int set_breakpoint_at(int const address);

		/*Sets a temporary breakpoint at the specified address. A normal breakpoint is created, which is then
		added to the set of temporary breakpoints which shall be deleted after being hit.
		Returns zero if the operation ended successfully.*/
		int set_temp_breakpoint_at(int const address);

		/*Removes the breakpoint from code. If the specified breakpoint does not exist, crashes painfully*/
		void remove_breakpoint(breakpoint* const bp);

		/*Prompts the user whether he wants to create a new breakpoint if a unknown breakpoint had been hit.
		If he chooses not to, this function does nothing. Otherwise calls set_breakpoint_at with current address.*/
		void handle_unknown_breakpoint_at(int const address);

		/*Handles all brekpoints that have been hit by the CPU. If there is none at the specified address, delegates
		call to handle_unknown_breakpoint_at as the breakpoint was most likely from the program itself. For known breakpoints
		a messages is printed and temporary breakpoints are deleted.*/
		void handle_breakpoints_at(int const address);

		/*Checks if specified breakpoint shall be ignored. If the specified breakpoint does not exist,
		a unknown breakpoint had been hit and it shall be examined properly in the handle_breakpoint function.
		If there are multiple breakpoints residing at the specified address, each of them is checked for its
		ignore count reaching zero - in such case the breakpoint shall not be ignored.
		Returns true if all breakpoints at address are known and have positive ignore count.*/
		bool should_ignore_breakpoints_at(int const address);

		/*Returns a reference to read-only set of pointers to BPs residing at specified address.
		This function assumes that there truly are breakpoints at the specified address. It fails painfully otherwise.*/
		std::unordered_set<breakpoint*> const& get_breakpoints_at(int const address);

		/*Returns an integer - the number of breakpoints at given address.*/
		int count_breakpoints_at(int address);

		/*Erases breakpoints from all internal containers.*/
		void clear_all();

		/*Returns a pointer to breakpoint with specified id or nullptr if such breakpoint does not exist.*/
		breakpoint* get_breakpoint(int id);

	};

	/*Single global instance of breakpoint_manager.*/
	extern breakpoint_manager bp_manager;

	/*Initialization function which shall be called just once by main. Creates CLI commands.*/
	void initialize();
}
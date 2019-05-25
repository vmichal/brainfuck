#include "emulator.h"
#include "cli.h"
#include "compiler.h"
#include <cassert>
#include <iostream>
#include <charconv>

namespace bf::execution {

	cpu_emulator emulator;

	void cpu_emulator::breakpoint_interrupt_handler() {
		flags_register_.breakpoint_hit() = true; //set flag to interrupt execution
		if (breakpoints::bp_manager.should_ignore_breakpoints(program_counter_)) {
			flags_register_.breakpoint_hit() = false; //if breakpoints shall be ignored, proceed and before that
			do_execute(breakpoints::bp_manager.get_replaced_instruction(program_counter_++)); //execute the replaced instruction 
		}
		else //otherwise break completely performing further operations requested by the breakpoints
			breakpoints::bp_manager.handle_breakpoints(program_counter_);
	}

	void cpu_emulator::flash_program(syntax_tree new_instructions) {
		instructions_ = std::move(new_instructions);
		breakpoints::bp_manager.clear_all();
	}

	flag_reference<flag::halt> cpu_emulator::halt() {
		state_ = execution_state::halted;
		return flags_register_.halt();
	}

	flag_reference<flag::single_step> cpu_emulator::single_step() {
		return flags_register_.single_step();
	}

	flag_reference<flag::os_interrupt> cpu_emulator::os_interrupt() {
		return flags_register_.os_interrupt();
	}

	flag_reference<flag::suppress_stop_interrupt> cpu_emulator::suppress_stop_interrupt() {
		return flags_register_.suppress_stop_interrupt();
	}

	void cpu_emulator::reset() {
		program_counter_ = 0;
		executed_instructions_counter_ = 0;
		memory_.reset();
		flags_register_.reset();
		cell_pointer_reg_ = memory_.data();
		state_ = execution_state::not_started;
		stdin_eof_ = false;
		assert(emulated_program_stdin_);
		assert(emulated_program_stdout_);
		if (emulated_program_stdin_ != &std::cin) {
			emulated_program_stdin_->clear(); //if a disk file is used as CPU's input, reset it
			emulated_program_stdin_->seekg(0);
		}
	}

	void cpu_emulator::left(int count) {
		assert(count > 0);
		cell_pointer_reg_ -= count;
		while (cell_pointer_reg_ < memory_begin())  //if the value exceeds memory's bounds
			cell_pointer_reg_ += memory_size();
	}

	void cpu_emulator::right(int count) {
		assert(count > 0);
		cell_pointer_reg_ += count;
		while (cell_pointer_reg_ >= memory_end())   //if the value exceeds memory's bounds
			cell_pointer_reg_ -= memory_size();
	}

	void cpu_emulator::do_execute(instruction const& instruction) {
		++executed_instructions_counter_;
		switch (instruction.type_) {
		case instruction_type::nop: //no-op
			break;
		case instruction_type::inc: //increase value of cell under the pointer
			*cell_pointer_reg_ += instruction.argument_;
			break;
		case instruction_type::dec: //decrease the value of cell under the pointer
			*cell_pointer_reg_ -= instruction.argument_;
			break;
		case instruction_type::left: //move the pointer to left
			left(instruction.argument_);
			break;
		case instruction_type::right: //move the pointer to right
			right(instruction.argument_);
			break;
		case instruction_type::loop_begin:
			if (*cell_pointer_reg_ == 0) //if value under the pointer is zero, perform jump to the instruction
				program_counter_ = instruction.argument_; //following the closing brace
			break;
		case instruction_type::loop_end: //check value under the pointer. If it's nonzero, jump to the start of this loop
			if (*cell_pointer_reg_)
				program_counter_ = instruction.argument_;
			//the next executed instruction is the one behind opening brace
			break;
		case instruction_type::in: //read char from stdin
			if (int const read = emulated_program_stdin_->get(); read == std::char_traits<char>::eof()) {
				std::cout << "\nEnd of input stream hit.\n";
				if (stdin_eof_)
					flags_register_.os_interrupt() = true;
				stdin_eof_ = true;
			}
			else
				*cell_pointer_reg_ = static_cast<memory_cell_t>(read);
			break;
		case instruction_type::out: //print char to stdout
			emulated_program_stdout_->put(static_cast<char>(*cell_pointer_reg_));
			break;
		case instruction_type::breakpoint: //pause the execution due to a breakpoint
			--executed_instructions_counter_;
			flags_register_.breakpoint_hit() = true;
			break;
		case instruction_type::load_const:
			*cell_pointer_reg_ = instruction.argument_;
			break;
		default: //die painfully
			--executed_instructions_counter_;
			std::cerr << "Unknown instruction " << instruction.type_ << ". Halting.\n";
			halt() = true;
		}

	}

	void cpu_emulator::execution_stops_callback() {
		emulated_program_stdout_->flush();
		execution_state new_state = execution_state::interrupted;
		if (program_counter_ == instructions_.size()) {
			std::cout << "\nExecution has finished.\n";
			new_state = execution_state::finished; //set state to finished. Memory may still be inspected, but PC is out of bounds
		}
		if (state_ != execution_state::halted)
			state_ = new_state;
		if (!flags_register_.suppress_stop_interrupt())
			cli::execute_command("stop", false);
	}

	void cpu_emulator::do_execute() {
		assert(has_program()); //may be removed later if I find a case in which it is undesirable to crash if no program is contained.
		assert(program_counter_ >= 0 && program_counter_ <= static_cast<int>(instructions_.size())); //Sanity check for PC not out of bounds
		assert(!flags_register_.halt());
		state_ = execution_state::running;
		flags_register_.os_interrupt() = false;
		if (flags_register_.breakpoint_hit()) {  //we continue after a breakpoint, PC is pointing to the BP's address. First execute the substituted instruction
			flags_register_.breakpoint_hit() = false;
			if (breakpoints::bp_manager.breakpoint_count(program_counter_))
				do_execute(breakpoints::bp_manager.get_replaced_instruction(program_counter_++));
			else
				do_execute(instructions_[program_counter_++]);
			if (flags_register_.single_step())
				return execution_stops_callback();
		}

		//the execution cannot proceed unless the halt flag is cleared 
		for (; !flags_register_.halt() && program_counter_ < static_cast<int>(instructions_.size());) {
			assert(program_counter_ >= 0);
			do_execute(instructions_[program_counter_++]); //increment PC immediatelly after instruction fetching to mimic real-life CPU 
			if (flags_register_.breakpoint_hit()) {
				--program_counter_; //execution hit a breakpoint, decrement the PC to make it store hit BP's address
				breakpoint_interrupt_handler(); //handle the interrupt request
				if (flags_register_.breakpoint_hit()) //if the request still prevails, interrupt the execution
					break;
			}
			assert(program_counter_ >= 0); //safety and sanity check
			if (flags_register_.os_interrupt()) {
				std::cout << "\nOperating system raised an interrupt signal!\n";
				break;
			}
			if (flags_register_.single_step() || flags_register_.halt())
				break;
		}
		execution_stops_callback();
	}
}


#ifndef EMULATOR_H
#define EMULATOR_H
#pragma once

#include "program_code.h"
#include "breakpoint.h"

#include <cstdint>
#include <ostream>
#include <iostream>
#include <array>

namespace bf::execution {

	/*All CPU flags' bitmasks.*/
	enum class flag : std::uint32_t {
		halt = 1UL << 0,
		single_step = 1UL << 1,
		breakpoint_hit = 1UL << 2,
		os_interrupt = 1UL << 3,
		suppress_stop_interrupt = 1UL << 4
	};

	/*A reference to a CPU flag. Allows writing and reading of a single flag determined by the template parameter*/
	template<flag FLAG>
	class flag_reference {
		static constexpr flag flag_ = FLAG;
		using value_type = std::underlying_type_t<flag>;
		static constexpr value_type bit_mask_ = static_cast<value_type>(flag_);
		static constexpr value_type not_mask_ = ~bit_mask_;

		value_type volatile& register_reference_;


	public:
		flag_reference(value_type volatile& val) noexcept : register_reference_{ val } {}
		flag_reference(flag_reference const& copy) noexcept = default;
		flag_reference(flag_reference&& move) noexcept = default;

		void flip() volatile {
			register_reference_ ^= bit_mask_;
		}

		void set() volatile {
			register_reference_ |= bit_mask_;
		}

		void clear() volatile {
			register_reference_ &= not_mask_;
		}

		[[nodiscard]]
		operator bool() volatile {
			return register_reference_ & bit_mask_;
		}

		flag_reference volatile& operator=(bool const new_val) volatile {
			if (new_val)
				set();
			else
				clear();
			return *this;
		}
	};

	/*CPU's FLAGS register. Stores its state as a bitfield and allows access through flag_references.*/
	class flags_register final {
		std::underlying_type_t<flag> value_ = 0;


	public:
		//default state is all flags zero
		flags_register() noexcept = default;
		explicit flags_register(std::underlying_type_t<flag> value) noexcept : value_{ value } {}


		[[nodiscard]]
		flag_reference<flag::halt> volatile halt() volatile {
			return value_;
		}

		[[nodiscard]]
		flag_reference<flag::single_step> volatile single_step() volatile {
			return value_;
		}

		[[nodiscard]]
		flag_reference<flag::breakpoint_hit> volatile breakpoint_hit() volatile {
			return value_;
		}

		[[nodiscard]]
		flag_reference<flag::os_interrupt> volatile os_interrupt() volatile {
			return value_;
		}

		[[nodiscard]]
		flag_reference<flag::suppress_stop_interrupt> volatile suppress_stop_interrupt() volatile {
			return value_;
		}

		void reset() volatile {
			value_ = 0;
		}
	};

	/*Simple enum containing the possible states of execution. cpu_emulator holds a sinsle instance as member, which
	may be queried to learn about the current state of execution.*/
	enum class execution_state {
		not_started, //No execution has started yet, the CPU has been recently reset.
		halted, //CPU had to halt due to an internal error. The halt flag is set
		finished, //Execution has finished. CPU has PC pointing just past the last instruction
		running, //The CPU is currently executing instructions
		interrupted //A program execution has been interrupted e.g by a breakpoint
	};


	class cpu_emulator {

		friend class breakpoints::breakpoint_manager;

	public: //underlying type for memory cells
		using memory_cell_t = unsigned char;

	private:

		std::vector<instruction> instructions_;
		std::ptrdiff_t program_counter_ = 0,
			executed_instructions_counter_ = 0;
		flags_register volatile flags_register_;
		std::array<memory_cell_t, 64> memory_ = { 0 };
		memory_cell_t* cell_pointer_reg_ = memory_.data();
		execution_state state_ = execution_state::not_started;

		std::istream* emulated_program_stdin_ = &std::cin;
		std::ostream* emulated_program_stdout_ = &std::cout;
		bool stdin_eof_ = false;

	public:
		[[nodiscard]]
		std::istream*& emulated_program_stdin() { return emulated_program_stdin_; }
		[[nodiscard]]
		std::ostream*& emulated_program_stdout() { return emulated_program_stdout_; }

	private:
		/*Handler executed if a breakpoint instruction is hit. First sets the breakpoint_hit flag to prevent further execution.
		Then consults the collection of defined breakpoints searching for breakpoints with matching address, determining whether
		they have or have not been hit with regards to their ignore count and condition. If breakpoints shall be ignored, clears the
		breakpoint_hit flag and proceeds with execution. Otherwise emulator is stopped and breakpoints get handled.*/
		void breakpoint_interrupt_handler();

		/*MicroOP code for the "right" instruction. Advances the CPR one cell to right and wraps it around the boundary of memory if such
		shift would overflow.*/
		void right(std::ptrdiff_t count);

		/*Executes a single specified instruction and returns.*/
		void do_execute(instruction const& instruction);

		/*Performs some state housekeeping when the execution is interrupted. Flushes output stream,
		prints message if the execution reached end of program and fires "stop" cli command if suppression is disabled.*/
		void execution_stops_callback();

	public:
		cpu_emulator() = default; //no need to call reset here as the default initialization is sufficient
		~cpu_emulator() = default;
		cpu_emulator(cpu_emulator const&) = delete;
		cpu_emulator(cpu_emulator&&) = delete;
		cpu_emulator& operator=(cpu_emulator const&) = delete;
		cpu_emulator& operator=(cpu_emulator&&) = delete;

		void flash_program(std::vector<instruction> new_instructions);

		/*Zeroes out memory, resets CPR and PC, in case input is redirected to a disk file, it's reset*/
		void reset();

		[[nodiscard]]
		execution_state state() const { return state_; }

		[[nodiscard]]
		flag_reference<flag::halt> halt();
		[[nodiscard]]
		flag_reference<flag::single_step> single_step();
		[[nodiscard]]
		flag_reference<flag::os_interrupt> os_interrupt();
		[[nodiscard]]
		flag_reference<flag::suppress_stop_interrupt> suppress_stop_interrupt();

		[[nodiscard]]
		bool has_program() const { return !instructions_.empty(); }

		void do_execute();


		[[nodiscard]]
		std::ptrdiff_t program_counter() { return program_counter_; }
		[[nodiscard]]
		std::ptrdiff_t executed_instructions_counter() { return executed_instructions_counter_; }

		[[nodiscard]]
		instruction* instructions_begin() { return instructions_.data(); }
		[[nodiscard]]
		instruction const* instructions_cbegin() const { return instructions_.data(); }

		[[nodiscard]]
		instruction* instructions_end() { return instructions_.data() + instructions_.size(); }
		[[nodiscard]]
		instruction const* instructions_cend() const { return instructions_.data() + instructions_.size(); }

		[[nodiscard]]
		ptrdiff_t instructions_size() { return static_cast<std::ptrdiff_t>(instructions_.size()); }




		[[nodiscard]]
		constexpr std::ptrdiff_t memory_size() const { return static_cast<std::ptrdiff_t>(memory_.size()); }

		//returns a pointer to the first cell in data memory. Must be untyped due to raw byte manipulations done by some commands
		[[nodiscard]]
		void* memory_begin() { return memory_.data(); }
		[[nodiscard]]
		void const* memory_cbegin() const { return memory_.data(); }
		[[nodiscard]]
		void const* memory_begin() const { return memory_.data(); }

		//returns an address of the first element located past the memory's boundaries. Must be untyped due to raw byte manipulations done by some commands
		[[nodiscard]]
		void* memory_end() { return memory_.data() + memory_.size(); }
		[[nodiscard]]
		void const* memory_cend() const { return memory_.data() + memory_.size(); }
		[[nodiscard]]
		void const* memory_end() const { return memory_.data() + memory_.size(); }

		//Returns the value of CPU's CPR. Must be untyped due to raw byte manipulations done by some commands
		[[nodiscard]]
		void* cell_pointer() { return cell_pointer_reg_; }
		[[nodiscard]]
		void const* cell_pointer() const { return cell_pointer_reg_; }
		//returns an offset of cpr from the memory's bounds
		[[nodiscard]]
		std::ptrdiff_t cell_pointer_offset() const { return std::distance(static_cast<unsigned char const*>(memory_begin()), static_cast<unsigned char const*>(cell_pointer())); }



	};

	//global CPU instance 
	extern cpu_emulator emulator;



	/*Shall be called only once from main. Initializes cli commands which control
	the function of internal cpu emulator.*/
	void initialize();
}

#endif
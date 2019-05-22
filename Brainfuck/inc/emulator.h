#pragma once

#include "syntax_tree.h"
#include "memory_state.h"
#include "breakpoint.h"

#include <cstdint>
#include <ostream>

namespace bf::execution {

	/*All CPU flags' bitmasks.*/
	enum class flag : std::uint32_t {
		halt = 1 << 0,
		single_step = 1 << 1,
		breakpoint_hit = 1 << 2,
		os_interrupt = 1 << 3,
		suppress_stop_notification = 1 << 4
	};

	/*A reference to CPU flag. Allows writing and reading of a single flag determined by the template parameter*/
	template<flag BIT_MASK>
	class flag_reference {
		static constexpr flag flag_ = BIT_MASK;
		static constexpr std::uint32_t bit_mask_ = static_cast<std::uint32_t>(BIT_MASK);
		static constexpr std::uint32_t not_mask_ = ~bit_mask_;

		std::uint32_t volatile& register_reference_;


	public:
		flag_reference(std::uint32_t volatile &val) : register_reference_(val) {}
		flag_reference(flag_reference const& copy) = default;
		flag_reference(flag_reference &&move) = default;

		void flip() volatile {
			register_reference_ ^= bit_mask_;
		}

		void set() volatile {
			register_reference_ |= bit_mask_;
		}

		void clear() volatile {
			register_reference_ &= not_mask_;
		}

		operator bool() volatile {
			return register_reference_ & bit_mask_;
		}

		flag_reference volatile& operator=(bool new_val) volatile {
			if (new_val)
				set();
			else
				clear();
			return *this;
		}
	};

	/*CPU's FLAGS register. Stores its state as a bitfield and allows access through flag_references.*/
	class flags_register {
		std::uint32_t value_ = 0;

	public:
		//default state is all flags zero
		flags_register() = default;
		flags_register(std::uint32_t value) : value_(value) {}


		flag_reference<flag::halt> volatile halt() volatile {
			return value_;
		}

		flag_reference<flag::single_step> volatile single_step() volatile {
			return value_;
		}

		flag_reference<flag::breakpoint_hit> volatile breakpoint_hit() volatile {
			return value_;
		}

		flag_reference<flag::os_interrupt> volatile os_interrupt() volatile {
			return value_;
		}

		flag_reference<flag::suppress_stop_notification> volatile suppress_stop_notification() volatile {
			return value_;
		}

		bool get(flag flag) volatile {
			return value_ & static_cast<std::uint32_t>(flag);
		}

		void flip(flag flag) volatile {
			value_ ^= static_cast<std::uint32_t>(flag);
		}

		void set(flag flag) volatile {
			value_ |= static_cast<std::uint32_t>(flag);
		}

		void clear(flag flag) volatile {
			value_ &= ~static_cast<std::uint32_t>(flag);
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

		syntax_tree instructions_;
		int program_counter_ = 0;
		flags_register volatile flags_register_;
		memory<64, memory_cell_t> memory_;
		memory_cell_t* cell_pointer_reg_ = memory_.data();
		execution_state state_ = execution_state::not_started;

		/*Handler executed if a breakpoint instruction is hit. First sets the breakpoint_hit flag to prevent further execution.
		Then consults the collection of defined breakpoints searching for breakpoints with matching address, determining whether
		they have or have not been hit with regards to their ignore count and condition. If breakpoints shall be ignored, clears the
		breakpoint_hit flag and proceeds with execution. Otherwise emulator is stopped and breakpoints get handled.*/
		void breakpoint_interrupt_handler();

		/*MicroOP code for "left". Moves CPR one cell to the left wrapping around the boundaries.*/
		void left(int count);
		/*MicroOP code for the "right" instruction. Advances the CPR one cell to right and wraps it around the boundary of memory if such
		shift would overflow.*/
		void right(int count);

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

		void flash_program(syntax_tree new_instructions);

		/*Zeroes out memory, resets CPR and PC, in case input is redirected to a disk file, it's reset*/
		void reset();

		execution_state state() const { return state_; }

		flag_reference<flag::halt> halt();
		flag_reference<flag::single_step> single_step();
		flag_reference<flag::os_interrupt> os_interrupt();
		flag_reference<flag::suppress_stop_notification> suppress_stop_notification();

		bool has_program() const { return instructions_.size(); }

		void do_execute();


		int program_counter() { return program_counter_; }
		instruction* instructions_begin() { return instructions_.data(); }
		instruction* instructions_end() { return instructions_.data() + instructions_.size(); }
		int instructions_size() { return instructions_.size(); }




		constexpr int memory_size() const { return memory_.size(); }
		//returns a pointer to the first cell in data memory. Must be untyped due to raw byte manipulations done by some commands
		void* memory_begin() const{ return memory_.data(); }
		//returns an address of the first element located past the memory's boundaries. Must be untyped due to raw byte manipulations done by some commands
		void* memory_end() const { return memory_.data() + memory_.size(); }
		//Returns the value of CPU's CPR. Must be untyped due to raw byte manipulations done by some commands
		void* cell_pointer() const { return cell_pointer_reg_; }
		//returns an offset of cpr from the memory's bounds
		int cell_pointer_offset() const { return static_cast<int>(std::distance(static_cast<unsigned char*>(memory_begin()), static_cast<unsigned char*>(cell_pointer()))); }



	};

	//pointer to stdin for CPU. Is either &std::cin or disk file if input is redirected
	extern std::istream* emulated_program_stdin;  
	//pointer to stdout for CPU. Is either &std::cout or disk file if output is redirected
	extern std::ostream* emulated_program_stdout; 
	//global CPU instance 
	extern cpu_emulator emulator;
	//Note: It is important to preserve this order of initialization, as streams have to be initialized before the CPU is constructed



	/*Shall be called only once from main. Initializes cli commands which control
	the function of internal cpu emulator.*/
	void initialize();
}
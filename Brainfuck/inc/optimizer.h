#pragma once

#include "program_code.h"

namespace bf {

	namespace optimizations {
		enum optimization_t : unsigned {
			none = 0x0,
			op_folding = 1 << 0,
			dead_code_elimination = 1 << 1,
			const_propagation = 1 << 2,
			loop_analysis = 1 << 3,

			all = 0xff'ff'ff'ff
		};

		/*Get optimization bitmask from its name. If optimization_name is not recognized, returns value none.*/
		optimization_t get_opt_by_name(std::string_view const optimization_name);

		/*Bitwise operations on optimization levels.*/
		constexpr optimization_t operator|(optimization_t const, optimization_t const);
		constexpr optimization_t& operator|=(optimization_t &, optimization_t const);
		constexpr optimization_t operator&(optimization_t const, optimization_t const);
		constexpr optimization_t& operator&=(optimization_t &, optimization_t const);
		constexpr optimization_t operator^(optimization_t const, optimization_t const);
		constexpr optimization_t& operator^=(optimization_t &, optimization_t const);


	};
	using opt_level_t = optimizations::optimization_t;

	/*Perform optimizations on given program.
	Syntax tree "program" undergoes optimizations specified by the bitfield "optimizations"
	and new syntax_tree is constructed.*/
	void perform_optimizations(std::vector<std::shared_ptr<basic_block>> const& program_basic_blocks, opt_level_t const optimizations);

	/*Initialize CLI commands that control the function of optimizer.
	Shall be called only once.*/
	void optimizer_initialize();
}
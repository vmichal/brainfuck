#pragma once

#include "syntax_tree.h"

namespace bf {

	namespace optimizations {
		enum opt_level_t : unsigned {
			none = 0x0,
			op_folding = 0x1,
			const_propagation = 0x2,

			all = 0xff'ff'ff'ff
		};

		/*Get optimization bitmask from its name. If optimization_name is not recognized, returns value none.*/
		opt_level_t get_opt_by_name(std::string_view const optimization_name);

		/*Bitwise operations on optimization levels.*/
		constexpr opt_level_t operator|(opt_level_t const, opt_level_t const);
		constexpr opt_level_t& operator|=(opt_level_t &, opt_level_t const);
		constexpr opt_level_t operator&(opt_level_t const, opt_level_t const);
		constexpr opt_level_t& operator&=(opt_level_t &, opt_level_t const);
		constexpr opt_level_t operator^(opt_level_t const, opt_level_t const);
		constexpr opt_level_t& operator^=(opt_level_t &, opt_level_t const);


	};
	using opt_level_t = optimizations::opt_level_t;

	/*Perform optimizations on given program.
	Syntax tree "program" undergoes optimizations specified by the bitfield "optimizations"
	and new syntax_tree is constructed.*/
	syntax_tree optimize(syntax_tree program, opt_level_t const optimizations);

	/*Initialize CLI commands that control the function of optimizer.
	Shall be called only once.*/
	void optimizer_initialize();
}
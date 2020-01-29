#pragma once

#include "IR/instruction.h"
#include "source_location.h"


#include <cassert>


namespace bf::IR {

	class unary_instruction : public instruction {

	protected:
		std::ptrdiff_t const argument_;

		unary_instruction(op_code op_code, source_location loc, std::ptrdiff_t arg)
			:instruction{ op_code, loc }, argument_{ arg } {
			assert(arg > 0);
		}

	public:
		std::ptrdiff_t argument() const { return argument_; }

	};


	class nop_instruction : public instruction {

	public:
		nop_instruction(source_location loc)
			:instruction{ op_code::nop, loc } {}
	};

	class arithmetic_instruction : public unary_instruction {
	public:

		constexpr bool is_inc() const { return op_code_ == op_code::inc; }
		constexpr bool is_dec() const { return op_code_ == op_code::dec; }
		constexpr bool is_right() const { return op_code_ == op_code::right; }
		constexpr bool is_left() const { return op_code_ == op_code::left; }


		std::ptrdiff_t canonical_argument() const { return is_inc() || is_right() ? argument_ : -argument_; }

		using unary_instruction::unary_instruction;
	};


	class basic_block;

	class branch_instruction : public instruction {

		union {
			basic_block* destination_;
			basic_block* if_then_ = nullptr;
		};
		basic_block* if_else_ = nullptr;

		void assert_ok() const {
			assert(op_code_ == op_code::branch || op_code_ == op_code::branch_nz);
			if (is_conditional())
				assert(if_then_ && if_else_ && "Both targets have to exist!");
			else
				assert(destination_ && if_else_ == nullptr && "Target must exist!");
		}

	public:

		branch_instruction(op_code op_code, source_location location, basic_block* dest)
			: instruction{ op_code, location }, destination_{ dest } {
			assert_ok();
		}

		branch_instruction(op_code op_code, source_location location, basic_block* ifthen, basic_block* ifelse)
			:instruction{ op_code, location }, if_then_{ ifthen }, if_else_{ ifelse } {
			assert_ok();
		}

		[[nodiscard]]
		constexpr bool is_conditional() const { return op_code_ == op_code::branch_nz; }

		[[nodiscard]]
		basic_block* destination() const {
			assert(!is_conditional());
			return destination_;
		}

		[[nodiscard]]
		constexpr bool has_destination(basic_block* dest) const {
			return if_then_ == dest || if_else_ == dest;
		}

		[[nodiscard]]
		constexpr bool is_true_branch(basic_block* dest) const {
			assert(is_conditional());
			return if_then_ == dest;
		}

		[[nodiscard]]
		constexpr bool is_false_branch(basic_block* dest) const {
			assert(is_conditional());
			return if_else_ == dest;
		}

	};

	class read_instruction : public instruction {

	public:

		read_instruction(source_location loc)
			:instruction{ op_code::read, loc } {}

	};

	class write_instruction : public instruction {

	public:
		write_instruction(source_location loc)
			:instruction{ op_code::write, loc } {}
	};


	class search_instruction : public unary_instruction {

	public:
		enum class direction {
			left, right
		};

		search_instruction(source_location loc, direction dir, std::ptrdiff_t stride)
			:unary_instruction{ dir == direction::left ? op_code::search_left : op_code::search_right, loc, stride } {}


		[[nodiscard]]
		constexpr bool is_left() const { return op_code_ == op_code::search_left; }

		[[nodiscard]]
		constexpr bool is_right() const { return op_code_ == op_code::search_right; }

		[[nodiscard]]
		constexpr std::ptrdiff_t stride() const { return argument_; }
	};


	class load_const_instruction : public unary_instruction {

	public:

		load_const_instruction(source_location location, std::ptrdiff_t arg)
			:unary_instruction{ op_code::load_const, location, arg } {}
	};

	class infinite_instruction : public unary_instruction {

	public:

		enum class when {
			zero, not_zero
		};

		infinite_instruction(source_location location, when when)
			:unary_instruction{ op_code::infinite, location, when == when::not_zero } {}

		[[nodiscard]]
		bool loops_on_nz() const { return argument_; }

		[[nodiscard]]
		bool loops_on_zero() const { return argument_ == 0; }

	};




}
#pragma once
#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "program_code.h"
#include "utils.h"

#include <vector>
#include <string_view>
#include <set>
#include <map>
#include <optional>

namespace bf::analysis {

	struct ptr_stationary_range {
		std::ptrdiff_t offset_;
		std::vector<instruction>::iterator begin_, end_;

		bool operator<(ptr_stationary_range const& rhs) const {
			return offset_ < rhs.offset_;
		}
	};

	class same_offset_iterator {
	public:
		struct bounds {
			std::vector<ptr_stationary_range>::iterator begin_, end_;
		};
	private:
		std::vector<instruction>::iterator current_instruction_;
		std::vector<ptr_stationary_range>::iterator current_range_;
		bounds const bounds_;

		enum class state { valid, too_low, too_far, no_range } state_ = state::valid;

		friend class ptr_movement_local_result;
	public:

		same_offset_iterator() : state_{state::no_range} {}

		same_offset_iterator(std::vector<instruction>::iterator inst, std::vector<ptr_stationary_range>::iterator range,
			bounds bounds)
			: current_instruction_{ inst }, current_range_{ range }, bounds_{ bounds } {
			if (current_range_ == bounds_.end_)
				state_ = state::no_range;
		}

		same_offset_iterator& operator++();
		same_offset_iterator& operator--();

		[[nodiscard]]
		instruction& operator*();

		[[nodiscard]]
		instruction* operator->();
		[[nodiscard]]
		operator bool() const;

	};


	class ptr_movement_local_result {

		std::vector<ptr_stationary_range> stationary_ranges_;
		std::ptrdiff_t final_ptr_offset_;
		bool ptr_moves_ = false;

		std::vector<ptr_stationary_range>::iterator get_range_iter(std::vector<instruction>::iterator inst) {
			for (auto iter = stationary_ranges_.begin(); iter != stationary_ranges_.end(); ++iter)
				if (iter->begin_ <= inst && inst < iter->end_)
					return iter;
			MUST_NOT_BE_REACHED;
		}

		same_offset_iterator::bounds iterator_bounds(std::ptrdiff_t const offset) {
			ptr_stationary_range const dummy{ offset, {}, {} };
			auto const lower = std::lower_bound(stationary_ranges_.begin(), stationary_ranges_.end(), dummy);
			auto const upper = std::upper_bound(stationary_ranges_.begin(), stationary_ranges_.end(), dummy);
			assert(lower <= upper);
			return { lower, upper };
		}

		friend ptr_movement_local_result analyze_pointer_movement_local(basic_block*);
	public:

		[[nodiscard]]
		std::ptrdiff_t final_offset() const { return final_ptr_offset_; }

		[[nodiscard]]
		bool ptr_moves() const { return ptr_moves_; }

		[[nodiscard]]
		same_offset_iterator offset_iterator(std::vector<instruction>::iterator inst) {
			auto const range = get_range_iter(inst);
			auto const bounds = iterator_bounds(range->offset_);


			return { inst, range, bounds };
		}

		[[nodiscard]]
		same_offset_iterator offset_iterator(std::ptrdiff_t const offset) {
			auto const bounds = iterator_bounds(offset);
			auto const& lower = bounds.begin_;

			if (bounds.begin_ == bounds.end_)
				return {};

			return { lower->begin_, lower, bounds };
		}
	};

	ptr_movement_local_result analyze_pointer_movement_local(basic_block* block);

	class block_evaluation_analyzer {

	public:
		enum class result_state {
			unknown,
			//indeterminate_different_entry,
			indeterminate_read,
			indeterminate_possible_overflow,

			known_not_zero,
			known_constant

		};

		void analyze_predecessors();
		void analyze_within_block();

	private:
		basic_block* const subject_;
		result_state state_ = result_state::unknown;
		std::ptrdiff_t entry_value_ = 0xdead'beef;
		std::ptrdiff_t result_ = 0xdead'beef;
		std::ptrdiff_t value_delta_ = 0;
		bool has_sideeffect_ = false;

		ptr_movement_local_result ptr_movement_;
	public:
		explicit block_evaluation_analyzer(basic_block* block);

		[[nodiscard]]
		bool has_const_result() const { return state_ == result_state::known_constant; }
		[[nodiscard]]
		std::ptrdiff_t result() const { return result_; }

		[[nodiscard]]
		bool has_non_zero_result() const {
			return (has_const_result() && result_ != 0) || state_ == result_state::known_not_zero;
		}

		[[nodiscard]]
		bool has_indeterminate_value() const { return !has_const_result() && !has_non_zero_result(); }

		[[nodiscard]]
		bool has_sideeffects() const { return has_sideeffect_ ||ptr_movement_.ptr_moves(); }
		[[nodiscard]]
		std::ptrdiff_t value_delta() const {
			assert(!has_const_result() && !has_sideeffects());
			return value_delta_;
		}
	};


	block_evaluation_analyzer analyze_block_evaluation(basic_block const* block);

	std::map<std::ptrdiff_t, bool> analyze_block_lives(std::vector<basic_block*> const& program);

	class incoming_value_analyzer {

		basic_block* const subject_;
		bool zero_seen_ = false;
		bool non_zero_seen_ = false;

		void analyze_predecessor(basic_block* pred);

	public:
		explicit incoming_value_analyzer(basic_block* block);

		[[nodiscard]]
		bool all_non_zero() const;

		[[nodiscard]]
		bool all_zero() const;
	};

}

#endif //ANALYSIS_H
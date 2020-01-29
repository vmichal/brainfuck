#pragma once

namespace bf {

	struct source_location {

		int line_;
		int column_;

		int line() const { return line_; }
		int column() const { return column_; }

	};

	/*Relational operator for source locations. If the lines are same, the smaller location is identified by lower column.
	Otherwise orderes errors by line numbers. */
	[[nodiscard]]
	bool operator<(source_location const& lhs, source_location const& rhs) noexcept {
		return lhs.line_ != rhs.line_ ? lhs.line_ < rhs.line_ : lhs.column_ < rhs.column_;
	}
}
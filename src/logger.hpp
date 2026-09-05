#pragma once

#include <string>
#include <variant>
#include <vector>

namespace dip {

struct log_error { std::pmr::string v; };
struct log_info  { std::pmr::string v; };
struct log_warn  { std::pmr::string v; };

using log_item = std::variant<log_error, log_info, log_warn>;

struct logger {
	auto clear() -> void {
		items_.clear();
	}
	auto push(log_error v) -> void {
		items_.push_back(std::move(v));
	}
	auto push(log_info v) -> void {
		items_.push_back(std::move(v));
	}
	auto push(log_warn v) -> void {
		items_.push_back(std::move(v));
	}
	auto info(std::pmr::string v) -> void {
		push(log_info{std::move(v)});
	}
	auto error(std::pmr::string v) -> void {
		push(log_error{std::move(v)});
	}
	auto warn(std::pmr::string v) -> void {
		push(log_warn{std::move(v)});
	}
	auto visit(auto fn) -> void {
		for (const auto& item : items_) {
			fn(item);
		}
	}
	static auto visit(log_error v, auto fn_error, auto /*fn_info*/, auto /*fn_warn*/) -> void { fn_error(v.v); }
	static auto visit(log_info v, auto /*fn_error*/, auto fn_info, auto /*fn_warn*/) -> void  { fn_info(v.v); }
	static auto visit(log_warn v, auto /*fn_error*/, auto /*fn_info*/, auto fn_warn) -> void  { fn_warn(v.v); }
	auto visit(auto fn_error, auto fn_info, auto fn_warn) -> void {
		auto visitor = [fn_error, fn_info, fn_warn](const auto& item) {
			visit(item, fn_error, fn_info, fn_warn);
		};
		for (const auto& item : items_) {
			std::visit(visitor, item);
		}
	}
	std::vector<log_item> items_;
};

} // dip

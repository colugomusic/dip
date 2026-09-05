#pragma once

#include <string>
#include <variant>
#include <vector>

namespace dip {

struct log_dep_task { std::pmr::string dep; std::pmr::string task; };
struct log_error    { std::pmr::string v; };
struct log_info     { std::pmr::string v; };
struct log_warn     { std::pmr::string v; };

using log_item = std::variant<log_dep_task, log_error, log_info, log_warn>;

template <
	typename dep_task_fn,
	typename error_fn,
	typename info_fn,
	typename warn_fn
>
struct logger_fns {
	dep_task_fn dep_task;
	error_fn error;
	info_fn info;
	warn_fn warn;
};

struct logger {
	auto clear() -> void {
		items_.clear();
	}
	auto push(log_dep_task v) -> void {
		items_.push_back(std::move(v));
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
	auto dep_task(std::pmr::string dep, std::pmr::string task) -> void {
		push(log_dep_task{std::move(dep), std::move(task)});
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
	static auto visit(log_dep_task v, auto fns) -> void { fns.dep_task(v.dep, v.task); }
	static auto visit(log_error v, auto fns) -> void    { fns.error(v.v); }
	static auto visit(log_info v, auto fns) -> void     { fns.info(v.v); }
	static auto visit(log_warn v, auto fns) -> void     { fns.warn(v.v); }
	auto visit(auto fns) -> void {
		auto visitor = [fns](const auto& item) {
			visit(item, fns);
		};
		for (const auto& item : items_) {
			std::visit(visitor, item);
		}
	}
	std::vector<log_item> items_;
};

} // dip

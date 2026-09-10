#pragma once

#include <string>
#include <variant>
#include <vector>

namespace dip {

struct log_debug        { std::pmr::string v; };
struct log_dep_task     { std::pmr::string dep; std::pmr::string task; };
struct log_dep_cfg_task { std::pmr::string dep; std::pmr::string cfg; std::pmr::string task; };
struct log_detail       { std::pmr::string v; };
struct log_error        { std::pmr::string v; };
struct log_info         { std::pmr::string v; };
struct log_warn         { std::pmr::string v; };

using log_item = std::variant<
	log_debug,
	log_dep_task,
	log_dep_cfg_task,
	log_detail,
	log_error,
	log_info,
	log_warn
>;

template <
	typename debug_fn,
	typename dep_task_fn,
	typename dep_cfg_task_fn,
	typename detail_fn,
	typename error_fn,
	typename info_fn,
	typename warn_fn
>
struct logger_fns {
	debug_fn debug;
	dep_task_fn dep_task;
	dep_cfg_task_fn dep_cfg_task;
	detail_fn detail;
	error_fn error;
	info_fn info;
	warn_fn warn;
};

struct logger {
	auto clear() -> void {
		items_.clear();
	}
	auto push(log_debug v) -> void {
		items_.push_back(std::move(v));
	}
	auto push(log_dep_task v) -> void {
		items_.push_back(std::move(v));
	}
	auto push(log_dep_cfg_task v) -> void {
		items_.push_back(std::move(v));
	}
	auto push(log_detail v) -> void {
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
	auto debug(std::pmr::string v) -> void {
		push(log_debug{std::move(v)});
	}
	auto dep_task(std::pmr::string dep, std::pmr::string task) -> void {
		push(log_dep_task{std::move(dep), std::move(task)});
	}
	auto dep_cfg_task(std::pmr::string dep, std::pmr::string cfg, std::pmr::string task) -> void {
		push(log_dep_cfg_task{std::move(dep), std::move(cfg), std::move(task)});
	}
	auto detail(std::pmr::string v) -> void {
		push(log_detail{std::move(v)});
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
	static auto visit(log_debug v, auto fns) -> void        { fns.debug(v); }
	static auto visit(log_dep_task v, auto fns) -> void     { fns.dep_task(v); }
	static auto visit(log_dep_cfg_task v, auto fns) -> void { fns.dep_cfg_task(v); }
	static auto visit(log_detail v, auto fns) -> void       { fns.detail(v); }
	static auto visit(log_error v, auto fns) -> void        { fns.error(v); }
	static auto visit(log_info v, auto fns) -> void         { fns.info(v); }
	static auto visit(log_warn v, auto fns) -> void         { fns.warn(v); }
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

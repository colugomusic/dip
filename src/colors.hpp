#pragma once

#include <format>

namespace dip::colors {

static const auto bg_black         = "\033[40m";
static const auto bg_red           = "\033[41m";
static const auto bg_green         = "\033[42m";
static const auto bg_yellow        = "\033[43m";
static const auto bg_blue          = "\033[44m";
static const auto bg_magenta       = "\033[45m";
static const auto bg_cyan          = "\033[46m";
static const auto bg_white         = "\033[47m";
static const auto bg_reset         = "\033[49m";
static const auto bg_gray          = "\033[100m";
static const auto fg_black         = "\033[30m";
static const auto fg_red           = "\033[31m";
static const auto fg_green         = "\033[32m";
static const auto fg_yellow        = "\033[33m";
static const auto fg_blue          = "\033[34m";
static const auto fg_magenta       = "\033[35m";
static const auto fg_cyan          = "\033[36m";
static const auto fg_white         = "\033[37m";
static const auto fg_reset         = "\033[39m";
static const auto fg_gray          = "\033[90m";
static const auto fg_light_red     = "\033[91m";
static const auto fg_light_green   = "\033[92m";
static const auto fg_light_blue    = "\033[94m";
static const auto fg_light_magenta = "\033[95m";
static const auto fg_light_cyan    = "\033[96m";
static const auto fg_light_gray    = "\033[97m";
static const auto reset            = "\033[0m";
static const auto dep              = std::format("{}{}", bg_reset, fg_green);
static const auto detail           = std::format("{}{}", bg_reset, fg_gray);
static const auto error            = std::format("{}{}", bg_red, fg_reset);
static const auto info             = "";
static const auto success          = std::format("{}{}", bg_green, fg_reset);
static const auto warning          = std::format("{}{}", bg_yellow, fg_black);

} // dip::colors

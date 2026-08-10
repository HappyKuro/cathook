#pragma once

#include "config.hpp"

namespace mono
{
struct color final
{
	float r{};
	float g{};
	float b{};
	float a{ 1.0f };
};

void apply_layout();
void apply_colors(color accent);
}

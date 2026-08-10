#pragma once

#include "theme.hpp"

#include <functional>

namespace mono
{
struct backend final
{
	std::function<bool()> initialize{};
	std::function<void()> shutdown{};
	std::function<void()> new_frame{};
	std::function<void(ImDrawData *)> render{};
};

class runtime final
{
public:
	runtime() = default;
	~runtime();

	runtime(const runtime &) = delete;
	runtime &operator=(const runtime &) = delete;

	bool initialize(backend value, const std::function<bool(ImGuiIO &)> &load_fonts = {});
	void shutdown();
	void abandon();
	void begin_frame(color accent);
	void end_frame();
	bool initialized() const { return m_initialized; }

private:
	backend m_backend{};
	bool m_initialized{};
};
}

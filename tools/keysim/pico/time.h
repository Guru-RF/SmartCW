#pragma once
#include <cstdint>
struct repeating_timer_t { int dummy; };
typedef bool (*repeating_timer_callback_t)(repeating_timer_t *);
inline bool add_repeating_timer_us(int64_t, repeating_timer_callback_t, void *, repeating_timer_t *) { return true; }

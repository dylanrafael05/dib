#pragma once

#define FORWARD(x) static_cast<decltype(x)>(x)
#define MOVE(x) static_cast<::std::remove_reference_t<decltype(x)> &&>(x)
// Host-side stub of the Arduino core, minimal surface used by mic.cpp
#ifndef ARDUINO_STUB_H
#define ARDUINO_STUB_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using std::size_t;

extern uint32_t g_fake_millis;
inline uint32_t millis() { return g_fake_millis; }

inline void *ps_malloc(size_t n) { return malloc(n); }

struct SerialStub {
    void println(const char *s) { printf("%s\n", s); }
    template <typename... Args> void printf(const char *fmt, Args... args) { std::printf(fmt, args...); }
};
extern SerialStub Serial;

#endif

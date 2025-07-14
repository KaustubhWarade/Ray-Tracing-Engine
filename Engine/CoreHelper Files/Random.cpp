#include "Random.h"

thread_local std::mt19937 Random::s_RandomEngine;
std::uniform_int_distribution<uint32_t> Random::s_Distribution;
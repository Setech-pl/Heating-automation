#pragma once
// Deterministic clock for the existing response code, no wall-clock access.
inline int now() { return 123456789; }

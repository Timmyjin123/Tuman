#pragma once
#include "Common.h"
namespace tuman {
struct AlgShimmer { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
struct AlgGrainVerb { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
}

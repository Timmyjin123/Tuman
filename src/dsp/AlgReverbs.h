#pragma once
#include "Common.h"
namespace tuman {
struct AlgLush { void background(Ctx&) {} void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
struct AlgInfinity { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
struct AlgGenerative { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
}

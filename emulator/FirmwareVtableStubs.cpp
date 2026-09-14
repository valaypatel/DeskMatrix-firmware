// emulator/FirmwareVtableStubs.cpp
//
// Supplies out-of-line definitions for base-class virtual methods that
// firmware/DeskMatrix declares but never defines at the base level --
// only derived classes (Block, Mario, Ghost, Pacman, DateI18nEN, ...)
// override them, and every real call site dispatches through one of
// those concrete overrides (confirmed: zero call sites anywhere in
// firmware go through a Sprite*/IDateI18n* base pointer/reference -- this
// is genuinely dead code today, in both builds).
//
// Both GCC/xtensa-elf (the real ESP32 Arduino toolchain) and Clang
// implement the *same* Itanium C++ ABI key-function rule here -- it is
// NOT a GCC-vs-Clang difference. What actually differs between the two
// builds:
//   1. Optimization level: the ESP32 Arduino build compiles at a nonzero
//      -O level, so GCC optimizes away the base-subobject vptr-store
//      that would otherwise reference Sprite's/IDateI18n's vtable during
//      their (never-really-exercised-as-base) constructors -- the
//      reference simply doesn't survive to link time. This emulator's
//      CMakeLists.txt sets no CMAKE_BUILD_TYPE (defaults to -O0), so the
//      vptr-store reference to the undefined vtable survives unoptimized
//      and becomes a real "undefined reference to vtable for X" at link.
//   2. RTTI: the ESP32 Arduino build compiles with -fno-rtti, which
//      eliminates typeinfo generation/references entirely, so a missing
//      "typeinfo for X" symbol never comes up there. This emulator
//      build has no -fno-rtti flag, so it needs real typeinfo symbols
//      for these classes too -- another source of the same class of
//      link error, unrelated to the vtable/key-function issue above.
// Both are latent, harmless-today gaps in firmware/DeskMatrix (Sprite::
// name() and IDateI18n's three methods should arguably be pure virtual
// `= 0` there, which would make the "never defined at the base" state
// intentional and compiler-enforced instead of accidental -- worth a
// follow-up firmware fix, but out of scope for this emulator task).
//
// These stubs are never actually invoked (no code constructs a bare
// Sprite or IDateI18n, or calls these methods before the derived class's
// constructor has run and installed its own vtable), so their bodies
// are deliberately inert. This file lives entirely in emulator/ and
// does not modify any firmware/ source -- see firmware/DeskMatrix's
// lib/cw-gfx-engine/Sprite.h (`virtual const char* name();`) and
// screens/clockfaces/words/IDateI18n.h (`virtual const char*
// formatDate(...)`, `weekDayName(...)`, `virtual void timeInWords(...)`)
// for the declarations this satisfies.
#include "lib/cw-gfx-engine/Sprite.h"
#include "screens/clockfaces/words/IDateI18n.h"

const char* Sprite::name() { return "Sprite"; }

const char* IDateI18n::formatDate(int, int) { return ""; }
const char* IDateI18n::weekDayName(int) { return ""; }
void IDateI18n::timeInWords(int, int, char*, char*) {}

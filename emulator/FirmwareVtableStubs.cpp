// emulator/FirmwareVtableStubs.cpp
//
// Supplies out-of-line definitions for base-class virtual methods that
// firmware/DeskMatrix declares but never defines at the base level --
// only derived classes (Block, Mario, Ghost, Pacman, DateI18nEN, ...)
// override them, and every real call site dispatches through one of
// those concrete overrides. That's fine under the ESP32 Arduino
// toolchain's GCC/xtensa-elf linker (confirmed: `arduino-cli compile`
// against firmware/DeskMatrix/DeskMatrix.ino succeeds unmodified), but
// Homebrew LLVM's clang++/lld -- required for this native build, see
// CMakeLists.txt's compiler guard -- follows the Itanium C++ ABI's
// key-function rule strictly: a class's vtable/typeinfo are emitted in
// whichever translation unit defines its first non-inline virtual
// method, and if that method is declared but never defined anywhere,
// the vtable/typeinfo symbols are simply missing at link time.
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

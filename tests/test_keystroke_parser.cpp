#include <gtest/gtest.h>
#include <linux/input-event-codes.h>
#include <vector>

#include "input/UinputInjector.h"

using logitune::UinputInjector;

// ---------------------------------------------------------------------------
// Single letters
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, SingleLetterUpperA) {
    EXPECT_EQ(UinputInjector::parseKeystroke("A"), (std::vector<int>{KEY_A}));
}

TEST(KeystrokeParser, SingleLetterUpperZ) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Z"), (std::vector<int>{KEY_Z}));
}

TEST(KeystrokeParser, SingleLetterLowerA) {
    // lowercase maps to the same key as uppercase
    EXPECT_EQ(UinputInjector::parseKeystroke("a"), (std::vector<int>{KEY_A}));
}

// ---------------------------------------------------------------------------
// Digits
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, Digit0) {
    EXPECT_EQ(UinputInjector::parseKeystroke("0"), (std::vector<int>{KEY_0}));
}

TEST(KeystrokeParser, Digit1) {
    EXPECT_EQ(UinputInjector::parseKeystroke("1"), (std::vector<int>{KEY_1}));
}

TEST(KeystrokeParser, Digit9) {
    EXPECT_EQ(UinputInjector::parseKeystroke("9"), (std::vector<int>{KEY_9}));
}

// ---------------------------------------------------------------------------
// Modifiers
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, ModifierCtrl) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Ctrl"), (std::vector<int>{KEY_LEFTCTRL}));
}

TEST(KeystrokeParser, ModifierShift) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Shift"), (std::vector<int>{KEY_LEFTSHIFT}));
}

TEST(KeystrokeParser, ModifierAlt) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Alt"), (std::vector<int>{KEY_LEFTALT}));
}

TEST(KeystrokeParser, ModifierSuper) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Super"), (std::vector<int>{KEY_LEFTMETA}));
}

TEST(KeystrokeParser, ModifierMeta) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Meta"), (std::vector<int>{KEY_LEFTMETA}));
}

// ---------------------------------------------------------------------------
// Combos
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, ComboCtrlC) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Ctrl+C"),
              (std::vector<int>{KEY_LEFTCTRL, KEY_C}));
}

TEST(KeystrokeParser, ComboCtrlShiftZ) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Ctrl+Shift+Z"),
              (std::vector<int>{KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_Z}));
}

TEST(KeystrokeParser, ComboAltF4) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Alt+F4"),
              (std::vector<int>{KEY_LEFTALT, KEY_F4}));
}

TEST(KeystrokeParser, ComboSuperD) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Super+D"),
              (std::vector<int>{KEY_LEFTMETA, KEY_D}));
}

TEST(KeystrokeParser, ComboCtrlSuperLeft) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Ctrl+Super+Left"),
              (std::vector<int>{KEY_LEFTCTRL, KEY_LEFTMETA, KEY_LEFT}));
}

// ---------------------------------------------------------------------------
// Special keys
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, SpecialTab) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Tab"), (std::vector<int>{KEY_TAB}));
}

TEST(KeystrokeParser, SpecialSpace) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Space"), (std::vector<int>{KEY_SPACE}));
}

TEST(KeystrokeParser, SpecialEnter) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Enter"), (std::vector<int>{KEY_ENTER}));
}

TEST(KeystrokeParser, SpecialEscape) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Escape"), (std::vector<int>{KEY_ESC}));
}

TEST(KeystrokeParser, SpecialDelete) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Delete"), (std::vector<int>{KEY_DELETE}));
}

TEST(KeystrokeParser, SpecialBack) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Back"), (std::vector<int>{KEY_BACK}));
}

TEST(KeystrokeParser, SpecialForward) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Forward"), (std::vector<int>{KEY_FORWARD}));
}

TEST(KeystrokeParser, SpecialHome) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Home"), (std::vector<int>{KEY_HOME}));
}

TEST(KeystrokeParser, SpecialEnd) {
    EXPECT_EQ(UinputInjector::parseKeystroke("End"), (std::vector<int>{KEY_END}));
}

TEST(KeystrokeParser, SpecialPageUp) {
    EXPECT_EQ(UinputInjector::parseKeystroke("PageUp"), (std::vector<int>{KEY_PAGEUP}));
}

TEST(KeystrokeParser, SpecialPageDown) {
    EXPECT_EQ(UinputInjector::parseKeystroke("PageDown"), (std::vector<int>{KEY_PAGEDOWN}));
}

TEST(KeystrokeParser, SpecialPrint) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Print"), (std::vector<int>{KEY_SYSRQ}));
}

// ---------------------------------------------------------------------------
// Arrow keys
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, ArrowUp) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Up"), (std::vector<int>{KEY_UP}));
}

TEST(KeystrokeParser, ArrowDown) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Down"), (std::vector<int>{KEY_DOWN}));
}

TEST(KeystrokeParser, ArrowLeft) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Left"), (std::vector<int>{KEY_LEFT}));
}

TEST(KeystrokeParser, ArrowRight) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Right"), (std::vector<int>{KEY_RIGHT}));
}

// ---------------------------------------------------------------------------
// Media keys
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, MediaMute) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Mute"), (std::vector<int>{KEY_MUTE}));
}

TEST(KeystrokeParser, MediaPlay) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Play"), (std::vector<int>{KEY_PLAYPAUSE}));
}

TEST(KeystrokeParser, MediaVolumeUp) {
    EXPECT_EQ(UinputInjector::parseKeystroke("VolumeUp"), (std::vector<int>{KEY_VOLUMEUP}));
}

TEST(KeystrokeParser, MediaVolumeDown) {
    EXPECT_EQ(UinputInjector::parseKeystroke("VolumeDown"), (std::vector<int>{KEY_VOLUMEDOWN}));
}

TEST(KeystrokeParser, MediaNext) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Next"), (std::vector<int>{KEY_NEXTSONG}));
}

TEST(KeystrokeParser, MediaPrevious) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Previous"), (std::vector<int>{KEY_PREVIOUSSONG}));
}

TEST(KeystrokeParser, MediaStop) {
    EXPECT_EQ(UinputInjector::parseKeystroke("Stop"), (std::vector<int>{KEY_STOPCD}));
}

TEST(KeystrokeParser, MediaBrightnessUp) {
    EXPECT_EQ(UinputInjector::parseKeystroke("BrightnessUp"), (std::vector<int>{KEY_BRIGHTNESSUP}));
}

TEST(KeystrokeParser, MediaBrightnessDown) {
    EXPECT_EQ(UinputInjector::parseKeystroke("BrightnessDown"), (std::vector<int>{KEY_BRIGHTNESSDOWN}));
}

// ---------------------------------------------------------------------------
// F keys
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, FKeyF1) {
    EXPECT_EQ(UinputInjector::parseKeystroke("F1"), (std::vector<int>{KEY_F1}));
}

TEST(KeystrokeParser, FKeyF5) {
    EXPECT_EQ(UinputInjector::parseKeystroke("F5"), (std::vector<int>{KEY_F5}));
}

TEST(KeystrokeParser, FKeyF11) {
    EXPECT_EQ(UinputInjector::parseKeystroke("F11"), (std::vector<int>{KEY_F11}));
}

TEST(KeystrokeParser, FKeyF12) {
    EXPECT_EQ(UinputInjector::parseKeystroke("F12"), (std::vector<int>{KEY_F12}));
}

// ---------------------------------------------------------------------------
// Symbol keys (single-character tokens)
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, SymbolMinus) {
    EXPECT_EQ(UinputInjector::parseKeystroke("-"), (std::vector<int>{KEY_MINUS}));
}

TEST(KeystrokeParser, SymbolEqual) {
    EXPECT_EQ(UinputInjector::parseKeystroke("="), (std::vector<int>{KEY_EQUAL}));
}

TEST(KeystrokeParser, SymbolComma) {
    EXPECT_EQ(UinputInjector::parseKeystroke(","), (std::vector<int>{KEY_COMMA}));
}

TEST(KeystrokeParser, SymbolPeriod) {
    EXPECT_EQ(UinputInjector::parseKeystroke("."), (std::vector<int>{KEY_DOT}));
}

TEST(KeystrokeParser, SymbolSlash) {
    EXPECT_EQ(UinputInjector::parseKeystroke("/"), (std::vector<int>{KEY_SLASH}));
}

TEST(KeystrokeParser, SymbolBackslash) {
    EXPECT_EQ(UinputInjector::parseKeystroke("\\"), (std::vector<int>{KEY_BACKSLASH}));
}

TEST(KeystrokeParser, SymbolSemicolon) {
    EXPECT_EQ(UinputInjector::parseKeystroke(";"), (std::vector<int>{KEY_SEMICOLON}));
}

TEST(KeystrokeParser, SymbolApostrophe) {
    EXPECT_EQ(UinputInjector::parseKeystroke("'"), (std::vector<int>{KEY_APOSTROPHE}));
}

TEST(KeystrokeParser, SymbolGrave) {
    EXPECT_EQ(UinputInjector::parseKeystroke("`"), (std::vector<int>{KEY_GRAVE}));
}

TEST(KeystrokeParser, SymbolLeftBrace) {
    EXPECT_EQ(UinputInjector::parseKeystroke("["), (std::vector<int>{KEY_LEFTBRACE}));
}

TEST(KeystrokeParser, SymbolRightBrace) {
    EXPECT_EQ(UinputInjector::parseKeystroke("]"), (std::vector<int>{KEY_RIGHTBRACE}));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(KeystrokeParser, EdgeCaseBareplus) {
    // Bare "+" is special-cased before splitting
    EXPECT_EQ(UinputInjector::parseKeystroke("+"), (std::vector<int>{KEY_KPPLUS}));
}

TEST(KeystrokeParser, EdgeCaseEmptyString) {
    // Empty string: splits into one empty token, no match → empty vector
    EXPECT_TRUE(UinputInjector::parseKeystroke("").empty());
}

TEST(KeystrokeParser, EdgeCaseUnknownKey) {
    // "FooBar" does not match any key name → empty vector
    EXPECT_TRUE(UinputInjector::parseKeystroke("FooBar").empty());
}

TEST(KeystrokeParser, EdgeCaseWhitespaceInCombo) {
    // "Ctrl + C" — each token is trimmed, so this parses the same as "Ctrl+C"
    EXPECT_EQ(UinputInjector::parseKeystroke("Ctrl + C"),
              (std::vector<int>{KEY_LEFTCTRL, KEY_C}));
}

// ---------------------------------------------------------------------------
// parseModifierCombo — the refinement a latch accepts
// ---------------------------------------------------------------------------

TEST(ModifierCombo, SingleModifier) {
    EXPECT_EQ(UinputInjector::parseModifierCombo("Meta"), (std::vector<int>{KEY_LEFTMETA}));
    EXPECT_EQ(UinputInjector::parseModifierCombo("Ctrl"), (std::vector<int>{KEY_LEFTCTRL}));
    EXPECT_EQ(UinputInjector::parseModifierCombo("Shift"), (std::vector<int>{KEY_LEFTSHIFT}));
    EXPECT_EQ(UinputInjector::parseModifierCombo("Alt"), (std::vector<int>{KEY_LEFTALT}));
}

TEST(ModifierCombo, SuperIsMeta) {
    EXPECT_EQ(UinputInjector::parseModifierCombo("Super"), (std::vector<int>{KEY_LEFTMETA}));
}

TEST(ModifierCombo, ModifierOnlyCombo) {
    EXPECT_EQ(UinputInjector::parseModifierCombo("Ctrl+Shift"),
              (std::vector<int>{KEY_LEFTCTRL, KEY_LEFTSHIFT}));
}

TEST(ModifierCombo, MixedComboRefusedWhole) {
    // Not "latch the Ctrl and drop the C" — the whole combo is rejected.
    EXPECT_TRUE(UinputInjector::parseModifierCombo("Ctrl+C").empty());
    EXPECT_TRUE(UinputInjector::parseModifierCombo("Alt+Tab").empty());
}

TEST(ModifierCombo, OrdinaryKeyRefused) {
    EXPECT_TRUE(UinputInjector::parseModifierCombo("A").empty());
    EXPECT_TRUE(UinputInjector::parseModifierCombo("F5").empty());
    EXPECT_TRUE(UinputInjector::parseModifierCombo("VolumeUp").empty());
}

TEST(ModifierCombo, EmptyAndUnknownRefused) {
    EXPECT_TRUE(UinputInjector::parseModifierCombo("").empty());
    EXPECT_TRUE(UinputInjector::parseModifierCombo("NoSuchKey").empty());
}

// ---------------------------------------------------------------------------
// toggleModifierLatch — the flip-flop, observable without a uinput device
// ---------------------------------------------------------------------------

TEST(ModifierLatch, TogglingOneComboAlternates) {
    // No init() here: with no fd the key writes are no-ops, but the latch
    // bookkeeping (which is what callers act on) still runs.
    UinputInjector inj;
    EXPECT_TRUE(inj.toggleModifierLatch("Meta"));
    EXPECT_FALSE(inj.toggleModifierLatch("Meta"));
    EXPECT_TRUE(inj.toggleModifierLatch("Meta"));
}

TEST(ModifierLatch, RefusedComboNeverLatches) {
    UinputInjector inj;
    EXPECT_FALSE(inj.toggleModifierLatch("Ctrl+C"));
    EXPECT_FALSE(inj.toggleModifierLatch("Ctrl+C"));
}

TEST(ModifierLatch, DistinctCombosLatchIndependently) {
    UinputInjector inj;
    EXPECT_TRUE(inj.toggleModifierLatch("Meta"));
    EXPECT_TRUE(inj.toggleModifierLatch("Alt"));
    EXPECT_FALSE(inj.toggleModifierLatch("Meta"));
    EXPECT_FALSE(inj.toggleModifierLatch("Alt"));
}

TEST(ModifierLatch, OverlappingCombosReleaseOnlyWhenFullyHeld) {
    UinputInjector inj;

    EXPECT_TRUE(inj.toggleModifierLatch("Meta"));        // Meta held
    EXPECT_TRUE(inj.toggleModifierLatch("Ctrl+Meta"));   // Ctrl joins, not a release
    EXPECT_FALSE(inj.toggleModifierLatch("Ctrl+Meta"));  // both held -> both released

    // Meta went down once and came back up with the pair, so the next toggle
    // starts from nothing held.
    EXPECT_TRUE(inj.toggleModifierLatch("Meta"));
}

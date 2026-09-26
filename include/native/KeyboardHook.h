#pragma once

#include "core/PhoneticEngine.h"
#include "core/InputBuffer.h"
#include "core/SpecialCharPicker.h"
#include "core/FixedLayoutEngine.h"
#include "native/KeyboardState.h"
#include "core/SuggestionPolicy.h"
#include "core/WordDictionary.h"
#include "ui/CandidateWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

/**
 * @brief Manages installation and event handling of the Windows WH_KEYBOARD_LL hook.
 *
 * Implements low-level keyboard interception, mode switching, candidate cycling, special
 * character selection, fixed-layout mapping and injection coordination.
 *
 * Two composition strategies are supported (see KeyboardState::isLivePreviewEnabled):
 *
 *  - Live preview (default). Every keystroke redraws the in-progress word inside the
 *    target application: the previous rendering is erased with backspaces and the updated
 *    one is injected. The user watches Bengali form as they type, like Avro.
 *  - Flush on delimiter. Nothing is injected until space/enter/punctuation is pressed.
 *    Safer in applications that move the caret underneath us.
 */
class KeyboardHook {
public:
    KeyboardHook() = default;
    ~KeyboardHook();

    /**
     * @brief Installs the WH_KEYBOARD_LL hook.
     * @param engine Phonetic engine used in BENGALI_PHONETIC mode. Must outlive the hook.
     * @param layout Optional fixed layout used in BENGALI_FIXED mode; may be null.
     */
    bool install(PhoneticEngine* engine,
                 FixedLayoutEngine* layout = nullptr,
                 CandidateWindow* candidateWindow = nullptr,
                 WordDictionary* words = nullptr);

    /**
     * @brief Applies a candidate the user picked by clicking a chip, and redraws.
     *
     * Static because the candidate window's callback has no object to call back into: the
     * hook's state is process-wide by necessity, since a Win32 hook procedure is a plain
     * function pointer.
     */
    static void selectCandidate(size_t optionIndex);

    /**
     * @brief Replaces the whole in-progress word with a prediction and commits it.
     *
     * Distinct from selectCandidate, which swaps one letter. This throws away the
     * composition entirely, so it must only ever be reachable from the word row.
     */
    static void selectWord(const std::string& word);

    /**
     * @brief Replaces the word that was just committed with a correction.
     *
     * Separate from selectWord because the target is different: the word is already in the
     * document, followed by the delimiter that ended it, so both have to be removed and
     * both retyped.
     */
    static void applyCorrection(const std::string& word);

    /// Routes a clicked word chip to completion or correction, whichever is in play.
    static void selectWordOrCorrection(const std::string& word);

    /// Pushes the current composition state into the candidate window.
    static void refreshUi();

    /// Uninstalls the active hook.
    void uninstall();

    /// Runs the message pump required by the hook installation thread.
    void runMessageLoop();

    /// Posts WM_QUIT to terminate the message loop.
    void stopMessageLoop();

    /// Returns whether the hook is currently installed.
    bool isInstalled() const;

private:
    static LRESULT CALLBACK hookCallback(int nCode, WPARAM wParam, LPARAM lParam);

    /// Redraws the in-progress word in the target app (live preview only).
    static void refreshPreview();

    /// Finalises the in-progress word: applies exception overrides and clears state.
    static void commitBuffer();

    /// Erases any preview on screen and drops the in-progress word.
    static void discardBuffer();

    /**
     * @brief Decides the text a finished word commits as, and clears the active state.
     *
     * Exception overrides win; otherwise the active candidate list is composed, so that
     * candidates the user picked with Ctrl+Shift+Space survive the commit.
     */
    static std::string finalTextFor(const std::string& roman);

    /**
     * @brief Translates a key event to the ASCII character it would normally produce.
     *
     * Honours Shift and Caps Lock via the active keyboard layout rather than assuming a
     * US mapping, so punctuation keys used by fixed layouts resolve correctly.
     */
    static bool charFromKey(const KBDLLHOOKSTRUCT* kbd, char& out);

    static HHOOK s_hHook;
    static PhoneticEngine* s_engine;
    static FixedLayoutEngine* s_layout;
    static CandidateWindow* s_candidateWindow;
    static WordDictionary* s_words;
    static InputBuffer s_buffer;
    static SpecialCharPicker s_specialPicker;
    static DWORD s_threadId;

    /// UTF-16 code units of preview text currently displayed in the target application.
    static size_t s_previewUnits;

    /// The preview text currently displayed, so a commit can skip a redundant redraw.
    static std::string s_previewText;

    /// Shows corrections for the word just committed, if it was not a real word.
    static void offerCorrections();

    /// The word most recently committed, and the delimiter that ended it. Both are needed
    /// to undo a commit when the user takes a correction.
    static std::string s_committedText;
    static size_t s_committedUnits;
    static char s_committedDelimiter;
};

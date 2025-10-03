/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Profile Switcher - Manages HDR profile switching via software signals
 */

#ifndef HDR_PROFILE_SWITCHER_H
#define HDR_PROFILE_SWITCHER_H

#include <glib.h>

/**
 * HdrProfileSwitcher:
 *
 * Manages HDR profile switching requests and retry logic.
 * Keeps track of pending switch requests and manages the retry count
 * to ensure software signals are sent enough times for the camera
 * to receive them during its listening window.
 */
class HdrProfileSwitcher {
public:
    HdrProfileSwitcher();
    ~HdrProfileSwitcher() = default;

    /**
     * Request a profile switch
     * @param target_profile Target profile (0 or 1)
     * @param retry_count Number of times to retry sending the signal
     */
    void RequestSwitch(gint target_profile, gint retry_count);

    /**
     * Check if a switch signal should be sent
     * @param profile_to_signal Output: the profile to signal if return is TRUE
     * @return TRUE if a signal should be sent, FALSE otherwise
     */
    gboolean GetPendingSignal(gint* profile_to_signal);

    /**
     * Reset the switcher state
     */
    void Reset();

    /**
     * Check if currently switching
     * @return TRUE if switch is in progress
     */
    gboolean IsSwitching() const { return retry_count_ > 0; }

private:
    gint target_profile_;
    gint retry_count_;
};

#endif // HDR_PROFILE_SWITCHER_H
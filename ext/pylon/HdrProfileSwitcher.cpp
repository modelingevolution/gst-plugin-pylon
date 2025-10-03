/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Profile Switcher implementation
 */

#include "HdrProfileSwitcher.h"

HdrProfileSwitcher::HdrProfileSwitcher()
    : target_profile_(-1),
      retry_count_(0) {
}

void HdrProfileSwitcher::RequestSwitch(gint target_profile, gint retry_count) {
    g_return_if_fail(target_profile == 0 || target_profile == 1);
    g_return_if_fail(retry_count > 0);

    target_profile_ = target_profile;
    retry_count_ = retry_count;
}

gboolean HdrProfileSwitcher::GetPendingSignal(gint* profile_to_signal) {
    g_return_val_if_fail(profile_to_signal != nullptr, FALSE);

    if (retry_count_ <= 0) {
        return FALSE;
    }

    *profile_to_signal = target_profile_;
    retry_count_--;

    // Reset target when done with retries
    if (retry_count_ == 0) {
        target_profile_ = -1;
    }

    return TRUE;
}

void HdrProfileSwitcher::Reset() {
    target_profile_ = -1;
    retry_count_ = 0;
}
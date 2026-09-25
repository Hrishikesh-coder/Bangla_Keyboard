#include "core/CandidateResolver.h"

size_t DefaultCandidateResolver::resolve(const Candidate& /*candidate*/,
                                         size_t /*tokenIndex*/,
                                         const std::vector<Candidate>& /*allCandidates*/) {
    // The baseline educational implementation always selects candidate index 0.
    return 0;
}

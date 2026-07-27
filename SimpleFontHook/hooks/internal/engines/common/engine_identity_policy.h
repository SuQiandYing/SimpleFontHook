#pragma once

namespace EngineIdentityPolicy {

struct Evidence {
    bool identityMarker;
    bool validatedContract;
    bool containerFormat;
    bool engineResource;
    bool frameworkContract = false;
};

constexpr bool Confirm(const Evidence& evidence) {
    return evidence.identityMarker || evidence.validatedContract || evidence.frameworkContract ||
        (evidence.containerFormat && evidence.engineResource);
}

constexpr bool ConfirmFrameworkContract(bool runtimeLayout, bool identityManifest) {
    return runtimeLayout && identityManifest;
}

constexpr bool ConfirmCapability(bool identity, bool capability) {
    return identity && capability;
}

static_assert(!Confirm({ false, false, false, false, false }),
    "a path or suffix without identity evidence must not activate an adapter");
static_assert(!Confirm({ false, false, true, false, false }),
    "a container format without an engine resource is only a candidate");
static_assert(!Confirm({ false, false, false, true, false }),
    "an unvalidated resource name is only a candidate");
static_assert(Confirm({ false, false, true, true, false }),
    "a parsed container plus an engine resource confirms identity");
static_assert(!ConfirmFrameworkContract(true, false),
    "a framework directory without its identity manifest is only a candidate");
static_assert(ConfirmFrameworkContract(true, true),
    "a framework layout plus its identity manifest confirms identity");
static_assert(!ConfirmCapability(true, false),
    "identity without the requested capability must not install a specialized hook");

} // namespace EngineIdentityPolicy

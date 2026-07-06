#include "hook_policy.h"

namespace HookPolicy {

    static __declspec(thread) HookApi g_currentApi = HookApi::Unknown;
    static __declspec(thread) const char* g_currentApiName = "unknown";

    static bool IsName(const char* left, const char* right) {
        return left && right && lstrcmpA(left, right) == 0;
    }

    HookApi ApiFromName(const char* apiName) {
        if (IsName(apiName, "TextOutA")) return HookApi::TextOutA;
        if (IsName(apiName, "TextOutW")) return HookApi::TextOutW;
        if (IsName(apiName, "ExtTextOutA")) return HookApi::ExtTextOutA;
        if (IsName(apiName, "ExtTextOutW")) return HookApi::ExtTextOutW;
        if (IsName(apiName, "DrawTextA")) return HookApi::DrawTextA;
        if (IsName(apiName, "DrawTextW")) return HookApi::DrawTextW;
        if (IsName(apiName, "DrawTextExA")) return HookApi::DrawTextExA;
        if (IsName(apiName, "DrawTextExW")) return HookApi::DrawTextExW;
        if (IsName(apiName, "SelectObject")) return HookApi::SelectObject;
        if (IsName(apiName, "GetFontData")) return HookApi::GetFontData;
        if (IsName(apiName, "GetGlyphOutlineA")) return HookApi::GetGlyphOutlineA;
        if (IsName(apiName, "GetGlyphOutlineW")) return HookApi::GetGlyphOutlineW;
        if (IsName(apiName, "GetTextMetricsA")) return HookApi::GetTextMetricsA;
        if (IsName(apiName, "GetTextMetricsW")) return HookApi::GetTextMetricsW;
        return HookApi::Unknown;
    }

    const char* ApiName(HookApi api) {
        switch (api) {
        case HookApi::TextOutA: return "TextOutA";
        case HookApi::TextOutW: return "TextOutW";
        case HookApi::ExtTextOutA: return "ExtTextOutA";
        case HookApi::ExtTextOutW: return "ExtTextOutW";
        case HookApi::DrawTextA: return "DrawTextA";
        case HookApi::DrawTextW: return "DrawTextW";
        case HookApi::DrawTextExA: return "DrawTextExA";
        case HookApi::DrawTextExW: return "DrawTextExW";
        case HookApi::SelectObject: return "SelectObject";
        case HookApi::GetFontData: return "GetFontData";
        case HookApi::GetGlyphOutlineA: return "GetGlyphOutlineA";
        case HookApi::GetGlyphOutlineW: return "GetGlyphOutlineW";
        case HookApi::GetTextMetricsA: return "GetTextMetricsA";
        case HookApi::GetTextMetricsW: return "GetTextMetricsW";
        default: return "unknown";
        }
    }

    ApiScope::ApiScope(const char* apiName)
        : previousApi_(g_currentApi), previousName_(g_currentApiName) {
        g_currentApi = ApiFromName(apiName);
        g_currentApiName = apiName ? apiName : ApiName(g_currentApi);
    }

    ApiScope::~ApiScope() {
        g_currentApi = previousApi_;
        g_currentApiName = previousName_;
    }

    HookApi CurrentApi() {
        return g_currentApi;
    }

    const char* CurrentApiName() {
        return g_currentApiName ? g_currentApiName : "unknown";
    }

    HdcReplaceDecision ShouldReplaceHdcFont(HookApi api, const RuntimeContext& context) {
        if (context.isPickerThread) return { false, SkipReason::PickerThread };
        if (!context.enableFontHook && !context.enableCodepageSpoof) return { false, SkipReason::HookDisabled };

        if (context.enableFaceNameReplace && Config::CompatSkipDrawTextA &&
            (api == HookApi::DrawTextA || api == HookApi::DrawTextExA)) {
            return { false, SkipReason::DrawTextACompat };
        }

        if (context.enableFaceNameReplace && Config::CompatSkipFontDataQueries &&
            api == HookApi::GetFontData && !context.allowFontDataQueryReplacement) {
            return { false, SkipReason::FontDataQueryCompat };
        }

        return { true, SkipReason::None };
    }

    bool ShouldInstallHook(HookInstallPoint point) {
        switch (point) {
        case HookInstallPoint::CreateFontW:
            return Config::CompatHookCreateFontW;
        case HookInstallPoint::CreateFontIndirectW:
            return Config::CompatHookCreateFontIndirectW;
        case HookInstallPoint::GetTextFaceA:
        case HookInstallPoint::GetTextFaceW:
            return Config::CompatHookGetTextFace;
        default:
            return false;
        }
    }

    bool ShouldPassThroughUntrackedSelectObject() {
        return Config::CompatSelectObjectTrackedOnly;
    }

    bool ShouldLogSkipReason(SkipReason reason, LONG occurrence) {
        if (reason != SkipReason::PickerThread) return true;
        return Config::DebugPickerThreadLogLimit > 0 && occurrence <= Config::DebugPickerThreadLogLimit;
    }

    const char* SkipReasonName(SkipReason reason) {
        switch (reason) {
        case SkipReason::PickerThread: return "picker-thread";
        case SkipReason::HookDisabled: return "hook-disabled";
        case SkipReason::DrawTextACompat: return "drawtexta-compat-skip";
        case SkipReason::FontDataQueryCompat: return "fontdata-query-compat-skip";
        default: return "none";
        }
    }

    unsigned long long TraceSampleLimit() {
        if (Config::DebugTraceSampleLimit <= 0) return 0;
        return (unsigned long long)Config::DebugTraceSampleLimit;
    }

    const char* OnOff(bool value) {
        return value ? "on" : "off";
    }
}

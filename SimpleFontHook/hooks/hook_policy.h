#pragma once
#include "../framework.h"

namespace HookPolicy {

    enum class HookApi {
        Unknown = 0,
        TextOutA,
        TextOutW,
        ExtTextOutA,
        ExtTextOutW,
        DrawTextA,
        DrawTextW,
        DrawTextExA,
        DrawTextExW,
        SelectObject,
        GetFontData,
        GetGlyphOutlineA,
        GetGlyphOutlineW,
        GetTextMetricsA,
        GetTextMetricsW,
    };

    enum class HookInstallPoint {
        CreateFontW = 0,
        CreateFontIndirectW,
        GetTextFaceA,
        GetTextFaceW,
    };

    enum class SkipReason {
        None = 0,
        PickerThread,
        HookDisabled,
        DrawTextACompat,
        FontDataQueryCompat,
    };

    struct RuntimeContext {
        bool isPickerThread;
        bool enableFontHook;
        bool enableCodepageSpoof;
        bool enableFaceNameReplace;
        bool allowFontDataQueryReplacement;
    };

    struct HdcReplaceDecision {
        bool allow;
        SkipReason reason;
    };

    class ApiScope {
    public:
        explicit ApiScope(const char* apiName);
        ~ApiScope();

    private:
        HookApi previousApi_;
        const char* previousName_;
    };

    HookApi ApiFromName(const char* apiName);
    const char* ApiName(HookApi api);
    HookApi CurrentApi();
    const char* CurrentApiName();

    HdcReplaceDecision ShouldReplaceHdcFont(HookApi api, const RuntimeContext& context);
    bool ShouldInstallHook(HookInstallPoint point);
    bool ShouldPassThroughUntrackedSelectObject();
    bool ShouldLogSkipReason(SkipReason reason, LONG occurrence);
    const char* SkipReasonName(SkipReason reason);
    unsigned long long TraceSampleLimit();
    const char* OnOff(bool value);
}

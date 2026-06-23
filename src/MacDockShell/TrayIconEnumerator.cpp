#include "TrayIconEnumerator.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPoint>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include "IconUtils.h"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <UIAutomation.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace {

struct TrayDebugStats
{
    int uiaRoot = 0;
    int uiaDesktop = 0;
    int areas = 0;
    int descendants = 0;
    int win32Toolbar = 0;
    int win32Hosts = 0;
    int accepted = 0;
    int skippedTaskbar = 0;
    int skippedOverflow = 0;
    int skippedSystem = 0;
    int skippedEmpty = 0;
    int skippedUnknownHost = 0;
    QStringList candidates;

    void recordCandidate(const QString& source, const TrayIconInfo& info, bool acceptedCandidate, const QString& reason)
    {
        if (acceptedCandidate) {
            ++accepted;
        }
        if (candidates.size() >= 12) {
            return;
        }

        const QString title = info.tooltip.section(QLatin1Char('\n'), 0, 0).trimmed();
        candidates.push_back(QStringLiteral("%1 class=%2 pid=%3 title=%4 exe=%5 accepted=%6%7")
            .arg(source,
                 info.className.isEmpty() ? QStringLiteral("<none>") : info.className,
                 QString::number(info.processId),
                 title.isEmpty() ? QStringLiteral("<empty>") : title,
                 info.exePath.isEmpty() ? QStringLiteral("<empty>") : info.exePath,
                 acceptedCandidate ? QStringLiteral("yes") : QStringLiteral("no"),
                 reason.isEmpty() ? QString() : QStringLiteral(" reason=%1").arg(reason)));
    }

    QString summary(int displayedCount) const
    {
        return QStringLiteral("Tray debug: uiaRoot=%1 desktop=%2 areas=%3 descendants=%4 win32Toolbar=%5 win32Hosts=%6 accepted=%7 displayed=%8")
            .arg(uiaRoot)
            .arg(uiaDesktop)
            .arg(areas)
            .arg(descendants)
            .arg(win32Toolbar)
            .arg(win32Hosts)
            .arg(accepted)
            .arg(displayedCount);
    }

    QString skipSummary() const
    {
        return QStringLiteral("Tray debug skips: system=%1 overflow=%2 taskbar=%3 empty=%4 unknownHost=%5")
            .arg(skippedSystem)
            .arg(skippedOverflow)
            .arg(skippedTaskbar)
            .arg(skippedEmpty)
            .arg(skippedUnknownHost);
    }
};

thread_local TrayDebugStats* gTrayDebugStats = nullptr;

QString variantToQString(const VARIANT& value)
{
    if (value.vt == VT_BSTR && value.bstrVal) {
        return QString::fromWCharArray(value.bstrVal);
    }
    return {};
}

QVector<int> runtimeIdFromVariant(const VARIANT& value)
{
    QVector<int> result;
    if (value.vt != (VT_ARRAY | VT_I4) || !value.parray) {
        return result;
    }

    LONG lower = 0;
    LONG upper = -1;
    SafeArrayGetLBound(value.parray, 1, &lower);
    SafeArrayGetUBound(value.parray, 1, &upper);
    for (LONG i = lower; i <= upper; ++i) {
        int id = 0;
        if (SUCCEEDED(SafeArrayGetElement(value.parray, &i, &id))) {
            result.push_back(id);
        }
    }
    return result;
}

QRect rectFromVariant(const VARIANT& value)
{
    if (value.vt != (VT_ARRAY | VT_R8) || !value.parray) {
        return {};
    }

    double values[4] = {};
    for (LONG i = 0; i < 4; ++i) {
        SafeArrayGetElement(value.parray, &i, &values[i]);
    }
    const int left = static_cast<int>(values[0]);
    const int top = static_cast<int>(values[1]);
    const int width = static_cast<int>(values[2] - values[0]);
    const int height = static_cast<int>(values[3] - values[1]);
    return QRect(left, top, width, height);
}

bool invokeElement(IUIAutomationElement* element)
{
    if (!element) {
        return false;
    }

    IUnknown* patternObject = nullptr;
    if (FAILED(element->GetCurrentPattern(UIA_InvokePatternId, &patternObject)) || !patternObject) {
        return false;
    }

    IUIAutomationInvokePattern* invoke = nullptr;
    const HRESULT qi = patternObject->QueryInterface(__uuidof(IUIAutomationInvokePattern),
                                                       reinterpret_cast<void**>(&invoke));
    patternObject->Release();
    if (FAILED(qi) || !invoke) {
        return false;
    }

    const HRESULT hr = invoke->Invoke();
    invoke->Release();
    return SUCCEEDED(hr);
}

bool legacyDoDefaultAction(IUIAutomationElement* element)
{
    if (!element) {
        return false;
    }

    IUnknown* patternObject = nullptr;
    if (FAILED(element->GetCurrentPattern(UIA_LegacyIAccessiblePatternId, &patternObject)) || !patternObject) {
        return false;
    }

    IUIAutomationLegacyIAccessiblePattern* legacy = nullptr;
    const HRESULT qi = patternObject->QueryInterface(__uuidof(IUIAutomationLegacyIAccessiblePattern),
                                                       reinterpret_cast<void**>(&legacy));
    patternObject->Release();
    if (FAILED(qi) || !legacy) {
        return false;
    }

    const HRESULT hr = legacy->DoDefaultAction();
    legacy->Release();
    return SUCCEEDED(hr);
}

bool tryActivateElement(IUIAutomationElement* element)
{
    if (!element) {
        return false;
    }

    if (invokeElement(element)) {
        return true;
    }

    if (legacyDoDefaultAction(element)) {
        return true;
    }

    IUnknown* patternObject = nullptr;
    if (SUCCEEDED(element->GetCurrentPattern(UIA_TogglePatternId, &patternObject)) && patternObject) {
        IUIAutomationTogglePattern* toggle = nullptr;
        if (SUCCEEDED(patternObject->QueryInterface(__uuidof(IUIAutomationTogglePattern),
                                                     reinterpret_cast<void**>(&toggle)))
            && toggle) {
            const HRESULT hr = toggle->Toggle();
            toggle->Release();
            patternObject->Release();
            if (SUCCEEDED(hr)) {
                return true;
            }
        } else {
            patternObject->Release();
        }
    }

    patternObject = nullptr;
    if (SUCCEEDED(element->GetCurrentPattern(UIA_ExpandCollapsePatternId, &patternObject)) && patternObject) {
        IUIAutomationExpandCollapsePattern* expand = nullptr;
        if (SUCCEEDED(patternObject->QueryInterface(__uuidof(IUIAutomationExpandCollapsePattern),
                                                     reinterpret_cast<void**>(&expand)))
            && expand) {
            const HRESULT hr = expand->Expand();
            expand->Release();
            patternObject->Release();
            if (SUCCEEDED(hr)) {
                return true;
            }
        } else {
            patternObject->Release();
        }
    }

    return false;
}

TrayIconInfo infoFromElement(IUIAutomationElement* element, bool isSystemPromoted, bool inOverflow)
{
    TrayIconInfo info;
    info.isSystemPromoted = isSystemPromoted;
    info.inOverflow = inOverflow;

    VARIANT value;
    VariantInit(&value);

    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_NamePropertyId, &value))) {
        info.tooltip = variantToQString(value);
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_AutomationIdPropertyId, &value))) {
        info.automationId = variantToQString(value);
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_ClassNamePropertyId, &value))) {
        info.className = variantToQString(value);
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &value))) {
        info.boundingRect = rectFromVariant(value);
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_NativeWindowHandlePropertyId, &value))) {
        if (value.vt == VT_I4) {
            info.nativeWindowHandle = static_cast<quintptr>(value.lVal);
        }
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_ProcessIdPropertyId, &value))) {
        if (value.vt == VT_I4) {
            info.processId = static_cast<quint32>(value.lVal);
        }
        VariantClear(&value);
    }
    if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_RuntimeIdPropertyId, &value))) {
        info.runtimeId = runtimeIdFromVariant(value);
        VariantClear(&value);
    }

    QStringList idParts;
    for (int id : info.runtimeId) {
        idParts.push_back(QString::number(id));
    }
    info.stableId = idParts.join(QLatin1Char(':'));
    if (info.stableId.isEmpty()) {
        info.stableId = QStringLiteral("%1:%2:%3")
                            .arg(info.tooltip)
                            .arg(info.automationId)
                            .arg(inOverflow ? QStringLiteral("overflow") : QStringLiteral("main"));
    }
    return info;
}

bool isTaskbarListButton(const TrayIconInfo& info)
{
    const QString lowerClass = info.className.toLower();
    const QString lowerName = info.tooltip.toLower();
    if (lowerClass.contains(QStringLiteral("tasklist"))
        || lowerClass.contains(QStringLiteral("taskbar.tasklist"))) {
        return true;
    }
    if (lowerName.contains(QStringLiteral("running window"))
        || lowerName.contains(QStringLiteral(" pinned"))) {
        return true;
    }
    if (info.automationId.startsWith(QStringLiteral("Appid:"), Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

bool isOverflowLikeTrayButton(const TrayIconInfo& info)
{
    const QString lowerName = info.tooltip.toLower();
    const QString lowerId = info.automationId.toLower();
    const QString lowerClass = info.className.toLower();
    return lowerName.contains(QStringLiteral("hidden icons"))
        || lowerName.contains(QStringLiteral("notification chevron"))
        || lowerName.contains(QStringLiteral("show hidden icons"))
        || lowerName.startsWith(QStringLiteral("show hidden icons "))
        || lowerId.contains(QStringLiteral("overflow"))
        || lowerId.contains(QStringLiteral("chevron"))
        || lowerId == QStringLiteral("systemtray.overflow")
        || lowerClass.contains(QStringLiteral("showdesktop"));
}

bool isSystemStatusTrayButton(const TrayIconInfo& info)
{
    const QString lowerName = info.tooltip.toLower();
    const QString lowerId = info.automationId.toLower();
    const QString lowerClass = info.className.toLower();
    static const QStringList kSystemNames = {
        QStringLiteral("network"),
        QStringLiteral("wi-fi"),
        QStringLiteral("wifi"),
        QStringLiteral("ethernet"),
        QStringLiteral("volume"),
        QStringLiteral("speaker"),
        QStringLiteral("sound"),
        QStringLiteral("battery"),
        QStringLiteral("power"),
        QStringLiteral("clock"),
        QStringLiteral("date"),
        QStringLiteral("action center"),
        QStringLiteral("notification center"),
        QStringLiteral("notifications"),
        QStringLiteral("input indicator"),
        QStringLiteral("touch keyboard"),
        QStringLiteral("privacy "),
        QStringLiteral("microphone in use"),
        QStringLiteral("widgets"),
        QStringLiteral("start"),
    };
    for (const QString& token : kSystemNames) {
        if (lowerName.contains(token) || lowerId.contains(token)) {
            return true;
        }
    }
    if (lowerClass.contains(QStringLiteral("omnibutton"))
        || lowerClass.contains(QStringLiteral("accentbutton"))
        || lowerClass.contains(QStringLiteral("accenttext"))) {
        return true;
    }
    return false;
}

QString skipReasonForTrayCandidate(const TrayIconInfo& info, bool requireBoundingRect = false)
{
    if (info.tooltip.isEmpty() && info.automationId.isEmpty() && info.boundingRect.isEmpty()) {
        return QStringLiteral("empty");
    }
    if (requireBoundingRect && info.boundingRect.isEmpty()) {
        return QStringLiteral("empty");
    }
    if (isTaskbarListButton(info)) {
        return QStringLiteral("taskbar");
    }
    if (isOverflowLikeTrayButton(info)) {
        return QStringLiteral("overflow");
    }
    if (isSystemStatusTrayButton(info)) {
        return QStringLiteral("system");
    }
    return {};
}

void recordSkipReason(TrayDebugStats* stats, const QString& reason)
{
    if (!stats) {
        return;
    }
    if (reason == QLatin1String("taskbar")) {
        ++stats->skippedTaskbar;
    } else if (reason == QLatin1String("overflow")) {
        ++stats->skippedOverflow;
    } else if (reason == QLatin1String("system")) {
        ++stats->skippedSystem;
    } else if (reason == QLatin1String("empty")) {
        ++stats->skippedEmpty;
    } else if (reason == QLatin1String("unknownHost")) {
        ++stats->skippedUnknownHost;
    }
}

bool isSkippableTrayButton(const TrayIconInfo& info)
{
    return !skipReasonForTrayCandidate(info).isEmpty();
}

IUIAutomationElement* findByNameInSubtree(IUIAutomation* automation,
                                          IUIAutomationElement* root,
                                          const wchar_t* name)
{
    if (!automation || !root || !name) {
        return nullptr;
    }

    VARIANT nameVar;
    VariantInit(&nameVar);
    nameVar.vt = VT_BSTR;
    nameVar.bstrVal = SysAllocString(name);

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_NamePropertyId, nameVar, &condition);
    VariantClear(&nameVar);

    IUIAutomationElement* found = nullptr;
    if (condition) {
        root->FindFirst(TreeScope_Descendants, condition, &found);
        condition->Release();
    }
    return found;
}

void focusShellTrayWindow()
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        SetForegroundWindow(tray);
        Sleep(100);
    }
}

IUIAutomationElement* findByAutomationIdInSubtree(IUIAutomation* automation,
                                                  IUIAutomationElement* root,
                                                  const wchar_t* automationId)
{
    if (!automation || !root || !automationId) {
        return nullptr;
    }

    VARIANT idVar;
    VariantInit(&idVar);
    idVar.vt = VT_BSTR;
    idVar.bstrVal = SysAllocString(automationId);

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, idVar, &condition);
    VariantClear(&idVar);

    IUIAutomationElement* found = nullptr;
    if (condition) {
        root->FindFirst(TreeScope_Descendants, condition, &found);
        condition->Release();
    }
    return found;
}

bool isChevronLikeName(const QString& nameLower)
{
    return nameLower.contains(QStringLiteral("hidden icons"))
        || nameLower.contains(QStringLiteral("show hidden"))
        || nameLower.contains(QStringLiteral("overflow"));
}

IUIAutomationElement* findChevronInNotificationAreaButtons(IUIAutomation* automation,
                                                             IUIAutomationElement* trayRoot)
{
    static const wchar_t* kAreaNames[] = {
        L"User Promoted Notification Area",
        L"Notification Area",
        L"System Promoted Notification Area",
    };

    for (const wchar_t* areaName : kAreaNames) {
        IUIAutomationElement* area = findByNameInSubtree(automation, trayRoot, areaName);
        if (!area) {
            continue;
        }

        VARIANT buttonType;
        VariantInit(&buttonType);
        buttonType.vt = VT_I4;
        buttonType.lVal = UIA_ButtonControlTypeId;
        IUIAutomationCondition* typeCondition = nullptr;
        automation->CreatePropertyCondition(UIA_ControlTypePropertyId, buttonType, &typeCondition);
        VariantClear(&buttonType);
        if (!typeCondition) {
            area->Release();
            continue;
        }

        IUIAutomationElementArray* array = nullptr;
        if (FAILED(area->FindAll(TreeScope_Descendants, typeCondition, &array))) {
            typeCondition->Release();
            area->Release();
            continue;
        }
        typeCondition->Release();
        area->Release();

        IUIAutomationElement* chevron = nullptr;
        int count = 0;
        array->get_Length(&count);
        for (int i = 0; i < count; ++i) {
            IUIAutomationElement* element = nullptr;
            if (FAILED(array->GetElement(i, &element)) || !element) {
                continue;
            }

            VARIANT value;
            VariantInit(&value);
            QString name;
            if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_NamePropertyId, &value))) {
                name = variantToQString(value).toLower();
                VariantClear(&value);
            }

            if (isChevronLikeName(name)) {
                chevron = element;
                break;
            }
            element->Release();
        }
        array->Release();
        if (chevron) {
            return chevron;
        }
    }
    return nullptr;
}

IUIAutomationElement* findDescendantButton(IUIAutomation* automation,
                                             IUIAutomationElement* parent,
                                             const wchar_t* name)
{
    if (!automation || !parent || !name) {
        return nullptr;
    }

    VARIANT nameVar;
    VariantInit(&nameVar);
    nameVar.vt = VT_BSTR;
    nameVar.bstrVal = SysAllocString(name);

    IUIAutomationCondition* nameCondition = nullptr;
    automation->CreatePropertyCondition(UIA_NamePropertyId, nameVar, &nameCondition);
    VariantClear(&nameVar);

    VARIANT buttonType;
    VariantInit(&buttonType);
    buttonType.vt = VT_I4;
    buttonType.lVal = UIA_ButtonControlTypeId;
    IUIAutomationCondition* typeCondition = nullptr;
    automation->CreatePropertyCondition(UIA_ControlTypePropertyId, buttonType, &typeCondition);
    VariantClear(&buttonType);

    IUIAutomationCondition* condition = nullptr;
    if (nameCondition && typeCondition) {
        automation->CreateAndCondition(nameCondition, typeCondition, &condition);
        nameCondition->Release();
        typeCondition->Release();
    }

    IUIAutomationElement* found = nullptr;
    if (condition) {
        parent->FindFirst(TreeScope_Descendants, condition, &found);
        condition->Release();
    }
    return found;
}

IUIAutomationElement* findOverflowChevronElement(IUIAutomation* automation, IUIAutomationElement* trayRoot)
{
    if (!automation || !trayRoot) {
        return nullptr;
    }

    focusShellTrayWindow();

    static const wchar_t* kChevronNames[] = {
        L"Notification Chevron",
        L"Show Hidden Icons",
        L"Show hidden icons",
        L"Show hidden icons button",
    };

    for (const wchar_t* name : kChevronNames) {
        IUIAutomationElement* found = findByNameInSubtree(automation, trayRoot, name);
        if (found) {
            return found;
        }
        found = findDescendantButton(automation, trayRoot, name);
        if (found) {
            return found;
        }
    }

    static const wchar_t* kChevronIds[] = {
        L"OverflowToggleButton",
        L"SystemTrayOverflowButton",
        L"ChevronIcon",
        L"NotifyIconOverflow",
    };

    for (const wchar_t* automationId : kChevronIds) {
        IUIAutomationElement* found = findByAutomationIdInSubtree(automation, trayRoot, automationId);
        if (found) {
            return found;
        }
    }

    VARIANT idVar;
    VariantInit(&idVar);
    idVar.vt = VT_BSTR;
    idVar.bstrVal = SysAllocString(L"SystemTrayIcon");

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, idVar, &condition);
    VariantClear(&idVar);
    if (!condition) {
        return findChevronInNotificationAreaButtons(automation, trayRoot);
    }

    IUIAutomationElementArray* array = nullptr;
    if (FAILED(trayRoot->FindAll(TreeScope_Descendants, condition, &array))) {
        condition->Release();
        return findChevronInNotificationAreaButtons(automation, trayRoot);
    }
    condition->Release();

    IUIAutomationElement* chevron = nullptr;
    int count = 0;
    array->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(array->GetElement(i, &element)) || !element) {
            continue;
        }

        VARIANT value;
        VariantInit(&value);
        QString name;
        if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_NamePropertyId, &value))) {
            name = variantToQString(value).toLower();
            VariantClear(&value);
        }

        if (isChevronLikeName(name)) {
            chevron = element;
            break;
        }
        element->Release();
    }
    array->Release();

    if (chevron) {
        return chevron;
    }
    return findChevronInNotificationAreaButtons(automation, trayRoot);
}

bool sendVirtualKeyPress(WORD vk)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool sendWinBChord()
{
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LWIN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'B';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'B';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LWIN;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, inputs, sizeof(INPUT)) == 4;
}

HWND findOverflowXamlIslandWindow();

bool openOverflowViaKeyboardImpl(IUIAutomation* automation, IUIAutomationElement* chevron)
{
    Q_UNUSED(automation);

    focusShellTrayWindow();

    if (chevron) {
        if (SUCCEEDED(chevron->SetFocus())) {
            Sleep(50);
            sendVirtualKeyPress(VK_SPACE);
            Sleep(100);
            sendVirtualKeyPress(VK_RETURN);
            Sleep(100);
        }
    }

    focusShellTrayWindow();
    sendWinBChord();
    Sleep(150);
    for (int i = 0; i < 30; ++i) {
        sendVirtualKeyPress(VK_RIGHT);
        Sleep(30);
    }
    sendVirtualKeyPress(VK_SPACE);
    Sleep(100);

    sendVirtualKeyPress(VK_END);
    Sleep(50);
    sendVirtualKeyPress(VK_SPACE);
    Sleep(100);

    return true;
}

bool isNotifyIconHostClassName(const QString& windowClass)
{
    static const QStringList kHostClasses = {
        QStringLiteral("Electron_NotifyIconHostWindow"),
        QStringLiteral("Chrome_StatusTrayWindow"),
        QStringLiteral("Qt51519TrayIconMessageWindowClass"),
        QStringLiteral("Qt6TrayIconMessageWindowClass"),
    };
    for (const QString& hostClass : kHostClasses) {
        if (windowClass.contains(hostClass, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return windowClass.contains(QStringLiteral("NotifyIcon"), Qt::CaseInsensitive)
        || windowClass.contains(QStringLiteral("TrayIcon"), Qt::CaseInsensitive);
}

TrayIconInfo infoFromNotifyHostHwnd(HWND hwnd)
{
    TrayIconInfo info;
    if (!hwnd) {
        return info;
    }

    info.nativeWindowHandle = reinterpret_cast<quintptr>(hwnd);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    info.processId = processId;

    wchar_t className[256] = {};
    if (GetClassNameW(hwnd, className, 256) > 0) {
        info.className = QString::fromWCharArray(className);
    }

    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 512);
    info.tooltip = QString::fromWCharArray(title).trimmed();
    info.automationId = QStringLiteral("SystemTrayIcon");
    info.stableId = QStringLiteral("hwnd:%1").arg(info.nativeWindowHandle);
    return info;
}

QVector<TrayIconInfo> enumerateElementsByAutomationId(IUIAutomation* automation,
                                                       IUIAutomationElement* root,
                                                       const wchar_t* automationId,
                                                       bool inOverflow,
                                                       int TrayDebugStats::*debugCounter = nullptr,
                                                       const QString& debugSource = QString())
{
    QVector<TrayIconInfo> icons;
    if (!automation || !root || !automationId) {
        return icons;
    }

    VARIANT idVar;
    VariantInit(&idVar);
    idVar.vt = VT_BSTR;
    idVar.bstrVal = SysAllocString(automationId);

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, idVar, &condition);
    VariantClear(&idVar);

    IUIAutomationElementArray* array = nullptr;
    if (!condition || FAILED(root->FindAll(TreeScope_Descendants, condition, &array))) {
        if (condition) {
            condition->Release();
        }
        return icons;
    }
    condition->Release();

    int count = 0;
    array->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(array->GetElement(i, &element)) || !element) {
            continue;
        }

        if (gTrayDebugStats && debugCounter) {
            ++(gTrayDebugStats->*debugCounter);
        }
        TrayIconInfo info = infoFromElement(element, false, inOverflow);
        const QString skipReason = skipReasonForTrayCandidate(info);
        if (!skipReason.isEmpty()) {
            recordSkipReason(gTrayDebugStats, skipReason);
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(debugSource, info, false, skipReason);
            }
            element->Release();
            continue;
        }

        if (gTrayDebugStats) {
            gTrayDebugStats->recordCandidate(debugSource, info, true, {});
        }
        icons.push_back(info);
        element->Release();
    }

    array->Release();
    return icons;
}

HWND findOverflowXamlIslandWindow()
{
    HWND found = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }
        if (wcscmp(className, L"TopLevelWindowForOverflowXamlIsland") != 0) {
            return TRUE;
        }
        *reinterpret_cast<HWND*>(lParam) = hwnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&found));
    return found;
}

QVector<TrayIconInfo> enumerateButtonsInArea(IUIAutomation* automation,
                                             IUIAutomationElement* area,
                                             bool isSystemPromoted,
                                             bool inOverflow)
{
    QVector<TrayIconInfo> icons;
    if (!automation || !area) {
        return icons;
    }

    VARIANT buttonType;
    VariantInit(&buttonType);
    buttonType.vt = VT_I4;
    buttonType.lVal = UIA_ButtonControlTypeId;

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_ControlTypePropertyId, buttonType, &condition);
    VariantClear(&buttonType);

    IUIAutomationElementArray* array = nullptr;
    if (!condition || FAILED(area->FindAll(TreeScope_Descendants, condition, &array))) {
        if (condition) {
            condition->Release();
        }
        return icons;
    }
    condition->Release();

    int count = 0;
    array->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(array->GetElement(i, &element)) || !element) {
            continue;
        }

        if (gTrayDebugStats) {
            ++gTrayDebugStats->areas;
        }
        TrayIconInfo info = infoFromElement(element, isSystemPromoted, inOverflow);
        const QString skipReason = skipReasonForTrayCandidate(info);
        if (!skipReason.isEmpty()) {
            recordSkipReason(gTrayDebugStats, skipReason);
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(QStringLiteral("area"), info, false, skipReason);
            }
            element->Release();
            continue;
        }

        if (gTrayDebugStats) {
            gTrayDebugStats->recordCandidate(QStringLiteral("area"), info, true, {});
        }
        icons.push_back(info);
        element->Release();
    }

    array->Release();
    return icons;
}

QVector<TrayIconInfo> enumerateTrayDescendants(IUIAutomation* automation,
                                               IUIAutomationElement* root,
                                               bool inOverflow)
{
    QVector<TrayIconInfo> icons;
    if (!automation || !root) {
        return icons;
    }

    IUIAutomationCondition* trueCondition = nullptr;
    if (FAILED(automation->CreateTrueCondition(&trueCondition)) || !trueCondition) {
        return icons;
    }

    IUIAutomationElementArray* array = nullptr;
    if (FAILED(root->FindAll(TreeScope_Descendants, trueCondition, &array))) {
        trueCondition->Release();
        return icons;
    }
    trueCondition->Release();

    int count = 0;
    array->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(array->GetElement(i, &element)) || !element) {
            continue;
        }

        VARIANT value;
        VariantInit(&value);
        QString className;
        if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_ClassNamePropertyId, &value))) {
            className = variantToQString(value);
            VariantClear(&value);
        }
        QString automationId;
        if (SUCCEEDED(element->GetCurrentPropertyValue(UIA_AutomationIdPropertyId, &value))) {
            automationId = variantToQString(value);
            VariantClear(&value);
        }

        const bool looksLikeTrayIcon = className.contains(QStringLiteral("SystemTray"), Qt::CaseInsensitive)
            || automationId.contains(QStringLiteral("SystemTrayIcon"), Qt::CaseInsensitive)
            || automationId.contains(QStringLiteral("NotifyIcon"), Qt::CaseInsensitive);
        if (!looksLikeTrayIcon) {
            element->Release();
            continue;
        }

        TrayIconInfo info = infoFromElement(element, false, inOverflow);
        if (gTrayDebugStats) {
            ++gTrayDebugStats->descendants;
        }
        const QString skipReason = skipReasonForTrayCandidate(info, true);
        if (skipReason.isEmpty()) {
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(QStringLiteral("descendant"), info, true, {});
            }
            icons.push_back(info);
        } else {
            recordSkipReason(gTrayDebugStats, skipReason);
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(QStringLiteral("descendant"), info, false, skipReason);
            }
        }
        element->Release();
    }

    array->Release();
    return icons;
}

QVector<TrayIconInfo> deduplicateIcons(const QVector<TrayIconInfo>& icons)
{
    QVector<TrayIconInfo> unique;
    QSet<QString> seen;
    for (const TrayIconInfo& info : icons) {
        if (seen.contains(info.stableId)) {
            continue;
        }
        seen.insert(info.stableId);
        unique.push_back(info);
    }
    return unique;
}

QString normalizedTooltipLabel(const QString& tooltip);

bool iconMatchesTarget(const TrayIconInfo& candidate, const TrayIconInfo& target)
{
    if (!target.stableId.isEmpty() && candidate.stableId == target.stableId) {
        return true;
    }

    const QString candidateLabel = normalizedTooltipLabel(candidate.tooltip);
    const QString targetLabel = normalizedTooltipLabel(target.tooltip);
    return !candidateLabel.isEmpty()
        && candidateLabel.compare(targetLabel, Qt::CaseInsensitive) == 0;
}

IUIAutomationElement* findAutomationIdElementMatching(IUIAutomation* automation,
                                                       IUIAutomationElement* root,
                                                       const wchar_t* automationId,
                                                       const TrayIconInfo& target,
                                                       bool inOverflow)
{
    if (!automation || !root || !automationId) {
        return nullptr;
    }

    VARIANT idVar;
    VariantInit(&idVar);
    idVar.vt = VT_BSTR;
    idVar.bstrVal = SysAllocString(automationId);

    IUIAutomationCondition* condition = nullptr;
    automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, idVar, &condition);
    VariantClear(&idVar);
    if (!condition) {
        return nullptr;
    }

    IUIAutomationElementArray* array = nullptr;
    if (FAILED(root->FindAll(TreeScope_Descendants, condition, &array))) {
        condition->Release();
        return nullptr;
    }
    condition->Release();

    IUIAutomationElement* match = nullptr;
    int count = 0;
    array->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(array->GetElement(i, &element)) || !element) {
            continue;
        }

        const TrayIconInfo candidate = infoFromElement(element, false, inOverflow);
        if (!isSkippableTrayButton(candidate) && iconMatchesTarget(candidate, target)) {
            match = element;
            break;
        }
        element->Release();
    }
    array->Release();
    return match;
}

bool canQueryIconFromNotifyHwnd(const TrayIconInfo& info)
{
    if (info.nativeWindowHandle == 0) {
        return false;
    }
    if (info.inOverflow) {
        return false;
    }
    if (info.automationId.compare(QStringLiteral("NotifyItemIcon"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(info.nativeWindowHandle);
    wchar_t className[256] = {};
    if (GetClassNameW(hwnd, className, 256) == 0) {
        return false;
    }
    const QString windowClass = QString::fromWCharArray(className);

    static const QStringList kBlockedClasses = {
        QStringLiteral("Windows.UI.Core.CoreWindow"),
        QStringLiteral("TopLevelWindowForOverflowXamlIsland"),
        QStringLiteral("NotifyIconOverflowWindow"),
        QStringLiteral("XamlExplorerHostIslandWindow"),
    };
    for (const QString& blocked : kBlockedClasses) {
        if (windowClass.contains(blocked, Qt::CaseInsensitive)) {
            return false;
        }
    }

    static const QStringList kAllowedClasses = {
        QStringLiteral("Electron_NotifyIconHostWindow"),
        QStringLiteral("Chrome_StatusTrayWindow"),
        QStringLiteral("Qt51519TrayIconMessageWindowClass"),
        QStringLiteral("Qt6TrayIconMessageWindowClass"),
    };
    for (const QString& allowed : kAllowedClasses) {
        if (windowClass.contains(allowed, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return info.automationId.compare(QStringLiteral("SystemTrayIcon"), Qt::CaseInsensitive) == 0;
}

QString normalizedTooltipLabel(const QString& tooltip)
{
    return tooltip.section(QLatin1Char('\n'), 0, 0).trimmed();
}

bool isHostProcessBaseName(const QString& baseName)
{
    static const QStringList kHostNames = {
        QStringLiteral("explorer"),
        QStringLiteral("shellexperiencehost"),
        QStringLiteral("svchost"),
        QStringLiteral("dllhost"),
        QStringLiteral("runtimebroker"),
        QStringLiteral("searchhost"),
        QStringLiteral("startmenuexperiencehost"),
        QStringLiteral("textinputhost"),
        QStringLiteral("applicationframehost"),
        QStringLiteral("systemsettings"),
        QStringLiteral("sihost"),
        QStringLiteral("taskhostw"),
        QStringLiteral("ctfmon"),
    };
    return kHostNames.contains(baseName);
}

QString resolveExePathFromTooltip(const QString& tooltip)
{
    const QString needle = normalizedTooltipLabel(tooltip).toLower();
    if (needle.isEmpty()) {
        return {};
    }

    const QStringList words = needle.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    QString bestMatch;
    int bestScore = 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            const DWORD pid = entry.th32ProcessID;
            if (pid == 0) {
                continue;
            }

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!process) {
                continue;
            }

            wchar_t pathBuffer[MAX_PATH] = {};
            DWORD pathSize = MAX_PATH;
            if (!QueryFullProcessImageNameW(process, 0, pathBuffer, &pathSize)) {
                CloseHandle(process);
                continue;
            }
            CloseHandle(process);

            const QString exePath = QString::fromWCharArray(pathBuffer);
            if (!isUsableExecutableIconPath(exePath)) {
                continue;
            }

            const QFileInfo fileInfo(exePath);
            const QString baseName = fileInfo.completeBaseName().toLower();
            const QString fileName = fileInfo.fileName().toLower();
            if (isHostProcessBaseName(baseName)) {
                continue;
            }

            auto considerMatch = [&](const QString& token, int scoreBonus) {
                if (token.length() < 3) {
                    return;
                }
                const QString tokenLower = token.toLower();
                if (baseName.contains(tokenLower) || fileName.contains(tokenLower)
                    || tokenLower.contains(baseName)) {
                    const int score = tokenLower.length() + scoreBonus;
                    if (score > bestScore) {
                        bestScore = score;
                        bestMatch = exePath;
                    }
                }
            };

            considerMatch(needle, 4);
            for (const QString& word : words) {
                considerMatch(word, 0);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return bestMatch;
}

} // namespace

struct TrayIconEnumerator::TrayUiaState
{
    IUIAutomation* automation = nullptr;
};

TrayIconEnumerator::TrayIconEnumerator()
    : m_uia(new TrayUiaState())
{
}

TrayIconEnumerator::~TrayIconEnumerator()
{
    releaseAutomation();
    delete m_uia;
    m_uia = nullptr;
}

void TrayIconEnumerator::setTrayOnScreenScope(TrayOnScreenScope scope)
{
    m_trayOnScreenScope = std::move(scope);
}

void TrayIconEnumerator::setGuiInvoker(GuiInvoker invoker)
{
    m_guiInvoker = std::move(invoker);
}

void TrayIconEnumerator::setDebugEnabled(bool enabled)
{
    m_debugEnabled = enabled;
}

bool TrayIconEnumerator::ensureAutomation()
{
    if (m_uia->automation) {
        return true;
    }

    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IUIAutomation),
                                  reinterpret_cast<void**>(&m_uia->automation));
    return SUCCEEDED(hr) && m_uia->automation;
}

void TrayIconEnumerator::releaseAutomation()
{
    if (m_uia && m_uia->automation) {
        m_uia->automation->Release();
        m_uia->automation = nullptr;
    }
}

QString TrayIconEnumerator::lastEnumerateLog() const
{
    return m_lastEnumerateLog;
}

QString TrayIconEnumerator::lastDebugLog() const
{
    return m_lastDebugLog;
}

QString TrayIconEnumerator::processExePath(quint32 processId) const
{
    if (processId == 0) {
        return {};
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!process) {
        return {};
    }

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    QString result;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        result = QString::fromWCharArray(path);
    }
    CloseHandle(process);
    return result;
}

QString TrayIconEnumerator::resolveExePathForIcon(const TrayIconInfo& info) const
{
    if (info.processId != 0) {
        const QString fromPid = processExePath(info.processId);
        if (isUsableExecutableIconPath(fromPid)) {
            const QFileInfo fileInfo(fromPid);
            if (!isHostProcessBaseName(fileInfo.completeBaseName().toLower())) {
                return fromPid;
            }
        }
    }

    const QString fromTooltip = resolveExePathFromTooltip(info.tooltip);
    if (isUsableExecutableIconPath(fromTooltip)) {
        return fromTooltip;
    }

    return {};
}

IUIAutomationElement* TrayIconEnumerator::trayRootElement()
{
    if (!ensureAutomation()) {
        return nullptr;
    }

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!tray) {
        return nullptr;
    }

    IUIAutomationElement* trayElement = nullptr;
    if (FAILED(m_uia->automation->ElementFromHandle(reinterpret_cast<UIA_HWND>(tray), &trayElement))) {
        return nullptr;
    }
    return trayElement;
}

IUIAutomationElement* TrayIconEnumerator::desktopRootElement()
{
    if (!ensureAutomation()) {
        return nullptr;
    }

    IUIAutomationElement* desktop = nullptr;
    if (FAILED(m_uia->automation->GetRootElement(&desktop))) {
        return nullptr;
    }
    return desktop;
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateAreaInSubtree(IUIAutomationElement* root,
                                                                 const wchar_t* areaName,
                                                                 bool isSystemPromoted,
                                                                 bool inOverflow)
{
    QVector<TrayIconInfo> icons;
    if (!root || !areaName || !ensureAutomation()) {
        return icons;
    }

    IUIAutomationElement* area = findByNameInSubtree(m_uia->automation, root, areaName);
    if (!area) {
        return icons;
    }

    icons = enumerateButtonsInArea(m_uia->automation, area, isSystemPromoted, inOverflow);
    for (TrayIconInfo& info : icons) {
        info.exePath = resolveExePathForIcon(info);
    }
    area->Release();
    return icons;
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateWin11SystemTrayIcons(IUIAutomationElement* trayRoot)
{
    QVector<TrayIconInfo> icons;
    if (trayRoot) {
        icons = enumerateElementsByAutomationId(m_uia->automation,
                                                trayRoot,
                                                L"SystemTrayIcon",
                                                false,
                                                &TrayDebugStats::uiaRoot,
                                                QStringLiteral("uiaRoot"));
    }

    IUIAutomationElement* desktopRoot = desktopRootElement();
    if (desktopRoot) {
        icons += enumerateElementsByAutomationId(m_uia->automation,
                                                 desktopRoot,
                                                 L"SystemTrayIcon",
                                                 false,
                                                 &TrayDebugStats::uiaDesktop,
                                                 QStringLiteral("desktop"));
        desktopRoot->Release();
    }

    return deduplicateIcons(icons);
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateWin11OverflowIcons()
{
    QVector<TrayIconInfo> icons;
    if (!ensureAutomation()) {
        return icons;
    }

    const HWND overflowWindow = findOverflowXamlIslandWindow();
    if (overflowWindow) {
        IUIAutomationElement* overflowRoot = nullptr;
        if (SUCCEEDED(m_uia->automation->ElementFromHandle(reinterpret_cast<UIA_HWND>(overflowWindow),
                                                             &overflowRoot))
            && overflowRoot) {
            icons = enumerateElementsByAutomationId(m_uia->automation, overflowRoot, L"NotifyItemIcon", true);
            overflowRoot->Release();
        }
    }

    if (icons.isEmpty()) {
        IUIAutomationElement* desktopRoot = desktopRootElement();
        if (desktopRoot) {
            icons = enumerateElementsByAutomationId(m_uia->automation, desktopRoot, L"NotifyItemIcon", true);
            desktopRoot->Release();
        }
    }

    for (TrayIconInfo& info : icons) {
        info.exePath = resolveExePathForIcon(info);
    }
    return deduplicateIcons(icons);
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateTraySubtree(bool includeOverflow)
{
    QVector<TrayIconInfo> icons;
    IUIAutomationElement* trayRoot = trayRootElement();

    if (trayRoot) {
        icons = enumerateWin11SystemTrayIcons(trayRoot);
    } else {
        icons = enumerateWin11SystemTrayIcons(nullptr);
    }

    static const struct AreaSpec {
        const wchar_t* name;
        bool systemPromoted;
    } kAreas[] = {
        { L"User Promoted Notification Area", false },
        { L"System Promoted Notification Area", true },
        { L"Notification Area", false },
    };

    if (icons.isEmpty() && trayRoot) {
        for (const AreaSpec& area : kAreas) {
            icons += enumerateAreaInSubtree(trayRoot, area.name, area.systemPromoted, false);
        }
    }

    if (icons.isEmpty() && trayRoot) {
        icons += enumerateTrayDescendants(m_uia->automation, trayRoot, false);
    }

    if (trayRoot) {
        trayRoot->Release();
    }

    if (includeOverflow) {
        icons += enumerateOverflow();
    }

    return deduplicateIcons(icons);
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateWin32TrayIcons()
{
    QVector<TrayIconInfo> icons;

    const auto appendIfValid = [&](const TrayIconInfo& candidate, const QString& source) {
        if (candidate.nativeWindowHandle == 0) {
            recordSkipReason(gTrayDebugStats, QStringLiteral("empty"));
            return;
        }
        QString skipReason = skipReasonForTrayCandidate(candidate);
        if (skipReason.isEmpty()
            && candidate.tooltip.isEmpty()
            && !isNotifyIconHostClassName(candidate.className)) {
            skipReason = QStringLiteral("empty");
        }
        if (!skipReason.isEmpty()) {
            recordSkipReason(gTrayDebugStats, skipReason);
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(source, candidate, false, skipReason);
            }
            return;
        }

        TrayIconInfo info = candidate;
        info.exePath = resolveExePathForIcon(info);
        if (gTrayDebugStats) {
            gTrayDebugStats->recordCandidate(source, info, true, {});
        }
        icons.push_back(info);
    };

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        HWND notify = FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr);
        HWND pager = notify ? FindWindowExW(notify, nullptr, L"SysPager", nullptr) : nullptr;
        HWND toolbar = pager ? FindWindowExW(pager, nullptr, L"ToolbarWindow32", nullptr) : nullptr;
        if (toolbar) {
            const LRESULT buttonCount = SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0);
            for (LRESULT i = 0; i < buttonCount; ++i) {
                TBBUTTONINFOW buttonInfo = {};
                buttonInfo.cbSize = sizeof(TBBUTTONINFOW);
                if (SendMessageW(toolbar, TB_GETBUTTONINFOW, static_cast<WPARAM>(i),
                                 reinterpret_cast<LPARAM>(&buttonInfo))
                    == FALSE) {
                    continue;
                }

                wchar_t text[512] = {};
                const LRESULT textLen = SendMessageW(toolbar, TB_GETBUTTONTEXTW, static_cast<WPARAM>(i),
                                                     reinterpret_cast<LPARAM>(text));
                if (textLen <= 0) {
                    continue;
                }

                if (gTrayDebugStats) {
                    ++gTrayDebugStats->win32Toolbar;
                }
                TrayIconInfo info;
                info.nativeWindowHandle = reinterpret_cast<quintptr>(toolbar);
                info.tooltip = QString::fromWCharArray(text).trimmed();
                info.automationId = QStringLiteral("ToolbarButton");
                info.className = QStringLiteral("ToolbarWindow32");
                info.stableId = QStringLiteral("toolbar:%1:%2").arg(i).arg(info.tooltip);
                appendIfValid(info, QStringLiteral("win32Toolbar"));
            }
        }
    }

    struct EnumHostContext {
        QVector<TrayIconInfo>* icons = nullptr;
    };
    EnumHostContext hostContext = { &icons };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<EnumHostContext*>(lParam);
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }

        const QString windowClass = QString::fromWCharArray(className);
        if (!isNotifyIconHostClassName(windowClass)) {
            if (context && context->icons && gTrayDebugStats
                && (windowClass.contains(QStringLiteral("Tray"), Qt::CaseInsensitive)
                    || windowClass.contains(QStringLiteral("Notify"), Qt::CaseInsensitive)
                    || windowClass.contains(QStringLiteral("Icon"), Qt::CaseInsensitive))) {
                ++gTrayDebugStats->skippedUnknownHost;
            }
            return TRUE;
        }

        if (gTrayDebugStats) {
            ++gTrayDebugStats->win32Hosts;
        }
        TrayIconInfo info = infoFromNotifyHostHwnd(hwnd);
        if (info.nativeWindowHandle == 0) {
            recordSkipReason(gTrayDebugStats, QStringLiteral("empty"));
            return TRUE;
        }
        const QString skipReason = skipReasonForTrayCandidate(info);
        if (!skipReason.isEmpty()) {
            recordSkipReason(gTrayDebugStats, skipReason);
            if (gTrayDebugStats) {
                gTrayDebugStats->recordCandidate(QStringLiteral("win32Host"), info, false, skipReason);
            }
            return TRUE;
        }

        if (gTrayDebugStats) {
            TrayIconInfo debugInfo = info;
            gTrayDebugStats->recordCandidate(QStringLiteral("win32Host"), debugInfo, true, {});
        }
        context->icons->push_back(info);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hostContext));

    for (TrayIconInfo& info : icons) {
        if (info.exePath.isEmpty()) {
            info.exePath = resolveExePathForIcon(info);
        }
    }

    m_lastWin32IconCount = icons.size();
    return deduplicateIcons(icons);
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerate(bool includeOverflow)
{
    Q_UNUSED(includeOverflow);

    m_overflowEnumerateDetail.clear();
    m_lastDebugLog.clear();
    m_lastWin32IconCount = 0;
    const qint64 startedMs = QDateTime::currentMSecsSinceEpoch();

    TrayDebugStats debugStats;
    gTrayDebugStats = m_debugEnabled ? &debugStats : nullptr;

    QVector<TrayIconInfo> icons = enumerateTraySubtree(false);
    icons = deduplicateIcons(icons);
    bool usedWin32Fallback = false;

    if (icons.isEmpty()) {
        icons = enumerateWin32TrayIcons();
        icons = deduplicateIcons(icons);
        usedWin32Fallback = true;
    } else if (m_debugEnabled) {
        // Diagnostic-only when UIA already produced icons. This does not feed
        // the model and does not open the native overflow panel.
        enumerateWin32TrayIcons();
    }

    const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - startedMs;

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT trayRect = {};
    if (tray) {
        GetWindowRect(tray, &trayRect);
    }
    const bool offscreen = trayRect.left < -1000 || trayRect.top < -1000;

    m_lastEnumerateLog = QStringLiteral("Tray refresh: %1 icons in %2ms (offscreen=%3)")
                           .arg(icons.size())
                           .arg(elapsedMs)
                           .arg(offscreen ? QStringLiteral("true") : QStringLiteral("false"));
    if (usedWin32Fallback) {
        m_lastEnumerateLog += QStringLiteral(" [win32=%1]").arg(m_lastWin32IconCount);
    }
    if (m_debugEnabled) {
        QStringList lines;
        lines << debugStats.summary(icons.size());
        lines << debugStats.skipSummary();
        if (!debugStats.candidates.isEmpty()) {
            lines << QStringLiteral("Tray debug candidates: %1")
                         .arg(debugStats.candidates.join(QStringLiteral(" | ")));
        }
        m_lastDebugLog = lines.join(QStringLiteral("\n"));
    }
    gTrayDebugStats = nullptr;
    return icons;
}

QVector<TrayIconInfo> TrayIconEnumerator::collectOverflowIcons(QString* detailLog)
{
    QVector<TrayIconInfo> icons;
    if (!ensureAutomation()) {
        if (detailLog) {
            *detailLog = QStringLiteral("overflow automation unavailable");
        }
        return icons;
    }

    POINT savedCursor = {};
    GetCursorPos(&savedCursor);
    const auto restoreCursor = [&]() {
        SetCursorPos(savedCursor.x, savedCursor.y);
    };

    const auto runAttempt = [&]() {
        openOverflowPanel(false);

        icons = enumerateWin11OverflowIcons();

        static const wchar_t* kOverflowNames[] = {
            L"Overflow Notification Area",
            L"Notification Overflow",
        };

        if (icons.isEmpty()) {
            IUIAutomationElement* freshTrayRoot = trayRootElement();
            for (const wchar_t* name : kOverflowNames) {
                if (freshTrayRoot) {
                    icons = enumerateAreaInSubtree(freshTrayRoot, name, false, true);
                }
                if (!icons.isEmpty()) {
                    break;
                }
            }
            if (freshTrayRoot) {
                freshTrayRoot->Release();
            }
        }

        closeOverflowPanel();
    };

    runAttempt();
    restoreCursor();

    bool trayWasOffscreen = false;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT trayRect = {};
    if (taskbar && GetWindowRect(taskbar, &trayRect)) {
        trayWasOffscreen = trayRect.left < -1000 || trayRect.top < -1000;
    }

    if (detailLog) {
        *detailLog = QStringLiteral("chevron=%1 panel=%2 notify=%3 win32=%4 trayOffscreen=%5")
                         .arg(m_lastChevronLocated ? QStringLiteral("yes") : QStringLiteral("no"))
                         .arg(m_lastOverflowPanelOpen ? QStringLiteral("1") : QStringLiteral("0"))
                         .arg(icons.size())
                         .arg(m_lastWin32IconCount)
                         .arg(trayWasOffscreen ? QStringLiteral("yes") : QStringLiteral("no"));
    }

    return deduplicateIcons(icons);
}

QVector<TrayIconInfo> TrayIconEnumerator::enumerateOverflow()
{
    QString detail;
    QVector<TrayIconInfo> icons = collectOverflowIcons(&detail);
    m_overflowEnumerateDetail = detail;
    return icons;
}

bool TrayIconEnumerator::clickAtScreenPoint(int x, int y, bool rightButton)
{
    SetCursorPos(x, y);

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = rightButton ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = rightButton ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

QString TrayIconEnumerator::lastInteractionDetail() const
{
    return m_lastInteractionDetail;
}

bool TrayIconEnumerator::clickTaskbarChevronFallback()
{
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar) {
        return false;
    }

    RECT rect = {};
    if (!GetWindowRect(taskbar, &rect)) {
        return false;
    }

    const int x = rect.right - 40;
    const int y = (rect.top + rect.bottom) / 2;
    return clickAtScreenPoint(x, y, false);
}

bool TrayIconEnumerator::openOverflowViaKeyboard(IUIAutomation* automation, IUIAutomationElement* chevron)
{
    return openOverflowViaKeyboardImpl(automation, chevron);
}

bool TrayIconEnumerator::waitForOverflowPanel(int maxMs)
{
    const int stepMs = 100;
    for (int elapsed = 0; elapsed < maxMs; elapsed += stepMs) {
        if (!ensureAutomation()) {
            return false;
        }
        if (!enumerateWin11OverflowIcons().isEmpty()) {
            return true;
        }
        const HWND overflowHwnd = findOverflowXamlIslandWindow();
        if (overflowHwnd && IsWindowVisible(overflowHwnd)) {
            // HWND visible but NotifyItemIcon not ready yet — keep polling.
        }
        Sleep(stepMs);
    }
    return ensureAutomation() && !enumerateWin11OverflowIcons().isEmpty();
}

bool TrayIconEnumerator::openOverflowChevron(IUIAutomation* automation, bool allowMouseFallback)
{
    if (!automation) {
        m_lastChevronLocated = false;
        return false;
    }

    IUIAutomationElement* chevron = nullptr;
    IUIAutomationElement* trayRoot = nullptr;
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        if (SUCCEEDED(automation->ElementFromHandle(reinterpret_cast<UIA_HWND>(tray), &trayRoot))
            && trayRoot) {
            chevron = findOverflowChevronElement(automation, trayRoot);
        }
    }
    if (!chevron) {
        IUIAutomationElement* desktopRoot = nullptr;
        if (SUCCEEDED(automation->GetRootElement(&desktopRoot)) && desktopRoot) {
            chevron = findOverflowChevronElement(automation, desktopRoot);
            desktopRoot->Release();
        }
    }
    if (trayRoot) {
        trayRoot->Release();
    }

    m_lastChevronLocated = chevron != nullptr;

    bool opened = false;
    if (chevron) {
        opened = tryActivateElement(chevron);
        if (!opened && !allowMouseFallback) {
            opened = openOverflowViaKeyboard(automation, chevron);
        } else if (!opened && allowMouseFallback) {
            const TrayIconInfo chevronInfo = infoFromElement(chevron, false, false);
            if (!chevronInfo.boundingRect.isEmpty()) {
                opened = clickAtScreenPoint(chevronInfo.boundingRect.center().x(),
                                            chevronInfo.boundingRect.center().y(),
                                            false);
            }
        }
        chevron->Release();
    } else if (!allowMouseFallback) {
        opened = openOverflowViaKeyboard(automation, nullptr);
    } else if (allowMouseFallback) {
        opened = clickTaskbarChevronFallback();
    }

    return opened;
}

bool TrayIconEnumerator::openOverflowPanel(bool allowMouseFallback)
{
    if (!ensureAutomation()) {
        return false;
    }

    const QVector<TrayIconInfo> alreadyOpen = enumerateWin11OverflowIcons();
    if (!alreadyOpen.isEmpty()) {
        return true;
    }

    Sleep(150);

    const auto runChevronOpenOnGui = [this, allowMouseFallback]() -> bool {
        struct CoInitScope {
            CoInitScope() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
            ~CoInitScope() { CoUninitialize(); }
        } coInit;

        IUIAutomation* automation = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                    __uuidof(IUIAutomation),
                                    reinterpret_cast<void**>(&automation)))
            || !automation) {
            return false;
        }

        const bool opened = openOverflowChevron(automation, allowMouseFallback);
        automation->Release();
        return opened;
    };

    if (m_guiInvoker) {
        m_guiInvoker([&]() {
            runChevronOpenOnGui();
        });
    } else {
        openOverflowChevron(m_uia->automation, allowMouseFallback);
    }

    const bool panelReady = waitForOverflowPanel(2000);
    if (panelReady) {
        Sleep(150);
    }

    const QVector<TrayIconInfo> probe = enumerateWin11OverflowIcons();
    m_lastOverflowPanelOpen = !probe.isEmpty();
    return m_lastOverflowPanelOpen;
}

void TrayIconEnumerator::closeOverflowPanel()
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_ESCAPE;
    SendInput(1, &input, sizeof(INPUT));
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
    Sleep(100);
}

bool TrayIconEnumerator::sendShiftF10ContextMenu()
{
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_SHIFT;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_F10;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_F10;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_SHIFT;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, inputs, sizeof(INPUT)) == 4;
}

IUIAutomationElement* TrayIconEnumerator::findOverflowIconElement(const TrayIconInfo& icon)
{
    if (!ensureAutomation()) {
        return nullptr;
    }

    const auto searchRoot = [&](IUIAutomationElement* root) -> IUIAutomationElement* {
        return findAutomationIdElementMatching(m_uia->automation, root, L"NotifyItemIcon", icon, true);
    };

    const HWND overflowWindow = findOverflowXamlIslandWindow();
    if (overflowWindow) {
        IUIAutomationElement* overflowRoot = nullptr;
        if (SUCCEEDED(m_uia->automation->ElementFromHandle(reinterpret_cast<UIA_HWND>(overflowWindow),
                                                             &overflowRoot))
            && overflowRoot) {
            IUIAutomationElement* found = searchRoot(overflowRoot);
            overflowRoot->Release();
            if (found) {
                return found;
            }
        }
    }

    IUIAutomationElement* desktopRoot = desktopRootElement();
    if (!desktopRoot) {
        return nullptr;
    }

    IUIAutomationElement* found = searchRoot(desktopRoot);
    desktopRoot->Release();
    return found;
}

IUIAutomationElement* TrayIconEnumerator::findVisibleTrayIconElement(const TrayIconInfo& icon)
{
    if (!ensureAutomation()) {
        return nullptr;
    }

    const auto searchRoot = [&](IUIAutomationElement* root) -> IUIAutomationElement* {
        return findAutomationIdElementMatching(m_uia->automation, root, L"SystemTrayIcon", icon, false);
    };

    IUIAutomationElement* trayRoot = trayRootElement();
    if (trayRoot) {
        IUIAutomationElement* found = searchRoot(trayRoot);
        trayRoot->Release();
        if (found) {
            return found;
        }
    }

    IUIAutomationElement* desktopRoot = desktopRootElement();
    if (!desktopRoot) {
        return nullptr;
    }

    IUIAutomationElement* found = searchRoot(desktopRoot);
    desktopRoot->Release();
    return found;
}

bool TrayIconEnumerator::interactWithElement(IUIAutomationElement* element,
                                             bool rightButton,
                                             bool inOverflow)
{
    Q_UNUSED(inOverflow);

    if (!element) {
        return false;
    }

    if (!rightButton && invokeElement(element)) {
        return true;
    }

    if (rightButton) {
        if (SUCCEEDED(element->SetFocus())) {
            Sleep(50);
            if (sendShiftF10ContextMenu()) {
                return true;
            }
        }
    }

    return false;
}

bool TrayIconEnumerator::interactWithIcon(const TrayIconInfo& icon, bool rightButton)
{
    m_lastInteractionDetail.clear();
    if (!ensureAutomation()) {
        m_lastInteractionDetail = QStringLiteral("automation unavailable");
        return false;
    }

    const bool isOverflow = icon.inOverflow
        || icon.automationId.compare(QStringLiteral("NotifyItemIcon"), Qt::CaseInsensitive) == 0;

    bool ok = false;
    if (isOverflow) {
        m_lastInteractionDetail = QStringLiteral("overflow icons disabled in visible-only tray mode");
        return false;
    }

    IUIAutomationElement* element = findVisibleTrayIconElement(icon);
    if (element) {
        ok = interactWithElement(element, rightButton, false);
        element->Release();
        m_lastInteractionDetail = QStringLiteral("visible + %1 %2")
                                      .arg(rightButton ? QStringLiteral("shift-f10")
                                                       : QStringLiteral("invoke"))
                                      .arg(ok ? QStringLiteral("ok") : QStringLiteral("failed"));
        return ok;
    }

    if (canQueryIconFromNotifyHwnd(icon)) {
        IUIAutomationElement* hwndElement = nullptr;
        if (SUCCEEDED(m_uia->automation->ElementFromHandle(
                reinterpret_cast<UIA_HWND>(icon.nativeWindowHandle), &hwndElement))
            && hwndElement) {
            ok = interactWithElement(hwndElement, rightButton, false);
            hwndElement->Release();
            if (ok) {
                m_lastInteractionDetail = QStringLiteral("hwnd + %1 ok")
                                            .arg(rightButton ? QStringLiteral("shift-f10")
                                                             : QStringLiteral("invoke"));
                return true;
            }
        }
    }

    m_lastInteractionDetail = QStringLiteral("failed");
    return false;
}

bool TrayIconEnumerator::activate(const TrayIconInfo& icon)
{
    bool result = false;
    result = interactWithIcon(icon, false);
    return result;
}

bool TrayIconEnumerator::showContextMenu(const TrayIconInfo& icon)
{
    bool result = false;
    result = interactWithIcon(icon, true);
    return result;
}

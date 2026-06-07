#include "PinnedTaskbarResolver.h"

#include <QDir>
#include <QFileInfo>

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <objbase.h>

PinnedTaskbarResolver::PinnedTaskbarResolver(QObject* parent)
    : QObject(parent)
{
}

QString PinnedTaskbarResolver::pinnedTaskbarDir() const
{
    const QString appData = qEnvironmentVariable("APPDATA");
    return QDir(appData).filePath("Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar");
}

PinnedShortcutEntry PinnedTaskbarResolver::resolveShortcut(const QString& shortcutPath) const
{
    PinnedShortcutEntry entry;
    entry.shortcutPath = shortcutPath;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool mustUninit = SUCCEEDED(hr);
    const bool comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comReady) {
        return entry;
    }

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || !shellLink) {
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr) || !persistFile) {
        shellLink->Release();
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    hr = persistFile->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), STGM_READ);
    if (FAILED(hr)) {
        persistFile->Release();
        shellLink->Release();
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    wchar_t target[MAX_PATH * 4] = {};
    WIN32_FIND_DATAW findData = {};
    hr = shellLink->GetPath(target, static_cast<int>(std::size(target)), &findData, SLGP_RAWPATH);
    if (SUCCEEDED(hr) && target[0] != L'\0') {
        entry.targetPath = QString::fromWCharArray(target);
    }

    wchar_t arguments[MAX_PATH * 4] = {};
    hr = shellLink->GetArguments(arguments, static_cast<int>(std::size(arguments)));
    if (SUCCEEDED(hr) && arguments[0] != L'\0') {
        entry.arguments = QString::fromWCharArray(arguments);
    }

    wchar_t workdir[MAX_PATH * 4] = {};
    hr = shellLink->GetWorkingDirectory(workdir, static_cast<int>(std::size(workdir)));
    if (SUCCEEDED(hr) && workdir[0] != L'\0') {
        entry.workingDirectory = QString::fromWCharArray(workdir);
    }

    wchar_t iconPath[MAX_PATH * 4] = {};
    int iconIndex = 0;
    hr = shellLink->GetIconLocation(iconPath, static_cast<int>(std::size(iconPath)), &iconIndex);
    if (SUCCEEDED(hr) && iconPath[0] != L'\0') {
        entry.iconLocation = QString::fromWCharArray(iconPath);
        entry.iconIndex = iconIndex;
    }

    IPropertyStore* propertyStore = nullptr;
    hr = shellLink->QueryInterface(IID_PPV_ARGS(&propertyStore));
    if (SUCCEEDED(hr) && propertyStore) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(propertyStore->GetValue(PKEY_AppUserModel_ID, &value))) {
            if (value.vt == VT_LPWSTR && value.pwszVal) {
                entry.appUserModelId = QString::fromWCharArray(value.pwszVal);
            }
        }
        PropVariantClear(&value);
        propertyStore->Release();
    }

    persistFile->Release();
    shellLink->Release();
    if (mustUninit)
        CoUninitialize();
    return entry;
}

QList<PinnedShortcutEntry> PinnedTaskbarResolver::resolvePinnedShortcuts() const
{
    QList<PinnedShortcutEntry> results;
    const QString dirPath = pinnedTaskbarDir();
    QDir dir(dirPath);
    if (!dir.exists()) {
        return results;
    }

    const QFileInfoList shortcuts = dir.entryInfoList(QStringList() << "*.lnk", QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& shortcut : shortcuts) {
        const PinnedShortcutEntry entry = resolveShortcut(shortcut.absoluteFilePath());
        if (!entry.targetPath.isEmpty() || !entry.appUserModelId.isEmpty()) {
            results.push_back(entry);
        }
    }

    return results;
}

QStringList PinnedTaskbarResolver::resolvePinnedExecutablePaths() const
{
    QStringList results;
    const QList<PinnedShortcutEntry> shortcuts = resolvePinnedShortcuts();
    for (const PinnedShortcutEntry& shortcut : shortcuts) {
        if (!shortcut.targetPath.isEmpty() && QFileInfo::exists(shortcut.targetPath)) {
            results << shortcut.targetPath;
        }
    }

    results.removeDuplicates();
    return results;
}

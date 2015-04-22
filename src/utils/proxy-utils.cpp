#include <QtGlobal>
#include <QStringList>
#include <QUrl>
#include "utils-mac.h"

#ifdef Q_OS_WIN32
// WinHttpGetIEProxyConfigForCurrentUser is available only
// for Windows XP or later
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <winhttp.h>
#elif defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif

#include "utils/proxy-utils.h"

namespace {
enum ProxyScheme {
  Http,
  Https,
  Socks5,
  Direct,
  Invalid
};
unsigned short defaultPortNumberForScheme(ProxyScheme scheme) {
    switch(scheme) {
        case Http:
            return 80;
        case Https:
            return 443;
        case Socks5:
            return 1080;
        case Direct:
        case Invalid:
            break;
    }
    return 0;
}
QNetworkProxy getFirstNetworkProxyFromString(const QString &proxy_uri_list) {
    // only pick the first one;
    QString proxy_uri = proxy_uri_list.split(",").front();
    // proxy [scheme://]host[:port]

    // do we find a :// here?
    int index = proxy_uri.indexOf(':');
    ProxyScheme scheme = Direct;
    if (index != -1 && (proxy_uri.size() - index) >= 3 &&
        proxy_uri[index+1] == '/' && proxy_uri[index+2] == '/') {
        QString scheme_lower = proxy_uri.left(index).toLower();
        if (scheme_lower == "socks" || scheme_lower == "socks5") {
            scheme = Socks5;
        } else if (scheme_lower == "http") {
            scheme = Http;
        } else if (scheme_lower == "https") {
            scheme = Https;
        } else if (scheme_lower == "direct") {
            scheme = Direct;
        } else {
            scheme = Invalid;
        }
        // by pass the first
        proxy_uri = proxy_uri.right(proxy_uri.size() - index - 3);
    }
    if (scheme == Invalid || scheme == Direct)
        return QNetworkProxy();
    // eliminate the trailing whites
    proxy_uri = proxy_uri.trimmed();
    // proxy host[:port]
    QUrl proxy_host_and_port;
    // default scheme
    proxy_host_and_port.setScheme("http");
    proxy_host_and_port.setPort(defaultPortNumberForScheme(scheme));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    proxy_host_and_port.setAuthority(proxy_uri, QUrl::StrictMode);
#else
    proxy_host_and_port.setAuthority(proxy_uri);
#endif
    if (!proxy_host_and_port.isValid())
        return QNetworkProxy();

    return QNetworkProxy(scheme == Socks5 ? QNetworkProxy::Socks5Proxy
                                          : QNetworkProxy::HttpProxy,
                         proxy_host_and_port.host(),
                         proxy_host_and_port.port());
}
#ifdef Q_OS_WIN32
void freeIEConfig(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_config) {
  if (ie_config->lpszAutoConfigUrl)
    GlobalFree(ie_config->lpszAutoConfigUrl);
  if (ie_config->lpszProxy)
    GlobalFree(ie_config->lpszProxy);
  if (ie_config->lpszProxyBypass)
    GlobalFree(ie_config->lpszProxyBypass);
}
#elif defined(Q_OS_MAC)
bool getBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
    CFNumberRef number = utils::mac::getValueFromDictionary<CFNumberRef>(dict, key);
    if (!number)
        return default_value;

    int int_value;
    if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
        return int_value;
    else
        return default_value;
}
QNetworkProxy getProxyFromDictionary(ProxyScheme scheme,
                                     CFDictionaryRef dict,
                                     CFStringRef host_key,
                                     CFStringRef port_key) {
    CFStringRef host_ref = utils::mac::getValueFromDictionary<CFStringRef>(dict, host_key);
    CFNumberRef port_ref = utils::mac::getValueFromDictionary<CFNumberRef>(dict, port_key);
    int port;
    if (port_ref)
        CFNumberGetValue(port_ref, kCFNumberIntType, &port);
    else
        port = defaultPortNumberForScheme(scheme);
    switch(scheme) {
        case Http:
        case Https:
            return QNetworkProxy(QNetworkProxy::HttpProxy, utils::mac::getValueFromCFString(host_ref), port);
        case Socks5:
            return QNetworkProxy(QNetworkProxy::Socks5Proxy, utils::mac::getValueFromCFString(host_ref), port);
        default:
            return QNetworkProxy();
    }
}
#endif
} // anonymous namespace

void ProxyRules::parseFromString(const QString &rules) {
    type = NoRules;
    single_proxy = QNetworkProxy();
    proxy_for_http = QNetworkProxy();
    proxy_for_https = QNetworkProxy();
    proxy_for_ftp = QNetworkProxy();
    proxy_for_fallback = QNetworkProxy();

    QStringList proxy_scheme_list = rules.split(";");
    Q_FOREACH(const QString &proxy_scheme, proxy_scheme_list) {
        QStringList proxy_server_for_scheme = proxy_scheme.split("=");
        for (int i = 0; i < proxy_server_for_scheme.size(); ++i) {
            const QString &scheme_or_uri = proxy_server_for_scheme[i];
            // unexpected
            if (++i >= proxy_server_for_scheme.size()) {
                type = SingleProxy;
                single_proxy = getFirstNetworkProxyFromString(scheme_or_uri);
                return;
            }
            const QString &proxy_uri = proxy_server_for_scheme[i];
            const QString scheme_lower = scheme_or_uri.trimmed().toLower();
            if (scheme_lower == "http") {
                proxy_for_http = getFirstNetworkProxyFromString(proxy_uri);
            } else if (scheme_lower == "https") {
                proxy_for_https = getFirstNetworkProxyFromString(proxy_uri);
            } else if (scheme_lower == "ftp") {
                proxy_for_ftp = getFirstNetworkProxyFromString(proxy_uri);
            } else if (scheme_lower == "socks5" || scheme_lower == "socks") {
                proxy_for_fallback = getFirstNetworkProxyFromString(proxy_uri);
            } /* else ignore */
        }
    }
}

ProxyConfig ProxyConfig::getSystemProxyConfig() {
    ProxyConfig config;
#ifdef Q_OS_WIN32
    // refer to http://stackoverflow.com/questions/202547/how-do-i-find-out-the-browsers-proxy-settings
    // refer to https://msdn.microsoft.com/en-us/library/windows/desktop/aa384096%28v=vs.85%29.aspx
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0, 0, 0, 0};
    if (!WinHttpGetIEProxyConfigForCurrentUser(&ie_config)) {
        config.setSource(SourceSystemFailed);
        return config;
    }
    if (ie_config.fAutoDetect)
        config.setAutoDetect(true);
    // lpszProxy may be a single proxy, or a proxy per scheme. The format
    // is compatible with ProxyConfig::ProxyRules's string format.
    if (ie_config.lpszProxy)
        config.mutableProxyRules().parseFromString(QString::fromWCharArray(ie_config.lpszProxy));
    if (ie_config.lpszProxyBypass) {
        QString proxy_bypass = QString::fromWCharArray(ie_config.lpszProxyBypass);

        QStringList bypass_list = proxy_bypass.split(";, \t\n\r");
        Q_FOREACH(const QString& bypass_item, bypass_list) {
            config.mutableProxyRules().bypass_rules.push_back(bypass_item);
        }
    }
    if (ie_config.lpszAutoConfigUrl)
        config.setPacUrl(QString::fromWCharArray(ie_config.lpszAutoConfigUrl));
    freeIEConfig(&ie_config);
#elif defined(Q_OS_MAC)
    // refer to https://developer.apple.com/library/mac/documentation/Networking/Reference/SCDynamicStoreCopySpecific/index.html#//apple_ref/c/func/SCDynamicStoreCopyProxies
    // refer to http://stackoverflow.com/questions/13276195/mac-osx-how-can-i-grab-proxy-configuration-using-cocoa-or-even-pure-c-function
    CFDictionaryRef dict = SCDynamicStoreCopyProxies(NULL);
    config.setAutoDetect(getBoolFromDictionary(dict, kSCPropNetProxiesProxyAutoDiscoveryEnable, false));
    if (getBoolFromDictionary(dict, kSCPropNetProxiesProxyAutoConfigEnable, false)) {
        CFStringRef pac_url_ref = utils::mac::getValueFromDictionary<CFStringRef>(dict, kSCPropNetProxiesProxyAutoConfigURLString);
        config.setPacUrl(utils::mac::getValueFromCFString(pac_url_ref));
    }
    if (getBoolFromDictionary(dict, kSCPropNetProxiesFTPEnable, false)) {
        QNetworkProxy proxy = getProxyFromDictionary(Http, dict, kSCPropNetProxiesFTPProxy, kSCPropNetProxiesFTPPort);
        config.mutableProxyRules().type = ProxyRules::PerSchemConfig;
        config.mutableProxyRules().proxy_for_ftp = proxy;
    }
    if (getBoolFromDictionary(dict, kSCPropNetProxiesHTTPSEnable, false)) {
        QNetworkProxy proxy = getProxyFromDictionary(Https, dict, kSCPropNetProxiesHTTPSProxy, kSCPropNetProxiesHTTPSPort);
        config.mutableProxyRules().type = ProxyRules::PerSchemConfig;
        config.mutableProxyRules().proxy_for_https = proxy;
    }
    if (getBoolFromDictionary(dict, kSCPropNetProxiesHTTPEnable, false)) {
        QNetworkProxy proxy = getProxyFromDictionary(Http, dict, kSCPropNetProxiesHTTPProxy, kSCPropNetProxiesHTTPPort);
        config.mutableProxyRules().type = ProxyRules::PerSchemConfig;
        config.mutableProxyRules().proxy_for_http = proxy;
    }
    if (getBoolFromDictionary(dict, kSCPropNetProxiesSOCKSEnable, false)) {
        QNetworkProxy proxy = getProxyFromDictionary(Socks5, dict, kSCPropNetProxiesSOCKSProxy, kSCPropNetProxiesSOCKSPort);
        config.mutableProxyRules().type = ProxyRules::PerSchemConfig;
        config.mutableProxyRules().proxy_for_fallback = proxy;
    }
    CFArrayRef bypass_array_ref = utils::mac::getValueFromDictionary<CFArrayRef>(
        dict, kSCPropNetProxiesExceptionsList);
    if (bypass_array_ref) {
        CFIndex count = CFArrayGetCount(bypass_array_ref);
        for (CFIndex i = 0; i < count; ++i) {
            CFStringRef item_ref = CFCast<CFStringRef>(CFArrayGetValueAtIndex(bypass_array_ref, i));
            if (!item_ref) {
                // TODO parse the bypass rules!
                config.mutableProxyRules().bypass_rules.push_back(utils::mac::getValueFromCFString(item_ref));
            }
        }
    }
    if (getBoolFromDictionary(dict, kSCPropNetProxiesExcludeSimpleHostnames, false)) {
        config.mutableProxyRules().bypass_rules.push_back("<local>");
    }
    CFRelease(dict);
#elif defined(Q_OS_LINUX)
    // TODO add helpers to detect different linux desktops
    // TODO add supports to read network proxy config according the different linux desktops
    //
    // TODO deal with gnome2, gnome3, kde2, kde3 and etc
    // please refer to chromium source code,
    // e.g. https://github.com/Chilledheart/chromium/blob/master/net/proxy/proxy_config_service_linux.h
    // and https://github.com/Chilledheart/chromium/blob/master/net/proxy/proxy_config_service_linux.cc
#endif
    config.setSource(SourceSystem);
    return config;
}

#ifndef SEAFILE_CLIENT_PROXY_UTILS_H_
#define SEAFILE_CLIENT_PROXY_UTILS_H_
#include <QString>
#include <QUrl>
#include <vector>
#include <QNetworkProxy>
// originally from chromium's source file (net/proxy)
// modified to support qt's way
class ProxyRules {
public:
    enum ProxyType {
        NoRules,
        SingleProxy,
        PerSchemConfig
    };

    ProxyType type;

    QNetworkProxy single_proxy;

    QNetworkProxy proxy_for_http;
    QNetworkProxy proxy_for_https;
    QNetworkProxy proxy_for_ftp;

    QNetworkProxy proxy_for_fallback;
    // TODO any improvement on it?
    std::vector<QString> bypass_rules;

    // Parses the rules from a string, indicating which proxies to use.
    //
    //   proxy-uri = [<proxy-scheme>"://"]<proxy-host>[":"<proxy-port>]
    //
    //   proxy-uri-list = <proxy-uri>[","<proxy-uri-list>]
    //
    //   url-scheme = "http" | "https" | "ftp" | "socks"
    //
    //   scheme-proxies = [<url-scheme>"="]<proxy-uri-list>
    //
    //   proxy-rules = scheme-proxies[";"<scheme-proxies>]
    //
    //   we will pick only fist one from proxy-uri-list
    //
    void parseFromString(const QString &rules);
};

class ProxyConfig {
public:
    enum ProxyConfigSource {
        SourceUnknown,      // The source hasn't been set.
        SourceSystem,       // System settings (Win/Mac).
        SourceSystemFailed, // Default settings after failure to
                            // determine system settings.
        SourceGonf,         // GConf (Linux)
        SourceGsettings,    // GSettings (Linux).
        SourceKDE,          // KDE (Linux).
        SourceENV,          // Environment variables.
        SourceCustom,       // Custom settings local to the
                            // application (command line,
                            // extensions, application
                            // specific preferences, etc.)
        NumProxyConfigSoureces
    };
    static ProxyConfig getSystemProxyConfig();

    ProxyConfigSource source() { return proxy_source_; }
    void setSource(ProxyConfigSource proxy_source) { proxy_source_ = proxy_source; }

    bool autoDetect() { return auto_detect_; }
    void setAutoDetect(bool auto_detect) { auto_detect_ = auto_detect; }

    const QUrl& pacUrl() { return pac_url_; }
    void setPacUrl(const QUrl& pac_url) { pac_url_ = pac_url; }

    const ProxyRules& proxyRules() const { return proxy_rules_; }
    ProxyRules&  mutableProxyRules() { return proxy_rules_; }

private:
    ProxyConfig() : proxy_source_(SourceUnknown) {}
    ProxyConfigSource proxy_source_;
    ProxyRules proxy_rules_;
    bool auto_detect_;
    QUrl pac_url_;
};

#endif // SEAFILE_CLIENT_PROXY_UTILS_H_

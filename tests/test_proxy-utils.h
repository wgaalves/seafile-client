#ifndef TESTS_PROXY_UTILS_H
#define TESTS_PROXY_UTILS_H
#include <QObject>

class ProxyUtils : public QObject {
    Q_OBJECT
public:
    virtual ~ProxyUtils() {};

private slots:
    void getSystemProxyConfig();
};

#endif // TESTS_PROXY_UTILS_H


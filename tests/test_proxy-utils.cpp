#include "test_proxy-utils.h"
#include <QtTest/QtTest>

#include "../src/utils/proxy-utils.h"

void ProxyUtils::getSystemProxyConfig() {
    ProxyConfig config = ProxyConfig::getSystemProxyConfig();
    QVERIFY(config.source() == ProxyConfig::SourceSystem);
}

QTEST_APPLESS_MAIN(ProxyUtils)


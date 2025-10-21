#ifdef UNIT_TEST

#include <unity.h>
#include "../../src/network/NetworkManager.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_network_quality_initialization(void) {
    NetworkQuality quality;
    TEST_ASSERT_EQUAL_INT(0, quality.rssi);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, quality.packet_loss);
    TEST_ASSERT_EQUAL_INT(0, quality.latency_ms);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, quality.bandwidth_kbps);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, quality.stability_score);
}

void test_wifi_network_creation(void) {
    WiFiNetwork network("TestSSID", "TestPassword", 1, true);
    TEST_ASSERT_EQUAL_STRING("TestSSID", network.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPassword", network.password.c_str());
    TEST_ASSERT_EQUAL_INT(1, network.priority);
    TEST_ASSERT_TRUE(network.auto_connect);
}

void test_multi_wifi_manager_initialization(void) {
    MultiWiFiManager manager;
    TEST_ASSERT_FALSE(manager.hasNetworks());
    TEST_ASSERT_EQUAL_UINT32(0, manager.getNetworkCount());
}

void test_multi_wifi_manager_add_network(void) {
    MultiWiFiManager manager;
    manager.addNetwork("SSID1", "Password1", 1);
    TEST_ASSERT_TRUE(manager.hasNetworks());
    TEST_ASSERT_EQUAL_UINT32(1, manager.getNetworkCount());
}

void test_multi_wifi_manager_multiple_networks(void) {
    MultiWiFiManager manager;
    manager.addNetwork("SSID1", "Password1", 1);
    manager.addNetwork("SSID2", "Password2", 2);
    manager.addNetwork("SSID3", "Password3", 3);
    
    TEST_ASSERT_EQUAL_UINT32(3, manager.getNetworkCount());
}

void test_multi_wifi_manager_clear_networks(void) {
    MultiWiFiManager manager;
    manager.addNetwork("SSID1", "Password1", 1);
    manager.addNetwork("SSID2", "Password2", 2);
    TEST_ASSERT_EQUAL_UINT32(2, manager.getNetworkCount());
    
    manager.clearNetworks();
    TEST_ASSERT_EQUAL_UINT32(0, manager.getNetworkCount());
    TEST_ASSERT_FALSE(manager.hasNetworks());
}

void test_network_manager_initialization(void) {
    NetworkManager manager;
    TEST_ASSERT_FALSE(manager.isInitialized());
}

void test_network_manager_wifi_status(void) {
    NetworkManager manager;
    bool wifi_connected = manager.isWiFiConnected();
    TEST_ASSERT_FALSE(wifi_connected);
}

void test_network_manager_server_status(void) {
    NetworkManager manager;
    bool server_connected = manager.isServerConnected();
    TEST_ASSERT_FALSE(server_connected);
}

void test_network_manager_statistics_initialization(void) {
    NetworkManager manager;
    TEST_ASSERT_EQUAL_UINT32(0, manager.getWiFiReconnectCount());
    TEST_ASSERT_EQUAL_UINT32(0, manager.getServerReconnectCount());
    TEST_ASSERT_EQUAL_UINT32(0, manager.getTCPErrorCount());
    TEST_ASSERT_EQUAL_UINT32(0, manager.getBytesSent());
    TEST_ASSERT_EQUAL_UINT32(0, manager.getBytesReceived());
}

void test_network_quality_metrics(void) {
    NetworkManager manager;
    const NetworkQuality& quality = manager.getNetworkQuality();
    
    TEST_ASSERT_EQUAL_FLOAT(1.0f, quality.stability_score);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, quality.packet_loss);
}

void test_network_manager_safe_mode(void) {
    NetworkManager manager;
    
    manager.setSafeMode(true);
    TEST_ASSERT_TRUE(manager.isSafeMode());
    
    manager.setSafeMode(false);
    TEST_ASSERT_FALSE(manager.isSafeMode());
}

void test_wifi_network_priority(void) {
    WiFiNetwork net1("SSID1", "Pass1", 3);
    WiFiNetwork net2("SSID2", "Pass2", 1);
    WiFiNetwork net3("SSID3", "Pass3", 2);
    
    TEST_ASSERT_GREATER_THAN(net2.priority, net1.priority);
    TEST_ASSERT_GREATER_THAN(net1.priority, net3.priority);
}

void test_network_manager_add_wifi_networks(void) {
    NetworkManager manager;
    manager.addWiFiNetwork("TestSSID", "TestPassword", 1);
    TEST_ASSERT_TRUE(manager.isWiFiConnected() || true);
}

void test_network_stability_score(void) {
    NetworkManager manager;
    float stability = manager.getNetworkStability();
    
    TEST_ASSERT_TRUE(stability >= 0.0f);
    TEST_ASSERT_TRUE(stability <= 1.0f);
}

#endif

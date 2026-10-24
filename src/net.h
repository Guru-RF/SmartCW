#pragma once
#include <Arduino.h>

// USB Ethernet (CDC-NCM): SmartCW is 192.168.7.1 (config.ip), hands the
// computer an address with a small DHCP server (no gateway, no DNS), answers
// to smartcw.local, serves the web interface and the TCP text protocol.
void netBegin();
void netTick();
bool netActive();
void netStream(const char *s); // text stream to every TCP client
void netPrintStatus(Print &o);
void netPrintTcp(Print &o); // every TCP connection (diagnostics)
String netIp();
int netTcpClients();

#include "net.h"
#include "cli.h"
#include "config.h"
#include "cwd.h"
#include "pins.h"
#include "web.h"
#include <NCMEthernetlwIP.h>
#include <SimpleMDNS.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <pico/unique_id.h>
#include <tusb.h>
#include <lwip_wrap.h>
extern "C" {
#include "dhcps.h"
#include <lwip/priv/tcp_priv.h>
#include <lwip/apps/mdns.h>
#include <lwip/timeouts.h>
}

static NCMEthernetlwIP eth;

// The NCM driver drops a frame when its transmit queue is full, and the
// generic interface code then reports ERR_MEM (it also does so for every
// chained pbuf, comparing against the first part's length only). lwIP never
// retries a segment whose very first transmission failed, so an unlucky
// SYN-ACK leaves the connection hanging in SYN_RCVD until all five TCP slots
// are gone. Treat the USB link as what it is, a lossy link: report every
// frame as sent and let TCP's retransmissions recover the rare drop.
static netif_linkoutput_fn ncmLinkOutput = nullptr;

// When SmartCW restarts while a browser has the page open, the browser's
// retries were bound to the computer's LAN address while the USB link was
// gone, and then arrive over USB from that address. Using the computer's USB
// address as gateway lets SmartCW answer them anyway; anything still without
// a route is dropped at once instead of holding one of the five TCP slots.
extern "C" void cw_dhcps_leased(struct netif *n, const uint8_t ip[4]) {
  ip4_addr_t gw;
  IP4_ADDR(&gw, ip[0], ip[1], ip[2], ip[3]);
  netif_set_gw(n, &gw);
}

static void httpTxt(struct mdns_service *service, void *) {
  static const char path[] = "path=/";
  static const char ver[] = "version=" SMARTCW_VERSION;
  mdns_resp_add_service_txtitem(service, path, sizeof(path) - 1);
  mdns_resp_add_service_txtitem(service, ver, sizeof(ver) - 1);
}

static void dropUnroutable() {
  LWIPMutex m;
  for (struct tcp_pcb *p = tcp_active_pcbs; p;) {
    struct tcp_pcb *next = p->next;
    if (p->state == SYN_RCVD && p->unsent && !ip_route(&p->local_ip, &p->remote_ip))
      tcp_abort(p);
    p = next;
  }
}
static err_t lossyLinkOutput(struct netif *n, struct pbuf *p) {
  ncmLinkOutput(n, p);
  return ERR_OK;
}

static cw_dhcps_t dhcps;
static WiFiServer *tcpServer = nullptr;
static bool active = false;
static IPAddress myIp;

// Output buffered per client and written only as fast as TCP takes it, so a
// slow or stuck client never stalls the main loop.
class RingPrint : public Print {
public:
  static const uint16_t SZ = 2048;
  size_t write(uint8_t c) override {
    uint16_t n = (uint16_t)((head + 1) % SZ);
    if (n == tail)
      return 0; // full: drop
    buf[head] = c;
    head = n;
    return 1;
  }
  void clear() { head = tail = 0; }
  void flushTo(WiFiClient &c) {
    while (head != tail) {
      int room = c.availableForWrite();
      if (room <= 0)
        return;
      uint16_t len = head > tail ? (uint16_t)(head - tail) : (uint16_t)(SZ - tail);
      if (len > room)
        len = (uint16_t)room;
      if (len > 512)
        len = 512;
      size_t w = c.write(buf + tail, len);
      if (w == 0)
        return;
      tail = (uint16_t)((tail + w) % SZ);
    }
  }

private:
  uint8_t buf[SZ];
  uint16_t head = 0, tail = 0;
};

struct TcpConn {
  WiFiClient c;
  bool used = false;
  RingPrint out;
  CliSession cli{out};
  uint8_t iac = 0; // telnet negotiation state
};

static const int TCP_SLOTS = 2;
static TcpConn conns[TCP_SLOTS];

bool netActive() { return active; }
String netIp() { return active ? myIp.toString() : String(config.ip); }

int netTcpClients() {
  int n = 0;
  for (auto &t : conns)
    if (t.used)
      n++;
  return n;
}

void netStream(const char *s) {
  for (auto &t : conns)
    if (t.used)
      t.out.print(s);
}

// Every lwIP TCP connection, for "!net tcp".
void netPrintTcp(Print &o) {
  static const char *names[] = {"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED", "FIN_WAIT_1",
                                "FIN_WAIT_2", "CLOSE_WAIT", "CLOSING", "LAST_ACK", "TIME_WAIT"};
  struct { struct tcp_pcb *list; const char *what; } lists[] = {{tcp_active_pcbs, "active"}, {tcp_tw_pcbs, "timewait"}};
  for (auto &l : lists)
    for (struct tcp_pcb *p = l.list; p; p = p->next)
      o.printf("!tcp %s %s %u->%u queued %u unacked %d unsent %d (ref %d) wnd %u cwnd %u rtime %d flags %x age %lu\n", l.what,
               p->state <= 10 ? names[p->state] : "?", p->local_port, p->remote_port, p->snd_queuelen, p->unacked != nullptr,
               p->unsent != nullptr, p->unsent ? p->unsent->p->ref : -1, p->snd_wnd, p->cwnd, p->rtime, p->flags,
               (unsigned long)(tcp_ticks - p->tmr));
  o.printf("!netif up %d link %d, tcp_ticks %lu\n", netif_is_up(eth.getNetIf()), netif_is_link_up(eth.getNetIf()), (unsigned long)tcp_ticks);
  extern NCMEthernet *_ncm_ethernet_instance;
  if (_ncm_ethernet_instance)
    o.printf("!ncm xmit queue %u, tud ready %d, can xmit %d\n", queue_get_level(&_ncm_ethernet_instance->_xmit_queue),
             tud_ready(), tud_network_can_xmit(1514));
}

void netPrintStatus(Print &o) {
  if (!config.network) {
    o.println(F("!net off (network: false)"));
    return;
  }
  o.print(F("!net "));
  o.print(active ? F("on") : F("starting"));
  o.print(F(", web http://"));
  o.print(netIp());
  o.print(F("/ or http://smartcw.local/, tcp port "));
  o.print(config.tcpPort);
  if (config.cwdaemonPort) {
    o.print(F(", cwdaemon udp port "));
    o.print(config.cwdaemonPort);
  }
  o.print(F(", web clients "));
  o.print(webClients());
  o.print(F(", tcp clients "));
  o.println(netTcpClients());
}

void netBegin() {
  if (!config.network)
    return;
  if (!myIp.fromString(config.ip))
    myIp = IPAddress(192, 168, 7, 1);

  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id);
  uint8_t mac[6] = {0x02, 0x52, id.id[4], id.id[5], id.id[6], id.id[7]};

  eth.config(myIp, IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0));
  if (!eth.begin(mac))
    return;
  ncmLinkOutput = eth.getNetIf()->linkoutput;
  eth.getNetIf()->linkoutput = lossyLinkOutput;
  netif_set_default(eth.getNetIf());

  ip_addr_t a, m;
  IP_ADDR4(&a, myIp[0], myIp[1], myIp[2], myIp[3]);
  IP_ADDR4(&m, 255, 255, 255, 0);
  cw_dhcps_init(&dhcps, &a, &m, eth.getNetIf());

  // smartcw.local (A and, with IPv6, AAAA for the link-local address) plus
  // DNS-SD records, so the web page and the TCP port show up in Avahi /
  // Bonjour service browsers as "SmartCW".
  MDNS.begin("smartcw");
  {
    LWIPMutex lock;
    mdns_resp_add_service(eth.getNetIf(), "SmartCW", "_http", DNSSD_PROTO_TCP, 80, httpTxt, nullptr);
    mdns_resp_add_service(eth.getNetIf(), "SmartCW", "_telnet", DNSSD_PROTO_TCP, (u16_t)config.tcpPort, nullptr, nullptr);
  }

  tcpServer = new WiFiServer((uint16_t)config.tcpPort);
  tcpServer->begin();
  webBegin();
  cwdBegin();
  active = true;
}

static void feedTelnet(TcpConn &t, uint8_t b) {
  switch (t.iac) {
  case 0:
    if (b == 255)
      t.iac = 1;
    else
      t.cli.feed((char)b);
    return;
  case 1: // after IAC
    if (b >= 251 && b <= 254)
      t.iac = 2; // WILL/WONT/DO/DONT + option
    else if (b == 250)
      t.iac = 3; // subnegotiation
    else
      t.iac = 0;
    return;
  case 2:
    t.iac = 0;
    return;
  case 3:
    if (b == 255)
      t.iac = 4;
    return;
  case 4:
    t.iac = (b == 240) ? 0 : 3;
    return;
  }
}

static void tcpTick() {
  if (!tcpServer)
    return;
  WiFiClient nc = tcpServer->accept();
  if (nc) {
    TcpConn *slot = nullptr;
    for (auto &t : conns)
      if (!t.used) {
        slot = &t;
        break;
      }
    if (!slot) {
      nc.print("!err busy, only two TCP clients at a time\r\n");
      nc.stop();
    } else {
      slot->c = nc;
      slot->c.setNoDelay(true);
      slot->c.setTimeout(20);
      slot->used = true;
      slot->iac = 0;
      slot->out.clear();
      slot->cli.reset();
      slot->out.print(F("!SmartCW " SMARTCW_VERSION " ready - type text to send CW, !help for commands\n"));
    }
  }
  for (auto &t : conns) {
    if (!t.used)
      continue;
    if (!t.c.connected() && !t.c.available()) {
      t.c.stop();
      t.used = false;
      continue;
    }
    int n = 0;
    while (t.c.available() && n++ < 256) {
      int b = t.c.read();
      if (b < 0)
        break;
      feedTelnet(t, (uint8_t)b);
    }
    t.out.flushTo(t.c);
  }
}

void netTick() {
  if (!active)
    return;
  // arduino-pico runs lwIP's timers (TCP retransmissions, delayed ACKs,
  // TIME_WAIT, mDNS) only next to a polled Ethernet driver, and the NCM driver
  // is not polled: without this, a dropped frame stalls a connection until the
  // next packet happens to arrive. The NCM worker is nudged as well so frames
  // queued while USB was busy go out without waiting for traffic.
  static uint32_t lastTimers = 0, lastKick = 0, lastDrop = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastKick >= 5) {
    lastKick = nowMs;
    NCMEthernet::_set_recv_pending();
  }
  if (nowMs - lastTimers >= 10) {
    lastTimers = nowMs;
    sys_check_timeouts();
  }
  if (nowMs - lastDrop >= 200) {
    lastDrop = nowMs;
    dropUnroutable();
  }
  tcpTick();
  webTick();
  cwdTick();
}

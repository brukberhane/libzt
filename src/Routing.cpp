#include "Routing.hpp"

#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"

#include <algorithm>
#include <mutex>
#include <string.h>

namespace {

std::mutex g_routes_m;
std::vector<ZtsRoute> g_routes;   // sorted longest-prefix first

bool routeMatches(const ZtsRoute& route, const uint8_t* dest)
{
    const int len = route.isV6 ? 16 : 4;
    int bits = route.prefixLen;
    for (int i = 0; i < len && bits > 0; ++i, bits -= 8) {
        uint8_t mask = bits >= 8 ? 0xFF : (uint8_t)(0xFF << (8 - bits));
        if ((dest[i] & mask) != (route.target[i] & mask)) {
            return false;
        }
    }
    return true;
}

bool isZero(const uint8_t* addr, int len)
{
    for (int i = 0; i < len; ++i) {
        if (addr[i] != 0) {
            return false;
        }
    }
    return true;
}

}   // namespace

void zts_routes_replace(const std::vector<ZtsRoute>& routes)
{
    std::lock_guard<std::mutex> lock(g_routes_m);
    g_routes = routes;
    std::sort(g_routes.begin(), g_routes.end(), [](const ZtsRoute& a, const ZtsRoute& b) {
        if (a.prefixLen != b.prefixLen) {
            return a.prefixLen > b.prefixLen;
        }
        if (a.metric != b.metric) {
            return a.metric < b.metric;
        }
        return a.netId < b.netId;
    });
}

extern "C" struct netif* zts_ip4_route_hook(const ip4_addr_t* dest)
{
    if (! dest) {
        return NULL;
    }
    const uint8_t* d = (const uint8_t*)&(dest->addr);
    std::lock_guard<std::mutex> lock(g_routes_m);
    for (const auto& route : g_routes) {
        if (! route.isV6 && route.netif && routeMatches(route, d)) {
            return (struct netif*)route.netif;
        }
    }
    return NULL;
}

extern "C" const ip4_addr_t* zts_etharp_get_gw_hook(struct netif* netif, const ip4_addr_t* dest)
{
    if (! netif || ! dest) {
        return NULL;
    }
    // On-link destinations keep default behaviour: ARP for the dest itself.
    if (ip4_addr_netcmp(dest, netif_ip4_addr(netif), netif_ip4_netmask(netif))) {
        return NULL;
    }
    // Callers are serialized by the lwIP core lock, so a static is safe here.
    static ip4_addr_t gateway;
    const uint8_t* d = (const uint8_t*)&(dest->addr);
    std::lock_guard<std::mutex> lock(g_routes_m);
    for (const auto& route : g_routes) {
        if (route.isV6 || route.netif != (void*)netif || ! routeMatches(route, d)) {
            continue;
        }
        if (! isZero(route.via, 4)) {
            // Managed route with a gateway: resolve the gateway on the
            // overlay (it answers ARP), it forwards to the routed prefix.
            memcpy(&(gateway.addr), route.via, 4);
            return &gateway;
        }
        // Via-less managed route: the whole prefix lives on the overlay.
        return dest;
    }
    return NULL;
}

extern "C" struct netif* zts_ip6_route_hook(const ip6_addr_t* dest, const ip6_addr_t* src)
{
    (void)src;
    if (! dest) {
        return NULL;
    }
    const uint8_t* d = (const uint8_t*)&(dest->addr);
    std::lock_guard<std::mutex> lock(g_routes_m);
    for (const auto& route : g_routes) {
        if (route.isV6 && route.netif && routeMatches(route, d)) {
            return (struct netif*)route.netif;
        }
    }
    return NULL;
}

extern "C" const ip6_addr_t* zts_nd6_get_gw_hook(struct netif* netif, const ip6_addr_t* dest)
{
    if (! netif || ! dest) {
        return NULL;
    }
    // nd6 only calls this hook for off-link, non-link-local destinations,
    // so no on-link check is needed here (unlike the IPv4 hook).
    // Callers are serialized by the lwIP core lock, so a static is safe here.
    static ip6_addr_t gateway;
    const uint8_t* d = (const uint8_t*)&(dest->addr);
    std::lock_guard<std::mutex> lock(g_routes_m);
    for (const auto& route : g_routes) {
        if (! route.isV6 || route.netif != (void*)netif || ! routeMatches(route, d)) {
            continue;
        }
        if (! isZero(route.via, 16)) {
            memcpy(gateway.addr, route.via, 16);
            return &gateway;
        }
        // Via-less managed route: the whole prefix lives on the overlay.
        return dest;
    }
    return NULL;
}

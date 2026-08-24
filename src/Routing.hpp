#ifndef ZTS_ROUTING_HPP
#define ZTS_ROUTING_HPP

#include <stdbool.h>
#include <stdint.h>
#include <vector>

/**
 * One ZeroTier managed route made visible to the lwIP stack.
 *
 * ZeroTierOne only applies managed routes at the OS level (see
 * osdep/ManagedRoute.cpp), so libzt's lwIP never learns about off-subnet
 * prefixes and zts_connect() fails with ERR_RTE for them. NodeService
 * maintains this table and the lwIP hooks in Routing.cpp consult it.
 */
struct ZtsRoute {
    uint64_t netId;       // Owning ZeroTier network
    bool isV6;            // false: IPv4 (4-byte prefix), true: IPv6 (16-byte prefix)
    uint8_t prefixLen;    // 0..32 (IPv4) or 0..128 (IPv6)
    uint16_t metric;      // Route metric, tie-break after prefix length
    uint8_t target[16];   // Masked network address (IPv4 uses first 4 bytes)
    uint8_t via[16];      // Gateway on the overlay, all-zero = whole prefix on-link
    void* netif;          // lwIP netif of the owning network, NULL = not usable
};

/**
 * Atomically replace the managed-route table used by the lwIP hooks.
 * Called by NodeService whenever network configs or settings change.
 * Entries are sorted longest-prefix-first (then metric, then netId).
 */
void zts_routes_replace(const std::vector<ZtsRoute>& routes);

#endif

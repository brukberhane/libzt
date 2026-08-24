#ifndef ZTS_LWIP_HOOKS_H
#define ZTS_LWIP_HOOKS_H

/* Declarations for the lwIP hooks defined in lwipopts.h (LWIP_HOOK_FILENAME).
 * Included by lwip's ip4.c, ip6.c, etharp.c and nd6.c; see Routing.cpp for
 * the implementations. */

#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

struct netif* zts_ip4_route_hook(const ip4_addr_t* dest);
const ip4_addr_t* zts_etharp_get_gw_hook(struct netif* netif, const ip4_addr_t* dest);
struct netif* zts_ip6_route_hook(const ip6_addr_t* dest, const ip6_addr_t* src);
const ip6_addr_t* zts_nd6_get_gw_hook(struct netif* netif, const ip6_addr_t* dest);

#ifdef __cplusplus
}
#endif

#endif

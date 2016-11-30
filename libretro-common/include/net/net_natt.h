#include <net/net_compat.h>

struct natt_status {
    /** The fdset to be selected upon to check for responses */
    fd_set fds;

    /** True if we've resolved an external IPv4 address */
    bool have_inet4;

    /** External IPv4 address */
    struct sockaddr_in ext_inet4_addr;

    /** True if we've resolved an external IPv6 address */
    bool have_inet6;

#ifdef AF_INET6
    /** External IPv6 address */
    struct sockaddr_in6 ext_inet6_addr;
#endif

    /** Internal status (currently unused) */
    void *internal;
};

/**
 * Initialize global NAT traversal structures (must be called once to use other
 * functions) */
void natt_init(void);

/** Initialize a NAT traversal status object */
bool natt_new(struct natt_status *status);

/** Free a NAT traversal status object */
void natt_free(struct natt_status *status);

/**
 * Make a port forwarding request. This may finish immediately or just send a
 * request to the network. */
bool natt_open_port(struct natt_status *status, struct sockaddr *addr, socklen_t addrlen);

/**
 * Make a port forwarding request when only the port is known. Forwards any
 * address it can find. */
bool natt_open_port_any(struct natt_status *status, uint16_t port);

/** Check for port forwarding responses */
bool natt_read(struct natt_status *status);

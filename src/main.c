#include "io.systemd.network.varlink.h"

#include <errno.h>
#include <libnl3/netlink/route/link.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <varlink.h>

#define _cleanup_(_x) __attribute__((__cleanup__(_x)))

static inline void freep(void *p) {
        free(*(void **)p);
}

static inline void closep(int *fd) {
        if (*fd >= 0)
                close(*fd);
}

static inline void nl_socket_freep(void *p) {
        if (p)
                nl_socket_free(*(struct nl_sock **)p);
}

static inline void nl_cache_putp(void *p) {
        if (p)
                nl_cache_put(*(struct nl_cache **)p);
}

static inline void rtnl_link_putp(void *p) {
        if (p)
                rtnl_link_put(*(struct rtnl_link **)p);
}

static long io_systemd_network_Info(VarlinkServer *server,
                                    VarlinkCall *call,
                                    VarlinkObject *parameters,
                                    uint64_t flags,
                                    void *userdata) {
        struct nl_sock *nl_sock = userdata;
        int64_t ifindex;
        _cleanup_(rtnl_link_putp) struct rtnl_link *rtnl_link = NULL;
        _cleanup_(varlink_object_unrefp) VarlinkObject *info = NULL;
        _cleanup_(varlink_object_unrefp) VarlinkObject *reply = NULL;
        long r;

        r = varlink_object_get_int(parameters, "ifindex", &ifindex);
        if (r < 0)
                return varlink_call_reply_invalid_parameters(call, "ifindex", NULL);

        if (ifindex < 1)
                return varlink_call_reply_invalid_parameters(call, "ifindex", NULL);

        r = rtnl_link_get_kernel(nl_sock, ifindex, NULL, &rtnl_link);
        if (r < 0)
                return varlink_call_reply_error(call, "io.systemd.network.UnknownNetworkDevice", NULL);

        if (varlink_object_new(&info) < 0 ||
            varlink_object_set_int(info, "ifindex", ifindex) < 0 ||
            varlink_object_set_string(info, "ifname", rtnl_link_get_name(rtnl_link)) < 0)
                return -EUCLEAN;

        if (varlink_object_new(&reply) < 0 || varlink_object_set_object(reply, "info", info) < 0)
                return -EUCLEAN;

        return varlink_call_reply(call, reply, 0);
}

static long io_systemd_network_List(VarlinkServer *server,
                                    VarlinkCall *call,
                                    VarlinkObject *parameters,
                                    uint64_t flags,
                                    void *userdata) {
        struct nl_sock *nl_sock = userdata;
        _cleanup_(nl_cache_putp) struct nl_cache *nl_cache = NULL;
        _cleanup_(varlink_array_unrefp) VarlinkArray *links = NULL;
        _cleanup_(varlink_object_unrefp) VarlinkObject *reply = NULL;
        int r;

        r = rtnl_link_alloc_cache(nl_sock, AF_UNSPEC, &nl_cache);
        if (r < 0)
                return r;

        r = varlink_array_new(&links);
        if (r < 0)
                return r;

        for (struct nl_object *nl_object = nl_cache_get_first(nl_cache); nl_object; nl_object = nl_cache_get_next(nl_object)) {
                struct rtnl_link *rtnl_link = (struct rtnl_link *)nl_object;
                _cleanup_(varlink_object_unrefp) VarlinkObject *link = NULL;

                if (varlink_object_new(&link) < 0 ||
                    varlink_object_set_string(link, "ifname", rtnl_link_get_name(rtnl_link)) < 0 ||
                    varlink_object_set_int(link, "ifindex", rtnl_link_get_ifindex(rtnl_link)) < 0)
                        return -EUCLEAN;

                r = varlink_array_append_object(links, link);
                if (r < 0)
                        return r;
        }

        r = varlink_object_new(&reply);
        if (r < 0)
                return r;

        r = varlink_object_set_array(reply, "links", links);
        if (r < 0)
                return r;

        return varlink_call_reply(call, reply, 0);
}

int main(int argc, char **argv) {
        _cleanup_(nl_socket_freep) struct nl_sock *nl_sock = NULL;
        _cleanup_(varlink_server_freep) VarlinkServer *server = NULL;
        const char *address;
        int fd = -1;
        _cleanup_(closep) int fd_epoll = -1;
        _cleanup_(closep) int fd_signal = -1;
        sigset_t mask;
        struct epoll_event ep = {};
        bool exit = false;
        int r;

        address = argv[1];
        if (!address) {
                fprintf(stderr, "Error: missing address.\n");

                return EXIT_FAILURE;
        }

        /* An activator passed us our connection. */
        if (read(3, NULL, 0) == 0)
                fd = 3;

        r = varlink_server_new(&server,
                               address,
                               fd,
                               NULL,
                               &io_systemd_network_varlink, 1);
        if (r < 0)
                return EXIT_FAILURE;

        nl_sock = nl_socket_alloc();
        if (!nl_sock)
                return EXIT_FAILURE;

        r = nl_connect(nl_sock, NETLINK_ROUTE);
        if (r < 0)
                return EXIT_FAILURE;

        r = varlink_server_set_method_callback(server, "io.systemd.network.List",
                                               io_systemd_network_List, nl_sock);
        if (r < 0)
                return EXIT_FAILURE;

        r = varlink_server_set_method_callback(server, "io.systemd.network.Info",
                                               io_systemd_network_Info, nl_sock);
        if (r < 0)
                return EXIT_FAILURE;

        fd_epoll = epoll_create1(EPOLL_CLOEXEC);
        if (fd_epoll < 0)
                return EXIT_FAILURE;

        ep.events = EPOLLIN;
        ep.data.fd = varlink_server_get_fd(server);
        if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, varlink_server_get_fd(server), &ep) < 0)
                return EXIT_FAILURE;

        sigemptyset(&mask);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGINT);
        sigprocmask(SIG_BLOCK, &mask, NULL);

        fd_signal = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd_signal < 0)
                return EXIT_FAILURE;

        ep.events = EPOLLIN;
        ep.data.fd = fd_signal;
        if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_signal, &ep) < 0)
                return EXIT_FAILURE;

        while (!exit) {
                int n;
                struct epoll_event event;

                n = epoll_wait(fd_epoll, &event, 1, -1);
                if (n < 0) {
                        if (errno == EINTR)
                                continue;

                        return EXIT_FAILURE;
                }

                if (n == 0)
                        continue;

                if (event.data.fd == varlink_server_get_fd(server)) {
                        r = varlink_server_process_events(server);
                        if (r < 0) {
                                fprintf(stderr, "Control: %s\n", strerror(-r));
                                if (r != -EPIPE)
                                        return EXIT_FAILURE;
                        }
                } else if (event.data.fd == fd_signal) {
                        struct signalfd_siginfo fdsi;
                        long size;

                        size = read(fd_signal, &fdsi, sizeof(struct signalfd_siginfo));
                        if (size != sizeof(struct signalfd_siginfo))
                                continue;

                        switch (fdsi.ssi_signo) {
                                case SIGTERM:
                                case SIGINT:
                                        exit = true;
                                        break;

                                default:
                                        return -EINVAL;
                        }
                }
        }

        return EXIT_SUCCESS;
}

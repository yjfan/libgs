#include "unix_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

static int gs_unix_socket_init(struct gs_socket_t *gsocket)
{
    gsocket->fd = -1;
    gsocket->address = NULL;

    return 0;
}

static int gs_unix_socket_close(struct gs_socket_t *gsocket)
{
    if (gsocket->fd >= 0) {
        close(gsocket->fd);
        gsocket->fd = -1;
    }

    if (gsocket->address) {
        if (gsocket->address[0] != '@') {
            unlink(gsocket->address);
        }

        free(gsocket->address);
        gsocket->address = NULL;
    }

    return 0;
}

static int gs_unix_socket_bind(struct gs_socket_t *gsocket, const char *address, int backlog)
{
    if (gsocket->fd >= 0) {
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un socket_addr;
    memset(&socket_addr, 0, sizeof(struct sockaddr_un));
    socket_addr.sun_family = AF_UNIX;

    snprintf(socket_addr.sun_path, sizeof(socket_addr.sun_path), "%s", address);

    if (socket_addr.sun_path[0] == '@') {
        socket_addr.sun_path[0] = '\0';
    }

    if (bind(fd, &socket_addr, sizeof(struct sockaddr_un)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, backlog) < 0) {
        close(fd);
        return -1;
    }

    gsocket->fd = fd;
    gsocket->address = strdup(address);

    return 0;
}

static int gs_unix_socket_accept(struct gs_socket_t *gsocket, char *address, unsigned int length, struct gs_socket_t *client)
{
    struct sockaddr_storage socket_storage;
    memset(&socket_storage, 0, sizeof(struct sockaddr_storage));

    struct sockaddr_un *socket_addr = (struct sockaddr_un *)&socket_storage;
    socklen_t socket_length = sizeof(struct sockaddr_storage);

    const int client_fd = accept(gsocket->fd, (struct sockaddr *)socket_addr, &socket_length);

    if (client_fd < 0) {
        return -1;
    }

    client->fd = client_fd;

    if (address && length) {
        snprintf(address, length, "%s", socket_addr->sun_path);
    }

    return 0;
}

static int gs_unix_socket_connect(struct gs_socket_t *gsocket, const char *address)
{
    if (gsocket->fd >= 0) {
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un socket_addr;
    memset(&socket_addr, 0, sizeof(struct sockaddr_un));
    socket_addr.sun_family = AF_UNIX;

    snprintf(socket_addr.sun_path, sizeof(socket_addr.sun_path), "%s", address);

    if (socket_addr.sun_path[0] == '@') {
        socket_addr.sun_path[0] = '\0';
    }

    if (connect(fd, &socket_addr, sizeof(struct sockaddr_un)) < 0) {
        close(fd);
        return -1;
    }

    gsocket->fd = fd;

    return 0;
}

static int gs_unix_socket_send(struct gs_socket_t *gsocket, const void *data, unsigned int length, int flags)
{
    struct iovec iov;
    memset(&iov, 0, sizeof(struct iovec));
    iov.iov_base = (void *)data;
    iov.iov_len = length;

    struct msghdr message_header;
    memset(&message_header, 0, sizeof(struct msghdr));
    message_header.msg_iov = &iov;
    message_header.msg_iovlen = 1;
    message_header.msg_control = NULL;
    message_header.msg_controllen = 0;

    return sendmsg(gsocket->fd, &message_header, flags);
}

static int gs_unix_socket_recv(struct gs_socket_t *gsocket, void *data, unsigned int length, int flags)
{
    struct iovec iov;
    memset(&iov, 0, sizeof(struct iovec));
    iov.iov_base = data;
    iov.iov_len = length;

    struct msghdr message_header;
    memset(&message_header, 0, sizeof(struct msghdr));
    message_header.msg_iov = &iov;
    message_header.msg_iovlen = 1;
    message_header.msg_control = NULL;
    message_header.msg_controllen = 0;

    return recvmsg(gsocket->fd, &message_header, flags);
}

const struct gs_socket_base_t * gs_unix_socket_base(void)
{
    static const struct gs_socket_base_t base = {
        .init = gs_unix_socket_init,
        .close = gs_unix_socket_close,
        .bind = gs_unix_socket_bind,
        .accept = gs_unix_socket_accept,
        .connect = gs_unix_socket_connect,
        .send = gs_unix_socket_send,
        .recv = gs_unix_socket_recv
    };

    return &base;
}
/**
 * @file socket.h
 * @author charles
 * @brief 
*/

#ifndef __BASE_SOCKET_H_
#define __BASE_SOCKET_H_

#include <sys/socket.h>

namespace xrtc {

int create_tcp_server(const char* addr, int port);

int tcp_accept(int sock, char *host, int *port);

int sock_setnoblock(int sock);

int sock_setnodelay(int sock);

int sock_peer_to_str(int sock, char *ip, int *port);

int sock_read_data(int sock, char *buf, size_t len);

int sock_write_data(int sock, const char *buf, size_t len);

int create_udp_socket(int family);

int sock_bind(int sock, struct sockaddr* addr, socklen_t len, int min_port, int max_port);

int sock_get_address(int sock, char* ip, int* port);

int sock_recv_from(int sock, char* buf, size_t len, struct sockaddr* addr, socklen_t addr_len);

int64_t sock_get_recv_timestamp(int sock);

int sock_send_to(int sock, const char* buf, size_t len, int flag, struct sockaddr* addr, socklen_t addr_len);

} // end namespace xrtc

#endif // __BASE_SOCKET_H_

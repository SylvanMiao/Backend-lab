#pragma once
extern int client_end_point();
extern int server_end_point();
extern int create_tcp_socket();
extern int create_acceptor_socket();

extern int bind_acceptor_socket();
extern int connect_to_end();
extern int dns_connect_end();
extern int accept_new_connection();

extern void use_const_buffer();
extern void use_buffer_str();
extern void use_buffer_array();
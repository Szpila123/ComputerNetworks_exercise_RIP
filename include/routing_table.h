#ifndef ROUTING_TABLE
#define ROUTING_TABLE

#include <inttypes.h>

//structure to hold interface info
typedef struct {
	char addr[16];
	int8_t mask;
	uint32_t dist;
	int32_t sock_fd;
} ifce_s;

void Init_routing_info( int32_t arg_nof_ifcs , ifce_s *arg_ifces);
void Recv_routing_info( int32_t soc_fd, int32_t turn_time );
void Send_routing_info( int32_t port );
void End_turn();
void Kill_routing_info();
#endif

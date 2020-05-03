
#include "../include/routing_table.h"

#include <error.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/ip.h>
#include <arpa/inet.h>


#define DEBUG

#define TRUE  1
#define FALSE 0

#define TTL 3
#define MAX_DIST 16
#define DATA_SIZE 9


typedef struct {
    union{
        uint8_t is_indirect;
        char next_addr[16];
    };

    uint32_t dist;
    uint32_t last_info;
    
    uint32_t addr;
    uint8_t  mask;

} rout_info_t;

typedef struct {
    uint32_t addr;
    uint8_t mask;
    uint32_t dist;
} udp_data;

static rout_info_t *r_info;
uint32_t r_info_recs;
uint32_t r_info_size;


static ifce_s *ifces;
static int32_t nof_ifcs;

//Helping functions
static uint32_t get_mask( uint8_t decimal_mask );
static uint32_t get_dist( uint32_t addr );
static void rinfo_add( uint32_t addr, int8_t mask, char *next_addr, int32_t dist );
static int32_t is_addr_mine( char* addr );



void Init_routing_info( int32_t arg_nof_ifcs , ifce_s *arg_ifces){
    //Initializa the number of records, number of interfaces and size of routing table
    r_info_recs    = arg_nof_ifcs;
    nof_ifcs       = arg_nof_ifcs;
    r_info_size    = arg_nof_ifcs * 2;

    //Initialize pointers to routing table and interfaces table
    r_info         = (rout_info_t*) malloc( sizeof( rout_info_t ) * r_info_size );
    ifces          = arg_ifces;
    if( r_info == NULL )
        error( 1, 0, "Couldn't allocate the space for routing info\n" );

    //Add records of interfaces to routing table 
    char empty_str = 0;
    int32_t addr;

    //Enable broadcasting for sending sockets
    int broadcastPermission = 1;


    for( int i = 0 ; i < nof_ifcs ; i++ ){
        setsockopt( ifces[i].sock_fd, SOL_SOCKET, SO_BROADCAST,
                     (void*) &broadcastPermission,
                     sizeof(broadcastPermission) );

        if( inet_pton( AF_INET, ifces[i].addr, &addr )  != 1 )
            error( 1, 0, "Couldn't translate an address from text to binary\n" );

        //after conversion addr is in network notation, so we need to fix it...
        addr = ntohl(addr);

        rinfo_add( addr, ifces[i].mask, &empty_str, ifces[i].dist );
    }
    
    return;
}





void Recv_routing_info( int32_t soc_fd, int32_t turn_time ){

    #ifdef DEBUG
    printf("Starting receiving\n");
    #endif
    struct timeval tv;
    tv.tv_sec         = turn_time;
    tv.tv_usec        = 0;

    fd_set descriptors;
    int ready;

    uint8_t buffer[IP_MAXPACKET];

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof( sender );

    for(;;){
        FD_ZERO( &descriptors );
        FD_SET( soc_fd, &descriptors );

        ready = select( soc_fd + 1, &descriptors, NULL, NULL, &tv );

        if( ready == -1 ){
            if( errno == EINTR )
                continue;
            else
                error( 1, 0, "select function failed\n" );
        } else if( ready == 0 )
            break;
        
        ssize_t    packet_len = recvfrom(
                                    soc_fd,
                                    buffer,
                                    IP_MAXPACKET,
                                    0,
                                    (struct sockaddr*) &sender,
                                    &sender_len
                                );

        if( packet_len != 9 ){
            #ifdef DEBUG
                printf( "Got a package of wrong size\n" );
            #endif //DEBUG
            continue;
        }
        else{
            #ifdef DEBUG
                printf( "Got a package of right size\n" );
            #endif //DEBUG
        }

        //Translete data from packet to little endian
        udp_data data; 
        data.addr     = ntohl( *((int32_t*)buffer) );
        data.mask     =           *((int8_t*)  (buffer+sizeof(int32_t)                  ))  ;
        data.dist     = ntohl(    *((int32_t*) (buffer+sizeof(int32_t)+sizeof(int8_t)   )) );

        data.addr    &= get_mask( data.mask );

        //Convert sender address
        char ip_addr[16];
        if( ip_addr != inet_ntop(AF_INET, &(sender.sin_addr), ip_addr, sizeof(ip_addr)) )
            break;

        #ifdef DEBUG
            printf("\tReceived from %s\n", ip_addr );
            printf("\tAddr: %x, mask %hhd, dist: %d\n", data.addr, data.mask, data.dist );
        #endif

        //Get distance to sender
        uint32_t dist = get_dist( sender.sin_addr.s_addr );

        //Look for network address in routing table
        uint32_t i = 0;
        for( ; i < r_info_recs ; i++ ){
            if( data.addr == r_info[i].addr && data.mask == r_info[i].mask ){

                //Check if the sender is the same as before
                if( strcmp( ip_addr, r_info[i].next_addr ) == 0 ){
                     r_info[i].dist         = data.dist + dist;
                     r_info[i].last_info    = TTL;
                }
                else if( r_info[i].dist > data.dist + dist && !is_addr_mine(ip_addr)){
                     r_info[i].dist         = data.dist + dist;
                     r_info[i].last_info    = TTL;
                     strcpy( r_info[i].next_addr, ip_addr );
                }
                i = 0;
                break;
            }
        }

        //If network was not found in the routing table add it as new record
        if( i == r_info_recs )
            rinfo_add( data.addr, data.mask, ip_addr, data.dist );
    }

    #ifdef DEBUG
        printf("Finished receiving\n");
    #endif
    return;
}





void Send_routing_info( int32_t port ){

    struct sockaddr_in broadcast_addr;
    bzero(&broadcast_addr, sizeof(broadcast_addr) );
    broadcast_addr.sin_family        = AF_INET;
    broadcast_addr.sin_port          = htons(port);


    for( int i = 0 ; i < nof_ifcs ; i++ ){

        //get address of the interface
        inet_pton( AF_INET, ifces[i].addr, &broadcast_addr.sin_addr);

        //apply mask on the broadcast address
        broadcast_addr.sin_addr.s_addr |= get_mask(32 - ifces[i].mask);
        uint8_t data[DATA_SIZE];


        //Send data about each known network
        for( uint32_t j = 0 ; j < r_info_recs ; j++ )
            if( r_info[j].dist < MAX_DIST ){
                //Prepare package data
                *((int32_t*)  data    )      = htonl(r_info[j].addr);
                *((int8_t*)  (data+4) )      = r_info[j].mask;
                *((int32_t*) (data+5) )      = htonl(r_info[j].dist);

                if( sendto( ifces[i].sock_fd, data, DATA_SIZE, MSG_DONTROUTE,
                            (struct sockaddr*) &broadcast_addr, sizeof(broadcast_addr) ) < 0 ){ 
                    //Note that ifces[i] and r_info[i] describe the same network for i < nof_ifces
                    if( !r_info[i].is_indirect ){
                        r_info[i].dist         = MAX_DIST;
                        break;
                    }
                } else {
                    if( r_info[i].is_indirect && r_info[i].dist > ifces[i].dist ){
                        r_info[i].dist         = ifces[i].dist;
                        r_info[i].is_indirect  = 0;
                        r_info[i].last_info    = TTL;
                    }
                    else if( !r_info[i].is_indirect )
                        r_info[i].last_info    = TTL;
                }
            }
    }
    return;
}

void End_turn(){
    for( uint32_t i = 0 ; i < r_info_recs ; i++ ){
        char addr[16];
        uint32_t addr_net = htonl(r_info[i].addr);
        inet_ntop( AF_INET, &addr_net, addr, sizeof(addr) );

        printf("%s/%d distance %d %s %s\n", addr, r_info[i].mask, r_info[i].dist,
            r_info[i].is_indirect ? "via" : "connected", r_info[i].is_indirect ? r_info[i].next_addr : "directly"  );

        if( --r_info[i].last_info == 0 )
                r_info[i].dist = MAX_DIST;
    }
    return;
}



//Deinitialize routing_info data
void Kill_routing_info(){
    free(r_info);
    r_info_recs      = 0;
    r_info_size      = 0;

    ifces            = NULL;
    nof_ifcs         = 0;
    return;
}




//------------------------------------------------------------------
static uint32_t get_mask( uint8_t decimal_mask ){
    if( decimal_mask > 32 )
        error( 1, 0, "One of supplied masks was bigger than 32");

    int32_t binary_mask = 0x80000000;

    if( decimal_mask == 0 )
        return 0;

    return (binary_mask >>= decimal_mask - 1);
}


//add record to routing table, maybe resize the table
static void rinfo_add( uint32_t addr, int8_t mask, char *next_addr, int32_t dist ){
    if( r_info_recs == r_info_size ){

        r_info_size    *= 2;
        r_info          = (rout_info_t*) realloc( r_info, r_info_size * sizeof(rout_info_t) );

        if( r_info == NULL )
            error( 1, 0, "Couldnt resize the routing info table\n" );
    }

    int idx    = r_info_recs++;
    
    r_info[idx].mask           = mask;
    r_info[idx].dist           = dist;
    r_info[idx].last_info      = TTL;
    r_info[idx].addr           = addr;

    strcpy( r_info[idx].next_addr, next_addr ); 

    return;
}

static int32_t is_addr_mine( char* addr ){
    for( int i = 0 ; i < nof_ifcs ; i++ )
        if( strcmp(ifces[i].addr, addr ) == 0)
            return TRUE;
    return FALSE;
}

static uint32_t get_dist( uint32_t addr ){
    uint32_t network_addr;
    uint32_t mask;
    for( int i = 0 ; i < nof_ifcs ; i++ ){
        inet_pton( AF_INET, ifces[i].addr, &network_addr );
        mask = get_mask( 32 - ifces[i].mask );
        if( (mask & network_addr) == (mask & addr) )
            return ifces[i].dist;
    }
    return MAX_DIST;
}

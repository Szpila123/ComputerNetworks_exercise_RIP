//bartosz_szpila
//307554

//UWAGA: WARTOSC GRANICZNA (DYSTANS NIESKONCZONY) ZNAJDUJE SIĘ W PLIKU bin/routing_table.c
//należy go ujednolicić dla wszystkich działających programów w sieci - jego wartość nie była podana w opisie pracowni
#define __USE_MISC
#include <inttypes.h>
#include <error.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "../include/routing_table.h"

#define TURN 15
#define LISTEN_PORT 54321

int main(){

    //read input----------------------------------------------------------------------------------------------------
    int nof_ifcs;
    if( scanf( " %d ", &nof_ifcs ) < 1)
        error( 1, 0, "First input row should be the number of interfaces\n");

    if( nof_ifcs < 1 )
        error( 1, 0, "Number of supplied interfaces should be more than zero\n");

    ifce_s ifces[nof_ifcs];
    for( int i = 0; i < nof_ifcs; i++ )
        if( scanf( 
                   " %[.0-9]/%hhd distance %d ",
                    ifces[i].addr,
                   &ifces[i].mask,
                   &ifces[i].dist
         )
            < 3 
      )
            error( 1, 0, "Supplied address in line %d is in incorrect format\n", i + 1);
    //---------------------------------------------------------------------------------------------------------------

    //create socket on port LISTEN_PORT for INADDR_ANY---------------------------------------------------------------
    int udp_socketfd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( udp_socketfd == -1 )
        error( 1, 0, "Error while creating listening socket\n");

    struct sockaddr_in server_addr;
    bzero( &server_addr, sizeof(server_addr) );
    server_addr.sin_family          = AF_INET;
    server_addr.sin_port            = htons(LISTEN_PORT);
    server_addr.sin_addr.s_addr     = htonl(INADDR_ANY);

    if( bind(
                    udp_socketfd,
                    (struct sockaddr*) &server_addr,
                    sizeof( server_addr )
                )
            != 0
          )
        error( 1, 0, "Couldnt bind the listening socket to the port\n");
    //---------------------------------------------------------------------------------------------------------------
    
    //create sending sockets for each interface----------------------------------------------------------------------
    int sending_soc;
    struct sockaddr_in soc_addr;

    bzero( &soc_addr, sizeof(soc_addr) );
    soc_addr.sin_family        = AF_INET;

    for( int i = 0 ; i < nof_ifcs; i++ ){

        sending_soc = socket(AF_INET, SOCK_DGRAM, 0 );

        if( sending_soc == -1 )
            error( 1, 0, "Error while creating sending socket\n");

        inet_pton( AF_INET, ifces[i].addr, &server_addr.sin_addr );

        if( bind( sending_soc, (struct sockaddr*) &soc_addr, sizeof(soc_addr) ) != 0 )
            error( 1, 0, "Error while binding sending socket to the address\n" );

        ifces[i].sock_fd = sending_soc;
    }

    //---------------------------------------------------------------------------------------------------------------
    
    
    
    //main loop------------------------------------------------------------------------------------------------------
    //Initialize routing info 
    Init_routing_info( nof_ifcs, ifces );
    for(;;){
        Recv_routing_info( udp_socketfd, TURN );
        Send_routing_info( LISTEN_PORT );
        End_turn();
    }

    //program will never reach that part
    Kill_routing_info();
    close(udp_socketfd);

    return 0;

}

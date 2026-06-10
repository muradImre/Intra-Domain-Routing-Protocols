#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "Node.h"
#include <vector>
#include <cstdint>

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces 

    std::vector<uint8_t> route_good;
    std::vector<uint16_t> route_port;

    std::vector<uint32_t> route_weight;
    std::vector<uint32_t> route_prev_ref;

    uint32_t dv_prev_tm_ = 0;
    eProtocolType pt_ = P_DV;

    std::vector<double> nb_p_pong, nb_p_ping;
    std::vector<uint16_t> nb_router_num;
    std::vector<uint32_t> nb_round_trip;
    std::vector<uint8_t> nb_secure;

    uint16_t identifier_ = 0;
    uint16_t port_cnt_ = 0;
    uint32_t old_ping_time_ = 0;
    uint16_t highest_identifier_;
};

#endif


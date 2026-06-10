#include "RoutingProtocolImpl.h"
#include <cstdint>
#include <netinet/in.h>
#include <cstring>
#include "global.h"
#include <algorithm>

using namespace std;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  // add your own code  
  identifier_ = router_id;
  port_cnt_ = num_ports;

  nb_p_pong.assign(port_cnt_, 0.0);
  nb_p_ping.assign(port_cnt_, 0.0);

  nb_router_num.assign(port_cnt_, 0);
  nb_round_trip.assign(port_cnt_, 0);
  nb_secure.assign(port_cnt_, 0);

  old_ping_time_ = 0;

  pt_ = protocol_type;

  dv_prev_tm_ = sys -> time();
  highest_identifier_ = identifier_;

  route_good.assign(65536, 0);
  route_prev_ref.assign(65536, 0);

  route_port.assign(65536, 65535);
  route_weight.assign(65536, 1000000000u);

  route_good[identifier_] = 1;
  route_weight[identifier_] = 0;

  route_port[identifier_] = 65535;
  route_prev_ref[identifier_] = dv_prev_tm_;

  uint8_t *data;

  uint16_t sz, origin, destination;
  uint32_t timestamp;

  uint32_t starting_time = sys -> time();

  //transmit pings
  for (int port_idx = 0; port_idx < port_cnt_; port_idx++) {
    data = (uint8_t *) malloc(12);

    data[0] = PING;
    data[1] = 0;

    origin = htons(identifier_);
    destination = htons(0);

    sz = htons(12);
    timestamp = htonl(starting_time);

    memcpy(data + 2, &sz, 2);
    memcpy(data + 4, &origin, 2);
    memcpy(data + 6, &destination, 2);
    memcpy(data + 8, &timestamp, 4);

    sys -> send(port_idx, data, 12);
  }

  old_ping_time_ = starting_time;

  sys -> set_alarm(this, 1000, nullptr); //each sec
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  uint32_t curr_time = sys -> time();

  bool flag_d = false;

  if (curr_time - old_ping_time_ >= 10000) {
    for (int port = 0; port < port_cnt_; port++) {
      void* empty = malloc(12);

      uint8_t* header = (uint8_t *) empty;

      header[0] = PING;
      
      header[1] = 0;

      uint16_t len, source, dest;
      uint32_t timestamp;

      len = htons(12);
      source = htons(identifier_);
      dest = htons(0);
      timestamp = htonl(curr_time);

      memcpy(header + 2, &len, 2);
      memcpy(header + 4, &source, 2);
      memcpy(header + 6, &dest, 2);
      memcpy(header + 8, &timestamp, 4);

      sys -> send(port, empty, 12);

    }

    old_ping_time_ = curr_time;
  }

  // live?
  for (int port = 0; port < port_cnt_; port++) {
    if (!nb_secure[port]) {
      continue;
    }

    if (curr_time - nb_p_pong[port] >= 15000.0) {

      nb_secure[port] = 0;

      if (pt_ == P_DV) {
        for (int loc = 0; loc <= highest_identifier_; loc++) {
          if (route_port[loc] == port && route_good[loc] > 0) {
            route_port[loc] = 65535;
            route_good[loc] = 0;

            route_weight[loc] = 1000000000u;
            flag_d = true;
          }
        }
      }
    }
  }

  if (pt_ == P_DV) {
    for (int loc = 0; loc <= highest_identifier_; loc++) {
      if (loc != identifier_ && curr_time - route_prev_ref[loc] >= 45000 && route_good[loc] > 0) {
        route_port[loc] = 65535;
        route_good[loc] = 0;

        route_weight[loc] = 1000000000u;
        flag_d = true;
      }
    }
  }

  if (flag_d ==true && pt_ == P_DV) {
    for (int port_idx = 0; port_idx < port_cnt_; port_idx++) {
      int sum = 1;

      if (!nb_secure[port_idx]) {
        continue;
      }

      for (int loc = 0; loc <= highest_identifier_; loc++) {
        sum += (loc != identifier_ && route_good[loc]);
      }

      size_t sz = 4 * sum + 8;

      uint8_t *malloc_sz, *header_buf;

      uint16_t len1, source1, dest1;
      uint16_t dest2, wgt;

      uint16_t dest3, wgt2;
      
      malloc_sz = (uint8_t *) malloc(sz);

      uint8_t *curr_buf = malloc_sz + 8;

      header_buf = malloc_sz;
      header_buf[0] = DV;
      header_buf[1] = 0;

      source1 = htons(identifier_);
      dest1 = htons(nb_router_num[port_idx]);

      len1 = htons((uint16_t) sz);

      memcpy(header_buf + 2, &len1, 2);
      memcpy(header_buf + 4, &source1, 2);
      memcpy(header_buf + 6, &dest1, 2);

      wgt = htons(0);

      dest2 = htons(identifier_);
      
      memcpy(curr_buf, &dest2, 2);
      memcpy(curr_buf + 2, &wgt, 2);

      curr_buf += 4;

      // go over what's left (reverse)
      for (int loc = 0; loc <= highest_identifier_; loc++) {
        uint16_t next_wgt;

        if (loc == identifier_ || !route_good[loc]) {
          continue;
        }

        if (port_idx != route_port[loc]) {
          next_wgt = (uint16_t) min<uint32_t>(65535u, route_weight[loc]);
        }
        else {
          next_wgt = 65535;
        }

        dest3 = htons((uint16_t) loc);
        wgt2 = htons(next_wgt);

        memcpy(curr_buf, &dest3, 2);
        memcpy(curr_buf + 2, &wgt2, 2);

        curr_buf += 4;
      }

      sys -> send(port_idx, malloc_sz, (unsigned short) sz);
    }
  }

  // every thirty sec
  if (curr_time - dv_prev_tm_ >= 30000u && pt_ == P_DV) {
    for (int port_idx = 0; port_idx < port_cnt_; port_idx++) {
      int sum = 1;

      if (!nb_secure[port_idx]) {
        continue;
      }

      for (int loc = 0; loc <= highest_identifier_; loc++) {
        sum += (loc != identifier_ && route_good[loc]);
      }

      size_t sz = 4 * sum + 8;

      uint8_t *malloc_sz, *header_buf;

      uint16_t len1, source1, dest1;
      uint16_t dest2, wgt;

      uint16_t dest3, wgt2;
      
      malloc_sz = (uint8_t *) malloc(sz);

      uint8_t *curr_buf = malloc_sz + 8;

      header_buf = malloc_sz;
      header_buf[0] = DV;
      header_buf[1] = 0;

      source1 = htons(identifier_);
      dest1 = htons(nb_router_num[port_idx]);

      len1 = htons((uint16_t) sz);

      memcpy(header_buf + 2, &len1, 2);
      memcpy(header_buf + 4, &source1, 2);
      memcpy(header_buf + 6, &dest1, 2);

      wgt = htons(0);

      dest2 = htons(identifier_);
      
      memcpy(curr_buf, &dest2, 2);
      memcpy(curr_buf + 2, &wgt, 2);

      curr_buf += 4;

      // reverse
      for (int loc = 0; loc <= highest_identifier_; loc++) {
        uint16_t next_wgt;

        if (loc == identifier_ || !route_good[loc]) {
          continue;
        }

        if (port_idx != route_port[loc]) {
          next_wgt = (uint16_t) min<uint32_t>(65535u, route_weight[loc]);
        }
        else {
          next_wgt = 65535;
        }

        dest3 = htons((uint16_t) loc);
        wgt2 = htons(next_wgt);

        memcpy(curr_buf, &dest3, 2);
        memcpy(curr_buf + 2, &wgt2, 2);

        curr_buf += 4;
      }

      sys -> send(port_idx, malloc_sz, (unsigned short) sz);
    }

    dv_prev_tm_ = curr_time;
  }

  sys -> set_alarm(this, 1000, nullptr);
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
  
  uint8_t* recv_buf = (uint8_t *) packet;

  // uint8_t tp = recv_buf[0];

  if (size < 8) {
    free(packet);

    return;
  }

  if (size >= 8 && recv_buf[0] == DATA) {
    uint16_t dest;

    memcpy(&dest, recv_buf + 6, 2);

    dest = ntohs(dest);

    if (identifier_ == dest) {

      free(packet);

      return;
    }

    for (int port_idx = 0; port_idx < port_cnt_; port_idx++) {
      if (!nb_secure[port_idx] || nb_router_num[port_idx] != dest) {
        continue;
      }

      sys -> send(port_idx, packet, size);

      return;
    }

    if (pt_ == P_DV) {
      uint16_t curr_dest, leave;

      curr_dest = dest;

      if (curr_dest <= highest_identifier_ && route_good[curr_dest] > 0) {
        leave = route_port[curr_dest];

        if (leave != 65535 && leave < port_cnt_ && nb_secure[leave]) {
          sys -> send(leave, packet, size);


          return;
        }
      }
    }


    free(packet);

    return;
  }

  // ping to pong
  if (recv_buf[0] == PING) {
    uint16_t source1, source2;

    uint16_t origin, dest;

    memcpy(&source1, recv_buf + 4, 2);

    source2 = ntohs(source1);

    recv_buf[0] = PONG;

    origin = htons(identifier_);
    dest = htons(source2);

    memcpy(recv_buf + 4, &origin, 2);
    memcpy(recv_buf + 6, &dest, 2);

    sys -> send(port, packet, size);
    

    return;
  }

  // pong upd
  if (recv_buf[0] == PONG) {
    if (size >= 12) {
      uint32_t timestamp;
      uint32_t curr, round_trip;

      memcpy(&timestamp, recv_buf + 8, 4);

      timestamp = ntohl(timestamp);

      curr = sys -> time();
      
      round_trip = 0;
      if (curr >= timestamp) {
        round_trip = curr - timestamp;
      }

      uint16_t num;

      memcpy(&num, recv_buf + 4, 2);

      num = ntohs(num);

      if (num > highest_identifier_) {
        highest_identifier_ = num;
      }

      uint32_t curr_time = sys -> time();

      bool is_secure = (nb_secure[port] != 0);

      //neighbor info
      nb_secure[port] = 1;
      nb_router_num[port] = num;

      nb_p_pong[port] = curr_time;
      nb_round_trip[port] = round_trip;

      route_prev_ref[num] = curr_time;

      bool flag = false;

      if (pt_ == P_DV) {
        if (route_port[num] != port || !route_good[num] || route_weight[num] != nb_round_trip[port]) {
          flag = true;

          route_port[num] = port;
          route_good[num] = 1;
          route_weight[num] = nb_round_trip[port];
          route_prev_ref[num] = curr_time;
        }
      }

      // neighbor DV
      if (flag == true && pt_ == P_DV) {
        for (int port_i = 0; port_i < port_cnt_; port_i++) {
          if (nb_secure[port_i]) {
            int sum = 1;

            for (int loc = 0; loc <= highest_identifier_; loc++) {
              sum += (loc != identifier_ && route_good[loc]);
            }

            uint8_t *malloc_sz, *header_buf;

            uint8_t *c_buf;

            size_t sz = 4 * sum + 8;

            uint16_t len1, source1, dest1;
            uint16_t n_d, n_wgt;

            malloc_sz = (uint8_t *) malloc(sz);

            header_buf = malloc_sz;

            header_buf[0] = DV;

            header_buf[1] = 0;

            c_buf = malloc_sz + 8;

            len1 = htons((uint16_t) sz);

            source1 = htons(identifier_);
            dest1 = htons(nb_router_num[port_i]);

            memcpy(header_buf + 2, &len1, 2);
            memcpy(header_buf + 4, &source1, 2);
            memcpy(header_buf + 6, &dest1, 2);

            n_d = htons(identifier_);

            n_wgt = htons(0);

            memcpy(c_buf, &n_d, 2);
            memcpy(c_buf + 2, &n_wgt, 2);

            c_buf += 4;

            for (int loc = 0; loc <= highest_identifier_; loc++) {
              // reverse
              if (loc != identifier_ && route_good[loc]) {
                uint16_t n_d2, n_wgt2;
                uint16_t prc;

                if (route_port[loc] == port_i) {
                  prc = 65535;
                }
                else {
                  prc = (uint16_t) min(route_weight[loc], 65535u);
                }

                n_wgt2 = htons(prc);

                n_d2 = htons(loc);

                memcpy(c_buf, &n_d2, 2);
                memcpy(c_buf + 2, &n_wgt2, 2);

                c_buf += 4;
              }
            }

            sys -> send(port_i, malloc_sz, (unsigned short) sz);
          }
        }
      }

    }

    free(packet);

    return;
  }

  if (recv_buf[0] == DV) {
    if (pt_ != P_DV || !nb_secure[port] || size < 8 || ((size - 8) % 4)!=0) {
      free(packet);

      return;
    }

    uint8_t *receive_buf;
    uint8_t *finish_buf;

    uint16_t nb_num;

    uint32_t curr_tm;
    bool diff = false;

    memcpy(&nb_num, recv_buf + 4, 2);

    nb_num = ntohs(nb_num);

    curr_tm = sys -> time();

    receive_buf = recv_buf + 8;

    finish_buf = recv_buf + size;

    while (receive_buf < finish_buf) {
      uint16_t curr_destination;
      uint16_t curr_weight_temp;

      uint32_t curr_weight;

      memcpy(&curr_destination, receive_buf, 2);
      memcpy(&curr_weight_temp, receive_buf + 2, 2);

      curr_weight = (uint32_t) ntohs(curr_weight_temp);

      curr_destination = ntohs(curr_destination);

      if (curr_destination > highest_identifier_) {
        highest_identifier_ = curr_destination;
      }

      //get cost
      if (curr_destination != identifier_) {
        uint32_t curr_price;

        if (curr_weight >= 65535) {
          curr_price = 1000000000;
        }
        else {
          curr_price = curr_weight + nb_round_trip[port];
        }

        if (curr_price >= 1000000000) {
          if (route_port[curr_destination] == port && route_good[curr_destination]) {
            diff = true;

            route_good[curr_destination] = 0;
            route_port[curr_destination] = 65535;

            route_weight[curr_destination] = 1000000000;
          }
        }
        else {
          bool f = false;

          if (!route_good[curr_destination] || curr_price < route_weight[curr_destination]) {
            f = true;
          }

          if (curr_price != route_weight[curr_destination] && route_port[curr_destination] == port) {
            f = true;
          }

          if (port == route_port[curr_destination]) {
            route_prev_ref[curr_destination] = curr_tm;
          }

          if (f == true) {
            diff = true;

            route_port[curr_destination] = port;
            route_good[curr_destination] = 1;

            route_weight[curr_destination] = curr_price;
            route_prev_ref[curr_destination] = curr_tm;
          }
        }

      }
      receive_buf += 4;
    }

    if (pt_ == P_DV && diff == true) {
      for (int port_i = 0; port_i < port_cnt_; port_i++) {
        if (port_i != port && nb_secure[port_i]) {
          int sm = 1;

          for (int loc = 0; loc <= highest_identifier_; loc++) {
            sm += (loc != identifier_ && route_good[loc]);
          }

          uint8_t *temp_header;
          uint8_t *header_buf;

          uint8_t *weight_dest;

          uint16_t length_b, source_b, dest_b;

          temp_header = (uint8_t *) malloc(4 * sm + 8);

          header_buf = temp_header;

          header_buf[0] = DV;
          header_buf[1] = 0;

          length_b = htons(4 * sm + 8);

          source_b = htons(identifier_);
          dest_b = htons(nb_router_num[port_i]);

          memcpy(header_buf + 2, &length_b, 2);
          memcpy(header_buf + 4, &source_b, 2);
          memcpy(header_buf + 6, &dest_b, 2);

          weight_dest = temp_header + 8;

          uint16_t dest_me, wgt_me;

          dest_me = htons(identifier_);

          wgt_me = htons(0);

          memcpy(weight_dest, &dest_me, 2);
          memcpy(weight_dest + 2, &wgt_me, 2);

          weight_dest += 4;

          // reverse
          for (int loc = 0; loc <= highest_identifier_; loc++) {
            uint16_t move_price;

            if (loc != identifier_ && route_good[loc]) {
              if (port_i != route_port[loc]) {
                move_price = (uint16_t) min(65535u, route_weight[loc]);
              }
              else {
                move_price = 65535;
              }

              uint16_t net_dest;
              uint16_t net_wgt;

              net_dest = htons(loc);

              net_wgt = htons(move_price);

              memcpy(weight_dest, &net_dest, 2);
              memcpy(weight_dest + 2, &net_wgt, 2);
              
              weight_dest += 4;
            }
          }

          sys -> send(port_i, temp_header, (unsigned short) (4 * sm + 8));
        }
      }
    }
  }

  free(packet);

  return;
}

// add more of your own code

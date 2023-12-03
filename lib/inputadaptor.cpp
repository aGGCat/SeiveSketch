#include "inputadaptor.hpp"
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <IPv4Layer.h>
#include <TcpLayer.h>
#include <UdpLayer.h>
#include <Packet.h>
#include <PcapFileDevice.h>
#include <arpa/inet.h>

InputAdaptor::InputAdaptor(std::string dir, std::string filename, uint64_t buffersize) {
    data = (adaptor_t*)calloc(1, sizeof(adaptor_t));
    data->databuffer = (unsigned char*)calloc(buffersize, sizeof(unsigned char));
    data->ptr = data->databuffer;
    data->cnt = 0;
    data->cur = 0;
    //Read pcap file
    std::string path = dir+filename;
    pcpp::PcapFileReaderDevice reader(path.c_str());
    if (!reader.open()) {
        std::cout << "[Error] Fail to open pcap file" << std::endl;
        exit(-1);
    }
    
    unsigned char* p = data->databuffer;
    pcpp::RawPacket rawPacket;
    std::cout << "begin read packets" << std::endl;
    while (reader.getNextPacket(rawPacket)) {
        pcpp::Packet parsedPacket(&rawPacket,pcpp::IPv4);
        if(parsedPacket.isPacketOfType(pcpp::IPv4)) {
            pcpp::IPv4Layer* ipLayer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
            uint16_t size = (uint16_t)ntohs(ipLayer->getIPv4Header()->totalLength);
            uint32_t ipsrc = (uint32_t)ntohl(ipLayer->getIPv4Header()->ipSrc);
            uint32_t ipdst = (uint32_t)ntohl(ipLayer->getIPv4Header()->ipDst);

            if (p+sizeof(edge_tp) < data->databuffer + buffersize) {
                memcpy(p, &ipsrc, sizeof(uint32_t));
                memcpy(p+sizeof(uint32_t), &ipdst, sizeof(uint32_t));
                memcpy(p+sizeof(uint32_t)*2, &(size), sizeof(uint16_t));
                p += sizeof(uint32_t)*2+sizeof(uint16_t);
                data->cnt++;
            }  else break;
            // if(data->cnt == 35722890)
            //     std::cout<<ipsrc<<" "<<ipdst<<std::endl;
        }
    }
    std::cout << "[Message] Read " << data->cnt << " items" << std::endl;
    reader.close();
}

/*
 * Read graph stream dataset
 * */
InputAdaptor::InputAdaptor(std::string dir, std::string filename, uint64_t buffersize, int type) {
    data = (adaptor_t*)calloc(1, sizeof(adaptor_t));
    data->databuffer = (unsigned char*)calloc(buffersize, sizeof(unsigned char));
    data->ptr = data->databuffer;
    data->cnt = 0;
    data->cur = 0;
    //Read graph stream file
    std::string path = dir+filename;
    std::ifstream file(path.c_str(), std::ios_base::in);
    if (!file.is_open()) {
        std::cout << "[Error] Fail to open graph stream file" << std::endl;
        exit(-1);
    }
    uint32_t src, dst;
    double weight, timestamp;
    unsigned char* p = data->databuffer;
    while (file >> src >> dst >> weight >> timestamp) {
        if (p+sizeof(edge_tp) < data->databuffer + buffersize) {
            memcpy(p, &(src), sizeof(uint32_t));
            memcpy(p+sizeof(uint32_t), &(dst), sizeof(uint32_t));
            p += sizeof(uint8_t)*8;
            data->cnt++;
        }  else break;
    }
    std::cout << "[Message] Read " << data->cnt << " items" << std::endl;
    file.close();
}


InputAdaptor::~InputAdaptor() {
    free(data->databuffer);
    free(data);
}

int InputAdaptor::GetNext(edge_tp* t) {
    if (data->cur >= data->cnt) {
        return -1;
    }
    t->src_ip = *((uint32_t*)data->ptr);
    t->dst_ip = *((uint32_t*)(data->ptr+4));
    t->size = *((uint16_t*)(data->ptr+8));
    data->cur++;
    data->ptr += 10;
    return 1;
}

void InputAdaptor::Reset() {
    data->cur = 0;
    data->ptr = data->databuffer;
}

uint64_t InputAdaptor::GetDataSize() {
    return data->cnt;
}

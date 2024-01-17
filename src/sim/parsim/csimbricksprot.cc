#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include "omnetpp/cenvir.h"
#include "omnetpp/globals.h"
#include "omnetpp/cfutureeventset.h"
#include "omnetpp/regmacros.h"
#include "omnetpp/cconfiguration.h"
#include "omnetpp/cparsimcomm.h"
#include "omnetpp/ccommbuffer.h"
#include "omnetpp/csimplemodule.h"
#include "cparsimpartition.h"
#include "clinkdelaylookahead.h"
#include "cproxygate.h"
#include "csimbricksprot.h"
#include "messagetags.h"
#include "omnetpp/cconfigoption.h"
#include <x86intrin.h>



namespace omnetpp {

Register_Class(cSimbricksProtocol);
Register_GlobalConfigOption(CFGID_PARSIM_SIMBRICKSPROTOCOL_LOOKAHEAD_CLASS, "parsim-simbricksprotocol-lookahead-class", CFG_STRING, "cLinkDelayLookahead", "When `cNullMessageProtocol` is selected as parsim synchronization class: specifies the C++ class that calculates lookahead. The class should subclass from `cNMPLookahead`.");    

extern cConfigOption *CFGID_PARSIM_DEBUG;

std::map<uint32_t, SimbricksNetIf*> cSimbricksProtocol::m_nsif;
std::map<uint32_t, SimbricksProtoNetIntro*> cSimbricksProtocol::m_net_intro;
std::map<uint32_t, SimbricksBaseIfParams*> cSimbricksProtocol::m_bifparam;
std::map<uint32_t, SimbricksBaseIfSHMPool*> cSimbricksProtocol::m_pool;
std::string cSimbricksProtocol::m_dir;
unsigned cSimbricksProtocol::n_bifs = 0;
struct SimBricksBaseIfEstablishData* cSimbricksProtocol::ests;

// simtime_t a;

// static bool flag = false;
// static const_simtime_t thresh1(0.01);
// static const_simtime_t thresh2(0.02);
// static uint64_t tsc1=0, tsc2=0, tsc3;

cSimbricksProtocol::cSimbricksProtocol() : cParsimSynchronizer()
{
    debug = getEnvir()->getConfig()->getAsBool(CFGID_PARSIM_DEBUG);
    std::string lookhClass = getEnvir()->getConfig()->getAsString(CFGID_PARSIM_SIMBRICKSPROTOCOL_LOOKAHEAD_CLASS);
    lookaheadcalc = dynamic_cast<cNMPLookahead *>(createOne(lookhClass.c_str()));
    if (!lookaheadcalc) \
        throw cRuntimeError("Class \"%s\" is not subclassed from cNMPLookahead", lookhClass.c_str());
}

cSimbricksProtocol::~cSimbricksProtocol()
{
    delete lookaheadcalc;
    for(auto i = m_bifparam.begin(); i != m_bifparam.end(); i++) delete i->second;
    for(auto i = m_nsif.begin(); i != m_nsif.end(); i++) delete i->second;
    for(auto i = m_net_intro.begin(); i != m_net_intro.end(); i++) delete i->second;
    for(auto i = m_pool.begin(); i != m_pool.end(); i++) delete i->second;
}

void cSimbricksProtocol::startRun()
{
    EV << "starting Simbrcks Message Protocol...\n";
    m_dir = "/dev/shm/env/";
    
    lookaheadcalc->startRun();
    buffer = new cMemCommBuffer();
    SetupInterconnections();

    std::ofstream out(m_dir+"mapping_"+std::to_string(comm->getProcId())+".txt");

    for (int i = 0; i < comm->getNumPartitions(); i++) {
        if (i != comm->getProcId() && lookaheadcalc->getCurrentLookahead(i)!=SIMTIME_MAX) {
        // if (i != comm->getProcId()) {
            // sprintf(buf, "sendSync-%d", i);
            cMessage *syncMsg = new cMessage(buf, 777);
            syncMsg->setContextPointer((void *)(uintptr_t)i);
            syncMsg->setArrivalTime(0.0);
            sim->getFES()->insert(syncMsg);
            out << i << "\n";
        }
    }

    out.close();

    EV << "  setup done.\n";
}

void cSimbricksProtocol::endRun()
{
    lookaheadcalc->endRun();
}

SendOptions cSimbricksProtocol::unpackOptions(cCommBuffer *buffer)
{
    SendOptions options;
    buffer->unpack(options.sendDelay);
    buffer->unpack(options.propagationDelay_);
    buffer->unpack(options.duration_);
    buffer->unpack(options.transmissionId_);
    buffer->unpack(options.remainingDuration);
    return options;
}

void cSimbricksProtocol::setContext(cSimulation *sim, cParsimPartition *seg, cParsimCommunications *co)
{
    cParsimSynchronizer::setContext(sim, seg, co);
    lookaheadcalc->setContext(sim, seg, co);
}

void cSimbricksProtocol::packOptions(cCommBuffer *buffer, const SendOptions& options)
{
    buffer->pack(options.sendDelay);
    buffer->pack(options.propagationDelay_);
    buffer->pack(options.duration_);
    buffer->pack(options.transmissionId_);
    buffer->pack(options.remainingDuration);
}

volatile union SimbricksProtoNetMsg* cSimbricksProtocol::AllocTx (int systemId)
{
  volatile union SimbricksProtoNetMsg *msg;
  do {    
    msg = SimbricksNetIfOutAlloc (m_nsif[systemId], sim->getSimTime().inUnit(SIMTIME_PS));
  } while (!msg);
  
  return msg;
}

void cSimbricksProtocol::processOutgoingMessage(cMessage *msg, const SendOptions& options, int destProcId, int destModuleId, int destGateId, void *data)
{
    {if (debug) EV << "sending '" << msg->getName() << "' to " << destProcId << "\n";}
    volatile union SimbricksProtoNetMsg *ms;
    volatile struct SimbricksProtoNetMsgPacket *recv;

    ms = AllocTx (destProcId);
    recv = &ms->packet;
    
    buffer->reset();
    buffer->setBuffer((char*)recv->data, INT32_MAX);
    buffer->pack(destModuleId);
    buffer->pack(destGateId);
    packOptions(buffer, options);
    buffer->packObject(msg);

    recv->len = buffer->getMessageSize();
    // std:: cout << "Send len: " << recv->len << " to " << destProcId << "\n";
    // std::flush(std::cout);
    recv->port = 0;

    // std::memcpy((uint8_t*)recv->data, buffer->getBuffer(), buffer->getMessageSize());
    SimbricksNetIfOutSend(m_nsif[destProcId], ms, SIMBRICKS_PROTO_NET_MSG_PACKET);
    // std::cout << sim->getSimTime().inUnit(SIMTIME_PS) << " " << msg->getArrivalTime() << " " << lookaheadcalc->getCurrentLookahead(destProcId) << "\n";
    // delete buffer;
}

// static int deb = 0;

void cSimbricksProtocol::ReceivedPacket (const void *buf, size_t len, int systemId)
{ 
    int destModuleId;
    int destGateId;
    buffer->reset();
    buffer->setBuffer((char*)buf, INT32_MAX);
    buffer->setMessageSize(len);

    buffer->unpack(destModuleId);
    buffer->unpack(destGateId);
    SendOptions options = unpackOptions(buffer);
    cMessage *msg = (cMessage *)buffer->unpackObject();
    // if(msg->getArrivalTime() < sim->getSimTime()){
    //     deb++;
    //     std::cout<<deb<<"\n";
    //     std::flush(std::cout);
    //     return;
    // }
    partition->processReceivedMessage(msg, options, destModuleId, destGateId, systemId);
    buffer->assertBufferEmpty();
    // std:: cout << len << "\n";
    // std::flush(std::cout);
}

cEvent *cSimbricksProtocol::takeNextEvent()
{
    // our EIT and resendEOT messages are always scheduled, so the FES can
    // only be empty if there are no other partitions at all -- "no events" then
    // means we're finished.
    if (sim->getFES()->isEmpty())
        return nullptr;


    cEvent *event;
    while (true) {
        event = sim->getFES()->peekFirst();
        cMessage *msg = event->isMessage() ? static_cast<cMessage *>(event) : nullptr;
        if (msg && msg->getKind() == 777) {
            int procId = (uintptr_t)msg->getContextPointer();
            SendSyncEvent(procId, msg->getArrivalTime());
            delete sim->getFES()->removeFirst();
            continue;
        }
        else {
            // just a normal event -- go ahead with it
            break;
        }
    }

    // remove event from FES and return it
    cEvent *tmp = sim->getFES()->removeFirst();
    ASSERT(tmp == event);
    return event;
}

void cSimbricksProtocol::putBackEvent(cEvent *event)
{
    throw cRuntimeError("cSimbricksProtocol: \"Run Until Event/Module\" functionality "
                        "cannot be used with this scheduler (putBackEvent() not implemented)");
}

void cSimbricksProtocol::threadFunc(int i){
    int systemId = comm->getProcId();
    m_bifparam[i] = new SimbricksBaseIfParams();
    SimbricksNetIfDefaultParams(m_bifparam[i]);
    m_bifparam[i]->in_entries_size = m_bifparam[i]->out_entries_size = 512;

    m_bifparam[i]->sync_mode = (enum SimbricksBaseIfSyncMode)1;
    m_bifparam[i]->sync_interval = lookaheadcalc->getCurrentLookahead(i).inUnit(SIMTIME_PS);
    m_bifparam[i]->link_latency = lookaheadcalc->getCurrentLookahead(i).inUnit(SIMTIME_PS);

    int a,b;
    if(systemId>i){
      a=i;
      b=systemId;
    }
    else{
      a=systemId;
      b=i;
    }

    std::string shm_path = m_dir+"sim_shm"+std::to_string(a)+"_"+std::to_string(b), sock_path=m_dir+"sim_socket"+std::to_string(a)+"_"+std::to_string(b);
    m_bifparam[i]->sock_path = sock_path.c_str();
    int sync = m_bifparam[i]->sync_mode;
    m_nsif[i] = new SimbricksNetIf();
    if(a==systemId) {std::cout<<"RID "<<systemId<<" waiting for "<<sock_path<<"\n"; while(access(sock_path.c_str(), R_OK));}
    else {
        m_pool[i] = new SimbricksBaseIfSHMPool();
        struct SimbricksBaseIf *netif = &m_nsif[i]->base;

        // first allocate pool
        size_t shm_size = 3200*8192;
        if (SimbricksBaseIfSHMPoolCreate(m_pool[i], shm_path.c_str(), shm_size)) {
        perror("SimbricksNicIfInit: SimbricksBaseIfSHMPoolCreate failed");
        exit(0);
        return;
        }
        if (SimbricksBaseIfInit(netif, m_bifparam[i])) {
        perror("SimbricksNicIfInit: SimbricksBaseIfInit net failed");
        exit(0);
        return;
        }
        if (SimbricksBaseIfListen(netif, m_pool[i])) {
        perror("SimbricksNicIfInit: SimbricksBaseIfListen net failed");
        exit(0);
        return;
        }
        m_net_intro[i] = new SimbricksProtoNetIntro();
        memset(m_net_intro[i], 0, sizeof(*m_net_intro[i]));
        ests[n_bifs].base_if = netif;
        ests[n_bifs].tx_intro = m_net_intro[i];
        ests[n_bifs].tx_intro_len = sizeof(*m_net_intro[i]);
        ests[n_bifs].rx_intro = m_net_intro[i];
        ests[n_bifs].rx_intro_len = sizeof(*m_net_intro[i]);
        
        ++n_bifs;
        // SimBricksBaseIfEstablish(ests, n_bifs);
        return;
    }
    if(SimbricksNetIfInit(m_nsif[i], m_bifparam[i], m_bifparam[i]->sock_path, &sync)) exit(0);
}

void cSimbricksProtocol::SetupInterconnections (){
    ests = (struct SimBricksBaseIfEstablishData*)malloc(sizeof(struct SimBricksBaseIfEstablishData)*comm->getNumPartitions());
    std::cerr<<"RID "<<comm->getProcId()<<"\n";
    std::flush(std::cerr);
    // for(int i=0;i<comm->getNumPartitions();i++){
    //     if(i==comm->getProcId()) continue;
    //     threadFunc(i);
    // }
    for(int i=0;i<comm->getNumPartitions();i++){
        if(i==comm->getProcId() || lookaheadcalc->getCurrentLookahead(i)==SIMTIME_MAX) continue;
        threadFunc(i);
    }
    if(n_bifs) SimBricksBaseIfEstablish(ests, n_bifs);
    delete[] ests;
}

void cSimbricksProtocol::SendSyncEvent (int systemId, simtime_t at)
{

    volatile union SimbricksProtoNetMsg *msg = AllocTx (systemId);
    SimbricksBaseIfOutSend(&m_nsif[systemId]->base, &msg->base, SIMBRICKS_PROTO_MSG_TYPE_SYNC);
    // int cnt = 0;
    // if(at > thresh1 && at < thresh2 && !flag){
    //     flag=true;
    //     tsc2=0;
    //     tsc1=__rdtsc();
    // }
    // tsc3 = __rdtsc();
    uint8_t ret = Poll (systemId);
    while (ret){
        ret = Poll (systemId);
        // if(ret==255 && flag){
            // tsc2 += __rdtsc()-tsc3;
        // }
        // tsc3 = __rdtsc();
    }
    // if(at > thresh2 && flag){
    //     flag = false;
    //     std::cout << (double)tsc2 /(__rdtsc() - tsc1) << "\n";
    //     std::flush(std::cout);
    // }
    // std::cout << comm->getProcId() << " got sync from "<< systemId << "\n";
    // std::flush(std::cout);

    // sprintf(buf, "sendSync-%d", systemId);
    cMessage *syncMsg = new cMessage(buf, 777);
    syncMsg->setContextPointer((void *)(uintptr_t)systemId);
    syncMsg->setArrivalTime(at + lookaheadcalc->getCurrentLookahead(systemId));
    sim->getFES()->insert(syncMsg);
}

uint8_t cSimbricksProtocol::Poll (int systemId)
{
    volatile union SimbricksProtoNetMsg *msg;
    uint8_t ty;
    // std::cout<<"Time: "<<sim->getSimTime().inUnit(SIMTIME_PS) << " "  << lookaheadcalc->getCurrentLookahead(systemId).inUnit(SIMTIME_PS)<<"\n";
    // std::flush(std::cout);

    msg = SimbricksNetIfInPoll (m_nsif[systemId], INT64_MAX);


    if (!msg){
        return 255;
    }

    ty = SimbricksNetIfInType(m_nsif[systemId], msg);
    switch (ty) {
    case SIMBRICKS_PROTO_NET_MSG_PACKET:
        ReceivedPacket ((const void *) msg->packet.data, msg->packet.len, systemId);
        break;

    case SIMBRICKS_PROTO_MSG_TYPE_SYNC:
        break;

    default:
        break;
    }

    SimbricksNetIfInDone (m_nsif[systemId], msg);
    return ty;
}

}

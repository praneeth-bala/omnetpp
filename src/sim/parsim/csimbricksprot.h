//=========================================================================
//  CSimbricksPROT.H - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//=========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2017 Andras Varga
  Copyright (C) 2006-2017 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#ifndef __OMNETPP_CSIMBRICKSSYNC_H
#define __OMNETPP_CSIMBRICKSSYNC_H

#include "omnetpp/cmessage.h"
#include "cparsimsynchr.h"
#include <string>
#include <map>
#include <vector>
#include <simbricks/base/cxxatomicfix.h>
#include <unistd.h>
extern "C" {
#include <simbricks/network/if.h>
#include <simbricks/network/proto.h>
#include <simbricks/nicif/nicif.h>
}


namespace omnetpp {

class cNMPLookahead;

class SIM_API cSimbricksProtocol : public cParsimSynchronizer
{
  public:
    
    bool debug;
    char buf[30];
    cNMPLookahead *lookaheadcalc;
    static std::map<uint32_t, SimbricksNetIf*> m_nsif;
    static std::map<uint32_t, SimbricksBaseIfParams*> m_bifparam;
    static std::map<uint32_t, uint64_t> m_pollDelay;
    static std::string m_dir;

    /**
     * Constructor.
     */
    cSimbricksProtocol();

    /**
     * Destructor.
     */
    virtual ~cSimbricksProtocol();

    /**
     * Called at the beginning of a simulation run.
     */
    virtual void startRun() override;

    /**
     * Called at the end of a simulation run.
     */
    virtual void endRun() override;

    /**
     * Scheduler function. The null message algorithm is embedded here.
     */
    virtual cEvent *takeNextEvent() override;

    /**
     * Undo takeNextEvent() -- it comes from the cScheduler interface.
     */
    virtual void putBackEvent(cEvent *event) override;

    virtual void setContext(cSimulation *sim, cParsimPartition *seg, cParsimCommunications *co) override;

    /**
     * In addition to its normal task (sending out the cMessage to the
     * given partition), it also does lookahead calculation and optional
     * piggybacking of null message on the cMessage.
     */
    SendOptions unpackOptions(cCommBuffer *buffer);
    void packOptions(cCommBuffer *buffer, const SendOptions& options);

    virtual void processReceivedBuffer(cCommBuffer *buffer, int tag, int sourceProcId);
    virtual void processOutgoingMessage(cMessage *msg, const SendOptions& options, int procId, int moduleId, int gateId, void *data) override;
    virtual void receiveBlocking();
    virtual void SetupInterconnections ();
    virtual void SendSyncEvent (int systemId);
    virtual uint8_t Poll (int systemId);
    virtual void ReceivedPacket (const void *buf, size_t len, int systemId);
    virtual volatile union SimbricksProtoNetMsg* AllocTx (int systemId);
};

}  // namespace omnetpp


#endif

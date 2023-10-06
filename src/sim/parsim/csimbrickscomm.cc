#include <cstdio>
#include "omnetpp/clog.h"
#include "omnetpp/globals.h"
#include "omnetpp/regmacros.h"
#include "omnetpp/cenvir.h"
#include "omnetpp/csimulation.h"
#include "omnetpp/cconfiguration.h"
#include "omnetpp/cconfigoption.h"
#include "omnetpp/platdep/platmisc.h"
#include "csimbrickscomm.h"
#include "parsimutil.h"

namespace omnetpp {

Register_Class(cSimbricksCommunications);


cSimbricksCommunications::cSimbricksCommunications()
{

}

cSimbricksCommunications::~cSimbricksCommunications()
{
    // do nothing: Simbricks_finalize() has been called in shutdown()
}

void cSimbricksCommunications::init(int np)
{
    // store parameter
    numPartitions = np;

    // sanity check
    int argc = getEnvir()->getArgCount();
    char **argv = getEnvir()->getArgVector();
    // for (int i = 1; i < argc; i++)
    //     if (argv[i][0] == '-' && argv[i][1] == 'p')
    //         EV_WARN << "cSimbricksCommunications doesn't need -p command-line option, ignored\n";
    myRank = getProcIdFromCommandLineArgs(numPartitions, "cSimbricksCommunications");
}

void cSimbricksCommunications::shutdown()
{

}

int cSimbricksCommunications::getNumPartitions() const
{
    return numPartitions;
}

int cSimbricksCommunications::getProcId() const
{
    return myRank;
}

cCommBuffer *cSimbricksCommunications::createCommBuffer()
{
    // we pool only one reusable buffer -- additional buffers are created/deleted on demand
    return NULL;
}

void cSimbricksCommunications::recycleCommBuffer(cCommBuffer *buffer)
{
    return;
}

void cSimbricksCommunications::send(cCommBuffer *buffer, int tag, int destination)
{
    return;
}

void cSimbricksCommunications::broadcast(cCommBuffer *buffer, int tag)
{
    return;
}

bool cSimbricksCommunications::receiveBlocking(int filtTag, cCommBuffer *buffer, int& receivedTag, int& sourceProcId)
{
    return true;
}

bool cSimbricksCommunications::receiveNonblocking(int filtTag, cCommBuffer *buffer, int& receivedTag, int& sourceProcId)
{
    return false;
}

}  // namespace omnetpp


/*******************************************************************
*
*    This library is free software, you can redistribute it
*    and/or modify
*    it under  the terms of the GNU Lesser General Public License
*    as published by the Free Software Foundation;
*    either version 2 of the License, or any later version.
*    The library is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*    See the GNU Lesser General Public License for more details.
*
*
*********************************************************************/
#include "MPLSModule.h"
#include "TED.h"

#include "stlwatch.h"


Define_Module(TED);


int TED::tedModuleId;


std::ostream & operator<<(std::ostream & os, const TELinkState & linkstate)
{
    os << "AdvRte:" << linkstate.advrouter;
    os << "  t:" << linkstate.type;
    os << "  id:" << linkstate.linkid;
    os << "  loc:" << linkstate.local;
    os << "  rem:" << linkstate.remote;
    os << "  M:" << linkstate.metric;
    os << "  BW:" << linkstate.MaxBandwith;
    os << "  rBW:" << linkstate.MaxResvBandwith;
    os << "  urBW[0]:" << linkstate.UnResvBandwith[0];
    os << "  AdmGrp:" << linkstate.AdminGrp;
    return os;
};

TED *TED::getGlobalInstance()
{
    cModule *m;
    if (tedModuleId == 0 || dynamic_cast < TED * >(m = simulation.module(tedModuleId)) == NULL)
        opp_error("TED::getGlobalInstance(): TED module not yet initialized");
    return (TED *) m;
}


void TED::initialize()
{
    if (dynamic_cast < TED * >(simulation.module(tedModuleId)) != NULL)
        opp_error("A TED module already exists in the network -- there should be only one");
    tedModuleId = id();

    buildDatabase();
    printDatabase();

    WATCH_VECTOR(ted);
}


const std::vector < TELinkState > &TED::getTED()
{
    Enter_Method("getTED()");
    return ted;
}


void TED::updateTED(const std::vector < TELinkState > &copy)
{
    Enter_Method("updateTED()");
    ted = copy;
}



void TED::buildDatabase()
{
    if (!(ted.empty()))
        ted.clear();

    TELinkState *entry = new TELinkState;

    cTopology topo;
    topo.extractByModuleType("TCPClientTest", "TCPServerTest", "RSVP_LSR_Node", NULL);
    ev << "Total number of RSVP LSR nodes = " << topo.nodes() << "\n";

    for (int i = 0; i < topo.nodes(); i++)
    {
        sTopoNode *node = topo.node(i);
        cModule *module = node->module();
        // Get the MPLS componet  FIXME
        RoutingTable *myRT =
            (RoutingTable *) (module->submodule("networkLayer")->submodule("routingTable"));

        // Get the RoutingTable component - Todo: Set it public
        entry->advrouter = IPAddress(module->par("local_addr").stringValue());

        // *(myRT->getLoopbackAddress()); // Todo: Add this function to rt

        for (int j = 0; j < node->outLinks(); j++)
        {

            cModule *neighbour = node->out(j)->remoteNode()->module();

            RoutingTable *neighbourRT =
                check_and_cast <
                RoutingTable * >(neighbour->submodule("networkLayer")->submodule("routingTable"));

            // For each link
            // Get linkId
            entry->linkid = IPAddress(neighbour->par("local_addr").stringValue());

            int local_gateIndex = node->out(j)->localGate()->index();
            int remote_gateIndex = node->out(j)->remoteGate()->index();

            // Get local address
            entry->local = myRT->interfaceByPortNo(local_gateIndex)->inetAddr;
            // Get remote address
            entry->remote = neighbourRT->interfaceByPortNo(remote_gateIndex)->inetAddr;

            double BW = node->out(j)->localGate()->datarate()->doubleValue();
            double delay = node->out(j)->localGate()->delay()->doubleValue();
            entry->MaxBandwith = BW;
            entry->MaxResvBandwith = BW;
            entry->metric = delay;
            for (int k = 0; k < 8; k++)
                entry->UnResvBandwith[k] = BW;
            entry->type = 1;


            ted.push_back(*entry);
        }
    }

    printDatabase();

}

void TED::handleMessage(cMessage *)
{
    error
        ("Message arrived to TED -- it doesn't process messages, it is used via direct method calls");
}


void TED::printDatabase()
{
    ev << "*************TED DATABASE*******************\n";
    std::vector < TELinkState >::iterator i;
    for (i = ted.begin(); i != ted.end(); i++)
    {
        const TELinkState & linkstate = *i;
        ev << "Adv Router: " << linkstate.advrouter << "\n";
        ev << "Link Id (neighbour IP): " << linkstate.linkid << "\n";
        ev << "Max Bandwidth: " << linkstate.MaxBandwith << "\n";
        ev << "Metric: " << linkstate.metric << "\n\n";
    }
}


void TED::updateLink(simple_link_t * aLink, double metric, double bw)
{
    Enter_Method("updateLink()");

    for (int i = 0; i < ted.size(); i++)
    {
        if ((ted[i].advrouter.getInt() == (aLink->advRouter)) &&
            (ted[i].linkid.getInt() == (aLink->id)))
        {
            ev << "TED update an entry\n";
            ev << "Advrouter=" << ted[i].advrouter << "\n";
            ev << "linkId=" << ted[i].linkid << "\n";
            double bwIncrease = bw - (ted[i].MaxBandwith);
            ted[i].MaxBandwith = ted[i].MaxBandwith + bwIncrease;
            ted[i].MaxResvBandwith = ted[i].MaxResvBandwith + bwIncrease;
            ev << "Old metric= " << ted[i].metric << "\n";

            ted[i].metric = metric;
            ev << "New metric= " << ted[i].metric << "\n";

            for (int j = 0; j < 8; j++)
                ted[i].UnResvBandwith[j] = ted[i].UnResvBandwith[j] + bwIncrease;
            break;
        }
    }
    printDatabase();
}

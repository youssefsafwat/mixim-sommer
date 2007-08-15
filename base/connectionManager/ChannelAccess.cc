/***************************************************************************
 * file:        ChannelAccess.cc
 *
 * author:      Marc Loebbers
 *

 * copyright:   (C) 2004 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 *
 *              This program is free software; you can redistribute it 
 *              and/or modify it under the terms of the GNU General Public 
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later 
 *              version.
 *              For further information see file COPYING 
 *              in the top level directory
 ***************************************************************************
 * part of:     framework implementation developed by tkn
 * description: - Base class for physical layers
 *              - if you create your own physical layer, please subclass 
 *                from this class and use the sendToChannel() function!!
 ***************************************************************************
 * changelog:   $Revision: 284 $
 *              last modified:   $Date: 2006-06-07 16:55:24 +0200 (Mi, 07 Jun 2006) $
 *              by:              $Author: willkomm $
 **************************************************************************/


#include "ChannelAccess.h"

#include "Move.h"

#include "FindModule.h"

#include <cassert>

/**
 * Upon initialization ChannelAccess registers the nic parent module
 * to have all its connections handeled by ConnectionManager
 **/
void ChannelAccess::initialize( int stage )
{
    BaseModule::initialize(stage);

    if( stage == 0 ){
        hasPar("coreDebug") ? coreDebug = par("coreDebug").boolValue() : coreDebug = false;

        cModule* nic = parentModule();
        if (nic->hasPar("connectionManagerName")){        		
			cc = dynamic_cast<BaseConnectionManager *>(simulation.moduleByPath(nic->par("connectionManagerName").stringValue()));
		} else {
			cc = FindModule<BaseConnectionManager *>::findGlobalModule();
		}
		
		if( cc == 0 ) error("Could not find connectionmanager module");
        		
		bu = FindModule<BaseUtility*>::findSubModule(getNode());
		if (bu == 0) error("Could not find BaseUtility module");

        // subscribe to position changes
        catMove = bu->subscribe(this, &move, findHost()->id());
        isRegistered = false;
	}
}


/**
 * This function has to be called whenever a packet is supposed to be
 * sent to the channel. Don't try to figure out what gates you have
 * and which ones are connected, this function does this for you!
 *
 * depending on which ConnectionManager module is used, the messages are
 * send via sendDirect() or to the respective gates.
 **/
void ChannelAccess::sendToChannel(cMessage *msg, double delay)
{
    const NicEntry::GateList& gateList = cc->getGateList( parentModule()->id());
    NicEntry::GateList::const_iterator i = gateList.begin();
        
    if(useSendDirect){
        // use Andras stuff
        if( i != gateList.end() ){
            for(; i != --gateList.end(); ++i){

                int radioStart = i->second->id();
                int radioEnd = radioStart + i->second->size();
                for (int g = radioStart; g != radioEnd; ++g)
                    sendDirect(static_cast<cMessage*>(msg->dup()),
                               delay, i->second->ownerModule(), g);
            }
            int radioStart = i->second->id();
            int radioEnd = radioStart + i->second->size();
            for (int g = radioStart; g != --radioEnd; ++g)
                sendDirect(static_cast<cMessage*>(msg->dup()),
                           delay, i->second->ownerModule(), g);
            
            sendDirect(msg, delay, i->second->ownerModule(), radioEnd);
        }
        else{
            coreEV << "Nic is not connected to any gates!" << endl;
            delete msg;
        }
    }
    else{
        // use our stuff
        coreEV <<"sendToChannel: sending to gates\n";
        if( i != gateList.end() ){
            for(; i != --gateList.end(); ++i){
                sendDelayed( static_cast<cMessage*>(msg->dup()),
                             delay, i->second );
            }
            sendDelayed( msg, delay, i->second );
        }
        else{
            coreEV << "Nic is not connected to any gates!" << endl;
            delete msg;
        }
    }
}

/**
 * ChannelAccess is subscribed to position changes and informs the
 * ConnectionManager
 */
void ChannelAccess::receiveBBItem(int category, const BBItem *details, int scopeModuleId) 
{    
    BaseModule::receiveBBItem(category, details, scopeModuleId);
    
    if(category == catMove)
    {
        Move m(*static_cast<const Move*>(details));
        
        if(isRegistered) {
            cc->updateNicPos(parentModule()->id(), &m.startPos);
        }
        else {
            // register the nic with ConnectionManager
            // returns true, if sendDirect is used
            useSendDirect = cc->registerNic(parentModule(), &m.startPos);
            isRegistered = true;
        }
        move = m;
        coreEV<<"new HostMove: "<<move.info()<<endl;
    }
}


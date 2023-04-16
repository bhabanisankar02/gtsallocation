/*
 * SlaveApp.cc
 *
 *  Created on: Aug 6, 2021
 *      Author: rahul
 *  Modified from inet/applications/udpapp/UdpBasicBurst.cc
 */

#include "../Apps/SlaveApp.h"

#include "omnetpp.h"
#include "inet/common/ModuleAccess.h"
# include <cmath>
# include <random>
# include <cstdlib>
# include <iostream>
# include <iomanip>
# include <fstream>
# include <iomanip>
# include <ctime>
# include <cstring>
# include <sstream>
#include "../Apps/MasterApp.h"

Define_Module(inet::SlaveApp);

int inet::SlaveApp::sseed = 123456789;

void inet::SlaveApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        counter = 0;
        numSent = 0;
        numReceived = 0;
        numDeleted = 0;
        numDuplicated = 0;

        delayLimit = par("delayLimit");
        startTime = par("startTime");
        stopTime = par("stopTime");
        if (stopTime >= SIMTIME_ZERO && stopTime <= startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        messageLengthPar = &par("messageLength");
        burstDurationPar = &par("burstDuration");
        sleepDurationPar = &par("sleepDuration");
        sendIntervalPar = &par("sendInterval");
        nextSleep = startTime;
        nextBurst = startTime;
        nextPkt = startTime;
        dontFragment = par("dontFragment");

        destAddrRNG = par("destAddrRNG");
        const char *addrModeStr = par("chooseDestAddrMode");
        int addrMode = cEnum::get("inet::ChooseDestAddrMode")->lookup(addrModeStr);
        if (addrMode == -1)
            throw cRuntimeError("Invalid chooseDestAddrMode: '%s'", addrModeStr);
        chooseDestAddrMode = static_cast<ChooseDestAddrMode>(addrMode);

        WATCH(numSent);
        WATCH(numReceived);
        WATCH(numDeleted);
        WATCH(numDuplicated);


        localPort = par("localPort");
        destPort = par("destPort");

        timerNext = new cMessage("UDPBasicBurstTimer");
        // Added:
        SPval = 100.0;
    }
}

void inet::SlaveApp::transferSP(double SendProb){
    Enter_Method_Silent();
    SPval = SendProb;
}

int inet::SlaveApp::bernoulli_choice(double p,int &seed){

    const int large = 2147483647;
    int k;
    k = seed/127773;
    seed = 16807*(seed-k*127773)-k*2836;
    if(seed<=0){
        seed += large;
    }
    std::mt19937 gen(seed); //Standard mersenne_twister_engine seeded with seed
    std::bernoulli_distribution d(p);
    int val = d(gen);

    return val;
}

void inet::SlaveApp::generateBurst()
{
    simtime_t now = simTime();
    int temp = 0;

    if (nextPkt < now)
        nextPkt = now;

    double sendInterval = *sendIntervalPar;
    if (sendInterval <= 0.0)
        throw cRuntimeError("The sendInterval parameter must be bigger than 0");
    nextPkt += sendInterval;

    if (activeBurst && nextBurst <= now) {    // new burst
        double burstDuration = *burstDurationPar;
        if (burstDuration < 0.0)
            throw cRuntimeError("The burstDuration parameter mustn't be smaller than 0");
        double sleepDuration = *sleepDurationPar;

        if (burstDuration == 0.0)
            activeBurst = false;
        else {
            if (sleepDuration < 0.0)
                throw cRuntimeError("The sleepDuration parameter mustn't be smaller than 0");
            nextSleep = now + burstDuration;
            nextBurst = nextSleep + sleepDuration;
        }

        if (chooseDestAddrMode == PER_BURST)
            destAddr = chooseDestAddr();
    }

    if (chooseDestAddrMode == PER_SEND)
        destAddr = chooseDestAddr();

    Packet *payload = createPacket();
    payload->setTimestamp();
    //**************************ADDED****************************
    // Watch for signal Sp (Sending Probability) from master node
    // and control transmission of message below.
    temp = bernoulli_choice(SPval/100,sseed);
    if (temp==1){
        emit(packetSentSignal, payload);
        socket.sendTo(payload, destAddr, destPort);
        numSent++;
    }
    //***********************END OF ADDED*************************
    // Next timer
    if (activeBurst && nextPkt >= nextSleep)
        nextPkt = nextBurst;

    if (stopTime >= SIMTIME_ZERO && nextPkt >= stopTime) {
        timerNext->setKind(STOP);
        nextPkt = stopTime;
    }
    scheduleAt(nextPkt, timerNext);
}


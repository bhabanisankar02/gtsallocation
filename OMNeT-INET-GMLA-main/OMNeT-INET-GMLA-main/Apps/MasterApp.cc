/*
 * MasterApp.cc
 *
 *  Created on: Aug 6, 2021
 *      Author: rahul
 *  Modified from inet/applications/udpapp/UdpBasicBurst.cc
 */
#include "../Apps/MasterApp.h"
#include "../Apps/SlaveApp.h"
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

# define POPSIZE 64
# define PMUTATION 0.01
# define QOFTHRESHOLD 7.0
# define NUMBEST 10

Define_Module(inet::MasterApp);

inet::simsignal_t inet::MasterApp::spSignal = registerSignal("SendingProbability");
inet::simsignal_t inet::MasterApp::efSignal = registerSignal("Efficiency");
inet::simsignal_t inet::MasterApp::qofSignal = registerSignal("QualityOfFusion");

int inet::MasterApp::seed = 123456789; // static initialization outside class

void inet::MasterApp::initialize(int stage)
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

        // Added to original initialize from this line:
        last_efficiency = 0.3;
        current_received = 0;
        initialize_pop(); // Initialize GA classifiers population
        numConsults = 0;
        SendProb = 100;
        numHostsPar = &par("NumHosts");
        numHosts = *numHostsPar;
        numBest = 0;
        numE = 0;
        numExp = 0;
        back_to_L = 0;
        n = 0;

        // Initialize the listener for transmitting the control signal:
        for (int i=0;i<numHosts;i++){
            char buf[30];
            /* !!! NOTE !!!
             * Use IEEE_802154_Net below for static GMLA simulation and
             * IEEE_802154_Mobile_Net in next line for mobile GMLA simulation
             */
            sprintf(buf,"IEEE_802154_Net.slave[%d]",i); //Static case
            //sprintf(buf,"IEEE_802154_Mobile_Net.slave[%d]",i); // Mobile case
            cModule *downmod = getModuleByPath(buf);
            cModule *pmod = downmod->getSubmodule("app",0);
            listener[i] = check_and_cast<SlaveApp*>(pmod);
        }
    }
}

void inet::MasterApp::processSend()
{
    if (stopTime < SIMTIME_ZERO || simTime() < stopTime) {
        // send and reschedule next sending
        controlSP();
    }
}

//Genetic ML Algorithm:

int inet::MasterApp::selectSP = 0; // Static initialization

void inet::MasterApp::controlSP(){

    int i;
    int rcv_count;
    int consultedVal;
    double current_SP;
    double delta_ef;
    struct classifier *chosen[4];

    // Calculate Efficiency:
    rcv_count = numReceived-current_received;
    current_received = numReceived;
    current_SP = SendProb/100;
    if (current_SP == 0){
        efficiency = 0;
    }
    else
        efficiency = rcv_count/(current_SP*numHosts*4);
    emit(efSignal,efficiency);

    // Calculate Quality of Fusion:
    QoF = rcv_count/4;
    emit(qofSignal,QoF);

    // Selecting Expert or Learning Phase:
    char Phase;
    if (back_to_L == 1){
        Phase = 'L';
    }
    if (numE>15 || numBest==NUMBEST){
            Phase = 'E';
        }
    // Different outcomes at beginning of simulation
    char Segment;
    if (numConsults == 0)
        Segment = '0';
    else if (numConsults == 1)
        Segment = '1';
    else
        Segment = 'A';

    //*************** START GA PORTION OF CONTROL ******************
    switch(Segment){
        case '0':{ // Do at simulation time 0s (startTime)
            SendProb = 50;
            break;
        }
        case '1':{ //Do at simulation time = 1s
            best_efficiency = efficiency;
            if (QoF>=QOFTHRESHOLD){
                numBest++;
                BestSPList[numBest-1].SPValue = SendProb;
                BestSPList[numBest-1].score = 1;

            }
            int class_choice = random_choice(0,63,seed);
            curr_classifier = &population[class_choice];
            //Pay tax to bank:
            bank += 0.1*curr_classifier->budget;
            curr_classifier->budget = 0.9*curr_classifier->budget;
            //get SP value based on the chosen classifier:
            SendProb = SendProb*getSPVal(curr_classifier->action);
            if (SendProb>100)
                SendProb = 100;
            last_classifier = curr_classifier;
            last_efficiency = efficiency;
            break;
        }
        default:{ // Do for all calls except at simulation time = 0 or 1

            switch(Phase){
                case 'L':{ // Do when in Learning phase
                    delta_ef = (last_efficiency/efficiency - 1)*100;
                    if (delta_ef>0&&QoF>=QOFTHRESHOLD){
                        //Reward last classifier if it had better performance
                        last_classifier->budget += bank;
                        bank = 0;
                        if(efficiency>best_efficiency){
                            numBest++;
                            BestSPList[numBest-1].SPValue = SendProb;
                            BestSPList[numBest-1].score = 1;
                            best_efficiency = efficiency;
                            //if (numBest == NUMBEST){
                             //   back_to_L = 0;
                            //}
                        }
                    }
                    consultedVal = consult(delta_ef);
                    for (i=0;i<4;i++)
                        chosen[i] = &population[consultedVal+i];
                    //With condition, choose 1 of 4 classifiers based on budget or random
                    int n = 3;
                    struct classifier *best[3];
                    best[0] = chosen[0];
                    best[1] = chosen[0];
                    best[2] = chosen[0];
                    int numMax = 0;
                    while(--n>0 && chosen[n]->budget==chosen[0]->budget);// check if all budgets are equal
                    if (n!=0){ //if not equal then get max and check if more than 1 has max value (out of the 4)
                        for (i=1;i<4;i++){
                            if (chosen[i]->budget > best[0]->budget){
                                best[0] = chosen[i];
                                numMax++;
                            }
                        }
                        for (i=1;i<4;i++){
                            if (chosen[i]->budget == best[0]->budget && best[0] != chosen[i]){
                                best[1] = chosen[i];
                                numMax++;
                            }
                            if (chosen[i]->budget == best[0]->budget && best[0] != chosen[i] && best[1] != chosen[i]){
                                best[2] = chosen[i];
                                numMax++;
                            }
                        }
                    }

                    int temp;
                    if (numMax == 0){ // all 4 classifiers have same budget; random selection
                        temp = random_choice(0,3,seed);
                        curr_classifier = chosen[temp];
                    }
                    else if (numMax == 1){ // 1 classifier has highest budget
                        curr_classifier = best[0];
                    }
                    else if (numMax == 2){ // 2 classifiers have max budget; random selection
                        temp = random_choice(0,1,seed);
                        curr_classifier = best[temp];
                    }
                    else{ // 3 classifiers have same max budget; random selection
                        temp = random_choice(0,2,seed);
                        curr_classifier = best[temp];
                    }
                    //Pay tax to bank:
                    bank += 0.1*curr_classifier->budget;
                    curr_classifier->budget = 0.9*curr_classifier->budget;
                    //get SP value based on the chosen classifier:
                    SendProb = SendProb*getSPVal(curr_classifier->action);
                    if (SendProb>100)
                        SendProb = 100;
                    last_classifier = curr_classifier;
                    last_efficiency = efficiency;
                    numE++;// Keep track of number of learning cycles

        //********* Do Evolution (mating and replacement of 2 old classifiers with 2 new):**********
                    if ((numConsults)%4==0){ // Do every 4 classifier system consults
                        int adapted1;
                        int adapted2;
                        int worst1;
                        int worst2;
                        //Arrange the 4 consulted classifiers in descending order of budget:
                        struct classifier *best_bud, *temp_bud;
                        int j;
                        best_bud = chosen[0];
                        for (i=0;i<4;i++){
                            if (chosen[i]->budget > best_bud->budget){
                                best_bud = chosen[i];
                                j = i;
                            }
                            temp_bud = chosen[0];
                            chosen[0] = best_bud;
                            chosen[j] = temp_bud;
                        }
                        best_bud = chosen[1];
                        for (i=1;i<4;i++){
                            if (chosen[i]->budget > best_bud->budget){
                                best_bud = chosen[i];
                                j = i;
                            }
                            temp_bud = chosen[1];
                            chosen[1] = best_bud;
                        }
                        if (chosen[3]>chosen[2]){
                            temp_bud = chosen[2];
                            chosen[2] = chosen[3];
                            chosen[3] = temp_bud;
                        }

                        //Select Parents and evolve:
                        adapted1 = chosen[0]->action;
                        adapted2 = chosen[1]->action;
                        worst1 = chosen[2]->action;
                        worst2 = chosen[3]->action;
                        from_evolve = Evolve(adapted1,adapted2);
                        //Replace worst 2 with children:
                        chosen[2]->action = from_evolve.c1;
                        chosen[3]->action = from_evolve.c2;
                        chosen[2]->budget = 100;
                        chosen[3]->budget = 100;
                    } // End of evolution
                    break;

                }
                case 'E':{ // Do when in Expert phase
                    Packet *payload = createPacket();
                    payload->setTimestamp();
                    emit(packetSentSignal, payload);
                    socket.sendTo(payload, destAddr, destPort);
                    numSent++;
                    int flag = 0;
                    if (numExp != 0){
                        // If QoF threshold is not met, reduce score by 1
                        if (QoF<QOFTHRESHOLD){
                            BestSPList[selectSP].score += -1;
                        }
                        else{
                            BestSPList[selectSP].score += 1;
                        }
                        // If no entry with score above 0 exists, go back to Learning
                        int temp = numBest;
                        for (i=0;i<numBest;i++){
                            if (BestSPList[i].score==0){
                                temp--;
                            }
                            else{
                                flag = 1;
                            }
                        }
                        if (flag == 0){
                            numE=0;
                            numBest = 0;
                        }

                    }
                    // Select the first above-zero score entry in best SP list
                    for (i=0;i<numBest;i++){
                        if (BestSPList[i].score>0){
                            SendProb = BestSPList[i].SPValue;
                            selectSP = i;
                            flag = 1;
                            numExp++;
                            break;
                        }
                    }
                    last_efficiency = efficiency; // In case another learning cycle happens after expert phase
                } // End of Expert Phase Case of Phase Switch
            }  // End of Phase Switch
        } // End of default Case of Segment Switch
    } // End of Segment Switch

    //***************** END GA PORTION OF CONTROL ******************

    // Send updates and symbolic message
    Packet *payload = createPacket();
    payload->setTimestamp();
    emit(packetSentSignal, payload);
    socket.sendTo(payload, destAddr, destPort);
    numSent++;

    SendProb+=20; // To avoid getting stuck at value close to 0.
    emit(spSignal,SendProb);
    for (i=0;i<numHosts;i++){
        listener[i]->transferSP(SendProb); // Sets value of SPVal at each slave nodes' app[0](SlaveApp class)
    }
    numConsults++;

    // Reschedule next update:
    double sendInterval = *sendIntervalPar;
    simtime_t nxt = simTime() + sendInterval;
    if (stopTime < SIMTIME_ZERO || nxt < stopTime) {
        timerNext->setKind(SEND);
        scheduleAt(nxt, timerNext);
    }
    else {
        timerNext->setKind(STOP);
        scheduleAt(stopTime, timerNext);
    }
} // End of Control SP

inet::MasterApp::children inet::MasterApp::Evolve(int action1, int action2){
    //Does fixed point crossover for the given 4 bit integers
    //and applies mutation to a randomly chosen bit with a
    //probability of PMUTATION.
    //Returns a Struct children{int,int}.
    int child1;
    int child2;
    int m1,n1;
    int m2,n2;
    int mut_check;
    int bit_to_flip;
    children childvals;
    m1 = action1 & 3;
    n1 = action1 & 12;
    m2 = action2 & 3;
    n2 = action2 & 12;
    childvals.c1 = m1 | n2;
    childvals.c2 = m2 | n1;
    mut_check = bernoulli_choice(PMUTATION,seed);
    if (mut_check == 1){
        bit_to_flip = random_choice(0,3,seed);
        childvals.c1 ^= 1<<bit_to_flip;
    }
    mut_check = bernoulli_choice(PMUTATION,seed);
    if (mut_check == 1){
        bit_to_flip = random_choice(0,3,seed);
        childvals.c2 ^= 1<<bit_to_flip;
    }
    return childvals;
}

int inet::MasterApp::bernoulli_choice(double p,int &seed){

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

double inet::MasterApp::getSPVal(int action){
    // Lookup table from paper
    double val;
    if (action==0){
        val = 1-0.12;
    }
    else if (action==1){
        val = 1-0.24;
    }
    else if (action==2){
        val = 1-0.36;
        }
    else if (action==3){
        val = 1-0.48;
        }
    else if (action==4){
        val = 1-0.64;
        }
    else if (action==5){
        val = 1-0.72;
        }
    else if (action==6){
        val = 1-0.84;
        }
    else if (action==7){
        val = 1-1.0;
        }
    else if (action==8){
        val = 1+0.12;
        }
    else if (action==9){
        val = 1+0.24;
        }
    else if (action==10){
        val = 1+0.36;
        }
    else if (action==11){
        val = 1+0.48;
        }
    else if (action==12){
        val = 1+0.64;
        }
    else if (action==13){
        val = 1+0.72;
        }
    else if (action==14){
        val = 1+0.84;
        }
    else{
        val = 1+1.0;
    }
    return val;
}


int inet::MasterApp::random_choice(int a, int b,int &seed){

    const int large = 2147483647;
    int k;
    k = seed/127773;
    seed = 16807*(seed-k*127773)-k*2836;
    if(seed<=0){
        seed += large;
    }
    std::mt19937 gen(seed); //Standard mersenne_twister_engine seeded with seed
    std::uniform_int_distribution<> distrib(a,b);
    int val = distrib(gen);

    return val;
}

int inet::MasterApp::consult(double delta_ef){
    // Lookup table from paper
    int value = 0;

    if (delta_ef>0){
        if (delta_ef<12){
            value = 0;
        }
        else if (delta_ef<24){
            value = 4;
        }
        else if (delta_ef<36){
            value = 8;
                }
        else if (delta_ef<48){
            value = 12;
                }
        else if (delta_ef<64){
            value = 16;
                }
        else if (delta_ef<72){
            value = 20;
                }
        else if (delta_ef<84){
            value = 24;
                }
        else{
            value = 28;
                }
    }
    else{
        if (delta_ef>-12){
            value = 32;
                }
        else if (delta_ef>-24){
            value = 36;
        }
        else if (delta_ef>-36){
            value = 40;
                }
        else if (delta_ef>-48){
            value = 44;
                }
        else if (delta_ef>-64){
            value = 48;
                }
        else if (delta_ef>-72){
            value = 52;
                }
        else if (delta_ef>-84){
            value = 56;
                }
        else{
            value = 60;
                }
    }
    return value;
}

inet::MasterApp::classifier inet::MasterApp::population[POPSIZE];


void inet::MasterApp::initialize_pop(){
    //  Process: get 16 conditions (numbers 0 to 15);
    //  for each of the 16 numbers get 4 actions randomly from numbers 0 to 15
    //  combine to get 64 classifiers.
    int i;
    int j;
    int count = 0;
    int uniform_int[POPSIZE];

    std::random_device rd;  //Used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, 15);

    for (int n=0; n<64; ++n)
        uniform_int[n] = distrib(gen);

    for (i=0;i<16;i++){
        for (j=0;j<4;j++){
            population[count].budget = 100;
            population[count].condition = i;
            population[count].action = uniform_int[count];
            count++;
        }
    }

}

//Genetic Algorithm (just for reference)
//Initialize population
//Then at every multiple of 1s simtime,
//    calculate efficiency since cycle start
//    if efficiency is greater than best:
//        set best eF = efficiency
//        set best SP = current SP
//        store current SP value in list of best SPs
//    calculate delta Ef (ratio with previous value of Ef)
//    store current value as previous value of Ef
//    reward current classifier with all accumulated budget in bank if delta Ef>0
//    select new classifier based on condition (highest budget or random if budget is same)
//    get new SP value
//    new classifier pays 10% of budget to bank
//    Output new SP at start of every MA
//At every multiple of 4s simtime,
//    for the 4 classifiers belonging to current condition:
//        select best 2 parents
//        do crossover() for parents
//        do mutation() for parents
//If expert phase:
//    new sp = from set of best SPs
//else:
//    new sp = current sp





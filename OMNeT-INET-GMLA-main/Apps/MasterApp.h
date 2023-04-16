/*
 * MasterApp.h
 *
 *  Created on: Aug 6, 2021
 *      Author: rahul
 *  Modified from inet/applications/udpapp/UdpBasicBurst.h
 */

#ifndef __INET_MASTERAPP_H_
#define __INET_MASTERAPP_H_

# define POPSIZE 64
# define NUMBEST 10

# include <map>
# include <vector>
# include <cmath>
# include <random>
# include <cstdlib>
# include <iostream>
# include <iomanip>
# include <fstream>
# include <iomanip>
# include <ctime>
# include <cstring>

# include "inet/common/INETDefs.h"

# include "inet/applications/udpapp/UdpBasicBurst.h"

namespace inet {

/**
 * UDP application with GMLA control.
 */
//Forward declaration
class SlaveApp;

class MasterApp : public UdpBasicBurst
{
  public:
    struct classifier{ //Defining structure of a classifier
        int condition;
        int action;
        double budget;
    };
    static classifier population[POPSIZE]; // pool of candidate classifiers
    classifier newpopulation[POPSIZE]; // to store after evolution
    classifier *last_classifier;
    classifier *curr_classifier;
    struct BestSPValue{
        double SPValue;
        int score;
    };
    BestSPValue BestSPList[NUMBEST]; //list of learned best SP values
    static int selectSP;
    double bank = 0;
    double efficiency;
    double last_efficiency;
    double best_efficiency;
    double QoF;
    int current_received;
    int n;
    double SendProb;
    int numHosts;
    static int seed; // Random seed
    int numConsults;
    int numBest;
    int numE;
    int numExp;
    int back_to_L;
    cPar *numHostsPar = nullptr;
    SlaveApp *listener[30]; //!!! Change number here to match number of slave nodes !!!
    struct children{
        int c1,c2;
    };
    children from_evolve;

  protected:
    //Overridden functions:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void processSend() override;
    // Added functions (GMLA):
    virtual void controlSP(); // Calculates SP using GA and sends periodically to slave nodes' apps
    virtual void initialize_pop(); // Initialize classifier population
    virtual int consult(double delta_ef); // Get current condition to determine classifier
    virtual int random_choice(int a, int b, int &seed); // Random integer generator (within a range)
    virtual double getSPVal(int action); // Get value of next round Sending Probability based on selected classifier
    virtual children Evolve(int action1, int action2); // Mate selected parent classifiers (crossover, mutation)
    virtual int bernoulli_choice(double p, int &seed); // Random Bernoulli number generator

    static simsignal_t spSignal;
    static simsignal_t efSignal;
    static simsignal_t qofSignal;
};

} // namespace inet




#endif /* __INET_MASTERAPP_H_ */

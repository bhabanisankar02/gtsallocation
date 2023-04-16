/*
 * SlaveApp.h
 *
 *  Created on: Aug 6, 2021
 *      Author: rahul
 *  Modified from inet/applications/udpapp/UdpBasicBurst.h
 *
 */

#ifndef __INET_SLAVEAPP_H_
#define __INET_SLAVEAPP_H_

#include <map>
#include <vector>

#include "inet/common/INETDefs.h"

#include "inet/applications/udpapp/UdpBasicBurst.h"

namespace inet {

/**
 * UDP application modified to receive control signal from MasterApp.
 */
class SlaveApp : public UdpBasicBurst
{
  public:
    cModule *test;
    void transferSP(double SendProb); //Receives Sending Probability control signal here
    double SPval; // Sending Probability
    static int sseed; // Random seed

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void generateBurst() override;
    virtual int bernoulli_choice(double p, int &seed); // Bernoulli random number generation
};
} // namespace inet


#endif /* SLAVEAPP_H_ */

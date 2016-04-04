/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define __STDC_LIMIT_MACROS
#include "SMPS.h"
#include "Hardware.h"
#include "ProgramData.h"
#include "Screen.h"
#include "Utils.h"
#include "Settings.h"
#include "TheveninMethod.h"
#include "Balancer.h"

//#define ENABLE_DEBUG
#include "debug.h"

namespace TheveninMethod {

    enum State {ConstantCurrentBalancing, ConstantCurrent,
                RthMesurment, LastRthMesurment, LastConstantCurrent, ConstantVoltageBalancing};
    /* possible transitions:
     *   RthMesurment <--> ConstantCurrentBalancing --> ConstantCurrent -->
     *   LastRthMesurment --> LastConstantCurrent --> ConstantVoltageBalancing
     */

    enum State state_;
    AnalogInputs::ValueType newI_;

    uint8_t fullCount_;

    uint16_t lastBallancingEnded_;
    enum Strategy::statusType bstatus_;

    AnalogInputs::ValueType normalizeI(AnalogInputs::ValueType newI, AnalogInputs::ValueType I);

    uint16_t getMinIwithBalancer() {
        if(bstatus_ != Strategy::COMPLETE)
            return 0;
        else return Strategy::minI;
    }
}

AnalogInputs::ValueType TheveninMethod::getReadableWiresRth()
{
    int16_t iV;
    uint16_t uI;
    iV =  AnalogInputs::getRealValue(AnalogInputs::Vout);
    iV -= AnalogInputs::getRealValue(AnalogInputs::VoutBalancer);
    uI = AnalogInputs::getRealValue(AnalogInputs::Iout);
    return Thevenin::getReadableRth(iV, uI);
}

void TheveninMethod::initialize(bool charge)
{
    bstatus_ = Strategy::COMPLETE;

    Thevenin::initialize(charge);

    state_ = ConstantCurrentBalancing;
    fullCount_ = 0;
    newI_ = 0;
}

//TODO: the TheveninMethod  is too complex, should be refactored, maybe when switching to mAmps


bool TheveninMethod::balance_isComplete(bool isEndVout, AnalogInputs::ValueType I)
{
    if(Strategy::doBalance) {
        if(isEndVout && state_ == ConstantCurrentBalancing) {
            Balancer::endBalancing();
            state_ = ConstantCurrent;
        }
        if(state_ == ConstantCurrentBalancing || state_ == ConstantVoltageBalancing) {
            if(I > max(BALANCER_I, Strategy::minI))
                Balancer::done = false;
            bstatus_ = Balancer::doStrategy();
            if(bstatus_ != Strategy::COMPLETE) {
                //TheveninMethod is not complete until we finish balancing
                return false;
            } else {
                //if last balancing is complete test if we need a new minCell
                if(Balancer::isCalibrationRequired()) {
                    Balancer::resetMinCell();
                }
            }
        }
    }

    if(I <= getMinIwithBalancer() && isEndVout && state_ == ConstantVoltageBalancing) {
        if(fullCount_++ >= 10) {
            return true;
        }
    } else {
        fullCount_ = 0;
    }
    return false;
}


AnalogInputs::ValueType TheveninMethod::calculateNewI(bool isEndVout, AnalogInputs::ValueType I)
{
    //update when output is stable or end voltage reached
    bool updateI = AnalogInputs::isOutStable() || (isEndVout && newI_ != 0);

    //update only when we are not balancing:
    //- on PowerB6 Balancer::getPresumedV is not stable enough
    updateI = updateI && !Balancer::isWorking();

    if(updateI) {

        LogDebug(" I=", I, " tVout_: Rth=", tVout_.Rth.iV, ',', tVout_.Rth.uI, " Vth=", tVout_.Vth_);

        Thevenin::calculateRthVth(I);
        Thevenin::storeI(I);

        newI_ = Thevenin::calculateI();

        LogDebug("newI=", newI_);

        if(newI_ < I) {
            //low pass filter
            //static assert: low pass filter overflow
            STATIC_ASSERT(MAX_CHARGE_I < INT16_MAX);
            STATIC_ASSERT(MAX_DISCHARGE_I < INT16_MAX);
            newI_ = (newI_ + I)/2;
        }

        newI_ = normalizeI(newI_, I);

        switch(state_) {
        case ConstantCurrentBalancing:
            if(!isEndVout)
                break;
            Balancer::endBalancing();
            state_ = ConstantCurrent;
            break;
        case ConstantCurrent:
            if(!isEndVout)
                break;
            state_ = LastRthMesurment;
            //temporarily turn off
            newI_ = 0;
            break;
        case RthMesurment:
            state_ = ConstantCurrentBalancing;
            break;
        case LastRthMesurment:
            newI_ = 0;
            state_ = LastConstantCurrent;
            break;
        case LastConstantCurrent:
            if(isEndVout)
                state_ = ConstantVoltageBalancing;
            break;
        default:
            state_ = ConstantVoltageBalancing;
            break;
        }
    }

    LogDebug("normalized newI=", newI_);
    return newI_;
}

AnalogInputs::ValueType TheveninMethod::normalizeI(AnalogInputs::ValueType newI, AnalogInputs::ValueType I)
{
    if(newI > Strategy::maxI) {
        newI = Strategy::maxI;
    }
    if(newI < getMinIwithBalancer()) {
        newI = getMinIwithBalancer();
    }

    if(I != newI) {
        //update current when:
        // - we are NOT in the ConstantVoltageBalancing state, or
        // - if we are in ConstantVoltageBalancing AND new current is smaller then the previous one, or
        // - if we are in ConstantVoltageBalancing AND new current is not larger then the minimum, or
        // - if we are in ConstantVoltageBalancing AND we did balancing (current went below minimum)
        if(state_ != ConstantVoltageBalancing
            || newI < I
            || newI <= getMinIwithBalancer()
            || (I <= Strategy::minI && lastBallancingEnded_ != Balancer::balancingEnded)) {

            lastBallancingEnded_ = Balancer::balancingEnded;
            return newI;
        }
    }
    return I;
}


// FGAIMultiplayer - FGAIBase-derived class creates an AI multiplayer aircraft
//
// Based on FGAIAircraft
// Written by David Culp, started October 2003.
// Also by Gregor Richards, started December 2005.
//
// Copyright (C) 2003  David P. Culp - davidculp2@comcast.net
// Copyright (C) 2005  Gregor Richards
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string>
#include <stdio.h>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Time/TimeManager.hxx>

#include "AIMultiplayer.hxx"

using std::string;

// #define SG_DEBUG SG_ALERT

FGAIMultiplayer::FGAIMultiplayer() :
   FGAIBase(otMultiplayer, fgGetBool("/sim/multiplay/hot", false))
{
   no_roll = false;

   mTimeOffsetSet = false;
   mAllowExtrapolation = true;
   mLagAdjustSystemSpeed = 10;
   mLastTimestamp = 0;
   lastUpdateTime = 0;
   playerLag = 0.03;
   compensateLag = 1;
   realTime = false;
   lastTime=0.0;
   lagPpsAveraged = 1.0;
   rawLag = 0.0;
   rawLagMod = 0.0;
   lagModAveraged = 0.0;
   _searchOrder = PREFER_DATA;
}

FGAIMultiplayer::~FGAIMultiplayer() {
}

bool FGAIMultiplayer::init(ModelSearchOrder searchOrder)
{
    props->setStringValue("sim/model/path", model_path);
    props->setIntValue("sim/model/fallback-model-index", _getFallbackModelIndex());
    //refuel_node = fgGetNode("systems/refuel/contact", true);
    isTanker = false; // do this until this property is
                      // passed over the net

    const string& str1 = _getCallsign();
    const string str2 = "MOBIL";

    string::size_type loc1= str1.find( str2, 0 );
    if ( (loc1 != string::npos && str2 != "") ){
        //	   cout << " string found "	<< str2 << " in " << str1 << endl;
        isTanker = true;
        //	   cout << "isTanker " << isTanker << " " << mCallSign <<endl;
    }

    // load model
    bool result = FGAIBase::init(searchOrder);
    // propagate installation state (used by MP pilot list)
    props->setBoolValue("model-installed", _installed);
    return result;
}

void FGAIMultiplayer::bind() {
    FGAIBase::bind();

    //2018.1 mp-clock-mode indicates the clock mode that the client is running, so for backwards
    //       compatibility ensure this is initialized to 0 which means pre 2018.1
    props->setIntValue("sim/multiplay/mp-clock-mode", 0);

    tie("refuel/contact", SGRawValuePointer<bool>(&contact));
    tie("tanker", SGRawValuePointer<bool>(&isTanker));

    tie("controls/invisible",
        SGRawValuePointer<bool>(&invisible));
	_uBodyNode = props->getNode("velocities/uBody-fps", true);
	_vBodyNode = props->getNode("velocities/vBody-fps", true);
	_wBodyNode = props->getNode("velocities/wBody-fps", true);

#define AIMPROProp(type, name) \
SGRawValueMethods<FGAIMultiplayer, type>(*this, &FGAIMultiplayer::get##name)

#define AIMPRWProp(type, name) \
SGRawValueMethods<FGAIMultiplayer, type>(*this, \
      &FGAIMultiplayer::get##name, &FGAIMultiplayer::set##name)

    //tie("callsign", AIMPROProp(const char *, CallSign));

    tie("controls/allow-extrapolation",
        AIMPRWProp(bool, AllowExtrapolation));
    tie("controls/lag-adjust-system-speed",
        AIMPRWProp(double, LagAdjustSystemSpeed));
    tie("controls/player-lag",
        AIMPRWProp(double, playerLag));
    tie("controls/compensate-lag",
        AIMPRWProp(int, compensateLag));

#undef AIMPROProp
#undef AIMPRWProp
}

void FGAIMultiplayer::update(double dt)
{
  using namespace simgear;

  if (dt <= 0)
    return;

  FGAIBase::update(dt);

  // Check if we already got data
  if (mMotionInfo.empty())
    return;

  // The current simulation time we need to update for,
  // note that the simulation time is updated before calling all the
  // update methods. Thus it contains the time intervals *end* time
  // 2018: notice this time is specifically used for mp protocol
  double curtime = globals->get_subsystem<TimeManager>()->getMPProtocolClockSec();

  // Get the last available time
  MotionInfo::reverse_iterator it = mMotionInfo.rbegin();
  double curentPkgTime = it->second.time;

  // Dynamically optimize the time offset between the feeder and the client
  // Well, 'dynamically' means that the dynamic of that update must be very
  // slow. You would otherwise notice huge jumps in the multiplayer models.
  // The reason is that we want to avoid huge extrapolation times since
  // extrapolation is highly error prone. For that we need something
  // approaching the average latency of the packets. This first order lag
  // component will provide this. We just take the error of the currently
  // requested time to the most recent available packet. This is the
  // target we want to reach in average.
  double lag = it->second.lag;

  rawLag = curentPkgTime - curtime;
  realTime = false; //default behaviour

  if (!mTimeOffsetSet) {
    mTimeOffsetSet = true;
    mTimeOffset = curentPkgTime - curtime - lag;
    lastTime = curentPkgTime;
    lagModAveraged = remainder((curentPkgTime - curtime), 3600.0);
    props->setDoubleValue("lag/pps-averaged", lagPpsAveraged);
    props->setDoubleValue("lag/lag-mod-averaged", lagModAveraged);
  } else {
      if ((curentPkgTime - lastTime) != 0) {
        lagPpsAveraged = 0.99 * lagPpsAveraged + 0.01 * fabs( 1 / (lastTime - curentPkgTime));
        lastTime = curentPkgTime;
        rawLagMod = remainder(rawLag, 3600.0);
        lagModAveraged = lagModAveraged *0.99 + 0.01 * rawLagMod;
        props->setDoubleValue("lag/pps-averaged", lagPpsAveraged);
        props->setDoubleValue("lag/lag-mod-averaged", lagModAveraged);
      }

      double offset = 0.0;

        //spectator mode, more late to be in the interpolation zone
    if (compensateLag == 3) { offset = curentPkgTime -curtime -lag + playerLag;

       // old behaviour
    } else if (compensateLag == 1) { offset = curentPkgTime - curtime - lag;

    // using the prediction mode to display the mpaircraft in the futur/past with given playerlag value
    //currently compensatelag = 2
    } else if (fabs(lagModAveraged) < 0.3) {
        mTimeOffset = (round(rawLag/3600))*3600; //real time mode if close enough
        realTime = true;

    } else { offset = curentPkgTime - curtime + 0.48*lag + playerLag;
    }

    if (!realTime) {

      if ((!mAllowExtrapolation && offset + lag < mTimeOffset)
            || (offset - 10 > mTimeOffset)) {
        mTimeOffset = offset;
        SG_LOG(SG_AI, SG_DEBUG, "Resetting time offset adjust system to "
               "avoid extrapolation: time offset = " << mTimeOffset);
      } else {
          // the error of the offset, respectively the negative error to avoid
          // a minus later ...
        double err = offset - mTimeOffset;
          // limit errors leading to shorter lag values somehow, that is late
          // arriving packets will pessimize the overall lag much more than
          // early packets will shorten the overall lag
        double sysSpeed;
          //trying to slow the rudderlag phenomenon thus using more the prediction system
          //if we are off by less than 1.5s, do a little correction, and bigger step above 1.5s
        if (fabs(err) < 1.5) {
          if (err < 0) {
            sysSpeed = mLagAdjustSystemSpeed*err*0.01;
          } else {
            sysSpeed = SGMiscd::min(0.5*err*err, 0.05);
          }
        } else {
          if (err < 0) {

              // Ok, we have some very late packets and nothing newer increase the
              // lag by the given speedadjust
            sysSpeed = mLagAdjustSystemSpeed*err;
          } else {
                // We have a too pessimistic display delay shorten that a small bit
            sysSpeed = SGMiscd::min(0.1*err*err, 0.5);
          }
        }


      // simple euler integration for that first order system including some
      // overshooting guard to prevent to aggressive system speeds
      // (stiff systems) to explode the systems state
        double systemIncrement = dt*sysSpeed;
        if (fabs(err) < fabs(systemIncrement))
          systemIncrement = err;
        mTimeOffset += systemIncrement;

        SG_LOG(SG_AI, SG_DEBUG, "Offset adjust system: time offset = "
             << mTimeOffset << ", expected longitudinal position error due to "
             " current adjustment of the offset: "
             << fabs(norm(it->second.linearVel)*systemIncrement));
      }
    }
  }

  // Compute the time in the feeders time scale which fits the current time
  // we need to
  double tInterp = curtime + mTimeOffset;

  SGVec3d ecPos;
  SGQuatf ecOrient;
  SGVec3f ecLinearVel;

  if (tInterp < curentPkgTime) {
    // Ok, we need a time prevous to the last available packet,
    // that is good ...
    // the case tInterp = curentPkgTime need to be in the interpolation, to avoid a bug zeroing the position

    // Find the first packet before the target time
    MotionInfo::iterator nextIt = mMotionInfo.upper_bound(tInterp);
    if (nextIt == mMotionInfo.begin()) {
      SG_LOG(SG_AI, SG_DEBUG, "Taking oldest packet!");
      // We have no packet before the target time, just use the first one
      MotionInfo::iterator firstIt = mMotionInfo.begin();
      ecPos = firstIt->second.position;
      ecOrient = firstIt->second.orientation;
      ecLinearVel = firstIt->second.linearVel;
      speed = norm(ecLinearVel) * SG_METER_TO_NM * 3600.0;

      std::vector<FGPropertyData*>::const_iterator firstPropIt;
      std::vector<FGPropertyData*>::const_iterator firstPropItEnd;
      firstPropIt = firstIt->second.properties.begin();
      firstPropItEnd = firstIt->second.properties.end();
      while (firstPropIt != firstPropItEnd) {
        //cout << " Setting property..." << (*firstPropIt)->id;
        PropertyMap::iterator pIt = mPropertyMap.find((*firstPropIt)->id);
        if (pIt != mPropertyMap.end())
        {
          //cout << "Found " << pIt->second->getPath() << ":";
          switch ((*firstPropIt)->type) {
            case props::INT:
            case props::BOOL:
            case props::LONG:
              pIt->second->setIntValue((*firstPropIt)->int_value);
              //cout << "Int: " << (*firstPropIt)->int_value << "\n";
              break;
            case props::FLOAT:
            case props::DOUBLE:
              pIt->second->setFloatValue((*firstPropIt)->float_value);
              //cout << "Flo: " << (*firstPropIt)->float_value << "\n";
              break;
            case props::STRING:
            case props::UNSPECIFIED:
              pIt->second->setStringValue((*firstPropIt)->string_value);
              //cout << "Str: " << (*firstPropIt)->string_value << "\n";
              break;
            default:
              // FIXME - currently defaults to float values
              pIt->second->setFloatValue((*firstPropIt)->float_value);
              //cout << "Unknown: " << (*firstPropIt)->float_value << "\n";
              break;
          }
        }
        else
        {
          SG_LOG(SG_AI, SG_DEBUG, "Unable to find property: " << (*firstPropIt)->id << "\n");
        }
        ++firstPropIt;
      }

    } else {
      // Ok, we have really found something where our target time is in between
      // do interpolation here
      MotionInfo::iterator prevIt = nextIt;
      --prevIt;

      /*
      * RJH: 2017-02-16 another exception thrown here when running under debug (and hence huge frame delays)
      * the value of nextIt was already end(); which I think means that we cannot run the entire next section of code.
      */
      if (nextIt != prevIt && nextIt != mMotionInfo.end()) {
          // Interpolation coefficient is between 0 and 1
          double intervalStart = prevIt->second.time;
          double intervalEnd = nextIt->second.time;

          double intervalLen = intervalEnd - intervalStart;
          double tau = 0.0;
          if (intervalLen != 0.0) tau = (tInterp - intervalStart) / intervalLen;

          SG_LOG(SG_AI, SG_DEBUG, "Multiplayer vehicle interpolation: ["
              << intervalStart << ", " << intervalEnd << "], intervalLen = "
              << intervalLen << ", interpolation parameter = " << tau);

          // Here we do just linear interpolation on the position
          ecPos = interpolate(tau, prevIt->second.position, nextIt->second.position);
          ecOrient = interpolate((float)tau, prevIt->second.orientation,
              nextIt->second.orientation);
          ecLinearVel = interpolate((float)tau, prevIt->second.linearVel, nextIt->second.linearVel);
          speed = norm(ecLinearVel) * SG_METER_TO_NM * 3600.0;

          if (prevIt->second.properties.size()
              == nextIt->second.properties.size()) {
              std::vector<FGPropertyData*>::const_iterator prevPropIt;
              std::vector<FGPropertyData*>::const_iterator prevPropItEnd;
              std::vector<FGPropertyData*>::const_iterator nextPropIt;
              std::vector<FGPropertyData*>::const_iterator nextPropItEnd;
              prevPropIt = prevIt->second.properties.begin();
              prevPropItEnd = prevIt->second.properties.end();
              nextPropIt = nextIt->second.properties.begin();
              nextPropItEnd = nextIt->second.properties.end();
              while (prevPropIt != prevPropItEnd) {
                  PropertyMap::iterator pIt = mPropertyMap.find((*prevPropIt)->id);
                  //cout << " Setting property..." << (*prevPropIt)->id;

                  if (pIt != mPropertyMap.end())
                  {
                      //cout << "Found " << pIt->second->getPath() << ":";

                      float val;
                      /*
                       * RJH - 2017-01-25
                       * During multiplayer operations a series of crashes were encountered that affected all players
                       * within range of each other and resulting in an exception being thrown at exactly the same moment in time
                       * (within case props::STRING: ref http://i.imgur.com/y6MBoXq.png)
                       * Investigation showed that the nextPropIt and prevPropIt were pointing to different properties
                       * which may be caused due to certain models that have overloaded mp property transmission and
                       * these craft have their properties truncated due to packet size. However the result of this
                       * will be different contents in the previous and current packets, so here we protect against
                       * this by only considering properties where the previous and next id are the same.
                       * It might be a better solution to search the previous and next lists to locate the matching id's
                       */
                      if (*nextPropIt && (*nextPropIt)->id == (*prevPropIt)->id) {
                          switch ((*prevPropIt)->type) {
                          case props::INT:
                          case props::BOOL:
                          case props::LONG:
                              // Jean Pellotier, 2018-01-02 : we don't want interpolation for integer values, they are mostly used
                              // for non linearly changing values (e.g. transponder etc ...)
                              // fixes: https://sourceforge.net/p/flightgear/codetickets/1885/
                              pIt->second->setIntValue((*nextPropIt)->int_value);
                              break;
                          case props::FLOAT:
                          case props::DOUBLE:
                              val = (1 - tau)*(*prevPropIt)->float_value +
                                  tau*(*nextPropIt)->float_value;
                              //cout << "Flo: " << val << "\n";
                              pIt->second->setFloatValue(val);
                              break;
                          case props::STRING:
                          case props::UNSPECIFIED:
                              //cout << "Str: " << (*nextPropIt)->string_value << "\n";
                              pIt->second->setStringValue((*nextPropIt)->string_value);
                              break;
                          default:
                              // FIXME - currently defaults to float values
                              val = (1 - tau)*(*prevPropIt)->float_value +
                                  tau*(*nextPropIt)->float_value;
                              //cout << "Unk: " << val << "\n";
                              pIt->second->setFloatValue(val);
                              break;
                          }
                      }
                      else
                      {
                          SG_LOG(SG_AI, SG_WARN, "MP packet mismatch during lag interpolation: " << (*prevPropIt)->id << " != " << (*nextPropIt)->id << "\n");
                      }
                  }
                  else
                  {
                      SG_LOG(SG_AI, SG_DEBUG, "Unable to find property: " << (*prevPropIt)->id << "\n");
                  }

                  ++prevPropIt;
                  ++nextPropIt;
              }
          }

          // Now throw away too old data
          if (prevIt != mMotionInfo.begin())
          {
              --prevIt;
              mMotionInfo.erase(mMotionInfo.begin(), prevIt);
          }
      }
    }
  } else {
    // Ok, we need to predict the future, so, take the best data we can have
    // and do some eom computation to guess that for now.
    FGExternalMotionData& motionInfo = it->second;

    // The time to predict, limit to 3 seconds
    double t = tInterp - motionInfo.time;
    t = SGMisc<double>::min(t, 3);

    SG_LOG(SG_AI, SG_DEBUG, "Multiplayer vehicle extrapolation: "
           "extrapolation time = " << t);

    // using velocity and acceleration to guess a parabolic position...
    ecPos = motionInfo.position;
    ecOrient = motionInfo.orientation;
    ecLinearVel = motionInfo.linearVel;
    SGVec3d ecVel = toVec3d(ecOrient.backTransform(ecLinearVel));
    SGVec3f angularVel = motionInfo.angularVel;
    SGVec3d ecAcc = toVec3d(ecOrient.backTransform(motionInfo.linearAccel));

	double normVel = norm(ecVel);

	// not doing rotationnal prediction for small speed or rotation rate,
	// to avoid agitated parked plane
    if (( norm(angularVel) > 0.05 ) || ( normVel > 1.0 )) {
		ecOrient += t*ecOrient.derivative(angularVel);
	}

	// not using acceleration for small speed, to have better parked planes
	// note that anyway acceleration is not transmit yet by mp
	if ( normVel > 1.0 ) {
		ecPos += t*(ecVel + 0.5*t*ecAcc);
	} else {
		ecPos += t*(ecVel);
	}

	std::vector<FGPropertyData*>::const_iterator firstPropIt;
    std::vector<FGPropertyData*>::const_iterator firstPropItEnd;
    speed = norm(ecLinearVel) * SG_METER_TO_NM * 3600.0;
    firstPropIt = it->second.properties.begin();
    firstPropItEnd = it->second.properties.end();
    while (firstPropIt != firstPropItEnd) {
      PropertyMap::iterator pIt = mPropertyMap.find((*firstPropIt)->id);
      //cout << " Setting property..." << (*firstPropIt)->id;

      if (pIt != mPropertyMap.end())
      {
        switch ((*firstPropIt)->type) {
          case props::INT:
          case props::BOOL:
          case props::LONG:
            pIt->second->setIntValue((*firstPropIt)->int_value);
            //cout << "Int: " << (*firstPropIt)->int_value << "\n";
            break;
          case props::FLOAT:
          case props::DOUBLE:
            pIt->second->setFloatValue((*firstPropIt)->float_value);
            //cout << "Flo: " << (*firstPropIt)->float_value << "\n";
            break;
          case props::STRING:
          case props::UNSPECIFIED:
            pIt->second->setStringValue((*firstPropIt)->string_value);
            //cout << "Str: " << (*firstPropIt)->string_value << "\n";
            break;
          default:
            // FIXME - currently defaults to float values
            pIt->second->setFloatValue((*firstPropIt)->float_value);
            //cout << "Unk: " << (*firstPropIt)->float_value << "\n";
            break;
        }
      }
      else
      {
        SG_LOG(SG_AI, SG_DEBUG, "Unable to find property: " << (*firstPropIt)->id << "\n");
      }

      ++firstPropIt;
    }
  }

  // extract the position
  pos = SGGeod::fromCart(ecPos);
  double recent_alt_ft = altitude_ft;
  altitude_ft = pos.getElevationFt();

  // expose a valid vertical speed
  if (lastUpdateTime != 0)
  {
      double dT = curtime - lastUpdateTime;
      double Weighting=1;
      if (dt < 1.0)
          Weighting = dt;
      // simple smoothing over 1 second
      vs_fps = (1.0-Weighting)*vs_fps +  Weighting * (altitude_ft - recent_alt_ft) / dT * 60;
  }
  lastUpdateTime = curtime;

  // The quaternion rotating from the earth centered frame to the
  // horizontal local frame
  SGQuatf qEc2Hl = SGQuatf::fromLonLatRad((float)pos.getLongitudeRad(),
                                          (float)pos.getLatitudeRad());
  // The orientation wrt the horizontal local frame
  SGQuatf hlOr = conj(qEc2Hl)*ecOrient;
  float hDeg, pDeg, rDeg;
  hlOr.getEulerDeg(hDeg, pDeg, rDeg);
  hdg = hDeg;
  roll = rDeg;
  pitch = pDeg;

  // expose velocities/u,v,wbody-fps in the mp tree
  _uBodyNode->setValue(ecLinearVel[0] * SG_METER_TO_FEET);
  _vBodyNode->setValue(ecLinearVel[1] * SG_METER_TO_FEET);
  _wBodyNode->setValue(ecLinearVel[2] * SG_METER_TO_FEET);

  SG_LOG(SG_AI, SG_DEBUG, "Multiplayer position and orientation: "
         << ecPos << ", " << hlOr);

  //###########################//
  // do calculations for radar //
  //###########################//
    double range_ft2 = UpdateRadar(manager);

    //************************************//
    // Tanker code                        //
    //************************************//


    if ( isTanker) {
        //cout << "IS tanker ";
        if ( (range_ft2 < 250.0 * 250.0) &&
            (y_shift > 0.0)    &&
            (elevation > 0.0) ){
                // refuel_node->setBoolValue(true);
                 //cout << "in contact"  << endl;
            contact = true;
        } else {
            // refuel_node->setBoolValue(false);
            //cout << "not in contact"  << endl;
            contact = false;
        }
    } else {
        //cout << "NOT tanker " << endl;
        contact = false;
    }

  Transform();
}

void
FGAIMultiplayer::addMotionInfo(FGExternalMotionData& motionInfo,
                               long stamp)
{
  mLastTimestamp = stamp;

  if (!mMotionInfo.empty()) {
    double diff = motionInfo.time - mMotionInfo.rbegin()->first;

    // packet is very old -- MP has probably reset (incl. his timebase)
    if (diff < -10.0)
      mMotionInfo.clear();

    // drop packets arriving out of order
    else if (diff < 0.0)
      return;
  }
  mMotionInfo[motionInfo.time] = motionInfo;
  // We just copied the property (pointer) list - they are ours now. Clear the
  // properties list in given/returned object, so former owner won't deallocate them.
  motionInfo.properties.clear();
}

void
FGAIMultiplayer::setDoubleProperty(const std::string& prop, double val)
{
  SGPropertyNode* pNode = props->getChild(prop.c_str(), true);
  pNode->setDoubleValue(val);
}

void FGAIMultiplayer::clearMotionInfo()
{
    mMotionInfo.clear();
    mLastTimestamp = 0;
}

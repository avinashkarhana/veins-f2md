//
// Copyright (C) 2006-2017 Christoph Sommer <sommer@ccs-labs.org>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

//
// Veins Mobility module for the INET Framework (i.e., implementing inet::IMobility)
// Based on inet::MobilityBase of INET Framework v3.4.0
//

#undef INET_IMPORT
#include "inet/common/INETMath.h"
#include "veins_inet/VeinsInetMobility.h"
#include "inet/visualizer/canvas/mobility/MobilityCanvasVisualizer.h"

namespace veins {

    Register_Class(VeinsInetMobility);

    //
    // Public, lifecycle
    //

    VeinsInetMobility::VeinsInetMobility()
        : visualRepresentation(nullptr)
        , constraintAreaMin(inet::Coord::ZERO)
        , constraintAreaMax(inet::Coord::ZERO)
        , lastPosition(inet::Coord::ZERO)
        , lastSpeed(inet::Coord::ZERO)
        , lastOrientation(inet::Quaternion::NIL)
    {
        lastAcceleration = inet::Coord::ZERO;
    }

    //
    // Public, called from manager
    //

    void VeinsInetMobility::preInitialize(std::string external_id, const inet::Coord& position, std::string road_id, double speed, double acceleration, double angle)
    {
        Enter_Method_Silent();
        this->external_id = external_id;
        lastPosition = position;
        lastSpeed = inet::Coord(cos(angle), -sin(angle)) * speed;
        lastAcceleration = inet::Coord(cos(angle), -sin(angle)) * acceleration;
        lastOrientation.setS(-angle);
    }

    void VeinsInetMobility::nextPosition(const inet::Coord& position, std::string road_id, double speed, double acceleration, double angle)
    {
        Enter_Method_Silent();
        lastPosition = position;
        lastSpeed = inet::Coord(cos(angle), -sin(angle)) * speed;
        lastAcceleration = inet::Coord(cos(angle), -sin(angle)) * acceleration;
        lastOrientation.setS(-angle);

        // Update display string to show node is getting updates
        auto hostMod = getParentModule();
        if (std::string(hostMod->getDisplayString().getTagArg("veins", 0)) == ". ") {
            hostMod->getDisplayString().setTagArg("veins", 0, " .");
        }
        else {
            hostMod->getDisplayString().setTagArg("veins", 0, ". ");
        }

        emitMobilityStateChangedSignal();
        updateVisualRepresentation();
    }

    //
    // Public, implementing IMobility interface
    //

    double VeinsInetMobility::getMaxSpeed() const
    {
        return NaN;
    }

    const inet::Coord& VeinsInetMobility::getCurrentPosition()
    {
        return lastPosition;
    }

    const inet::Coord& VeinsInetMobility::getCurrentVelocity()
    {
        return lastSpeed;
    }

    const inet::Coord& VeinsInetMobility::getCurrentAcceleration()
    {
        return lastAcceleration;
    }

    const inet::Quaternion& VeinsInetMobility::getCurrentAngularPosition()
    {
        return lastOrientation;
    }

    //
    // Protected
    //

    void VeinsInetMobility::initialize(int stage)
    {
        cSimpleModule::initialize(stage);
        // EV_TRACE << "initializing VeinsInetMobility stage " << stage << endl;
        if (stage == inet::INITSTAGE_LOCAL) {
            constraintAreaMin.x = par("constraintAreaMinX");
            constraintAreaMin.y = par("constraintAreaMinY");
            constraintAreaMin.z = par("constraintAreaMinZ");
            constraintAreaMax.x = par("constraintAreaMaxX");
            constraintAreaMax.y = par("constraintAreaMaxY");
            constraintAreaMax.z = par("constraintAreaMaxZ");
            bool visualizeMobility = par("visualizeMobility");
            if (visualizeMobility) {
                visualRepresentation = inet::getModuleFromPar<cModule>(par("visualRepresentation"), this);
            }
        }
        else if (stage == inet::INITSTAGE_PHYSICAL_ENVIRONMENT) {
            if (visualRepresentation != nullptr) {
                auto visualizationTarget = visualRepresentation->getParentModule();
                canvasProjection = inet::CanvasProjection::getCanvasProjection(visualizationTarget->getCanvas());
            }
            emitMobilityStateChangedSignal();
            updateVisualRepresentation();
        }
    }

    void VeinsInetMobility::handleMessage(cMessage* message)
    {
        throw cRuntimeError("This module does not handle messages");
    }

    void VeinsInetMobility::updateVisualRepresentation()
    {
        EV_DEBUG << "current position = " << lastPosition << endl;
    #ifdef WITH_VISUALIZERS
        if (hasGUI() && visualRepresentation != nullptr) {
            inet::visualizer::MobilityCanvasVisualizer::setPosition(visualRepresentation, canvasProjection->computeCanvasPoint(lastPosition));
        }
    #else
        auto position = canvasProjection->computeCanvasPoint(lastPosition);
        char buf[32];
        snprintf(buf, sizeof(buf), "%lf", position.x);
        buf[sizeof(buf) - 1] = 0;
        visualRepresentation->getDisplayString().setTagArg("p", 0, buf);
        snprintf(buf, sizeof(buf), "%lf", position.y);
        buf[sizeof(buf) - 1] = 0;
        visualRepresentation->getDisplayString().setTagArg("p", 1, buf);
    #endif
    }

    void VeinsInetMobility::emitMobilityStateChangedSignal()
    {
        emit(mobilityStateChangedSignal, this);
    }

    std::string VeinsInetMobility::getExternalId() const
    {
        if (external_id == "") throw cRuntimeError("VeinsInetMobility::getExternalId called with no external_id set yet");
        return external_id;
    }

    TraCIScenarioManager* VeinsInetMobility::getManager() const
    {
        if (!manager) manager = TraCIScenarioManagerAccess().get();
        return manager;
    }

    TraCICommandInterface* VeinsInetMobility::getCommandInterface() const
    {
        if (!commandInterface) commandInterface = getManager()->getCommandInterface();
        return commandInterface;
    }

    TraCICommandInterface::Vehicle* VeinsInetMobility::getVehicleCommandInterface() const
    {
        if (!vehicleCommandInterface) vehicleCommandInterface = new TraCICommandInterface::Vehicle(getCommandInterface()->vehicle(getExternalId()));
        return vehicleCommandInterface;
    }

} // namespace veins

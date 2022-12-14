/*
 * Copyright (C) 2016 Edward d'Auvergne
 *
 * This file is part of the program FlightGear.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <simgear/props/props.hxx>
#include <simgear/structure/event_mgr.hxx>

// The unit tests of the simgear property tree.
class SimgearEventTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(SimgearEventTests);
    CPPUNIT_TEST(testTaskRescheduleDuringRun);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    void runForTestTime(double totalTime, double updateHz, double simTimeScaling = 1.0);

    // The tests.
    void testTaskRescheduleDuringRun();

private:
    SGSharedPtr<SGEventMgr> _eventManager;
    SGPropertyNode_ptr _realTimeProp;
    SGPropertyNode_ptr _freezeProp;
};

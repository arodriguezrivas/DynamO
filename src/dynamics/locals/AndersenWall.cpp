/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2011  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AndersenWall.hpp"
#include "../overlapFunc/CubePlane.hpp"
#include "localEvent.hpp"
#include "../NparticleEventData.hpp"
#include "../../datatypes/vector.xml.hpp"
#include "../../extcode/include/boost/random/normal_distribution.hpp"
#include "../../base/is_simdata.hpp"
#include "../../simulation/particle.hpp"
#include "../liouvillean/liouvillean.hpp"
#include "../units/units.hpp"
#include "../../schedulers/scheduler.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/uniform_real.hpp>
#include <magnet/xmlwriter.hpp>
#include <cmath>

CLAndersenWall::CLAndersenWall(const magnet::xml::Node& XML, dynamo::SimData* ptrSim):
  Local(ptrSim, "GlobalAndersenWall"),
  sqrtT(1.0)
{
  operator<<(XML);
}

CLAndersenWall::CLAndersenWall(dynamo::SimData* nSim, double nsqrtT,
			       Vector  nnorm, Vector norigin, 
			       std::string nname, CRange* nRange):
  Local(nRange, nSim, "AndersenWall"),
  vNorm(nnorm),
  vPosition(norigin),
  sqrtT(nsqrtT)
{
  localName = nname;
}

LocalEvent 
CLAndersenWall::getEvent(const Particle& part) const
{
#ifdef ISSS_DEBUG
  if (!Sim->dynamics.getLiouvillean().isUpToDate(part))
    M_throw() << "Particle is not up to date";
#endif

  return LocalEvent(part, Sim->dynamics.getLiouvillean().getWallCollision(part, vPosition, vNorm), WALL, *this);
}

void
CLAndersenWall::runEvent(const Particle& part, const LocalEvent& iEvent) const
{
  ++Sim->eventCount;
  
  NEventData EDat
    (Sim->dynamics.getLiouvillean().runAndersenWallCollision
     (part, vNorm, sqrtT));
  
  Sim->signalParticleUpdate(EDat);
  
  Sim->ptrScheduler->fullUpdate(part);
  
  BOOST_FOREACH(magnet::ClonePtr<OutputPlugin> & Ptr, Sim->outputPlugins)
    Ptr->eventUpdate(iEvent, EDat);
}

bool 
CLAndersenWall::isInCell(const Vector & Origin, 
			 const Vector & CellDim) const
{
  return dynamo::OverlapFunctions::CubePlane
    (Origin, CellDim, vPosition, vNorm);
}

void 
CLAndersenWall::initialise(size_t nID)
{
  ID = nID;
}

void 
CLAndersenWall::operator<<(const magnet::xml::Node& XML)
{
  range.set_ptr(CRange::getClass(XML,Sim));
  
  try {
    
    sqrtT = sqrt(XML.getAttribute("Temperature").as<double>() 
		 * Sim->dynamics.units().unitEnergy());

    localName = XML.getAttribute("Name");
    vNorm << XML.getNode("Norm");
    vNorm /= vNorm.nrm();
    vPosition << XML.getNode("Origin");
    vPosition *= Sim->dynamics.units().unitLength();

  } 
  catch (boost::bad_lexical_cast &)
    {
      M_throw() << "Failed a lexical cast in CLAndersenWall";
    }
}

void
CLAndersenWall::outputXML(xml::XmlStream& XML) const
{
  XML << xml::attr("Type") << "AndersenWall"
      << xml::attr("Name") << localName
      << xml::attr("Temperature") << sqrtT * sqrtT 
    / Sim->dynamics.units().unitEnergy()
      << range
      << xml::tag("Norm")
      << vNorm
      << xml::endtag("Norm")
      << xml::tag("Origin")
      << vPosition / Sim->dynamics.units().unitLength()
      << xml::endtag("Origin");
}

void 
CLAndersenWall::write_povray_info(std::ostream& os) const
{
  os << "object {\n plane {\n  <" << vNorm[0] << ", " << vNorm[1] 
     << ", " << vNorm[2] << ">, 0 texture{pigment { color rgb<0.5,0.5,0.5>}}}\n clipped_by{box {\n  <" << -Sim->primaryCellSize[0]/2 
     << ", " << -Sim->primaryCellSize[1]/2 << ", " << -Sim->primaryCellSize[2]/2 
     << ">, <" << Sim->primaryCellSize[0]/2 << ", " << Sim->primaryCellSize[1]/2 
     << ", " << Sim->primaryCellSize[2]/2 << "> }\n}\n translate <" << vPosition[0] << 
    ","<< vPosition[1] << "," << vPosition[2] << ">\n}\n";
}

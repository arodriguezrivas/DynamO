/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2008  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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

#include "include.hpp"
#include "../../extcode/xmlwriter.hpp"
#include "../../extcode/xmlParser.h"
#include "../../base/is_exception.hpp"
#include "../../base/is_simdata.hpp"
#include "../2particleEventData.hpp"
#include "../units/units.hpp"
#include <boost/foreach.hpp>
#include <boost/progress.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/iostreams/filter/base64.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/chain.hpp>
#include <boost/iostreams/device/stream_sink.hpp>
#include <boost/iostreams/device/stream_source.hpp>
#include <boost/iostreams/filter/base64cleaner.hpp>
#include <boost/iostreams/filter/linewrapout.hpp>

xmlw::XmlStream& operator<<(xmlw::XmlStream& XML, const CLiouvillean& g)
{
  g.outputXML(XML);
  return XML;
}

CLiouvillean* 
CLiouvillean::loadClass(const XMLNode& XML, DYNAMO::SimData* tmp)
{
  if (!strcmp(XML.getAttribute("Type"),"Newtonian"))
    return new CLNewton(tmp);
  else if (!strcmp(XML.getAttribute("Type"),"NOrientation"))
    return new CLNOrientation(tmp, XML);
  else if (!strcmp(XML.getAttribute("Type"),"SLLOD"))
    return new CLSLLOD(tmp);
  else 
    D_throw() << "Unknown type of Liouvillean encountered";
}

C2ParticleData 
CLiouvillean::runLineLineCollision(const CIntEvent&,
				   const Iflt&, const Iflt&) const
{ D_throw() << "Not implemented for this Liouvillean."; }

bool
CLiouvillean::getLineLineCollision(CPDData&, const Iflt&, 
				   const CParticle&, const CParticle&
				   ) const
{ D_throw() << "Not implemented for this Liouvillean."; }

Iflt 
CLiouvillean::getPBCSentinelTime(const CParticle&, const Iflt&) const
{ D_throw() << "Not implemented for this Liouvillean."; }

void 
CLiouvillean::loadParticleXMLData(const XMLNode& XML, std::istream& os)
{
  I_cout() << "Loading Particle Data ";
  fflush(stdout);
  if (XML.isAttributeSet("AttachedBinary")
      && (std::toupper(XML.getAttribute("AttachedBinary")[0]) == 'Y'))
    {
#ifndef DYNAMO_CONDOR
      if (XML.isAttributeSet("OrientationDataInc")
	  && (std::toupper(XML.getAttribute("OrientationDataInc")[0]) == 'Y'))
	D_throw() << "Orientation data is present in the binary data,"
		  << " cannot load using this liouvillean.";
	
      Sim->binaryXML = true;
      unsigned long nPart 
	= boost::lexical_cast<unsigned long>(XML.getAttribute("N"));

      boost::progress_display prog(nPart);

      boost::iostreams::filtering_istream base64Convertor;
      
      base64Convertor.push(boost::iostreams::base64_decoder());
      base64Convertor.push(boost::iostreams::base64cleaner_input_filter());
      base64Convertor.push(boost::iostreams::stream_source<std::istream>(os));

      for (unsigned long i = 0; i < nPart; ++i)
	{
	  unsigned long ID;
	  Vector  vel;
	  Vector  pos;
	  
	  binaryread(base64Convertor, ID);

	  if (i != ID) 
	    D_throw() << "Binary data corruption detected, id's don't match"
		      << "\nMight be because this file was generated on another architecture"
		      << "\nTry using the --text option to avoid using the binary format";
	  
	  for (size_t iDim(0); iDim < NDIM; ++iDim)
	    binaryread(base64Convertor, vel[iDim]);
	  
	  for (size_t iDim(0); iDim < NDIM; ++iDim)
	    binaryread(base64Convertor, pos[iDim]);
	  
	  vel *= Sim->Dynamics.units().unitVelocity();
	  pos *= Sim->Dynamics.units().unitLength();
	  
	  Sim->vParticleList.push_back(CParticle(pos, vel, ID));

	  ++prog;
	}      
#else
      D_throw() << "Cannot use appended binary data in Condor executables!";
#endif
    }
  else
    {
      int xml_iter = 0;
      
      unsigned long nPart = XML.nChildNode("Pt");
      boost::progress_display prog(nPart);
      bool outofsequence = false;  
      
      for (unsigned long i = 0; i < nPart; i++)
	{
	  XMLNode xBrowseNode = XML.getChildNode("Pt", &xml_iter);
	  
	  if (boost::lexical_cast<unsigned long>
	      (xBrowseNode.getAttribute("ID")) != i)
	    outofsequence = true;
	  
	  CParticle part(xBrowseNode, i);
	  part.scaleVelocity(Sim->Dynamics.units().unitVelocity());
	  part.scalePosition(Sim->Dynamics.units().unitLength());
	  Sim->vParticleList.push_back(part);
	  ++prog;
	}
      if (outofsequence)
	I_cout() << IC_red << "Particle ID's out of sequence!\n"
		 << IC_red << "This can result in incorrect capture map loads etc.\n"
		 << IC_red << "Erase any capture maps in the configuration file so they are regenerated."
		 << IC_reset;            
    }  
}

//! \brief Helper function for writing out data
template<class T>
void 
CLiouvillean::binarywrite(std::ostream& os, const T& val ) const
{
  os.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

template<class T>
void 
CLiouvillean::binaryread(std::istream& os, T& val) const
{
  os.read(reinterpret_cast<char*>(&val), sizeof(T));
}

void 
CLiouvillean::outputParticleBin64Data(std::ostream& os) const
{
  if (!Sim->binaryXML)
    return;
  
  boost::iostreams::filtering_ostream base64Convertor;
  base64Convertor.push(boost::iostreams::base64_encoder());
  base64Convertor.push(boost::iostreams::line_wrapping_output_filter(80));
  base64Convertor.push(boost::iostreams::stream_sink<std::ostream>(os));
  
  boost::progress_display prog(Sim->lN);

  BOOST_FOREACH(const CParticle& part, Sim->vParticleList)
    {
      CParticle tmp(part);
      Sim->Dynamics.BCs().setPBC(tmp.getPosition(), tmp.getVelocity());
      
      tmp.scaleVelocity(1.0 / Sim->Dynamics.units().unitVelocity());
      tmp.scalePosition(1.0 / Sim->Dynamics.units().unitLength());	  

      binarywrite(base64Convertor, tmp.getID());

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	binarywrite(base64Convertor, tmp.getVelocity()[iDim]);

      for (size_t iDim(0); iDim < NDIM; ++iDim)
	binarywrite(base64Convertor, tmp.getPosition()[iDim]);

      ++prog;
    }
}

void 
CLiouvillean::outputParticleXMLData(xmlw::XmlStream& XML) const
{
  XML << xmlw::tag("ParticleData")
      << xmlw::attr("N") << Sim->lN
      << xmlw::attr("AttachedBinary") << (Sim->binaryXML ? "Y" : "N")
      << xmlw::attr("OrientationDataInc") << "N";

  if (!Sim->binaryXML)
    {
      I_cout() << "Writing Particles ";
      
      boost::progress_display prog(Sim->lN);
      
      for (unsigned long i = 0; i < Sim->lN; ++i)
	{
	  CParticle tmp(Sim->vParticleList[i]);
	  Sim->Dynamics.BCs().setPBC(tmp.getPosition(), tmp.getVelocity());
	  
	  tmp.scaleVelocity(1.0 / Sim->Dynamics.units().unitVelocity());
	  tmp.scalePosition(1.0 / Sim->Dynamics.units().unitLength());
	  
	  XML << xmlw::tag("Pt") << tmp << xmlw::endtag("Pt");
	  
	  ++prog;
	}
    }

  XML << xmlw::endtag("ParticleData");
}

Iflt 
CLiouvillean::getParticleKineticEnergy(const CParticle& part) const
{
  return 0.5 * (part.getVelocity().nrm2()) * Sim->Dynamics.getSpecies(part).getMass();
}

Vector  
CLiouvillean::getVectorParticleKineticEnergy(const CParticle& part) const
{
  Vector  tmp(0.5 * part.getVelocity() * Sim->Dynamics.getSpecies(part).getMass());

  for (size_t i = 0; i < NDIM; ++i)
    tmp[i] *= part.getVelocity()[i];

  return tmp;
}

Iflt 
CLiouvillean::getSystemKineticEnergy() const
{
  Iflt sumEnergy(0);

  BOOST_FOREACH(const CParticle& part, Sim->vParticleList)
    sumEnergy += getParticleKineticEnergy(part);

  return sumEnergy;
}

Vector 
CLiouvillean::getVectorSystemKineticEnergy() const
{
  Vector  sumEnergy(0,0,0);

  BOOST_FOREACH(const CParticle& part, Sim->vParticleList)
    sumEnergy += getVectorParticleKineticEnergy(part);

  return sumEnergy;
}

Iflt 
CLiouvillean::getkT() const
{
  return 2.0 * getSystemKineticEnergy() / (Sim->vParticleList.size() * static_cast<Iflt>(this->getParticleDOF()));
}

void
CLiouvillean::rescaleSystemKineticEnergy(const Iflt& scale)
{
  Iflt scalefactor(sqrt(scale));

  BOOST_FOREACH(CParticle& part, Sim->vParticleList)
    part.getVelocity() *= scalefactor;
}

void
CLiouvillean::rescaleSystemKineticEnergy(const Vector& scalefactors)
{
  BOOST_FOREACH(CParticle& part, Sim->vParticleList)
    for (size_t iDim(0); iDim < NDIM; ++iDim)
      part.getVelocity()[iDim] *= scalefactors[iDim];
}

C2ParticleData 
CLiouvillean::parallelCubeColl(const CIntEvent& event, 
			       const Iflt& e, 
			       const Iflt& d, 
			       const EEventType& eType) const
{ D_throw() << "Not Implemented"; }

C2ParticleData 
CLiouvillean::parallelCubeColl(const CIntEvent& event, 
			       const Iflt& e, 
			       const Iflt& d,
			       const Matrix& Rot,
			       const EEventType& eType) const
{ D_throw() << "Not Implemented"; }

Iflt 
CLiouvillean::getPointPlateCollision(const CParticle& np1, const Vector& nrw0,
				     const Vector& nhat, const Iflt& Delta,
				     const Iflt& Omega, const Iflt& Sigma,
				     const Iflt& t, bool) const
{
  D_throw() << "Not Implemented";
}

C1ParticleData 
CLiouvillean::runOscilatingPlate
(const CParticle& part, const Vector& rw0, Iflt& delta, const Iflt& omega0,
 const Iflt& sigma, const Iflt& mass, const Iflt& e, const Iflt& t) const
{
  D_throw() << "Not Implemented";
}

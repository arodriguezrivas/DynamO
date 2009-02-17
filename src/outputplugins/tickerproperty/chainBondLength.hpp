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

#ifndef COPChainBondLength_H
#define COPChainBondLength_H

#include "ticker.hpp"
#include "../../datatypes/histogram.hpp"
#include <boost/shared_array.hpp>

class COPChainBondLength: public COPTicker
{
 public:
  COPChainBondLength(const DYNAMO::SimData*, const XMLNode&);

  virtual COutputPlugin *Clone() const
  { return new COPChainBondLength(*this); }

  virtual void initialise();

  virtual void stream(Iflt) {}

  virtual void ticker();

  virtual void changeSystem(COutputPlugin*);

  void temperatureRescale(const Iflt&) {}

  virtual void output(xmlw::XmlStream&);
  
 protected:

  struct Cdata
  {
    Cdata(size_t chainID, size_t CL);
    const size_t chainID;
    std::vector<C1DHistogram> BondLengths;
  };

  std::list<Cdata> chains;

};

#endif

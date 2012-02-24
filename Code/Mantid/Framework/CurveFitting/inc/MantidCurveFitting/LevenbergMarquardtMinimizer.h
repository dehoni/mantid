#ifndef MANTID_CURVEFITTING_LEVENBERGMARQUARDTMINIMIZER_H_
#define MANTID_CURVEFITTING_LEVENBERGMARQUARDTMINIMIZER_H_

//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidCurveFitting/IFuncMinimizer.h"

namespace Mantid
{
namespace CurveFitting
{

class CostFuncLeastSquares;

/** Implementing Levenberg-Marquardt by wrapping the IFuncMinimizer interface
    around the GSL implementation of this algorithm.

    @author Anders Markvardsen, ISIS, RAL
    @date 11/12/2009

    Copyright &copy; 2009 ISIS Rutherford Appleton Laboratory & NScD Oak Ridge National Laboratory

    This file is part of Mantid.

    Mantid is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Mantid is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    File change history is stored at: <https://svn.mantidproject.org/mantid/trunk/Code/Mantid>.
    Code Documentation is available at: <http://doxygen.mantidproject.org>
*/
class DLLExport LevenbergMarquardtMinimizer : public IFuncMinimizer
{
public:
  /// Constructor
  LevenbergMarquardtMinimizer():IFuncMinimizer(){}
  /// Name of the minimizer.
  std::string name() const {return "Levenberg-Marquardt";}

  /// Initialize minimizer, i.e. pass a function to minimize.
  virtual void initialize(API::ICostFunction_sptr function);
  /// Do one iteration.
  virtual bool iterate();
  /// Return current value of the cost function
  virtual double costFunctionVal();

private:

  boost::shared_ptr<CostFuncLeastSquares> m_leastSquares;
	/// Static reference to the logger class
	static Kernel::Logger& g_log;
};


} // namespace CurveFitting
} // namespace Mantid

#endif /*MANTID_CURVEFITTING_LEVENBERGMARQUARDTMINIMIZER_H_*/

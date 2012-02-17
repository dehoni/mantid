#ifndef MANTID_CURVEFITTING_DERIVMINIMIZERMINIMIZER_H_
#define MANTID_CURVEFITTING_DERIVMINIMIZERMINIMIZER_H_

//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidCurveFitting/DllConfig.h"
#include "MantidCurveFitting/IFuncMinimizer.h"
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_multifit_nlin.h>

namespace Mantid
{
namespace CurveFitting
{
/** Implementing Broyden-Fletcher-Goldfarb-Shanno (BFGS) algorithm
    by wrapping the IFuncMinimizer interface around the GSL implementation of this algorithm.

    @author Anders Markvardsen, ISIS, RAL
    @date 13/1/2010

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
class DLLExport DerivMinimizer : public IFuncMinimizer
{
public:
  /// Constructor
  DerivMinimizer();
  /// Destructor
  ~DerivMinimizer();

  /// Do one iteration.
  bool iterate();
  /// Do the minimization.
  bool minimize();
  /// Return current value of the cost function
  double costFunctionVal();
  /// Calculate the covariance matrix.
  void calCovarianceMatrix(gsl_matrix * covar, double epsrel = 0.0001);
  /// Initialize minimizer, i.e. pass a function to minimize.
  virtual void initialize(API::ICostFunction_sptr function);

protected:

  /// Return a concrete type to initialize m_gslSolver with
  virtual gsl_multimin_fdfminimizer_type* getGSLMinimizerType() = 0;

  /// Function to minimize.
  API::ICostFunction_sptr m_costFunction;

  /// pointer to the GSL solver doing the work
  gsl_multimin_fdfminimizer *m_gslSolver;

  /// GSL container
  gsl_multimin_function_fdf m_gslMultiminContainer;

  /// GSL vector with function parameters
  gsl_vector *m_x;

  static double fun(const gsl_vector * x, void * params);
  static void dfun(const gsl_vector * x, void * params, gsl_vector * g);
  static void fundfun (const gsl_vector * x, void * params, double * f, gsl_vector * g);
};


} // namespace CurveFitting
} // namespace Mantid

#endif /*MANTID_CURVEFITTING_BFGS_MINIMIZERMINIMIZER_H_*/

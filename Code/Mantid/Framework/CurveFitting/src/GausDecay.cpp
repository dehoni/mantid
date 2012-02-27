//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidCurveFitting/GausDecay.h"
#include <cmath>


namespace Mantid
{
namespace CurveFitting
{

using namespace Kernel;
using namespace API;

DECLARE_FUNCTION(GausDecay)

void GausDecay::init()
{
   declareParameter("A", 10.0); 
   declareParameter("Sigma", 0.2);  
}


void GausDecay::functionMW(double* out, const double* xValues, const size_t nData)const
{
  const double& A = getParameter("A"); 
  const double& G = getParameter("Sigma");  

  for (size_t i = 0; i < nData; i++) {
    double x = xValues[i];
    out[i] = A*exp(-G*G*x*x);
  } 
}

void GausDecay::functionDerivMW(Jacobian* out, const double* xValues, const size_t nData)
{
  calNumericalDeriv(out, xValues, nData);
}

void GausDecay::setActiveParameter(size_t i,double value)
{
  size_t j = indexOfActive(i);

  if (parameterName(j) == "Sigma") 
    setParameter(j,fabs(value),false);  // Make sigma positive
  else
    setParameter(j,value,false);
}


} // namespace CurveFitting
} // namespace Mantid

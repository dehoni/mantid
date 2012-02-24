//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidCurveFitting/Convolution.h"
#include "MantidCurveFitting/DeltaFunction.h"
#include "MantidAPI/IFunction1D.h"
#include "MantidAPI/FunctionFactory.h"

#include <cmath>
#include <algorithm>
#include <functional>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>

#include <sstream>

#include <fstream>

namespace Mantid
{
namespace CurveFitting
{

using namespace Kernel;
using namespace API;

DECLARE_FUNCTION(Convolution)

/// Constructor
Convolution::Convolution()
:m_resolution(NULL),m_resolutionSize(0)
{
}

/// Destructor
Convolution::~Convolution()
{
  if (m_resolution) delete[] m_resolution;
}

void Convolution::init()
{
}

void Convolution::function(const FunctionDomain& domain, FunctionValues& values)const
{
  const FunctionDomain1D* d1d = dynamic_cast<const FunctionDomain1D*>(&domain);
  if (!d1d)
  {
    throw std::invalid_argument("Unexpected domain in Convolution");
  }
  function1D(values.getPointerToCalculated(0),d1d->getPointerAt(0),d1d->size());
}

void Convolution::functionDeriv(const FunctionDomain& domain, Jacobian& jacobian)
{
  calNumericalDeriv(domain,jacobian);
}


/**
 * Calculates convolution of the two member functions. 
 */
void Convolution::function1D(double* out, const double* xValues, const size_t nData)const
{
  if (nFunctions() == 0)
  {
    std::fill_n(out,nData,0.0);
    return;
  }

  if (m_resolutionSize != nData)
  {
    refreshResolution();
  }

  gsl_fft_real_workspace * workspace = gsl_fft_real_workspace_alloc(nData);
  gsl_fft_real_wavetable * wavetable = gsl_fft_real_wavetable_alloc(nData);

  int n2 = static_cast<int>(nData) / 2;
  bool odd = n2*2 != static_cast<int>(nData);
  if (m_resolution == 0)
  {
    m_resolutionSize = nData;
    m_resolution = new double[nData];
    // the resolution must be defined on interval -L < xr < L, L == (xValues[nData-1] - xValues[0]) / 2 
    double* xr = new double[nData];
    double dx = (xValues[nData-1] - xValues[0]) / static_cast<double>((nData - 1));
    // make sure that xr[nData/2] == 0.0
    xr[n2] = 0.0;
    for(int i=1;i<n2;i++)
    {
      double x = i*dx;
      xr[n2 + i] =  x;
      xr[n2 - i] = -x;
    }
    
    xr[0] = -n2*dx;
    if (odd) xr[nData-1] = -xr[0];

    IFunction1D_sptr fun = boost::dynamic_pointer_cast<IFunction1D>(getFunction(0));
    if (!fun)
    {
      delete [] xr;
      throw std::runtime_error("Convolution can work only with IFunctionMW");
    }
    fun->function1D(m_resolution,xr,nData);

    // rotate the data to produce the right transform
    if (odd)
    {
      double tmp = m_resolution[nData-1];
      for(int i=n2-1;i>=0;i--)
      {
        m_resolution[n2+i+1] = m_resolution[i];
        m_resolution[i] = m_resolution[n2+i];
      }
      m_resolution[n2] = tmp;
    }
    else
    {
      for(int i=0;i<n2;i++)
      {
        double tmp = m_resolution[i];
        m_resolution[i] = m_resolution[n2+i];
        m_resolution[n2+i] = tmp;
      }
    }
    gsl_fft_real_transform (m_resolution, 1, nData, wavetable, workspace);
    std::transform(m_resolution,m_resolution+nData,m_resolution,std::bind2nd(std::multiplies<double>(),dx));
    delete[] xr;
  }

  // return the resolution transform for testing
  if (nFunctions() == 1)
  {
    double dx = 1.;//nData > 1? xValues[1] - xValues[0]: 1.;
    std::transform(m_resolution,m_resolution+nData,out,std::bind2nd(std::multiplies<double>(),dx));
    gsl_fft_real_wavetable_free (wavetable);
    gsl_fft_real_workspace_free (workspace);
    return;
  }

  IFunction1D_sptr resolution = boost::dynamic_pointer_cast<IFunction1D>(getFunction(0));

  // check for delta functions
  std::vector< boost::shared_ptr<DeltaFunction> > dltFuns;
  double dltF = 0;
  CompositeFunction_sptr cf = boost::dynamic_pointer_cast<CompositeFunction>(getFunction(1));
  if (cf)
  {
    for(size_t i = 0; i < cf->nFunctions(); ++i)
    {
      boost::shared_ptr<DeltaFunction> df = boost::dynamic_pointer_cast<DeltaFunction>(cf->getFunction(i));
      if (df)
      {
        dltFuns.push_back(df);

        dltF += df->getParameter("Height") * df->HeightPrefactor();
      }
    }
    if (dltFuns.size() == cf->nFunctions())
    {// all delta functions - return scaled reslution
      resolution->function1D(out,xValues,nData);
      std::transform(out,out+nData,out,std::bind2nd(std::multiplies<double>(),dltF));
      return;
    }
  }
  else if (dynamic_cast<DeltaFunction*>(getFunction(1).get()))
  {// single delta function - return scaled reslution
    DeltaFunction* df = dynamic_cast<DeltaFunction*>(getFunction(1));
    resolution->function1D(out,xValues,nData);
    std::transform(out,out+nData,out,std::bind2nd(std::multiplies<double>(),df->getParameter("Height")*df->HeightPrefactor()));
    return;
  }

  IFunction1D_sptr funct = boost::dynamic_pointer_cast<IFunction1D>(getFunction(1));
  funct->function1D(out,xValues,nData);
  gsl_fft_real_transform (out, 1, nData, wavetable, workspace);
  gsl_fft_real_wavetable_free (wavetable);

  double dx = nData > 1? xValues[1] - xValues[0]: 1.;
  std::transform(out,out+nData,out,std::bind2nd(std::multiplies<double>(),dx));

  HalfComplex res(m_resolution,nData);
  HalfComplex fun(out,nData);

  //double df = nData > 1? 1./(xValues[nData-1] - xValues[0]): 1.;
  //std::cerr<<"df="<<df<<'\n';
  //std::ofstream ftrans("trans.txt");
  for(size_t i = 0; i <= res.size(); i++)
  {
    // complex multiplication
    double res_r = res.real(i);
    double res_i = res.imag(i);
    double fun_r = fun.real(i);
    double fun_i = fun.imag(i);
    fun.set(i,res_r*fun_r - res_i*fun_i,res_r*fun_i + res_i*fun_r);
    //ftrans<<df*i<<' '<<fun.real(i)<<' '<<fun.imag(i)<<'\n';
  }

  gsl_fft_halfcomplex_wavetable * wavetable_r = gsl_fft_halfcomplex_wavetable_alloc(nData);
  gsl_fft_halfcomplex_inverse(out, 1, nData, wavetable_r, workspace);
  gsl_fft_halfcomplex_wavetable_free (wavetable_r);

  gsl_fft_real_workspace_free (workspace);

  dx = nData > 1? 1./(xValues[1] - xValues[0]): 1.;
  std::transform(out,out+nData,out,std::bind2nd(std::multiplies<double>(),dx));

  if (dltF != 0.)
  {
    double* tmp = new double[nData];
    resolution->function1D(tmp,xValues,nData);
    std::transform(tmp,tmp+nData,tmp,std::bind2nd(std::multiplies<double>(),dltF));
    std::transform(out,out+nData,tmp,out,std::plus<double>());
    delete [] tmp;
  }

}

//void Convolution::functionDerivMW(Jacobian* out, const double* xValues, const size_t nData)
//{
//  if (nData == 0) return;
//  std::vector<double> dp(nParams());
//  std::vector<double> param(nParams());
//  for(size_t i=0;i<nParams();i++)
//  {
//    double param = getParameter(i);
//    if (param != 0.0)
//    {
//      dp[i] = param*0.01;
//    }
//    else
//    {
//      dp[i] = 0.01;
//    }
//  }
//
//  if (!m_tmp)
//  {
//    m_tmp.reset(new double[nData]);
//    m_tmp1.reset(new double[nData]);
//  }
//
//  functionMW(m_tmp.get(),xValues, nData);
//
//  for (size_t j = 0; j < nParams(); j++) 
//  {
//    double p0 = getParameter(j);
//    setParameter(j,p0 + dp[j],false);
//    functionMW(m_tmp1.get(),xValues, nData);
//    for (size_t i = 0; i < nData; i++)
//    {
//      out->set(i,j, (m_tmp1[i] - m_tmp[i])/dp[j]);
//    }
//    setParameter(j,p0,false);
//  }
//}
//
/**
 * The first function added must be the resolution. 
 * The second is the convoluted (model) function. If third, fourth and so on
 * functions added they will be combined in a composite function. So the
 * Convolution always has two member functions: the resolution and the model.
 * @param f :: A pointer to the function to add
 * @return The index of the new function which will be 0 for the resolution and 1 for the model
 */
size_t Convolution::addFunction(IFunction_sptr f)
{
  if (nFunctions() == 0)
  {
    for(size_t i=0;i<f->nParams();i++)
    {
      f->fix(i);
    }
  }
  size_t iFun = 0;
  if (nFunctions() < 2)
  {
    iFun = CompositeFunction::addFunction(f);
  }
  else
  {
    API::IFunction_sptr f1 = getFunction(1);
    if (!f1)
    {
      throw std::runtime_error("IFitFunction expected but function of another type found");
    }
    CompositeFunction_sptr cf = boost::dynamic_pointer_cast<CompositeFunction>(f1);
    if (cf == 0)
    {
      cf = boost::dynamic_pointer_cast<CompositeFunction>(API::FunctionFactory::Instance().createFunction("CompositeFunction"));
      removeFunction(1);
      cf->addFunction(f1);
      CompositeFunction::addFunction(cf);
    }
    cf->addFunction(f);
    checkFunction();
    iFun = 1;
  }
  return iFun;
}

/// Deletes and zeroes pointer m_resolution forsing function(...) to recalculate the resolution function
void Convolution::refreshResolution()const
{
  if (m_resolution) delete[] m_resolution;
  m_resolution = NULL;
  m_resolutionSize = 0;
}

/// Writes itself into a string
std::string Convolution::asString()const
{
  if (nFunctions() != 2)
  {
    throw std::runtime_error("Convolution function is incomplete");
  }
  std::ostringstream ostr;
  ostr<<"composite=Convolution;";
  IFunction_sptr res = getFunction(0);
  IFunction_sptr fun = getFunction(1);
  if (!res || !fun)
  {
    throw std::runtime_error("IFitFunction expected but function of another type found");
  }
  bool isCompRes = boost::dynamic_pointer_cast<CompositeFunction>(res) != 0;
  bool isCompFun = boost::dynamic_pointer_cast<CompositeFunction>(fun) != 0;

  if (isCompRes) ostr << '(';
  ostr << res->asString() ;
  if (isCompRes) ostr << ')';
  ostr << ';';

  if (isCompFun) ostr << '(';
  ostr << fun->asString() ;
  if (isCompFun) ostr << ')';

  return ostr.str();
}


} // namespace CurveFitting
} // namespace Mantid

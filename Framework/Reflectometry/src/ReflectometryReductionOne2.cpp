// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidReflectometry/ReflectometryReductionOne2.h"
#include "MantidAPI/AnalysisDataService.h"
#include "MantidAPI/Axis.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/Run.h"
#include "MantidAPI/SpectrumInfo.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidGeometry/IDetector.h"
#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/Instrument/ReferenceFrame.h"
#include "MantidGeometry/Objects/BoundingBox.h"
#include "MantidHistogramData/LinearGenerator.h"
#include "MantidIndexing/IndexInfo.h"
#include "MantidKernel/EnabledWhenProperty.h"
#include "MantidKernel/MandatoryValidator.h"
#include "MantidKernel/StringTokenizer.h"
#include "MantidKernel/Strings.h"
#include "MantidKernel/Unit.h"
#include "MantidKernel/UnitFactory.h"

#include <algorithm>
#include <utility>

using namespace Mantid::Kernel;
using namespace Mantid::API;
using namespace Mantid::Geometry;
using namespace Mantid::HistogramData;
using namespace Mantid::Indexing;

namespace Mantid::Reflectometry {

/*Anonomous namespace */
namespace {

std::string const OUTPUT_WORKSPACE_DEFAULT_PREFIX("IvsQ");
std::string const OUTPUT_WORKSPACE_WAVELENGTH_DEFAULT_PREFIX("IvsLam");

static constexpr double degToRad = M_PI / 180.;
static constexpr double radToDeg = 180. / M_PI;

/** Get the twoTheta angle for the centre of the detector associated with the
 * given spectrum
 *
 * @param spectrumInfo : the spectrum info
 * @param spectrumIdx : the workspace index of the spectrum
 * @return : the twoTheta angle in radians
 */
double getDetectorTwoTheta(const SpectrumInfo *spectrumInfo, const size_t spectrumIdx) {
  return spectrumInfo->signedTwoTheta(spectrumIdx);
}

/** Get the start/end of the lambda range for the detector associated
 * with the given spectrum
 *
 * @return : the lambda range
 */
double getLambdaRange(const HistogramX &xValues, const int xIdx) {
  // The lambda range is the bin width from the given index to the next.
  if (xIdx < 0 || xIdx + 1 >= static_cast<int>(xValues.size())) {
    throw std::runtime_error("Error accessing X values out of range (index=" + std::to_string(xIdx + 1) +
                             ", size=" + std::to_string(xValues.size()));
  }

  double result = xValues[xIdx + 1] - xValues[xIdx];
  return result;
}

/** Get the lambda value at the centre of the detector associated
 * with the given spectrum
 *
 * @return : the lambda range
 */
double getLambda(const HistogramX &xValues, const int xIdx) {
  if (xIdx < 0 || xIdx >= static_cast<int>(xValues.size())) {
    throw std::runtime_error("Error accessing X values out of range (index=" + std::to_string(xIdx) +
                             ", size=" + std::to_string(xValues.size()));
  }

  // The centre of the bin is the lower bin edge plus half the width
  return xValues[xIdx] + getLambdaRange(xValues, xIdx) / 2.0;
}

/**
 * Get the topbottom extent of a detector for the given axis
 *
 * @param axis [in] : the axis to get the extent for
 * @param top [in] : if true, get the max extent, or min otherwise
 * @return : the max/min extent on the given axis
 */
double getBoundingBoxExtent(const BoundingBox &boundingBox, const PointingAlong axis, const bool top) {

  double result = 0.0;
  switch (axis) {
  case X:
    result = top ? boundingBox.xMax() : boundingBox.xMin();
    break;
  case Y:
    result = top ? boundingBox.yMax() : boundingBox.yMin();
    break;
  case Z:
    result = top ? boundingBox.zMax() : boundingBox.zMin();
    break;
  default:
    throw std::runtime_error("Axis is not X/Y/Z");
    break;
  }
  return result;
}
} // namespace

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(ReflectometryReductionOne2)

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void ReflectometryReductionOne2::init() {

  // Input workspace
  declareProperty(std::make_unique<WorkspaceProperty<MatrixWorkspace>>("InputWorkspace", "", Direction::Input),
                  "Run to reduce.");

  initReductionProperties();

  // ThetaIn
  declareProperty(std::make_unique<PropertyWithValue<double>>("ThetaIn", Mantid::EMPTY_DBL(), Direction::Input),
                  "Angle in degrees");

  // Processing instructions
  declareProperty(std::make_unique<PropertyWithValue<std::string>>("ProcessingInstructions", "",
                                                                   std::make_shared<MandatoryValidator<std::string>>(),
                                                                   Direction::Input),
                  "Grouping pattern on spectrum numbers to yield only "
                  "the detectors of interest. See GroupDetectors for details.");

  // Minimum wavelength
  declareProperty(std::make_unique<PropertyWithValue<double>>("WavelengthMin", Mantid::EMPTY_DBL(),
                                                              std::make_shared<MandatoryValidator<double>>(),
                                                              Direction::Input),
                  "Wavelength minimum in angstroms");

  // Maximum wavelength
  declareProperty(std::make_unique<PropertyWithValue<double>>("WavelengthMax", Mantid::EMPTY_DBL(),
                                                              std::make_shared<MandatoryValidator<double>>(),
                                                              Direction::Input),
                  "Wavelength maximum in angstroms");

  initMonitorProperties();
  initBackgroundProperties();
  initTransmissionProperties();
  initAlgorithmicProperties();
  initDebugProperties();

  declareProperty(
      std::make_unique<WorkspaceProperty<>>("OutputWorkspace", "", Direction::Output, PropertyMode::Optional),
      "Output Workspace IvsQ.");

  declareProperty(
      std::make_unique<WorkspaceProperty<>>("OutputWorkspaceWavelength", "", Direction::Output, PropertyMode::Optional),
      "Output Workspace IvsLam. Intermediate workspace.");
  setPropertySettings("OutputWorkspaceWavelength",
                      std::make_unique<Kernel::EnabledWhenProperty>("Debug", IS_EQUAL_TO, "1"));

  initTransmissionOutputProperties();
}

/** Validate inputs
 */
std::map<std::string, std::string> ReflectometryReductionOne2::validateInputs() {

  std::map<std::string, std::string> results;

  const auto background = validateBackgroundProperties();
  results.insert(background.begin(), background.end());

  const auto reduction = validateReductionProperties();
  results.insert(reduction.begin(), reduction.end());

  const auto wavelength = validateWavelengthRanges();
  results.insert(wavelength.begin(), wavelength.end());

  const auto transmission = validateTransmissionProperties();
  results.insert(transmission.begin(), transmission.end());

  const std::string summationType = getProperty("SummationType");
  const std::string reductionType = getProperty("ReductionType");
  if (summationType == "SumInQ" && reductionType == "DivergentBeam" && (*getProperty("ThetaIn")).isDefault())
    results["ThetaIn"] = "ThetaIn must be specified for the DivergentBeam case";

  return results;
}

// Set default names for output workspaces
void ReflectometryReductionOne2::setDefaultOutputWorkspaceNames() {
  bool const isDebug = getProperty("Debug");
  MatrixWorkspace_sptr ws = getProperty("InputWorkspace");
  auto const runNumber = getRunNumber(*ws);
  if (isDefault("OutputWorkspace")) {
    setPropertyValue("OutputWorkspace", OUTPUT_WORKSPACE_DEFAULT_PREFIX + runNumber);
  }
  if (isDebug && isDefault("OutputWorkspaceWavelength")) {
    setPropertyValue("OutputWorkspaceWavelength", OUTPUT_WORKSPACE_WAVELENGTH_DEFAULT_PREFIX + runNumber);
  }
}

/** Execute the algorithm.
 */
void ReflectometryReductionOne2::exec() {
  setDefaultOutputWorkspaceNames();

  // Get input properties
  m_runWS = getProperty("InputWorkspace");
  const auto xUnitID = m_runWS->getAxis(0)->unit()->unitID();

  // Handle processing instructions conversion from spectra number to workspace
  // indexes
  convertProcessingInstructions(m_runWS);

  // Neither TOF or Lambda? Abort.
  if ((xUnitID != "Wavelength") && (xUnitID != "TOF"))
    throw std::invalid_argument("InputWorkspace must have units of TOF or Wavelength");

  // Cache the spectrum info and reference frame
  m_spectrumInfo = &m_runWS->spectrumInfo();
  auto instrument = m_runWS->getInstrument();
  m_refFrame = instrument->getReferenceFrame();
  m_partialBins = getProperty("IncludePartialBins");

  // Find and cache detector groups and theta0
  findDetectorGroups();
  findTheta0();

  // Check whether conversion, normalisation, summation etc. need to be done
  m_convertUnits = true;
  m_normaliseMonitors = true;
  m_normaliseTransmission = true;
  m_sum = true;
  if (xUnitID == "Wavelength") {
    // Already converted converted to wavelength
    m_convertUnits = false;
    // Assume it's also already been normalised by monitors and summed
    m_normaliseMonitors = false;
    m_sum = false;
  }

  // Create the output workspace in wavelength
  MatrixWorkspace_sptr IvsLam = makeIvsLam();

  // Convert to Q
  auto IvsQ = convertToQ(IvsLam);

  // Set outputs
  if (!isDefault("OutputWorkspaceWavelength") || isChild()) {
    setProperty("OutputWorkspaceWavelength", IvsLam);
  }
  setProperty("OutputWorkspace", IvsQ);
}

/** Get the twoTheta angle range for the top/bottom of the detector associated
 * with the given spectrum
 *
 * @param spectrumIdx : the workspace index of the spectrum
 * @return : the twoTheta range in radians
 */
double ReflectometryReductionOne2::getDetectorTwoThetaRange(const size_t spectrumIdx) {

  double bTwoTheta = 0;

  // Get the sample->detector distance along the beam
  const V3D detSample = m_spectrumInfo->position(spectrumIdx) - m_spectrumInfo->samplePosition();
  const double beamOffset = m_refFrame->vecPointingAlongBeam().scalar_prod(detSample);
  // Get the bounding box for this detector/group
  BoundingBox boundingBox;
  auto detector = m_runWS->getDetector(spectrumIdx);
  detector->getBoundingBox(boundingBox);
  // Get the top and bottom on the axis pointing up
  const double top = getBoundingBoxExtent(boundingBox, m_refFrame->pointingUp(), true);
  const double bottom = getBoundingBoxExtent(boundingBox, m_refFrame->pointingUp(), false);
  // Calculate the difference in twoTheta between the top and bottom
  const double twoThetaTop = std::atan(top / beamOffset);
  const double twoThetaBottom = std::atan(bottom / beamOffset);
  bTwoTheta = twoThetaTop - twoThetaBottom;

  // We must have non-zero width to project a range
  if (bTwoTheta < Tolerance) {
    throw std::runtime_error("Error calculating pixel size.");
  }

  return bTwoTheta;
}

/**
 * Utility function to create a unique workspace name for diagnostic outputs
 * based on a given input workspace name
 *
 * @param inputName [in] : the input name
 * @return : the output name
 */
std::string ReflectometryReductionOne2::createDebugWorkspaceName(const std::string &inputName) {
  std::string result = inputName;

  if (summingInQ()) {
    if (getPropertyValue("ReductionType") == "DivergentBeam")
      result += "_QSD"; // Q-summed divergent beam
    else
      result += "_QSB"; // Q-summed bent (non-flat) sample
  } else {
    result += "_LS"; // lambda-summed
  }

  return result;
}

/**
 * Utility function to add the given workspace to the ADS if debugging is
 * enabled.
 *
 * @param ws [in] : the workspace to add to the ADS
 * @param wsName [in] : the name to output the workspace as
 * @param wsSuffix [in] : a suffix to apply to the wsName
 * @param debug [in] : true if the workspace should be added to the ADS (the
 * function does nothing if this is false)
 * @param step [inout] : the current step number, which is added to the wsSuffix
 * and is incremented after the workspace is output
 */
void ReflectometryReductionOne2::outputDebugWorkspace(const MatrixWorkspace_sptr &ws, const std::string &wsName,
                                                      const std::string &wsSuffix, const bool debug, int &step) {
  // Nothing to do if debug is not enabled
  if (debug) {
    // Clone the workspace because otherwise we can end up outputting the same
    // workspace twice with different names, which is confusing.
    MatrixWorkspace_sptr cloneWS = ws->clone();
    AnalysisDataService::Instance().addOrReplace(wsName + "_" + std::to_string(step) + wsSuffix, cloneWS);
    ++step;
  }
}

/**
 * Creates the output 1D array in wavelength from an input 2D workspace in
 * TOF. Summation is done over lambda or over lines of constant Q depending on
 * the type of reduction. For the latter, the output is projected to "virtual
 * lambda" at a reference angle twoThetaR.
 *
 * @return :: the output workspace in wavelength
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::makeIvsLam() {
  MatrixWorkspace_sptr result = m_runWS;

  std::string wsName = createDebugWorkspaceName(getPropertyValue("InputWorkspace"));
  const bool debug = getProperty("Diagnostics");
  int step = 1;

  if (summingInQ()) {
    // Background subtraction
    result = backgroundSubtraction(result);
    outputDebugWorkspace(result, wsName, "_subtracted_bkg", debug, step);
    if (m_convertUnits) {
      g_log.debug("Converting input workspace to wavelength\n");
      result = convertToWavelength(result);
      outputDebugWorkspace(result, wsName, "_lambda", debug, step);
    }
    // Now the workspace is in wavelength, find the min/max wavelength
    findWavelengthMinMax(result);
    if (m_normaliseMonitors) {
      g_log.debug("Normalising input workspace by monitors\n");
      result = monitorCorrection(result);
      outputDebugWorkspace(result, wsName, "_norm_monitor", debug, step);
    }
    if (m_normaliseTransmission) {
      g_log.debug("Normalising input workspace by transmission run\n");
      result = transOrAlgCorrection(result, false);
      outputDebugWorkspace(result, wsName, "_norm_trans", debug, step);
    }
    if (m_sum) {
      g_log.debug("Summing in Q\n");
      result = sumInQ(result);
      outputDebugWorkspace(result, wsName, "_summed", debug, step);
    }
    // Crop to wavelength limits
    g_log.debug("Cropping output workspace\n");
    result = cropWavelength(result, true, wavelengthMin(), wavelengthMax());
    outputDebugWorkspace(result, wsName, "_cropped", debug, step);
  } else {
    g_log.debug("Extracting ROI\n");
    result = makeDetectorWS(result, false, false);
    outputDebugWorkspace(result, wsName, "_lambda", debug, step);
    // Background subtraction
    result = backgroundSubtraction(result);
    outputDebugWorkspace(result, wsName, "_lambda_subtracted_bkg", debug, step);
    // Sum in lambda
    if (m_sum) {
      g_log.debug("Summing in wavelength\n");
      result = makeDetectorWS(result, m_convertUnits, true);
      outputDebugWorkspace(result, wsName, "_summed", debug, step);
    }
    // Now the workspace is in wavelength, find the min/max wavelength
    findWavelengthMinMax(result);
    if (m_normaliseMonitors) {
      g_log.debug("Normalising output workspace by monitors\n");
      result = monitorCorrection(result);
      outputDebugWorkspace(result, wsName, "_norm_monitor", debug, step);
    }
    // Crop to wavelength limits
    g_log.debug("Cropping output workspace\n");
    result = cropWavelength(result, true, wavelengthMin(), wavelengthMax());
    outputDebugWorkspace(result, wsName, "_cropped", debug, step);
    if (m_normaliseTransmission) {
      g_log.debug("Normalising output workspace by transmission run\n");
      result = transOrAlgCorrection(result, true);
      outputDebugWorkspace(result, wsName, "_norm_trans", debug, step);
    }
  }

  return result;
}

/**
 * Normalize by monitors (only if I0MonitorIndex, MonitorBackgroundWavelengthMin
 * and MonitorBackgroundWavelengthMax have been given)
 *
 * @param detectorWS :: the detector workspace to normalise, in lambda
 * @return :: the normalized workspace in lambda
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::monitorCorrection(MatrixWorkspace_sptr detectorWS) {
  MatrixWorkspace_sptr IvsLam;
  const Property *monProperty = getProperty("I0MonitorIndex");
  const Property *backgroundMinProperty = getProperty("MonitorBackgroundWavelengthMin");
  const Property *backgroundMaxProperty = getProperty("MonitorBackgroundWavelengthMax");
  if (!monProperty->isDefault() && !backgroundMinProperty->isDefault() && !backgroundMaxProperty->isDefault()) {
    const bool integratedMonitors = getProperty("NormalizeByIntegratedMonitors");
    int index = getProperty("I0MonitorIndex");
    if (!m_spectrumInfo->isMonitor(index)) {
      throw std::invalid_argument("A monitor is expected at spectrum index " + std::to_string(index));
    }
    const auto monitorWS = makeMonitorWS(m_runWS, integratedMonitors);
    if (!integratedMonitors)
      detectorWS = rebinDetectorsToMonitors(detectorWS, monitorWS);
    IvsLam = divide(detectorWS, monitorWS);
  } else {
    IvsLam = detectorWS;
  }

  return IvsLam;
}

MatrixWorkspace_sptr ReflectometryReductionOne2::backgroundSubtraction(MatrixWorkspace_sptr detectorWS) {
  bool subtractBackground = getProperty("SubtractBackground");
  if (!subtractBackground)
    return detectorWS;
  auto alg = this->createChildAlgorithm("ReflectometryBackgroundSubtraction");
  alg->initialize();
  alg->setProperty("InputWorkspace", detectorWS);
  alg->setProperty("InputWorkspaceIndexType", "SpectrumNumber");
  alg->setProperty("ProcessingInstructions", getPropertyValue("BackgroundProcessingInstructions"));
  alg->setProperty("BackgroundCalculationMethod", getPropertyValue("BackgroundCalculationMethod"));
  alg->setProperty("DegreeOfPolynomial", getPropertyValue("DegreeOfPolynomial"));
  alg->setProperty("CostFunction", getPropertyValue("CostFunction"));
  // For our case, the peak range is the same as the main processing
  // instructions, and we do the summation separately so don't sum the peak
  alg->setProperty("PeakRange", getPropertyValue("ProcessingInstructions"));
  alg->setProperty("SumPeak", false);
  alg->execute();
  MatrixWorkspace_sptr corrected = alg->getProperty("OutputWorkspace");
  return corrected;
}

/**
 * Perform either transmission or algorithmic correction according to the
 * settings.
 * @param detectorWS : workspace in wavelength which is to be normalized
 * @param detectorWSReduced:: whether the input detector workspace has been
 * reduced
 * @return : corrected workspace
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::transOrAlgCorrection(const MatrixWorkspace_sptr &detectorWS,
                                                                      const bool detectorWSReduced) {

  MatrixWorkspace_sptr normalized;
  MatrixWorkspace_sptr transRun = getProperty("FirstTransmissionRun");
  if (transRun) {
    normalized = transmissionCorrection(detectorWS, detectorWSReduced);
  } else if (getPropertyValue("CorrectionAlgorithm") != "None") {
    normalized = algorithmicCorrection(detectorWS);
  } else {
    normalized = detectorWS;
  }

  return normalized;
}

/** Perform transmission correction by running 'CreateTransmissionWorkspace' on
 * the input workspace
 * @param detectorWS :: the input workspace
 * @param detectorWSReduced:: whether the input detector workspace has been
 * reduced
 * @return :: the input workspace normalized by transmission
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::transmissionCorrection(const MatrixWorkspace_sptr &detectorWS,
                                                                        const bool detectorWSReduced) {

  MatrixWorkspace_sptr transmissionWS = getProperty("FirstTransmissionRun");
  auto transmissionWSName = transmissionWS->getName();

  // Reduce the transmission workspace, if not already done (assume that if
  // the workspace is in wavelength then it has already been reduced)
  Unit_const_sptr xUnit = transmissionWS->getAxis(0)->unit();
  if (xUnit->unitID() == "TOF") {

    // If TransmissionProcessingInstructions are not passed then use
    // passed processing instrucions
    std::string transmissionCommands = "";
    if (getPointerToProperty("TransmissionProcessingInstructions")->isDefault()) {
      transmissionCommands = m_processingInstructions;
    } else {
      transmissionCommands = getPropertyValue("TransmissionProcessingInstructions");
    }

    bool const isDebug = getProperty("Debug");
    MatrixWorkspace_sptr secondTransmissionWS = getProperty("SecondTransmissionRun");
    auto alg = this->createChildAlgorithm("CreateTransmissionWorkspace");
    alg->initialize();
    alg->setProperty("FirstTransmissionRun", transmissionWS);
    alg->setProperty("SecondTransmissionRun", secondTransmissionWS);
    alg->setPropertyValue("Params", getPropertyValue("Params"));
    alg->setPropertyValue("StartOverlap", getPropertyValue("StartOverlap"));
    alg->setPropertyValue("EndOverlap", getPropertyValue("EndOverlap"));
    alg->setProperty("ScaleRHSWorkspace", getPropertyValue("ScaleRHSWorkspace"));
    alg->setPropertyValue("I0MonitorIndex", getPropertyValue("I0MonitorIndex"));
    alg->setPropertyValue("WavelengthMin", getPropertyValue("WavelengthMin"));
    alg->setPropertyValue("WavelengthMax", getPropertyValue("WavelengthMax"));
    alg->setPropertyValue("MonitorBackgroundWavelengthMin", getPropertyValue("MonitorBackgroundWavelengthMin"));
    alg->setPropertyValue("MonitorBackgroundWavelengthMax", getPropertyValue("MonitorBackgroundWavelengthMax"));
    alg->setPropertyValue("MonitorIntegrationWavelengthMin", getPropertyValue("MonitorIntegrationWavelengthMin"));
    alg->setPropertyValue("MonitorIntegrationWavelengthMax", getPropertyValue("MonitorIntegrationWavelengthMax"));
    alg->setProperty("ProcessingInstructions", transmissionCommands);
    alg->setProperty("NormalizeByIntegratedMonitors", getPropertyValue("NormalizeByIntegratedMonitors"));
    alg->setProperty("Debug", isDebug);
    alg->execute();
    transmissionWS = alg->getProperty("OutputWorkspace");
    transmissionWSName = alg->getPropertyValue("OutputWorkspace");

    // Set interim workspace outputs
    setWorkspacePropertyFromChild(alg, "OutputWorkspaceFirstTransmission");
    setWorkspacePropertyFromChild(alg, "OutputWorkspaceSecondTransmission");
  }

  // Rebin the transmission run to be the same as the input.
  auto rebinToWorkspaceAlg = this->createChildAlgorithm("RebinToWorkspace");
  rebinToWorkspaceAlg->initialize();
  rebinToWorkspaceAlg->setProperty("WorkspaceToMatch", detectorWS);
  rebinToWorkspaceAlg->setProperty("WorkspaceToRebin", transmissionWS);
  rebinToWorkspaceAlg->execute();
  transmissionWS = rebinToWorkspaceAlg->getProperty("OutputWorkspace");

  // If the detector workspace has been reduced then the spectrum maps
  // should match AFTER reducing the transmission workspace
  if (detectorWSReduced) {
    verifySpectrumMaps(detectorWS, transmissionWS);
  }

  MatrixWorkspace_sptr normalized = divide(detectorWS, transmissionWS);

  // Set output transmission workspace
  if (isDefault("OutputWorkspaceTransmission") && !transmissionWSName.empty()) {
    setPropertyValue("OutputWorkspaceTransmission", transmissionWSName);
  }
  setProperty("OutputWorkspaceTransmission", transmissionWS);

  return normalized;
}

/**
 * Perform transmission correction using alternative correction algorithms
 * @param detectorWS : workspace in wavelength which is to be normalized by the
 * results of the transmission corrections.
 * @return : corrected workspace
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::algorithmicCorrection(const MatrixWorkspace_sptr &detectorWS) {

  const std::string corrAlgName = getProperty("CorrectionAlgorithm");

  auto corrAlg = createChildAlgorithm(corrAlgName);
  corrAlg->initialize();
  if (corrAlgName == "PolynomialCorrection") {
    corrAlg->setPropertyValue("Coefficients", getPropertyValue("Polynomial"));
  } else if (corrAlgName == "ExponentialCorrection") {
    corrAlg->setPropertyValue("C0", getPropertyValue("C0"));
    corrAlg->setPropertyValue("C1", getPropertyValue("C1"));
  } else {
    throw std::runtime_error("Unknown correction algorithm: " + corrAlgName);
  }

  corrAlg->setProperty("InputWorkspace", detectorWS);
  corrAlg->setProperty("Operation", "Divide");
  corrAlg->execute();

  return corrAlg->getProperty("OutputWorkspace");
}

/** Returns true if we should use the output workspace's detector position
 * for the theta angle to use in the conversion to Q */
bool ReflectometryReductionOne2::useDetectorAngleForQConversion(const MatrixWorkspace_sptr &ws) const {
  // If summing in Q, the output workspace is constructed such that the angle
  // of the detector is correct for the conversion to Q
  if (summingInQ())
    return false;

  // Otherwise we are summing in lambda. If ThetaIn was not specified then no
  // correction was requested so assume the detector angle is correct
  if ((*getProperty("ThetaIn")).isDefault())
    return true;

  // If there is only one detector (i.e. no summation was required) then we use
  // the detector angle. This is because the correction is assumed to only be
  // required when the region of interest is not centred on a pixel other than
  // the specular pixel, but if we only have one pixel we assume it is the
  // centre pixel. I'm not sure if we may want to change this in future to
  // enable overriding it anyway.
  // Note that when summing in lambda it is not allowed to pass ThetaIn with
  // multiple groups, so we can assume we only have one group (i.e. one output
  // spectrum) here.
  bool const singleDetector = ws->getDetector(0)->nDets() <= 1;
  if (singleDetector)
    return true;

  return false;
}

/** Convert a workspace to Q
 *
 * @param inputWS : The input workspace (in wavelength) to convert to Q
 * @return : output workspace in Q
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::convertToQ(const MatrixWorkspace_sptr &inputWS) {
  MatrixWorkspace_sptr IvsQ;
  if (useDetectorAngleForQConversion(inputWS)) {
    // We can just use ConvertUnits, because the detectors are at the correct
    // position
    auto convertUnits = this->createChildAlgorithm("ConvertUnits");
    convertUnits->initialize();
    convertUnits->setProperty("InputWorkspace", inputWS);
    convertUnits->setProperty("Target", "MomentumTransfer");
    convertUnits->setProperty("AlignBins", false);
    convertUnits->execute();
    IvsQ = convertUnits->getProperty("OutputWorkspace");
  } else {
    // Use RefRoi with the given ThetaIn. This is currently only possible for a
    // single region of interest (i.e. a single histogram in the summed
    // workspace) because different regions of interest would be centred around
    // different angles.
    if (detectorGroups().size() != 1) {
      throw std::invalid_argument("Angle correction using ThetaIn can only be done for a single group "
                                  "in ProcessingInstructions, but the number of groups specified was " +
                                  std::to_string(inputWS->getNumberHistograms()));
    }

    // The angle to use in conversion is different for each mode
    double theta = 0.0;
    if (summingInQ())
      theta = getDetectorTwoTheta(m_spectrumInfo, twoThetaRDetectorIdx(detectorGroups()[0])) * radToDeg / 2.;
    else
      theta = getProperty("ThetaIn");

    auto refRoi = this->createChildAlgorithm("RefRoi");
    refRoi->initialize();
    refRoi->setProperty("InputWorkspace", inputWS);
    refRoi->setProperty("NXPixel", 1);
    refRoi->setProperty("NYPixel", 1);
    refRoi->setProperty("ConvertToQ", true);
    refRoi->setProperty("SumPixels", false);
    refRoi->setProperty("ScatteringAngle", theta);
    refRoi->execute();
    IvsQ = refRoi->getProperty("OutputWorkspace");
    // RefRoi outputs as distribution data but ConvertUnits outputs as counts
    // if the input is counts. For now make this consistent with the original
    // behaviour using ConvertUnits but we may wish to change this in future.
    if (!inputWS->isDistribution()) {
      IvsQ->setYUnitLabel("Counts");
      IvsQ->setDistribution(false);
    }
  }
  return IvsQ;
}

/**
 * Determine whether the reduction should sum along lines of constant
 * Q or in the default lambda.
 *
 * @return : true if the reduction should sum in Q; false otherwise
 */
bool ReflectometryReductionOne2::summingInQ() const {
  bool result = false;
  const std::string summationType = getProperty("SummationType");

  if (summationType == "SumInQ") {
    result = true;
  }

  return result;
}

/**
 * Find and cache the indicies of the detectors of interest
 */
void ReflectometryReductionOne2::findDetectorGroups() {
  std::string instructions = m_processingInstructionsWorkspaceIndex;

  m_detectorGroups = Kernel::Strings::parseGroups<size_t>(instructions);

  // Sort the groups by the first spectrum number in the group (to give the same
  // output order as GroupDetectors)
  std::sort(m_detectorGroups.begin(), m_detectorGroups.end(),
            [](const std::vector<size_t> &a, const std::vector<size_t> &b) { return a.front() < b.front(); });

  if (m_detectorGroups.empty()) {
    throw std::runtime_error("Invalid processing instructions");
  }

  for (const auto &group : m_detectorGroups) {
    for (const auto &wsIdx : group) {
      if (m_spectrumInfo->isMonitor(wsIdx)) {
        throw std::invalid_argument("A detector is expected at workspace index " + std::to_string(wsIdx) +
                                    " (Was converted from specnum), found a monitor");
      }
      if (wsIdx > m_spectrumInfo->size() - 1) {
        throw std::runtime_error("ProcessingInstructions contains an out-of-range index: " + std::to_string(wsIdx));
      }
    }
  }
}

/**
 * Find and cache the angle theta0 from which lines of constant Q emanate
 */
void ReflectometryReductionOne2::findTheta0() {
  // Only requried if summing in Q
  if (!summingInQ()) {
    return;
  }

  const std::string reductionType = getProperty("ReductionType");

  // For the non-flat sample case theta0 is 0
  m_theta0 = 0.0;

  if (reductionType == "DivergentBeam") {
    // theta0 is the horizon angle, which is half the twoTheta angle of the
    // detector position. This is the angle the detector has been rotated
    // to, which we can get from ThetaIn
    const Property *thetaIn = getProperty("ThetaIn");
    if (!thetaIn->isDefault()) {
      m_theta0 = getProperty("ThetaIn");
    } else {
      /// @todo Currently, ThetaIn must be provided via a property. We could
      /// calculate its value instead using
      /// ReflectometryReductionOneAuto2::calculateTheta, which could be moved
      /// to the base class (ReflectometryWorkflowBase2). Users normally use
      /// ReflectometryReductionOneAuto2 though, so at the moment it isn't a
      /// high priority to be able to calculate it here.
      throw std::runtime_error("The ThetaIn property is required for the DivergentBeam case");
    }
  }

  g_log.debug("theta0: " + std::to_string(theta0()) + " degrees\n");

  // Convert to radians
  m_theta0 *= degToRad;
}

/**
 * Get the (arbitrary) reference angle twoThetaR for use for summation
 * in Q
 *
 * @return : the angle twoThetaR in radians
 * @throws : if the angle could not be found
 */
double ReflectometryReductionOne2::twoThetaR(const std::vector<size_t> &detectors) {
  // Get the twoTheta value for the destinaion pixel that we're projecting onto
  double twoThetaR = getDetectorTwoTheta(m_spectrumInfo, twoThetaRDetectorIdx(detectors));
  if (getPropertyValue("ReductionType") == "DivergentBeam") {
    // The angle that should be used in the final conversion to Q is
    // (twoThetaR-theta0). However, the angle actually used by ConvertUnits is
    // twoThetaD/2 where twoThetaD is the detector's twoTheta angle. Since it
    // is not easy to change what angle ConvertUnits uses, we can compensate by
    // setting twoThetaR = twoThetaD/2+theta0
    twoThetaR = twoThetaR / 2.0 + theta0();
  }
  return twoThetaR;
}

/**
 * Get the spectrum index which defines the twoThetaR reference angle
 * @return : the spectrum index
 */
size_t ReflectometryReductionOne2::twoThetaRDetectorIdx(const std::vector<size_t> &detectors) {
  // Get the mid-point of the area of interest
  return detectors.front() + (detectors.back() - detectors.front()) / 2;
}

void ReflectometryReductionOne2::findWavelengthMinMax(const MatrixWorkspace_sptr &inputWS) {

  // Get the max/min wavelength of region of interest
  const double lambdaMin = getProperty("WavelengthMin");
  const double lambdaMax = getProperty("WavelengthMax");

  // If summing in lambda, the min/max wavelength is the same as the input
  if (!summingInQ()) {
    m_wavelengthMin = lambdaMin;
    m_wavelengthMax = lambdaMax;
    return;
  }

  // If summing in Q, we need to do a projection the input min/max for each
  // detector group and take the overall min/max of the projected range
  const size_t numGroups = detectorGroups().size();

  // Find the projected min/max wavelength for all detector groups
  bool first = true;
  for (size_t groupIdx = 0; groupIdx < numGroups; ++groupIdx) {
    // Get the detectors in this group
    const auto &detectors = detectorGroups()[groupIdx];
    double projectedMin = 0;
    double projectedMax = 0;
    // Get the projected lambda for this detector group
    findIvsLamRange(inputWS, detectors, lambdaMin, lambdaMax, projectedMin, projectedMax);
    // Set the overall min/max
    if (first) {
      m_wavelengthMin = projectedMin;
      m_wavelengthMax = projectedMax;
      first = false;
    } else {
      m_wavelengthMin = std::min(m_wavelengthMin, projectedMin);
      m_wavelengthMax = std::max(m_wavelengthMax, projectedMax);
    }
  }
}

/** Return the spectrum index of the detector to use in the projection for the
 * start of the virtual IvsLam range when summing in Q
 */
size_t ReflectometryReductionOne2::findIvsLamRangeMinDetector(const std::vector<size_t> &detectors) {
  // If we're including partial bins, we use the full input range, which means
  // we project the top left and bottom right corner. For the start of the
  // range we therefore use the highest theta, i.e. max detector index. If
  // excluding partial bins we use the bottom left and top right corner so use
  // the min detector for the start of the range.
  if (m_partialBins)
    return detectors.back();
  else
    return detectors.front();
}

/** Return the spectrum index of the detector to use in the projection for the
 * end of the virtual IvsLam range when summing in Q
 */
size_t ReflectometryReductionOne2::findIvsLamRangeMaxDetector(const std::vector<size_t> &detectors) {
  // If we're including partial bins, we use the full input range, which means
  // we project the top left and bottom right corner. For the end (max) of the
  // range we therefore use the lowest theta, i.e. min detector index. If
  // excluding partial bins we use the bottom left and top right corner so use
  // the max detector for the end of the range.
  if (m_partialBins)
    return detectors.front();
  else
    return detectors.back();
}

double ReflectometryReductionOne2::findIvsLamRangeMin(const MatrixWorkspace_sptr &detectorWS,
                                                      const std::vector<size_t> &detectors, const double lambda) {
  double projectedMin = 0.0;

  const size_t spIdx = findIvsLamRangeMinDetector(detectors);
  const double twoTheta = getDetectorTwoTheta(m_spectrumInfo, spIdx);
  const double bTwoTheta = getDetectorTwoThetaRange(spIdx);

  // For bLambda, use the average bin size for this spectrum
  const auto &xValues = detectorWS->x(spIdx);
  double bLambda = (xValues[xValues.size() - 1] - xValues[0]) / static_cast<int>(xValues.size());
  double dummy = 0.0;
  getProjectedLambdaRange(lambda, twoTheta, bLambda, bTwoTheta, detectors, projectedMin, dummy, m_partialBins);
  return projectedMin;
}

double ReflectometryReductionOne2::findIvsLamRangeMax(const MatrixWorkspace_sptr &detectorWS,
                                                      const std::vector<size_t> &detectors, const double lambda) {
  double projectedMax = 0.0;

  const size_t spIdx = findIvsLamRangeMaxDetector(detectors);
  const double twoTheta = getDetectorTwoTheta(m_spectrumInfo, spIdx);
  const double bTwoTheta = getDetectorTwoThetaRange(spIdx);

  // For bLambda, use the average bin size for this spectrum
  const auto &xValues = detectorWS->x(spIdx);
  double bLambda = (xValues[xValues.size() - 1] - xValues[0]) / static_cast<int>(xValues.size());

  double dummy = 0.0;
  getProjectedLambdaRange(lambda, twoTheta, bLambda, bTwoTheta, detectors, dummy, projectedMax, m_partialBins);
  return projectedMax;
}

/**
 * Find the range of the projected lambda range when summing in Q
 *
 * @param detectorWS [in] : the workspace containing the values to project
 * @param detectors [in] : the workspace indices of the detectors of interest
 * @param lambdaMin [in] : the start of the range to project
 * @param lambdaMax [in] : the end of the range to project
 * @param projectedMin [out] : the start of the resulting projected range
 * @param projectedMax [out] : the end of the resulting projected range
 */
void ReflectometryReductionOne2::findIvsLamRange(const MatrixWorkspace_sptr &detectorWS,
                                                 const std::vector<size_t> &detectors, const double lambdaMin,
                                                 const double lambdaMax, double &projectedMin, double &projectedMax) {

  // Get the new max and min X values of the projected (virtual) lambda range
  projectedMin = findIvsLamRangeMin(detectorWS, detectors, lambdaMin);
  projectedMax = findIvsLamRangeMax(detectorWS, detectors, lambdaMax);

  if (projectedMin > projectedMax) {
    throw std::runtime_error(
        "Error projecting lambda range to reference line at twoTheta=" + std::to_string(twoThetaR(detectors)) +
        "; projected range (" + std::to_string(projectedMin) + "," + std::to_string(projectedMax) + ") is negative.");
  }
}

/**
 * Construct an "empty" output workspace in virtual-lambda for summation in Q.
 * The workspace will have the same x values as the input workspace but the y
 * values will all be zero.
 *
 * @return : a 1D workspace where y values are all zero
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::constructIvsLamWS(const MatrixWorkspace_sptr &detectorWS) {

  // There is one output spectrum for each detector group
  const size_t numGroups = detectorGroups().size();
  // Calculate the number of bins based on the min/max wavelength, using
  // the same bin width as the input workspace
  const double binWidth =
      (detectorWS->x(0).back() - detectorWS->x(0).front()) / static_cast<double>(detectorWS->blocksize());
  const auto numBins = static_cast<int>(std::ceil((wavelengthMax() - wavelengthMin()) / binWidth));
  // Construct the histogram with these X values. Y and E values are zero.
  const BinEdges xValues(numBins, LinearGenerator(wavelengthMin(), binWidth));
  // Create the output workspace
  MatrixWorkspace_sptr outputWS = WorkspaceFactory::Instance().create(detectorWS, numGroups, numBins, numBins - 1);

  // Loop through each detector group in the input and collate the related list
  // of spectrum numbers for the output
  std::vector<SpectrumNumber> spectrumNumbers;
  for (size_t groupIdx = 0; groupIdx < numGroups; ++groupIdx) {
    // Get the detectors in this group
    auto &detectors = detectorGroups()[groupIdx];
    // Set the x values for this spectrum
    outputWS->setBinEdges(groupIdx, xValues);
    // Set the detector ID from the twoThetaR detector.
    const size_t twoThetaRIdx = twoThetaRDetectorIdx(detectors);
    auto &outSpec = outputWS->getSpectrum(groupIdx);
    const detid_t twoThetaRDetID = m_spectrumInfo->detector(twoThetaRIdx).getID();
    outSpec.clearDetectorIDs();
    outSpec.addDetectorID(twoThetaRDetID);
    // Set the spectrum number from the twoThetaR detector
    spectrumNumbers.emplace_back(detectorWS->indexInfo().spectrumNumber(twoThetaRIdx));
  }
  auto indexInf = outputWS->indexInfo();
  indexInf.setSpectrumNumbers(std::move(spectrumNumbers));
  outputWS->setIndexInfo(indexInf);

  return outputWS;
}

/**
 * Sum counts from the input workspace in lambda along lines of constant Q by
 * projecting to "virtual lambda" at a reference angle twoThetaR.
 *
 * @param detectorWS [in] :: the input workspace in wavelength
 * @return :: the output workspace in wavelength
 */
MatrixWorkspace_sptr ReflectometryReductionOne2::sumInQ(const MatrixWorkspace_sptr &detectorWS) {

  // Construct the output array in virtual lambda
  MatrixWorkspace_sptr IvsLam = constructIvsLamWS(detectorWS);

  // Loop through each input group (and corresponding output spectrum)
  const size_t numGroups = detectorGroups().size();
  for (size_t groupIdx = 0; groupIdx < numGroups; ++groupIdx) {
    auto &detectors = detectorGroups()[groupIdx];
    auto &outputE = IvsLam->dataE(groupIdx);

    // Loop through each spectrum in the detector group
    for (auto spIdx : detectors) {
      // Get the angle of this detector and its size in twoTheta
      const double twoTheta = getDetectorTwoTheta(m_spectrumInfo, spIdx);
      const double bTwoTheta = getDetectorTwoThetaRange(spIdx);

      // Check X length is Y length + 1
      const auto &inputX = detectorWS->x(spIdx);
      const auto &inputY = detectorWS->y(spIdx);
      const auto &inputE = detectorWS->e(spIdx);
      if (inputX.size() != inputY.size() + 1) {
        throw std::runtime_error("Expected input workspace to be histogram data (got X len=" +
                                 std::to_string(inputX.size()) + ", Y len=" + std::to_string(inputY.size()) + ")");
      }

      // Create a vector for the projected errors for this spectrum.
      // (Output Y values can simply be accumulated directly into the output
      // workspace, but for error values we need to create a separate error
      // vector for the projected errors from each input spectrum and then
      // do an overall sum in quadrature.)
      std::vector<double> projectedE(outputE.size(), 0.0);

      // Process each value in the spectrum
      const auto ySize = static_cast<int>(inputY.size());
      for (int inputIdx = 0; inputIdx < ySize; ++inputIdx) {
        // Do the summation in Q
        sumInQProcessValue(inputIdx, twoTheta, bTwoTheta, inputX, inputY, inputE, detectors, groupIdx, IvsLam,
                           projectedE);
      }

      // Sum errors in quadrature
      const auto eSize = static_cast<int>(outputE.size());
      for (int outIdx = 0; outIdx < eSize; ++outIdx) {
        outputE[outIdx] += projectedE[outIdx] * projectedE[outIdx];
      }
    }

    // Take the square root of all the accumulated squared errors for this
    // detector group. Assumes Gaussian errors
    double (*rs)(double) = std::sqrt;
    std::transform(outputE.begin(), outputE.end(), outputE.begin(), rs);
  }

  return IvsLam;
}

/**
 * Share counts from an input value onto the projected output in virtual-lambda
 *
 * @param inputIdx [in] :: the index into the input arrays
 * @param twoTheta [in] :: the value of twotTheta for this spectrum
 * @param bTwoTheta [in] :: the size of the pixel in twoTheta
 * @param inputX [in] :: the input spectrum X values
 * @param inputY [in] :: the input spectrum Y values
 * @param inputE [in] :: the input spectrum E values
 * @param detectors [in] :: spectrum indices of the detectors of interest
 * @param outSpecIdx [in] :: the output spectrum index
 * @param IvsLam [in,out] :: the output workspace
 * @param outputE [in,out] :: the projected E values
 */
void ReflectometryReductionOne2::sumInQProcessValue(const int inputIdx, const double twoTheta, const double bTwoTheta,
                                                    const HistogramX &inputX, const HistogramY &inputY,
                                                    const HistogramE &inputE, const std::vector<size_t> &detectors,
                                                    const size_t outSpecIdx, const MatrixWorkspace_sptr &IvsLam,
                                                    std::vector<double> &outputE) {

  // Check whether there are any counts (if not, nothing to share)
  const double inputCounts = inputY[inputIdx];
  if (inputCounts <= 0.0 || std::isnan(inputCounts) || std::isinf(inputCounts)) {
    return;
  }
  // Get the bin width and the bin centre
  const double bLambda = getLambdaRange(inputX, inputIdx);
  const double lambda = getLambda(inputX, inputIdx);
  // Project these coordinates onto the virtual-lambda output (at twoThetaR)
  double lambdaVMin = 0.0;
  double lambdaVMax = 0.0;
  getProjectedLambdaRange(lambda, twoTheta, bLambda, bTwoTheta, detectors, lambdaVMin, lambdaVMax);
  // Share the input counts into the output array
  sumInQShareCounts(inputCounts, inputE[inputIdx], bLambda, lambdaVMin, lambdaVMax, outSpecIdx, IvsLam, outputE);
}

/**
 * Share the given input counts into the output array bins proportionally
 * according to how much the bins overlap the given lambda range.
 * outputX.size() must equal outputY.size() + 1
 *
 * @param inputCounts [in] :: the input counts to share out
 * @param inputErr [in] :: the input errors to share out
 * @param bLambda [in] :: the bin width in lambda
 * @param lambdaMin [in] :: the start of the range to share counts to
 * @param lambdaMax [in] :: the end of the range to share counts to
 * @param outSpecIdx [in] :: the spectrum index to be updated in the output
 * workspace
 * @param IvsLam [in,out] :: the output workspace
 * @param outputE [in,out] :: the projected E values
 */
void ReflectometryReductionOne2::sumInQShareCounts(const double inputCounts, const double inputErr,
                                                   const double bLambda, const double lambdaMin, const double lambdaMax,
                                                   const size_t outSpecIdx, const MatrixWorkspace_sptr &IvsLam,
                                                   std::vector<double> &outputE) {
  // Check that we have histogram data
  const auto &outputX = IvsLam->dataX(outSpecIdx);
  auto &outputY = IvsLam->dataY(outSpecIdx);
  if (outputX.size() != outputY.size() + 1) {
    throw std::runtime_error("Expected output array to be histogram data (got X len=" + std::to_string(outputX.size()) +
                             ", Y len=" + std::to_string(outputY.size()) + ")");
  }

  const double totalWidth = lambdaMax - lambdaMin;

  // Get the first bin edge in the output X array that is within range.
  // There will probably be some overlap, so start from the bin edge before
  // this (unless we're already at the first bin edge).
  auto startIter = std::lower_bound(outputX.begin(), outputX.end(), lambdaMin);
  if (startIter != outputX.begin()) {
    --startIter;
  }

  // Loop through all overlapping output bins. Convert the iterator to an
  // index because we need to index both the X and Y arrays.
  const auto xSize = static_cast<int>(outputX.size());
  for (auto outIdx = startIter - outputX.begin(); outIdx < xSize - 1; ++outIdx) {
    const double binStart = outputX[outIdx];
    const double binEnd = outputX[outIdx + 1];
    if (binStart > lambdaMax) {
      // No longer in the overlap region so we're finished
      break;
    }
    // Add a share of the input counts to this bin based on the proportion of
    // overlap.
    if (totalWidth > Tolerance) {
      // Share counts out proportionally based on the overlap of this range
      const double overlapWidth = std::min({bLambda, lambdaMax - binStart, binEnd - lambdaMin});
      const double fraction = overlapWidth / totalWidth;
      outputY[outIdx] += inputCounts * fraction;
      outputE[outIdx] += inputErr * fraction;
    } else {
      // Projection to a single value. Put all counts in the overlapping output
      // bin.
      outputY[outIdx] += inputCounts;
      outputE[outIdx] += inputCounts;
    }
  }
}

/**
 * Project an input pixel onto an arbitrary reference line at twoThetaR. The
 * projection is done along lines of constant Q, which emanate from theta0. The
 * top-left and bottom-right corners of the pixel are projected, resulting in an
 * output range in "virtual" lambda (lambdaV).
 *
 * For a description of this projection, see:
 *   R. Cubitt, T. Saerbeck, R.A. Campbell, R. Barker, P. Gutfreund
 *   J. Appl. Crystallogr., 48 (6) (2015)
 *
 * @param lambda [in] :: the lambda coord of the centre of the pixel to project
 * @param twoTheta [in] :: the twoTheta coord of the centre of the pixel to
 *project
 * @param bLambda [in] :: the pixel size in lambda
 * @param bTwoTheta [in] :: the pixel size in twoTheta
 * @param detectors [in] :: spectrum indices of the detectors of interest
 * @param lambdaVMin [out] :: the projected range start
 * @param lambdaVMax [out] :: the projected range end
 * @param outerCorners [in] :: true to project from top-left and bottom-right
 * corners of the pixel; false to use bottom-left and top-right
 */
void ReflectometryReductionOne2::getProjectedLambdaRange(const double lambda, const double twoTheta,
                                                         const double bLambda, const double bTwoTheta,
                                                         const std::vector<size_t> &detectors, double &lambdaVMin,
                                                         double &lambdaVMax, const bool outerCorners) {

  // We cannot project pixels below the horizon angle
  if (twoTheta <= theta0()) {
    throw std::runtime_error("Cannot process twoTheta=" + std::to_string(twoTheta * radToDeg) +
                             " as it is below the horizon angle=" + std::to_string(theta0() * radToDeg));
  }

  // Get the angle from twoThetaR to this detector
  const double twoThetaRVal = twoThetaR(detectors);
  // Get the angle from the horizon to the reference angle
  const double delta = twoThetaRVal - theta0();
  // For outer corners use top left, bottom right; otherwise bottom left, top
  // right
  const double lambda1 = lambda - bLambda / 2.0;
  const double lambda2 = lambda + bLambda / 2.0;
  double twoTheta1 = twoTheta + bTwoTheta / 2.0;
  double twoTheta2 = twoTheta - bTwoTheta / 2.0;
  if (!outerCorners)
    std::swap(twoTheta1, twoTheta2);

  // Calculate the projected wavelength range
  try {
    const double lambdaV1 = lambda1 * (std::sin(delta) / std::sin(twoTheta1 - theta0()));
    const double lambdaV2 = lambda2 * (std::sin(delta) / std::sin(twoTheta2 - theta0()));
    lambdaVMin = std::min(lambdaV1, lambdaV2);
    lambdaVMax = std::max(lambdaV1, lambdaV2);
  } catch (std::exception &ex) {
    throw std::runtime_error("Failed to project (lambda, twoTheta) = (" + std::to_string(lambda) + "," +
                             std::to_string(twoTheta * radToDeg) +
                             ") onto twoThetaR = " + std::to_string(twoThetaRVal) + ": " + ex.what());
  }
}

/**
Check whether the spectra for the given workspaces are the same.

@param ws1 : First workspace to compare
@param ws2 : Second workspace to compare against
exception. Otherwise a warning is generated.
*/
void ReflectometryReductionOne2::verifySpectrumMaps(const MatrixWorkspace_const_sptr &ws1,
                                                    const MatrixWorkspace_const_sptr &ws2) {

  bool mismatch = false;
  // Check that the number of histograms is the same
  if (ws1->getNumberHistograms() != ws2->getNumberHistograms()) {
    mismatch = true;
  }
  // Check that the spectrum numbers match for each histogram
  if (!mismatch) {
    for (size_t i = 0; i < ws1->getNumberHistograms(); ++i) {
      if (ws1->indexInfo().spectrumNumber(i) != ws2->indexInfo().spectrumNumber(i)) {
        mismatch = true;
        break;
      }
    }
  }
  // Handle if error
  if (mismatch) {
    const std::string message = "Spectrum maps between workspaces do NOT match up.";
    g_log.warning(message);
  }
}
} // namespace Mantid::Reflectometry

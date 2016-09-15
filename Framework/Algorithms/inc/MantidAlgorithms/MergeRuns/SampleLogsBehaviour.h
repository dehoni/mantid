#ifndef MANTID_ALGORITHMS_MERGERUNS_SAMPLELOGSBEHAVIOUR_H_
#define MANTID_ALGORITHMS_MERGERUNS_SAMPLELOGSBEHAVIOUR_H_

#include "MantidAlgorithms/DllConfig.h"
#include <MantidAPI/MatrixWorkspace.h>

namespace Mantid {
namespace Algorithms {

/** MergeRunsSampleLogsBehaviour : This class holds information relating to the
  behaviour of the sample log merging. It holds a map of all the sample log
  parameters to merge, how to merge them, and the associated tolerances.

  Copyright &copy; 2016 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
  National Laboratory & European Spallation Source

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

  File change history is stored at: <https://github.com/mantidproject/mantid>
  Code Documentation is available at: <http://doxygen.mantidproject.org>
*/
class MANTID_ALGORITHMS_DLL SampleLogsBehaviour {
public:
  enum class MergeLogType { TimeSeries, List, Warn, Fail };

  static const std::string TIME_SERIES_MERGE;
  static const std::string LIST_MERGE;
  static const std::string WARN_MERGE;
  static const std::string FAIL_MERGE;
  static const std::string WARN_MERGE_TOLERANCES;
  static const std::string FAIL_MERGE_TOLERANCES;

  static const std::string TIME_SERIES_SUFFIX;
  static const std::string LIST_SUFFIX;

  struct SampleLogBehaviour {
    MergeLogType type;
    std::shared_ptr<Kernel::Property> property;
    double tolerance;
    bool isNumeric;
  };

  SampleLogsBehaviour(const API::MatrixWorkspace_sptr &ws,
                      Kernel::Logger &logger,
                      const std::string sampleLogsTimeSeries,
                      const std::string sampleLogsList,
                      const std::string sampleLogsWarn,
                      const std::string sampleLogsWarnTolerances,
                      const std::string sampleLogsFail,
                      const std::string sampleLogsFailTolerances);

  /// Create and update sample logs according to instrument parameters
  void mergeSampleLogs(const API::MatrixWorkspace_sptr &addeeWS,
                       const API::MatrixWorkspace_sptr &outWS);
  void setUpdatedSampleLogs(const API::MatrixWorkspace_sptr &ws);
  void resetSampleLogs(const API::MatrixWorkspace_sptr &ws);

private:
  Kernel::Logger &m_logger;

  typedef std::map<const std::string, SampleLogBehaviour> SampleLogsMap;
  SampleLogsMap m_logMap;

  void
  createSampleLogsMapsFromInstrumentParams(SampleLogsMap &instrumentMap,
                                           const API::MatrixWorkspace_sptr &ws);

  std::shared_ptr<Kernel::Property>
  addPropertyForTimeSeries(const std::string item, const double value,
                           const API::MatrixWorkspace_sptr &ws);
  std::shared_ptr<Kernel::Property>
  addPropertyForList(const std::string item, const std::string value,
                     const API::MatrixWorkspace_sptr &ws);
  bool setNumericValue(const std::string item,
                       const API::MatrixWorkspace_sptr &ws, double &value);

  void setSampleMap(SampleLogsMap &map, const MergeLogType &,
                    const std::string &params,
                    const API::MatrixWorkspace_sptr &ws,
                    const std::string paramsTolerances = "",
                    bool skipIfInPrimaryMap = false);

  std::vector<double>
  createTolerancesVector(const size_t numberNames,
                         const std::vector<std::string> &tolerances);

  void updateTimeSeriesProperty(const API::MatrixWorkspace_sptr &addeeWS,
                                const API::MatrixWorkspace_sptr &outWS,
                                const std::string name);
  void updateListProperty(const API::MatrixWorkspace_sptr &addeeWS,
                          const API::MatrixWorkspace_sptr &outWS,
                          Kernel::Property *addeeWSProperty,
                          const std::string name);
  void checkWarnProperty(const API::MatrixWorkspace_sptr &addeeWS,
                         Kernel::Property *addeeWSProperty,
                         const SampleLogBehaviour &behaviour,
                         const double addeeWSNumber, const double outWSNumber,
                         const std::string name);
  void checkErrorProperty(const API::MatrixWorkspace_sptr &addeeWS,
                          Kernel::Property *addeeWSProperty,
                          const SampleLogBehaviour &behaviour,
                          const double addeeWSNumber, const double outWSNumber,
                          const std::string name);
};

} // namespace Algorithms
} // namespace Mantid

#endif /* MANTID_ALGORITHMS_MERGERUNS_SAMPLELOGSBEHAVIOUR_H_ */

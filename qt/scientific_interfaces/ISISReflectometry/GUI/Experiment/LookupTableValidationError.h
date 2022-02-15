// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once
#include "../../Reduction/LookupRow.h"
#include "InvalidLookupRowCells.h"
#include "ThetaValuesValidationError.h"
#include <vector>

namespace MantidQt {
namespace CustomInterfaces {
namespace ISISReflectometry {

class MANTIDQT_ISISREFLECTOMETRY_DLL LookupTableValidationError {
public:
  LookupTableValidationError(std::vector<InvalidLookupRowCells> validationErrors,
                             boost::optional<ThetaValuesValidationError> fullTableError);

  std::vector<InvalidLookupRowCells> const &errors() const;
  boost::optional<ThetaValuesValidationError> fullTableError() const;

private:
  std::vector<InvalidLookupRowCells> m_validationErrors;
  boost::optional<ThetaValuesValidationError> m_fullTableError;
};

} // namespace ISISReflectometry
} // namespace CustomInterfaces
} // namespace MantidQt

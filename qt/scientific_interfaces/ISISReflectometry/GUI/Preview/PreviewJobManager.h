// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2021 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "Common/DllConfig.h"
#include "GUI/Batch/IReflAlgorithmFactory.h"

namespace MantidQt::CustomInterfaces::ISISReflectometry {
class IBatch;

class MANTIDQT_ISISREFLECTOMETRY_DLL PreviewJobManager {
public:
  PreviewJobManager(IBatch &batch, std::unique_ptr<IReflAlgorithmFactory> algFactory = nullptr);

  MantidQt::API::IConfiguredAlgorithm_sptr getPreprocessingAlgorithm(PreviewRow &) const;

private:
  std::unique_ptr<IReflAlgorithmFactory> m_algFactory;
};

} // namespace MantidQt::CustomInterfaces::ISISReflectometry
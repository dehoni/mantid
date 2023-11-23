// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2022 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "DllConfig.h"
#include "ISqwView.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidQtWidgets/Common/QtPropertyBrowser/DoubleEditorFactory.h"
#include "MantidQtWidgets/Common/QtPropertyBrowser/QtTreePropertyBrowser"
#include "MantidQtWidgets/Common/QtPropertyBrowser/qtpropertymanager.h"
#include "MantidQtWidgets/Plotting/RangeSelector.h"
#include "ui_InelasticDataManipulationSqwTab.h"

namespace MantidQt {
namespace CustomInterfaces {

class MANTIDQT_INELASTIC_DLL InelasticDataManipulationSqwTabView : public QWidget, public ISqwView {
  Q_OBJECT

public:
  InelasticDataManipulationSqwTabView(QWidget *perent = nullptr);
  ~InelasticDataManipulationSqwTabView();

  IndirectPlotOptionsView *getPlotOptions() override;
  void setFBSuffixes(QStringList suffix) override;
  void setWSSuffixes(QStringList suffix) override;
  std::tuple<double, double> getQRangeFromPlot() override;
  std::tuple<double, double> getERangeFromPlot() override;
  std::string getDataName() override;
  void plotRqwContour(Mantid::API::MatrixWorkspace_sptr rqwWorkspace) override;
  void setDefaultQAndEnergy() override;
  void setSaveEnabled(bool enabled) override;
  bool validate() override;

signals:
  void valueChanged(QtProperty *, double);
  void dataReady(QString const &);
  void qLowChanged(double);
  void qWidthChanged(double);
  void qHighChanged(double);
  void eLowChanged(double);
  void eWidthChanged(double);
  void eHighChanged(double);
  void rebinEChanged(int);
  void runClicked();
  void saveClicked();
  void showMessageBox(const QString &message);

public slots:
  void updateRunButton(bool enabled, std::string const &enableOutputButtons, QString const &message,
                       QString const &tooltip);

private:
  void setQRange(std::tuple<double, double> const &axisRange);
  void setEnergyRange(std::tuple<double, double> const &axisRange);

  void setRunEnabled(bool enabled);
  Ui::InelasticDataManipulationSqwTab m_uiForm;

  /// Tree of the properties
  std::map<QString, QtTreePropertyBrowser *> m_propTrees;
  /// Internal list of the properties
  QMap<QString, QtProperty *> m_properties;
};
} // namespace CustomInterfaces
} // namespace MantidQt
// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2011 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "GUI/Runs/IRunsView.h"
#include "ISearcher.h"
#include "MantidAPI/AlgorithmObserver.h"
#include "MantidAPI/IAlgorithm_fwd.h"
#include "MantidAPI/ITableWorkspace_fwd.h"

namespace MantidQt {
namespace CustomInterfaces {
namespace ISISReflectometry {

/** @class QtCatalogSearcher

QtCatalogSearcher implements ISearcher to provide ICAT search
functionality.
*/
class QtCatalogSearcher : public QObject,
                          public ISearcher,
                          public RunsViewSearchSubscriber,
                          public Mantid::API::AlgorithmObserver {
  Q_OBJECT
public:
  explicit QtCatalogSearcher(IRunsView *m_view);
  ~QtCatalogSearcher() override{};

  // ISearcher overrides
  void subscribe(SearcherSubscriber *notifyee) override;
  SearchResults search(const std::string &text, const std::string &instrument,
                       SearchType searchType) override;
  bool startSearchAsync(const std::string &text, const std::string &instrument,
                        SearchType searchType) override;
  bool searchInProgress() const override;
  SearchResult const &getSearchResult(int index) const override;
  void reset() override;
  bool searchSettingsChanged(const std::string &text,
                             const std::string &instrument,
                             SearchType searchType) const override;

  // RunsViewSearchSubscriber overrides
  void notifySearchComplete() override;

protected:
  void finishHandle(const Mantid::API::IAlgorithm *alg) override;
  void errorHandle(const Mantid::API::IAlgorithm *alg,
                   const std::string &what) override;

private slots:
  void dialogClosed();

private:
  IRunsView *m_view;
  SearcherSubscriber *m_notifyee;
  std::string m_searchText;
  std::string m_instrument;
  SearchType m_searchType;
  bool m_searchInProgress;

  bool hasActiveSession() const;
  void logInToCatalog();
  void execLoginDialog(const Mantid::API::IAlgorithm_sptr &alg);
  std::string activeSessionId() const;
  Mantid::API::IAlgorithm_sptr createSearchAlgorithm(const std::string &text);
  ISearchModel &results() const;
  void searchAsync();
  SearchResults
  convertTableToSearchResults(Mantid::API::ITableWorkspace_sptr tableWorkspace);
};
bool hasActiveCatalogSession();
} // namespace ISISReflectometry
} // namespace CustomInterfaces
} // namespace MantidQt

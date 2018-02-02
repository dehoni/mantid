#ifndef MANTIDQT_CUSTOMINTERFACES_ENGGDIFFMULTIRUNFITTINGWIDGETPRESENTERTEST_H_
#define MANTIDQT_CUSTOMINTERFACES_ENGGDIFFMULTIRUNFITTINGWIDGETPRESENTERTEST_H_

#include "../EnggDiffraction/EnggDiffMultiRunFittingWidgetPresenter.h"
#include "EnggDiffMultiRunFittingWidgetModelMock.h"
#include "EnggDiffMultiRunFittingWidgetViewMock.h"

#include "MantidAPI/WorkspaceFactory.h"
#include "MantidTestHelpers/WorkspaceCreationHelper.h"

#include <cxxtest/TestSuite.h>

using namespace Mantid;

using namespace MantidQt::CustomInterfaces;
using testing::Return;

class EnggDiffMultiRunFittingWidgetPresenterTest : public CxxTest::TestSuite {
public:
  void test_addFittedPeaks() {
    auto presenter = setUpPresenter();
    const API::MatrixWorkspace_sptr ws =
        API::WorkspaceFactory::Instance().create("Workspace2D", 1, 1, 1);

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockModel, addFittedPeaks(runLabel, ws)).Times(1);

    presenter->addFittedPeaks(runLabel, ws);
    assertMocksUsedCorrectly();
  }

  void test_focusedRunIsAddedToModel() {
    auto presenter = setUpPresenter();
    const API::MatrixWorkspace_sptr ws =
        API::WorkspaceFactory::Instance().create("Workspace2D", 1, 1, 1);

    const RunLabel runLabel(123, 1);

    EXPECT_CALL(*m_mockModel, addFocusedRun(runLabel, ws)).Times(1);

    const std::vector<RunLabel> workspaceLabels({runLabel});
    EXPECT_CALL(*m_mockModel, getAllWorkspaceLabels())
        .Times(1)
        .WillOnce(Return(workspaceLabels));

    presenter->addFocusedRun(runLabel, ws);
    assertMocksUsedCorrectly();
  }

  void test_loadRunUpdatesView() {
    auto presenter = setUpPresenter();
    const API::MatrixWorkspace_sptr ws =
        API::WorkspaceFactory::Instance().create("Workspace2D", 1, 1, 1);

    const RunLabel runLabel(123, 1);
    const std::vector<RunLabel> workspaceLabels({runLabel});
    ON_CALL(*m_mockModel, getAllWorkspaceLabels())
        .WillByDefault(Return(workspaceLabels));
    EXPECT_CALL(*m_mockView, updateRunList(workspaceLabels));

    presenter->addFocusedRun(runLabel, ws);
    assertMocksUsedCorrectly();
  }

  void test_getFittedPeaks() {
    auto presenter = setUpPresenter();

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockModel, getFittedPeaks(runLabel))
        .Times(1)
        .WillOnce(Return(boost::none));

    presenter->getFittedPeaks(runLabel);
    assertMocksUsedCorrectly();
  }

  void test_getFocusedRun() {
    auto presenter = setUpPresenter();

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockModel, getFocusedRun(runLabel))
        .Times(1)
        .WillOnce(Return(boost::none));

    presenter->getFocusedRun(runLabel);
    assertMocksUsedCorrectly();
  }

  void test_selectRunValidNoFittedPeaks() {
    auto presenter = setUpPresenter();

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockView, getSelectedRunLabel())
        .Times(1)
        .WillOnce(Return(runLabel));

    const boost::optional<Mantid::API::MatrixWorkspace_sptr> sampleWorkspace(
        WorkspaceCreationHelper::create2DWorkspaceBinned(1, 100));
    EXPECT_CALL(*m_mockModel, getFocusedRun(runLabel))
        .Times(1)
        .WillOnce(Return(sampleWorkspace));

    EXPECT_CALL(*m_mockView, userError(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*m_mockView, resetCanvas()).Times(1);
    EXPECT_CALL(*m_mockView, plotFocusedRun(testing::_)).Times(1);

    EXPECT_CALL(*m_mockModel, hasFittedPeaksForRun(runLabel))
        .Times(1)
        .WillOnce(Return(false));
    EXPECT_CALL(*m_mockView, plotFittedPeaks(testing::_)).Times(0);

    presenter->notify(IEnggDiffMultiRunFittingWidgetPresenter::SelectRun);
    assertMocksUsedCorrectly();
  }

  void test_selectRunInvalid() {
    auto presenter = setUpPresenter();

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockView, getSelectedRunLabel())
        .Times(1)
        .WillOnce(Return(runLabel));
    EXPECT_CALL(*m_mockModel, getFocusedRun(runLabel))
        .Times(1)
        .WillOnce(Return(boost::none));
    EXPECT_CALL(*m_mockView,
                userError("Invalid focused run identifier",
                          "Tried to access invalid run, run number 123 and "
                          "bank ID 1. Please contact the development team with "
                          "this message")).Times(1);
    EXPECT_CALL(*m_mockView, resetCanvas()).Times(0);

    presenter->notify(IEnggDiffMultiRunFittingWidgetPresenter::SelectRun);
    assertMocksUsedCorrectly();
  }

  void test_selectRunValidWithFittedPeaks() {
    auto presenter = setUpPresenter();

    const RunLabel runLabel(123, 1);
    EXPECT_CALL(*m_mockView, getSelectedRunLabel())
        .Times(1)
        .WillOnce(Return(runLabel));

    const boost::optional<Mantid::API::MatrixWorkspace_sptr> sampleWorkspace(
        WorkspaceCreationHelper::create2DWorkspaceBinned(1, 100));
    EXPECT_CALL(*m_mockModel, getFocusedRun(runLabel))
        .Times(1)
        .WillOnce(Return(sampleWorkspace));

    EXPECT_CALL(*m_mockView, userError(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*m_mockView, resetCanvas()).Times(1);
    EXPECT_CALL(*m_mockView, plotFocusedRun(testing::_)).Times(1);

    EXPECT_CALL(*m_mockModel, hasFittedPeaksForRun(runLabel))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(*m_mockView, showFitResultsSelected())
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(*m_mockModel, getFittedPeaks(runLabel))
        .Times(1)
        .WillOnce(Return(sampleWorkspace));
    EXPECT_CALL(*m_mockView, userError(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*m_mockView, plotFittedPeaks(testing::_)).Times(1);

    presenter->notify(IEnggDiffMultiRunFittingWidgetPresenter::SelectRun);
    assertMocksUsedCorrectly();
  }

private:
  MockEnggDiffMultiRunFittingWidgetModel *m_mockModel;
  MockEnggDiffMultiRunFittingWidgetView *m_mockView;

  std::unique_ptr<EnggDiffMultiRunFittingWidgetPresenter> setUpPresenter() {
    auto mockModel_uptr = Mantid::Kernel::make_unique<
        testing::NiceMock<MockEnggDiffMultiRunFittingWidgetModel>>();
    m_mockModel = mockModel_uptr.get();

    auto mockView_uptr = Mantid::Kernel::make_unique<
        testing::NiceMock<MockEnggDiffMultiRunFittingWidgetView>>();
    m_mockView = mockView_uptr.get();

    return Mantid::Kernel::make_unique<EnggDiffMultiRunFittingWidgetPresenter>(
        std::move(mockModel_uptr), std::move(mockView_uptr));
  }

  void assertMocksUsedCorrectly() {
    TSM_ASSERT("View mock not used as expected: some EXPECT_CALL conditions "
               "not satisfied",
               testing::Mock::VerifyAndClearExpectations(m_mockModel));
    TSM_ASSERT("Model mock not used as expected: some EXPECT_CALL conditions "
               "not satisfied",
               testing::Mock::VerifyAndClearExpectations(m_mockView));
  }
};

#endif // MANTIDQT_CUSTOMINTERFACES_ENGGDIFFMULTIRUNFITTINGWIDGETPRESENTERTEST_H_

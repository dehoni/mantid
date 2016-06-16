#ifndef VTK_MD_HISTO_HEX_FACTORY_TEST_H_
#define VTK_MD_HISTO_HEX_FACTORY_TEST_H_

#include "MantidKernel/make_unique.h"
#include "MantidDataObjects/MDHistoWorkspace.h"
#include "MantidTestHelpers/MDEventsTestHelper.h"
#include "MantidVatesAPI/UserDefinedThresholdRange.h"
#include "MantidVatesAPI/NoThresholdRange.h"
#include "MantidVatesAPI/vtkMDHistoHexFactory.h"
#include "MockObjects.h"
#include <cxxtest/TestSuite.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "MantidVatesAPI/vtkStructuredGrid_Silent.h"
#include "vtkStructuredGrid.h"
#include "vtkUnsignedCharArray.h"
#include "vtkSmartPointer.h"

using namespace Mantid;
using namespace Mantid::API;
using namespace Mantid::DataObjects;
using namespace Mantid::VATES;
using namespace Mantid::Geometry;
using namespace testing;

//=====================================================================================
// Functional Tests
//=====================================================================================
class vtkMDHistoHexFactoryTest : public CxxTest::TestSuite {

public:
  void testThresholds() {
    FakeProgressAction progressUpdate;

    // Workspace with value 1.0 everywhere
    MDHistoWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 3);
    ws_sptr->setTransformFromOriginal(new NullCoordTransform);

    vtkMDHistoHexFactory inside(
        boost::make_shared<UserDefinedThresholdRange>(0, 2),
        Mantid::VATES::VolumeNormalization);
    inside.initialize(ws_sptr);
    auto insideData = inside.create(progressUpdate);
    auto insideProduct = vtkStructuredGrid::SafeDownCast(insideData.Get());

    vtkMDHistoHexFactory below(
        boost::make_shared<UserDefinedThresholdRange>(0, 0.5),
        Mantid::VATES::VolumeNormalization);
    below.initialize(ws_sptr);
    auto belowData = below.create(progressUpdate);
    auto belowProduct = vtkStructuredGrid::SafeDownCast(belowData.Get());

    vtkMDHistoHexFactory above(
        boost::make_shared<UserDefinedThresholdRange>(2, 3),
        Mantid::VATES::VolumeNormalization);
    above.initialize(ws_sptr);
    auto aboveData = above.create(progressUpdate);
    auto aboveProduct = vtkStructuredGrid::SafeDownCast(aboveData.Get());

    TS_ASSERT_EQUALS((10 * 10 * 10), insideProduct->GetNumberOfCells());
    for (auto i = 0; i < insideProduct->GetNumberOfCells(); ++i) {
      TS_ASSERT(insideProduct->IsCellVisible(i) != 0);
    }

    // This has changed. Cells are still present but not visible.
    TS_ASSERT_EQUALS((10 * 10 * 10), belowProduct->GetNumberOfCells());
    for (auto i = 0; i < belowProduct->GetNumberOfCells(); ++i) {
      TS_ASSERT(belowProduct->IsCellVisible(i) == 0);
    }

    TS_ASSERT_EQUALS((10 * 10 * 10), aboveProduct->GetNumberOfCells());
    for (auto i = 0; i < aboveProduct->GetNumberOfCells(); ++i) {
      TS_ASSERT(aboveProduct->IsCellVisible(i) == 0);
    }
  }

  void testSignalAspects() {
    FakeProgressAction progressUpdate;

    // Workspace with value 1.0 everywhere
    MDHistoWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 3);
    ws_sptr->setTransformFromOriginal(new NullCoordTransform);

    // Constructional method ensures that factory is only suitable for providing
    // mesh information.
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);
    factory.initialize(ws_sptr);

    auto product = factory.create(progressUpdate);
    TSM_ASSERT_EQUALS(
        "A single array should be present on the product dataset.", 1,
        product->GetCellData()->GetNumberOfArrays());
    vtkDataArray *signalData = product->GetCellData()->GetArray(0);
    TSM_ASSERT_EQUALS("The obtained cell data has the wrong name.",
                      std::string("signal"),
                      std::string(signalData->GetName()));
    const int correctCellNumber = 10 * 10 * 10;
    TSM_ASSERT_EQUALS("The number of signal values generated is incorrect.",
                      correctCellNumber, signalData->GetSize());
  }

  void testProgressUpdating() {
    MockProgressAction mockProgressAction;
    // Expectation checks that progress should be >= 0 and <= 100 and called at
    // least once!
    EXPECT_CALL(mockProgressAction, eventRaised(AllOf(Le(100), Ge(0))))
        .Times(AtLeast(1));

    MDHistoWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 3);
    vtkMDHistoHexFactory factory(boost::make_shared<NoThresholdRange>(),
                                 Mantid::VATES::VolumeNormalization);

    factory.initialize(ws_sptr);
    auto product = factory.create(mockProgressAction);

    TSM_ASSERT("Progress Updates not used as expected.",
               Mock::VerifyAndClearExpectations(&mockProgressAction));
  }

  void testIsValidThrowsWhenNoWorkspace() {
    IMDWorkspace *nullWorkspace = NULL;
    Mantid::API::IMDWorkspace_sptr ws_sptr(nullWorkspace);

    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);

    TSM_ASSERT_THROWS(
        "No workspace, so should not be possible to complete initialization.",
        factory.initialize(ws_sptr), std::invalid_argument);
  }

  void testCreateWithoutInitializeThrows() {
    FakeProgressAction progressUpdate;
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);
    TS_ASSERT_THROWS(factory.create(progressUpdate), std::runtime_error);
  }

  void testInitializationDelegates() {
    // If the workspace provided is not a 4D imdworkspace, it should call the
    // successor's initalization
    Mantid::API::IMDWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 2);

    auto pMockFactorySuccessor = new MockvtkDataSetFactory();
    auto uniqueSuccessor =
        std::unique_ptr<MockvtkDataSetFactory>(pMockFactorySuccessor);
    EXPECT_CALL(*pMockFactorySuccessor, initialize(_))
        .Times(1); // expect it then to call initialize on the successor.
    EXPECT_CALL(*pMockFactorySuccessor, getFactoryTypeName())
        .WillOnce(testing::Return("TypeA"));

    // Constructional method ensures that factory is only suitable for providing
    // mesh information.
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);

    // Successor is provided.
    factory.setSuccessor(std::move(uniqueSuccessor));

    factory.initialize(ws_sptr);

    // Need the raw pointer to test assertions here. Object is not yet deleted
    // as the factory is still in scope.
    TSM_ASSERT("successor factory not used as expected.",
               Mock::VerifyAndClearExpectations(pMockFactorySuccessor));
  }

  void testInitializationDelegatesThrows() {
    // If the workspace provided is not a 4D imdworkspace, it should call the
    // successor's initalization. If there is no successor an exception should
    // be thrown.
    Mantid::API::IMDWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 2);

    // Constructional method ensures that factory is only suitable for providing
    // mesh information.
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);

    TSM_ASSERT_THROWS("Should have thrown an execption given that no successor "
                      "was available.",
                      factory.initialize(ws_sptr), std::runtime_error);
  }

  void testCreateDelegates() {
    FakeProgressAction progressUpdate;
    // If the workspace provided is not a 4D imdworkspace, it should call the
    // successor's initalization
    // 2 dimensions on the workspace.
    Mantid::API::IMDWorkspace_sptr ws_sptr =
        MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 2);

    auto pMockFactorySuccessor = new MockvtkDataSetFactory();
    auto uniqueSuccessor =
        std::unique_ptr<MockvtkDataSetFactory>(pMockFactorySuccessor);
    EXPECT_CALL(*pMockFactorySuccessor, initialize(_))
        .Times(1); // expect it then to call initialize on the successor.
    EXPECT_CALL(*pMockFactorySuccessor, create(Ref(progressUpdate)))
        .Times(1)
        .WillOnce(
            Return(vtkSmartPointer<vtkStructuredGrid>::New())); // expect it
                                                                // then to call
                                                                // create on the
                                                                // successor.
    EXPECT_CALL(*pMockFactorySuccessor, getFactoryTypeName())
        .WillOnce(testing::Return("TypeA"));

    // Constructional method ensures that factory is only suitable for providing
    // mesh information.
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);

    // Successor is provided.
    factory.setSuccessor(std::move(uniqueSuccessor));

    factory.initialize(ws_sptr);
    factory.create(progressUpdate); // should be called on successor.

    // Need the raw pointer to test assertions here. Object is not yet deleted
    // as the factory is still in scope.
    TSM_ASSERT("successor factory not used as expected.",
               Mock::VerifyAndClearExpectations(pMockFactorySuccessor));
  }

  void testTypeName() {
    using namespace Mantid::VATES;
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);
    TS_ASSERT_EQUALS("vtkMDHistoHexFactory", factory.getFactoryTypeName());
  }
};

//=====================================================================================
// Performance tests
//=====================================================================================
class vtkMDHistoHexFactoryTestPerformance : public CxxTest::TestSuite {
private:
  Mantid::API::IMDWorkspace_sptr m_ws_sptr;

public:
  void setUp() override {
    // Create the workspace. 20 bins in each dimension.
    m_ws_sptr = MDEventsTestHelper::makeFakeMDHistoWorkspace(1.0, 3, 100);
    m_ws_sptr->setTransformFromOriginal(new NullCoordTransform);
  }

  void testGenerateHexahedronVtkDataSet() {
    FakeProgressAction progressUpdate;

    // Create the factory.
    vtkMDHistoHexFactory factory(
        boost::make_shared<UserDefinedThresholdRange>(0, 10000),
        Mantid::VATES::VolumeNormalization);
    factory.initialize(m_ws_sptr);

    TS_ASSERT_THROWS_NOTHING(factory.create(progressUpdate));
  }
};

#endif

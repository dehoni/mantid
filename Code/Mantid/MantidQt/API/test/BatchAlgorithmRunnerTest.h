#ifndef MANTIDQT_API_BATCHALGORITHMRUNNERTEST_H_
#define MANTIDQT_API_BATCHALGORITHMRUNNERTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/FrameworkManager.h"
#include "MantidQtAPI/BatchAlgorithmRunner.h"

using namespace Mantid::API;

class BatchAlgorithmRunnerTest : public CxxTest::TestSuite
{
  public:
    // This pair of boilerplate methods prevent the suite being created statically
    // This means the constructor isn't called when running other tests
    static BatchAlgorithmRunnerTest *createSuite() { return new BatchAlgorithmRunnerTest; }
    static void destroySuite(BatchAlgorithmRunnerTest *suite) { delete suite; }

    BatchAlgorithmRunnerTest()
    {
      // To make sure API is initialized properly
      FrameworkManager::Instance();
    }

    void test_basicBatch()
    {
      MantidQt::API::BatchAlgorithmRunner runner(NULL);

      // Create some algorithms
      // Each algorithm depends on the output workspace of the previous
      IAlgorithm_sptr createWsAlg = AlgorithmManager::Instance().create("CreateSampleWorkspace", -1);
      createWsAlg->initialize();
      createWsAlg->setProperty("OutputWorkspace", "ws1");
      createWsAlg->setProperty("Function", "Exp Decay");
      createWsAlg->setProperty("XMax", 20.0);
      createWsAlg->setProperty("BinWidth", 1.0);

      IAlgorithm_sptr cropWsAlg = AlgorithmManager::Instance().create("CropWorkspace", -1);
      cropWsAlg->initialize();
      cropWsAlg->setProperty("OutputWorkspace", "ws2");
      cropWsAlg->setProperty("StartWorkspaceIndex", 4);
      cropWsAlg->setProperty("EndWorkspaceIndex", 5);

      IAlgorithm_sptr scaleWsAlg = AlgorithmManager::Instance().create("Scale", -1);
      scaleWsAlg->initialize();
      scaleWsAlg->setProperty("OutputWorkspace", "ws3");
      scaleWsAlg->setProperty("Factor", 5.0);
      scaleWsAlg->setProperty("Operation", "Add");

      // Add them to the queue
      // Define the input (and inout, if used) WS properties here
      runner.addAlgorithm(createWsAlg);
      runner.addAlgorithm(cropWsAlg, {{"InputWorkspace", "ws1"}});
      runner.addAlgorithm(scaleWsAlg, {{"InputWorkspace", "ws2"}});

      // Run queue
      runner.startBatch();

      // Wait for queue completion
      while(runner.isExecuting()) {}

      // Get workspace history
      std::string wsName = "ws3";
      auto history = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(wsName)->getHistory();

      // Check the algorithm history of the workspace matches what should have been done to it
      TS_ASSERT_EQUALS("CreateSampleWorkspace", history.getAlgorithmHistory(0)->name())
      TS_ASSERT_EQUALS("CropWorkspace", history.getAlgorithmHistory(1)->name())
      TS_ASSERT_EQUALS("Scale", history.getAlgorithmHistory(2)->name())
    }
};

#endif /* MANTIDQT_API_BATCHALGORITHMRUNNERTEST_H_ */

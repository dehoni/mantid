#ifndef MANTID_GEOMETRY_HKLGENERATORTEST_H_
#define MANTID_GEOMETRY_HKLGENERATORTEST_H_

#include <cxxtest/TestSuite.h>
#include <iostream>

#include "MantidGeometry/Crystal/HKLGenerator.h"
#include "MantidGeometry/Crystal/BasicHKLFilters.h"
#include "MantidGeometry/Crystal/SpaceGroupFactory.h"

#include "MantidKernel/Timer.h"

using namespace Mantid::Geometry;
using namespace Mantid::Kernel;

class HKLGeneratorTest : public CxxTest::TestSuite {
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static HKLGeneratorTest *createSuite() { return new HKLGeneratorTest(); }
  static void destroySuite(HKLGeneratorTest *suite) { delete suite; }

  void test_HKLGeneratorReturnsCorrectSizeSymmetricInt() {
    HKLGenerator gen(2, 2, 2);

    TS_ASSERT_EQUALS(gen.size(), 125);
  }

  void test_HKLGeneratorReturnsCorrectSizeSymmetricV3D() {
    HKLGenerator gen(V3D(2, 2, 2));

    TS_ASSERT_EQUALS(gen.size(), 125);
  }

  void test_HKLGeneratorReturnsCorrectSizeAsymmetricV3D() {
    HKLGenerator gen(V3D(-2, -1, -5), V3D(3, 4, -2));

    TS_ASSERT_EQUALS(gen.size(), 144);
  }

  void test_HKLGeneratorReturnsCorrectSizeOne() {
      HKLGenerator gen(V3D(-2, -1, -5), V3D(-2, -1, -5));
      TS_ASSERT_EQUALS(gen.size(), 1);
  }

  void test_beginIterator() {
    HKLGenerator gen(V3D(-2, -2, -3), V3D(1, 1, 0));

    HKLGenerator::const_iterator it = gen.begin();
    TS_ASSERT_EQUALS(*it, V3D(-2, -2, -3));
  }

  void test_endIterator() {
    HKLGenerator gen(V3D(-2, -2, -3), V3D(1, 1, 0));

    HKLGenerator::const_iterator it = gen.end();
    TS_ASSERT_EQUALS(*it, V3D(2, -2, -3));
  }

  void test_iterator_dereference() {
    HKLGenerator::const_iterator it;
    TS_ASSERT_EQUALS(*it, V3D(0, 0, 0));
  }

  void test_comparison() {
    HKLGenerator::const_iterator it1(V3D(-2, -3, 1));
    HKLGenerator::const_iterator it2(V3D(-2, -3, 1));
    HKLGenerator::const_iterator it3(V3D(-2, -3, 0));

    TS_ASSERT_EQUALS(it1, it2);
    TS_ASSERT_DIFFERS(it1, it3);
  }

  void test_iterator_increment() {
    HKLGenerator::const_iterator defaultIterator;
    TS_ASSERT_THROWS_NOTHING(++defaultIterator);
  }

  void test_iterator_dereference_range() {
    HKLGenerator::const_iterator it(V3D(-1, -1, -1), V3D(1, 1, 1));
    TS_ASSERT_EQUALS(*it, V3D(-1, -1, -1));
    ++it;
    TS_ASSERT_EQUALS(*it, V3D(-1, -1, 0));
  }

  void test_hkl_range() {
    HKLGenerator::const_iterator it(V3D(-1, -1, -1), V3D(1, 1, 1));
    HKLGenerator::const_iterator end(V3D(2, -1, -1));

    std::vector<V3D> hkls;
    std::copy(it, end, std::back_inserter(hkls));

    TS_ASSERT_EQUALS(hkls.size(), 27);
    TS_ASSERT_EQUALS(hkls[0], V3D(-1, -1, -1));
    TS_ASSERT_EQUALS(hkls[1], V3D(-1, -1, 0));
    TS_ASSERT_EQUALS(hkls[2], V3D(-1, -1, 1));
    TS_ASSERT_EQUALS(hkls[3], V3D(-1, 0, -1));
    TS_ASSERT_EQUALS(hkls[26], V3D(1, 1, 1));

    TS_ASSERT_EQUALS(std::distance(it, end), 27);
  }

  void test_speed() {
    size_t N = 1000;

    Timer t;
    for (size_t i = 0; i < N; ++i) {
      HKLGenerator gen(20, 20, 20);

      std::vector<V3D> hkls;
      hkls.reserve(gen.size());

      for (auto it = gen.begin(), end = gen.end(); it != end; ++it) {
        hkls.push_back(*it);
      }

      TS_ASSERT_EQUALS(hkls.size(), 41 * 41 * 41);
    }

    float e = t.elapsed();
    std::cout << "T: " << e / static_cast<float>(N) << std::endl;
  }

  void test_speed_filter_constructor() {
    size_t N = 1000;

    Timer t;
    for (size_t i = 0; i < N; ++i) {
        HKLGenerator gen(20, 20, 20);

      std::vector<V3D> hkls;
      hkls.reserve(gen.size());
      std::copy(gen.begin(), gen.end(), std::back_inserter(hkls));

      // TS_ASSERT_EQUALS(hkls.size(), gen.size());
      TS_ASSERT_EQUALS(hkls.size(), 41 * 41 * 41);
    }

    float e = t.elapsed();
    std::cout << "T: " << e / static_cast<float>(N) << std::endl;
  }

  void test_speed_filter() {
    size_t N = 1000;

    Timer t;
    for (size_t i = 0; i < N; ++i) {
      double dMin = 0.5;
      UnitCell cell(8, 8, 8);

      HKLGenerator gen(cell, dMin);

      std::vector<V3D> hkls;
      hkls.reserve(gen.size());

      HKLFilterDRange dFilter(cell, dMin, 200.0);

      for (auto it = gen.begin(); it != gen.end(); ++it) {
        if (dFilter.isAllowed(*it)) {
          hkls.push_back(*it);
        }
      }

      // TS_ASSERT_EQUALS(hkls.size(), gen.size());
      TS_ASSERT_DIFFERS(hkls.size(), 0);
    }

    float e = t.elapsed();
    std::cout << "T: " << e / static_cast<float>(N) << std::endl;
  }

  void test_speed_filter_stl() {
    size_t N = 1000;

    Timer t;
    for (size_t i = 0; i < N; ++i) {
      double dMin = 0.5;
      UnitCell cell(8, 8, 8);

      HKLGenerator gen(cell, dMin);

      std::vector<V3D> hkls;
      hkls.reserve(gen.size());

      HKLFilter_const_sptr dFilter = boost::make_shared<HKLFilterDRange>(cell, dMin, 200.0);

      std::remove_copy_if(gen.begin(), gen.end(), std::back_inserter(hkls), (~dFilter)->fn());

      // TS_ASSERT_EQUALS(hkls.size(), gen.size());
      TS_ASSERT_DIFFERS(hkls.size(), 0);
    }

    float e = t.elapsed();
    std::cout << "T: " << e / static_cast<float>(N) << std::endl;
  }
};

#endif /* MANTID_GEOMETRY_HKLGENERATORTEST_H_ */

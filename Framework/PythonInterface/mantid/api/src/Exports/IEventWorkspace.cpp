#include "MantidAPI/IEventWorkspace.h"
#include "MantidAPI/IEventList.h"
#include "MantidPythonInterface/kernel/Registry/RegisterWorkspacePtrToPython.h"

#include <boost/python/class.hpp>
#include <boost/python/object.hpp>

using namespace Mantid::API;
using Mantid::PythonInterface::Registry::RegisterWorkspacePtrToPython;
using namespace boost::python;

namespace {
/**
 * Returns a reference to EventList and raises a deprecation warning
 * @param self A reference to calling object
 * @param index Workspace index
 */
IEventList &deprecatedGetEventList(IEventWorkspace &self, const size_t index) {
  PyErr_Warn(PyExc_DeprecationWarning,
             "'getEventList' is deprecated, use 'getSpectrum' instead.");
  return self.getSpectrum(index);
}
}

/**
 * Python exports of the Mantid::API::IEventWorkspace class.
 */
void export_IEventWorkspace() {
  class_<IEventWorkspace, bases<Mantid::API::MatrixWorkspace>,
         boost::noncopyable>("IEventWorkspace", no_init)
      .def("getNumberEvents", &IEventWorkspace::getNumberEvents, args("self"),
           "Returns the number of events in the workspace")
      .def("getTofMin", &IEventWorkspace::getTofMin, args("self"),
           "Returns the minimum TOF value (in microseconds) held by the "
           "workspace")
      .def("getTofMax", &IEventWorkspace::getTofMax, args("self"),
           "Returns the maximum TOF value (in microseconds) held by the "
           "workspace")
      .def("getEventList", &deprecatedGetEventList,
           return_internal_reference<>(), args("self", "workspace_index"),
           "Return the event list managing the events at the given workspace "
           "index")
      .def("clearMRU", &IEventWorkspace::clearMRU, args("self"),
           "Clear the most-recently-used lists");

  RegisterWorkspacePtrToPython<IEventWorkspace>();
}
